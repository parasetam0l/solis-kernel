/**
 * @file us_manager/img/img_file.h
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
 *
 */


#ifndef _IMG_FILE_H
#define _IMG_FILE_H

#include <linux/types.h>
#include <linux/mutex.h>

struct probe_desc;

/**
 * @struct img_file
 * @breaf Image of file
 */
struct img_file {
	/* img_proc */
	struct list_head list;		/**< List for img_proc */

	/* img_ip */
	struct {
		struct mutex mtx;
		struct list_head head;	/**< Head for img_ip */
	} ips;

	struct dentry *dentry;		/**< Dentry of file */
	atomic_t use;
};

struct img_file *img_file_create(struct dentry *dentry);
void img_file_get(struct img_file *file);
void img_file_put(struct img_file *file);

struct img_ip *img_file_add_ip(struct img_file *file, unsigned long addr,
			       struct probe_desc *pd);
void img_file_del_ip(struct img_file *file, struct img_ip *ip);

int img_file_empty(struct img_file *file);
bool img_file_is_unloadable(void);

/* debug */
void img_file_print(struct img_file *file);
/* debug */

#endif /* _IMG_FILE_H */

