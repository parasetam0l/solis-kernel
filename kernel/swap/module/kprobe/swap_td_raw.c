/*
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2014         Vasiliy Ulyanov <v.ulyanov@samsung.com>
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/sched.h>
#include <linux/module.h>
#include "swap_td_raw.h"


#define TD_OFFSET		1  /* skip STACK_END_MAGIC */
#define TD_PREFIX		"[SWAP_TD_RAW] "
#define TD_STACK_USAGE_MAX	0x200
#define TD_CHUNK_MIN		sizeof(long)


static DEFINE_MUTEX(mutex_stack_usage);
static unsigned long stack_usage = 0;
static LIST_HEAD(td_raw_list);


/*
 * take small area from stack
 *
 * 0x00 +--------------------------+
 *      |      STACK_END_MAGIC     |
 *      +--------------------------+  <-- bottom of stack;
 *      |                          |
 *      |           stack          |
 *      |                          |
 * 0xff
 *
 */

static void *bottom_of_stack(struct task_struct *task)
{
	return (void *)(end_of_stack(task) + TD_OFFSET);
}

int swap_td_raw_reg(struct td_raw *raw, unsigned long size)
{
	int ret = 0;

	size = (size / TD_CHUNK_MIN + !!(size % TD_CHUNK_MIN)) * TD_CHUNK_MIN;

	mutex_lock(&mutex_stack_usage);
	if (stack_usage + size > TD_STACK_USAGE_MAX) {
		pr_warn(TD_PREFIX "free stack ended: usage=%ld size=%ld\n",
			stack_usage, size);
		ret = -ENOMEM;
		goto unlock;
	}

	raw->offset = stack_usage;

	INIT_LIST_HEAD(&raw->list);
	list_add(&raw->list, &td_raw_list);

	stack_usage += size;

unlock:
	mutex_unlock(&mutex_stack_usage);
	return ret;
}
EXPORT_SYMBOL_GPL(swap_td_raw_reg);

void swap_td_raw_unreg(struct td_raw *raw)
{
	mutex_lock(&mutex_stack_usage);

	list_del(&raw->list);
	if (list_empty(&td_raw_list))
		stack_usage = 0;

	mutex_unlock(&mutex_stack_usage);
}
EXPORT_SYMBOL_GPL(swap_td_raw_unreg);

void *swap_td_raw(struct td_raw *raw, struct task_struct *task)
{
	return bottom_of_stack(task) + raw->offset;
}
EXPORT_SYMBOL_GPL(swap_td_raw);

int swap_td_raw_init(void)
{
	WARN_ON(stack_usage);

	stack_usage = 0;

	return 0;
}

void swap_td_raw_uninit(void)
{
	WARN_ON(!list_empty(&td_raw_list));
	WARN_ON(stack_usage);
}
