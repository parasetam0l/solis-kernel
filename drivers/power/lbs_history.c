/*
 * Copyright (C) 2017 SAMSUNG, Inc.
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
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Hunsup Jung <hunsup.jungsamsung.com>        <2017.07.21>              *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <linux/lbs_history.h>
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif

#define LBS_HISTORY_PREFIX "[lbs_history] "
#define MAX_BUFFER_SIZE 128
#define REQ_TYPE_LEN 4

#define NO_REQUEST		0
#define GPS_REQUEST		BIT(0)
#define WPS_REQUEST		BIT(1)

struct lbs_request {
	struct list_head list;

	char name[TASK_COMM_LEN];
	unsigned int pid;
	u8 request_type_flag;

	ktime_t gps_start_time;
	ktime_t wps_start_time;
	ktime_t gps_total_time;
	ktime_t wps_total_time;

	unsigned int gps_count;
	unsigned int wps_count;

#ifdef CONFIG_ENERGY_MONITOR
	ktime_t e_gps_time;
	ktime_t e_wps_time;
	ktime_t e_gps_dump_time;
	ktime_t e_wps_dump_time;
	unsigned int e_gps_count;
	unsigned int e_wps_count;
#endif
};

static LIST_HEAD(lbs_request_list);
static DEFINE_SPINLOCK(lbs_history_lock);

#ifdef CONFIG_ENERGY_MONITOR
static ktime_t e_check_time;

/* Initial energy_monitor info */
static void init_energy_mon_time(void)
{
	struct  lbs_request *entry;
	ktime_t ktime_zero = ktime_set(0, 0);
	unsigned long flags;

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		entry->e_gps_time = ktime_zero;
		entry->e_wps_time = ktime_zero;
		entry->e_gps_dump_time = ktime_zero;
		entry->e_wps_dump_time = ktime_zero;
		entry->e_gps_count = 0;
		entry->e_wps_count = 0;
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	e_check_time = ktime_get();
}

static int insert_top_gps_time(int type, struct gps_request *gps,
		struct lbs_request *entry, int size)
{
	ktime_t ktime_zero = ktime_set(0, 0);
	int i = 0, j = 0;

	for (i = 0; i < size; i++) {
		/* Insertion in empty space */
		if (ktime_to_ms(gps[i].gps_time) == ktime_to_ms(ktime_zero)) {
			strncpy(gps[i].name, entry->name, TASK_COMM_LEN);
			gps[i].name[TASK_COMM_LEN - 1] = 0;
			if (type != ENERGY_MON_TYPE_DUMP)
				gps[i].gps_time = entry->e_gps_time;
			else
				gps[i].gps_time = entry->e_gps_dump_time;
			break;
		}

		/* Insertion in order */
		if (ktime_to_ms(entry->e_gps_time) > ktime_to_ms(gps[i].gps_time)) {
			for (j = size - 1; j > i; j--) {
				if (ktime_to_ms(gps[j - 1].gps_time) == ktime_to_ms(ktime_zero))
					continue;
				strncpy(gps[j].name, gps[j - 1].name, TASK_COMM_LEN);
				gps[j].name[TASK_COMM_LEN - 1] = 0;
				gps[j].gps_time = gps[j - 1].gps_time;
			}
			strncpy(gps[i].name, entry->name, TASK_COMM_LEN);
			gps[i].name[TASK_COMM_LEN - 1] = 0;
			if (type != ENERGY_MON_TYPE_DUMP)
				gps[i].gps_time = entry->e_gps_time;
			else
				gps[i].gps_time = entry->e_gps_dump_time;
			break;
		}
	}

	return 0;
}

static void insert_top_wps_time(int type, struct wps_request *wps,
		struct lbs_request *entry, int size)
{
	ktime_t ktime_zero = ktime_set(0, 0);
	int i = 0, j = 0;

