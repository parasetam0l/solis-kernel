/*
 * To log which slave wakelock prevent AP sleep.
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 * Hunsup Jung <hunsup.jung@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _SLAVE_WAKELOCK_H
#define _SLAVE_WAKELOCK_H

#include <linux/ktime.h>

#define SLWL_ARRAY_SIZE 4
#define SLWL_NAME_LENGTH 15
#define SLWL_IDX_BIT 24
#define SLWL_PREVENT_TIME_MAX BIT(SLWL_IDX_BIT) - 1

#ifdef CONFIG_SLAVE_WAKELOCK
extern int add_slp_mon_slwl_list(char *name);
extern int slave_wake_lock(const char *buf);
extern int slave_wake_unlock(const char *buf);
#else
static int add_slp_mon_slwl_list(char *name){}
static int slave_wake_lock(const char *buf){}
static int slave_wake_unlock(const char *buf){}
#endif /* CONFIG_SLAVE_WAKELOCK */

#ifdef CONFIG_ENERGY_MONITOR
struct slp_mon_slave_wakelocks {
	char slwl_name[SLWL_NAME_LENGTH];
	ktime_t prevent_time;
};
#ifdef CONFIG_SLAVE_WAKELOCK
void get_sleep_monitor_slave_wakelock(int type, struct slp_mon_slave_wakelocks *slwl, int size);
#else
void get_sleep_monitor_slave_wakelock(int type, struct slp_mon_slave_wakelocks *slwl, int size) {}
#endif
#endif /* CONFIG_ENERGY_MONITOR */

#endif /* _SLAVE_WAKELOCK_H */
