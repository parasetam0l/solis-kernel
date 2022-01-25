/* drivers/misc/sap_pid_stat.c
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
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.08.01>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/suspend.h>
#include <asm/uaccess.h>

#include <linux/sap_pid_stat.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#define SAP_STAT_SLEEP_MON_RAW_LENGTH			0xf
#define SAP_STAT_SLEEP_MON_RAW_LENGTH_SHIFT		4
#endif

#define SAP_STAT_PREFIX	"sap_stat:  "
#define ASP_ID_LEN 15

typedef struct {
	char id[ASP_ID_LEN + 1];
} aspid_t;

typedef enum {
	SAPID_ALRAM = 0,
	SAPID_CALL,
	SAPID_SETTING,
	SAPID_MESSAGES,
	SAPID_SHEALTH,
	SAPID_TEXTTEMPLATE,
	SAPID_CALENDAR,
	SAPID_CONTACTS,
	SAPID_GALLERYTRANSFER,
	SAPID_GALLERYRECEIVER,
	SAPID_MUSICCONTROLLER,
	SAPID_MUSICTRANSFER,
	SAPID_VOICEMEMO,
	SAPID_CONTEXT,
	SAPID_WEATHER,
	SAPID_EMAILNOTI,
	SAPID_FMP,
	SAPID_WMS,
	SAPID_LBS,
	SAPID_BCMSERVICE,
	SAPID_WEBPROXY,
	SAPID_NOTIFICATION,
	SAPID_FILETRANSFER,
	SAPID_SERVICECAPABILITY,
	SAPID_NONE = -1
} sapid_t;

struct sap_pid_stat {
	struct list_head list;
	sapid_t sapid;
	aspid_t aspid;
	atomic_t rcv;
	atomic_t snd;
	atomic_t rcv_count;
	atomic_t snd_count;
	atomic_t wakeup_count;
	atomic_t activity;
	atomic_t total_activity;
	atomic_t rcv_post_suspend;
	atomic_t snd_post_suspend;
	atomic_t rcv_count_post_suspend;
	atomic_t snd_count_post_suspend;
	ktime_t first_transmit;
	ktime_t last_transmit;
	int suspend_count;
};

/* Maps sapid type to SAP aspid */
static aspid_t sapid_to_aspid[] = {
	[SAPID_ALRAM] = {"alarm"},
	[SAPID_CALL] = {"callhandler"},
	[SAPID_SETTING] = {"setting"},
	[SAPID_MESSAGES] = {"minimessage"},
	[SAPID_SHEALTH] = {"watch_pedometer"},
	[SAPID_TEXTTEMPLATE] = {"texttemplate"},
	[SAPID_CALENDAR] = {"calendar"},
	[SAPID_CONTACTS] = {"b2contact"},
	[SAPID_GALLERYTRANSFER] = {"transfer"},
	[SAPID_GALLERYRECEIVER] = {"receiver"},
	[SAPID_MUSICCONTROLLER] = {"music"},
	[SAPID_MUSICTRANSFER] = {"musictransfer"},
	[SAPID_VOICEMEMO] = {"voicememo"},
	[SAPID_CONTEXT] = {"context"},
	[SAPID_WEATHER] = {"weather"},
	[SAPID_EMAILNOTI] = {"emailnotificat"},
	[SAPID_FMP] = {"fmp"},
	[SAPID_WMS] = {"hostmanager"},
	[SAPID_LBS] = {"lbs-server"},
	[SAPID_BCMSERVICE] = {"bcmservice"},
	[SAPID_WEBPROXY] = {"webproxy"},
	[SAPID_NOTIFICATION] = {"NotificationSe"},
	[SAPID_FILETRANSFER] = {"filetransfer"},
	[SAPID_SERVICECAPABILITY] = {"ServiceCapabil"},
};

static DEFINE_SPINLOCK(sap_pid_lock);
static LIST_HEAD(sap_pid_list);
static ktime_t resume_time;

