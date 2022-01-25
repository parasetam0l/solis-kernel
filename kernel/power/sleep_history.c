/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2016 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 * Sangin Lee <sangin78.lee@samsung.com>
 * Hnusup Jung <hunsup.jung@samsung.com>
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

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/circ_buf.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/power_supply.h>
#include <linux/suspend.h>
#include <linux/power/sleep_history.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define WS_ARRAY_MAX 10
#define SLEEP_HISTORY_RINGBUFFER_SIZE 2048

struct slp_irq {
	unsigned int irq;
	char name[16];
};

struct slp_ws {
	char name[16];
	ktime_t prevent_time;
};

union wakeup_history {
	struct slp_irq slp_irq;
	struct slp_ws ws;
};

struct battery_history {
	int status;
	int capacity;
};

struct sleep_history {
	char type;
	char failed_step;
	int suspend_count;
	struct timespec ts;
	struct battery_history battery;
	union wakeup_history ws;
#ifdef CONFIG_SLEEP_MONITOR
	int pretty_group[SLEEP_MONITOR_GROUP_SIZE];
#endif
};

struct sleep_history_data {
	struct circ_buf sleep_history;
};

static struct sleep_history_data sleep_history_data;
static struct suspend_stats suspend_stats_bkup;

#ifdef CONFIG_DEBUG_FS
static struct sleep_history_data copy_history_data;
static int sleep_history_index = 0xFFFFFFFF;
static char *type_text[] = {
		"none",
		"autosleep", "autosleep",
		"suspend", "suspend",
		"irq"
};

static char *suspend_text[] = {
		"failed", "freeze", "prepare", "suspend", "suspend_late",
		"suspend_noirq", "resume_noirq", "resume_early",  "resume"
};

static struct sleep_history *get_cur_input_buf(struct sleep_history_data *input_data)
{
	struct sleep_history *buf_ptr;
	int head;

	buf_ptr = (struct sleep_history *)input_data->sleep_history.buf;
	head = input_data->sleep_history.head;

	return buf_ptr + head;
}

static struct sleep_history *get_cur_output_buf(struct sleep_history_data *output_data)
{
	struct sleep_history *buf_ptr;
	int tail;

	buf_ptr = (struct sleep_history *)output_data->sleep_history.buf;
	tail = output_data->sleep_history.tail;

	return buf_ptr + tail;
}

static void move_to_next_input_buf(struct sleep_history_data *output_data)
{
	int *head_ptr = &output_data->sleep_history.head;
	int *tail_ptr = &output_data->sleep_history.tail;

	if (CIRC_SPACE(*head_ptr, *tail_ptr, SLEEP_HISTORY_RINGBUFFER_SIZE) == 0)
		*tail_ptr = (*tail_ptr + 1) & (SLEEP_HISTORY_RINGBUFFER_SIZE - 1);

	*head_ptr = (*head_ptr + 1) & (SLEEP_HISTORY_RINGBUFFER_SIZE - 1);
}

static void set_buf_ts(struct sleep_history *input_buf, struct timespec *ts)
{
	struct timespec *ts_ptr = &(input_buf->ts);

	if (ts)
		memcpy(ts_ptr, ts, sizeof(struct timespec));
}

static bool is_valid_buf(struct sleep_history_data *data)
{
	struct sleep_history *buf_ptr;
	int head;
	int tail;

	buf_ptr = (struct sleep_history *)data->sleep_history.buf;
	head = data->sleep_history.head;
	tail = data->sleep_history.tail;

	/* have no buf data or have only one buf data  */
	if ((head == 0 && tail == 0) || (head  == 1 && tail  == 0))
		return false;

	/* head or tail is incorrect value*/
	if ((head < 0 || head >= SLEEP_HISTORY_RINGBUFFER_SIZE) ||
		(tail < 0 || tail >= SLEEP_HISTORY_RINGBUFFER_SIZE)) {
		pr_info("[slp_his] invalid size -> head = %d,  tail =%d\n", head, tail);
		return false;
	}

	/* invalid access pointer */
	if ((buf_ptr + tail) == NULL) {
		pr_info("[slp_his] invalid buf -> buf_ptr: %p tail: %d\n", buf_ptr, tail);
		return false;
	}

	return true;
}

