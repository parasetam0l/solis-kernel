/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2016 SAMSUNG, Inc.
 * Sangin Lee <sangin78.lee@samsung.com>
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 * Hunsup Jung <hunsup.jung@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the impliesd warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/rtc.h>
#include <linux/delay.h>

#include <linux/power/sleep_monitor.h>
#include <linux/power_supply.h>

struct dentry *slp_mon_d;
EXPORT_SYMBOL(slp_mon_d);
unsigned int special_key;
static char *store_buf;
static ssize_t store_data;
static struct wake_lock slp_mon_wl;
static int sleep_monitor_enable = 1;
static unsigned short slp_mon_retry_cnt;

/* For monitor_interval func */
#define SLEEP_MONITOR_MAX_MONITOR_INTERVAL 3600 /* second */
static struct task_struct *monitor_kthread;
static unsigned int monitor_interval = 0; /* 0: Disable, 1~3600: Enable. */

/* SLEEP_MONITOR_LPM_MODE - Charging mode */
#define SLEEP_MONITOR_LPM_MODE 1
extern unsigned int lpcharge;

/* Debug Level */
static int debug_level = SLEEP_MONITOR_DEBUG_LABEL |
						 SLEEP_MONITOR_DEBUG_INFO |
						 SLEEP_MONITOR_DEBUG_ERR |
						 SLEEP_MONITOR_DEBUG_DEVICE |
						 SLEEP_MONITOR_DEBUG_INIT_TIMER;

/* Checking timing */
static char *type_text[] = {
	"suspend", "resume",
};

/* Sleep Monitor of each device */
static sleep_monitor_device slp_mon[SLEEP_MONITOR_NUM_MAX] = {
	/* CAUTION!!! Need to sync with SLEEP_MONITOR_DEVICE in sleep_monitor.h */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"BT      " }, /* SLEEP_MONITOR_BT */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WIFI    " }, /* SLEEP_MONITOR_WIFI */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WIFI1   " },/* SLEEP_MONITOR_WIFI1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"IRQ     " }, /* SLEEP_MONITOR_IRQ */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"BAT     " }, /* SLEEP_MONITOR_BAT */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"NFC     " }, /* SLEEP_MONITOR_NFC */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SENSOR  " }, /* SLEEP_MONITOR_SENSOR */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SENSOR1 " }, /* SLEEP_MONITOR_SENSOR1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"AUDIO   " }, /* SLEEP_MONITOR_AUDIO */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPA    " }, /* SLEEP_MONITOR_SAPA */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPA1   " }, /* SLEEP_MONITOR_SAPA1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPB    " }, /* SLEEP_MONITOR_SAPB */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPB1   " }, /* SLEEP_MONITOR_SAPB1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CONHR   " }, /* SLEEP_MONITOR_CONHR */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"KEY     " }, /* SLEEP_MONITOR_KEY */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV15   " }, /* SLEEP_MONITOR_DEV15 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CPU_UTIL" }, /* SLEEP_MONITOR_CPU_UTIL */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"LCD     " }, /* SLEEP_MONITOR_LCD */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TSP     " }, /* SLEEP_MONITOR_TSP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"ROTARY  " }, /* SLEEP_MONITOR_ROTARY */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"REGUL   " }, /* SLEEP_MONITOR_REGULATOR */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"REGUL1  " }, /* SLEEP_MONITOR_REGULATOR1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"PMDOMAIN" }, /* SLEEP_MONITOR_PMDOMAINS */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CP      " }, /* SLEEP_MONITOR_CP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CP1     " }, /* SLEEP_MONITOR_CP1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"MST     " }, /* SLEEP_MONITOR_MST */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CPUIDLE " }, /* SLEEP_MONITOR_CPUIDLE */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TEMP    " }, /* SLEEP_MONITOR_TEMP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TEMPMAX " }, /* SLEEP_MONITOR_TEMPMAX */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TCP     " }, /* SLEEP_MONITOR_TCP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"SYST    " }, /* SLEEP_MONITOR_SYS_TIME */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"RTCT    " }, /* SLEEP_MONITOR_RTC_TIME */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WS      " }, /* SLEEP_MONITOR_WS */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WS1     " }, /* SLEEP_MONITOR_WS1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WS2     " }, /* SLEEP_MONITOR_WS2 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WS3     " }, /* SLEEP_MONITOR_WS3 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"SLWL    " }, /* SLEEP_MONITOR_SLWL */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"SLWL1   " }, /* SLEEP_MONITOR_SLWL1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"SLWL2   " }, /* SLEEP_MONITOR_SLWL2 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"SLWL3   " }, /* SLEEP_MONITOR_SLWL3 */
};