static sapid_t sap_stat_aspid_to_sapid(aspid_t *asp)
{
	int i;
	sapid_t sapid = SAPID_NONE;

	for (i = 0; i < ARRAY_SIZE(sapid_to_aspid); i++)
		if (!strncmp(asp->id, sapid_to_aspid[i].id, ASP_ID_LEN - 1))
			return (sapid_t)i;

	return sapid;
}

int sap_stat_get_wakeup(struct sap_pid_wakeup_stat *sap_wakeup)
{
	unsigned long flags;
	unsigned int wakeup_cnt, activity_cnt;
	struct sap_pid_stat *entry;

	if (!sap_wakeup)
		return -EINVAL;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		if (entry->sapid < SAP_STAT_SAPID_MAX) {
			wakeup_cnt = (unsigned int) (atomic_read(&entry->wakeup_count) + INT_MIN);
			activity_cnt = (unsigned int) (atomic_read(&entry->total_activity) + INT_MIN);

			sap_wakeup->wakeup_cnt[entry->sapid] = wakeup_cnt;
			sap_wakeup->activity_cnt[entry->sapid] = activity_cnt;

			pr_debug(SAP_STAT_PREFIX"%s %d %d(%d)\n",
					__func__, entry->sapid, activity_cnt, wakeup_cnt);
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return 0;
}

static int sap_stat_pm_suspend_prepare_cb(ktime_t ktime)
{
	struct sap_pid_stat *wakeup_entry = NULL;
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count, transmit_count;
	unsigned int entry_cnt = 0;

	struct sap_pid_stat *entry;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			if (!wakeup_entry)
				wakeup_entry = entry;
			else if (wakeup_entry->first_transmit.tv64 > entry->first_transmit.tv64)
				wakeup_entry = entry;

			entry_cnt++;
			atomic_inc(&entry->total_activity);
			snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
			rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
			transmit_count = snd_count + rcv_count;

			pr_info(SAP_STAT_PREFIX"%4d: %2d %-16s %6u %6u %6u %10u %6u %10u %10lld %10lld %10lld\n",
					suspend_stats.success, entry_cnt,
					entry->aspid.id,
					atomic_read(&entry->total_activity) + INT_MIN,
					transmit_count,
					snd_count, snd_bytes,
					rcv_count, rcv_bytes,
					ktime_to_ms(ktime),
					ktime_to_ms(entry->last_transmit),
					ktime_to_ms(ktime_sub(entry->last_transmit, ktime)));
		}
	}
	if (wakeup_entry)
		atomic_inc(&wakeup_entry->wakeup_count);
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return 0;
}

static int sap_stat_pm_post_suspend_cb(ktime_t ktime)
{
	unsigned long flags;
	struct sap_pid_stat *entry;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			/* Reset counter, so we can track SAP traffic during next post suspend */
			atomic_set(&entry->rcv_post_suspend, INT_MIN);
			atomic_set(&entry->snd_post_suspend, INT_MIN);
			atomic_set(&entry->rcv_count_post_suspend, INT_MIN);
			atomic_set(&entry->snd_count_post_suspend, INT_MIN);
			atomic_set(&entry->activity, INT_MIN);
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return 0;
}


#ifdef CONFIG_SLEEP_MONITOR
static int sap_stat_sleep_monitor_a_read64_cb(void *priv,
		long long *raw_val, int check_level, int caller_type)
{
	unsigned long flags = 0;
	unsigned int act_cnt  = 0, total_act_cnt = 0;
	struct sap_pid_stat *entry;
	ktime_t *resume_time = (ktime_t *)priv;

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		*raw_val = 0;
		return 0;
	}

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		if (entry->last_transmit.tv64 > resume_time->tv64) {
			if (entry->sapid >= 0 && entry->sapid < 16) {
				act_cnt = (unsigned int) (atomic_read(&entry->activity) + INT_MIN);
				if (act_cnt > SAP_STAT_SLEEP_MON_RAW_LENGTH)
					act_cnt = SAP_STAT_SLEEP_MON_RAW_LENGTH;
				*raw_val = *raw_val |
					((long long) (act_cnt & SAP_STAT_SLEEP_MON_RAW_LENGTH) <<
					(entry->sapid * SAP_STAT_SLEEP_MON_RAW_LENGTH_SHIFT));
				if (act_cnt > 0)
					total_act_cnt++;
			}
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	pr_debug(SAP_STAT_PREFIX"0x%llx\n", *raw_val);

	return (total_act_cnt > DEVICE_UNKNOWN) ? DEVICE_UNKNOWN : total_act_cnt;
}

