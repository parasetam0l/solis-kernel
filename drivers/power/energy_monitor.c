
/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
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
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Sangin Lee <sangin78.lee@samsung.com>       Initial Release           *
 *       Sanghyeon Lee <sirano06.lee@samsung.com>                              *
 *       Junyoun Kim <junyouns.kim@samsung.com>                                *
 * ---- -------------------------------------------- ------------------------- *
 * 2.0   Hunsup Jung <hunsup.jung@samsung.com>       Remove unnecessary code   *
 *                                                   Add info - battery type   *
 *                                                            - battery temp   *
 *                                                   Remove info - status      *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>
#include <linux/slab.h>

#include <linux/power/energy_monitor.h>
#include <linux/battery/sec_battery.h>
#include <linux/time_history.h>

#ifdef CONFIG_SENSORHUB_STAT
#include <linux/sensorhub_stat.h>
#endif
#ifdef CONFIG_SAP_PID_STAT
#include <linux/sap_pid_stat.h>
#endif
#ifdef CONFIG_PID_STAT
#include <linux/pid_stat.h>
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
#include <linux/power/slave_wakelock.h>
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
#include <linux/platform_data/sec_thermistor_history.h>
#endif
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
static struct device *sec_energy_monitor;
#endif
#ifdef CONFIG_LBS_HISTORY
#include <linux/lbs_history.h>
#endif

#define ENERGY_MON_VERSION "2.0"

#define ENERGY_MON_HISTORY_NUM 64
#define ENERGY_MONITOR_MAX_MONITOR_INTERVAL 86400 /* second */

// TODO: Need to seperate model specific information
#if defined(CONFIG_MACH_VOLT)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#define CONFIG_ENERGY_MONITOR_USE_RAW_SOC
#define BATTERY_mAh 200
#elif defined(CONFIG_SOLIS)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#define CONFIG_ENERGY_MONITOR_USE_RAW_SOC
#define BATTERY_mAh 380
#elif defined(CONFIG_POP)
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#define CONFIG_ENERGY_MONITOR_USE_RAW_SOC
#define BATTERY_mAh 300
#else
#define CONFIG_ENERGY_MONITOR_WAKEUP_STAT
#define CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
#define CONFIG_ENERGY_MONITOR_USE_RAW_SOC
#define BATTERY_mAh 250
#endif

#define UNIT_BATTERY_mAs (BATTERY_mAh*3600/100)

#define ENERGY_MON_MAX_WAKEUP_STAT_TIME 16
#define ENERGY_MON_IRQ_NAME_LENGTH 30

#define ENERGY_MON_MAX_SLEEP_ESTIMATOR_CNT 20

#define MIN_SLEEP_TIME_S 500 /* second */

#define ENERGY_MON_DEBUG_PREFIX "[energy_mon]"
#define energy_mon_dbg(debug_level_mask, fmt, ...)\
do {\
	if (debug_level & debug_level_mask)\
		pr_info("%s[%d]" fmt, ENERGY_MON_DEBUG_PREFIX,\
						debug_level_mask, ##__VA_ARGS__);\
} while (0)

enum ENERGY_MON_DEBUG_LEVEL {
	ENERGY_MON_DEBUG_INFO = BIT(0),
	ENERGY_MON_DEBUG_ERR = BIT(1),
	ENERGY_MON_DEBUG_WARN = BIT(2),
	ENERGY_MON_DEBUG_DBG = BIT(3),
	ENERGY_MON_DEBUG_WAKEUP_STAT = BIT(4),
	ENERGY_MON_DEBUG_SLEEP_ESTIMATOR = BIT(5),
};

enum energy_mon_print_step {
	STEP_SUMMARY = 0,
#ifdef CONFIG_SENSORHUB_STAT
	STEP_SENSORHUB_STAT,
#endif
#ifdef CONFIG_SAP_PID_STAT
	STEP_SAP_STAT,
#endif
#ifdef CONFIG_PID_STAT
	STEP_PID_STAT,
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	STEP_SLAVE_WAKELOCK,
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	STEP_SEC_THERM_HISTORY,
#endif
#ifdef CONFIG_SENSORHUB_STAT
	STEP_GPS_STAT,
#endif
#ifdef CONFIG_LBS_HISTORY
	STEP_LBS_HISTORY,
#endif
	STEP_MAX
};

enum energy_mon_print_type {
	ENERGY_MON_PRINT_TITLE = 0,
	ENERGY_MON_PRINT_MAIN,
	ENERGY_MON_PRINT_TAIL,
};

struct energy_mon_data {
	int type;
	int log_count;
	int suspend_count;
	int bat_status;
	int bat_capacity;
	int cable_type;
	int bat_temp;
	struct timespec ts_real;
	struct timespec ts_boot;
	struct timespec ts_kern;
	struct timespec ts_disp;
	int wakeup_cause[ENERGY_MON_WAKEUP_MAX];
#ifdef CONFIG_SENSORHUB_STAT
	struct sensorhub_wakeup_stat sh_wakeup;
#endif
#ifdef CONFIG_SAP_PID_STAT
	struct sap_pid_wakeup_stat sap_wakeup;
#endif
#ifdef CONFIG_PID_STAT
	struct pid_stat_monitor pid_stat_mon[PID_STAT_ARRAY_SIZE];
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	struct slp_mon_slave_wakelocks slwl[SLWL_ARRAY_SIZE];
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	struct sec_therm_history_info therm_history[MAX_SEC_THERM_DEVICE_NUM];
#endif
#ifdef CONFIG_SENSORHUB_STAT
	struct sensorhub_gps_stat sh_gps;
#endif
#ifdef CONFIG_LBS_HISTORY
	struct gps_request lbs_gps[LBS_HISTORY_ARRAY_SIZE];
	struct wps_request lbs_wps[LBS_HISTORY_ARRAY_SIZE];
#endif
	int penalty_score;
};

struct energy_monitor_cb {
	int running;
	int data_index;
	int read_index;

	int charging_count;
	int discharging_count;

	int wakeup_cause[ENERGY_MON_WAKEUP_MAX];
	int penalty_score;
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	int last_wakeup;
	struct timespec kern_last_wakeup;
	struct timespec kern_wakeup_time[ENERGY_MON_WAKEUP_MAX];
	unsigned int wakeup_time_stats[ENERGY_MON_MAX_WAKEUP_STAT_TIME];
#endif
#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
	int estimator_index;
	long long estimator_average[ENERGY_MON_MAX_SLEEP_ESTIMATOR_CNT];
#endif
	int disp_state;
	struct timespec disp_total;
	struct timespec disp_last_on;

	struct energy_mon_data boot;
	struct energy_mon_data data[ENERGY_MON_HISTORY_NUM];
	struct energy_mon_data dump;

	struct energy_mon_data charging;
	struct energy_mon_data discharging;

	struct energy_mon_data charging_dump;
	struct energy_mon_data discharging_dump;

	long long unit_bat_capacity;
};

struct energy_mon_mapping_table_t {
	char irq_name[ENERGY_MON_IRQ_NAME_LENGTH];
	int wakeup_idx;
};

/* global variable */
static struct energy_monitor_cb energy_mon;

static int energy_monitor_enable = 1;
static unsigned int monitor_interval = 120; /* 0: Disable, 1~86400: Enable */
static unsigned int logging_interval = 3590; /* 0: Disable, 1~86400: Enable */
static struct delayed_work monitor_work;

static int debug_level = ENERGY_MON_DEBUG_INFO |
					ENERGY_MON_DEBUG_ERR |
					ENERGY_MON_DEBUG_WARN |
					ENERGY_MON_DEBUG_SLEEP_ESTIMATOR |
					ENERGY_MON_DEBUG_WAKEUP_STAT;

// TODO: Need to seperate model specific information
#if defined(CONFIG_SOLIS)
static struct energy_mon_mapping_table_t energy_mon_table[] = {
	/* ENERGY_MON_WAKEUP_INPUT */
	{"sec_touchscreen", ENERGY_MON_WAKEUP_INPUT},	/* sec_touchscreen */
	{"KEY_BACK", ENERGY_MON_WAKEUP_INPUT},			/* KEY_BACK */
	{"pwronr-irq", ENERGY_MON_WAKEUP_INPUT},		/* pwronr-irq */
	{"pwronf-irq", ENERGY_MON_WAKEUP_INPUT},		/* pwronf-irq */
	{"hall_a_status", ENERGY_MON_WAKEUP_INPUT},		/* hall_a_status */
	{"hall_b_status", ENERGY_MON_WAKEUP_INPUT},		/* hall_b_status */
	{"hall_c_status", ENERGY_MON_WAKEUP_INPUT},		/* hall_c_status */

	/* ENERGY_MON_WAKEUP_SSP */
	{"ttyBCM", ENERGY_MON_WAKEUP_SSP},				/* ttyBCM */

	/* ENERGY_MON_WAKEUP_RTC */
	{"rtc-alarm0", ENERGY_MON_WAKEUP_RTC},			/* rtc-alarm0 */

	/* ENERGY_MON_WAKEUP_BT */
	{"BT", ENERGY_MON_WAKEUP_BT},		/* bt host_wake */

	/* ENERGY_MON_WAKEUP_WIFI */
	{"WIFI", ENERGY_MON_WAKEUP_WIFI},		/* bcmsdh_sdmmc */

	/* ENERGY_MON_WAKEUP_CP */
	{"CP", ENERGY_MON_WAKEUP_CP},	/* mcu_ipc_cp */
};
#elif defined(CONFIG_POP)
static struct energy_mon_mapping_table_t energy_mon_table[] = {
	/* ENERGY_MON_WAKEUP_INPUT */
	{"sec_touchscreen", ENERGY_MON_WAKEUP_INPUT},	/* sec_touchscreen */
	{"KEY_BACK", ENERGY_MON_WAKEUP_INPUT},			/* KEY_BACK */
	{"pwronr-irq", ENERGY_MON_WAKEUP_INPUT},		/* pwronr-irq */
	{"pwronf-irq", ENERGY_MON_WAKEUP_INPUT},		/* pwronf-irq */
	{"hall_a_status", ENERGY_MON_WAKEUP_INPUT},		/* hall_a_status */
	{"hall_b_status", ENERGY_MON_WAKEUP_INPUT},		/* hall_b_status */
	{"hall_c_status", ENERGY_MON_WAKEUP_INPUT},		/* hall_c_status */

	/* ENERGY_MON_WAKEUP_SSP */
	{"ttyBCM", ENERGY_MON_WAKEUP_SSP},				/* ttyBCM */

	/* ENERGY_MON_WAKEUP_RTC */
	{"rtc-alarm0", ENERGY_MON_WAKEUP_RTC},			/* rtc-alarm0 */

	/* ENERGY_MON_WAKEUP_BT */
	{"bt_host_wake", ENERGY_MON_WAKEUP_BT},		/* bt host_wake */

	/* ENERGY_MON_WAKEUP_WIFI */
	{"WIFI", ENERGY_MON_WAKEUP_WIFI},		/* bcmsdh_sdmmc */

};
#else
static struct energy_mon_mapping_table_t energy_mon_table[] = {
};
#endif

/*
 * Functions for marking and recording
 */
int energy_monitor_record_disp_time(int type)
{
	struct timespec ts_curr = {0, 0};
	struct timespec ts_sub = {0, 0};

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: %d(%s)\n", __func__, type, type ? "off" : "on");

	energy_mon.disp_state = type;

	if (type == ENERGY_MON_DISP_ON)
		get_monotonic_boottime(&energy_mon.disp_last_on);
	else if (type == ENERGY_MON_DISP_OFF) {
		/* Aggregate total disp time */
		get_monotonic_boottime(&ts_curr);
		ts_sub = timespec_sub(ts_curr, energy_mon.disp_last_on);
		energy_mon.disp_total = timespec_add(energy_mon.disp_total, ts_sub);
	}

	/* Debug logs */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s\n", __func__);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "on___: %15lu.%09lu\n", energy_mon.disp_last_on.tv_sec,
																 energy_mon.disp_last_on.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "off__: %15lu.%09lu\n", ts_curr.tv_sec,
																 ts_curr.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "total: %15lu.%09lu\n", energy_mon.disp_total.tv_sec,
																 energy_mon.disp_total.tv_nsec);

	return 0;
}
EXPORT_SYMBOL_GPL(energy_monitor_record_disp_time);