/**
* @brief : Register sleep monitor ops by devices.
* @param-ops : Sleep_monitor_ops pointer. It need to implemetns call back function.
* @param-device_type : It's device enum. Many device separated by this. It's located in sleep_monitor.h
* @return 0 on success, otherwise a negative error value.
*/
int sleep_monitor_register_ops(void *priv, struct sleep_monitor_ops *ops, int device_type)
{
	if (device_type < 0 || device_type >= SLEEP_MONITOR_NUM_MAX) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "device type error\n");
		return -1;
	}

	if (priv)
		slp_mon[device_type].priv = priv;

	if (ops) {
		if (ops->read_cb_func || ops->read64_cb_func)
			slp_mon[device_type].sm_ops = ops;
		else {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "read cb function error\n");
			return -1;
		}
	} else {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "ops not valid\n");
		return -1;
	}
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEVICE,
		"sleep_monitor register success type:%d(%s)\n", device_type,
									slp_mon[device_type].device_name);
	return 0;
}
EXPORT_SYMBOL_GPL(sleep_monitor_register_ops);

/**
* @brief : Unregister sleep monitor ops by devices.
* @param-device_type : It's device enum. Each device separated by this. It's located in sleep_monitor.h
* @return 0 on success, otherwise a negative error value.
*/
int sleep_monitor_unregister_ops(int device_type)
{
	if (device_type < 0 || device_type >= SLEEP_MONITOR_NUM_MAX) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "device type error\n");
		return -1;
	}
	slp_mon[device_type].priv = NULL;
	slp_mon[device_type].sm_ops = NULL;
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEVICE, "sleep_monitor unregister success\n");
	return 0;

}
EXPORT_SYMBOL_GPL(sleep_monitor_unregister_ops);

char* get_type_marker(int type)
{
	switch (type) {
		case SLEEP_MONITOR_CALL_SUSPEND : return "+";
		case SLEEP_MONITOR_CALL_RESUME : return "-";
		case SLEEP_MONITOR_CALL_IRQ_LIST : return "=";
		case SLEEP_MONITOR_CALL_WS_LIST : return "^";
		case SLEEP_MONITOR_CALL_SLWL_LIST : return "%";
		case SLEEP_MONITOR_CALL_INIT : return "$";
		case SLEEP_MONITOR_CALL_POFF : return "!";
		case SLEEP_MONITOR_CALL_DUMP : return "#";
		case SLEEP_MONITOR_CALL_ETC: return "";
		default : return "\0";
	}
}
EXPORT_SYMBOL_GPL(get_type_marker);

void sleep_monitor_update_req(void)
{
	pr_info("%s call\n", __func__);
	sysfs_notify(power_kobj, NULL, "slp_mon_save_req");
}
EXPORT_SYMBOL(sleep_monitor_update_req);

void sleep_monitor_store_buf(char* buf, int ret, enum SLEEP_MONITOR_BOOLEAN is_first)
{
	/*
	 * If there is not enough store_buf space,
	 * ask TRM to get store_buf.
	 */
	if (is_first == SLEEP_MONITOR_BOOLEAN_TRUE) {
		if (store_data + (3 * SLEEP_MONITOR_SUSPEND_RESUME_PAIR_BUFF)
						> SLEEP_MONITOR_STORE_BUFF) {
			wake_lock_timeout(&slp_mon_wl, msecs_to_jiffies(5000));
			sleep_monitor_update_req();
		}
	}

	/*
	 * If there is not enough store_buf space,
	 * increase slp_mon_retry_cnt.
	 */
	if (store_data + ret > SLEEP_MONITOR_STORE_BUFF) {
		if (slp_mon_retry_cnt >= SLEEP_MONITOR_MAX_RETRY_CNT) {
			pr_info("%s - sleep monitor disable, please check TRM status\n", __func__);
			sleep_monitor_enable = 0;
			return;
		} else
			slp_mon_retry_cnt++;

		pr_info("WARN!! - %s Check whether power off charging or not : retry_cnt : %d\n",
				__func__, slp_mon_retry_cnt);
		return;
	}

	/* store buf to store_buf */
	memcpy(store_buf + store_data, buf, ret);
	store_data += ret;
}
EXPORT_SYMBOL_GPL(sleep_monitor_store_buf);

