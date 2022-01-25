/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  energy/lcd/lcd_debugfs.c
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


#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <master/swap_debugfs.h>
#include <energy/lcd/lcd_base.h>
#include <energy/debugfs_energy.h>
#include <energy/rational_debugfs.h>


static int get_system(void *data, u64 *val)
{
	struct lcd_ops *ops = (struct lcd_ops *)data;
	const size_t size = get_lcd_size_array(ops);
	const size_t size_1 = size - 1;
	u64 i_max, j_min, t, e = 0;
	u64 *array_time;
	int i, j;

	array_time = kmalloc(sizeof(*array_time) * size, GFP_KERNEL);
	if (array_time == NULL)
		return -ENOMEM;

	get_lcd_array_time(ops, array_time);

	for (i = 0; i < size; ++i) {
		t = array_time[i];

		/* e = (i * max + (k - i) * min) * t / k */
		j = size_1 - i;
		i_max = div_u64(i * ops->max_coef.num * t,
				ops->max_coef.denom);
		j_min = div_u64(j * ops->min_coef.num * t,
				ops->min_coef.denom);
		e += div_u64(i_max + j_min, size_1);
	}

	kfree(array_time);

	*val = e;

	return 0;
}

SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_get_system, get_system, NULL, "%llu\n");


static struct dentry *lcd_dir;

/**
 * @brief Register LCD in debugfs
 *
 * @param ops LCD operations
 * @return Error code
 */
int register_lcd_debugfs(struct lcd_ops *ops)
{
	int ret;
	struct dentry *dentry, *system;

	if (lcd_dir == NULL)
		return -EINVAL;

	dentry = swap_debugfs_create_dir(ops->name, lcd_dir);
	if (dentry == NULL)
		return -ENOMEM;

	ret = create_rational_files(dentry, &ops->min_coef,
				    "min_num", "min_denom");
	if (ret)
		goto fail;

	ret = create_rational_files(dentry, &ops->max_coef,
				    "max_num", "max_denom");
	if (ret)
		goto fail;

	system = swap_debugfs_create_file("system", 0600, dentry, (void *)ops,
					  &fops_get_system);
	if (system == NULL)
		goto fail;

	ops->dentry = dentry;

	return 0;
fail:
	debugfs_remove_recursive(dentry);
	return -ENOMEM;
}

/**
 * @brief Unregister LCD in debugfs
 *
 * @param ops LCD operations
 * @return Void
 */
void unregister_lcd_debugfs(struct lcd_ops *ops)
{
	debugfs_remove_recursive(ops->dentry);
}

/**
 * @brief Destroy debugfs for LCD
 *
 * @return Void
 */
void exit_lcd_debugfs(void)
{
	if (lcd_dir)
		debugfs_remove_recursive(lcd_dir);

	lcd_dir = NULL;
}

/**
 * @brief Create debugfs for LCD
 *
 * @param dentry Dentry
 * @return Error code
 */
int init_lcd_debugfs(struct dentry *energy_dir)
{
	lcd_dir = swap_debugfs_create_dir("lcd", energy_dir);
	if (lcd_dir == NULL)
		return -ENOMEM;

	return 0;
}
