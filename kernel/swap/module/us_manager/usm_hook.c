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
 * Copyright (C) Samsung Electronics, 2017
 *
 * 2017         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/rwsem.h>
#include <linux/errno.h>
#include <linux/module.h>
#include "usm_hook.h"


static HLIST_HEAD(hook_head);
static DECLARE_RWSEM(hook_sem);


int usm_hook_reg(struct usm_hook *hook)
{
	if (!try_module_get(hook->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hook->node);

	down_write(&hook_sem);
	hlist_add_head(&hook->node, &hook_head);
	up_write(&hook_sem);

	return 0;
}
EXPORT_SYMBOL_GPL(usm_hook_reg);

void usm_hook_unreg(struct usm_hook *hook)
{
	down_write(&hook_sem);
	hlist_del(&hook->node);
	up_write(&hook_sem);

	module_put(hook->owner);
}
EXPORT_SYMBOL_GPL(usm_hook_unreg);

void usm_hook_mmap(struct sspt_proc *proc, struct vm_area_struct *vma)
{
	struct usm_hook *hook;

	down_read(&hook_sem);
	hlist_for_each_entry(hook, &hook_head, node) {
		if (hook->mmap)
			hook->mmap(proc, vma);
	}
	up_read(&hook_sem);
}