	for (i = 0; i < size; i++) {
		/* Insertion in empty space */
		if (ktime_to_ms(wps[i].wps_time) == ktime_to_ms(ktime_zero)) {
			strncpy(wps[i].name, entry->name, TASK_COMM_LEN);
			wps[i].name[TASK_COMM_LEN - 1] = 0;
			if (type != ENERGY_MON_TYPE_DUMP)
				wps[i].wps_time = entry->e_wps_time;
			else
				wps[i].wps_time = entry->e_wps_dump_time;
			break;
		}

		/* Insertion in order */
		if (ktime_to_ms(entry->e_wps_time) > ktime_to_ms(wps[i].wps_time)) {
			for (j = size - 1; j > i; j--) {
				if (ktime_to_ms(wps[j - 1].wps_time) == ktime_to_ms(ktime_zero))
					continue;
				strncpy(wps[j].name, wps[j - 1].name, TASK_COMM_LEN);
				wps[j].name[TASK_COMM_LEN - 1] = 0;
				wps[j].wps_time = wps[j - 1].wps_time;
			}
			strncpy(wps[i].name, entry->name, TASK_COMM_LEN);
			wps[i].name[TASK_COMM_LEN - 1] = 0;
			if (type != ENERGY_MON_TYPE_DUMP)
				wps[i].wps_time = entry->e_wps_time;
			else
				wps[i].wps_time = entry->e_wps_dump_time;
			break;
		}
	}
}

static void sort_top_lbs_time(int type, struct gps_request *gps,
		struct wps_request *wps, int size)
{
	struct lbs_request *entry;
	unsigned long flags;

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (type != ENERGY_MON_TYPE_DUMP && ktime_to_ms(entry->e_gps_time) <= 0)
			continue;
		if (type == ENERGY_MON_TYPE_DUMP && ktime_to_ms(entry->e_gps_dump_time) <= 0)
			continue;

		insert_top_gps_time(type, gps, entry, size);
	}

	list_for_each_entry(entry, &lbs_request_list, list) {
		if (type != ENERGY_MON_TYPE_DUMP && ktime_to_ms(entry->e_wps_time) <= 0)
			continue;
		if (type == ENERGY_MON_TYPE_DUMP && ktime_to_ms(entry->e_wps_dump_time) <= 0)
			continue;

		insert_top_wps_time(type, wps, entry, size);
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);
}

/* return top e_gps_time and e_wps_time */
void get_top_lbs_time(int type, struct gps_request *gps,
		struct wps_request *wps, int size)
{
	struct lbs_request *entry;
	ktime_t ktime_zero = ktime_set(0, 0);
	ktime_t gps_time = ktime_zero;
	ktime_t wps_time = ktime_zero;
	unsigned long flags;
	int i = 0;

	for (i = 0; i < size; i++) {
		memset(gps[i].name, 0, TASK_COMM_LEN);
		memset(wps[i].name, 0, TASK_COMM_LEN);
		gps[i].gps_time = ktime_zero;
		wps[i].wps_time = ktime_zero;
	}

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (entry->request_type_flag == GPS_REQUEST)
			continue;

		/* update gps time */
		if (ktime_to_ms(entry->gps_start_time) > ktime_to_ms(e_check_time))
			gps_time = ktime_sub(ktime_get(), entry->gps_start_time);
		else
			gps_time = ktime_sub(ktime_get(), e_check_time);

		if (type != ENERGY_MON_TYPE_DUMP)
			entry->e_gps_time = ktime_add(entry->e_gps_time, gps_time);
		else
			entry->e_gps_dump_time = ktime_add(entry->e_gps_time, gps_time);
	}

	list_for_each_entry(entry, &lbs_request_list, list) {
		if (entry->request_type_flag == WPS_REQUEST)
			continue;

		/* update wps time */
		if (ktime_to_ms(entry->wps_start_time) > ktime_to_ms(e_check_time))
			wps_time = ktime_sub(ktime_get(), entry->wps_start_time);
		else
			wps_time = ktime_sub(ktime_get(), e_check_time);
		if (type != ENERGY_MON_TYPE_DUMP)
			entry->e_wps_time = ktime_add(entry->e_wps_time, wps_time);
		else
			entry->e_wps_dump_time = ktime_add(entry->e_wps_time, wps_time);
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	sort_top_lbs_time(type, gps, wps, size);

	if (type != ENERGY_MON_TYPE_DUMP)
		init_energy_mon_time();
}
#endif