/**
* @brief : Return result as raw format from devices.
* @param-pretty_group : It's assigned value as raw format.
* @return 0 on success, otherwise a negative error value.
*/
int sleep_monitor_get_raw_value(int *raw_value)
{
	int i = 0, ret = 0;

	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if (SLEEP_MONITOR_BOOLEAN_TRUE == slp_mon[i].skip_device)
			continue;
		if (slp_mon[i].sm_ops && slp_mon[i].sm_ops->read_cb_func) {
			ret = slp_mon[i].sm_ops->read_cb_func(slp_mon[i].priv, &raw_value[i],
									slp_mon[i].check_level, SLEEP_MONITOR_CALL_ETC);
			if (ret < 0) {
				sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR,
				"%s device cb func error: %d\n", slp_mon[i].device_name, ret);
				raw_value[i] = DEVICE_ERR_1;
			}
		} else {
			raw_value[i] = DEVICE_ERR_1;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sleep_monitor_get_raw_value);

/**
* @brief : Return result as pretty format from devices.
* @param-pretty_group : It's assigned value as pretty format.
* @return 0 on success, otherwise a negative error value.
*/
int sleep_monitor_get_pretty(int *pretty_group, int type)
{
	int i = 0, temp_pretty = 0, ret = 0;
	int mask = 0, shift = 0, offset = 0;
	int raw_value[SLEEP_MONITOR_NUM_MAX] = {0,};
	char* buf = NULL;
	static int first_time = 1;
	ktime_t diff_time[2] = {ktime_set(0,0), };

	/* Allocate for temporary buffer */
	buf = kmalloc(SLEEP_MONITOR_ONE_LINE_RAW_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE);

	/* If sleep_monitor is disable */
	if (!sleep_monitor_enable) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "sleep_monitor is not enabled\n");
		kfree(buf);
		return -1;
	}

	/* Get raw value and pretty value */
	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if (slp_mon[i].skip_device)
			continue;

		if ((SLEEP_MONITOR_DEBUG_READ_SUS_RES_TIME & debug_level) &&
			((type == SLEEP_MONITOR_CALL_SUSPEND) || (type == SLEEP_MONITOR_CALL_RESUME)))
			diff_time[0] = ktime_get();

		temp_pretty = 0x0;
		if (slp_mon[i].sm_ops) {
			if (slp_mon[i].sm_ops->read_cb_func)
				temp_pretty = slp_mon[i].sm_ops->read_cb_func(slp_mon[i].priv,
										&raw_value[i], slp_mon[i].check_level, type);
			else if (slp_mon[i].sm_ops->read64_cb_func)
				temp_pretty = slp_mon[i].sm_ops->read64_cb_func(slp_mon[i].priv,
									(long long *)&raw_value[i], slp_mon[i].check_level, type);
		}

		if ((SLEEP_MONITOR_DEBUG_READ_SUS_RES_TIME & debug_level) &&
			((type == SLEEP_MONITOR_CALL_SUSPEND) || (type == SLEEP_MONITOR_CALL_RESUME))) {
			diff_time[1] = ktime_get();
			slp_mon[i].sus_res_time[type] = ktime_sub(diff_time[1], diff_time[0]);
			pr_info("[slp_mon]-%s %s - %lld(ns)\n",type_text[type], slp_mon[i].device_name, ktime_to_ns(slp_mon[i].sus_res_time[type]));
		}

		if (temp_pretty < 0) {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR,
				"%s device cb func error: %d\n", slp_mon[i].device_name, temp_pretty);
			temp_pretty = DEVICE_UNKNOWN;
		}

		mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) - 1;
		shift = offset;
		temp_pretty &= mask;
		temp_pretty <<= shift;
		pretty_group[i/(SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE)] += temp_pretty;

		if (offset == (SLEEP_MONITOR_BIT_INT_SIZE - SLEEP_MONITOR_DEVICE_BIT_WIDTH) )
			offset = 0;
		else
			offset += SLEEP_MONITOR_DEVICE_BIT_WIDTH;
	}

	/*
	 * Print device name and value at
	 * first time(init type) or debug level is enabled
	 */
	if (first_time || (debug_level & SLEEP_MONITOR_DEBUG_DEBUG)) {
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			if((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == 0)
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%s[%02d-%02d]", get_type_marker(type), i, i + 7);
			ret += snprintf(buf + ret, PAGE_SIZE-ret, "%8s/",slp_mon[i].device_name);
			if ((((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == SLEEP_MONITOR_NUM_DEVICE_RAW_VAL - 1)
				&& (i != 1)) || (i == SLEEP_MONITOR_NUM_MAX - 1)) {
				ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
				sleep_mon_dbg(SLEEP_MONITOR_DEBUG_LABEL, "%s", buf);
				memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
				ret = 0;
			}
		}
		first_time = 0;
	}

	/* Print and Store pretty data of each device */
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s[pretty@%04d]",
					get_type_marker(type), suspend_stats.success + 1);
	for (i = SLEEP_MONITOR_GROUP_SIZE - 1; i >= 0; i--) {
		ret += snprintf(buf + ret,PAGE_SIZE - ret, "%08x/",pretty_group[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "&%08x\n", special_key);
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_INFO, "%s", buf);
	if (type != SLEEP_MONITOR_CALL_ETC)
		sleep_monitor_store_buf(buf, ret, SLEEP_MONITOR_BOOLEAN_TRUE);
	memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
	ret = 0;

	/* Print and Store raw data of each device */
	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if ((i%SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == 0)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%s[%02d-%02d]", get_type_marker(type), i, i+7);
		ret += snprintf(buf + ret,PAGE_SIZE-ret, "%08x/",raw_value[i]);
		if ((((i %SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == SLEEP_MONITOR_NUM_DEVICE_RAW_VAL-1)
			&& (i != 1)) || (i == SLEEP_MONITOR_NUM_MAX - 1)) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "%s", buf);
			if (type != SLEEP_MONITOR_CALL_ETC)
				sleep_monitor_store_buf(buf, ret, SLEEP_MONITOR_BOOLEAN_FALSE);
			memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
			ret = 0;
		}
	}

	kfree(buf);
	return 0;
}
EXPORT_SYMBOL_GPL(sleep_monitor_get_pretty);

