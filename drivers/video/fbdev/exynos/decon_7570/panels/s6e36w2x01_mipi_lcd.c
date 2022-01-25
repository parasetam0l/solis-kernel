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

#include "../dsim.h"
#include "s6e36w2x01_dimming.h"
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "s6e36w2x01_param.h"
#include "s6e36w2x01_mipi_lcd.h"
#include <video/mipi_display.h>

#include <linux/time.h>
#include <linux/rtc.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define	BACKLIGHT_DEV_NAME	"s6e36w2x01-bl"
#define	LCD_DEV_NAME		"s6e36w2x01"
#define	ID		0

static int panel_id;

static struct class *esd_class;
static struct device *esd_dev;

#ifdef CONFIG_EXYNOS_HLPM_TEST_SUPPORT
static int hlpm_mode = 0;
static int normal_dc_mode = 0;
#endif
unsigned char gamma_set[MAX_BR_INFO][GAMMA_CMD_CNT];

static signed char a3_da_rtbl10nit[10] = {0, 33, 32, 27, 23, 18, 11, 6, 2, 0};
static signed char a3_da_rtbl11nit[10] = {0, 31, 30, 25, 21, 17, 10, 6, 2, 0};
static signed char a3_da_rtbl12nit[10] = {0, 29, 28, 23, 19, 16, 9, 5, 1, 0};
static signed char a3_da_rtbl13nit[10] = {0, 29, 26, 21, 17, 14, 8, 5, 1, 0};
static signed char a3_da_rtbl14nit[10] = {0, 29, 26, 20, 16, 13, 7, 4, 1, 0};
static signed char a3_da_rtbl15nit[10] = {0, 26, 25, 20, 16, 13, 7, 4, 0, 0};
static signed char a3_da_rtbl16nit[10] = {0, 26, 25, 20, 16, 13, 7, 4, 0, 0};
static signed char a3_da_rtbl17nit[10] = {0, 26, 25, 20, 15, 12, 7, 4, 0, 0};
static signed char a3_da_rtbl19nit[10] = {0, 23, 22, 17, 13, 11, 6, 4, 1, 0};
static signed char a3_da_rtbl20nit[10] = {0, 22, 21, 16, 12, 10, 5, 3, 0, 0};
static signed char a3_da_rtbl21nit[10] = {0, 22, 21, 16, 12, 10, 5, 4, 1, 0};
static signed char a3_da_rtbl22nit[10] = {0, 21, 20, 15, 11, 9, 5, 4, 0, 0};
static signed char a3_da_rtbl24nit[10] = {0, 20, 18, 14, 11, 9, 4, 3, 0, 0};
static signed char a3_da_rtbl25nit[10] = {0, 18, 17, 14, 11, 8, 4, 2, 0, 0};
static signed char a3_da_rtbl27nit[10] = {0, 19, 18, 13, 10, 8, 5, 4, 0, 0};
static signed char a3_da_rtbl29nit[10] = {0, 17, 16, 12, 9, 7, 3, 2, 0, 0};
static signed char a3_da_rtbl30nit[10] = {0, 17, 15, 11, 8, 7, 4, 4, 0, 0};
static signed char a3_da_rtbl32nit[10] = {0, 16, 15, 10, 8, 7, 4, 4, 0, 0};
static signed char a3_da_rtbl34nit[10] = {0, 15, 14, 10, 7, 6, 3, 3, 0, 0};
static signed char a3_da_rtbl37nit[10] = {0, 14, 13, 9, 7, 6, 3, 3, 3, 0};
static signed char a3_da_rtbl39nit[10] = {0, 14, 11, 8, 5, 4, 2, 2, 2, 0};
static signed char a3_da_rtbl41nit[10] = {0, 13, 12, 7, 4, 4, 2, 2, 0, 0};
static signed char a3_da_rtbl44nit[10] = {0, 12, 11, 8, 5, 4, 2, 3, 0, 0};
static signed char a3_da_rtbl47nit[10] = {0, 11, 10, 6, 4, 4, 2, 3, 0, 0};
static signed char a3_da_rtbl50nit[10] = {0, 11, 9, 6, 4, 4, 2, 2, 0, 0};
static signed char a3_da_rtbl53nit[10] = {0, 11, 8, 5, 3, 3, 1, 2, 0, 0};
static signed char a3_da_rtbl56nit[10] = {0, 10, 9, 6, 4, 3, 1, 4, 0, 0};
static signed char a3_da_rtbl60nit[10] = {0, 10, 8, 5, 3, 2, 1, 2, 0, 0};
static signed char a3_da_rtbl64nit[10] = {0, 8, 7, 4, 3, 2, 1, 0, 0, 0};
static signed char a3_da_rtbl68nit[10] = {0, 7, 6, 4, 3, 2, 1, 2, 0, 0};
static signed char a3_da_rtbl72nit[10] = {0, 7, 6, 3, 3, 2, 1, 0, 1, 0};
static signed char a3_da_rtbl77nit[10] = {0, 7, 6, 3, 2, 2, 1, 0, 0, 0};
static signed char a3_da_rtbl82nit[10] = {0, 7, 6, 4, 2, 3, 2, 0, 1, 0};
static signed char a3_da_rtbl87nit[10] = {0, 7, 6, 3, 2, 3, 2, 3, 0, 0};
static signed char a3_da_rtbl93nit[10] = {0, 7, 6, 3, 3, 2, 2, 3, 1, 0};
static signed char a3_da_rtbl98nit[10] = {0, 8, 7, 4, 2, 2, 2, 1, 0, 0};
static signed char a3_da_rtbl105nit[10] = {0, 7, 5, 2, 1, 1, 0, 1, 1, 0};
static signed char a3_da_rtbl111nit[10] = {0, 8, 6, 4, 3, 3, 2, 3, 2, 0};
static signed char a3_da_rtbl119nit[10] = {0, 7, 5, 3, 1, 2, 1, 0, 0, 0};
static signed char a3_da_rtbl126nit[10] = {0, 6, 5, 2, 2, 2, 1, 0, 1, 0};
static signed char a3_da_rtbl134nit[10] = {0, 6, 6, 3, 2, 2, 2, 2, 2, 0};
static signed char a3_da_rtbl143nit[10] = {0, 5, 5, 3, 2, 1, 0, 2, 3, 0};
static signed char a3_da_rtbl152nit[10] = {0, 5, 4, 2, 1, 2, 1, 3, 2, 0};
static signed char a3_da_rtbl162nit[10] = {0, 5, 4, 2, 1, 1, 2, 3, 1, 0};
static signed char a3_da_rtbl172nit[10] = {0, 5, 3, 1, 0, 0, 1, 2, 1, 0};
static signed char a3_da_rtbl183nit[10] = {0, 4, 3, 2, 1, 1, 1, 4, 1, 0};
static signed char a3_da_rtbl195nit[10] = {0, 3, 2, 1, 0, 0, 1, 0, 3, 0};
static signed char a3_da_rtbl207nit[10] = {0, 3, 2, 1, 0, 0, 1, 3, 0, 0};
static signed char a3_da_rtbl220nit[10] = {0, 3, 2, 1, 0, 0, 1, 2, 1, 0};
static signed char a3_da_rtbl234nit[10] = {0, 3, 2, 1, 1, 0, 1, 3, 1, 0};
static signed char a3_da_rtbl249nit[10] = {0, 3, 2, 1, 0, 0, 1, 1, 1, 0};
static signed char a3_da_rtbl265nit[10] = {0, 2, 1, 0, 0, 0, 1, 0, 0, 0};
static signed char a3_da_rtbl282nit[10] = {0, 2, 1, 1, 0, 0, 1, 0, 0, 0};
static signed char a3_da_rtbl300nit[10] = {0, 3, 2, 0, 0, 0, 0, 2, 2, 0};
static signed char a3_da_rtbl316nit[10] = {0, 2, 1, 0, 0, 0, 0, 2, 3, 0};
static signed char a3_da_rtbl333nit[10] = {0, 2, 1, 0, 1, 0, 0, 2, 0, 0};
static signed char a3_da_rtbl350nit[10] = {0, 1, 0, 0, 0, 0, 0, 4, 1, 0};
static signed char a3_da_rtbl357nit[10] = {0, 2, 1, 1, 1, 1, 2, 4, 0, 0};
static signed char a3_da_rtbl365nit[10] = {0, 3, 2, 1, 1, 1, 2, 5, 0, 0};
static signed char a3_da_rtbl372nit[10] = {0, 1, 0, 0, 0, 0, 1, 3, 0, 0};
static signed char a3_da_rtbl380nit[10] = {0, 1, 0, 0, 0, 0, 1, 0, 0, 0};
static signed char a3_da_rtbl387nit[10] = {0, 2, 1, 1, 1, 1, 2, 4, 0, 0};
static signed char a3_da_rtbl395nit[10] = {0, 2, 1, 1, 0, 1, 1, 1, 1, 0};
static signed char a3_da_rtbl403nit[10] = {0, 2, 1, 0, 0, 0, 1, 0, 0, 0};
static signed char a3_da_rtbl412nit[10] = {0, 2, 1, 0, 0, 0, 1, 0, 0, 0};
static signed char a3_da_rtbl420nit[10] = {0, 2, 1, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl443nit[10] = {0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl465nit[10] = {0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
static signed char a3_da_rtbl488nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl510nit[10] = {0, 0, 0, 0, 0, 1, 1, 1, 0, 0};
static signed char a3_da_rtbl533nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl555nit[10] = {0, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl578nit[10] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl600nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char a3_da_ctbl10nit[30] = {0, 0, 0, -8, 1, -3, -12, -1, -19, -9, 1, -13, -15, 0, -14, -12, 2, -12, -10, 0, -8, 0, 3, 2, -2, 0, -1, -1, 0, -3};
static signed char a3_da_ctbl11nit[30] = {0, 0, 0, -6, -1, -10, -10, -1, -19, -9, 1, -12, -14, 1, -11, -14, 0, -15, -9, 0, -8, -2, 0, -4, -2, 0, -1, 1, 1, -1};
static signed char a3_da_ctbl12nit[30] = {0, 0, 0, 0, 0, -5, -10, -1, -19, -7, 1, -12, -12, 0, -11, -14, 0, -14, -8, 0, -8, -2, 0, -2, -1, 0, 0, 1, 1, -1};
static signed char a3_da_ctbl13nit[30] = {0, 0, 0, -3, 0, -5, -10, -1, -19, -7, 1, -11, -12, 1, -12, -12, 1, -13, -7, 0, -6, -2, 0, -3, 0, 0, 1, 0, 0, -2};
static signed char a3_da_ctbl14nit[30] = {0, 0, 0, -9, 0, -8, -17, -1, -23, -10, 1, -14, -11, 0, -11, -13, 1, -13, -6, 1, -5, -1, 1, -3, 0, 0, 0, 0, 0, -1};
static signed char a3_da_ctbl15nit[30] = {0, 0, 0, -6, 1, -3, -13, 1, -22, -9, 1, -15, -12, 0, -12, -14, 0, -13, -7, 0, -5, -1, 0, -3, 0, 0, 0, 0, 0, -1};
static signed char a3_da_ctbl16nit[30] = {0, 0, 0, -11, 1, -8, -14, 1, -20, -9, 1, -14, -13, -1, -12, -14, -1, -14, -7, 0, -5, -1, 0, -3, 0, 0, 1, 1, 1, -1};
static signed char a3_da_ctbl17nit[30] = {0, 0, 0, -7, 0, -7, -14, 0, -22, -14, -1, -17, -12, -1, -11, -14, 1, -13, -6, 0, -5, -2, 0, -3, 0, 0, 1, 1, 0, -1};
static signed char a3_da_ctbl19nit[30] = {0, 0, 0, -5, 1, -5, -11, -3, -21, -12, 1, -13, -10, 3, -14, -12, -1, -11, -6, 0, -6, -1, 0, -2, 1, 0, 1, 0, 0, -1};
static signed char a3_da_ctbl20nit[30] = {0, 0, 0, -15, 1, -4, -16, -1, -24, -11, 0, -14, -8, 1, -7, -12, 0, -13, -6, 1, -5, 0, 0, -1, 0, 0, 0, 1, 0, 0};
static signed char a3_da_ctbl21nit[30] = {0, 0, 0, -2, 0, -5, -14, -1, -23, -12, 0, -16, -8, 1, -7, -12, -1, -13, -5, 1, -4, 0, 0, -1, 0, 0, 0, 0, -1, -1};
static signed char a3_da_ctbl22nit[30] = {0, 0, 0, -4, 0, -8, -13, -3, -16, -14, -1, -16, -10, 1, -16, -8, 1, -7, -4, 1, -4, -1, -1, -2, 0, 0, 0, 1, 0, 0};
static signed char a3_da_ctbl24nit[30] = {0, 0, 0, -4, -1, -8, -13, 1, -21, -9, 1, -13, -8, -1, -8, -12, 0, -12, -5, 0, -4, -1, 0, -1, 1, 0, 0, 1, 0, 0};
static signed char a3_da_ctbl25nit[30] = {0, 0, 0, 0, 2, -6, -8, 3, -17, -10, 0, -13, -12, -1, -10, -10, 1, -10, -6, 0, -5, 1, 0, 0, 1, 0, 1, 1, 0, 0};
static signed char a3_da_ctbl27nit[30] = {0, 0, 0, 0, 0, -5, -17, 2, -25, -10, 0, -14, -10, 1, -10, -8, 2, -8, -5, 0, -3, 0, 0, -1, -2, -3, -2, 1, 0, 0};
static signed char a3_da_ctbl29nit[30] = {0, 0, 0, -5, 2, -6, -11, 2, -20, -12, -1, -14, -11, 0, -11, -9, 0, -9, -5, 1, -4, 2, 1, 1, 1, -1, 1, 1, 0, 0};
static signed char a3_da_ctbl30nit[30] = {0, 0, 0, -7, 0, -9, -8, 1, -17, -13, 2, -16, -8, 2, -11, -6, 1, -5, -3, 0, -3, -3, -2, -4, 1, 0, 1, 1, 0, 0};
static signed char a3_da_ctbl32nit[30] = {0, 0, 0, -1, 1, -6, -16, 0, -24, -5, 3, -9, -7, 2, -6, -8, 0, -7, -3, 0, -3, -1, 0, -1, -1, -1, -1, 1, -1, 0};
static signed char a3_da_ctbl34nit[30] = {0, 0, 0, 0, 1, -6, -13, 1, -21, -9, 2, -12, -7, 1, -7, -7, 0, -6, -3, 0, -3, 0, 0, -1, -1, -2, -1, 1, 0, 0};
static signed char a3_da_ctbl37nit[30] = {0, 0, 0, -2, 2, -7, -10, 1, -18, -8, 4, -11, -9, 1, -10, -6, -1, -5, -2, 0, -3, 2, 2, 1, -3, -3, -3, 1, 0, 0};
static signed char a3_da_ctbl39nit[30] = {0, 0, 0, -4, 0, -10, -8, 3, -17, -10, 1, -12, -6, 1, -6, -3, 2, -4, -3, 1, -2, 1, 1, 0, -1, 0, -1, 1, -1, 0};
static signed char a3_da_ctbl41nit[30] = {0, 0, 0, 0, -1, -6, -21, 0, -27, -10, 1, -13, -2, 3, -4, -3, 2, -4, -2, 1, -1, 0, 0, -1, 0, 0, 0, 1, -1, 0};
static signed char a3_da_ctbl44nit[30] = {0, 0, 0, -2, 2, -6, -8, 1, -19, -13, 1, -13, -6, 0, -6, -5, 0, -6, -1, 1, -1, 1, 0, 0, -1, 0, 0, 1, -1, 0};
static signed char a3_da_ctbl47nit[30] = {0, 0, 0, -2, 2, -7, -17, 1, -24, -4, 2, -7, -3, 2, -4, -3, 1, -4, -2, 0, -2, 0, 0, -1, 0, 0, 1, 1, -1, 0};
static signed char a3_da_ctbl50nit[30] = {0, 0, 0, -5, 0, -11, -11, 2, -20, -7, 1, -8, -3, 3, -4, -4, 0, -5, -2, 1, -2, 1, 0, -1, 0, 0, 1, 1, -1, 0};
static signed char a3_da_ctbl53nit[30] = {0, 0, 0, -6, 0, -12, -12, 3, -20, -6, 2, -8, -2, 2, -3, -2, 1, -3, -2, 1, -1, 1, 0, 0, 0, -1, 1, 1, 0, -1};
static signed char a3_da_ctbl56nit[30] = {0, 0, 0, -3, 2, -8, -11, 2, -20, -9, -1, -9, -6, 0, -8, -3, 1, -3, 0, 1, 0, 0, 0, -1, 0, -2, 1, 1, 0, -1};
static signed char a3_da_ctbl60nit[30] = {0, 0, 0, -6, 0, -11, -13, 1, -21, -9, 1, -10, -5, 0, -5, -1, 2, -2, 0, 1, 0, 0, 0, -1, -1, -1, -1, 2, 0, 1};
static signed char a3_da_ctbl64nit[30] = {0, 0, 0, -2, 1, -9, -13, 2, -21, -4, 3, -4, -6, -1, -6, -1, 2, -2, 1, 1, 2, -1, -1, -3, 0, -2, 2, 1, 0, -1};
static signed char a3_da_ctbl68nit[30] = {0, 0, 0, -2, 1, -9, -13, 0, -21, -4, 2, -4, -2, 3, -3, -2, 1, -1, 1, 1, 1, -1, 0, -3, -3, -1, 0, 2, -1, 0};
static signed char a3_da_ctbl72nit[30] = {0, 0, 0, -3, 1, -9, -16, 0, -24, -5, 2, -5, -2, 0, -1, 0, 2, -1, 0, 1, 1, -1, -2, -2, -1, 0, 0, 2, 0, 1};
static signed char a3_da_ctbl77nit[30] = {0, 0, 0, -3, 1, -10, -17, -1, -23, -4, 2, -4, 1, 2, 0, -1, 1, -1, -1, 0, 0, 1, 0, 0, -1, 0, -1, 1, 0, 0};
static signed char a3_da_ctbl82nit[30] = {0, 0, 0, -2, 1, -9, -13, 3, -20, -5, 2, -6, 4, 2, 2, -4, 0, -3, 0, 0, 0, -2, -2, -2, 0, 0, 0, 1, 0, 1};
static signed char a3_da_ctbl87nit[30] = {0, 0, 0, -4, 0, -11, -12, 3, -19, -5, 1, -5, 2, 3, 2, -2, 1, -1, 1, 1, 0, -2, -2, -1, 0, 0, 0, 1, -1, 0};
static signed char a3_da_ctbl93nit[30] = {0, 0, 0, -4, 1, -10, -15, 3, -21, -2, 2, -2, -3, 1, -3, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1, -1, 1};
static signed char a3_da_ctbl98nit[30] = {0, 0, 0, -4, 0, -11, -16, 0, -20, -8, -1, -7, 0, -1, -1, -1, 1, -1, 0, 0, 0, -2, -2, -1, 1, 1, 0, 1, 0, 0};
static signed char a3_da_ctbl105nit[30] = {0, 0, 0, -5, 0, -12, -17, 0, -22, -1, 1, -2, 0, 2, 0, -3, 0, -2, 3, 1, 1, 0, 0, 1, 0, 0, -1, 1, 0, 1};
static signed char a3_da_ctbl111nit[30] = {0, 0, 0, -8, 0, -15, -14, 2, -19, -7, -2, -4, -3, 0, -2, -2, 1, -2, 1, 1, 0, -2, -1, -2, -2, 0, -1, 2, 0, 0};
static signed char a3_da_ctbl119nit[30] = {0, 0, 0, -4, 0, -10, -16, 2, -21, -3, -2, -2, 2, 3, 0, -2, 2, -1, -1, -1, -1, -2, 0, -1, 1, 1, 1, 1, 0, 1};
static signed char a3_da_ctbl126nit[30] = {0, 0, 0, -7, -1, -13, -15, 0, -19, -1, 3, 0, 0, 1, -2, -2, 1, -1, 0, 1, 1, -2, -2, -2, 2, 0, 2, 3, 1, 3};
static signed char a3_da_ctbl134nit[30] = {0, 0, 0, -3, 3, -9, -16, 0, -19, -3, 0, -2, 0, 2, -1, -4, 0, -2, 0, 0, 0, 0, 0, -1, -1, 0, 0, 2, -1, 0};
static signed char a3_da_ctbl143nit[30] = {0, 0, 0, 0, 3, -6, -12, 2, -16, -3, 1, -2, -1, 0, -1, -3, 0, -3, 1, 1, 2, 1, 1, 0, -1, 0, 0, 1, -1, -1};
static signed char a3_da_ctbl152nit[30] = {0, 0, 0, -1, 3, -7, -11, 1, -15, -5, 0, -3, 3, 2, 1, -4, 0, -3, 1, 1, 2, 1, 0, 1, -1, 0, -2, 2, 0, 1};
static signed char a3_da_ctbl162nit[30] = {0, 0, 0, -2, 3, -7, -13, 0, -16, -5, -1, -3, 3, 2, 0, -1, 1, 0, 1, 0, 1, 0, 0, 0, -1, 0, 0, 2, 0, 0};
static signed char a3_da_ctbl172nit[30] = {0, 0, 0, -7, -1, -13, -13, -1, -16, 0, 3, 1, 2, 2, 1, 0, 2, 0, 0, 1, 2, 0, 0, 0, -1, 0, -2, 1, 0, 0};
static signed char a3_da_ctbl183nit[30] = {0, 0, 0, 0, 4, -4, -9, 2, -13, -1, 1, -1, 1, 1, 0, -3, -1, -2, 3, 3, 4, -2, -2, -3, -1, 0, -1, 1, 0, 0};
static signed char a3_da_ctbl195nit[30] = {0, 0, 0, -2, 3, -6, -6, 2, -9, 0, 2, 2, 2, 1, 0, 0, 2, 0, 0, 1, 1, 0, 0, 0, -2, -1, -2, 1, 0, 0};
static signed char a3_da_ctbl207nit[30] = {0, 0, 0, -2, 2, -6, -6, 2, -9, -1, 1, 1, 1, 0, 0, 0, 2, 0, 1, 1, 2, -1, 0, 0, 0, 0, -1, 0, 0, -1};
static signed char a3_da_ctbl220nit[30] = {0, 0, 0, -4, 2, -6, -7, 1, -9, 1, 1, 1, 1, 0, 0, 0, 2, 1, 0, 0, 1, 0, 0, -1, -1, 0, -1, 0, 0, -1};
static signed char a3_da_ctbl234nit[30] = {0, 0, 0, -4, 2, -7, -3, 2, -6, -1, 0, -1, 1, -1, -1, 1, 3, 2, 1, 0, 1, -2, -3, -3, -1, 0, -1, 1, 0, 1};
static signed char a3_da_ctbl249nit[30] = {0, 0, 0, -5, 1, -7, -3, 2, -5, -1, 1, -1, 1, 1, 1, 0, 1, 0, 1, 0, 2, 1, 0, 0, -1, 0, -1, 1, -1, 0};
static signed char a3_da_ctbl265nit[30] = {0, 0, 0, -5, 1, -8, -6, 2, -6, 2, 2, 3, 1, 1, 0, 1, 1, 2, 0, 0, 0, 0, 0, 0, -1, 0, -1, 1, -1, 0};
static signed char a3_da_ctbl282nit[30] = {0, 0, 0, -6, 0, -8, -1, 4, -2, -1, 0, 0, 0, -1, -1, 2, 1, 2, 1, 0, 1, 0, -1, 0, -1, 0, -1, 2, -1, 1};
static signed char a3_da_ctbl300nit[30] = {0, 0, 0, -8, 0, -9, -10, -2, -12, -2, -1, 0, 2, 1, 1, 1, 0, 1, 0, -1, 1, 1, 0, 0, -1, 0, 0, 1, 0, -1};
static signed char a3_da_ctbl316nit[30] = {0, 0, 0, -8, 0, -9, -2, 3, -3, 2, 1, 3, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, -1, 0, -1, 1, 0, 0};
static signed char a3_da_ctbl333nit[30] = {0, 0, 0, -1, 3, -2, -6, 3, -7, -1, 0, 0, 0, 0, 0, -1, -2, 0, 0, 0, -1, 1, 0, 0, -1, 0, 0, 1, 0, 0};
static signed char a3_da_ctbl350nit[30] = {0, 0, 0, 0, 4, -2, 1, 2, 1, 1, 1, 1, 1, 1, 2, -1, 0, 0, 3, 3, 3, -2, -3, -3, -1, 0, -1, 1, 0, -1};
static signed char a3_da_ctbl357nit[30] = {0, 0, 0, -2, 3, -2, -2, 3, -3, -2, 0, 0, 1, 1, 0, -1, 0, 1, 1, 1, 0, 0, 0, 0, -3, -3, -3, 2, 0, 0};
static signed char a3_da_ctbl365nit[30] = {0, 0, 0, -5, 2, -6, -8, 0, -8, -2, -1, 0, 0, 0, 0, 1, 0, 0, 1, 1, 2, -2, 0, -2, -2, -3, -3, 2, 0, 0};
static signed char a3_da_ctbl372nit[30] = {0, 0, 0, -1, 3, -3, 0, 3, 1, 0, -1, 0, 3, 2, 3, 0, 1, 1, 3, 1, 2, -2, -1, -1, -1, -1, -2, 2, 0, 0};
static signed char a3_da_ctbl380nit[30] = {0, 0, 0, -1, 3, -3, 0, 3, 0, 2, 1, 2, 0, 0, 1, 2, 1, 1, 0, 1, 1, 1, 0, 0, -1, -1, -1, 1, -1, -1};
static signed char a3_da_ctbl387nit[30] = {0, 0, 0, -4, 2, -3, -3, 2, -4, -2, -1, 0, 1, 2, 1, 0, 0, 1, 1, 0, 0, -1, 0, 0, -3, -2, -3, 2, -1, -1};
static signed char a3_da_ctbl395nit[30] = {0, 0, 0, -3, 2, -3, -5, 1, -4, -3, -2, -1, 1, 0, 1, 0, -1, 0, 0, 0, 1, -1, 0, -1, 1, 0, -1, 2, 0, -1};
static signed char a3_da_ctbl403nit[30] = {0, 0, 0, -4, 2, -4, -4, 0, -4, -2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2, -2, -1, -3, 0, 0, 1, 2, 0, -1};
static signed char a3_da_ctbl412nit[30] = {0, 0, 0, -4, 1, -4, -4, 0, -4, -2, -1, 0, 1, 0, 1, 0, 1, 1, -1, -1, -2, 1, 1, 1, -1, 0, -1, 1, 0, -1};
static signed char a3_da_ctbl420nit[30] = {0, 0, 0, -5, 0, -6, -2, 0, -2, -3, -2, -2, 2, 0, 2, 0, 0, 1, 1, 0, 1, 0, 2, 0, 0, 0, 0, 1, 0, -1};
static signed char a3_da_ctbl443nit[30] = {0, 0, 0, -6, 1, -5, -2, -1, -4, 1, 0, 1, 1, 1, 1, -1, -2, -1, 0, 0, 1, 1, 1, 1, 0, 0, -1, 0, 0, 0};
static signed char a3_da_ctbl465nit[30] = {0, 0, 0, -6, 1, -5, -1, 2, -2, 0, 0, 0, -1, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, -1, 0, 1, 1, 0, -2};
static signed char a3_da_ctbl488nit[30] = {0, 0, 0, -6, 0, -6, -1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0, -1, 0, 1, 0, 1, -1, 0, -1, 1, 0, 0};
static signed char a3_da_ctbl510nit[30] = {0, 0, 0, 2, 3, 2, 2, 2, 1, -3, -1, -2, 2, 2, 1, 1, 0, 1, 0, -1, -1, 0, 1, 0, -1, -1, 0, 1, 0, -1};
static signed char a3_da_ctbl533nit[30] = {0, 0, 0, 1, 1, 0, 0, 2, 1, 0, 1, 1, -2, -2, -3, 0, 0, 1, -1, -1, -1, 1, 1, 1, 0, 0, 0, 1, -1, -1};
static signed char a3_da_ctbl555nit[30] = {0, 0, 0, -5, 0, 0, 0, 0, 0, -2, -1, -1, -1, -1, -2, 1, 1, 2, -1, -2, -1, 0, 1, 0, 0, 1, 0, 0, -1, -1};
static signed char a3_da_ctbl578nit[30] = {0, 0, 0, -4, 1, -4, 0, 0, 0, 0, 0, 1, -1, 0, -1, 1, 1, 1, -2, -2, -1, 0, 0, -1, -1, -1, 0, 1, 0, 0};
static signed char a3_da_ctbl600nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


struct SmtDimInfo dimming_info[MAX_BR_INFO] = {
	{ .br = 10, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl10nit, .cTbl = a3_da_ctbl10nit, .way = W1},
	{ .br = 11, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl11nit, .cTbl = a3_da_ctbl11nit, .way = W1},
	{ .br = 12, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl12nit, .cTbl = a3_da_ctbl12nit, .way = W1},
	{ .br = 13, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl13nit, .cTbl = a3_da_ctbl13nit, .way = W1},
	{ .br = 14, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl14nit, .cTbl = a3_da_ctbl14nit, .way = W1},
	{ .br = 15, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl15nit, .cTbl = a3_da_ctbl15nit, .way = W1},
	{ .br = 16, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl16nit, .cTbl = a3_da_ctbl16nit, .way = W1},
	{ .br = 17, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl17nit, .cTbl = a3_da_ctbl17nit, .way = W1},
	{ .br = 19, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl19nit, .cTbl = a3_da_ctbl19nit, .way = W1},
	{ .br = 20, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl20nit, .cTbl = a3_da_ctbl20nit, .way = W1},
	{ .br = 21, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl21nit, .cTbl = a3_da_ctbl21nit, .way = W1},
	{ .br = 22, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl22nit, .cTbl = a3_da_ctbl22nit, .way = W1},
	{ .br = 24, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl24nit, .cTbl = a3_da_ctbl24nit, .way = W1},
	{ .br = 25, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl25nit, .cTbl = a3_da_ctbl25nit, .way = W1},
	{ .br = 27, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl27nit, .cTbl = a3_da_ctbl27nit, .way = W1},
	{ .br = 29, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl29nit, .cTbl = a3_da_ctbl29nit, .way = W1},
	{ .br = 30, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl30nit, .cTbl = a3_da_ctbl30nit, .way = W1},
	{ .br = 32, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl32nit, .cTbl = a3_da_ctbl32nit, .way = W1},
	{ .br = 34, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl34nit, .cTbl = a3_da_ctbl34nit, .way = W1},
	{ .br = 37, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl37nit, .cTbl = a3_da_ctbl37nit, .way = W1},
	{ .br = 39, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl39nit, .cTbl = a3_da_ctbl39nit, .way = W1},
	{ .br = 41, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl41nit, .cTbl = a3_da_ctbl41nit, .way = W1},
	{ .br = 44, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl44nit, .cTbl = a3_da_ctbl44nit, .way = W1},
	{ .br = 47, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl47nit, .cTbl = a3_da_ctbl47nit, .way = W1},
	{ .br = 50, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl50nit, .cTbl = a3_da_ctbl50nit, .way = W1},
	{ .br = 53, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl53nit, .cTbl = a3_da_ctbl53nit, .way = W1},
	{ .br = 56, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl56nit, .cTbl = a3_da_ctbl56nit, .way = W1},
	{ .br = 60, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl60nit, .cTbl = a3_da_ctbl60nit, .way = W1},
	{ .br = 64, .refBr = 107, .cGma = gma2p15, .rTbl = a3_da_rtbl64nit, .cTbl = a3_da_ctbl64nit, .way = W1},
	{ .br = 68, .refBr = 113, .cGma = gma2p15, .rTbl = a3_da_rtbl68nit, .cTbl = a3_da_ctbl68nit, .way = W1},
	{ .br = 72, .refBr = 118, .cGma = gma2p15, .rTbl = a3_da_rtbl72nit, .cTbl = a3_da_ctbl72nit, .way = W1},
	{ .br = 77, .refBr = 128, .cGma = gma2p15, .rTbl = a3_da_rtbl77nit, .cTbl = a3_da_ctbl77nit, .way = W1},
	{ .br = 82, .refBr = 135, .cGma = gma2p15, .rTbl = a3_da_rtbl82nit, .cTbl = a3_da_ctbl82nit, .way = W1},
	{ .br = 87, .refBr = 142, .cGma = gma2p15, .rTbl = a3_da_rtbl87nit, .cTbl = a3_da_ctbl87nit, .way = W1},
	{ .br = 93, .refBr = 150, .cGma = gma2p15, .rTbl = a3_da_rtbl93nit, .cTbl = a3_da_ctbl93nit, .way = W1},
	{ .br = 98, .refBr = 156, .cGma = gma2p15, .rTbl = a3_da_rtbl98nit, .cTbl = a3_da_ctbl98nit, .way = W1},
	{ .br = 105, .refBr = 169, .cGma = gma2p15, .rTbl = a3_da_rtbl105nit, .cTbl = a3_da_ctbl105nit, .way = W1},
	{ .br = 111, .refBr = 177, .cGma = gma2p15, .rTbl = a3_da_rtbl111nit, .cTbl = a3_da_ctbl111nit, .way = W1},
	{ .br = 119, .refBr = 188, .cGma = gma2p15, .rTbl = a3_da_rtbl119nit, .cTbl = a3_da_ctbl119nit, .way = W1},
	{ .br = 126, .refBr = 197, .cGma = gma2p15, .rTbl = a3_da_rtbl126nit, .cTbl = a3_da_ctbl126nit, .way = W1},
	{ .br = 134, .refBr = 208, .cGma = gma2p15, .rTbl = a3_da_rtbl134nit, .cTbl = a3_da_ctbl134nit, .way = W1},
	{ .br = 143, .refBr = 222, .cGma = gma2p15, .rTbl = a3_da_rtbl143nit, .cTbl = a3_da_ctbl143nit, .way = W1},
	{ .br = 152, .refBr = 234, .cGma = gma2p15, .rTbl = a3_da_rtbl152nit, .cTbl = a3_da_ctbl152nit, .way = W1},
	{ .br = 162, .refBr = 247, .cGma = gma2p15, .rTbl = a3_da_rtbl162nit, .cTbl = a3_da_ctbl162nit, .way = W1},
	{ .br = 172, .refBr = 253, .cGma = gma2p15, .rTbl = a3_da_rtbl172nit, .cTbl = a3_da_ctbl172nit, .way = W1},
	{ .br = 183, .refBr = 253, .cGma = gma2p15, .rTbl = a3_da_rtbl183nit, .cTbl = a3_da_ctbl183nit, .way = W1},
	{ .br = 195, .refBr = 253, .cGma = gma2p15, .rTbl = a3_da_rtbl195nit, .cTbl = a3_da_ctbl195nit, .way = W1},
	{ .br = 207, .refBr = 253, .cGma = gma2p15, .rTbl = a3_da_rtbl207nit, .cTbl = a3_da_ctbl207nit, .way = W1},
	{ .br = 220, .refBr = 253, .cGma = gma2p15, .rTbl = a3_da_rtbl220nit, .cTbl = a3_da_ctbl220nit, .way = W1},
	{ .br = 234, .refBr = 267, .cGma = gma2p15, .rTbl = a3_da_rtbl234nit, .cTbl = a3_da_ctbl234nit, .way = W1},
	{ .br = 249, .refBr = 284, .cGma = gma2p15, .rTbl = a3_da_rtbl249nit, .cTbl = a3_da_ctbl249nit, .way = W1},
	{ .br = 265, .refBr = 301, .cGma = gma2p15, .rTbl = a3_da_rtbl265nit, .cTbl = a3_da_ctbl265nit, .way = W1},
	{ .br = 282, .refBr = 320, .cGma = gma2p15, .rTbl = a3_da_rtbl282nit, .cTbl = a3_da_ctbl282nit, .way = W1},
	{ .br = 300, .refBr = 338, .cGma = gma2p15, .rTbl = a3_da_rtbl300nit, .cTbl = a3_da_ctbl300nit, .way = W1},
	{ .br = 316, .refBr = 357, .cGma = gma2p15, .rTbl = a3_da_rtbl316nit, .cTbl = a3_da_ctbl316nit, .way = W1},
	{ .br = 333, .refBr = 373, .cGma = gma2p15, .rTbl = a3_da_rtbl333nit, .cTbl = a3_da_ctbl333nit, .way = W1},
	{ .br = 350, .refBr = 393, .cGma = gma2p15, .rTbl = a3_da_rtbl350nit, .cTbl = a3_da_ctbl350nit, .way = W1},
	{ .br = 357, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl357nit, .cTbl = a3_da_ctbl357nit, .way = W1},
	{ .br = 365, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl365nit, .cTbl = a3_da_ctbl365nit, .way = W1},
	{ .br = 372, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl372nit, .cTbl = a3_da_ctbl372nit, .way = W1},
	{ .br = 380, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl380nit, .cTbl = a3_da_ctbl380nit, .way = W1},
	{ .br = 387, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl387nit, .cTbl = a3_da_ctbl387nit, .way = W1},
	{ .br = 395, .refBr = 405, .cGma = gma2p15, .rTbl = a3_da_rtbl395nit, .cTbl = a3_da_ctbl395nit, .way = W1},
	{ .br = 403, .refBr = 414, .cGma = gma2p15, .rTbl = a3_da_rtbl403nit, .cTbl = a3_da_ctbl403nit, .way = W1},
	{ .br = 412, .refBr = 420, .cGma = gma2p15, .rTbl = a3_da_rtbl412nit, .cTbl = a3_da_ctbl412nit, .way = W1},
	{ .br = 420, .refBr = 428, .cGma = gma2p15, .rTbl = a3_da_rtbl420nit, .cTbl = a3_da_ctbl420nit, .way = W1},
	{ .br = 443, .refBr = 453, .cGma = gma2p15, .rTbl = a3_da_rtbl443nit, .cTbl = a3_da_ctbl443nit, .way = W1},
	{ .br = 465, .refBr = 472, .cGma = gma2p15, .rTbl = a3_da_rtbl465nit, .cTbl = a3_da_ctbl465nit, .way = W1},
	{ .br = 488, .refBr = 493, .cGma = gma2p15, .rTbl = a3_da_rtbl488nit, .cTbl = a3_da_ctbl488nit, .way = W1},
	{ .br = 510, .refBr = 517, .cGma = gma2p15, .rTbl = a3_da_rtbl510nit, .cTbl = a3_da_ctbl510nit, .way = W1},
	{ .br = 533, .refBr = 538, .cGma = gma2p15, .rTbl = a3_da_rtbl533nit, .cTbl = a3_da_ctbl533nit, .way = W1},
	{ .br = 555, .refBr = 562, .cGma = gma2p15, .rTbl = a3_da_rtbl555nit, .cTbl = a3_da_ctbl555nit, .way = W1},
	{ .br = 578, .refBr = 582, .cGma = gma2p15, .rTbl = a3_da_rtbl578nit, .cTbl = a3_da_ctbl578nit, .way = W1},
	{ .br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = a3_da_rtbl600nit, .cTbl = a3_da_ctbl600nit, .way = W1},
};

static const unsigned char MAX_GAMMA_SET[GAMMA_CMD_CNT] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x00
};

int get_panel_id(void)
{
	return panel_id;
}
EXPORT_SYMBOL(get_panel_id);

static int __init panel_id_cmdline(char *mode)
{
	char *pt;

	panel_id = 0;
	if (mode == NULL)
		return 1;

	for (pt = mode; *pt != 0; pt++) {
		panel_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			panel_id += *pt - '0';
		break;
		case 'a' ... 'f':
			panel_id += 10 + *pt - 'a';
		break;
		case 'A' ... 'F':
			panel_id += 10 + *pt - 'A';
		break;
		}
	}

	pr_info("%s: panel_id = 0x%x", __func__, panel_id);

	return 0;
}
__setup("lcdtype=", panel_id_cmdline);

static void s6e36w2x01_esd_detect_work(struct work_struct *work)
{
	struct s6e36w2x01 *lcd = container_of(work,
						struct s6e36w2x01, det_work);
	char *event_string = "LCD_ESD=ON";
	char *envp[] = {event_string, NULL};

	if (!POWER_IS_OFF(lcd->power)) {
		kobject_uevent_env(&esd_dev->kobj,
			KOBJ_CHANGE, envp);
		dev_info(lcd->dev, "%s:Send uevent. ESD DETECTED\n", __func__);
	}
}

irqreturn_t s6e36w2x01_esd_interrupt(int irq, void *dev_id)
{
	struct s6e36w2x01 *lcd = dev_id;

	return IRQ_HANDLED;

	if (!work_busy(&lcd->det_work)) {
		schedule_work(&lcd->det_work);
		dev_info(lcd->dev, "%s: add esd schedule_work by irq[%d]]\n",
			__func__, irq);
	}

	return IRQ_HANDLED;
}

static ssize_t s6e36w2x01_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "SDC_%06x\n", panel_id);
	strcat(buf, temp);

	return strlen(buf);
}
DEVICE_ATTR(lcd_type, 0444, s6e36w2x01_lcd_type_show, NULL);