static int sap_stat_sleep_monitor_b_read64_cb(void *priv,
		long long *raw_val, int check_level, int caller_type)
{
	unsigned long flags = 0;
	unsigned int act_cnt = 0, total_act_cnt = 0;
	struct sap_pid_stat *entry;
	ktime_t *resume_time = (ktime_t *)priv;

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		*raw_val = 0;
		return 0;
	}

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		if (entry->last_transmit.tv64 > resume_time->tv64) {
			if (entry->sapid >= 16) {
				act_cnt = (unsigned int) (atomic_read(&entry->activity) + INT_MIN);
				if (act_cnt > SAP_STAT_SLEEP_MON_RAW_LENGTH)
					act_cnt = SAP_STAT_SLEEP_MON_RAW_LENGTH;
				*raw_val = *raw_val |
					((long long) (act_cnt & SAP_STAT_SLEEP_MON_RAW_LENGTH) <<
					((entry->sapid - 16) * SAP_STAT_SLEEP_MON_RAW_LENGTH_SHIFT));
				if (act_cnt > 0)
					total_act_cnt++;
			}
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	pr_debug(SAP_STAT_PREFIX"0x%llx\n", *raw_val);

	return (total_act_cnt > DEVICE_UNKNOWN) ? DEVICE_UNKNOWN : total_act_cnt;
}

static struct sleep_monitor_ops sap_stat_sleep_monitor_a_ops = {
	.read64_cb_func = sap_stat_sleep_monitor_a_read64_cb,
};

static struct sleep_monitor_ops sap_stat_sleep_monitor_b_ops = {
	.read64_cb_func = sap_stat_sleep_monitor_b_read64_cb,
};
#endif /* CONFIG_SLEEP_MONITOR */

static struct sap_pid_stat *find_sap_stat(aspid_t *asp)
{
	unsigned long flags;
	struct sap_pid_stat *entry;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		if (!strcmp(asp->id, entry->aspid.id)) {
			spin_unlock_irqrestore(&sap_pid_lock, flags);
			return entry;
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return NULL;
}

/* Create a new entry for tracking the specified aspid. */
static struct sap_pid_stat *create_stat(aspid_t *asp)
{
	struct sap_pid_stat *new_sap_pid;
	unsigned long flags;

	/* Create the pid stat struct and append it to the list. */
	if ((new_sap_pid = kzalloc(sizeof(struct sap_pid_stat), GFP_KERNEL)) == NULL)
		return NULL;

	memcpy(new_sap_pid->aspid.id, asp->id, sizeof(aspid_t));
	new_sap_pid->sapid = sap_stat_aspid_to_sapid(asp);

