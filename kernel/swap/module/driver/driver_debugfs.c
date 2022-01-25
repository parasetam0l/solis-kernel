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
 * Copyright (C) Samsung Electronics, 2016
 *
 * @section DESCRIPTION
 *
 * SWAP debugfs interface definition.
 */

#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <master/swap_debugfs.h>
#include "driver_to_buffer.h"
#include "swap_driver_errors.h"


#define WRITER_DBG_PERMS (S_IRUSR | S_IWUSR) /* u+rw */


static int buf_enabled_set(u64 val)
{
	int ret = -EINVAL;

	switch (val) {
	case 0:
		ret = driver_to_buffer_uninitialize();
		break;
	case 1:
		ret = driver_to_buffer_initialize();
		break;
	}

	return ret;
}

static u64 buf_enabled_get(void)
{
	return driver_to_buffer_enabled();
}

static struct dfs_setget_64 dfs_enabled = {
	.set = buf_enabled_set,
	.get = buf_enabled_get,
};

static int subbuf_size_set(u64 val)
{

	if (driver_to_buffer_set_size(val) != E_SD_SUCCESS)
		return -EINVAL;

	return 0;
}

static u64 subbuf_size_get(void)
{
	return driver_to_buffer_get_size();
}

static struct dfs_setget_64 dfs_subbuf_size = {
	.set = subbuf_size_set,
	.get = subbuf_size_get,
};

static int subbuf_count_set(u64 val)
{
	if (driver_to_buffer_set_count(val) != E_SD_SUCCESS)
		return -EINVAL;

	return 0;
}

static u64 subbuf_count_get(void)
{
	return driver_to_buffer_get_count();
}

static struct dfs_setget_64 dfs_subbuf_count = {
	.set = subbuf_count_set,
	.get = subbuf_count_get,
};


struct dbgfs_data {
	const char *name;
	struct dfs_setget_64 *setget;
};

static struct dbgfs_data dbgfs[] = {
	{
		.name = "buffer_enabled",
		.setget = &dfs_enabled,
	}, {
		.name = "subbuf_size",
		.setget = &dfs_subbuf_size,
	}, {
		.name = "subbuf_conunt",
		.setget = &dfs_subbuf_count,
	}
};


static struct dentry *driver_dir;

void driver_debugfs_uninit(void)
{
	debugfs_remove_recursive(driver_dir);
	driver_dir = NULL;
}

int driver_debugfs_init(void)
{
	int i;
	struct dentry *swap_dir, *dentry;

	swap_dir = swap_debugfs_getdir();
	if (swap_dir == NULL)
		return -ENOENT;

	driver_dir = swap_debugfs_create_dir("driver", swap_dir);
	if (driver_dir == NULL)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(dbgfs); ++i) {
		struct dbgfs_data *data = &dbgfs[i];
		dentry = swap_debugfs_create_setget_u64(data->name,
							WRITER_DBG_PERMS,
							driver_dir,
							data->setget);
		if (!dentry)
			goto fail;
	}

	return 0;
fail:
	driver_debugfs_uninit();
	return -ENOMEM;
}
