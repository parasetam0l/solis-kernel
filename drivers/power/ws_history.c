/*
 * Copyright (C) 2016 SAMSUNG, Inc.
 * Hunsup Jung<hunsup.jung@samsung.com>
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

#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/rculist.h>
#include <linux/ktime.h>
#include <linux/slab.h>

#include <linux/power/ws_history.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/uaccess.h>
#include <linux/power/sleep_monitor.h>
#endif

struct ws_history {
	struct list_head entry;
	char name[WS_HISTORY_NAME_LENGTH];
	ktime_t prevent_time;
};

static LIST_HEAD(ws_history_list);

static int ws_history_count;
static int ws_history_idx[WS_HISTORY_ARRAY_SIZE];
static char ws_history_name[WS_HISTORY_ARRAY_SIZE][WS_HISTORY_NAME_LENGTH];
static ktime_t ws_history_prv_time[WS_HISTORY_ARRAY_SIZE];

#ifdef CONFIG_SLEEP_MONITOR
static unsigned int get_pretty_value(ktime_t prv_time)
{
	unsigned int pretty = 0;
	s64 prv_time_ms = 0;

	prv_time_ms = ktime_to_ms(prv_time);
	if (prv_time_ms > SLP_MON_WS_TIME_MAX)
		prv_time_ms = SLP_MON_WS_TIME_MAX;

	pretty = (prv_time_ms + SLP_MON_TIME_INTERVAL_MS - 1) / SLP_MON_TIME_INTERVAL_MS;
	if (pretty > DEVICE_UNKNOWN)
		return DEVICE_UNKNOWN;
	else
		return pretty;
}

static int slp_mon_ws_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	if (ws_history_count < 1)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		*raw_val = ws_history_idx[0] << SLP_MON_WS_IDX_BIT;
		if (ktime_to_ms(ws_history_prv_time[0]) > SLP_MON_WS_TIME_MAX)
			*raw_val |= SLP_MON_WS_TIME_MAX;
		else
			*raw_val |= ktime_to_ms(ws_history_prv_time[0]);
		return get_pretty_value(ws_history_prv_time[0]);
	} else
		return 0;
}

static int slp_mon_ws1_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	if (ws_history_count < 2)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		*raw_val = ws_history_idx[1] << SLP_MON_WS_IDX_BIT;
		if (ktime_to_ms(ws_history_prv_time[1]) > SLP_MON_WS_TIME_MAX)
			*raw_val |= SLP_MON_WS_TIME_MAX;
		else
			*raw_val |= ktime_to_ms(ws_history_prv_time[1]);
		return get_pretty_value(ws_history_prv_time[1]);
	} else
		return 0;
}

static int slp_mon_ws2_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	if (ws_history_count < 3)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		*raw_val = ws_history_idx[2] << SLP_MON_WS_IDX_BIT;
		if (ktime_to_ms(ws_history_prv_time[2]) > SLP_MON_WS_TIME_MAX)
			*raw_val |= SLP_MON_WS_TIME_MAX;
		else
			*raw_val |= ktime_to_ms(ws_history_prv_time[2]);
		return get_pretty_value(ws_history_prv_time[2]);
	} else
		return 0;
}

static int slp_mon_ws3_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	if (ws_history_count < 4)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		*raw_val = ws_history_idx[3] << SLP_MON_WS_IDX_BIT;
		if (ktime_to_ms(ws_history_prv_time[3]) > SLP_MON_WS_TIME_MAX)
			*raw_val |= SLP_MON_WS_TIME_MAX;
		else
			*raw_val |= ktime_to_ms(ws_history_prv_time[3]);
		return get_pretty_value(ws_history_prv_time[3]);
	} else
		return 0;
}

static struct sleep_monitor_ops slp_mon_ws_dev = {
	.read_cb_func = slp_mon_ws_cb,
};

static struct sleep_monitor_ops slp_mon_ws_dev1 = {
	.read_cb_func = slp_mon_ws1_cb,
};

static struct sleep_monitor_ops slp_mon_ws_dev2 = {
	.read_cb_func = slp_mon_ws2_cb,
};

static struct sleep_monitor_ops slp_mon_ws_dev3 = {
	.read_cb_func = slp_mon_ws3_cb,
};

static ssize_t slp_mon_read_ws_list(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	struct ws_history *iter;
	char *buf = NULL;
	ssize_t ret = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, PAGE_SIZE);

	if (*ppos == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%08x]%s", special_key,
				get_type_marker(SLEEP_MONITOR_CALL_WS_LIST));
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &ws_history_list, entry)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s/", iter->name);
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		rcu_read_unlock();
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static const struct file_operations slp_mon_ws_list_ops = {
	.read   = slp_mon_read_ws_list,
};

static void register_ws_his_slp_mon_cb(void)
{
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev, SLEEP_MONITOR_WS);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev1, SLEEP_MONITOR_WS1);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev2, SLEEP_MONITOR_WS2);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev3, SLEEP_MONITOR_WS3);

	if (slp_mon_d) {
		struct dentry *d = debugfs_create_file("slp_mon_ws", 0400,
				slp_mon_d, NULL, &slp_mon_ws_list_ops);
		if (!d)
			pr_err("%s : debugfs_create_file, error\n", "slp_mon_ws");
	} else
		pr_info("%s - dentry not defined\n", __func__);
}
#endif

static void init_ws_history_list(void)
{
	ktime_t ktime_zero = ktime_set(0, 0);
	int i = 0;

	for (i = 0; i < ws_history_count; i++) {
		memset(ws_history_name[i], 0, WS_HISTORY_ARRAY_SIZE);
		ws_history_prv_time[i] = ktime_zero;
		ws_history_idx[i] = 0;
	}
	ws_history_count = 0;
}

static void sort_ws_history_prv_time(void)
{
	struct ws_history *iter;
	ktime_t ktime_zero = ktime_set(0, 0);
	int i = 0, j = 0, idx = 0;

	init_ws_history_list();

	rcu_read_lock();
	list_for_each_entry_rcu(iter, &ws_history_list, entry) {

		if (ktime_to_ms(iter->prevent_time) <= 0) {
			idx++;
			continue;
		}

		for (i = 0; i < WS_HISTORY_ARRAY_SIZE; i++) {
			/* Insertion in empty space */
			if (ktime_to_ms(ws_history_prv_time[i]) == ktime_to_ms(ktime_zero)) {
				ws_history_idx[i] = idx;
				strncpy(ws_history_name[i], iter->name, WS_HISTORY_NAME_LENGTH);
				ws_history_name[i][WS_HISTORY_NAME_LENGTH - 1] = 0;
				ws_history_prv_time[i] = iter->prevent_time;
				break;
			}

			/* Insertion in order */
			if (ktime_to_ms(iter->prevent_time) > ktime_to_ms(ws_history_prv_time[i])) {
				for (j = WS_HISTORY_ARRAY_SIZE - 1; j > i; j--) {
					if (ktime_to_ms(ws_history_prv_time[j - 1]) == ktime_to_ms(ktime_zero))
						continue;
					ws_history_idx[j] = ws_history_idx[j - 1];
					strncpy(ws_history_name[j], ws_history_name[j - 1], WS_HISTORY_NAME_LENGTH);
					ws_history_name[j][WS_HISTORY_NAME_LENGTH - 1] = 0;
					ws_history_prv_time[j] = ws_history_prv_time[j - 1];
				}
				ws_history_idx[i] = idx;
				strncpy(ws_history_name[i], iter->name, WS_HISTORY_NAME_LENGTH);
				ws_history_name[i][WS_HISTORY_NAME_LENGTH - 1] = 0;
				ws_history_prv_time[i] = iter->prevent_time;
				break;
			}
		}
		idx++;
	}
	rcu_read_unlock();

	for (i = 0; i < WS_HISTORY_ARRAY_SIZE; i++) {
		if (ktime_to_ms(ws_history_prv_time[i]) != ktime_to_ms(ktime_zero))
			ws_history_count++;
		else
			break;

	}
}