	/* Counters start at INT_MIN, so we can track 4GB of SAP traffic. */
	atomic_set(&new_sap_pid->rcv, INT_MIN);
	atomic_set(&new_sap_pid->snd, INT_MIN);
	atomic_set(&new_sap_pid->rcv_count, INT_MIN);
	atomic_set(&new_sap_pid->snd_count, INT_MIN);
	atomic_set(&new_sap_pid->wakeup_count, INT_MIN);
	atomic_set(&new_sap_pid->activity, INT_MIN);
	atomic_set(&new_sap_pid->total_activity, INT_MIN);
	atomic_set(&new_sap_pid->rcv_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->snd_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->rcv_count_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->snd_count_post_suspend, INT_MIN);

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_add_tail(&new_sap_pid->list, &sap_pid_list);
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return new_sap_pid;
}

static int sap_stat_snd(aspid_t *asp, int size)
{
	struct sap_pid_stat *entry;
	ktime_t now;

	entry = find_sap_stat(asp);

	if (entry == NULL) {
		entry = create_stat(asp);
		if (entry == NULL)
			return -1;
	}

	now =  ktime_get();
	entry->last_transmit = now;
	if (entry->suspend_count < suspend_stats.success)
		entry->first_transmit= now;

	atomic_add(size, &entry->snd);
	atomic_inc(&entry->snd_count);
	atomic_add(size, &entry->snd_post_suspend);
	atomic_inc(&entry->snd_count_post_suspend);

	atomic_set(&entry->activity, INT_MIN + 1);
	entry->suspend_count = suspend_stats.success;

	return 0;
}

static int sap_stat_rcv(aspid_t *asp, int size)
{
	struct sap_pid_stat *entry;
	ktime_t now;

	entry = find_sap_stat(asp);

	if (entry == NULL) {
		entry = create_stat(asp);
		if (entry == NULL)
			return -1;
	}

	now =  ktime_get();
	entry->last_transmit = now;
	if (entry->suspend_count < suspend_stats.success)
		entry->first_transmit= now;

	atomic_add(size, &entry->rcv);
	atomic_inc(&entry->rcv_count);
	atomic_add(size, &entry->rcv_post_suspend);
	atomic_inc(&entry->rcv_count_post_suspend);

	atomic_set(&entry->activity, INT_MIN + 1);
	entry->suspend_count = suspend_stats.success;

	return 0;
}

static ssize_t sap_stat_snd_write_proc(struct file *file, const char __user *buf,
		size_t count, loff_t *offs)
{

	const char *str;
	char pid_buf[128] = {0};
	size_t buf_size = min(count, sizeof(pid_buf) - 1);
	aspid_t aspid;
	int bytes = 0;
	size_t len;
	int ret = 0;

	if (count > sizeof(pid_buf))
		return -EINVAL;

	if (copy_from_user(pid_buf, buf, buf_size))
		return -EFAULT;
	str = pid_buf;

	pr_debug(SAP_STAT_PREFIX"%s: user buf=%s: count=%d\n",
			__func__, buf, (int)count);

	while (*str && !isspace(*str))
		str++;

	len = str - pid_buf;
	if (!len || len > ASP_ID_LEN)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a byte string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &bytes);
		if (ret)
			return -EINVAL;
	}

	memset(&aspid, 0, sizeof(aspid_t));
	memcpy(&aspid, pid_buf, len);

	pr_debug(SAP_STAT_PREFIX"%s: aspid=%s: byte=%d: len=%d\n",
			__func__, aspid.id, bytes, (int)len);

	if (bytes > 0)
		sap_stat_snd(&aspid, bytes);

	return count;
}

static const struct file_operations sap_stat_snd_fops = {
	.open = nonseekable_open,
	.write = sap_stat_snd_write_proc,
	.llseek = no_llseek,
};

static ssize_t sap_stat_rcv_write_proc(struct file *file, const char __user *buf,
		size_t count, loff_t *offs)
{

	const char *str;
	char pid_buf[128] = {0};
	size_t buf_size = min(count, sizeof(pid_buf) - 1);
	aspid_t aspid;
	int bytes = 0;
	size_t len;
	int ret = 0;

	if (count > sizeof(pid_buf))
		return -EINVAL;

	if (copy_from_user(pid_buf, buf, buf_size))
		return -EFAULT;
	str = pid_buf;

	pr_debug(SAP_STAT_PREFIX"%s: user buf=%s: count=%d\n",
			__func__, buf, (int)count);

	while (*str && !isspace(*str))
		str++;

	len = str - pid_buf;
	if (!len || len > ASP_ID_LEN)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a byte string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &bytes);
		if (ret)
			return -EINVAL;
	}

	memset(&aspid, 0, sizeof(aspid_t));
	memcpy(&aspid, pid_buf, len);

	pr_debug(SAP_STAT_PREFIX"%s: aspid=%s: byte=%d: len=%d\n",
			__func__, aspid.id, bytes, (int)len);

	if (bytes > 0)
		sap_stat_rcv(&aspid, bytes);

	return count;
}