static int output_buf_check(struct sleep_history_data *data)
{
	int head = data->sleep_history.head;
	int tail = data->sleep_history.tail;

	/*
	 * head_n == tail_n: buffer is empty
	 * head -1 == tail: have only one buf data
	 */
	if (head - 1 == tail || head == tail) {
		return SLEEP_HISTORY_PRINT_STOP;
	}

	/* move to next output buffer */
	if (tail >= SLEEP_HISTORY_RINGBUFFER_SIZE - 1)
		data->sleep_history.tail = 0;
	else
		data->sleep_history.tail++;

	return SLEEP_HISTORY_PRINT_CONTINUE;
}

static int sleep_history_headline_show(char* buffer, size_t count, int offset)
{
	offset += snprintf(buffer + offset, count - offset, "    type      count     entry time          ");
	offset += snprintf(buffer + offset, count - offset, "exit time           ");
	offset += snprintf(buffer + offset, count - offset, "    diff      ");
	offset += snprintf(buffer + offset, count - offset, "battery         ");
	offset += snprintf(buffer + offset, count - offset, "                                wakeup source\n");
	offset += snprintf(buffer + offset, count - offset, "--- --------- --------- ------------------- ");
	offset += snprintf(buffer + offset, count - offset, "------------------- ");
	offset += snprintf(buffer + offset, count - offset, "------- ");
	offset += snprintf(buffer + offset, count - offset, "---------------- ");
	offset += snprintf(buffer + offset, count - offset, "-------------------------------------------------------------------------------------------\n");

	return offset;
}

static int print_sleep_history_time(
	char *buffer, int count, struct timespec *entry_ts, struct timespec *exit_ts, int offset)
{
	struct timespec delta;
	struct rtc_time entry_tm, exit_tm;

	if (entry_ts)
		rtc_time_to_tm(entry_ts->tv_sec, &entry_tm);
	else
		rtc_time_to_tm(0, &entry_tm);
	if (exit_ts)
		rtc_time_to_tm(exit_ts->tv_sec, &exit_tm);
	else
		rtc_time_to_tm(0, &exit_tm);
	if (entry_ts && exit_ts)
		delta = timespec_sub(*exit_ts, *entry_ts);

	offset += snprintf(buffer + offset, count - offset,
				"%04d-%02d-%02d/%02d:%02d:%02d "
				"%04d-%02d-%02d/%02d:%02d:%02d ",
				entry_tm.tm_year + 1900, entry_tm.tm_mon + 1,
				entry_tm.tm_mday, entry_tm.tm_hour,
				entry_tm.tm_min, entry_tm.tm_sec,
				exit_tm.tm_year + 1900, exit_tm.tm_mon + 1,
				exit_tm.tm_mday, exit_tm.tm_hour,
				exit_tm.tm_min, exit_tm.tm_sec);

	if (entry_ts && exit_ts)
		offset += snprintf(buffer + offset, count - offset, "%7d ", (int)delta.tv_sec);
	else
		offset += snprintf(buffer + offset, count - offset, "        ");

	return offset;
}

static int print_sleep_history_battery(
	char *buffer, int count, struct battery_history *entry, struct battery_history *exit, int offset)
{
	int capacity_delta = -1;

	if (!entry || !exit)
		return -EINVAL;

	if (entry->capacity != -1 && exit->capacity != -1)
		capacity_delta = exit->capacity - entry->capacity;

	offset += snprintf(buffer + offset, count - offset, "%3d %3d %3d %d %d   ",
					entry->capacity, exit->capacity, capacity_delta,
					entry->status, exit->status);

	return offset;
}

