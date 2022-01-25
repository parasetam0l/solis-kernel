/**
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * SWAP debugfs interface definition.
 */

#ifndef _SWAP_DEBUGFS_H
#define _SWAP_DEBUGFS_H


#include <linux/types.h>


struct dfs_setget_64 {
	int (*set)(u64 val);
	u64 (*get)(void);
};

struct dentry;

struct dentry *swap_debugfs_create_setget_u64(const char *name, umode_t mode,
					      struct dentry *parent,
					      struct dfs_setget_64 *setget);

struct dentry *swap_debugfs_create_setget_x64(const char *name, umode_t mode,
					      struct dentry *parent,
					      struct dfs_setget_64 *setget);

struct dentry *swap_debugfs_create_file(const char *name, umode_t mode,
					struct dentry *parent, void *data,
					const struct file_operations *fops);
struct dentry *swap_debugfs_create_dir(const char *name, struct dentry *parent);

struct dentry *swap_debugfs_create_x32(const char *name, umode_t mode,
				       struct dentry *parent, u32 *value);
struct dentry *swap_debugfs_create_x64(const char *name, umode_t mode,
				       struct dentry *parent, u64 *value);
struct dentry *swap_debugfs_create_u64(const char *name, umode_t mode,
				       struct dentry *parent, u64 *value);

struct dentry *swap_debugfs_getdir(void);

int swap_debugfs_init(void);
void swap_debugfs_uninit(void);


#endif /* _SWAP_DEBUGFS_H */