/* Initialize lbs_request */
static void init_lbs_request(struct lbs_request *lbs)
{
	ktime_t ktime_zero = ktime_set(0, 0);
	memset(lbs->name, 0, TASK_COMM_LEN);
	lbs->pid = 0;
	lbs->request_type_flag = NO_REQUEST;

	lbs->gps_start_time = ktime_zero;
	lbs->wps_start_time = ktime_zero;
	lbs->gps_total_time = ktime_zero;
	lbs->wps_total_time = ktime_zero;

	lbs->gps_count = 0;
	lbs->wps_count = 0;

#ifdef CONFIG_ENERGY_MONITOR
	lbs->e_gps_time = ktime_zero;
	lbs->e_wps_time = ktime_zero;
	lbs->e_gps_dump_time = ktime_zero;
	lbs->e_wps_dump_time = ktime_zero;
	lbs->e_gps_count = 0;
	lbs->e_wps_count = 0;
#endif
}

static int gps_request_start(const char *name, unsigned int pid)
{
	struct lbs_request *lbs, *entry;
	ktime_t ktime_zero = ktime_set(0, 0);
	unsigned long flags;

	/* Already added instance */
	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (!strncmp(entry->name, name, TASK_COMM_LEN - 1)) {
			if (entry->request_type_flag & GPS_REQUEST) {
				spin_unlock_irqrestore(&lbs_history_lock, flags);
				return 0;
			}
			entry->pid = pid;
			entry->request_type_flag |= GPS_REQUEST;
			if (ktime_to_ms(entry->gps_start_time) == ktime_to_ms(ktime_zero))
				entry->gps_start_time = ktime_get();
			entry->gps_count++;
			spin_unlock_irqrestore(&lbs_history_lock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	/* Add new instance */
	lbs = kmalloc(sizeof(struct lbs_request), GFP_KERNEL);
	if (!lbs)
		return -ENOMEM;

	init_lbs_request(lbs);

	lbs->pid = pid;
	strncpy(lbs->name, name, TASK_COMM_LEN - 1);
	lbs->name[TASK_COMM_LEN - 1] = 0;

	lbs->request_type_flag |= GPS_REQUEST;

	lbs->gps_start_time = ktime_get();
	lbs->gps_count++;

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_add_tail(&lbs->list, &lbs_request_list);
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	return 0;
}

static int wps_request_start(const char *name, unsigned int pid)
{
	struct lbs_request *lbs, *entry;
	ktime_t ktime_zero = ktime_set(0, 0);
	unsigned long flags;

	/* Already added instance */
	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (!strncmp(entry->name, name, TASK_COMM_LEN - 1)) {
			if (entry->request_type_flag & WPS_REQUEST) {
				spin_unlock_irqrestore(&lbs_history_lock, flags);
				return 0;
			}
			entry->pid = pid;
			entry->request_type_flag |= WPS_REQUEST;
			if (ktime_to_ms(entry->wps_start_time) == ktime_to_ms(ktime_zero))
				entry->wps_start_time = ktime_get();
			entry->wps_count++;
			spin_unlock_irqrestore(&lbs_history_lock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	/* Add new instance */
	lbs = kmalloc(sizeof(struct lbs_request), GFP_KERNEL);
	if (!lbs)
		return -ENOMEM;

	init_lbs_request(lbs);

	lbs->pid = pid;
	strncpy(lbs->name, name, TASK_COMM_LEN - 1);
	lbs->name[TASK_COMM_LEN - 1] = 0;

	lbs->request_type_flag |= WPS_REQUEST;

	lbs->wps_start_time = ktime_get();
	lbs->wps_count++;

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_add_tail(&lbs->list, &lbs_request_list);
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	return 0;
}

static void gps_request_stop(const char *name, unsigned int pid)
{
	struct lbs_request *entry;
	ktime_t diff_time = ktime_set(0, 0);
	unsigned long flags;

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (!strncmp(entry->name, name, TASK_COMM_LEN - 1)) {
			if (!(entry->request_type_flag & GPS_REQUEST)) {
				spin_unlock_irqrestore(&lbs_history_lock, flags);
				return;
			}
			diff_time = ktime_sub(ktime_get(), entry->gps_start_time);
			entry->gps_total_time = ktime_add(entry->gps_total_time, diff_time);
#ifdef CONFIG_ENERGY_MONITOR
{
			ktime_t gps_time = ktime_set(0, 0);

			if (ktime_to_ms(entry->gps_start_time) > ktime_to_ms(e_check_time))
				gps_time = ktime_sub(ktime_get(), entry->gps_start_time);
			else
				gps_time = ktime_sub(ktime_get(), e_check_time);
			entry->e_gps_time = ktime_add(entry->e_gps_time, gps_time);
			entry->e_gps_dump_time = entry->e_gps_time;
}
#endif
			entry->gps_start_time = ktime_set(0, 0);
			entry->request_type_flag &= ~GPS_REQUEST;

			spin_unlock_irqrestore(&lbs_history_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);
}

static void wps_request_stop(const char *name, unsigned int pid)
{
	struct lbs_request *entry;
	ktime_t diff_time = ktime_set(0, 0);
	unsigned long flags;

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (!strncmp(entry->name, name, TASK_COMM_LEN - 1)) {
			if (!(entry->request_type_flag & WPS_REQUEST)) {
				spin_unlock_irqrestore(&lbs_history_lock, flags);
				return;
			}
			diff_time = ktime_sub(ktime_get(), entry->wps_start_time);
			entry->wps_total_time = ktime_add(entry->wps_total_time, diff_time);
#ifdef CONFIG_ENERGY_MONITOR
{
			ktime_t wps_time = ktime_set(0, 0);

			if (ktime_to_ms(entry->wps_start_time) > ktime_to_ms(e_check_time))
				wps_time = ktime_sub(ktime_get(), entry->wps_start_time);
			else
				wps_time = ktime_sub(ktime_get(), e_check_time);
			entry->e_wps_time = ktime_add(entry->e_wps_time, wps_time);
			entry->e_wps_dump_time = entry->e_wps_time;
}
#endif
			entry->wps_start_time = ktime_set(0, 0);
			entry->request_type_flag &= ~WPS_REQUEST;

			spin_unlock_irqrestore(&lbs_history_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);
}

static ssize_t request_start_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct task_struct *task;
	char buf[MAX_BUFFER_SIZE] = {};
	char *str;

	char type[REQ_TYPE_LEN] = {0};
	char name[TASK_COMM_LEN] = {0};
	int pid = 0, pos = -1;

	/* Copy_from_user */
	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, user_buf, count))
		return -EFAULT;
	buf[count] = '\0';
	str = strstrip(buf);

	/* Request type length is 3 */
	if (sscanf(str, "%d %3s %n", &pid, type, &pos) != 2) {
		pr_err(LBS_HISTORY_PREFIX"Invalid number of arguments passed\n");
		return -EINVAL;
	}

	/* Check pid */
	if (pid == 0) {
		/* If there's no pid, return -EINVAL */
		pr_err(LBS_HISTORY_PREFIX"Invalid pid(%d)\n", pid);
		return -EINVAL;
	}

	/* Get process name */
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (task) {
		strncpy(name, task->comm, TASK_COMM_LEN - 1);
		name[TASK_COMM_LEN - 1] = 0;
	} else {
		pr_err(LBS_HISTORY_PREFIX"No task(%d)\n", pid);
		return -EINVAL;
	}

	/* Get request type and call request func */
	if (!strncmp(type, "GPS", REQ_TYPE_LEN)) {
		pr_info(LBS_HISTORY_PREFIX"GPS: %s: %s(%d)\n", __func__, name, pid);
		gps_request_start(name, pid);
	} else if (!strncmp(type, "WPS", REQ_TYPE_LEN)) {
		pr_info(LBS_HISTORY_PREFIX"WPS: %s: %s(%d)\n", __func__, name, pid);
		wps_request_start(name, pid);
	} else {
		pr_err(LBS_HISTORY_PREFIX"Invalid type %s\n", type);
		return -EINVAL;
	}

	return count;
}

static ssize_t request_stop_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct task_struct *task;
	char buf[MAX_BUFFER_SIZE] = {};
	char *str;

	char type[REQ_TYPE_LEN] = {0};
	char name[TASK_COMM_LEN] = {0};
	int pid = 0, pos = -1;

	/* Copy_from_user */
	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, user_buf, count))
		return -EFAULT;
	buf[count] = '\0';
	str = strstrip(buf);

	/* Request type length is 3 */
	if (sscanf(str, "%d %3s %n", &pid, type, &pos) != 2) {
		pr_err(LBS_HISTORY_PREFIX"Invalid number of arguments passed\n");
		return -EINVAL;
	}

	/* Check pid */
	if (pid == 0) {
		/* If there's no pid, return -EINVAL */
		pr_err(LBS_HISTORY_PREFIX"Invalid pid(%d)\n", pid);
		return -EINVAL;
	}

	/* Get process name */
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (task) {
		strncpy(name, task->comm, TASK_COMM_LEN - 1);
		name[TASK_COMM_LEN - 1] = 0;
	} else {
		pr_err(LBS_HISTORY_PREFIX"No task(%d)\n", pid);
		return -EINVAL;
	}

	/* Get request type and call request func */
	if (!strncmp(type, "GPS", REQ_TYPE_LEN)) {
		pr_info(LBS_HISTORY_PREFIX"GPS: %s: %s(%d)\n", __func__, name, pid);
		gps_request_stop(name, pid);
	} else if (!strncmp(type, "WPS", REQ_TYPE_LEN)) {
		pr_info(LBS_HISTORY_PREFIX"WPS: %s: %s(%d)\n", __func__, name, pid);
		wps_request_stop(name, pid);
	} else {
		pr_err(LBS_HISTORY_PREFIX"Invalid type %s\n", type);
		return -EINVAL;
	}

	return count;
}