static int print_autoline_display(char* buffer, size_t count, int offset)
{
	struct sleep_history_data *output_data;
	struct sleep_history *output_buf;

	struct timespec *entry_ts = 0, *exit_ts = 0;
	struct battery_history batt_entry, batt_exit;

	union wakeup_history wakeup[WS_ARRAY_MAX];
	int wakeup_count = 0;

	/* Get sleep history output data */
	output_data = &copy_history_data;

	/* Get current output buffer */
	output_buf = get_cur_output_buf(output_data);

	/* Check output buffer */
	if (!is_valid_buf(output_data))
		return offset;

	/* Get autosleep entry info*/
	entry_ts = &(output_buf->ts);
	batt_entry.status = output_buf->battery.status;
	batt_entry.capacity = output_buf->battery.capacity;

	/* Increase tail count */
	if (output_buf_check(output_data) == SLEEP_HISTORY_PRINT_STOP)
		return offset;
	output_buf = get_cur_output_buf(output_data);

	/* Get autosleep exit info */
	exit_ts = &(output_buf->ts);
	if (!exit_ts || !entry_ts) {
		pr_info("[slp_his] auto line time fail\n");
		return offset;
	}
	batt_exit.status = output_buf->battery.status;
	batt_exit.capacity = output_buf->battery.capacity;
	memset(wakeup, 0, sizeof(union wakeup_history) * WS_ARRAY_MAX);
	wakeup[wakeup_count] = output_buf->ws;

	/* print sleep history */
	offset += snprintf(buffer + offset, count - offset, "%3d %9s           ",
						sleep_history_index++, type_text[(int)output_buf->type]);
	offset = print_sleep_history_time(buffer, count, entry_ts, exit_ts, offset);
	offset = print_sleep_history_battery(buffer, count, &batt_entry, &batt_exit, offset);
	offset += snprintf(buffer + offset, count - offset,
					"%s:%lld ", wakeup[wakeup_count].ws.name,
					ktime_to_ms(wakeup[wakeup_count].ws.prevent_time));
	wakeup_count++;

	/* check & print additional wakeup source */
	do {
		if (output_buf_check(output_data) == SLEEP_HISTORY_PRINT_STOP)
			return offset;
		output_buf = get_cur_output_buf(output_data);

		if (output_buf->type == SLEEP_HISTORY_AUTOSLEEP_EXIT) {
			wakeup[wakeup_count] = output_buf->ws;
			offset += snprintf(buffer + offset, count - offset,
					"%s:%lld ", wakeup[wakeup_count].ws.name,
					ktime_to_ms(wakeup[wakeup_count].ws.prevent_time));
			wakeup_count++;
		} else
			break;

	} while (wakeup_count < WS_ARRAY_MAX);
	offset += snprintf(buffer + offset, count - offset, "\n");

	/* suspend fail case */
	if (output_buf->type == SLEEP_HISTORY_AUTOSLEEP_ENTRY) {
		/* get entry info */
		entry_ts = exit_ts;
		batt_entry.status = batt_exit.status;
		batt_entry.capacity = batt_exit.capacity;

		/* get exit info */
		exit_ts = &(output_buf->ts);
		batt_exit.status = output_buf->battery.status;
		batt_exit.capacity = output_buf->battery.capacity;

		/* print info */
		if (output_buf->failed_step > 0) {
			int fail = output_buf->failed_step;
			offset += snprintf(buffer + offset, count - offset, "%3d %9s %9s ",
				sleep_history_index++, suspend_text[0], suspend_text[fail]);
		} else {
			output_data->sleep_history.tail--;
			return offset;
		}
		offset = print_sleep_history_time(buffer, count, entry_ts, exit_ts, offset);
		offset = print_sleep_history_battery(buffer, count, &batt_entry, &batt_exit, offset);
		offset += snprintf(buffer + offset, count - offset, "\n");
	}

	return offset;
}

