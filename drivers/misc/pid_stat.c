/* drivers/misc/pid_stat.c
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2015>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.05.16>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 * 1.2   Hunsup Jung <hunsup.jung@samsung.com>       <2017.06.12>              *
 *                                                   Just release version 1.2  *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>

#include <linux/pid_stat.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif /* CONFIG_SLEEP_MONITOR */

#define PID_STAT_PREFIX	"pid_stat:  "

struct pid_stat {
	struct list_head list;
	pid_t pid;
	char comm[TASK_COMM_LEN];

	atomic_t total_rcv;
	atomic_t total_snd;
	atomic_t total_rcv_count;
	atomic_t total_snd_count;
	atomic_t current_rcv;
	atomic_t current_snd;
	atomic_t current_rcv_count;
	atomic_t current_snd_count;

	atomic_t activity;
	atomic_t total_activity;
	ktime_t last_transmit;
	int suspend_count;

#ifdef CONFIG_ENERGY_MONITOR
	unsigned int energy_mon_transmit;
	unsigned int energy_mon_count;
#endif
};

static DEFINE_SPINLOCK(pid_lock);
static LIST_HEAD(pid_list);

static ktime_t resume_time;

#ifdef CONFIG_ENERGY_MONITOR
static void init_energy_mon_info(void)
{
	struct pid_stat *entry;
	unsigned long flags;

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		entry->energy_mon_transmit = 0;
		entry->energy_mon_count = 0;
	}
	spin_unlock_irqrestore(&pid_lock, flags);
}
#endif

static struct pid_stat *find_pid_stat(pid_t pid)
{
	unsigned long flags;
	struct pid_stat *entry;
	struct task_struct *tsk;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk) {
		rcu_read_unlock();
		return ERR_PTR(-ESRCH);
	}
	rcu_read_unlock();

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		if (!strcmp(tsk->comm, entry->comm)) {
			spin_unlock_irqrestore(&pid_lock, flags);
			return entry;
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);
	return NULL;
}

/* Create a new entry for tracking the specified pid. */
static struct pid_stat *create_stat(pid_t pid)
{
	unsigned long flags;
	struct task_struct *tsk;
	struct pid_stat *new_pid;

	/* Create the pid stat struct and append it to the list. */
	new_pid = kmalloc(sizeof(struct pid_stat), GFP_KERNEL);
	if (new_pid == NULL)
		return NULL;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk) {
		rcu_read_unlock();
		kfree(new_pid);
		return ERR_PTR(-ESRCH);
	}
	rcu_read_unlock();
	memcpy(new_pid->comm, tsk->comm, TASK_COMM_LEN);

	/* Counters start at INT_MIN, so we can track 4GB of traffic. */
	atomic_set(&new_pid->total_rcv, INT_MIN);
	atomic_set(&new_pid->total_snd, INT_MIN);
	atomic_set(&new_pid->total_rcv_count, INT_MIN);
	atomic_set(&new_pid->total_snd_count, INT_MIN);

	atomic_set(&new_pid->current_rcv, INT_MIN);
	atomic_set(&new_pid->current_snd, INT_MIN);
	atomic_set(&new_pid->current_rcv_count, INT_MIN);
	atomic_set(&new_pid->current_snd_count, INT_MIN);

	atomic_set(&new_pid->activity, INT_MIN);
	atomic_set(&new_pid->total_activity, INT_MIN);

#ifdef CONFIG_ENERGY_MONITOR
	new_pid->energy_mon_transmit = 0;
	new_pid->energy_mon_count = 0;
#endif

	spin_lock_irqsave(&pid_lock, flags);
	list_add_tail(&new_pid->list, &pid_list);
	spin_unlock_irqrestore(&pid_lock, flags);

	return new_pid;
}

int pid_stat_tcp_snd(pid_t pid, int size)
{
	struct pid_stat *entry;

	entry = find_pid_stat(pid);
	if (entry == NULL) {
		entry = create_stat(pid);

		if (entry == NULL)
			return -1;
	}

	entry->last_transmit = ktime_get();

	atomic_add(size, &entry->total_snd);
	atomic_inc(&entry->total_snd_count);
	atomic_add(size, &entry->current_snd);
	atomic_inc(&entry->current_snd_count);

	atomic_set(&entry->activity, INT_MIN + 1);
	entry->suspend_count = suspend_stats.success;

#ifdef CONFIG_ENERGY_MONITOR
	entry->energy_mon_transmit += size;
	entry->energy_mon_count++;
#endif

	return 0;
}