static int lbs_history_show(struct seq_file *m, void *v)
{
	struct lbs_request *entry;
	unsigned long flags;
	ktime_t gps_time, wps_time, diff_time;

	seq_printf(m, "name            current_req  gps_count  gps_time  "
			"wps_count  wps_time\n");

	spin_lock_irqsave(&lbs_history_lock, flags);
	list_for_each_entry(entry, &lbs_request_list, list) {
		if (entry->request_type_flag & GPS_REQUEST) {
			diff_time = ktime_sub(ktime_get(), entry->gps_start_time);
			gps_time = ktime_add(entry->gps_total_time, diff_time);
		} else
			gps_time = entry->gps_total_time;

		if (entry->request_type_flag & WPS_REQUEST) {
			diff_time = ktime_sub(ktime_get(), entry->wps_start_time);
			wps_time = ktime_add(entry->wps_total_time, diff_time);
		} else
			wps_time = entry->wps_total_time;

		seq_printf(m, "%-15s %11u  %9u  %8lld  %9u  %8lld\n",
				entry->name, entry->request_type_flag,
				entry->gps_count, ktime_to_ms(gps_time),
				entry->wps_count, ktime_to_ms(wps_time));
	}
	spin_unlock_irqrestore(&lbs_history_lock, flags);

	return 0;
}

static const struct file_operations request_start_fops = {
	.write		= request_start_write,
};

static const struct file_operations request_stop_fops = {
	.write		= request_stop_write,
};

static int lbs_history_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, lbs_history_show, NULL);
}

static const struct file_operations lbs_history_fops = {
	.open		= lbs_history_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int lbs_history_init(void)
{
	static struct dentry *root;

	pr_debug("%s\n", __func__);

	root = debugfs_create_dir("lbs_history", NULL);
	if (!root) {
		pr_err(LBS_HISTORY_PREFIX"failed to create lbs_history debugfs dir\n");
		return -ENOMEM;
	}
	if (!debugfs_create_file("request_start", 0220, root, NULL, &request_start_fops))
		goto error_debugfs;
	if (!debugfs_create_file("request_stop", 0220, root, NULL, &request_stop_fops))
		goto error_debugfs;
	if (!debugfs_create_file("lbs_history_show", 0440, root, NULL, &lbs_history_fops))
		goto error_debugfs;

#ifdef CONFIG_ENERGY_MONITOR
	e_check_time = ktime_get();
#endif

	return 0;

error_debugfs:
	debugfs_remove_recursive(root);
	return -1;
}

static void lbs_history_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(lbs_history_init);
module_exit(lbs_history_exit);