void update_ws_history_prv_time(void)
{
	struct ws_history *iter;
	ktime_t ktime_zero = ktime_set(0, 0);
	int i;

	sort_ws_history_prv_time();

	pr_cont("PM: ws_history_list: ");
	for (i = 0; i < ws_history_count; i++)
		pr_cont("%s(%lld)/", ws_history_name[i], ktime_to_ms(ws_history_prv_time[i]));
	pr_info("");

	list_for_each_entry_rcu(iter, &ws_history_list, entry)
		iter->prevent_time = ktime_zero;
}
EXPORT_SYMBOL_GPL(update_ws_history_prv_time);

/* Add wakeup source to ws_history_list */
int add_ws_history(const char *name, ktime_t prevent_time)
{
	struct ws_history *ws_history_ins, *iter;

	if (!name) {
		pr_info("%s: Invalid argument\n", __func__);
		return -EINVAL;
	}

	rcu_read_lock();
	list_for_each_entry_rcu(iter, &ws_history_list, entry) {
		if (!strncmp(iter->name, name, WS_HISTORY_NAME_LENGTH - 1)) {
			iter->prevent_time = ktime_add(iter->prevent_time, prevent_time);
			rcu_read_unlock();
			return 0;
		}
	}
	rcu_read_unlock();

	ws_history_ins = kmalloc(sizeof(struct ws_history), GFP_ATOMIC);
	if (!ws_history_ins)
		return -ENOMEM;

	strncpy(ws_history_ins->name, name, WS_HISTORY_NAME_LENGTH);
	ws_history_ins->name[WS_HISTORY_NAME_LENGTH - 1] = 0;
	ws_history_ins->prevent_time = prevent_time;
	list_add_tail(&ws_history_ins->entry, &ws_history_list);

	return 0;
}
EXPORT_SYMBOL_GPL(add_ws_history);

static int ws_history_init(void)
{
	pr_info("%s\n", __func__);

#ifdef CONFIG_SLEEP_MONITOR
	register_ws_his_slp_mon_cb();
#endif

	return 0;
}


static void ws_history_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(ws_history_init);
module_exit(ws_history_exit);
