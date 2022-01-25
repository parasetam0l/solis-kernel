/*
 * include/linux/input/hall_sensor.h
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Joong-Mock Shin <jmock.shin@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef HALL_SENSOR_H
#define HALL_SENSOR_H

#define HALL_NAME	"tizen_detent"

struct hall_sensor_platform_data {
	int gpio_a;
	int gpio_b;
	int gpio_c;
	const char *ldo_name;
};


struct hall_sensor_driverdata {
	struct input_dev *input_dev;
	struct device *rotary_dev;
	struct hall_sensor_platform_data *pdata;
	struct regulator *power;
	struct mutex hall_lock;
	int gpio_a;
	int gpio_b;
	int gpio_c;
	int hall_a_irq;
	int hall_b_irq;
	int hall_c_irq;
	bool a_status;
	bool b_status;
	bool c_status;
	bool open_state;
	bool resume_state;
	int last_status;
	int last_value;
	bool probe_done;
	bool factory_mode;
	struct delayed_work detect_dwork;
	struct platform_device *dev;
#ifdef CONFIG_SLEEP_MONITOR
	u32 event_cnt;
#endif
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*gpio_config;
};
#endif
