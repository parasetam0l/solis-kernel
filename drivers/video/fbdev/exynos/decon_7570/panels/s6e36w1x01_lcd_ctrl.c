/* s6e3ha2k_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e36w1x01_dimming.h"
#include "s6e36w1x01_param.h"
#include "s6e36w1x01_mipi_lcd.h"

/* use FW_TEST definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "../dsim_fw.h"
#include "mipi_display.h"
#else
#include "../dsim.h"
#include <video/mipi_display.h>
#endif

#define LDI_ID_REG	0x04
#define LDI_ID_LEN	3

#define ID		0

#define VIDEO_MODE      1
#define COMMAND_MODE    0

static const unsigned char *aor_tbl[DIMMING_COUNT] = {
	panel_aor_rate_10, panel_aor_rate_11, panel_aor_rate_12,
	panel_aor_rate_13, panel_aor_rate_14, panel_aor_rate_15,
	panel_aor_rate_16, panel_aor_rate_17, panel_aor_rate_19,
	panel_aor_rate_20, panel_aor_rate_21, panel_aor_rate_22,
	panel_aor_rate_24, panel_aor_rate_25, panel_aor_rate_27,
	panel_aor_rate_29, panel_aor_rate_30, panel_aor_rate_32,
	panel_aor_rate_34, panel_aor_rate_37, panel_aor_rate_39,
	panel_aor_rate_41, panel_aor_rate_44, panel_aor_rate_47,
	panel_aor_rate_50, panel_aor_rate_53, panel_aor_rate_56,
	panel_aor_rate_60, panel_aor_rate_64, panel_aor_rate_68,
	panel_aor_rate_72, panel_aor_rate_77, panel_aor_rate_82,
	panel_aor_rate_87, panel_aor_rate_93, panel_aor_rate_98,
	panel_aor_rate_105, panel_aor_rate_111, panel_aor_rate_119,
	panel_aor_rate_126, panel_aor_rate_134, panel_aor_rate_143,
	panel_aor_rate_152, panel_aor_rate_162, panel_aor_rate_172,
	panel_aor_rate_183, panel_aor_rate_195, panel_aor_rate_207,
	panel_aor_rate_220, panel_aor_rate_234, panel_aor_rate_249,
	panel_aor_rate_265, panel_aor_rate_282, panel_aor_rate_300,
	panel_aor_rate_316, panel_aor_rate_333, panel_aor_rate_360
};

static const unsigned char *elvss_tbl[DIMMING_COUNT] = {
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_82,
	panel_elvss_87, panel_elvss_93, panel_elvss_98,
	panel_elvss_105, panel_elvss_111, panel_elvss_119,
	panel_elvss_126, panel_elvss_134, panel_elvss_143,
	panel_elvss_152, panel_elvss_162, panel_elvss_172,
	panel_elvss_183, panel_elvss_195, panel_elvss_207,
	panel_elvss_220, panel_elvss_234, panel_elvss_249,
	panel_elvss_265, panel_elvss_282, panel_elvss_300,
	panel_elvss_316, panel_elvss_333, panel_elvss_360
};

void s6e36w1x01_display_init(struct decon_lcd * lcd)
{
	/* Test key enable */
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_1,
		ARRAY_SIZE(TEST_KEY_ON_1)) < 0)
		dsim_err("failed to send TEST_KEY_ON_1.\n");

	/* sleep out */
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) SLPOUT,
		ARRAY_SIZE(SLPOUT)) < 0)
		dsim_err("failed to send SLPOUTs.\n");

	/* 120ms delay */
	msleep(120);

	/* Module Information Read */
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) GAMMA_360,
		ARRAY_SIZE(GAMMA_360)) < 0)
		dsim_err("failed to send GAMMA_360.\n");

	/* Brightness Setting */
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) AOR_360,
		ARRAY_SIZE(AOR_360)) < 0)
		dsim_err("failed to send AOR_360.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ELVSS_360,
		ARRAY_SIZE(ELVSS_360)) < 0)
		dsim_err("failed to send ELVSS_360.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) VINT_360,
		ARRAY_SIZE(VINT_360)) < 0)
		dsim_err("failed to send VINT_360.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) HIDDEN_KEY_ON,
		ARRAY_SIZE(HIDDEN_KEY_ON)) < 0)
		dsim_err("failed to send HIDDEN_KEY_ON.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ETC_GPARA,
		ARRAY_SIZE(ETC_GPARA)) < 0)
		dsim_err("failed to send ETC_GPARA.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ETC_SET,
		ARRAY_SIZE(ETC_SET)) < 0)
		dsim_err("failed to send ETC_SET.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) HIDDEN_KEY_OFF,
		ARRAY_SIZE(HIDDEN_KEY_OFF)) < 0)
		dsim_err("failed to send HIDDEN_KEY_OFF.\n");

	/* Test key disable */
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_1,
		ARRAY_SIZE(TEST_KEY_OFF_1)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_1.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEON,
		ARRAY_SIZE(TEON)) < 0)
		dsim_err("failed to send TEON.\n");

	/* display on */
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) DISPON,
		ARRAY_SIZE(DISPON)) < 0)
		dsim_err("failed to send DISPLAY_ON.\n");
}