static struct timespec energy_monitor_get_disp_time(struct timespec ts_curr)
{
	if (energy_mon.disp_state == ENERGY_MON_DISP_OFF)
		return energy_mon.disp_total;
	else
		return timespec_add(energy_mon.disp_total, timespec_sub(ts_curr, energy_mon.disp_last_on));
}

static void energy_mon_get_time_info(struct energy_mon_data *p_curr)
{
	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
		return;
	}

	p_curr->ts_real = current_kernel_time();
	get_monotonic_boottime(&p_curr->ts_boot);
	p_curr->ts_kern = ktime_to_timespec(ktime_get());
	p_curr->ts_disp = energy_monitor_get_disp_time(p_curr->ts_boot);
	p_curr->suspend_count = suspend_stats.success;

	/* Debug logs */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s\n", __func__);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_real: %15lu.%09lu\n", p_curr->ts_real.tv_sec,
																   p_curr->ts_real.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_boot: %15lu.%09lu\n", p_curr->ts_boot.tv_sec,
																   p_curr->ts_boot.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_kern: %15lu.%09lu\n", p_curr->ts_kern.tv_sec,
																   p_curr->ts_kern.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_disp: %15lu.%09lu\n", p_curr->ts_disp.tv_sec,
																   p_curr->ts_disp.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "suspend_count=%04d\n", p_curr->suspend_count);
}

static void energy_mon_get_battery_info(struct energy_mon_data *p_curr)
{
	struct power_supply *psy_battery = NULL;
	union power_supply_propval value;
	int err;

	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
		return;
	}

	psy_battery = power_supply_get_by_name("battery");
	if (psy_battery) {
		err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_STATUS, &value);
		if (err < 0)
			value.intval = -1;
		p_curr->bat_status = value.intval;

#ifdef CONFIG_ENERGY_MONITOR_USE_RAW_SOC
		value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
		psy_do_property("sec-fuelgauge", get, POWER_SUPPLY_PROP_CAPACITY, value);
		p_curr->bat_capacity = value.intval;
#else
		err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_CAPACITY, &value);
		if (err < 0)
			value.intval = -1;
		p_curr->bat_capacity = value.intval * 100;
#endif

		err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_ONLINE, &value);
		if (err < 0)
			value.intval = -1;
		p_curr->cable_type = value.intval;

		err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_TEMP, &value);
		if (err < 0)
			value.intval = -999;
		p_curr->bat_temp = value.intval;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "bat_stat=%d, bat_capa=%03d\n", p_curr->bat_status,
																		 p_curr->bat_capacity);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "cable_type=%d, bat_temp=%03d\n", p_curr->cable_type,
																		   p_curr->bat_temp);
}

// TODO: code review and verifying
static int energy_monitor_calculate_penalty_score(struct energy_mon_data *p_curr,
	struct energy_mon_data *p_prev)
{
	long long average;
	int diff_soc;
	long diff_boot = 0;

	if (!p_prev || !p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
		return -1;
	}

	diff_soc = p_curr->bat_capacity - p_prev->bat_capacity;
	diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;

	if (diff_boot <= 0)
		return -1;

	average = abs64(energy_mon.unit_bat_capacity * (p_curr->bat_capacity - p_prev->bat_capacity));
	do_div(average, diff_boot);

	/*
	 * penalty_score scheme - above 40 points(critical point) alarm quick discharging
	 * above 40 minitues usage times - based on average currents
	 *   - above average 30mA currents - +40 points
	 *   - above average 25mA currents - +20 points
	 * under 40 minitues usage times - based on dSOC
	 *   - above 20% dSOC - +40 points
	 *   - above 15% dSOC - +20 points
	 */
	if (diff_soc < 0) {
		if (diff_boot >= 40 * 60) {
			if (average / 100 >= 30)
				return 40;
			else if (average / 100 >= 25)
				return 20;
		} else {
			if (diff_soc <= -2000)
				return 40;
			else if (diff_soc <= -1500)
				return 20;
		}
	}

	return 0;
}