static ssize_t s6e36w2x01_mcd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return sprintf(buf, "%s\n", lcd->mcd_on ? "on" : "off");
}

extern void s6e36w2x01_mcd_test_on(void);
static ssize_t s6e36w2x01_mcd_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->mcd_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->mcd_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	s6e36w2x01_mcd_test_on();

	return size;
}

static ssize_t s6e36w2x01_hlpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	pr_info("%s:val[%d]\n", __func__, lcd->hlpm_on);

	return sprintf(buf, "%d\n", lcd->hlpm_on);
}

extern void s6e36w2x01_hlpm_ctrl(struct s6e36w2x01 *lcd, bool enable);
static ssize_t s6e36w2x01_hlpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hlpm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->hlpm_on = false;
	else {
		dev_warn(dev, "invalid command.\n");
		return size;
	}

	pr_info("%s: hlpm_on[%d]\n", __func__, lcd->hlpm_on);

	return size;
}

static ssize_t s6e36w2x01_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	int len = 0;

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

	switch (lcd->ao_mode) {
	case AO_NODE_OFF:
		len = sprintf(buf, "%s\n", "off");
		break;
	case AO_NODE_ALPM:
		len = sprintf(buf, "%s\n", "on");
		break;
	case AO_NODE_SELF:
		len = sprintf(buf, "%s\n", "self");
		break;
	default:
		dev_warn(dev, "invalid status.\n");
		break;
	}

	return len;
}

