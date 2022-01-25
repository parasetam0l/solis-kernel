/*
 *  SWAP uprobe manager
 *  modules/us_manager/us_slot_manager.c
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013	 Vyacheslav Cherkashin: SWAP us_manager implement
 *
 */


#include <linux/slab.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/list.h>

#include <kprobe/swap_slots.h>
#include <swap-asm/swap_kprobes.h>
#include <swap-asm/swap_uprobes.h>
#include "us_manager_common.h"


static void *sm_alloc_us(struct slot_manager *sm)
{
	unsigned long addr;

	addr = __swap_do_mmap(NULL, 0, PAGE_SIZE,
			      PROT_EXEC | PROT_READ | PROT_WRITE,
			      MAP_ANONYMOUS | MAP_PRIVATE, 0);
	return (void *)addr;
}

static void sm_free_us(struct slot_manager *sm, void *ptr)
{
	/*
	 * E. G.: This code provides kernel dump because of rescheduling while
	 * atomic. As workaround, this code was commented. In this case we will
	 * have memory leaks for instrumented process, but instrumentation
	 * process should functionate correctly. Planned that good solution for
	 * this problem will be done during redesigning KProbe for improving
	 * supportability and performance.
	 */
#if 0
	struct task_struct *task = sm->data;

	mm = get_task_mm(task);
	if (mm) {
		down_write(&mm->mmap_sem);
		do_munmap(mm, (unsigned long)(ptr), PAGE_SIZE);
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
#endif
	/* FIXME: implement the removal of memory for task */
}

/**
 * @brief Create slot_manager struct for US
 *
 * @param task Pointer to the task_struct struct
 * @return Pointer to the created slot_manager struct
 */
struct slot_manager *create_sm_us(struct task_struct *task)
{
	struct slot_manager *sm = kmalloc(sizeof(*sm), GFP_ATOMIC);

	if (sm == NULL)
		return NULL;

	sm->slot_size = UPROBES_TRAMP_LEN;
	sm->alloc = sm_alloc_us;
	sm->free = sm_free_us;
	INIT_HLIST_HEAD(&sm->page_list);
	sm->data = task;

	return sm;
}

/**
 * @brief Remove slot_manager struct for US
 *
 * @param sm remove object
 * @return Void
 */
void free_sm_us(struct slot_manager *sm)
{
	if (sm == NULL)
		return;

	if (!hlist_empty(&sm->page_list)) {
		printk(KERN_WARNING "SWAP US_MANAGER: Error! Slot manager is "
				    "not empty!\n");
		return;
	}

	kfree(sm);
}
