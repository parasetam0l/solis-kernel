/*
 * To monitor and log cpuidle state at every suspend
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Sangin Lee <sangin78.lee@samsung.com>
 * Hunsup Jung <hunsup.jung@samsung.com>
 * Junho Jang <vincent.jang@samsung.com>
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/cpuidle.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif


#define IGNORE_TIME_S 5
#define PRETTY_STEPS 12
#define SHORT_TIME 1
#define MONITOR_STATE 1

struct cpuidle_device *cpu_idle_dev_mon;

/* To get wakeup reason(irq), called by sleep monitor */
static int slp_mon_cpuidle_cb(void *priv, unsigned int *raw_val, int check_level, int caller_type)
{

	static struct timespec ts_resume = {0, 0};
	struct timespec ts_suspend = {0, 0};
	struct timespec ts_diff = {0, 0};

	static unsigned long long time_resume;
	static unsigned long long usage_resume;
	unsigned long long time_suspend;
	unsigned long long usage_suspend;
	unsigned long long time_diff;
	unsigned long long usage_diff;
	unsigned long long time_active;
	unsigned long long time_active_per;

	unsigned int temp;


	if (cpu_idle_dev_mon == NULL) {
		*raw_val = 0xABCDABCD;
		return 0;
	}

	/* 1. check at resume time - save boottime and cpuidle time and usage for each cpu(s) and state(s)
	 * 2-1. check at suspend time - save boottime and cpuidel time and usage for each cpu(s) and state(s)
	 * 2-2. calculate pretty and raw value
	 *          - raw: 0xaaaabbbb where aaaa is increased usage, bbbb is increased time in sec.
	 *          - pretty: 0 for resume
	 *                    1 for short time (less than 5 sec)
	 *                    2, 3 for reserved
	 *                    4~15 for usage (higher is heavy use case)
	 */


	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		/* Get informations */
		get_monotonic_boottime(&ts_suspend);
		time_suspend = cpu_idle_dev_mon->states_usage[MONITOR_STATE].time;
		usage_suspend = cpu_idle_dev_mon->states_usage[MONITOR_STATE].usage;

		/* Calculate increasement */
		ts_diff = timespec_sub(ts_suspend, ts_resume);
		time_diff = (time_suspend - time_resume) / 1000000;
		usage_diff = usage_suspend - usage_resume;

		if (time_diff > 0xffff)
			time_diff = 0xffff;
		if (usage_diff > 0xffff)
			usage_diff = 0xffff;

		/*  Raw value */
		*raw_val = ((usage_diff & 0xffff) << 16) | (((__kernel_time_t)time_diff) & 0xffff);
		temp = *raw_val;

		/* If elapsed time is too short, return 1 for readibility */
		if (ts_diff.tv_sec < IGNORE_TIME_S)
			return SHORT_TIME;


		/* Calculate pretty value */
		// TODO:: Need to consider about time unit - s? us?
		time_active = (ts_diff.tv_sec - (__kernel_time_t)time_diff);
		time_active_per = time_active * PRETTY_STEPS;
		do_div(time_active_per, ts_diff.tv_sec);

		if (time_active_per >= PRETTY_STEPS)
			time_active_per = PRETTY_STEPS-1;

		pr_debug("ts=%15lu, idle_t=%llu, active_t=%llu, active_p=%llu, ret=%llu, raw=%08x\n",
			ts_diff.tv_sec, time_diff, time_active, time_active_per, (DEVICE_UNKNOWN - PRETTY_STEPS) + time_active_per, temp);

		return (DEVICE_UNKNOWN - PRETTY_STEPS) + time_active_per;

	} else if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		/* Get informations */
		get_monotonic_boottime(&ts_resume);
		time_resume = cpu_idle_dev_mon->states_usage[MONITOR_STATE].time;
		usage_resume = cpu_idle_dev_mon->states_usage[MONITOR_STATE].usage;

		return 0;
	}

	return 0;
}

static struct sleep_monitor_ops slp_mon_cpuidle_dev = {
	.read_cb_func = slp_mon_cpuidle_cb,
};


static int slp_mon_cpuidle_dev_init(void)
{
	pr_debug("%s\n", __func__);

	sleep_monitor_register_ops(NULL, &slp_mon_cpuidle_dev, SLEEP_MONITOR_CPUIDLE);

	return 0;
}

static void slp_mon_cpuidle_dev_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(slp_mon_cpuidle_dev_init);
module_exit(slp_mon_cpuidle_dev_exit);
