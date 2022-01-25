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


#ifndef _SWAP_USM_HOOK_H
#define _SWAP_USM_HOOK_H


#include <linux/list.h>


struct module;
struct sspt_proc;
struct vm_area_struct;


struct usm_hook {
	struct hlist_node node;
	struct module *owner;

	/*
	 * mmap hook called only for vma which we can instrument
	 * (e.g. vma->vm_file is already validate)
	 */
	void (*mmap)(struct sspt_proc *proc, struct vm_area_struct *vma);
};


int usm_hook_reg(struct usm_hook *hook);
void usm_hook_unreg(struct usm_hook *hook);


/* private interface */
void usm_hook_mmap(struct sspt_proc *proc, struct vm_area_struct *vma);


#endif /* _SWAP_USM_HOOK_H */