static void energy_mon_get_additional_info(int type,
		struct energy_mon_data *p_curr,	struct energy_mon_data *p_prev)
{
#if defined(CONFIG_SENSORHUB_STAT) || defined(CONFIG_SAP_PID_STAT)
	int i;
#endif

	if (!p_curr) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
		return;
	}

#ifdef CONFIG_SENSORHUB_STAT
	sensorhub_stat_get_wakeup(&p_curr->sh_wakeup);
	for (i = 0; i < SENSORHUB_LIB_MAX; i++)
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "sh: %d: %d(%d)\n", i,
																 p_curr->sh_wakeup.wakeup_cnt[i],
																 p_curr->sh_wakeup.ap_wakeup_cnt[i]);
#endif
#ifdef CONFIG_SAP_PID_STAT
	sap_stat_get_wakeup(&p_curr->sap_wakeup);
	for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "sap: %d: %d(%d)\n", i,
																  p_curr->sap_wakeup.wakeup_cnt[i],
																  p_curr->sap_wakeup.activity_cnt[i]);
#endif
#ifdef CONFIG_PID_STAT
	get_pid_stat_monitor(p_curr->pid_stat_mon, PID_STAT_ARRAY_SIZE);
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	get_sleep_monitor_slave_wakelock(type, p_curr->slwl, SLWL_ARRAY_SIZE);
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	if (type != ENERGY_MON_TYPE_DUMP) {
		get_sec_therm_history_energy_mon(type, p_curr->therm_history);
		for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "thm: %d: %d~%d, %d[%d/%d] %d\n",
					i, p_curr->therm_history[i].min, p_curr->therm_history[i].max,
					p_curr->therm_history[i].sum / p_curr->therm_history[i].cnt,
					p_curr->therm_history[i].sum, p_curr->therm_history[i].cnt,
					p_curr->therm_history[i].reset);
	}
#endif
#ifdef CONFIG_SENSORHUB_STAT
	sensorhub_stat_get_gps_info(&p_curr->sh_gps);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "gps: %d\n", p_curr->sh_gps.gps_time);
#endif
#ifdef CONFIG_LBS_HISTORY
	get_top_lbs_time(type, p_curr->lbs_gps, p_curr->lbs_wps, LBS_HISTORY_ARRAY_SIZE);
#endif
	if (type != ENERGY_MON_TYPE_DUMP) {
		p_curr->penalty_score = energy_monitor_calculate_penalty_score(p_curr, p_prev);
		if (p_curr->penalty_score > 0)
			energy_mon.penalty_score += p_curr->penalty_score;
	}
}

static int energy_monitor_inject_data(int type,
	struct energy_mon_data *p_curr, struct energy_mon_data *p_prev)
{
	struct energy_mon_data *p_buff;
	struct timespec time_diff;
	int charging_discharging;
	int i;

	/*
	 * If type is battery and monitor, inject data to charging buffer and
	 * if type is dump, inject data to dump buffer and charging_dump buffer
	 */
	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR) {
		if (!p_prev || !p_curr) {
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: pointer is NULL\n", __func__);
			return -EINVAL;
		}

		if (p_prev->bat_status == POWER_SUPPLY_STATUS_CHARGING) {
			p_buff = &energy_mon.charging;
			energy_mon.charging_count++;
			charging_discharging = ENERGY_MON_STATE_CHARGING;
		} else if (p_prev->bat_status == POWER_SUPPLY_STATUS_DISCHARGING) {
			p_buff = &energy_mon.discharging;
			energy_mon.discharging_count++;
			charging_discharging = ENERGY_MON_STATE_DISCHARGING;
		} else {
			/* If not logging case - i.e POWER_SUPPLY_STATUS_FULL and so on, just return 0 */
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
					"%s: not logging case, type=%d, prev bat_status=%d\n", __func__,
					type, p_prev->bat_status);
			return 0;
		}
	} else if (type == ENERGY_MON_TYPE_DUMP) {
		if (energy_mon.dump.bat_status == POWER_SUPPLY_STATUS_CHARGING) {
			p_buff = &energy_mon.charging_dump;
			charging_discharging = ENERGY_MON_STATE_CHARGING;
		} else if (energy_mon.dump.bat_status != POWER_SUPPLY_STATUS_CHARGING) {
			p_buff = &energy_mon.discharging_dump;
			charging_discharging = ENERGY_MON_STATE_DISCHARGING;
		} else {
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: unknown case\n", __func__);
			return -EPERM;
		}

		memcpy(&energy_mon.discharging_dump, &energy_mon.discharging,
				sizeof(struct energy_mon_data));
		memcpy(&energy_mon.charging_dump, &energy_mon.charging,
				sizeof(struct energy_mon_data));

		if (energy_mon.data_index == 0)
			p_prev = &energy_mon.boot;
		else {
			int prev_idx = (energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM;

			p_prev = &energy_mon.data[prev_idx];
		}
		p_curr = &energy_mon.dump;
	} else {
		/* If not logging case, just return 0 */
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: not logging case, type=%d", __func__, type);
		return 0;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: use %s buffer\n",
		__func__, charging_discharging ? "discharging" : "charging");

	/* Data checking routine */
	switch (charging_discharging) {
	case ENERGY_MON_STATE_DISCHARGING:
		if (p_curr->bat_capacity > p_prev->bat_capacity)
			energy_mon_dbg(ENERGY_MON_DEBUG_WARN,
				"%s: capacity is changed from %d to %d even discharged.\n", __func__,
				p_prev->bat_capacity, p_curr->bat_capacity);
		break;
	case ENERGY_MON_STATE_CHARGING:
		if (p_curr->bat_capacity < p_prev->bat_capacity)
			energy_mon_dbg(ENERGY_MON_DEBUG_WARN,
				"%s: capacity is changed from %d to %d even charged.\n", __func__,
				p_prev->bat_capacity, p_curr->bat_capacity);
		break;
	default:
		break;
	}

	if (p_curr->bat_capacity > p_prev->bat_capacity)
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
			"%s: capacity is changed from %d to %d\n", __func__,
			p_prev->bat_capacity, p_curr->bat_capacity);

	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR) {
		p_buff->log_count++;
		p_buff->bat_capacity += p_curr->bat_capacity - p_prev->bat_capacity;
		p_buff->suspend_count += p_curr->suspend_count - p_prev->suspend_count;
	}

	/* If diff_time is negative, change to zero */
	time_diff = timespec_sub(p_curr->ts_kern, p_prev->ts_kern);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_kern = timespec_add(p_buff->ts_kern, time_diff);

	time_diff = timespec_sub(p_curr->ts_boot, p_prev->ts_boot);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_boot = timespec_add(p_buff->ts_boot, time_diff);

	time_diff = timespec_sub(p_curr->ts_disp, p_prev->ts_disp);
	if (time_diff.tv_sec < 0) {
		time_diff.tv_sec = 0;
		time_diff.tv_nsec = 0;
	}
	p_buff->ts_disp = timespec_add(p_buff->ts_disp, time_diff);

	for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++)
		p_buff->wakeup_cause[i] += p_curr->wakeup_cause[i] - p_prev->wakeup_cause[i];

	/* Debug logs */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s\n", __func__);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_boot: %15lu.%09lu\n", p_buff->ts_boot.tv_sec,
																   p_buff->ts_boot.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_kern: %15lu.%09lu\n", p_buff->ts_kern.tv_sec,
																   p_buff->ts_kern.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "ts_disp: %15lu.%09lu\n", p_buff->ts_disp.tv_sec,
																   p_buff->ts_disp.tv_nsec);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "bat_stat=%d, bat_capa=%03d\n", p_buff->bat_status,
																		 p_buff->bat_capacity);
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "suspend_count=%04d\n", p_buff->suspend_count);

	return 0;
}