static ssize_t s6e36w2x01_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	if (!strncmp(buf, "on", 2))
		lcd->ao_mode = AO_NODE_ALPM;
	else if (!strncmp(buf, "off", 3))
		lcd->ao_mode = AO_NODE_OFF;
	else if (!strncmp(buf, "self", 4))
		lcd->ao_mode = AO_NODE_SELF;
	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

	return size;
}

extern void s6e36w2x01_write_sc_config(void);
extern void s6e36w2x01_write_sideram_data(struct s6e36w2x01 *lcd);
extern void s6e36w2x01_set_sc_config(int cmd, unsigned int param);
extern void s6e36w2x01_disable_sc_config(void);
extern int s6e36w2x01_get_sc_time(int *h, int* m, int* s, int *ms);
extern int s6e36w2x01_get_sc_config(int cmd);
static void s6e36w2x01_set_sc_time(struct s6e36w2x01 *lcd, int h, int m, int s, int ms);
static void s6e36w2x01_set_sc_center(int x, int y);
static void s6e36w2x01_set_sc_update_rate(struct s6e36w2x01 *lcd, unsigned int rate);
static ssize_t s6e36w2x01_selfclock_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	int panel_h, panel_m, panel_s, panel_ms, ret;


	pr_info("%s:%s\n", __func__, buf);

	if (!strncmp(buf, "on", 2)) {
		s6e36w2x01_hlpm_ctrl(lcd, true);
		s6e36w2x01_write_sideram_data(lcd);
		s6e36w2x01_set_sc_config(SC_ANA_CLOCK_EN, 1);
		s6e36w2x01_write_sc_config();
	} else if (!strncmp(buf, "up", 2)) {
		ret = s6e36w2x01_get_sc_time(&panel_h, &panel_m, &panel_s, &panel_ms);
		if (ret) {
			pr_err("%s: Failed to get panel time[%d]\n", __func__, ret);
			panel_h = 0;
			panel_m= 0;
			panel_s = 0;
			panel_ms = 0;
		}
		panel_m += 1;
		s6e36w2x01_set_sc_time(lcd, panel_h, panel_m, panel_s, panel_ms*SC_MSEC_DIVIDER);
		s6e36w2x01_write_sc_config();
		if (s6e36w2x01_get_sc_config(SC_TIME_UPDATE)) {
			usleep_range(SC_1FRAME_USEC, SC_1FRAME_USEC); /* wait 1 frame */
			s6e36w2x01_set_sc_config(SC_TIME_UPDATE, false);
			s6e36w2x01_write_sc_config();
		}
	} else {
		s6e36w2x01_hlpm_ctrl(lcd, false);
		s6e36w2x01_set_sc_config(SC_ANA_CLOCK_EN, 0);
		s6e36w2x01_disable_sc_config();
	}

	return size;
}
static ssize_t s6e36w2x01_self_blink_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	static int x1 = 180, y1 = 120;
	static int x2 = 180, y2 = 240;

	pr_info("%s:%s\n", __func__, buf);
	if (!strncmp(buf, "on", 2)) {
		s6e36w2x01_set_sc_config(SB_CIRCLE_1_X, x1);
		s6e36w2x01_set_sc_config(SB_CIRCLE_1_Y, y1 );
		s6e36w2x01_set_sc_config(SB_CIRCLE_2_X, x2);
		s6e36w2x01_set_sc_config(SB_CIRCLE_2_Y, y2);

		s6e36w2x01_set_sc_config(SB_BLK_EN, 1);
	}
	else if (!strncmp(buf, "up", 2)) {
		swap(x1, y1);
		swap(x2, y2);
		s6e36w2x01_set_sc_config(SB_CIRCLE_1_X, x1);
		s6e36w2x01_set_sc_config(SB_CIRCLE_1_Y, y1 );
		s6e36w2x01_set_sc_config(SB_CIRCLE_2_X, x2);
		s6e36w2x01_set_sc_config(SB_CIRCLE_2_Y, y2);
	} else
		s6e36w2x01_set_sc_config(SB_BLK_EN, 0);

	s6e36w2x01_write_sc_config();

	return size;
}


