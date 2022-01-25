/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  energy/rational_debugfs.c
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
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/dcache.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <master/swap_debugfs.h>
#include "debugfs_energy.h"
#include "rational_debugfs.h"


static int denom_set(void *data, u64 val)
{
	if (val == 0)
		return -EINVAL;

	*(u64 *)data = val;
	return 0;
}

static int denom_get(void *data, u64 *val)
{
	*val = *(u64 *)data;
	return 0;
}

SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_denom, denom_get, denom_set, "%llu\n");

/**
 * @brief Create file in debugfs for rational struct
 *
 * @param parent Dentry parent
 * @param r Pointer to the rational struct
 * @param num_name File name of numerator
 * @param denom_name File name of denominator
 * @return Error code
 */
int create_rational_files(struct dentry *parent, struct rational *r,
			  const char *num_name, const char *denom_name)
{
	struct dentry *d_num, *d_denom;

	d_num = swap_debugfs_create_u64(num_name, 0600, parent, &r->num);
	if (d_num == NULL)
		return -ENOMEM;

	d_denom = swap_debugfs_create_file(denom_name, 0600, parent, &r->denom,
					   &fops_denom);
	if (d_denom == NULL) {
		debugfs_remove(d_num);
		return -ENOMEM;
	}

	return 0;
}