void s6e36w1x01_enable(void)
{
}

void s6e36w1x01_disable(void)
{
	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)DISPOFF,
		ARRAY_SIZE(DISPOFF)) < 0)
		dsim_err("fail to write DISPLAY_OFF.\n");

	if (dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)SLPIN,
		ARRAY_SIZE(SLPIN)) < 0)
		dsim_err("fail to write SLEEP_IN.\n");

	/* 120ms delay */
	msleep(120);
}

int s6e36w1x01_gamma_ctrl(u32 level)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd = dsim->lcd;
	struct s6e36w1x01 *panel = dev_get_drvdata(&lcd->dev);

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return 0;
	}

	dsim_info("%s: level:[%d]\n", __func__, level);

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)panel->gamma_tbl[level],
		GAMMA_CMD_CNT) < 0)
		dsim_err("failed to send gamma_tbl.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)aor_tbl[level],
		GAMMA_CMD_CNT) < 0)
		dsim_err("failed to send aor_tbl.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)elvss_tbl[level],
		AOR_ELVSS_CMD_CNT) < 0)
		dsim_err("failed to send elvss_tbl.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	return 0;
}

int s6e36w1x01_gamma_update(void)
{
	return 0;
}

void s6e36w1x01_mcd_test_on(void)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd = dsim->lcd;
	struct s6e36w1x01 *panel = dev_get_drvdata(&lcd->dev);

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return;
	}

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if (panel->mcd_on) {
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_PWR_ON,
			ARRAY_SIZE(MCD_PWR_ON)) < 0)
			dsim_err("failed to send MCD_PWR_ON.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_TEST_ON,
			ARRAY_SIZE(MCD_TEST_ON)) < 0)
			dsim_err("failed to send MCD_TEST_ON.\n");
	} else {
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_PWR_OFF,
			ARRAY_SIZE(MCD_PWR_OFF)) < 0)
			dsim_err("failed to send MCD_PWR_OFF.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_TEST_OFF,
			ARRAY_SIZE(MCD_TEST_OFF)) < 0)
			dsim_err("failed to send MCD_TEST_OFF.\n");
	}
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) MCD_TEST_UPDATE,
		ARRAY_SIZE(MCD_TEST_UPDATE)) < 0)
		dsim_err("failed to send MCD_TEST_UPDATE.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	msleep(100);
}