extern void s6e36w2x01_write_sa_config(void);
extern void s6e36w2x01_set_sa_config(int cmd, unsigned int param);
static ssize_t s6e36w2x01_self_animation_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	pr_info("%s:%s\n", __func__, buf);

	if (!strncmp(buf, "on", 2))
		s6e36w2x01_set_sa_config(SA_BRT_EN, 1);
	else
		s6e36w2x01_set_sa_config(SA_BRT_EN, 0);

	s6e36w2x01_write_sa_config();

	return size;
}

static ssize_t s6e36w2x01_self_mask_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	pr_info("%s:%s\n", __func__, buf);

	if (!strncmp(buf, "on", 2))
		s6e36w2x01_set_sa_config(SM_CIR_EN, 1);
	else
		s6e36w2x01_set_sa_config(SM_CIR_EN, 0);

	s6e36w2x01_write_sa_config();

	return size;
}

static ssize_t s6e36w2x01_scm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return sprintf(buf, "%s\n", lcd->scm_on ? "on" : "off");
}

static ssize_t s6e36w2x01_scm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	if (!strncmp(buf, "on", 2))
		lcd->scm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->scm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s: val[%d]\n", __func__, lcd->scm_on);

	return size;
}

#define 	MAX_MTP_READ_SIZE	0xff
static unsigned char mtp_read_data[MAX_MTP_READ_SIZE] = {0, };
static int read_data_length = 0;

static ssize_t s6e36w2x01_read_mtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char buffer[LDI_MTP4_LEN*8] = {0, };
	int i, ret = 0;

	for ( i = 0; i < read_data_length; i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", mtp_read_data[i]);
		strcat(buf, buffer);

		if ( i < (read_data_length-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");

	pr_info("%s: length=%d\n", __func__, read_data_length);
	pr_info("%s\n", buf);

	return ret;
}

extern void s6e36w2x01_read_mtp_reg(u32 addr, char* buffer, u32 size);
static ssize_t s6e36w2x01_read_mtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	u32 buff[3] = {0, }; //register, size, offset
	int i;

	if (lcd->alpm_on) {
		pr_err("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_err("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	sscanf(buf, "0x%x, 0x%x, 0x%x", &buff[0], &buff[1], &buff[2]);
	pr_info("%s: 0x%x, 0x%x, 0x%x\n", __func__, buff[0], buff[1], buff[2]);

	for (i = 0; i < 3; i++) {
		if (buff[i] > MAX_MTP_READ_SIZE)
			return -EINVAL;
	}

	read_data_length = buff[1];

	s6e36w2x01_read_mtp_reg(buff[0]+buff[2], &mtp_read_data[0], read_data_length);

	return size;
}

static int dim_table_num = 0;
#define MAX_DIMMING_TABLE	4
#define DIMMING_TABLE_PRINT_SIZE	17
static ssize_t s6e36w2x01_dimming_table_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0) {
		dev_err(lcd->dev,
			"%s: failed to read parameter value\n",
			__func__);
		return -EIO;
	}

	if ((value > MAX_DIMMING_TABLE) ||(value < 0)){
		dev_err(lcd->dev, 	"%s: Invalid value. [%d]\n",
			__func__, (int)value);
		return -EIO;
	}

	dim_table_num = value;

	dev_info(lcd->dev, "%s: dimming_table[%d]\n",
			__func__, dim_table_num);

	return size;
}

static ssize_t s6e36w2x01_dimming_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct SmtDimInfo *diminfo = NULL;

	char buffer[32];
	int ret = 0;
	int i, j;
	int max_cnt;

	diminfo = (void *)dimming_info;

	if (dim_table_num == 0) {
		i = 0;
		max_cnt = DIMMING_TABLE_PRINT_SIZE;
	} else if (dim_table_num == 1) {
		i = DIMMING_TABLE_PRINT_SIZE;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 2;
	} else if (dim_table_num == 2)  {
		i = DIMMING_TABLE_PRINT_SIZE * 2;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 3;
	} else if (dim_table_num ==3) {
		i = DIMMING_TABLE_PRINT_SIZE * 3;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 4;
	} else if (dim_table_num ==4) {
		i = DIMMING_TABLE_PRINT_SIZE * 4;
		max_cnt = MAX_GAMMA_CNT;
	}

	for (; i < max_cnt; i++) {
		ret += sprintf(buffer, "[Gtbl][%3d] : ", diminfo[i].br);
		strcat(buf, buffer);

		for(j =0; j < GAMMA_CMD_CNT ; j++) {
				ret += sprintf(buffer, "0x%02x, ", gamma_set[i][j]);
				strcat(buf, buffer);
			}
		strcat(buf, "\n");
		ret += strlen("\n");
	}

	return ret;
}