static ssize_t read_dev_name(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	int i = 0;
	char *buf = NULL;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(SLEEP_MONITOR_ONE_LINE_RAW_SIZE * 4, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * 4);

	if (*ppos == 0) {
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			if((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == 0)
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%s[%02d-%02d]", get_type_marker(SLEEP_MONITOR_CALL_ETC), i, i + 7);

			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%8s/", slp_mon[i].device_name);
			if ((((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == SLEEP_MONITOR_NUM_DEVICE_RAW_VAL - 1)
				&& (i != 1)) || (i == SLEEP_MONITOR_NUM_MAX - 1))
				ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}
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

static ssize_t read_skip_device(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	int i = 0;
	char *buf;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, count);

	if (*ppos == 0) {
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			ret += snprintf(buf + ret, PAGE_SIZE-ret, "%2d %s %d\n", i,
								slp_mon[i].device_name, slp_mon[i].skip_device);
		}
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

static ssize_t write_skip_device(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int device = 0;
	int is_skip = 0;

	sscanf(user_buf,"%d--%d",&device, &is_skip);
	if (device >= 0 && device < SLEEP_MONITOR_NUM_MAX) {
		if (is_skip >= 0 && is_skip <= 1) {
			slp_mon[device].skip_device = (is_skip == 0 ? SLEEP_MONITOR_BOOLEAN_FALSE :
								 SLEEP_MONITOR_BOOLEAN_TRUE);
		}
	}
	return count;
}

static ssize_t read_check_device_level(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	int i = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, count);

	if (*ppos == 0){
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			if (slp_mon[i].skip_device == SLEEP_MONITOR_BOOLEAN_TRUE)
				continue;
			ret += snprintf(buf + ret,PAGE_SIZE-ret, "%2d %s %d\n", i,
								slp_mon[i].device_name, slp_mon[i].check_level);
		}
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

static ssize_t write_check_device_level(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int device = 0, depth = 0;

	sscanf(user_buf,"%d--%d",&device, &depth);
	if (device >= 0 && device < SLEEP_MONITOR_NUM_MAX) {
		slp_mon[device].check_level = depth;
	}
	return count;
}

static int monitor_thread(void *data)
{
	int pretty_group_buf[SLEEP_MONITOR_GROUP_SIZE] = {0,};
	int thread_cnt = 0;
	long timeout;

	while(1) {
		thread_cnt++;
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG,
			"monitor_thread: (count=%d)\n", thread_cnt);

		if (monitor_interval) {
			/* All check routine will be excuted
				when monitor is enabled. (monitor_interval > 0) */
			sleep_monitor_get_pretty(pretty_group_buf, SLEEP_MONITOR_CALL_ETC);
		}
		msleep(100);
		set_current_state(TASK_INTERRUPTIBLE);

		if (monitor_interval) {
			timeout = schedule_timeout(monitor_interval * HZ);
			if (!timeout)
				return -ETIMEDOUT;
		}
		else {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_WORK,
				"monior thread is destroyed (pid=%d)\n", monitor_kthread->pid);
			monitor_kthread = NULL;
			break;
		}
	}

	return 0;
}

static int create_monitor_thread(void *data)
{
	if (monitor_kthread)
		return 0;

	monitor_kthread = kthread_run(monitor_thread, NULL, "slp_mon");
	if (IS_ERR(monitor_kthread)) {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "Fail to create kthread\n");
			return -1;
	}

	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_WORK,
		"monior thread is created (pid=%d)\n", monitor_kthread->pid);

	return 0;
}

