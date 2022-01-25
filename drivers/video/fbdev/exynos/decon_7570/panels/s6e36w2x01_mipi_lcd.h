/* s6e36w2x01_mipi_lcd.c
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

#ifndef __S6E36W2X01_MIPI_LCD_H__
#define __S6E36W2X01_MIPI_LCD_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>

#include <video/mipi_display.h>
#include <linux/platform_device.h>
#include "mdnie_lite.h"

#define MAX_BR_INFO				74

#define 	SC_MIN_POSITION	1
#define 	SC_MAX_POSITION	360
#define 	SC_DEFAULT_POSITION	180
#define 	SC_DEFAULT_DATA	0xff
#define 	SC_MSEC_DIVIDER	100
#define 	SC_MAX_HOUR		12
#define 	SC_MAX_MSEC		10
#define 	SC_MIN_UPDATE_RATE	1
#define 	SC_MAX_UPDATE_RATE	10
#define 	SC_PANEL_VSYNC_RATE	30
#define 	SC_NEEDLE_HIGHT	40
#define 	SC_NEEDLE_WIDTH	720
#define 	SC_NEEDLE_SECTION	3
#define 	SC_RAM_WIDTH		721
#define 	SC_RAM_HIGHT		SC_NEEDLE_HIGHT*SC_NEEDLE_SECTION
#define	SC_START_CMD		0x4c
#define	SC_IMAGE_CMD		0x5c
#define	SC_CFG_REG		0xE9
#define	SC_TIME_REG		0xEC
#define	SC_DISABLE_DELAY	40 //It need a one frame delay time in HLPM
#define	SC_TIME_REG_SIZE	5
#define	SC_COMP_SPEED		30
#define	SC_1FRAME_USEC	33000 /* 33 ms */
#define	SC_COMP_NEED_TIME	30 /* 3 seconds */

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

enum {
	HLPM_NIT_OFF,
	HLPM_NIT_LOW,
	HLPM_NIT_HIGH,
};

enum {
	SC_ON_SEL,
	SB_ON_SEL,
	SC_ANA_CLOCK_EN,
	SB_BLK_EN,
	SC_HBP,
	SC_VBP,
	SC_BG_COLOR,
	SC_AA_WIDTH,
	SC_UPDATE_RATE,
	SC_TIME_UPDATE,
	SC_COMP_EN,
	SC_INC_STEP,
	SC_LINE_COLOR,
	SC_RADIUS,
	SC_CLS_CIR_ON,
	SC_OPN_CIR_ON,
	SC_RGB_INV,
	SC_LINE_WIDTH,
	SC_SET_HH,
	SC_SET_MM,
	SC_SET_SS,
	SC_SET_MSS,
	SC_CENTER_X,
	SC_CENTER_Y,
	SC_HH_CENTER_X,
	SC_HH_CENTER_Y,
	SC_HH_RGB_MASK,
	SC_MM_CENTER_X,
	SC_MM_CENTER_Y,
	SC_MM_RGB_MASK,
	SC_SS_CENTER_X,
	SC_SS_CENTER_Y,
	SC_SS_RGB_MASK,
	SB_DE,
	SB_RATE,
	SB_RADIUS,
	SB_LINE_COLOR_R,
	SB_LINE_COLOR_G,
	SB_LINE_COLOR_B,
	SB_LINE_WIDTH,
	SB_AA_ON,
	SB_AA_WIDTH,
	SB_CIRCLE_1_X,
	SB_CIRCLE_1_Y,
	SB_CIRCLE_2_X,
	SB_CIRCLE_2_Y,
};

enum {
	SA_ON_SEL,
	SM_ON_SEL,
	SA_RAND_EN,
	SA_BRT_EN,
	SM_CIR_EN,
	SA_BRIGHT_MIN,
	SA_BRIGHT_OP,
	SA_BRIGHT_STEP,
	SA_MOVE_STEP,
	SA_PAT_FIX,
	SA_RANDOM_WIDTH,
	SA_WAIT_STEP,
	SM_AA_WIDTH,
	SM_RADIUS,
	SM_OUT_COLOR,
	SM_CENTER_X,
	SM_CENTER_Y,
};

struct sc_time_st {
	unsigned int		sc_h;
	unsigned int		sc_m;
	unsigned int		sc_s;
	unsigned int		sc_ms;
	unsigned int		diff;
};

struct s6e36w2x01 {
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
	struct mutex		mipi_lock;
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
#ifdef CONFIG_COPR_SUPPORT
	unsigned int			copr;
#endif
	bool			hbm_on;
	bool			alpm_on;
	bool			hlpm_on;
	bool			lp_mode;
	bool			boot_power_on;
	bool			br_ctl;
	bool			scm_on;
	int			aod_mode;
	bool			aod_enable;
	bool			panel_sc_state;
	int				hlpm_nit;
	bool			mcd_on;
	bool			irq_on;
	char			*sc_buf;
	char			*sc_dft_buf;
	unsigned int		sc_rate;
#ifdef CONFIG_SLEEP_MONITOR
	unsigned int		act_cnt;
#endif
	struct sc_time_st		sc_time;
};
#endif