static int print_suspendline_display(char *buffer, size_t count, int offset)
{
	struct sleep_history_data *output_data;
	struct sleep_history *output_buf;

	struct timespec *entry_ts = 0, *exit_ts = 0;
	struct battery_history batt_entry, batt_exit;

	struct irq_desc *desc;
	union wakeup_history wakeup[WS_ARRAY_MAX];
	int wakeup_count = 0;
	int i = 0;

	/* Get sleep history output data */
	output_data = &copy_history_data;

	/* Get current output buffer */
	output_buf = get_cur_output_buf(output_data);

	/* get entry info */
	entry_ts = &(output_buf->ts);
	batt_entry.status = output_buf->battery.status;
	batt_entry.capacity = output_buf->battery.capacity;
	wakeup_count = 0;
	memset(wakeup, 0, sizeof(union wakeup_history)*WS_ARRAY_MAX);

	/* get irq info */
	do {
		if (output_buf_check(output_data) == SLEEP_HISTORY_PRINT_STOP)
			return offset;
		output_buf = get_cur_output_buf(output_data);

		if (output_buf->type == SLEEP_HISTORY_WAKEUP_IRQ)
			wakeup[wakeup_count++] = output_buf->ws;
		else
			break;

	} while (wakeup_count < WS_ARRAY_MAX);

	/* get exit info */
	exit_ts = &(output_buf->ts);
	if (!exit_ts || !entry_ts) {
		pr_info("sleep_history suspend line time fail\n");
		return offset;
	}
	batt_exit.status = output_buf->battery.status;
	batt_exit.capacity = output_buf->battery.capacity;

	/* print data */
	if (output_buf->failed_step > 0) {
		int fail = output_buf->failed_step;

		offset += snprintf(buffer + offset, count - offset, "%3d %9s %9s ",
						sleep_history_index++, suspend_text[0], suspend_text[fail]);
	} else {
		offset += snprintf(buffer + offset, count - offset, "%3d %9s %9d ",
						sleep_history_index++, type_text[(int)output_buf->type],
						output_buf->suspend_count);
	}
	offset = print_sleep_history_time(buffer, count, entry_ts, exit_ts, offset);
	offset = print_sleep_history_battery(buffer, count, &batt_entry, &batt_exit, offset);

	/* print saved irq data */
	for (i = 0; i < wakeup_count && wakeup[i].slp_irq.irq; i++) {
		if (!strncmp(wakeup[i].slp_irq.name, "UNKNOWN", 7)) {
			desc = irq_to_desc(wakeup[i].slp_irq.irq);
			if (desc && desc->action && desc->action->name)
				strncpy(wakeup[i].slp_irq.name, (char*)desc->action->name, 16);
		}
		offset += snprintf(buffer + offset, count - offset, "%d,%s/ ",
							wakeup[i].slp_irq.irq, wakeup[i].slp_irq.name);
	}
	offset += snprintf(buffer + offset, count - offset, "\n");

	return offset;
}