int energy_monitor_marker(int type)
{
	/* Do common works */
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: type=%d\n", __func__, type);

	if (type == ENERGY_MON_TYPE_BOOTING) {
		/* Call LCD on command at boot time */
		struct timespec disp_start = {0, 0};

		energy_mon.running = 1;
		energy_mon.disp_state = ENERGY_MON_DISP_ON;
		energy_mon.disp_last_on = disp_start;
	} else if (!energy_mon.running) {
		/* If marker is called before running(e.g. booting call), just ignore it */
		energy_mon_dbg(ENERGY_MON_DEBUG_WARN, "%s: called before running\n", __func__);
		return 0;
	}

	/* Assign proper buffer to save */
	if (type == ENERGY_MON_TYPE_BOOTING)
		p_curr = &energy_mon.boot;
	else if (type == ENERGY_MON_TYPE_DUMP)
		p_curr = &energy_mon.dump;
	else {
		p_curr = &energy_mon.data[energy_mon.data_index % ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index == 0) {
			/* If it is 1st marker, use boot data as previous one */
			p_prev = &energy_mon.boot;
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: use boot buffer as previous one\n", __func__);
		} else {
			p_prev = &energy_mon.data[(energy_mon.data_index - 1) % ENERGY_MON_HISTORY_NUM];
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
				"%s: use %d buffer as previous one\n", __func__, energy_mon.data_index);
		}
		p_curr->log_count = energy_mon.data_index;
	}

	/* Get time informations */
	energy_mon_get_time_info(p_curr);

	/* Get battery informations */
	energy_mon_get_battery_info(p_curr);

	/* Get wakeup reason informations */
	memcpy(p_curr->wakeup_cause, energy_mon.wakeup_cause, sizeof(energy_mon.wakeup_cause));
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"sizeof(energy_mon.wakeup_cause)=%lu\n",
		sizeof(energy_mon.wakeup_cause));

	/* Get additional informations */
	if (type == ENERGY_MON_TYPE_BATTERY ||
		type == ENERGY_MON_TYPE_MONITOR ||
		type == ENERGY_MON_TYPE_DUMP)
		energy_mon_get_additional_info(type, p_curr, p_prev);

	/* Inject to charging/discharging buffer */
	if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR)
		energy_monitor_inject_data(type, p_curr, p_prev);
	else if (type == ENERGY_MON_TYPE_BOOTING) {
		/* Compensate disp time at boot. Add ts_kern to ts_disp */
		energy_mon.boot.ts_disp = p_curr->ts_kern;
	}

	/* Add data_index after fill all datas if data ring buffer is used */
	if (type >= ENERGY_MON_TYPE_BATTERY && type < ENERGY_MON_TYPE_DUMP)
		energy_mon.data_index++;

	return 0;
}
EXPORT_SYMBOL_GPL(energy_monitor_marker);

static int energy_monitor_background_marker(void)
{
	struct timespec ts_curr;
	struct timespec ts_elapsed;
	struct energy_mon_data *p_prev;

	get_monotonic_boottime(&ts_curr);

	if (energy_mon.data_index == 0)
		p_prev = &energy_mon.boot;
	else
		p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	ts_elapsed = timespec_sub(ts_curr, p_prev->ts_boot);


	if (ts_elapsed.tv_sec >= logging_interval)
		energy_monitor_marker(ENERGY_MON_TYPE_MONITOR);

	return 0;
}

static int energy_monitor_find_wakeup_index(const char *irq_name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(energy_mon_table); i++) {
		if (!strncmp(irq_name, energy_mon_table[i].irq_name,
					ENERGY_MON_IRQ_NAME_LENGTH))
			return energy_mon_table[i].wakeup_idx;
	}

	return -1;
}

int energy_monitor_record_wakeup_reason(int irq, char *irq_name)
{
	struct irq_desc *desc = NULL;
	int wakeup_idx = -1;

	if (irq_name) {
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: irq=N/A(%s)\n", __func__, irq_name);
		wakeup_idx = energy_monitor_find_wakeup_index(irq_name);
	} else if (irq > 0) {
		desc = irq_to_desc(irq);
		if (desc && desc->action && desc->action->name) {
			energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: irq=%d(%s)\n", __func__, irq, desc->action->name);
			wakeup_idx = energy_monitor_find_wakeup_index(desc->action->name);
		}
	} else
		energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: irq=%d\n", __func__, irq);

	if (wakeup_idx >= 0) {
		energy_mon.wakeup_cause[wakeup_idx]++;
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
		energy_mon.last_wakeup = wakeup_idx;
#endif
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: %02d/%02d/%02d/%02d/%02d/%02d\n"
		, __func__
		, energy_mon.wakeup_cause[0]
		, energy_mon.wakeup_cause[1]
		, energy_mon.wakeup_cause[2]
		, energy_mon.wakeup_cause[3]
		, energy_mon.wakeup_cause[4]
		, energy_mon.wakeup_cause[5]);

	return 0;
}
EXPORT_SYMBOL_GPL(energy_monitor_record_wakeup_reason);


/*
 * Functions for printing out
 */

static bool is_last_read_index(void)
{
	if (energy_mon.read_index - energy_mon.data_index >= ENERGY_MON_HISTORY_NUM)
		return 1;
	else
		return 0;
}