void s6e36w1x01_hlpm_on(void)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd = dsim->lcd;
	struct s6e36w1x01 *panel = dev_get_drvdata(&lcd->dev);

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return;
	}

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if (panel->hlpm_on) {
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_TEMP_ETC0,
			ARRAY_SIZE(ALPM_TEMP_ETC0)) < 0)
			dsim_err("failed to send ALPM_TEMP_ETC0.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_TEMP_ETC1,
			ARRAY_SIZE(ALPM_TEMP_ETC1)) < 0)
			dsim_err("failed to send ALPM_TEMP_ETC1.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_TEMP_ETC2,
			ARRAY_SIZE(ALPM_TEMP_ETC2)) < 0)
			dsim_err("failed to send ALPM_TEMP_ETC2.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_TEMP_ETC3,
			ARRAY_SIZE(ALPM_TEMP_ETC3)) < 0)
			dsim_err("failed to send ALPM_TEMP_ETC3.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_TEMP_ETC4,
			ARRAY_SIZE(ALPM_TEMP_ETC4)) < 0)
			dsim_err("failed to send ALPM_TEMP_ETC4.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_TEMP_ETC5,
			ARRAY_SIZE(ALPM_TEMP_ETC5)) < 0)
			dsim_err("failed to send ALPM_TEMP_ETC5.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HLPM_GAMMA_ETC,
			ARRAY_SIZE(HLPM_GAMMA_ETC)) < 0)
			dsim_err("failed to send HLPM_GAMMA_ETC.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) PANEL_UPDATE,
			ARRAY_SIZE(PANEL_UPDATE)) < 0)
			dsim_err("failed to send PANEL_UPDATE.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HLPM_ETC,
			ARRAY_SIZE(HLPM_ETC)) < 0)
			dsim_err("failed to send HLPM_ETC.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ALPM_ON,
			ARRAY_SIZE(ALPM_ON)) < 0)
			dsim_err("failed to send ALPM_ON.\n");
	} else
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) NORMAL_ON,
			ARRAY_SIZE(NORMAL_ON)) < 0)
			dsim_err("failed to send NORMAL_ON.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	pr_info("%s: ALPM:%s\n", __func__, panel->alpm_on ? "ON" : "OFF");
}

int s6e36w1x01_hbm_on(void)
{
	struct dsim_device *dsim = get_dsim_drvdata(ID);
	struct lcd_device *lcd = dsim->lcd;
	struct s6e36w1x01 *panel = dev_get_drvdata(&lcd->dev);

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return 0;
	}

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if (panel->hbm_on) {
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ELVSS,
			ARRAY_SIZE(HBM_ELVSS)) < 0)
			dsim_err("failed to send HBM_ELVSS.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_VINT,
			ARRAY_SIZE(HBM_VINT)) < 0)
			dsim_err("failed to send HBM_VINT.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ACL_8P,
			ARRAY_SIZE(ACL_8P)) < 0)
			dsim_err("failed to send ACL_8P.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ACL_ON,
			ARRAY_SIZE(HBM_ACL_ON)) < 0)
			dsim_err("failed to send HBM_ACL_ON.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ON,
			ARRAY_SIZE(HBM_ON)) < 0)
			dsim_err("failed to send HBM_ON.\n");
	} else
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) HBM_OFF,
		ARRAY_SIZE(HBM_OFF)) < 0)
		dsim_err("failed to send HBM_OFF.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	return 0;
}