static ssize_t s6e36w2x01_cr_map_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	char buffer[32];
	int ret = 0;
	int i;

	for (i = 0; i <= MAX_BRIGHTNESS; i++) {
		ret += sprintf(buffer, " %3d,", candela_tbl[lcd->br_map[i]]);
		strcat(buf, buffer);
		if ((i == 0) || !(i %10)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");
	pr_info("%s:%s\n", __func__, buf);

	return ret;
}

static ssize_t s6e36w2x01_br_map_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	char buffer[32];
	int ret = 0;
	int i;

	for (i = 0; i <= MAX_BRIGHTNESS; i++) {
		ret += sprintf(buffer, " %3d,", lcd->br_map[i]);
		strcat(buf, buffer);
		if ((i == 0) || !(i %10)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");
	pr_info("%s:%s\n", __func__, buf);

	return ret;
}

static void s6e36w2x01_acl_update(struct s6e36w2x01 *lcd, unsigned int value)
{
	/* TODO: implement acl update function */
}

static ssize_t s6e36w2x01_cell_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	char temp[LDI_MTP4_MAX_PARA];
	char result_buff[CELL_ID_LEN+1];

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	memset(&temp[0], 0x00, LDI_MTP4_MAX_PARA);
	s6e36w2x01_read_mtp_reg(LDI_MTP4, &temp[0], LDI_MTP4_MAX_PARA);
	sprintf(result_buff, "%02x%02x%02x%02x%02x%02x%02x",
				temp[40], temp[41], temp[42], temp[43],
				temp[44], temp[45], temp[46]);
	strcat(buf, result_buff);

	memset(&temp[0], 0x00, WHITE_COLOR_LEN);
	s6e36w2x01_read_mtp_reg(LDI_WHITE_COLOR, &temp[0], WHITE_COLOR_LEN);
	sprintf(result_buff, "%02x%02x%02x%02x\n",
				temp[0], temp[1], temp[2], temp[3]);
	strcat(buf, result_buff);

	pr_info("%s:[%s]", __func__, buf);

	return strlen(buf);
}

static ssize_t s6e36w2x01_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return sprintf(buf, "%d\n", lcd->acl);
}

static ssize_t s6e36w2x01_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	unsigned long value;
	int rc;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	s6e36w2x01_acl_update(lcd, value);

	lcd->acl = value;

	dev_info(lcd->dev, "acl control[%d]\n", lcd->acl);

	return size;
}

static ssize_t s6e36w2x01_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

extern int s6e36w2x01_hbm_on(void);
static ssize_t s6e36w2x01_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	s6e36w2x01_hbm_on();

	dev_info(lcd->dev, "HBM %s.\n", lcd->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t s6e36w2x01_elvss_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", lcd->temp_stage);
}

extern int s6e36w2x01_temp_offset_comp(unsigned int stage);
static ssize_t s6e36w2x01_elvss_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	unsigned long value;
	int rc;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	lcd->temp_stage = value;

	s6e36w2x01_temp_offset_comp(lcd->temp_stage);

	dev_info(lcd->dev, "ELVSS temp stage[%d].\n", lcd->temp_stage);

	return size;
}

static ssize_t s6e36w2x01_octa_chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	char temp[20];

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	memset(&lcd->chip[0], 0x00, LDI_CHIP_LEN);

	s6e36w2x01_read_mtp_reg(LDI_CHIP_ID, &lcd->chip[0], LDI_CHIP_LEN);

	sprintf(temp, "%02x%02x%02x%02x%02x\n",
				lcd->chip[0], lcd->chip[1], lcd->chip[2],
				lcd->chip[3], lcd->chip[4]);
	strcat(buf, temp);

	pr_info("%s: %s\n", __func__, temp);

	return strlen(buf);
}

static ssize_t s6e36w2x01_refresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return sprintf(buf, "%d\n", lcd->refresh);
}

static ssize_t s6e36w2x01_refresh_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	if (strncmp(buf, "30", 2) && strncmp(buf, "60", 2)) {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	/* TODO: implement frame freq set */
	if (!strncmp(buf, "30", 2)) {
		lcd->refresh = 30;
	} else {
		lcd->refresh = 60;
	}

	return size;
}

#ifdef CONFIG_COPR_SUPPORT
int convert_to_candela(int brightnees){
	struct SmtDimInfo *diminfo = NULL;
	int count;

	diminfo = (void *)dimming_info;

	if (brightnees == HBM_BRIHGTNESS) {
		printk("[COPR] brightnees : %d, candela : %d\n", brightnees, HBM_CANDELA);
		return HBM_CANDELA;
	}
	for (count = 0; count < DIMMING_COUNT; count++) {
		if (br_convert[count] >= brightnees) {
			printk("[COPR] brightnees : %d, candela : %d\n", brightnees, diminfo[count].br);
			return diminfo[count].br;
		}
	}
	printk("[COPR] brightness is over the range(%d)\n", brightnees);
	return 0;
}
EXPORT_SYMBOL(convert_to_candela);

static ssize_t s6e36w2x01_copr_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);

	return sprintf(buf, "%d\n", lcd->copr);
}

extern void s6e36w2x01_copr_ctrl(u32 size);
static ssize_t s6e36w2x01_copr_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	unsigned long value;
	int ret;

	ret = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (ret < 0)
		return ret;

	s6e36w2x01_copr_ctrl(value);

	lcd->copr = value;

	dev_info(lcd->dev, "COPR %d\n", lcd->copr);

	return size;
}
#endif

#ifdef CONFIG_EXYNOS_HLPM_TEST_SUPPORT
extern void s6e36w2x01_hlpm_test(int mode);
static ssize_t s6e36w2x01_hlpm_power_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hlpm_mode);
}

static ssize_t s6e36w2x01_hlpm_power_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0) {
		dev_err(lcd->dev,
			"%s: failed to read parameter value\n",
			__func__);
		return -EIO;
	}

	if ((value > 8) ||(value < 0)){
		dev_err(lcd->dev, 	"%s: Invalid value. [%d]\n",
			__func__, (int)value);
		return -EIO;
	}

	s6e36w2x01_hlpm_test(value);
	hlpm_mode = value;
	dev_info(lcd->dev, "%s: mode[%d]\n",
			__func__, value);

	return size;
}

extern void s6e36w2x01_normal_dc_test(int mode);
static ssize_t s6e36w2x01_normal_dc_power_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", normal_dc_mode);
}

static ssize_t s6e36w2x01_normal_dc_power_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd_dev = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&lcd_dev->dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0) {
		dev_err(lcd->dev,
			"%s: failed to read parameter value\n",
			__func__);
		return -EIO;
	}

	if ((value > 4) ||(value < 0)){
		dev_err(lcd->dev, 	"%s: Invalid value. [%d]\n",
			__func__, (int)value);
		return -EIO;
	}

	s6e36w2x01_normal_dc_test(value);
	normal_dc_mode = value;
	dev_info(lcd->dev, "%s: mode[%d]\n",
			__func__, value);

	return size;
}
#endif

static struct device_attribute ld_dev_attrs[] = {
	__ATTR(mcd_test, S_IRUGO | S_IWUSR, s6e36w2x01_mcd_show, s6e36w2x01_mcd_store),
	__ATTR(self_mask, S_IWUSR, NULL, s6e36w2x01_self_mask_store),
	__ATTR(self_animation, S_IWUSR, NULL, s6e36w2x01_self_animation_store),
	__ATTR(self_clock, S_IWUSR, NULL, s6e36w2x01_selfclock_store),
	__ATTR(self_blink, S_IWUSR, NULL, s6e36w2x01_self_blink_store),
	__ATTR(alpm, S_IRUGO | S_IWUSR, s6e36w2x01_alpm_show, s6e36w2x01_alpm_store),
	__ATTR(hlpm, S_IRUGO | S_IWUSR, s6e36w2x01_hlpm_show, s6e36w2x01_hlpm_store),
	__ATTR(cell_id, S_IRUGO, s6e36w2x01_cell_id_show, NULL),
	__ATTR(acl, S_IRUGO | S_IWUSR, s6e36w2x01_acl_show, s6e36w2x01_acl_store),
	__ATTR(scm, S_IRUGO | S_IWUSR, s6e36w2x01_scm_show, s6e36w2x01_scm_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, s6e36w2x01_hbm_show, s6e36w2x01_hbm_store),
	__ATTR(elvss, S_IRUGO | S_IWUSR, s6e36w2x01_elvss_show, s6e36w2x01_elvss_store),
	__ATTR(chip_id, S_IRUGO, s6e36w2x01_octa_chip_id_show, NULL),
	__ATTR(refresh, S_IRUGO | S_IWUSR,
			s6e36w2x01_refresh_show, s6e36w2x01_refresh_store),
	__ATTR(cr_map, S_IRUGO, s6e36w2x01_cr_map_show, NULL),
	__ATTR(br_map, S_IRUGO, s6e36w2x01_br_map_show, NULL),
	__ATTR(dim_table, S_IRUGO | S_IWUSR,
			s6e36w2x01_dimming_table_show, s6e36w2x01_dimming_table_store),
	__ATTR(read_mtp, S_IRUGO | S_IWUSR,
			s6e36w2x01_read_mtp_show, s6e36w2x01_read_mtp_store),
#ifdef CONFIG_COPR_SUPPORT
	__ATTR(copr, S_IRUGO | S_IWUSR,
			s6e36w2x01_copr_show, s6e36w2x01_copr_store),
#endif
#ifdef CONFIG_EXYNOS_HLPM_TEST_SUPPORT
    __ATTR(hlpm_power, S_IRUGO | S_IWUSR,
	    	s6e36w2x01_hlpm_power_show, s6e36w2x01_hlpm_power_store),
    __ATTR(normal_dc_power, S_IRUGO | S_IWUSR,
	    	s6e36w2x01_normal_dc_power_show, s6e36w2x01_normal_dc_power_store),
#endif
};

static int s6e36w2x01_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

extern int s6e36w2x01_gamma_ctrl(u32 level);
static int s6e36w2x01_set_brightness(struct backlight_device *bd)
{
	struct s6e36w2x01 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness, ret = 0;

	if (!lcd) {
		pr_info("%s: LCD is NULL\n", "set_br");
		ret = -ENODEV;
		goto out;
	}

	pr_info("%s[%d]lv[%d]pwr[%d]aod[%d]hnit[%d]hbm[%d]\n", "set_br",
		brightness, lcd->br_map[brightness], lcd->power,
		lcd->aod_enable, lcd->hlpm_nit, lcd->hbm_on);

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		pr_debug("%s:Brightness should be in the range of %d ~ %d\n",
			"set_br", MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		ret = -EINVAL;
		goto out;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_debug("%s:invalid power[%d]\n", "set_br", lcd->power);
		ret = -EPERM;
		goto out;
	}

	if (lcd->aod_enable) {
		pr_debug("%s:aod enabled\n", "set_br");
		ret = -EPERM;
		goto out;
	}

	 if (lcd->hbm_on)
		ret = s6e36w2x01_hbm_on();
	else
		ret = s6e36w2x01_gamma_ctrl(lcd->br_map[brightness]);
out:
	return ret;
}

static int s6e36w2x01_get_power(struct lcd_device *ld)
{
	struct s6e36w2x01 *lcd = lcd_get_data(ld);

	pr_debug("%s[%d]\n", "get_power", lcd->power);

	return lcd->power;
}

static int s6e36w2x01_set_power(struct lcd_device *ld, int power)
{
	struct s6e36w2x01 *lcd = lcd_get_data(ld);

	lcd->power = power;

#ifdef CONFIG_SLEEP_MONITOR
	if (power == FB_BLANK_UNBLANK)
		lcd->act_cnt++;
#endif

	pr_info("%s[%d]\n", "set_power", lcd->power);

	return 0;
}

static struct lcd_ops s6e36w2x01_lcd_ops = {
	.get_power = s6e36w2x01_get_power,
	.set_power = s6e36w2x01_set_power,
};

static const struct backlight_ops s6e36w2x01_backlight_ops = {
	.get_brightness = s6e36w2x01_get_brightness,
	.update_status = s6e36w2x01_set_brightness,
};

extern void s6e36w2x01_mdnie_set(enum mdnie_scenario scenario);
extern void s6e36w2x01_mdnie_outdoor_set(enum mdnie_outdoor on);
static ssize_t s6e36w2x01_scenario_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w2x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->scenario);
}

static ssize_t s6e36w2x01_scenario_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w2x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	sscanf(buf, "%d", &value);

	dev_info(lcd->dev, "%s:cur[%d] new[%d]\n",
		__func__, mdnie->scenario, value);

	if (mdnie->scenario == value)
		return size;

	mdnie->scenario = value;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	s6e36w2x01_mdnie_set(mdnie->scenario);

	return size;
}

static ssize_t s6e36w2x01_outdoor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w2x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->outdoor);
}

static ssize_t s6e36w2x01_outdoor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w2x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_enable) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	sscanf(buf, "%d", &value);

	if (value >= OUTDOOR_MAX) {
		dev_warn(lcd->dev, "invalid outdoor mode set\n");
		return -EINVAL;
	}
	mdnie->outdoor = value;

	s6e36w2x01_mdnie_outdoor_set(value);

	mdnie->outdoor = value;

	return size;
}

static struct device_attribute mdnie_attrs[] = {
	__ATTR(scenario, 0664, s6e36w2x01_scenario_show, s6e36w2x01_scenario_store),
	__ATTR(outdoor, 0664, s6e36w2x01_outdoor_show, s6e36w2x01_outdoor_store),
};