static ssize_t energy_monitor_print_time_logs(char *buf, int buf_size, int type)
{
	ssize_t ret = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	long long average;
	int need_to_show_average = 0;
	int diff_soc;
	__kernel_time_t diff_boot = 0, diff_kern, diff_disp;
	int diff_wakeup[ENERGY_MON_WAKEUP_MAX];
	long kern_percent = 0, disp_percent = 0;
	struct rtc_time tm_real;
	static char temp_buf[100];
	char score_print[10] = "N/A";
	char print_type;
	char bat_status;
	char average_c;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, type=%d\n"
		, __func__, buf_size, type);

	/* Assign proper buffer to use */
	if (type == ENERGY_MON_TYPE_BOOTING) {
		print_type = 'b';
		p_curr = &energy_mon.boot;
		p_prev = NULL;
	} else if (type == ENERGY_MON_TYPE_DUMP) {
		print_type = 'd';
		need_to_show_average = 1;
		p_curr = &energy_mon.dump;
		if (energy_mon.data_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (type == ENERGY_MON_TYPE_BATTERY || type == ENERGY_MON_TYPE_MONITOR) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

		if (p_curr->penalty_score >= 0)
			snprintf(score_print, 10, "%d", p_curr->penalty_score);

		/* If nothing is marked */
		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM && energy_mon.read_index % ENERGY_MON_HISTORY_NUM == 0) {
			p_prev = &energy_mon.boot;
			need_to_show_average = 1;
		} else if ((energy_mon.read_index - energy_mon.data_index) % ENERGY_MON_HISTORY_NUM == 0)
			p_prev = NULL;
		else {
			p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
			need_to_show_average = 1;
		}
	} else if (type == ENERGY_MON_TYPE_CHARGING) {
		print_type = 'C';
		need_to_show_average = 1;
		p_curr = &energy_mon.charging_dump;
		p_prev = NULL;
	} else if (type == ENERGY_MON_TYPE_DISCHARGING) {
		print_type = 'D';
		need_to_show_average = 1;
		p_curr = &energy_mon.discharging_dump;
		p_prev = NULL;
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	if (p_curr->bat_status == POWER_SUPPLY_STATUS_CHARGING)
		bat_status = 'C';
	else if (p_curr->bat_status == POWER_SUPPLY_STATUS_DISCHARGING)
		bat_status = 'D';
	else if (p_curr->bat_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
		bat_status = 'N';
	else if (p_curr->bat_status == POWER_SUPPLY_STATUS_FULL)
		bat_status = 'F';
	else
		bat_status = 'U';

	if (need_to_show_average) {
		if (type == ENERGY_MON_TYPE_CHARGING || type == ENERGY_MON_TYPE_DISCHARGING) {
			diff_soc = p_curr->bat_capacity;
			diff_boot = p_curr->ts_boot.tv_sec;
			diff_kern = p_curr->ts_kern.tv_sec;
			diff_disp = p_curr->ts_disp.tv_sec;
			average = abs64(energy_mon.unit_bat_capacity * p_curr->bat_capacity);
			memcpy(diff_wakeup, p_curr->wakeup_cause, sizeof(p_curr->wakeup_cause));


		} else {
			diff_soc = p_curr->bat_capacity - p_prev->bat_capacity;
			diff_boot = p_curr->ts_boot.tv_sec - p_prev->ts_boot.tv_sec;
			diff_kern = p_curr->ts_kern.tv_sec - p_prev->ts_kern.tv_sec;
			diff_disp = p_curr->ts_disp.tv_sec - p_prev->ts_disp.tv_sec;

			/* If diff_time is negative, change to zero */
			if (diff_boot < 0)
				diff_boot = 0;
			if (diff_kern < 0)
				diff_kern = 0;
			if (diff_disp < 0)
				diff_disp = 0;

			average = abs64(energy_mon.unit_bat_capacity * (p_curr->bat_capacity - p_prev->bat_capacity));
			for (i = 0; i < ENERGY_MON_WAKEUP_MAX; i++)
				diff_wakeup[i] = p_curr->wakeup_cause[i] - p_prev->wakeup_cause[i];
		}

		if (diff_soc > 0)
			average_c = '+';
		else
			average_c = ' ';

		/* To prevent Device by Zero */
		if (diff_boot) {
			kern_percent = diff_kern*100/diff_boot;
			do_div(average, diff_boot);
		}
		if (diff_kern)
			disp_percent = diff_disp*100/diff_kern;


		snprintf(temp_buf, sizeof(temp_buf), "/%6d/%5lu/%5lu/%5lu/%c%3ld.%02ldmA/%3ld.%02ld/%3ld.%02ld/%3d/%3d/%3d/%3d/%3d/%3d\n"
			, diff_soc
			, diff_boot
			, diff_kern
			, diff_disp
			, average_c
			, (long)average/100, (long)average%100
			, kern_percent, kern_percent%100
			, disp_percent, disp_percent%100
			, diff_wakeup[0], diff_wakeup[1], diff_wakeup[2], diff_wakeup[3], diff_wakeup[4], diff_wakeup[5]
		);
	} else {
		snprintf(temp_buf, sizeof(temp_buf), "\n");
	}
	rtc_time_to_tm(p_curr->ts_real.tv_sec + alarm_get_tz(), &tm_real);

	ret += snprintf(buf + ret, buf_size - ret,
		"%c/%03d/%c/%2d/%4d/%6d/%04d/%10lu.%03lu/%04d-%02d-%02d %02d:%02d:%02d/%6lu.%03lu/%6lu.%03lu/%6lu.%03lu/%5s%s"
		, print_type, p_curr->log_count
		, bat_status, p_curr->cable_type, p_curr->bat_temp
		, p_curr->bat_capacity, p_curr->suspend_count
		, p_curr->ts_real.tv_sec, p_curr->ts_real.tv_nsec/NSEC_PER_MSEC
		, tm_real.tm_year + 1900, tm_real.tm_mon + 1
		, tm_real.tm_mday, tm_real.tm_hour
		, tm_real.tm_min, tm_real.tm_sec
		, p_curr->ts_boot.tv_sec, p_curr->ts_boot.tv_nsec/NSEC_PER_MSEC
		, p_curr->ts_kern.tv_sec, p_curr->ts_kern.tv_nsec/NSEC_PER_MSEC
		, p_curr->ts_disp.tv_sec, p_curr->ts_disp.tv_nsec/NSEC_PER_MSEC
		, score_print
		, temp_buf);

	return ret;
}

static ssize_t energy_mon_summary_print(char *buf, int buf_size,
		ssize_t ret, enum energy_mon_print_type p_type)
{
	struct timespec dump_time;
	struct rtc_time tm_real;

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		/* Print out Energy monitor version */
		ret += snprintf(buf + ret, buf_size - ret,
				"energy_mon_status_raw/ver%s", ENERGY_MON_VERSION);

		/* Print out UTC and Local time */
		dump_time = current_kernel_time();
		rtc_time_to_tm(dump_time.tv_sec, &tm_real);
		ret += snprintf(buf + ret, buf_size - ret,
				"/%04d-%02d-%02d %02d:%02d:%02d(UTC)"
				, tm_real.tm_year + 1900, tm_real.tm_mon + 1
				, tm_real.tm_mday, tm_real.tm_hour
				, tm_real.tm_min, tm_real.tm_sec);
		rtc_time_to_tm(dump_time.tv_sec + alarm_get_tz(), &tm_real);
		ret += snprintf(buf + ret, buf_size - ret,
				"/%04d-%02d-%02d %02d:%02d:%02d(LOCAL)\n\n"
				, tm_real.tm_year + 1900, tm_real.tm_mon + 1
				, tm_real.tm_mday, tm_real.tm_hour
				, tm_real.tm_min, tm_real.tm_sec);

		ret += snprintf(buf + ret, buf_size - ret, "[summary]\n");
		ret += snprintf(buf + ret, buf_size - ret,
				"T/CNT/B/CT/TEMP/CAPA__/SUSP/REAL_TIME_UTC_/REAL_TIME_RTC_LOCAL/"
				"BOOT_TIME_/KERN_TIME_/DISP_TIME_/Score/dSOC__/dBOOT/dKERN/dDISP/"
				"CUR_AVER/KERN_%%/DISP_%%/INP/SSP/RTC/BT_/WIF/CP_\n");
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_BOOTING);
	} else if (p_type == ENERGY_MON_PRINT_MAIN)
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_BATTERY);
	else {
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_DUMP);
		ret += snprintf(buf + ret, buf_size - ret, "\n");
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_CHARGING);
		ret += energy_monitor_print_time_logs(buf + ret, buf_size, ENERGY_MON_TYPE_DISCHARGING);
		ret += snprintf(buf + ret, buf_size - ret, "\n");
	}

	return ret;
}

#ifdef CONFIG_SENSORHUB_STAT
static ssize_t energy_monitor_print_sensorhub_logs(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_TAIL) {
		print_type = 'd';
		p_curr = &energy_mon.dump;
		if (energy_mon.read_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_MAIN) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
		p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SENSORHUB_LIB_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/%02d", i);
		ret += snprintf(buf + ret, buf_size - ret, "[sensorhub]\n%c/idx%s\n"
			, print_type, temp_buf);
	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;

	if (p_prev) {
		for (i = 0; i < SENSORHUB_LIB_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
					"/%02d",
					p_curr->sh_wakeup.ap_wakeup_cnt[i] - p_prev->sh_wakeup.ap_wakeup_cnt[i]);
	} else {
		for (i = 0; i < SENSORHUB_LIB_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
					"/%02d",
					p_curr->sh_wakeup.ap_wakeup_cnt[i]);
	}
	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n", print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_SAP_PID_STAT
static ssize_t energy_monitor_print_sap_logs(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	static char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_TAIL) {
		print_type = 'd';
		p_curr = &energy_mon.dump;
		if (energy_mon.read_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_MAIN) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
		p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/%03d", i);
		ret += snprintf(buf + ret, buf_size - ret, "[sap]\n%c/idx%s\n"
			, print_type, temp_buf);
	}

	ret_temp = 0;

	if (p_prev) {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
							"/%03d",
							p_curr->sap_wakeup.wakeup_cnt[i] - p_prev->sap_wakeup.wakeup_cnt[i]);
	} else {
		for (i = 0; i < SAP_STAT_SAPID_MAX; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
							"/%03d",
							p_curr->sap_wakeup.wakeup_cnt[i]);
	}

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n", print_type, p_curr->log_count, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);
	return ret;
}
#endif