int pid_stat_tcp_rcv(pid_t pid, int size)
{
	struct pid_stat *entry;

	entry = find_pid_stat(pid);
	if (entry == NULL) {
		entry = create_stat(pid);

		if (entry == NULL)
			return -1;
	}

	entry->last_transmit = ktime_get();

	atomic_add(size, &entry->total_rcv);
	atomic_inc(&entry->total_rcv_count);
	atomic_add(size, &entry->current_rcv);
	atomic_inc(&entry->current_rcv_count);

	atomic_set(&entry->activity, INT_MIN + 1);
	entry->suspend_count = suspend_stats.success;

#ifdef CONFIG_ENERGY_MONITOR
	entry->energy_mon_transmit += size;
	entry->energy_mon_count++;
#endif

	return 0;
}

static int pid_stat_show(struct seq_file *m, void *v)
{
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count;
	unsigned int total_snd_bytes, total_rcv_bytes;
	unsigned int total_snd_count, total_rcv_count;
	unsigned int transmit_count, total_transmit_count;
	unsigned int activity, total_activity;
	struct pid_stat *entry;

	seq_printf(m, "name            transmit_count "
				"snd_count snd_bytes "
				"rcv_count rcv_bytes "
				"activity total_activity total_transmit_count "
				"total_snd_count total_snd_bytes "
				"total_rcv_count total_rcv_bytes "
				"last_transmit suspend_count\n");
	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		snd_bytes = (unsigned int) (atomic_read(&entry->current_snd) + INT_MIN);
		rcv_bytes = (unsigned int) (atomic_read(&entry->current_rcv) + INT_MIN);
		snd_count = (unsigned int) (atomic_read(&entry->current_snd_count) + INT_MIN);
		rcv_count = (unsigned int) (atomic_read(&entry->current_rcv_count) + INT_MIN);
		activity = (unsigned int) (atomic_read(&entry->activity) + INT_MIN);

		total_snd_bytes = (unsigned int) (atomic_read(&entry->total_snd) + INT_MIN);
		total_rcv_bytes = (unsigned int) (atomic_read(&entry->total_rcv) + INT_MIN);
		total_snd_count = (unsigned int) (atomic_read(&entry->total_snd_count) + INT_MIN);
		total_rcv_count = (unsigned int) (atomic_read(&entry->total_rcv_count) + INT_MIN);
		total_activity = (unsigned int) (atomic_read(&entry->total_activity) + INT_MIN) + activity;

		transmit_count = snd_count + rcv_count;
		total_transmit_count = total_snd_count + total_rcv_count;

		seq_printf(m, "%-15s %14u %9u %9u %9u %9u %8u %14u %20u "
					"%15u %15u %15u %15u %13lld %13d\n",
					entry->comm, transmit_count,
					snd_count, snd_bytes,
					rcv_count, rcv_bytes,
					activity, total_activity, total_transmit_count,
					total_snd_count, total_snd_bytes,
					total_rcv_count, total_rcv_bytes,
					ktime_to_ms(entry->last_transmit),
					entry->suspend_count);
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	return 0;
}

static int pid_stat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, pid_stat_show, NULL);
}