void s6e36w2x01_mdnie_lite_init(struct s6e36w2x01 *lcd)
{
	static struct class *mdnie_class;
	struct mdnie_lite_device *mdnie;
	int i;

	mdnie = kzalloc(sizeof(struct mdnie_lite_device), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie object.\n");
		return;
	}

	mdnie_class = class_create(THIS_MODULE, "extension");
	if (IS_ERR(mdnie_class)) {
		pr_err("Failed to create class(mdnie)!\n");
		goto err_free_mdnie;
	}

	mdnie->dev = device_create(mdnie_class, NULL, 0, NULL, "mdnie");
	if (IS_ERR(&mdnie->dev)) {
		pr_err("Failed to create device(mdnie)!\n");
		goto err_free_mdnie;
	}

	for (i = 0; i < ARRAY_SIZE(mdnie_attrs); i++) {
		if (device_create_file(mdnie->dev, &mdnie_attrs[i]) < 0)
			pr_err("Failed to create device file(%s)!\n",
				mdnie_attrs[i].attr.name);
	}

	mdnie->scenario = SCENARIO_UI;
	lcd->mdnie = mdnie;

	dev_set_drvdata(mdnie->dev, lcd);

	return;

err_free_mdnie:
	kfree(mdnie);
}

extern void s6e36w2x01_display_init(struct decon_lcd * lcd);
static int s6e36w2x01_displayon(struct dsim_device *dsim)
{
	struct lcd_device *lcd = dsim->lcd;
	struct s6e36w2x01 *panel = dev_get_drvdata(&lcd->dev);
	struct mdnie_lite_device *mdnie = panel->mdnie;
	struct dsim_resources *res = &dsim->res;

	s6e36w2x01_display_init(&dsim->lcd_info);

	if (mdnie->outdoor == OUTDOOR_ON)
		s6e36w2x01_mdnie_outdoor_set(mdnie->outdoor);
	else
		s6e36w2x01_mdnie_set(mdnie->scenario);

	if (res->oled_det > 0) {
		enable_irq(panel->esd_irq);
	}

	panel->power = FB_BLANK_UNBLANK;

	return 1;
}

static int s6e36w2x01_set_panel_power(struct dsim_device *dsim, bool on)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	if (on) {
		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0], GPIOF_OUT_INIT_HIGH, "lcd_power0");
			if (ret < 0) {
				dev_err(dsim->dev, "failed LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(10000, 11000);
		} else if (res->lcd_regulator[0]) {
			ret = regulator_enable(res->lcd_regulator[0]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_0 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to enable regulator_0\n");

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1], GPIOF_OUT_INIT_HIGH, "lcd_power1");
			if (ret < 0) {
				dev_err(dsim->dev, "failed 2nd LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(10000, 11000);
		} else if (res->lcd_regulator[1]){
			ret = regulator_enable(res->lcd_regulator[1]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_1 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to enable regulator_1\n");

	} else {
		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0], GPIOF_OUT_INIT_LOW, "lcd_power0");
			if (ret < 0) {
				dev_err(dsim->dev, "failed LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(5000, 6000);
		} else if (res->lcd_regulator[0]){
			ret = regulator_disable(res->lcd_regulator[0]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_0 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to disable regulator_0\n");

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1], GPIOF_OUT_INIT_LOW, "lcd_power1");
			if (ret < 0) {
				dev_err(dsim->dev, "failed 2nd LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(5000, 6000);
		} else if (res->lcd_regulator[1]){
			ret = regulator_disable(res->lcd_regulator[1]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_1 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to disable regulator_1\n");

	}

	return 0;
}

int s6e36w2x01_reset_on(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_HIGH, "lcd_reset");
	if (ret < 0) {
		dev_err(dsim->dev, "failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 0);
	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 1);

	gpio_free(res->lcd_reset);

	usleep_range(5000, 6000);

	return 0;
}

int s6e36w2x01_reset_off(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_LOW, "lcd_reset");
	if (ret < 0) {
		dev_err(dsim->dev, "failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	gpio_free(res->lcd_reset);

	return 0;
}

static int s6e36w2x01_reset_ctrl(struct dsim_device *dsim, int on)
{
	struct dsim_resources *res = &dsim->res;
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);

	dev_info(dsim->dev, "%s(%d) +\n", __func__, on);

	if (on) {
		if (lcd->lp_mode)
			goto out;

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on) {
			lcd->boot_power_on = false;
			goto out;
		}
		s6e36w2x01_reset_on(dsim);

		if (res->oled_det > 0)
			enable_irq(lcd->esd_irq);
	} else {
		lcd->power = FB_BLANK_POWERDOWN;
		if (res->oled_det > 0)
			disable_irq(lcd->esd_irq);
		s6e36w2x01_reset_off(dsim);
	}

out:
	dev_info(dsim->dev, "%s(%d) -\n", __func__, on);

	return 0;
}

static int s6e36w2x01_power_on(struct dsim_device *dsim, int on)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:%s\n", __func__, on ? "ON":"OFF");

	if (on) {
		if (lcd->lp_mode)
			goto out;

		s6e36w2x01_set_panel_power(dsim, on);

		usleep_range(10000, 11000);

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on)
			goto out;
	} else {
		s6e36w2x01_set_panel_power(dsim, on);
		lcd->panel_sc_state = false;
		lcd->power = FB_BLANK_POWERDOWN;
	}

out:

	return 0;
}

extern int generate_volt_table(struct dim_data *data);
static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, struct dim_data *dimData, u8 * hbm)
{
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;
	int i = 0;
	memset(result, 0, OLED_CMD_GAMMA_CNT);

	result[index++] = OLED_CMD_GAMMA;

	while (index < OLED_CMD_GAMMA_CNT) {
		if (((i >= 50) && (i < 65)) || ((i >= 68) && (i < 88)))
			result[index++] = hbm[i];
		i++;
	}
	for (i = 0; i < OLED_CMD_GAMMA_CNT; i++)
		dsim_info("%d : %d\n", i + 1, result[i]);
	return ret;
}

static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= 0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = (unsigned char)((center_gamma[i][j] >> 8) & 0x01);
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
			else {
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
		}
	}
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}

extern int cal_gamma_from_index(struct dim_data *data, struct SmtDimInfo *brInfo);

static int s6e36w2x01_init_dimming(struct dsim_device *dsim, u8 * mtp, u8 * hbm)
{
	int i, j, k;
	int pos = 0;
	int ret = 0;
	short temp;
	int method = 0;
	static struct dim_data *dimming = NULL;

	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	int string_offset;
	char string_buf[1024];

	if (dimming == NULL) {
		dimming = (struct dim_data *)kmalloc(sizeof(struct dim_data), GFP_KERNEL);
		if (!dimming) {
			dsim_err("failed to allocate memory for dim data\n");
			ret = -ENOMEM;
			goto error;
		}
	}

	diminfo = (void *)dimming_info;

	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos + 1];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= 0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
	/* for vt */
	temp = (mtp[pos + 1]) << 8 | mtp[pos];

	for (i = 0; i < CI_MAX; i++)
		dimming->vt_mtp[i] = (temp >> (i * 4)) & 0x0f;
#ifdef SMART_DIMMING_DEBUG
	dsim_info("Center Gamma Info : \n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			  dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			  dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
	}
#endif
	dsim_info("VT MTP : \n");
	dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
		  dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
		  dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

	dsim_info("MTP Info : \n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			  dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			  dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE]);
	}

	ret = generate_volt_table(dimming);
	if (ret) {
		dsim_info("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;

		if (method == DIMMING_METHOD_FILL_CENTER) {
			ret = set_gamma_to_center(&diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to get center gamma\n", __func__);
				goto error;
			}
		}
		else if (method == DIMMING_METHOD_FILL_HBM) {
			ret = set_gamma_to_hbm(&diminfo[i], dimming, hbm);
			if (ret) {
				dsim_err("%s : failed to get hbm gamma\n", __func__);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;
		if (method == DIMMING_METHOD_AID) {
			ret = cal_gamma_from_index(dimming, &diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
	}

	for (i = 0; i < (MAX_BR_INFO-1); i++) {
		memset(string_buf, 0, sizeof(string_buf));
		string_offset = sprintf(string_buf, "gamma[%3d] : ", diminfo[i].br);

		for (j = 0; j < GAMMA_CMD_CNT; j++){
			string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[i].gamma[j]);
			gamma_set[i][j] = diminfo[i].gamma[j];
		}
		//dsim_info("[LCD] %s\n", string_buf);
	}

	for (k = 0; k < GAMMA_CMD_CNT; k++){
		diminfo[73].gamma[k] = MAX_GAMMA_SET[k];
		string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[73].gamma[k]);
		gamma_set[73][k] = diminfo[73].gamma[k];
	}

error:
	return ret;

}
#if 0
static void s6e3ha3_init_default_info(struct dsim_device *dsim)
{
	struct panel_private *panel = &dsim->priv;

	panel->elvss_len = S6E36W2_ELVSS_LEN + 1;
	panel->elvss_start_offset = S6E36W2_ELVSS_START;
	panel->elvss_temperature_offset = S6E36W2_ELVSS_TEMPERATURE_POS;
	panel->elvss_tset_offset = S6E36W2_ELVSS_TSET_POS;

	memset(panel->tset_set, 0, sizeof(panel->tset_set));
	panel->tset_len = 0;	// elvss set include tset

	memcpy(panel->aid_set, SEQ_AOR_CONTROL, S6E36W2_AID_CMD_CNT);
	panel->aid_len = S6E36W2_AID_LEN + 1;
	panel->aid_reg_offset = S6E36W2_AID_LEN - 1;

	memcpy(panel->vint_set, SEQ_VINT_SET, sizeof(SEQ_VINT_SET));
	panel->vint_len = S6E36W2_VINT_LEN + 1;
	memcpy(panel->vint_table, VINT_TABLE, sizeof(VINT_TABLE));
	memcpy(panel->vint_dim_table, VINT_DIM_TABLE, sizeof(VINT_DIM_TABLE));
	panel->vint_table_len = ARRAY_SIZE(VINT_DIM_TABLE);

	pr_info("%s : init default value\n", __func__);
}
#endif

static int s6e36w2x01_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
	int     i = 0;
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char buf[S6E36W2_MTP_DATE_SIZE] = { 0, };
	unsigned char bufForCoordi[S6E36W2_COORDINATE_LEN] = { 0, };
	dsim->id = 0;

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE, (unsigned long) TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W2_ID_REG, S6E36W2_ID_LEN, panel->id);
	if (ret < 0) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		goto read_exit;
	}
	dsim_info("READ ID : ");
	for (i = 0; i < S6E36W2_ID_LEN; i++)
		dsim_info("%02x, ", panel->id[i]);
	dsim_info("\n");

	dsim->priv.current_model = (dsim->priv.id[1] >> 4) & 0x0F;
	dsim->priv.panel_rev = dsim->priv.id[2] & 0x0F;
	dsim->priv.panel_line = (dsim->priv.id[0] >> 6) & 0x03;
	dsim->priv.panel_material = dsim->priv.id[1] & 0x01;
	dsim_info("%s model is %d, panel rev : %d, panel line : %d panel material : %d\n",
		__func__, dsim->priv.current_model, dsim->priv.panel_rev, dsim->priv.panel_line, dsim->priv.panel_material);

	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W2_MTP_ADDR, S6E36W2_MTP_DATE_SIZE, buf);
	if (ret < 0) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E36W2_MTP_SIZE);
	memcpy(panel->date, &buf[40], ARRAY_SIZE(panel->date));
	dsim_info("READ MTP SIZE : %d\n", S6E36W2_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E36W2_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

        // coordinate
    ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W2_COORDINATE_REG, S6E36W2_COORDINATE_LEN, bufForCoordi);
	if (ret < 0) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", panel->coordinate[i]);
	dsim_info("\n");

        // code
    ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W2_CODE_REG, S6E36W2_CODE_LEN, panel->code);
	if (ret < 0) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for (i = 0; i < S6E36W2_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");
	// tset
	panel->tset_set[0] = S6E36W2_TSET_REG;
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W2_TSET_REG, S6E36W2_TSET_LEN, &(panel->tset_set[1]));
	if (ret < 0) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ tset : ");
	for (i = 0; i <= S6E36W2_TSET_LEN; i++)
		dsim_info("%x, ", panel->tset_set[i]);
	dsim_info("\n");

	 // elvss
	panel->elvss_set[0] = S6E36W2_ELVSS_REG;
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W2_ELVSS_REG, S6E36W2_ELVSS_LEN, &(panel->elvss_set[1]));
	if (ret < 0) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
    dsim_info("READ elvss : ");
	for (i = 0; i <= S6E36W2_ELVSS_LEN; i++)
		dsim_info("%x \n", panel->elvss_set[i]);

	ret = dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE, (unsigned long) TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
    if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto read_exit;
    }
    ret = 0;
read_exit:
	return 0;

read_fail:
	return -ENODEV;
}

#define	TIME_ZONE_VALUE	9*60*60
extern void rtc_time_to_tm(unsigned long time, struct rtc_time *tm);

