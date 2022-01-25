#ifndef __SSPT_FILE__
#define __SSPT_FILE__

/**
 * @file us_manager/sspt/sspt_file.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * Copyright (C) Samsung Electronics, 2013
 */

#include "sspt_ip.h"
#include <linux/types.h>
#include <linux/rwsem.h>

enum US_FLAGS;
struct vm_area_struct;

/**
 * @struct sspt_file
 * @breaf Image of file for specified process
 */
struct sspt_file {
	/* sspt_proc */
	struct list_head list;		/**< For sspt_proc */
	struct sspt_proc *proc;		/**< Pointer to the proc (parent) */

	/* sspt_page */
	struct {
		struct rw_semaphore sem;	/**< Semaphore for hash-table */
		unsigned long bits;		/**< Hash-table size */
		struct hlist_head *heads;	/**< Heads for pages */
	} htable;

	struct dentry *dentry;		/**< Dentry of file */
	unsigned long vm_start;		/**< VM start */
	unsigned long vm_end;		/**< VM end */
	unsigned loaded:1;		/**< Flag of loading */
};


struct sspt_file *sspt_file_create(struct dentry *dentry, int page_cnt);
void sspt_file_free(struct sspt_file *file);

struct sspt_page *sspt_find_page_mapped(struct sspt_file *file,
					unsigned long page);
void sspt_file_add_ip(struct sspt_file *file, struct img_ip *img_ip);

void sspt_file_on_each_ip(struct sspt_file *file,
			  void (*func)(struct sspt_ip *, void *), void *data);

int sspt_file_check_install_pages(struct sspt_file *file);
void sspt_file_install(struct sspt_file *file);
int sspt_file_uninstall(struct sspt_file *file,
			struct task_struct *task,
			enum US_FLAGS flag);
void sspt_file_set_mapping(struct sspt_file *file, struct vm_area_struct *vma);

#endif /* __SSPT_FILE__ */