static ssize_t read_monitor_interval(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[10];

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0)
		ret += snprintf(buf,sizeof(buf), "%u\n", monitor_interval);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			return -EFAULT;
		}
		*ppos += ret;
	}

	return ret;
}

static ssize_t write_monitor_interval(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	unsigned int new_interval = 0;

	sscanf(user_buf,"%u",&new_interval);

	/* Maximum interval is 1 hour */
	if (new_interval > SLEEP_MONITOR_MAX_MONITOR_INTERVAL)
		new_interval = SLEEP_MONITOR_MAX_MONITOR_INTERVAL;

	monitor_interval = new_interval;

	if (monitor_interval > 0 && !monitor_kthread) {
		create_monitor_thread(NULL);
	} else if ( monitor_interval == 0 && monitor_kthread) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_WORK,
			"monior thread is stopped (pid=%d)\n", monitor_kthread->pid);
		kthread_stop(monitor_kthread);
		monitor_kthread = NULL;
	}

	return count;
}

static int show_device_raw_status(struct seq_file *s, void *data)
{
	int raw_value[SLEEP_MONITOR_NUM_MAX] = {0,};
	int pretty_value[SLEEP_MONITOR_NUM_MAX] = {0,};
	int oneline_pretty_value[SLEEP_MONITOR_GROUP_SIZE] = {0,};
	int i = 0, mask = 0, shift = 0, offset = 0, temp_pretty = 0;
	bool is_read64 = SLEEP_MONITOR_BOOLEAN_FALSE;

	if (!sleep_monitor_enable) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "sleep_monitor disable\n");
		return -1;
	}

	seq_printf(s, " idx dev     pretty    raw  read suspend time   read resumetime             cb\n");

	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if (SLEEP_MONITOR_BOOLEAN_TRUE == slp_mon[i].skip_device)
			continue;

		if (slp_mon[i].sm_ops) {
			if (slp_mon[i].sm_ops->read_cb_func) {
				pretty_value[i] = slp_mon[i].sm_ops->read_cb_func(slp_mon[i].priv,
										&raw_value[i],slp_mon[i].check_level, SLEEP_MONITOR_CALL_ETC);
				is_read64 = SLEEP_MONITOR_BOOLEAN_FALSE;
			} else if(slp_mon[i].sm_ops->read64_cb_func) {
				pretty_value[i] = slp_mon[i].sm_ops->read64_cb_func(slp_mon[i].priv,
									(long long*)&raw_value[i],slp_mon[i].check_level, SLEEP_MONITOR_CALL_ETC);
				is_read64 = SLEEP_MONITOR_BOOLEAN_TRUE;
			} else
				pretty_value[i] = 0x0;
		} else {
			pretty_value[i] = 0x0;
		}

		temp_pretty = pretty_value[i];
		mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) -1;
		shift = offset;
		temp_pretty &= mask;
		temp_pretty <<= shift;
		oneline_pretty_value[i/(SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE)] += temp_pretty;

		if (offset == (SLEEP_MONITOR_BIT_INT_SIZE - SLEEP_MONITOR_DEVICE_BIT_WIDTH) )
			offset = 0;
		else
			offset += SLEEP_MONITOR_DEVICE_BIT_WIDTH;

		if (is_read64 == SLEEP_MONITOR_BOOLEAN_FALSE)
			seq_printf(s, "%2d %s (0x%x)  0x%08x %8lld(ns)  %8lld(ns)         %pf\n", i, slp_mon[i].device_name,
					pretty_value[i],raw_value[i], ktime_to_ns(slp_mon[i].sus_res_time[0]),
					ktime_to_ns(slp_mon[i].sus_res_time[1]), (slp_mon[i].sm_ops != NULL) ?
					slp_mon[i].sm_ops->read_cb_func : NULL );
		else
			seq_printf(s, "%2d %s (0x%x)  0x%08x %8lld(ns)  %8lld(ns)         %pf\n", i, slp_mon[i].device_name,
					pretty_value[i],raw_value[i], ktime_to_ns(slp_mon[i].sus_res_time[0]),
					ktime_to_ns(slp_mon[i].sus_res_time[1]), slp_mon[i].sm_ops->read64_cb_func);

		is_read64 = SLEEP_MONITOR_BOOLEAN_FALSE;
	}

	for (i = SLEEP_MONITOR_GROUP_SIZE-1; i >= 0 ; i--) {
		seq_printf(s, "%08x/", oneline_pretty_value[i]);
	}
	seq_printf(s, "\n");

	return 0;
}