int s6e36w1x01_temp_offset_comp(unsigned int stage)
{
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	switch (stage) {
	case TEMP_RANGE_0:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TEMP_OFF,
			ARRAY_SIZE(MPS_TEMP_OFF)) < 0)
			dsim_err("failed to send MPS_TEMP_OFF.\n");
		break;
	case TEMP_RANGE_1:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) TEMP_OFFSET_GPARA,
			ARRAY_SIZE(TEMP_OFFSET_GPARA)) < 0)
			dsim_err("failed to send TEMP_OFFSET_GPARA.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TSET_1,
			ARRAY_SIZE(MPS_TSET_1)) < 0)
			dsim_err("failed to send MPS_TSET_1.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TEMP_ON,
			ARRAY_SIZE(MPS_TEMP_ON)) < 0)
			dsim_err("failed to send MPS_TEMP_ON.\n");
		break;
	case TEMP_RANGE_2:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) TEMP_OFFSET_GPARA,
			ARRAY_SIZE(TEMP_OFFSET_GPARA)) < 0)
			dsim_err("failed to send TEMP_OFFSET_GPARA.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TSET_2,
			ARRAY_SIZE(MPS_TSET_2)) < 0)
			dsim_err("failed to send MPS_TSET_2.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TEMP_ON,
			ARRAY_SIZE(MPS_TEMP_ON)) < 0)
			dsim_err("failed to send MPS_TEMP_ON.\n");
		break;
	case TEMP_RANGE_3:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) TEMP_OFFSET_GPARA,
			ARRAY_SIZE(TEMP_OFFSET_GPARA)) < 0)
			dsim_err("failed to send TEMP_OFFSET_GPARA.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TSET_3,
			ARRAY_SIZE(MPS_TSET_3)) < 0)
			dsim_err("failed to send MPS_TSET_3.\n");

		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MPS_TEMP_ON,
			ARRAY_SIZE(MPS_TEMP_ON)) < 0)
			dsim_err("failed to send MPS_TEMP_ON.\n");
		break;
	}

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");
	
	return 0;
}

void s6e36w1x01_mdnie_set(enum mdnie_scenario scenario)
{
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	switch (scenario) {
	case SCENARIO_GRAY:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) GRAY_TUNE,
			ARRAY_SIZE(GRAY_TUNE)) < 0)
			dsim_err("failed to send GRAY_TUNE.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_GRAY_ON,
			ARRAY_SIZE(MDNIE_GRAY_ON)) < 0)
			dsim_err("failed to send MDNIE_GRAY_ON.\n");
		break;
	case SCENARIO_NEGATIVE:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) NEGATIVE_TUNE,
			ARRAY_SIZE(NEGATIVE_TUNE)) < 0)
			dsim_err("failed to send NEGATIVE_TUNE.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_NEGATIVE_ON,
			ARRAY_SIZE(MDNIE_NEGATIVE_ON)) < 0)
			dsim_err("failed to send MDNIE_NEGATIVE_ON.\n");
		break;
	case SCENARIO_GRAY_NEGATIVE:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) GRAY_NEGATIVE_TUNE,
			ARRAY_SIZE(GRAY_NEGATIVE_TUNE)) < 0)
			dsim_err("failed to send GRAY_NEGATIVE_TUNE.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_GRAY_NEGATIVE_ON,
			ARRAY_SIZE(MDNIE_GRAY_NEGATIVE_ON)) < 0)
			dsim_err("failed to send MDNIE_GRAY_NEGATIVE_ON.\n");
		break;
	default:
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_CTL_OFF,
			ARRAY_SIZE(MDNIE_CTL_OFF)) < 0)
			dsim_err("failed to send MDNIE_CTL_OFF.\n");
		break;
	}
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	return;
}

void s6e36w1x01_mdnie_outdoor_set(enum mdnie_outdoor on)
{
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if (on) {
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) OUTDOOR_TUNE,
			ARRAY_SIZE(OUTDOOR_TUNE)) < 0)
			dsim_err("failed to send OUTDOOR_TUNE.\n");
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_OUTD_ON,
			ARRAY_SIZE(MDNIE_OUTD_ON)) < 0)
			dsim_err("failed to send MDNIE_OUTD_ON.\n");
	} else
		if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_CTL_OFF,
			ARRAY_SIZE(MDNIE_CTL_OFF)) < 0)
			dsim_err("failed to send MDNIE_CTL_OFF.\n");

	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");
	if(dsim_wr_data(ID, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("failed to send TEST_KEY_OFF_0.\n");

	return;
}