static const struct file_operations pid_stat_fops = {
	.open       = pid_stat_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int pid_stat_pm_suspend_prepare_cb(ktime_t ktime)
{
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int total_snd_count, total_rcv_count, transmit_count;
	unsigned int entry_cnt = 0;

	struct pid_stat *entry;

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			entry_cnt++;
			atomic_inc(&entry->total_activity);
			snd_bytes = (unsigned int) (atomic_read(&entry->current_snd) + INT_MIN);
			rcv_bytes = (unsigned int) (atomic_read(&entry->current_rcv) + INT_MIN);
			total_snd_count = (unsigned int) (atomic_read(&entry->current_snd_count) + INT_MIN);
			total_rcv_count = (unsigned int) (atomic_read(&entry->current_rcv_count) + INT_MIN);
			transmit_count = total_snd_count + total_rcv_count;

			pr_info(PID_STAT_PREFIX"%4d: %2d %-16s %6u %6u %6u %10u %6u %10u %10lld %10lld %10lld\n",
						suspend_stats.success, entry_cnt,
						entry->comm,
						atomic_read(&entry->total_activity) + INT_MIN,
						transmit_count,
						total_snd_count, snd_bytes,
						total_rcv_count, rcv_bytes,
						ktime_to_ms(ktime),
						ktime_to_ms(entry->last_transmit),
						ktime_to_ms(ktime_sub(entry->last_transmit, ktime)));
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	return 0;
}

static int pid_stat_pm_post_suspend_cb(ktime_t ktime)
{
	unsigned long flags;
	struct pid_stat *entry;

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			/* Reset counter, so we can track  traffic during next post suspend. */
			atomic_set(&entry->current_rcv, INT_MIN);
			atomic_set(&entry->current_snd, INT_MIN);
			atomic_set(&entry->current_rcv_count, INT_MIN);
			atomic_set(&entry->current_snd_count, INT_MIN);
			atomic_set(&entry->activity, INT_MIN);
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	return 0;
}

static int pid_stat_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		pid_stat_pm_suspend_prepare_cb(resume_time);
		break;
	case PM_POST_SUSPEND:
		pid_stat_pm_post_suspend_cb(resume_time);
		resume_time =  ktime_get();
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block pid_stat_notifier_block = {
	.notifier_call = pid_stat_pm_notifier,
};

#ifdef CONFIG_ENERGY_MONITOR
void get_pid_stat_monitor(struct pid_stat_monitor *pid_stat_mon, int size)
{
	struct pid_stat *entry;
	unsigned long flags;
	int i = 0, j = 0;

	for (i = 0; i < size; i++) {
		memset(pid_stat_mon[i].name, 0, TASK_COMM_LEN);
		pid_stat_mon[i].transmit = 0;
		pid_stat_mon[i].count = 0;
	}

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		if (entry->energy_mon_transmit == 0)
			continue;

		for (i = 0; i < size; i++) {
			/* Insertion in empty space */
			if (pid_stat_mon[i].transmit == 0) {
				strncpy(pid_stat_mon[i].name, entry->comm, TASK_COMM_LEN);
				pid_stat_mon[i].name[TASK_COMM_LEN - 1] = 0;
				pid_stat_mon[i].transmit = entry->energy_mon_transmit;
				pid_stat_mon[i].count = entry->energy_mon_count;
				break;
			}

			/* Insertion in order */
			if (entry->energy_mon_transmit > pid_stat_mon[i].transmit) {
				for (j = size - 1; j > i; j--) {
					if (pid_stat_mon[j - 1].transmit == 0)
						continue;
					strncpy(pid_stat_mon[j].name, pid_stat_mon[j - 1].name, TASK_COMM_LEN);
					pid_stat_mon[j].transmit = pid_stat_mon[j - 1].transmit;
					pid_stat_mon[j].count = pid_stat_mon[j - 1].count;
				}
				strncpy(pid_stat_mon[i].name, entry->comm, TASK_COMM_LEN);
				pid_stat_mon[i].name[TASK_COMM_LEN - 1] = 0;
				pid_stat_mon[i].transmit = entry->energy_mon_transmit;
				pid_stat_mon[i].count = entry->energy_mon_count;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	init_energy_mon_info();
}
#endif

#ifdef CONFIG_SLEEP_MONITOR
static int pid_stat_sleep_monitor_read_cb(void *priv,
		unsigned int *raw_val, int check_level, int caller_type)
{
	int mask = 0;
	unsigned long flags;
	unsigned int total_snd_count = 0, total_rcv_count = 0;
	unsigned int total_transmit_count = 0, total_act_cnt = 0;
	struct pid_stat *entry;
	ktime_t *resume_time = (ktime_t *)priv;

	if (caller_type != SLEEP_MONITOR_CALL_SUSPEND) {
		*raw_val = 0;
		return 0;
	}

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, list) {
		if (entry->last_transmit.tv64 > resume_time->tv64) {
			total_act_cnt += (unsigned int) (atomic_read(&entry->activity) + INT_MIN);
			total_snd_count = (unsigned int) (atomic_read(&entry->current_snd_count) + INT_MIN);
			total_rcv_count = (unsigned int) (atomic_read(&entry->current_rcv_count) + INT_MIN);
			total_transmit_count += total_snd_count + total_rcv_count;
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	mask = 0xffffffff;
	*raw_val = (total_transmit_count > mask) ? mask : total_transmit_count;

	if (total_act_cnt == 0)
		return DEVICE_POWER_OFF;
	else
		return (total_act_cnt > DEVICE_UNKNOWN) ? DEVICE_UNKNOWN : total_act_cnt;
}

static struct sleep_monitor_ops pid_stat_sleep_monitor_ops = {
	.read_cb_func = pid_stat_sleep_monitor_read_cb,
};
#endif

static int __init pid_stat_init(void)
{
	struct dentry *root;

	root = debugfs_create_dir("pid_stat", NULL);
	if (!root) {
		pr_err(PID_STAT_PREFIX"failed to create sap_pid_stat debugfs directory\n");
		return -ENOMEM;
	}

	/* Make interface to read the tcp traffic statistic */
	if (!debugfs_create_file("stat", 0660, root, NULL, &pid_stat_fops))
		goto error_debugfs;

	if (register_pm_notifier(&pid_stat_notifier_block))
		goto error_debugfs;

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(&resume_time,
			&pid_stat_sleep_monitor_ops,
			SLEEP_MONITOR_TCP);
#endif

	return 0;

error_debugfs:
    debugfs_remove_recursive(root);

	return -1;
}

late_initcall(pid_stat_init);
