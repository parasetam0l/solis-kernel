/* s6e36w1x01_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * SeungBeom, Park <sb1.parki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E36W1X01_MIPI_LCD_H__
#define __S6E36W1X01_MIPI_LCD_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <video/mipi_display.h>
#include <linux/platform_device.h>
#include "mdnie_lite.h"

enum {
	AO_NODE_OFF = 0,
	AO_NODE_ALPM = 1,
	AO_NODE_SELF = 2,
};

enum {
	TEMP_RANGE_0 = 0,	/* 0 < temperature*/
	TEMP_RANGE_1,		/*-10 < temperature < =0*/
	TEMP_RANGE_2,		/*-20<temperature <= -10*/
	TEMP_RANGE_3,		/*temperature <= -20*/
};

struct s6e36w1x01 {
	struct device		*dev;
	struct lcd_device		*ld;
	struct backlight_device	*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct mdnie_lite_device	*mdnie;
	struct lcd_platform_data	*pd;
	struct lcd_property	*property;
	struct smart_dimming	*dimming;
	struct regulator		*vdd3;
	struct regulator		*vci;
	struct work_struct		det_work;
	struct mutex		lock;
	struct class 		*esd_class;
	struct device		*esd_dev;
	unsigned char		*br_map;
	unsigned int		reset_gpio;
	unsigned int		te_gpio;
	unsigned int		det_gpio;
	unsigned int		err_gpio;
	unsigned int		esd_irq;
	unsigned int		err_irq;
	unsigned int		power;
	unsigned int		acl;
	unsigned int		refresh;
	unsigned char		default_hbm;
	unsigned char		hbm_gamma[GAMMA_CMD_CNT];
	unsigned char		hbm_elvss[HBM_ELVSS_CMD_CNT];
	unsigned char		*gamma_tbl[MAX_GAMMA_CNT];
	unsigned char		chip[LDI_CHIP_LEN];
	unsigned int		dbg_cnt;
	unsigned int		ao_mode;
	unsigned int		temp_stage;
	bool			hbm_on;
	bool			alpm_on;
	bool			hlpm_on;
	bool			lp_mode;
	bool			boot_power_on;
	bool			br_ctl;
	bool			scm_on;
	bool			self_mode;
	bool			mcd_on;
	bool			irq_on;
};
#endif
