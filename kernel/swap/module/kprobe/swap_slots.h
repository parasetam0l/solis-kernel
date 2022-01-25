/**
 * @file kprobe/swap_slots.h
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
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * SWAP slots interface declaration.
 */

#ifndef _SWAP_SLOTS_H
#define _SWAP_SLOTS_H

#include <linux/types.h>

/**
 * @struct slot_manager
 * @brief Manage slots.
 * @var slot_manager::slot_size
 * Size of the slot.
 * @var slot_manager::alloc
 * Memory allocation callback.
 * @var slot_manager::free
 * Memory release callback.
 * @var slot_manager::page_list
 * List of pages.
 * @var slot_manager::data
 * Slot manager data. task_struct pointer usually stored here.
 */
struct slot_manager {
	unsigned long slot_size;	/* FIXME: allocated in long (4 byte) */
	void *(*alloc)(struct slot_manager *sm);
	void (*free)(struct slot_manager *sm, void *ptr);
	struct hlist_head page_list;
	void *data;
};

void *swap_slot_alloc(struct slot_manager *sm);
void swap_slot_free(struct slot_manager *sm, void *slot);

#endif /* _SWAP_SLOTS_H */
