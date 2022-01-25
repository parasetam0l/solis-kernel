/*
 *  SWAP uprobe manager
 *  modules/us_manager/us_manager_common_file.h
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
 * Copyright (C) Samsung Electronics, 2017
 *
 * 2017	 Alexander Aksenov: SWAP us_manager implement
 *
 */

#ifndef __US_MANAGER_COMMON_FILE_H__
#define __US_MANAGER_COMMON_FILE_H__

#include <linux/dcache.h>
#include <linux/namei.h>

static inline struct dentry *swap_get_dentry(const char *filepath)
{
	struct path path;
	struct dentry *dentry = NULL;

	if (kern_path(filepath, LOOKUP_FOLLOW, &path) == 0) {
		dentry = dget(path.dentry);
		path_put(&path);
	}

	return dentry;
}

static inline void swap_put_dentry(struct dentry *dentry)
{
	dput(dentry);
}

#endif /* __US_MANAGER_COMMON_FILE_H__ */