#ifdef CONFIG_PID_STAT
static ssize_t energy_monitor_print_pid_stat_logs(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	static char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
			, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < PID_STAT_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/___________top%d@transmit(count)", i);
		ret += snprintf(buf + ret, buf_size - ret, "[pid_stat]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;
	for (i = 0; i < PID_STAT_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/%15s@%8d(%5d)", p_curr->pid_stat_mon[i].name,
				p_curr->pid_stat_mon[i].transmit, p_curr->pid_stat_mon[i].count);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n"
			, print_type, p_curr->log_count
			, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

#ifdef CONFIG_SLAVE_WAKELOCK
static ssize_t energy_monitor_print_slave_wakelock_logs(char *buf, int buf_size,
		enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	static char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < SLWL_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/___________top%d@_sec", i);
		ret += snprintf(buf + ret, buf_size - ret, "[slave_wakelock]\n%c/idx%s\n"
			, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p\n", __func__, p_curr);

	ret_temp = 0;
	for (i = 0; i < SLWL_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/%15s@%04ld", p_curr->slwl[i].slwl_name,
			ktime_to_timeval(p_curr->slwl[i].prevent_time).tv_sec);

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n"
		, print_type, p_curr->log_count
		, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
static ssize_t energy_monitor_print_sec_therm_history(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	static char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	if (p_type == ENERGY_MON_PRINT_TAIL) {
		ret += snprintf(buf + ret, buf_size - ret, "\n");
		return ret;
	}

	print_type = '*';
	p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
					"//___%12s ___", sec_therm_dev_name[i]); /* len : 20 */
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
				"\n%c    ", print_type);
		for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
					"//RND/MIN/MAX/AVG/CNT");
		ret += snprintf(buf + ret, buf_size - ret, "[SEC_THERMISTER_HISTORY]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;
	for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
		if (p_curr->therm_history[i].reset) {
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
					"//  -/  -/  -/  -/  -");
		} else {
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf),
				"//%3d/%3d/%3d/%3d/%3d", p_curr->therm_history[i].round,
				p_curr->therm_history[i].min, p_curr->therm_history[i].max,
				p_curr->therm_history[i].sum / p_curr->therm_history[i].cnt,
				p_curr->therm_history[i].cnt);
		}
	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n"
		, print_type, p_curr->log_count
		, temp_buf);

	return ret;
}
#endif

#ifdef CONFIG_SENSORHUB_STAT // GPS
static ssize_t energy_monitor_print_gps_logs(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	int gps_time = 0;
	char print_type;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];

		if (energy_mon.data_index <= ENERGY_MON_HISTORY_NUM)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_TAIL) {
		print_type = 'd';
		p_curr = &energy_mon.dump;
		if (energy_mon.read_index == 0)
			p_prev = &energy_mon.boot;
		else
			p_prev = &energy_mon.data[(energy_mon.data_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else if (p_type == ENERGY_MON_PRINT_MAIN) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
		p_prev = &energy_mon.data[(energy_mon.read_index + ENERGY_MON_HISTORY_NUM - 1) % ENERGY_MON_HISTORY_NUM];
	} else {
		// TODO: Need to check return value
		// TODO: What shall I do if there is no valid case
		return 0;
	}

	if (p_type == ENERGY_MON_PRINT_TITLE) {
		ret += snprintf(buf + ret, buf_size - ret, "[gps]\n");
		ret += snprintf(buf + ret, buf_size - ret, "%c/idx/time/", print_type);
		ret += snprintf(buf + ret, buf_size - ret, "___last_gps_user/ext/\n");
	}

	if (p_prev)
		gps_time = p_curr->sh_gps.gps_time - p_prev->sh_gps.gps_time;
	else
		gps_time = p_curr->sh_gps.gps_time;

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d/%04d/", print_type, p_curr->log_count, gps_time);
	ret += snprintf(buf + ret, buf_size - ret, "%016llx/", p_curr->sh_gps.last_gps_user);
	ret += snprintf(buf + ret, buf_size - ret, " %02u/\n", p_curr->sh_gps.last_gps_ext);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);
	return ret;
}
#endif

#ifdef CONFIG_LBS_HISTORY
static ssize_t energy_monitor_print_lbs_history(char *buf, int buf_size,
	enum energy_mon_print_type p_type)
{
	ssize_t ret = 0, ret_temp = 0;
	struct energy_mon_data *p_curr = NULL;
	struct energy_mon_data *p_prev = NULL;
	static char temp_buf[300];
	char print_type;
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: buf_size=%d, p_type=%d\n"
		, __func__, buf_size, p_type);

	/* Set print type */
	if (p_type != ENERGY_MON_PRINT_TAIL) {
		print_type = '*';
		p_curr = &energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM];
	} else {
		print_type = 'd';
		p_curr = &energy_mon.dump;
	}

	/* Assign proper buffer to use */
	if (p_type == ENERGY_MON_PRINT_TITLE) {
		for (i = 0; i < LBS_HISTORY_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/_______gps_top%d@___msec", i);
		for (i = 0; i < LBS_HISTORY_ARRAY_SIZE; i++)
			ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/_______wps_top%d@___msec", i);
		ret += snprintf(buf + ret, buf_size - ret, "[lbs_history]\n%c/idx%s\n"
				, print_type, temp_buf);

	}
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: p_curr=%p, p_prev=%p\n", __func__, p_curr, p_prev);

	ret_temp = 0;
	for (i = 0; i < LBS_HISTORY_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/%15s@%7lld", p_curr->lbs_gps[i].name,
				ktime_to_ms(p_curr->lbs_gps[i].gps_time));
	for (i = 0; i < LBS_HISTORY_ARRAY_SIZE; i++)
		ret_temp += snprintf(temp_buf + ret_temp, sizeof(temp_buf), "/%15s@%7lld", p_curr->lbs_wps[i].name,
				ktime_to_ms(p_curr->lbs_wps[i].wps_time));

	ret += snprintf(buf + ret, buf_size - ret, "%c/%03d%s\n"
			, print_type, p_curr->log_count
			, temp_buf);

	if (p_type == ENERGY_MON_PRINT_TAIL)
		ret += snprintf(buf + ret, buf_size - ret, "\n");

	return ret;
}
#endif

static ssize_t energy_mon_print(char *buf, int buf_size, ssize_t ret,
	enum energy_mon_print_step print_step, enum energy_mon_print_type p_type)
{
	switch (print_step) {
	case STEP_SUMMARY:
		ret += energy_mon_summary_print(buf, buf_size, ret, p_type);
		break;
#ifdef CONFIG_SENSORHUB_STAT
	case STEP_SENSORHUB_STAT:
		ret += energy_monitor_print_sensorhub_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SAP_PID_STAT
	case STEP_SAP_STAT:
		ret += energy_monitor_print_sap_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_PID_STAT
	case STEP_PID_STAT:
		ret += energy_monitor_print_pid_stat_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SLAVE_WAKELOCK
	case STEP_SLAVE_WAKELOCK:
		ret += energy_monitor_print_slave_wakelock_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	case STEP_SEC_THERM_HISTORY:
		ret += energy_monitor_print_sec_therm_history(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_SENSORHUB_STAT
	case STEP_GPS_STAT:
		ret += energy_monitor_print_gps_logs(buf + ret, buf_size, p_type);
		break;
#endif
#ifdef CONFIG_LBS_HISTORY
	case STEP_LBS_HISTORY:
		ret += energy_monitor_print_lbs_history(buf + ret, buf_size, p_type);
		break;
#endif
	default:
		break;
	}

	return ret;
}

static ssize_t read_status_raw(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	static char buf[PAGE_SIZE];
	static enum energy_mon_print_step print_step;
	static int need_to_print_title;
	ssize_t ret = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s:read_index=%d, write_index=%d\n",
		__func__, energy_mon.read_index, energy_mon.data_index);

	if (*ppos == 0) {
		energy_monitor_marker(ENERGY_MON_TYPE_DUMP);
		energy_monitor_inject_data(ENERGY_MON_TYPE_DUMP, NULL, NULL);

		/* Start print step */
		print_step = STEP_SUMMARY;
		/* Initialize read index */
		energy_mon.read_index = energy_mon.data_index;
		ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_TITLE);
		need_to_print_title = 0;
	} else if (print_step != STEP_MAX) {
		 if (energy_mon.data_index == 0 || is_last_read_index()) {
			ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_TAIL);

			/* Go to next print_step */
			print_step++;
			/* Initialize read index */
			energy_mon.read_index = energy_mon.data_index;
			need_to_print_title = 1;
		} else {
			/* Skip buffer when it is not used yet */
			while (energy_mon.data[energy_mon.read_index % ENERGY_MON_HISTORY_NUM].log_count == -1)
				energy_mon.read_index++;

			if (need_to_print_title == 1) {
				ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_TITLE);
				need_to_print_title = 0;
			} else
				ret += energy_mon_print(buf, sizeof(buf), ret, print_step, ENERGY_MON_PRINT_MAIN);

			energy_mon.read_index++;
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: ret = %d\n", __func__, (int)ret);

	return ret;
}

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
/* Need to make sync with enum energy_mon_wakeup_source in the energy_monitor.h */
static char *wakeup_text[] = {
	"INPUT", "SSP", "RTC", "BT", "WIFI", "CP"
};

