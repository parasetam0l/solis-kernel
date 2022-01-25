/**
 * kprobe/swap_slots.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com> new memory allocator for slots
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) IBM Corporation, 2002, 2004
 * Copyright (C) Samsung Electronics, 2006-2012
 *
 * @section DESCRIPTION
 *
 * SWAP slots implementation.
 */


#include <linux/module.h>
#include <linux/rculist.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "swap_slots.h"
#include "swap_kprobes_deps.h"


/**
 * @struct chunk
 * @brief Chunk of memory for trampolines.
 * @var chunk::data
 * Chunk data.
 * @var chunk::first_available
 * Index of the first available block.
 * @var chunk::count_available
 * Count of the blocks.
 * @var chunk::lock
 * Chunk's lock.
 * @var chunk::size
 * Count of the blocks in chunk.
 * @var chunk::index
 * Pointer to allocated memory.
 */
struct chunk {
	unsigned long *data;
	unsigned long first_available;
	unsigned long count_available;

	spinlock_t    lock;
	unsigned long size;
	unsigned long *index;
};

/**
 * @struct fixed_alloc
 * @brief Item of fixed allocs list.
 * @var fixed_alloc::hlist
 * Fixed alloc hash list node.
 * @var fixed_alloc::chunk
 * Chunk.
 */
struct fixed_alloc {
	struct hlist_node hlist;
	struct chunk chunk;
};

static int chunk_init(struct chunk *chunk, void *data,
		      size_t size, size_t size_block)
{
	unsigned long i;
	unsigned long *p;

	spin_lock_init(&chunk->lock);
	chunk->data = (unsigned long *)data;
	chunk->first_available = 0;
	chunk->count_available = size / size_block;
	chunk->size = chunk->count_available;

	chunk->index = kmalloc(sizeof(*chunk->index)*chunk->count_available,
			       GFP_ATOMIC);

	if (chunk->index == NULL) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	p = chunk->index;
	for (i = 0; i != chunk->count_available; ++p)
		*p = ++i;

	return 0;
}

static void chunk_uninit(struct chunk *chunk)
{
	kfree(chunk->index);
}

static void *chunk_allocate(struct chunk *chunk, size_t size_block)
{
	unsigned long *ret;

	if (!chunk->count_available)
		return NULL;

	spin_lock(&chunk->lock);
	ret = chunk->data + chunk->first_available*size_block;
	chunk->first_available = chunk->index[chunk->first_available];
	--chunk->count_available;
	spin_unlock(&chunk->lock);

	return ret;
}

static void chunk_deallocate(struct chunk *chunk, void *p, size_t size_block)
{
	unsigned long idx = ((unsigned long *)p - chunk->data)/size_block;

	spin_lock(&chunk->lock);
	chunk->index[idx] = chunk->first_available;
	chunk->first_available = idx;
	++chunk->count_available;
	spin_unlock(&chunk->lock);
}

static inline int chunk_check_ptr(struct chunk *chunk, void *p, size_t size)
{
	if ((chunk->data                             <= (unsigned long *)p) &&
	    ((chunk->data + size/sizeof(chunk->data))  > (unsigned long *)p))
		return 1;

	return 0;
}

static inline int chunk_free(struct chunk *chunk)
{
	return (chunk->count_available == chunk->size);
}

static struct fixed_alloc *create_fixed_alloc(struct slot_manager *sm)
{
	int ret;
	void *data;
	struct fixed_alloc *fa;

	fa = kmalloc(sizeof(*fa), GFP_ATOMIC);
	if (fa == NULL)
		return NULL;

	data = sm->alloc(sm);
	if (data == NULL)
		goto free_fa;

	ret = chunk_init(&fa->chunk, data,
			 PAGE_SIZE / sizeof(unsigned long), sm->slot_size);
	if (ret)
		goto free_sm;

	return fa;

free_sm:
	sm->free(sm, data);
free_fa:
	kfree(fa);
	return NULL;
}

static void free_fixed_alloc(struct slot_manager *sm, struct fixed_alloc *fa)
{
	chunk_uninit(&fa->chunk);
	sm->free(sm, fa->chunk.data);
	kfree(fa);
}


/**
 * @brief Allocates slot for slot manager.
 *
 * @param[in,out] sm Slot manager that should be filled.
 * @return Pointer to allocated slot.
 */
void *swap_slot_alloc(struct slot_manager *sm)
{
	void *free_slot;
	struct fixed_alloc *fa;
	DECLARE_NODE_PTR_FOR_HLIST(pos);

	swap_hlist_for_each_entry_rcu(fa, pos, &sm->page_list, hlist) {
		free_slot = chunk_allocate(&fa->chunk, sm->slot_size);
		if (free_slot)
			return free_slot;
	}

	fa = create_fixed_alloc(sm);
	if (fa == NULL)
		return NULL;

	INIT_HLIST_NODE(&fa->hlist);
	hlist_add_head_rcu(&fa->hlist, &sm->page_list);

	return chunk_allocate(&fa->chunk, sm->slot_size);
}
EXPORT_SYMBOL_GPL(swap_slot_alloc);

/**
 * @brief Releases allocated slot.
 *
 * @param sm Pointer to the target slot manager.
 * @param slot Pointer to the target slot.
 * @return Void.
 */
void swap_slot_free(struct slot_manager *sm, void *slot)
{
	struct fixed_alloc *fa;
	DECLARE_NODE_PTR_FOR_HLIST(pos);

	if (slot == NULL)
		return;

	swap_hlist_for_each_entry_rcu(fa, pos, &sm->page_list, hlist) {
		if (!chunk_check_ptr(&fa->chunk, slot, PAGE_SIZE))
			continue;

		chunk_deallocate(&fa->chunk, slot, sm->slot_size);

		if (chunk_free(&fa->chunk)) {
			hlist_del_rcu(&fa->hlist);
			free_fixed_alloc(sm, fa);
		}

		return;
	}

	panic("%s: slot=%p is not data base\n", __func__, slot);
}
EXPORT_SYMBOL_GPL(swap_slot_free);