static int sleep_history_print_control(char __user *buffer,
					size_t count, loff_t *ppos, enum sleep_history_type type)
{
	static char *buf = NULL;
	size_t buf_size = PAGE_SIZE;
	int offset = 0;
	int size_for_copy = count;
	static unsigned int rest_size = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf_size = min(buf_size, count);

	buf =vmalloc(buf_size);
	if (!buf)
		return -ENOMEM;

	memset(buf, 0, buf_size);

	if (*ppos == 0)
		offset += sleep_history_headline_show(buf, buf_size, offset);
	else {
		if (type == SLEEP_HISTORY_AUTOSLEEP_ENTRY)
			offset += print_autoline_display(buf, buf_size, offset);
		else if (type == SLEEP_HISTORY_SUSPEND_ENTRY)
			offset += print_suspendline_display(buf, buf_size, offset);
		else
			pr_err("[slp_his] %s: invalid type: %d\n", __func__, type);
	}

	if (offset <= count) {
		size_for_copy = offset;
		rest_size = 0;
	} else {
		size_for_copy = count;
		rest_size = offset - size_for_copy;
	}

	if (size_for_copy >  0) {
		if (copy_to_user(buffer, buf , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	}

	vfree(buf);
	return size_for_copy;
}

static ssize_t sleep_history_debug_read(struct file *file,
							char __user *buffer, size_t count, loff_t *ppos)
{
	struct sleep_history_data *output_data;
	struct sleep_history *output_buf;
	int copy_size = 0;

	/* Get sleep history output data */
	output_data = &copy_history_data;

	/* Get current output buffer */
	output_buf = get_cur_output_buf(output_data);

	/* check the head, tail and buf	*/
	if(!is_valid_buf(output_data))
		return 0;

	while(1) {
		if ((output_buf->type == SLEEP_HISTORY_AUTOSLEEP_ENTRY) ||
			(output_buf->type == SLEEP_HISTORY_SUSPEND_ENTRY))
			break;
		if (output_buf_check(output_data) == SLEEP_HISTORY_PRINT_STOP) {
			return 0;
		}
		output_buf = get_cur_output_buf(output_data);
	}

	/* print autosleep line or suspend line */
	switch (output_buf->type) {
	case SLEEP_HISTORY_AUTOSLEEP_ENTRY:
	case SLEEP_HISTORY_SUSPEND_ENTRY:
		copy_size += sleep_history_print_control(buffer, count, ppos, output_buf->type);
		break;
	default:
		break;
	}

	if (copy_size > 500)
		pr_info("[slp_his] %s - copy_size = %d\n", __func__, copy_size);

	return copy_size;
}

int sleep_history_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	pr_info("[slp_his] debug open\n");
	if (sleep_history_index != 0xFFFFFFFF)
		pr_info("[slp_his] sleep_history index invalid\n");
	copy_history_data = sleep_history_data;
	sleep_history_index = 1;

	return 0;
}

int sleep_history_debug_release(struct inode *inode, struct file *file)
{
	pr_info("[slp_his] debug release\n");
	sleep_history_index = 0xFFFFFFFF;
	return 0;
}

static const struct file_operations sleep_history_debug_fops = {
	.read		= sleep_history_debug_read,
	.open		= sleep_history_debug_open,
	.release	= sleep_history_debug_release,
};

static int __init sleep_history_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("sleep_history", S_IRUGO, NULL, NULL,
		&sleep_history_debug_fops);
	if (!d) {
		pr_err("Failed to create sleep_history debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(sleep_history_debug_init);
#endif


int sleep_history_marker(int type, struct timespec *ts,
				struct wakeup_source* wakeup,
				unsigned int irq, const char *irq_name)
{
	struct sleep_history_data *input_data;
	struct sleep_history *input_buf;
	struct power_supply *psy_battery;
	struct rtc_time tm;
	union wakeup_history *wakeup_ptr;

	if (type >= SLEEP_HISTORY_TYPE_MAX  || (!ts && !wakeup && !irq))
		return -EINVAL;

	pr_debug("[slp_his] marker: %d\n", type);

	/* Get sleep history input data */
	input_data = &sleep_history_data;

	/* Get current input buffer */
	input_buf = get_cur_input_buf(input_data);
	memset(input_buf, 0, sizeof(struct sleep_history));

	/* Save type */
	input_buf->type = type;

	/* Save battery info */
	psy_battery = power_supply_get_by_name("battery");
	if (psy_battery) {
		int err;
		union power_supply_propval value;

		err = psy_battery->get_property(psy_battery,
				POWER_SUPPLY_PROP_STATUS, &value);
		if (err < 0)
			input_buf->battery.status = -1;
		else
			input_buf->battery.status = value.intval;

		err = psy_battery->get_property(psy_battery,
				POWER_SUPPLY_PROP_CAPACITY, &value);
		if (err < 0)
			input_buf->battery.capacity = -1;
		else
			input_buf->battery.capacity = value.intval;
	}

	/* Save time info */
	set_buf_ts(input_buf, ts);

	switch (type) {
#ifdef CONFIG_PM_AUTOSLEEP
	case SLEEP_HISTORY_AUTOSLEEP_ENTRY:
		if (suspend_stats_bkup.fail != suspend_stats.fail) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			input_buf->failed_step = suspend_stats.failed_steps[last_step];
		}
		break;
	case SLEEP_HISTORY_AUTOSLEEP_EXIT:
		memcpy(&suspend_stats_bkup, &suspend_stats, sizeof(struct suspend_stats));
		if (ts) {
			rtc_time_to_tm(ts->tv_sec, &tm);
			pr_cont("t:%d/%d/%d/%d/%d/%d ",	tm.tm_year, tm.tm_mon,
											tm.tm_mday, tm.tm_hour,
											tm.tm_min, tm.tm_sec);
		}
		if (wakeup) {
			wakeup_ptr = &(input_buf->ws);
			memcpy(wakeup_ptr->ws.name, wakeup->name, 15);
			wakeup_ptr->ws.name[15] = '\0';
			wakeup_ptr->ws.prevent_time = wakeup->prevent_time;
			pr_info("[slp_his] ws:%s/%lld\n",
					wakeup_ptr->ws.name, ktime_to_ms(wakeup_ptr->ws.prevent_time));
		}
		break;
#endif
	case SLEEP_HISTORY_WAKEUP_IRQ:
		if (irq) {
			wakeup_ptr = &(input_buf->ws);
			wakeup_ptr->slp_irq.irq = irq;
			memcpy(wakeup_ptr->slp_irq.name, irq_name, 15);
			wakeup_ptr->slp_irq.name[15] = '\0';
			pr_info("[slp_his] irq: %d(%s)\n",
					wakeup_ptr->slp_irq.irq, wakeup_ptr->slp_irq.name);
		}
		break;
	case SLEEP_HISTORY_SUSPEND_ENTRY:
		break;
	case SLEEP_HISTORY_SUSPEND_EXIT:
		if (suspend_stats_bkup.fail != suspend_stats.fail ||
			suspend_stats_bkup.failed_prepare != suspend_stats.failed_prepare ||
			suspend_stats_bkup.failed_suspend != suspend_stats.failed_suspend ||
			suspend_stats_bkup.failed_suspend_late != suspend_stats.failed_suspend_late ||
			suspend_stats_bkup.failed_suspend_noirq != suspend_stats.failed_suspend_noirq) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			input_buf->failed_step = suspend_stats.failed_steps[last_step];
		} else {
			input_buf->suspend_count = suspend_stats.success + 1;
		}
		break;
	default:
		return -EPERM;
	}

	move_to_next_input_buf(input_data);

	return 0;
}

static int sleep_history_syscore_init(void)
{
	/* Init circ buf for sleep history*/
	sleep_history_data.sleep_history.head = 0;
	sleep_history_data.sleep_history.tail = 0;
	sleep_history_data.sleep_history.buf = (char *)kmalloc(sizeof(struct sleep_history) *
							SLEEP_HISTORY_RINGBUFFER_SIZE, GFP_KERNEL);
	memset(sleep_history_data.sleep_history.buf, 0,
				sizeof(struct sleep_history) * SLEEP_HISTORY_RINGBUFFER_SIZE);

	return 0;
}

static void sleep_history_syscore_exit(void)
{
	kfree(sleep_history_data.sleep_history.buf);
}
module_init(sleep_history_syscore_init);
module_exit(sleep_history_syscore_exit);