static ssize_t read_status_wakeup_time(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	static char buf[800];
	int i;
	int wakeup_count;
	long average_time_ms;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		ret += snprintf(buf+ret, sizeof(buf), "WAKEUP/CNT/TIME_TOTAL/TIME_AVERA\n");
		for (i = 0; i < ENERGY_MON_WAKEUP_MAX ; i++) {
			if (i == energy_mon.last_wakeup)
				wakeup_count = energy_mon.wakeup_cause[i] - 1;
			else
				wakeup_count = energy_mon.wakeup_cause[i];

			if (wakeup_count) {
				average_time_ms = energy_mon.kern_wakeup_time[i].tv_sec * 1000 + energy_mon.kern_wakeup_time[i].tv_nsec/NSEC_PER_MSEC;
				average_time_ms = average_time_ms / wakeup_count;
			} else {
				average_time_ms = 0;
			}

			ret += snprintf(buf+ret, sizeof(buf), "%6s/%3d/%6lu.%03lu/%6lu.%03lu\n",
				wakeup_text[i], wakeup_count,
				energy_mon.kern_wakeup_time[i].tv_sec, energy_mon.kern_wakeup_time[i].tv_nsec/NSEC_PER_MSEC,
				average_time_ms / 1000, average_time_ms % 1000);
		}

		for (i = 0; i < ENERGY_MON_MAX_WAKEUP_STAT_TIME ; i++)
			ret += snprintf(buf+ret, sizeof(buf), "%2d ~ %2ds : %3d\n", i, i + 1, energy_mon.wakeup_time_stats[i]);
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: ret = %d\n", __func__, (int)ret);

	if (ret > sizeof(buf))
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: buffer overflow!!! ret = %d\n", __func__, (int)ret);

	return ret;
}
#endif

#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
static ssize_t read_sleep_current(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	static char buf[800];
	int i;
	long long average_sum = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		ret += snprintf(buf+ret, sizeof(buf), "CNT/AVERAGE_CUR\n");
		for (i = 0; i < energy_mon.estimator_index; i++) {
			ret += snprintf(buf+ret, sizeof(buf), "%3d/%3ld.%02ldmA\n",
				i, (long)energy_mon.estimator_average[i]/100, (long)energy_mon.estimator_average[i]%100);
			average_sum += energy_mon.estimator_average[i];
		}
		if (energy_mon.estimator_index != 0) {
			do_div(average_sum, energy_mon.estimator_index);
			ret += snprintf(buf+ret, sizeof(buf), "avg/%3ld.%02ldmA\n", (long)average_sum/100, (long)average_sum%100);
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: ret = %d\n", __func__, (int)ret);

	if (ret > sizeof(buf))
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: buffer overflow!!! ret = %d\n", __func__, (int)ret);

	return ret;
}
#endif /* CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR */

static ssize_t read_monitor_interval(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[10];

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0)
		ret += snprintf(buf, sizeof(buf), "%u\n", monitor_interval);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	return ret;
}

static ssize_t write_monitor_interval(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	unsigned int new_interval = 0;
	int ret = 0;

	ret = kstrtouint(user_buf, 0, &new_interval);
	if (ret < 0)
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: kstrtouint is failed\n", __func__);

	/* Maximum interval is 1 day */
	if (new_interval > ENERGY_MONITOR_MAX_MONITOR_INTERVAL)
		new_interval = ENERGY_MONITOR_MAX_MONITOR_INTERVAL;

	monitor_interval = new_interval;

	if (monitor_interval > 0) {
		schedule_delayed_work(&monitor_work, monitor_interval * HZ);
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: monitor thread is started\n", __func__);
	} else if (monitor_interval == 0) {
		cancel_delayed_work(&monitor_work);
		energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s: monior thread is canceled\n", __func__);
	}

	return count;
}

static ssize_t read_logging_interval(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[10];

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0)
		ret += snprintf(buf, sizeof(buf), "%u\n", logging_interval);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret))
			return -EFAULT;
		*ppos += ret;
	}

	return ret;
}

static ssize_t write_logging_interval(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	unsigned int new_interval = 0;
	int ret = 0;

	ret = kstrtouint(user_buf, 0, &new_interval);
	if (ret < 0)
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s: kstrtouint is failed\n", __func__);

	logging_interval = new_interval;

	return count;
}

static const struct file_operations status_raw_fops = {
	.read = read_status_raw,
};

static const struct file_operations status_raw2_fops = {
	.read = read_status_raw,
};

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
static const struct file_operations status_wakeup_time_fops = {
	.read = read_status_wakeup_time,
};
#endif /* CONFIG_ENERGY_MONITOR_WAKEUP_STAT */

#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
static const struct file_operations sleep_current_fops = {
	.read = read_sleep_current,
};
#endif /* CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR */

static const struct file_operations monitor_interval_fops = {
	.write		= write_monitor_interval,
	.read		= read_monitor_interval,
};

static const struct file_operations logging_interval_fops = {
	.write		= write_logging_interval,
	.read		= read_logging_interval,
};

#ifdef CONFIG_SEC_SYSFS
static ssize_t energy_monitor_get_last_discharging_info(struct device *dev, struct device_attribute *attr,
		char *buf) {
	static struct energy_mon_data last_discharging_info;
	static bool is_first = true;
	int diff_soc;
	__kernel_time_t diff_boot = 0;

	energy_monitor_marker(ENERGY_MON_TYPE_DUMP);
	energy_monitor_inject_data(ENERGY_MON_TYPE_DUMP, NULL, NULL);

	if (is_first) {
		diff_soc = energy_mon.discharging_dump.bat_capacity * -1;
		diff_boot = energy_mon.discharging_dump.ts_boot.tv_sec;
		is_first = false;
	} else {
		diff_soc = (energy_mon.discharging_dump.bat_capacity - last_discharging_info.bat_capacity) * -1;
		diff_boot = energy_mon.discharging_dump.ts_boot.tv_sec - last_discharging_info.ts_boot.tv_sec;
	}

	if (diff_soc < 0)
		diff_soc = 0;
	if (diff_boot < 0)
		diff_boot = 0;

	memcpy(&last_discharging_info, &energy_mon.discharging_dump, sizeof(struct energy_mon_data));

	return scnprintf(buf, PAGE_SIZE, "dSOC:%d dBOOT:%lu\n", diff_soc, diff_boot);
}
static DEVICE_ATTR(get_last_discharging_info, 0440, energy_monitor_get_last_discharging_info, NULL);

static ssize_t energy_monitor_get_last_penalty_score(struct device *dev, struct device_attribute *attr,
		char *buf) {
	ssize_t ret = 0;

	ret += scnprintf(buf, PAGE_SIZE, "%d\n", energy_mon.penalty_score);
	energy_mon.penalty_score = 0;

	return ret;
}
static DEVICE_ATTR(last_penalty_score, 0440, energy_monitor_get_last_penalty_score, NULL);

static struct attribute *sec_energy_monitor_attrs[] = {
	&dev_attr_get_last_discharging_info.attr,
	&dev_attr_last_penalty_score.attr,
	NULL
};
static const struct attribute_group sec_energy_monitor_attr_group = {
	.attrs = sec_energy_monitor_attrs,
};
#endif

