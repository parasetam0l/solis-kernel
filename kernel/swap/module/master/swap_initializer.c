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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include "swap_initializer.h"
#include "swap_deps.h"


enum init_level {
	IL_CORE,
	IL_FS
};

static swap_init_t sis_get_fn_init(struct swap_init_struct *init,
				   enum init_level level)
{
	switch (level) {
	case IL_CORE:
		return init->core_init;
	case IL_FS:
		return init->fs_init;
	default:
		return NULL;
	}
}

static swap_uninit_t sis_get_fn_uninit(struct swap_init_struct *init,
				       enum init_level level)
{
	switch (level) {
	case IL_CORE:
		return init->core_uninit;
	case IL_FS:
		return init->fs_uninit;
	}

	return NULL;
}

static void sis_set_flag(struct swap_init_struct *init,
			 enum init_level level, bool val)
{
	switch (level) {
	case IL_CORE:
		init->core_flag = val;
		break;
	case IL_FS:
		init->fs_flag = val;
		break;
	}
}

static bool sis_get_flag(struct swap_init_struct *init, enum init_level level)
{
	switch (level) {
	case IL_CORE:
		return init->core_flag;
	case IL_FS:
		return init->fs_flag;
	}

	return false;
}

static int sis_once(struct swap_init_struct *init)
{
	swap_init_t once;

	once = init->once;
	if (!init->once_flag && once) {
		int ret;

		ret = once();
		if (ret)
			return ret;

		init->once_flag = true;
	}

	return 0;
}

static int sis_init_level(struct swap_init_struct *init, enum init_level level)
{
	int ret;
	swap_init_t fn;

	if (sis_get_flag(init, level))
		return -EPERM;

	fn = sis_get_fn_init(init, level);
	if (fn) {
		ret = fn();
		if (ret)
			return ret;
	}

	sis_set_flag(init, level, true);
	return 0;
}

static void sis_uninit_level(struct swap_init_struct *init,
			     enum init_level level)
{
	if (sis_get_flag(init, level)) {
		swap_uninit_t fn = sis_get_fn_uninit(init, level);
		if (fn)
			fn();
		sis_set_flag(init, level, false);
	}
}

static int sis_init(struct swap_init_struct *init)
{
	int ret;

	ret = sis_once(init);
	if (ret)
		return ret;

	ret = sis_init_level(init, IL_CORE);
	if (ret)
		return ret;

	ret = sis_init_level(init, IL_FS);
	if (ret)
		sis_uninit_level(init, IL_CORE);

	return ret;
}

static void sis_uninit(struct swap_init_struct *init)
{
	sis_uninit_level(init, IL_FS);
	sis_uninit_level(init, IL_CORE);
}

static LIST_HEAD(init_list);
static DEFINE_MUTEX(inst_mutex);
static unsigned init_flag;

static int do_once(void)
{
	int ret;
	struct swap_init_struct *init;

	ret = chef_once();
	if (ret)
		return ret;

	list_for_each_entry(init, &init_list, list) {
		ret = sis_once(init);
		if (ret)
			return ret;
	}

	return 0;
}

static void do_uninit_level(enum init_level level)
{
	struct swap_init_struct *init;

	list_for_each_entry_reverse(init, &init_list, list)
		sis_uninit_level(init, level);
}

static int do_init_level(enum init_level level)
{
	int ret;
	struct swap_init_struct *init;

	list_for_each_entry(init, &init_list, list) {
		ret = sis_init_level(init, level);
		if (ret) {
			do_uninit_level(level);
			return ret;
		}
	}

	return 0;
}

static int do_init(void)
{
	int ret;

	ret = do_once();
	if (ret)
		return ret;

	ret = do_init_level(IL_CORE);
	if (ret)
		return ret;

	ret = do_init_level(IL_FS);
	if (ret)
		do_uninit_level(IL_CORE);

	init_flag = 1;

	return 0;
}

static void do_uninit(void)
{
	do_uninit_level(IL_FS);
	do_uninit_level(IL_CORE);

	init_flag = 0;
}


static atomic_t init_use = ATOMIC_INIT(0);

enum init_stat_t {
	IS_OFF,
	IS_SWITCHING,
	IS_ON,
};

static enum init_stat_t init_stat;
static DEFINE_SPINLOCK(init_stat_lock);


static bool swap_init_try_get(void)
{
	spin_lock(&init_stat_lock);
	if (init_stat != IS_ON) {
		spin_unlock(&init_stat_lock);
		return false;
	}
	spin_unlock(&init_stat_lock);

	atomic_inc(&init_use);

	return true;
}

static void swap_init_put(void)
{
	atomic_dec(&init_use);
}

int swap_init_simple_open(struct inode *inode, struct file *file)
{
	if (swap_init_try_get() == false)
		return -EBUSY;

	return 0;
}
EXPORT_SYMBOL_GPL(swap_init_simple_open);

int swap_init_simple_release(struct inode *inode, struct file *file)
{
	swap_init_put();
	return 0;
}
EXPORT_SYMBOL_GPL(swap_init_simple_release);

int swap_init_init(void)
{
	int ret;

	spin_lock(&init_stat_lock);
	init_stat = IS_SWITCHING;
	spin_unlock(&init_stat_lock);

	ret = do_init();

	spin_lock(&init_stat_lock);
	init_stat = ret ? IS_OFF : IS_ON;
	spin_unlock(&init_stat_lock);

	return ret;
}

int swap_init_uninit(void)
{
	spin_lock(&init_stat_lock);
	init_stat = IS_SWITCHING;
	if (atomic_read(&init_use)) {
		init_stat = IS_ON;
		spin_unlock(&init_stat_lock);
		return -EBUSY;
	}
	spin_unlock(&init_stat_lock);

	do_uninit();

	spin_lock(&init_stat_lock);
	init_stat = IS_OFF;
	spin_unlock(&init_stat_lock);

	return 0;
}


int swap_init_stat_get(void)
{
	mutex_lock(&inst_mutex);

	return init_flag;
}

void swap_init_stat_put(void)
{
	mutex_unlock(&inst_mutex);
}

int swap_init_register(struct swap_init_struct *init)
{
	int ret = 0;

	mutex_lock(&inst_mutex);
	if (init_flag)
		ret = sis_init(init);

	if (ret == 0)
		list_add_tail(&init->list, &init_list);
	mutex_unlock(&inst_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_init_register);

void swap_init_unregister(struct swap_init_struct *init)
{
	mutex_lock(&inst_mutex);
	list_del(&init->list);
	sis_uninit(init);
	mutex_unlock(&inst_mutex);
}
EXPORT_SYMBOL_GPL(swap_init_unregister);