static int sleep_monitor_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_device_raw_status, NULL);
}

static ssize_t read_device_pretty_value(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	int i = 0;
	int pretty_group[SLEEP_MONITOR_GROUP_SIZE] = {0,};

	if (!sleep_monitor_enable) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "sleep_monitor disable\n");
		return -1;
	}

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, count);

	if (*ppos == 0) {
		sleep_monitor_get_pretty(pretty_group, SLEEP_MONITOR_CALL_ETC);
		for (i = SLEEP_MONITOR_GROUP_SIZE-1; i >= 0 ; i--) {
			ret += snprintf(buf + ret,PAGE_SIZE-ret, "%08x/",pretty_group[i]);
		}
		ret += snprintf(buf + ret,PAGE_SIZE-ret, "\n");
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

static ssize_t read_store_buf(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t temp = 0;

	if (*ppos < 0 || !count) {
		wake_unlock(&slp_mon_wl);
		return -EINVAL;
	}

	if (*ppos == 0) {
		if (store_data >= 0) {
			temp = store_data;
			if (copy_to_user(buffer, store_buf, temp)) {
				wake_unlock(&slp_mon_wl);
				return -EFAULT;
			}
			*ppos += temp;
		}
	}
	memset(store_buf, 0, SLEEP_MONITOR_STORE_BUFF);
	store_data = 0;

	pr_info("%s complete\n", __func__);
	wake_unlock(&slp_mon_wl);
	wake_lock_timeout(&slp_mon_wl, msecs_to_jiffies(400));

	return temp;
}

static ssize_t write_last_marker(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	unsigned int type = 0;
	int pretty_group[SLEEP_MONITOR_GROUP_SIZE];

	sscanf(user_buf,"%u",&type);
	memset(pretty_group, 0, sizeof(int) * SLEEP_MONITOR_GROUP_SIZE);
	sleep_monitor_get_pretty(pretty_group, type);
	sleep_monitor_update_req();

	return count;
}

static const struct file_operations dev_name_fops = {
	.read		= read_dev_name,
};

static const struct file_operations skip_device_fops = {
	.write		= write_skip_device,
	.read		= read_skip_device,
};

static const struct file_operations device_level_fops = {
	.write		= write_check_device_level,
	.read		= read_check_device_level,
};

static const struct file_operations monitor_interval_fops = {
	.write		= write_monitor_interval,
	.read		= read_monitor_interval,
};

static const struct file_operations sleep_monitor_debug_fops = {
	.open		= sleep_monitor_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations device_pretty_fops = {
	.read		= read_device_pretty_value,
};

static const struct file_operations store_buf_fops = {
	.read		= read_store_buf,
};

static const struct file_operations last_marker_fops = {
	.write		= write_last_marker,
};

int __init sleep_monitor_debug_init(void)
{
	/* Allocate memory for store_buf */
	store_buf = kmalloc(SLEEP_MONITOR_STORE_BUFF, GFP_KERNEL);
	if (!store_buf) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "slp_mon store buffer not alloc\n");
		return -ENOMEM;
	}
	memset(store_buf, 0, SLEEP_MONITOR_STORE_BUFF);

	/* Initialize sleep monitor wake lock */
	wake_lock_init(&slp_mon_wl, WAKE_LOCK_SUSPEND, "sleep_monitor");

	/* Set special_key */
	special_key = get_random_int();

	/* If LPM MODE was enabled, disable sleep monitor */
	if (lpcharge == SLEEP_MONITOR_LPM_MODE)
		sleep_monitor_enable = 0;

	/* Create debugfs */
	slp_mon_d = debugfs_create_dir("sleep_monitor", NULL);
	if (slp_mon_d) {
		/* Enable node */
		debugfs_create_u32("enable", S_IRUSR | S_IWUSR, slp_mon_d, &sleep_monitor_enable);

		/* Debug level */
		debugfs_create_u32("debug_level", S_IRUSR | S_IWUSR, slp_mon_d, &debug_level);

		/*
		 * Device configuration
		 * dev_name: device name in order
		 * skip_device: skip device
		 * check_device_level: checking level of each device
		 * monitor_interval: check each device info by periods
		 */
		if (!debugfs_create_file("dev_name", S_IRUGO, slp_mon_d, NULL, &dev_name_fops))
			pr_err("%s : debugfs_create_file, error\n", "dev_name");

		if (!debugfs_create_file("skip_device", S_IRUSR | S_IWUSR, slp_mon_d, NULL, &skip_device_fops))
			pr_err("%s : debugfs_create_file, error\n", "skip_device");

		if (!debugfs_create_file("check_device_level", S_IRUSR | S_IWUSR, slp_mon_d, NULL, &device_level_fops))
			pr_err("%s : debugfs_create_file, error\n", "check_device_level");

		if (!debugfs_create_file("monitor_interval", S_IRUSR | S_IWUSR, slp_mon_d, NULL, &monitor_interval_fops))
			pr_err("%s : debugfs_create_file, error\n", "monitor_interval");

		/*
		 * Stored information
		 * status: whole information
		 * pretty_status: pretty information
		 * store_buf: current store buf
		 */
		if (!debugfs_create_file("status", S_IRUSR, slp_mon_d, NULL, &sleep_monitor_debug_fops))
			pr_err("%s : debugfs_create_file, error\n", "status");

		if (!debugfs_create_file("pretty_status", S_IRUSR, slp_mon_d, NULL, &device_pretty_fops))
			pr_err("%s : debugfs_create_file, error\n", "pretty_status");

		if (!debugfs_create_file("store_buf", S_IRUSR, slp_mon_d, NULL, &store_buf_fops))
			pr_err("%s : debugfs_create_file, error\n", "store_buf");

		/* For dump or power off */
		if (!debugfs_create_file("last_marker", S_IWUSR, slp_mon_d, NULL, &last_marker_fops))
			pr_err("%s : debugfs_create_file, error\n", "last_marker");

	}

	/* Create monitor thread */
	if (monitor_interval > 0 && !monitor_kthread) {
		create_monitor_thread(NULL);
	}

	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEVICE, "sleep_monitor initialized\n");
	return 0;
}

postcore_initcall(sleep_monitor_debug_init);
