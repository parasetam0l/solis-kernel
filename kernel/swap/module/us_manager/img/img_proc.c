/*
 *  SWAP uprobe manager
 *  modules/us_manager/img/img_proc.c
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
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <us_manager/sspt/sspt_proc.h>
#include <us_manager/sspt/sspt_file.h>
#include "img_ip.h"
#include "img_proc.h"
#include "img_file.h"


struct img_proc {
	/* img_file */
	struct {
		struct list_head head;
		struct mutex mtx;
	} files;
};


static atomic_t proc_counter = ATOMIC_INIT(0);

static void img_del_file_by_list(struct img_file *file);

/**
 * @brief Create img_proc struct
 *
 * @return Pointer to the created img_proc struct
 */
struct img_proc *img_proc_create(void)
{
	struct img_proc *proc;

	proc = kmalloc(sizeof(*proc), GFP_ATOMIC);
	if (proc) {
		atomic_inc(&proc_counter);
		INIT_LIST_HEAD(&proc->files.head);
		mutex_init(&proc->files.mtx);
	}

	return proc;
}

/**
 * @brief Remove img_proc struct
 *
 * @param file remove object
 * @return Void
 */
void img_proc_free(struct img_proc *proc)
{
	struct img_file *file, *tmp;

	mutex_lock(&proc->files.mtx);
	list_for_each_entry_safe(file, tmp, &proc->files.head, list) {
		img_del_file_by_list(file);
		img_file_put(file);
	}
	mutex_unlock(&proc->files.mtx);

	atomic_dec(&proc_counter);
	kfree(proc);
}

/* called with mutex_[lock/unlock](&proc->files.mtx) */
static void img_add_file_by_list(struct img_proc *proc, struct img_file *file)
{
	list_add(&file->list, &proc->files.head);
}

/* called with mutex_[lock/unlock](&proc->files.mtx) */
static void img_del_file_by_list(struct img_file *file)
{
	list_del(&file->list);
}

/* called with mutex_[lock/unlock](&proc->files.mtx) */
static struct img_file *img_file_find(struct img_proc *proc,
				      struct dentry *dentry)
{
	struct img_file *file;

	list_for_each_entry(file, &proc->files.head, list) {
		if (file->dentry == dentry)
			return file;
	}

	return NULL;
}

/**
 * @brief Add instrumentation pointer
 *
 * @param proc Pointer to the img_proc struct
 * @param dentry Dentry of file
 * @param addr Function address
 * @param probe_i Pointer to a probe_info struct related with the probe
 * @return Error code
 */
struct img_ip *img_proc_add_ip(struct img_proc *proc, struct dentry *dentry,
			       unsigned long addr, struct probe_desc *pd)
{
	struct img_file *file;

	mutex_lock(&proc->files.mtx);
	file = img_file_find(proc, dentry);
	if (!file) {
		file = img_file_create(dentry);
		if (IS_ERR(file)) {
			mutex_unlock(&proc->files.mtx);

			/* handle type cast */
			return ERR_PTR(PTR_ERR(file));
		}

		img_add_file_by_list(proc, file);
	}
	mutex_unlock(&proc->files.mtx);

	return img_file_add_ip(file, addr, pd);
}

/**
 * @brief Remove instrumentation pointer
 *
 * @param proc Pointer to the img_proc struct
 * @param dentry Dentry of file
 * @param args Function address
 * @return Error code
 */
void img_proc_del_ip(struct img_proc *proc, struct img_ip *ip)
{
	struct img_file *file = ip->file;

	mutex_lock(&proc->files.mtx);
	img_file_del_ip(file, ip);
	if (img_file_empty(file)) {
		img_del_file_by_list(file);
		img_file_put(file);
	}
	mutex_unlock(&proc->files.mtx);
}

void img_proc_copy_to_sspt(struct img_proc *i_proc, struct sspt_proc *proc)
{
	struct sspt_file *file;
	struct img_file *i_file;

	mutex_lock(&i_proc->files.mtx);
	list_for_each_entry(i_file, &i_proc->files.head, list) {
		file = sspt_proc_find_file_or_new(proc, i_file->dentry);
		if (file) {
			struct img_ip *i_ip;

			mutex_lock(&i_file->ips.mtx);
			list_for_each_entry(i_ip, &i_file->ips.head, list)
				sspt_file_add_ip(file, i_ip);
			mutex_unlock(&i_file->ips.mtx);
		}
	}
	mutex_unlock(&i_proc->files.mtx);
}

bool img_proc_is_unloadable(void)
{
	return !(atomic_read(&proc_counter) + !img_file_is_unloadable());
}

/**
 * @brief For debug
 *
 * @param proc Pointer to the img_proc struct
 * @return Void
 */

/* debug */
void img_proc_print(struct img_proc *proc)
{
	struct img_file *file;

	printk(KERN_INFO "### img_proc_print:\n");

	mutex_lock(&proc->files.mtx);
	list_for_each_entry(file, &proc->files.head, list) {
		img_file_print(file);
	}
	mutex_unlock(&proc->files.mtx);
}
/* debug */