static const struct file_operations sap_stat_rcv_fops = {
	.open = nonseekable_open,
	.write = sap_stat_rcv_write_proc,
	.llseek = no_llseek,
};

static int sap_stat_stat_show(struct seq_file *m, void *v)
{

	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count;
	unsigned int total_snd_bytes, total_rcv_bytes;
	unsigned int total_snd_count, total_rcv_count;
	unsigned int transmit_count, total_transmit_count;
	unsigned int wakeup_count, activity, total_activity;
	struct sap_pid_stat *entry;

	seq_printf(m, "name            transmit_count "
				"snd_count snd_bytes "
				"rcv_count rcv_bytes "
				"wakeup_count activity "
				"total_activity total_transmit_count "
				"total_snd_count total_snd_bytes "
				"total_rcv_count total_rcv_bytes "
				"last_transmit suspend_count\n");

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, list) {
		snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
		rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
		snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
		rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
		transmit_count = snd_count + rcv_count;

		wakeup_count = (unsigned int) (atomic_read(&entry->wakeup_count) + INT_MIN);
		activity =  (unsigned int) (atomic_read(&entry->activity) + INT_MIN);

		total_activity = (unsigned int) (atomic_read(&entry->total_activity) + INT_MIN);
		total_snd_bytes = (unsigned int) (atomic_read(&entry->snd) + INT_MIN);
		total_rcv_bytes = (unsigned int) (atomic_read(&entry->rcv) + INT_MIN);
		total_snd_count = (unsigned int) (atomic_read(&entry->snd_count) + INT_MIN);
		total_rcv_count = (unsigned int) (atomic_read(&entry->rcv_count) + INT_MIN);
		total_transmit_count = total_snd_count + total_rcv_count;

		seq_printf(m, "%-15s %14u %9u %9u %9u %9u %12u %8u "
					"%14u %20u %15u %15u %15u %15u %13lld %13d\n",
					entry->aspid.id, transmit_count,
					snd_count, snd_bytes,
					rcv_count, rcv_bytes,
					wakeup_count, activity,
					total_activity, total_transmit_count,
					total_snd_count, total_snd_bytes,
					total_rcv_count, total_rcv_bytes,
					ktime_to_ms(entry->last_transmit),
					entry->suspend_count);
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return 0;
}

static int sap_stat_stat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sap_stat_stat_show, NULL);
}

static const struct file_operations sap_stat_stat_fops = {
	.open		= sap_stat_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sap_stat_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		sap_stat_pm_suspend_prepare_cb(resume_time);
		break;
	case PM_POST_SUSPEND:
		sap_stat_pm_post_suspend_cb(resume_time);
		resume_time =  ktime_get();
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block sap_stat_notifier_block = {
	.notifier_call = sap_stat_pm_notifier,
};

static int __init sap_stat_init(void)
{
	struct dentry *root;

	root = debugfs_create_dir("sap_pid_stat", NULL);
	if (!root) {
		pr_err("failed to create sap_pid_stat debugfs directory");
		return -ENOMEM;
	}

	if (!debugfs_create_file("snd", 0660, root, NULL, &sap_stat_snd_fops))
		goto error_debugfs;
	if (!debugfs_create_file("rcv", 0660, root, NULL, &sap_stat_rcv_fops))
		goto error_debugfs;
	if (!debugfs_create_file("stat", 0660, root, NULL, &sap_stat_stat_fops))
		goto error_debugfs;

	if (register_pm_notifier(&sap_stat_notifier_block))
		goto error_debugfs;

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(&resume_time,
			&sap_stat_sleep_monitor_a_ops,
			SLEEP_MONITOR_SAPA);
	sleep_monitor_register_ops(&resume_time,
			&sap_stat_sleep_monitor_b_ops,
			SLEEP_MONITOR_SAPB);
#endif

	return 0;

error_debugfs:
	debugfs_remove_recursive(root);

	return -1;
}

late_initcall(sap_stat_init);