static int energy_monitor_debug_init(void)
{
	struct dentry *d;
#ifdef CONFIG_SEC_SYSFS
	int err;
#endif

	d = debugfs_create_dir("energy_monitor", NULL);
	if (d) {
		if (!debugfs_create_file("status_raw", 0600, d, NULL, &status_raw_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s : debugfs_create_file, error\n", "status_raw");
		if (!debugfs_create_file("status_raw2", 0600, d, NULL, &status_raw2_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s : debugfs_create_file, error\n", "status_raw2");
#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
		if (!debugfs_create_file("status_wakeup_time", 0600, d, NULL, &status_wakeup_time_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s : debugfs_create_file, error\n", "status_wakeup_time");
#endif
#ifdef CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR
		if (!debugfs_create_file("sleep_current", 0600, d, NULL, &sleep_current_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "%s : debugfs_create_file, error\n", "sleep_current");
#endif
		if (!debugfs_create_file("monitor_interval", 0600, d, NULL, &monitor_interval_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "monitor_interval");
		if (!debugfs_create_file("logging_interval", 0600, d, NULL, &logging_interval_fops))
			energy_mon_dbg(ENERGY_MON_DEBUG_ERR,
				"%s : debugfs_create_file, error\n", "monitor_interval");

		debugfs_create_u32("debug_level", 0600, d, &debug_level);
		debugfs_create_u32("enable", 0600, d, &energy_monitor_enable);
	}

#ifdef CONFIG_SEC_SYSFS
	sec_energy_monitor = sec_device_create(NULL, "energy_monitor");
	if (IS_ERR(sec_energy_monitor)) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "sec_energy_monitor create fail\n");
		return -ENODEV;
	}

	err = sysfs_create_group(&sec_energy_monitor->kobj, &sec_energy_monitor_attr_group);
	if (err < 0) {
		energy_mon_dbg(ENERGY_MON_DEBUG_ERR, "sec_energy_monitor_attr create fail\n");
		return err;
	}
#endif

	return 0;
}

#if defined(CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR)
int energy_monitor_sleep_current_estimator(unsigned long event)
{
	struct power_supply *psy_battery = NULL;
	union power_supply_propval value;
	int err = -1;

	static struct timespec ts_susp;
	static int raw_soc_susp;
	static int valid_suspend;

	struct timespec ts_resume;
	struct timespec ts_elapsed;
	static int valid_count;
	long long average;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		valid_suspend = 0;
		psy_battery = power_supply_get_by_name("battery");
		if (psy_battery)
			err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_STATUS, &value);
		if (err < 0)
			value.intval = -1;

		/* Check only in discharging */
		if (value.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
#ifdef CONFIG_ENERGY_MONITOR_USE_RAW_SOC
			value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
			psy_do_property("sec-fuelgauge", get, POWER_SUPPLY_PROP_CAPACITY, value);
			raw_soc_susp = value.intval;
#else
			err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_CAPACITY, &value);
			if (err < 0)
				value.intval = -1;
			raw_soc_susp = value.intval * 100;
#endif
			ts_susp = current_kernel_time();
			valid_suspend = 1;
		}
		break;

	case PM_POST_SUSPEND:
#ifdef CONFIG_ENERGY_MONITOR_USE_RAW_SOC
		value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
		psy_do_property("sec-fuelgauge", get, POWER_SUPPLY_PROP_CAPACITY, value);
#else
		err = psy_battery->get_property(psy_battery, POWER_SUPPLY_PROP_CAPACITY, &value);
		if (err < 0)
			value.intval = -1;
#endif

		ts_resume = current_kernel_time();

		/* Calculate elapsed time and power consumption */
		ts_elapsed = timespec_sub(ts_resume, ts_susp);

		if (valid_suspend && ts_elapsed.tv_sec > MIN_SLEEP_TIME_S && raw_soc_susp - value.intval > 0) {
			average = (raw_soc_susp - value.intval) * UNIT_BATTERY_mAs;

			valid_count++;
			do_div(average, ts_elapsed.tv_sec);

			/* Save only first 20 results */
			if (energy_mon.estimator_index < ENERGY_MON_MAX_SLEEP_ESTIMATOR_CNT)
				energy_mon.estimator_average[energy_mon.estimator_index++] = average;

			energy_mon_dbg(ENERGY_MON_DEBUG_SLEEP_ESTIMATOR, "%s: %3ld.%02ldmA from %u to %u for %lus\n",
				__func__, (long)average/100, (long)average%100, raw_soc_susp, value.intval, ts_elapsed.tv_sec);
		}
		break;
	}
	return 0;
}
#endif /* CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR */

static int energy_monitor_pm_notifier(struct notifier_block *nb,
				unsigned long event, void *unused)
{

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: event=%lu\n", __func__, event);

	/* If monitor interval is 0, do nothing */
	if (monitor_interval) {
		switch (event) {
		case PM_SUSPEND_PREPARE:
			energy_monitor_background_marker();
			cancel_delayed_work(&monitor_work);
			break;

		case PM_POST_SUSPEND:
			schedule_delayed_work(&monitor_work, monitor_interval * HZ);
			break;
		}
	}

#if defined(CONFIG_ENERGY_MONITOR_SLEEP_CURRENT_ESTIMATOR)
	energy_monitor_sleep_current_estimator(event);
#endif

#ifdef CONFIG_ENERGY_MONITOR_WAKEUP_STAT
	{
		struct timespec diff_time = {0,};

		switch (event) {
		case PM_SUSPEND_PREPARE:
			if (energy_mon.last_wakeup < ENERGY_MON_WAKEUP_MAX && energy_mon.last_wakeup >= 0) {
				diff_time = timespec_sub(ktime_to_timespec(ktime_get()), energy_mon.kern_last_wakeup);

				energy_mon.kern_wakeup_time[energy_mon.last_wakeup] =
					timespec_add(energy_mon.kern_wakeup_time[energy_mon.last_wakeup]
					, diff_time);

				energy_mon_dbg(ENERGY_MON_DEBUG_WAKEUP_STAT, "%s: wake up by %s for %6lu.%03lu (%d)\n"
					, "WAKEUP_STAT"
					, wakeup_text[energy_mon.last_wakeup]
					, diff_time.tv_sec, diff_time.tv_nsec / NSEC_PER_MSEC
					, suspend_stats.success);

				// TODO: Need to find and fix whether wakeup is caused by wrist up if possible
				if (energy_mon.last_wakeup == ENERGY_MON_WAKEUP_SSP && diff_time.tv_sec >= 0) {
					if (diff_time.tv_sec > ENERGY_MON_MAX_WAKEUP_STAT_TIME - 1)
						diff_time.tv_sec = ENERGY_MON_MAX_WAKEUP_STAT_TIME - 1;
					energy_mon.wakeup_time_stats[(unsigned int)diff_time.tv_sec]++;

					energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: sec=%06u\n",
						__func__, (unsigned int)diff_time.tv_sec);
				}

			}
			energy_mon.last_wakeup = -1;
			break;

		case PM_POST_SUSPEND:
			energy_mon.kern_last_wakeup = ktime_to_timespec(ktime_get());
			break;
		}
	}
#endif

	return NOTIFY_DONE;
}

static struct notifier_block energy_monitor_notifier_block = {
	.notifier_call = energy_monitor_pm_notifier,
	.priority = 0,
};

static void energy_monitor_work(struct work_struct *work)
{
	static int thread_cnt;

	energy_mon_dbg(ENERGY_MON_DEBUG_DBG, "%s: (count=%d)\n", __func__, thread_cnt++);

	/* Call background marker when logging interval is setted */
	if (logging_interval)
		energy_monitor_background_marker();

	if (monitor_interval)
		schedule_delayed_work(&monitor_work, monitor_interval * HZ);
	else
		cancel_delayed_work(&monitor_work);
}

static int energy_monitor_init(void)
{
	int i;

	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s\n", __func__);

	/* Initialize datas */
	for (i = 0; i < ENERGY_MON_HISTORY_NUM ; i++)
		energy_mon.data[i].log_count = -1;
	energy_mon.unit_bat_capacity = UNIT_BATTERY_mAs;
	energy_mon.last_wakeup = -1;

	/* Check size of control block for information */
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: sizeof(struct energy_mon_data) is %lu\n",
		__func__, sizeof(struct energy_mon_data));
	energy_mon_dbg(ENERGY_MON_DEBUG_DBG,
		"%s: sizeof(energy_mon) is %lu(ENERGY_MON_HISTORY_NUM=%d)\n",
		__func__, sizeof(energy_mon), ENERGY_MON_HISTORY_NUM);

	energy_monitor_debug_init();

	register_pm_notifier(&energy_monitor_notifier_block);

	INIT_DELAYED_WORK(&monitor_work, energy_monitor_work);
	if (monitor_interval)
		schedule_delayed_work(&monitor_work, monitor_interval * HZ);

	return 0;
}

static void energy_monitor_exit(void)
{
	energy_mon_dbg(ENERGY_MON_DEBUG_INFO, "%s\n", __func__);
}

module_init(energy_monitor_init);
module_exit(energy_monitor_exit);