static void s6e36w2x01_comp_time_by_rate(struct s6e36w2x01 *lcd, int *h, int *m, int *s, int *ms);
static void s6e36w2x01_comp_time_by_diff(struct s6e36w2x01 *lcd)
{
	int time, diff_time, h, m, s, ms;

	if (!lcd->sc_time.diff)
		return;

	h = lcd->sc_time.sc_h;
	m = lcd->sc_time.sc_m;
	s = lcd->sc_time.sc_s;
	ms = lcd->sc_time.sc_ms;

	time = ktime_to_ms(ktime_get());

	diff_time = time - lcd->sc_time.diff;

	pr_info("%s:%2d:%2d:%2d.%d (diff:%dms)\n", "comp_time_by_diff", h, m, s, ms, diff_time);

	lcd->sc_time.diff = 0;

	if (diff_time <= 0)
		return;

	ms += (diff_time % 1000);
	s += (diff_time /1000);

	if (ms > 999) {
		s += (ms /1000);
		ms %= 1000;
	}

	if (s > 59) {
		m += (s/60);
		s %=60;
	}

	if (m > 59) {
		h += (s/60);
		m %=60;
	}

	if (h > 11)
		h %=12;

	s6e36w2x01_comp_time_by_rate(lcd, &h, &m, &s, &ms);

	s6e36w2x01_set_sc_config(SC_SET_HH, h);
	s6e36w2x01_set_sc_config(SC_SET_MM, m);
	s6e36w2x01_set_sc_config(SC_SET_SS, s);
	s6e36w2x01_set_sc_config(SC_SET_MSS, ms/SC_MSEC_DIVIDER);
}

static void s6e36w2x01_comp_time_by_rate(struct s6e36w2x01 *lcd, int *h, int *m, int *s, int *ms)
{
	unsigned int divider, value;

	pr_info("%s:before:%02d:%02d:%02d.%03d\n", "comp_time_by_rate", *h, *m, *s, *ms);

	if ((*ms > 999) || (*ms < 0))
		*ms = 0;

	if (lcd->sc_rate < SC_MIN_UPDATE_RATE)
		lcd->sc_rate = SC_MAX_UPDATE_RATE;

	value = *ms + ((1000/lcd->sc_rate)/2);
	divider = (SC_MAX_UPDATE_RATE / lcd->sc_rate) * SC_MSEC_DIVIDER;
	*ms = (value /divider) * divider;

	if (*ms > 999) {
		*s += (*ms /1000);
		*ms %= 1000;
	}

	if (*s > 59) {
		*m += (*s/60);
		*s %=60;
	}

	if (*m > 59) {
		*h += (*m/60);
		*m %=60;
	}

	if (*h > 11)
		*h %=12;

	pr_info("%s:after:%02d:%02d:%02d.%03d\n", "comp_time_by_rate", *h, *m, *s, *ms);
}

static void s6e36w2x01_set_sc_center(int x, int y)
{
	if ((x > SC_MAX_POSITION) || (x < SC_MIN_POSITION))
		x = SC_DEFAULT_POSITION;
	if ((y > SC_MAX_POSITION) || (y < SC_MIN_POSITION))
		y = SC_DEFAULT_POSITION;

	s6e36w2x01_set_sc_config(SC_CENTER_X, x);
	s6e36w2x01_set_sc_config(SC_CENTER_Y, y);

	pr_info("%s: x:%d, y:%d\n", "set_sc_center", x, y);
}

static void s6e36w2x01_update_needle_data(struct s6e36w2x01 *lcd, unsigned int *addr)
{
	long int source_addr;
	int i, j, k;

	pr_info("%s\n", "update_needle_data");

	for (i = 0; i < SC_NEEDLE_SECTION; i++) {
		if (!addr[i])
			pr_info("%s: Needle section%d is NULL\n", "update_needle_data", i);
	}

	for (i = 0, k = 0; k < SC_NEEDLE_SECTION; k++) {
		for(j = 0; j < SC_NEEDLE_HIGHT; j++) {
			if (i == 0)
				lcd->sc_buf[i] = SC_START_CMD;
			else
				lcd->sc_buf[i] = SC_IMAGE_CMD;
			i++;

			if (addr[k])
				source_addr = (SC_NEEDLE_WIDTH*j) +
					addr[k];
			else
				source_addr = (long int)lcd->sc_dft_buf;

			memcpy(&lcd->sc_buf[i], (char*)(source_addr), SC_NEEDLE_WIDTH);

			i += SC_NEEDLE_WIDTH;
		}
	}

	s6e36w2x01_write_sideram_data(lcd);
}

extern int s6e36w2x01_get_sc_register(int cmd);
static void s6e36w2x01_set_sc_time_update(struct s6e36w2x01 *lcd, int h, int m, int s, int ms)
{
	int time_diff;
	unsigned int kernel_time;
	unsigned int panel_time;
	int panel_h, panel_m, panel_s, panel_ms;
	int ret, clk_en;
	int need_time;

	if (ms > 9)
		ms /= SC_MSEC_DIVIDER;

	clk_en = s6e36w2x01_get_sc_register(SC_ANA_CLOCK_EN);
	if (clk_en < 0) {
		pr_err("%s:Failed to get SC_ANA_CLOCK_EN[%d]\n", __func__, clk_en);
		return;
	} else if (!clk_en) {
		pr_debug("%s:Don't need time update.\n", __func__);
		return;
	}

	kernel_time = (h*60*60*10) + (m*60*10) + (s *10) + ms;

	ret = s6e36w2x01_get_sc_time(&panel_h, &panel_m, &panel_s, &panel_ms);
	if (ret)
		pr_err("%s: Failed to get panel time[%d]\n", __func__, ret);

	panel_time = (panel_h*60*60*10) + (panel_m*60*10) + (panel_s *10) + panel_ms;

	time_diff = kernel_time - panel_time;
	if (time_diff < 0)
		time_diff *= -1;

	if (!lcd->sc_rate)
		lcd->sc_rate = 1;

	need_time = time_diff * SC_COMP_SPEED /lcd->sc_rate;

	pr_info("%s: time_diff:%d.%d sec, comp_need:%d.%d sec\n",
		"set_sc_time_update", time_diff/10, time_diff%10,
		need_time/10, need_time%10);

	if (need_time < SC_COMP_NEED_TIME)
		s6e36w2x01_set_sc_config(SC_COMP_EN, true);
	else
		s6e36w2x01_set_sc_config(SC_COMP_EN, false);

	s6e36w2x01_set_sc_config(SC_TIME_UPDATE, true);
}

static void s6e36w2x01_set_sc_time(struct s6e36w2x01 *lcd, int h, int m, int s, int ms)
{
	lcd->sc_time.sc_h = h;
	lcd->sc_time.sc_m = m;
	lcd->sc_time.sc_s = s;
	lcd->sc_time.sc_ms = ms;

	s6e36w2x01_comp_time_by_rate(lcd, &h, &m, &s, &ms);

	s6e36w2x01_set_sc_config(SC_SET_HH, h);
	s6e36w2x01_set_sc_config(SC_SET_MM, m);
	s6e36w2x01_set_sc_config(SC_SET_SS, s);
	s6e36w2x01_set_sc_config(SC_SET_MSS, ms/SC_MSEC_DIVIDER);

	s6e36w2x01_set_sc_time_update(lcd, h, m, s, ms/SC_MSEC_DIVIDER);
}

static void s6e36w2x01_set_sc_update_rate(struct s6e36w2x01 *lcd, unsigned int rate)
{
	const int divider = SC_PANEL_VSYNC_RATE / SC_MAX_UPDATE_RATE;
	int step, count;

	if (rate < SC_MIN_UPDATE_RATE)
		rate = SC_MAX_UPDATE_RATE;

	if (rate > SC_MAX_UPDATE_RATE)
		rate = SC_MAX_UPDATE_RATE;

	for (; rate <= SC_MAX_UPDATE_RATE; rate++) {
		if ((SC_PANEL_VSYNC_RATE % rate) == 0) {
			count = SC_PANEL_VSYNC_RATE / rate;
			if ((count % divider) == 0) {
				step = (count /divider);
				if (step != 0)
					break;
			}
		}
	}
	lcd->sc_rate = rate;
	s6e36w2x01_set_sc_config(SC_INC_STEP, step);
	s6e36w2x01_set_sc_config(SC_UPDATE_RATE, count);

	pr_info("%s:count:%d, step:%d, rate:%d\n", "set_sc_update_rate", count, step, rate);
}

static void s6e36w2x01_mdnie_restore(struct s6e36w2x01 *lcd, bool aod_state)
{
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	if ((mdnie->scenario == SCENARIO_UI) ||
	(mdnie->scenario == SCENARIO_GRAY))
		return;

	if (aod_state) {
		switch (mdnie->scenario) {
		case SCENARIO_GRAY:
		case SCENARIO_GRAY_NEGATIVE:
			s6e36w2x01_mdnie_set(SCENARIO_GRAY);
			break;
		case SCENARIO_UI:
		case SCENARIO_GALLERY:
		case SCENARIO_VIDEO:
		case SCENARIO_VTCALL:
		case SCENARIO_CAMERA:
		case SCENARIO_BROWSER:
		case SCENARIO_NEGATIVE:
		case SCENARIO_EMAIL:
		case SCENARIO_EBOOK:
		case SCENARIO_CURTAIN:
		default:
			s6e36w2x01_mdnie_set(SCENARIO_UI);
			break;
		}
		usleep_range(40000, 41000);
	} else {
		usleep_range(40000, 41000);
		s6e36w2x01_mdnie_set(mdnie->scenario);
	}
}

static int s6e36w2x01_aod_ctrl(struct dsim_device *dsim, int enable)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:enable[%d]aod_mode[%d]\n", "aod_ctrl", enable, lcd->aod_mode);

	lcd->aod_enable = enable;

	if (lcd->aod_enable) {
		switch (lcd->aod_mode) {
		case AOD_SCLK_ANALOG:
			if (!lcd->panel_sc_state) {
				s6e36w2x01_write_sideram_data(lcd);
			}
			s6e36w2x01_comp_time_by_diff(lcd);
			s6e36w2x01_set_sc_config(SC_ANA_CLOCK_EN, 1);
			s6e36w2x01_mdnie_restore(lcd, lcd->aod_enable);
			s6e36w2x01_hlpm_ctrl(lcd, true);
			s6e36w2x01_write_sc_config();
			break;
		case AOD_SCLK_DIGITAL:
			s6e36w2x01_set_sc_config(SB_BLK_EN, 1);
			s6e36w2x01_mdnie_restore(lcd, lcd->aod_enable);
			s6e36w2x01_hlpm_ctrl(lcd, true);
			s6e36w2x01_write_sc_config();
			break;
		default:
			s6e36w2x01_mdnie_restore(lcd, lcd->aod_enable);
			s6e36w2x01_hlpm_ctrl(lcd, true);
			break;
		}
	}else {
		switch (lcd->aod_mode) {
		case AOD_SCLK_ANALOG:
			s6e36w2x01_set_sc_config(SC_ANA_CLOCK_EN, 0);
			s6e36w2x01_disable_sc_config();
			s6e36w2x01_hlpm_ctrl(lcd, false);
			s6e36w2x01_mdnie_restore(lcd, lcd->aod_enable);
			break;
		case AOD_SCLK_DIGITAL:
			s6e36w2x01_set_sc_config(SB_BLK_EN, 0);
			s6e36w2x01_disable_sc_config();
			s6e36w2x01_hlpm_ctrl(lcd, false);
			s6e36w2x01_mdnie_restore(lcd, lcd->aod_enable);
			break;
		default:
			s6e36w2x01_hlpm_ctrl(lcd, false);
			s6e36w2x01_mdnie_restore(lcd, lcd->aod_enable);
			break;
		}
	}

	return 0;
}

static int s6e36w2x01_metadata_set(struct dsim_device *dsim, struct decon_metadata *metadata)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);
	struct aod_config *aod_cfg = &metadata->aod_cfg;
	struct sclk_analog_cfg ana_cfg = aod_cfg->analog_cfg;
	struct sclk_digital_cfg dig_cfg  = aod_cfg->digital_cfg;
	int ret = 0;

	switch (metadata->ops) {
	case METADATA_OP_AOD_SET_INFO: {
		pr_info("%s:req[%d]\n", "metadata_set", aod_cfg->req);

		switch (aod_cfg->req) {
		case AOD_SET_CONFIG:
			pr_info("%s:set_config:aod_mode[%d]\n", "metadata_set", aod_cfg->mode);

			switch (aod_cfg->mode) {
			case AOD_DISABLE:
			case AOD_ALPM:
			case AOD_HLPM:
				break;
			case AOD_SCLK_ANALOG: {
				pr_info("%s:pos[%d %d]ts[%d %d %d %d]rate[%d]buf[0x%x 0x%x 0x%x]\n",
					"analog", ana_cfg.pos[0], ana_cfg.pos[1],
					ana_cfg.timestamp[0], ana_cfg.timestamp[1],
					ana_cfg.timestamp[2], ana_cfg.timestamp[3],
					ana_cfg.rate, ana_cfg.addr[0], ana_cfg.addr[1],  ana_cfg.addr[2]);
				s6e36w2x01_update_needle_data(lcd, &ana_cfg.addr[0]);
				break;
			}
			case AOD_SCLK_DIGITAL: {
				s6e36w2x01_set_sc_config(SB_RADIUS, dig_cfg.circle_r);
				s6e36w2x01_set_sc_config(SB_CIRCLE_1_X, dig_cfg.circle1[0]);
				s6e36w2x01_set_sc_config(SB_CIRCLE_1_Y, dig_cfg.circle1[1]);
				s6e36w2x01_set_sc_config(SB_CIRCLE_2_X, dig_cfg.circle2[0]);
				s6e36w2x01_set_sc_config(SB_CIRCLE_2_Y, dig_cfg.circle2[1]);
				s6e36w2x01_set_sc_config(SB_LINE_COLOR_R, dig_cfg.color[0]);
				s6e36w2x01_set_sc_config(SB_LINE_COLOR_G, dig_cfg.color[1]);
				s6e36w2x01_set_sc_config(SB_LINE_COLOR_B, dig_cfg.color[2]);
				break;
			}
			default:
				pr_err("%s:invalid type[%d]\n", __func__, aod_cfg->mode);
				ret = -EINVAL;
				break;
			}

			if (!ret)
				lcd->aod_mode = aod_cfg->mode;
			break;
		case AOD_UPDATE_DATA: {
			pr_info("%s:update_data:aod_mode[%d]\n", "metadata_set", aod_cfg->mode);

			switch (aod_cfg->mode) {
			case AOD_DISABLE:
			case AOD_ALPM:
			case AOD_HLPM:
				break;
			case AOD_SCLK_ANALOG: {
				lcd->sc_time.diff = ktime_to_ms(ktime_get());
				s6e36w2x01_set_sc_center(ana_cfg.pos[0], ana_cfg.pos[1]);
				s6e36w2x01_set_sc_update_rate(lcd, ana_cfg.rate);
				s6e36w2x01_set_sc_time(lcd, ana_cfg.timestamp[0], ana_cfg.timestamp[1],
						ana_cfg.timestamp[2], ana_cfg.timestamp[3]);
				if (s6e36w2x01_get_sc_config(SC_ANA_CLOCK_EN)) {
					s6e36w2x01_write_sc_config();
					if (s6e36w2x01_get_sc_config(SC_TIME_UPDATE)) {
						usleep_range(SC_1FRAME_USEC, SC_1FRAME_USEC); /* wait 1 frame */
						s6e36w2x01_set_sc_config(SC_TIME_UPDATE, false);
						s6e36w2x01_write_sc_config();
					}
				}
				break;
			}
			case AOD_SCLK_DIGITAL: {
				s6e36w2x01_set_sc_config(SB_RADIUS, dig_cfg.circle_r);
				s6e36w2x01_set_sc_config(SB_CIRCLE_1_X, dig_cfg.circle1[0]);
				s6e36w2x01_set_sc_config(SB_CIRCLE_1_Y, dig_cfg.circle1[1]);
				s6e36w2x01_set_sc_config(SB_CIRCLE_2_X, dig_cfg.circle2[0]);
				s6e36w2x01_set_sc_config(SB_CIRCLE_2_Y, dig_cfg.circle2[1]);
				s6e36w2x01_set_sc_config(SB_LINE_COLOR_R, dig_cfg.color[0]);
				s6e36w2x01_set_sc_config(SB_LINE_COLOR_G, dig_cfg.color[1]);
				s6e36w2x01_set_sc_config(SB_LINE_COLOR_B, dig_cfg.color[2]);
				if (s6e36w2x01_get_sc_config(SB_BLK_EN)) {
					s6e36w2x01_write_sc_config();
				}
				break;
			}
			default:
				pr_err("%s:invalid type[%d]\n", __func__, aod_cfg->mode);
				ret = -EINVAL;
				break;
			}
			break;
		}
		default:
			pr_err("%s:invalid req[%d]\n", __func__, aod_cfg->req);
			ret = -EINVAL;
		}

		break;
	}
	default:
		pr_err("%s:invalid ops[%d]\n", __func__, metadata->ops);
		ret = -EINVAL;
		break;
	}

	pr_info("%s:ops[%d]ret[%d]\n", "metadata_set", metadata->ops, ret);

	return ret;
}

#ifdef CONFIG_SLEEP_MONITOR
int s6e36w2x01_sleep_monitor_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	struct s6e36w2x01 *lcd = priv;
	int state = DEVICE_UNKNOWN;
	int mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) - 1;
	unsigned int cnt = 0;
	int ret;

	switch (lcd->power) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND: {
		if (lcd->aod_enable)
			state = DEVICE_ON_LOW_POWER;
		else if (lcd->hbm_on)
			state = DEVICE_ON_ACTIVE2;
		else
			state = DEVICE_ON_ACTIVE1;
		break;
	}
	case FB_BLANK_POWERDOWN:
	default:
		state = DEVICE_POWER_OFF;
		break;
	}

	switch (caller_type) {
	case SLEEP_MONITOR_CALL_SUSPEND:
	case SLEEP_MONITOR_CALL_RESUME:
		cnt = lcd->act_cnt;
		if (state == DEVICE_ON_LOW_POWER)
			cnt++;
		break;
	case SLEEP_MONITOR_CALL_ETC:
		break;
	default:
		break;
	}

	*raw_val = cnt | state << 24;

	/* panel on count 1~15*/
	if (cnt > mask)
		ret = mask;
	else
		ret = cnt;

	pr_debug("%s: caller[%d], dpms[%d] panel on[%d], raw[0x%x]\n",
			__func__, caller_type, lcd->power, ret, *raw_val);

	lcd->act_cnt = 0;

	return ret;
}

static struct sleep_monitor_ops s6e36w2x01_sleep_monitor_ops = {
	.read_cb_func = s6e36w2x01_sleep_monitor_cb,
};
#endif


static int s6e36w2x01_probe(struct dsim_device *dsim)
{
	struct s6e36w2x01 *lcd;
	int start = 0, end, i, offset = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	struct dsim_resources *res = &dsim->res;
	unsigned char mtp[S6E36W2_MTP_SIZE] = { 0, };
	unsigned char hbm[S6E36W2_HBMGAMMA_LEN] = { 0, };
	panel->dim_data = (void *) NULL;
	panel->lcdConnected = PANEL_CONNECTED;

	pr_err("%s\n", __func__);

	if (!get_panel_id()) {
		pr_err("No lcd attached!\n");
		return -ENODEV;
	}

	lcd = devm_kzalloc(dsim->dev,
				sizeof(struct s6e36w2x01), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate s6e36w2x01 structure.\n");
		return -ENOMEM;
	}

	lcd->dev = dsim->dev;
	mutex_init(&lcd->mipi_lock);

//	lcd->pd = (struct lcd_platform_data *)dsim_dev->platform_data;
//	lcd->boot_power_on = lcd->pd->lcd_enabled;

	lcd->dimming = devm_kzalloc(dsim->dev,
				sizeof(*lcd->dimming), GFP_KERNEL);
	if (!lcd->dimming) {
		pr_err("failed to allocate dimming.\n");
		ret = -ENOMEM;
		goto err_free_lcd;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i] = (unsigned char *)
			kzalloc(sizeof(unsigned char) * GAMMA_CMD_CNT,
			GFP_KERNEL);
		if (!lcd->gamma_tbl[i]) {
			pr_err("failed to allocate gamma_tbl\n");
			ret = -ENOMEM;
			goto err_free_dimming;
		}
	}

	lcd->br_map = devm_kzalloc(dsim->dev,
		sizeof(unsigned char) * (MAX_BRIGHTNESS + 1), GFP_KERNEL);
	if (!lcd->br_map) {
		pr_err("failed to allocate br_map\n");
		ret = -ENOMEM;
		goto err_free_gamma_tbl;
	}

	for (i = 0; i < DIMMING_COUNT; i++) {
		end = br_convert[offset++];
		memset(&lcd->br_map[start], i, end - start + 1);
		start = end + 1;
	}

	lcd->bd = backlight_device_register(BACKLIGHT_DEV_NAME, lcd->dev, lcd,
		&s6e36w2x01_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		printk(KERN_ALERT "failed to register backlight device!\n");
		goto err_free_br_map;
	}

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;

	dsim->lcd = lcd_device_register(LCD_DEV_NAME, lcd->dev, lcd,
						&s6e36w2x01_lcd_ops);
	if (IS_ERR(dsim->lcd)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(dsim->lcd);
		goto err_unregister_bd;
	}
	lcd->ld = dsim->lcd;

	lcd->sc_buf= devm_kzalloc(dsim->dev,
		sizeof(char) * SC_RAM_WIDTH * SC_RAM_HIGHT, GFP_KERNEL);
	if (!lcd->sc_buf) {
		pr_err("%s: failed to allocate sc_buf\n", __func__);
		ret = -ENOMEM;
		goto err_free_SC;
	}

	lcd->sc_dft_buf= devm_kzalloc(dsim->dev,
			sizeof(char) * SC_NEEDLE_WIDTH, GFP_KERNEL);
	if (!lcd->sc_dft_buf) {
		pr_err("%s: failed to allocate sc_buf\n", __func__);
		ret = -ENOMEM;
		goto err_free_SC_DFT;
	}
	memset(lcd->sc_dft_buf, SC_DEFAULT_DATA, sizeof(char) * SC_NEEDLE_WIDTH);

	esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(esd_class)) {
		dev_err(lcd->dev, "Failed to create class(lcd_event)!\n");
		ret = PTR_ERR(esd_class);
		goto err_unregister_lcd;
	}

	esd_dev = device_create(esd_class, NULL, 0, NULL, "esd");

	INIT_WORK(&lcd->det_work, s6e36w2x01_esd_detect_work);

	if (res->oled_det > 0) {
		lcd->esd_irq = gpio_to_irq(res->oled_det);
		dev_info(lcd->dev, "esd_irq_num [%d]\n", lcd->esd_irq);
		ret = devm_request_irq(lcd->dev, lcd->esd_irq,
					s6e36w2x01_esd_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "oled_det",
					lcd);
		if (ret < 0) {
			dev_err(lcd->dev, "failed to request det irq.\n");
			goto err_unregister_lcd;
		}
	}

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, " failed to create lcd_type sysfs.\n");
		goto err_unregister_lcd;
	}

	for (i = 0; i < ARRAY_SIZE(ld_dev_attrs); i++) {
		ret = device_create_file(lcd->dev,
				&ld_dev_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add ld dev sysfs entries\n");
			for (i--; i >= 0; i--)
				device_remove_file(lcd->dev,
					&ld_dev_attrs[i]);
			goto err_remove_lcd_type_file;
		}
	}

	ret = s6e36w2x01_read_init_info(dsim, mtp, hbm);
	if (ret)
		dev_err(&lcd->ld->dev, "%s : failed to generate gamma tablen\n", __func__);

	ret = s6e36w2x01_init_dimming(dsim, mtp, hbm);

	s6e36w2x01_mdnie_lite_init(lcd);
	s6e36w2x01_set_panel_power(dsim, true);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(lcd, &s6e36w2x01_sleep_monitor_ops,
		SLEEP_MONITOR_LCD);
	lcd->act_cnt = 1;
#endif

	return 0;

err_remove_lcd_type_file:
	device_remove_file(&lcd->ld->dev, &dev_attr_lcd_type);
err_unregister_lcd:
	devm_kfree(dsim->dev, lcd->sc_dft_buf);
err_free_SC_DFT:
	devm_kfree(dsim->dev, lcd->sc_buf);
err_free_SC:
	lcd_device_unregister(lcd->ld);
err_unregister_bd:
	backlight_device_unregister(lcd->bd);
err_free_br_map:
	devm_kfree(dsim->dev, lcd->br_map);
err_free_gamma_tbl:
	for (i = 0; i < MAX_GAMMA_CNT; i++)
		if (lcd->gamma_tbl[i])
			devm_kfree(dsim->dev, lcd->gamma_tbl[i]);
err_free_dimming:
	devm_kfree(dsim->dev, lcd->dimming);
err_free_lcd:
	mutex_destroy(&lcd->mipi_lock);
	devm_kfree(dsim->dev, lcd);

	return ret;
}

extern void s6e36w2x01_disable(void);
static int s6e36w2x01_suspend(struct dsim_device *dsim)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);
	struct dsim_resources *res = &dsim->res;

	if (res->oled_det > 0) {
		disable_irq(lcd->esd_irq);
	}

	s6e36w2x01_disable();

	return 1;
}

struct mipi_dsim_lcd_driver s6e36w2x01_mipi_lcd_driver = {
	.probe		= s6e36w2x01_probe,
	.displayon	= s6e36w2x01_displayon,
	.suspend		= s6e36w2x01_suspend,
	.power_on	= s6e36w2x01_power_on,
	.reset_on		= s6e36w2x01_reset_ctrl,
	.aod_ctrl		=  s6e36w2x01_aod_ctrl,
	.metadata_set	= s6e36w2x01_metadata_set,
};
