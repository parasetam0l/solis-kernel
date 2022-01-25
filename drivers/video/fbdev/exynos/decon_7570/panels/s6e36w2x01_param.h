/* s6e36w2x01_param.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E36W2X01_PARAM_H__
#define __S6E36W2X01_PARAM_H__

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80
#define DIMMING_COUNT	74
#define EXTEND_BRIGHTNESS			365
#define MAX_BRIGHTNESS_COUNT		601
#define HBM_BRIHGTNESS	120
#define HBM_CANDELA	1000


#define MDNIE_TUNE	0
#define POWER_IS_ON(pwr)	((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)


#define	DIMMING_METHOD_AID				0
#define DIMMING_METHOD_FILL_CENTER		1
#define DIMMING_METHOD_INTERPOLATION	2
#define DIMMING_METHOD_FILL_HBM			3
#define DIMMING_METHOD_MAX				4

#define W1	DIMMING_METHOD_AID
#define W2	DIMMING_METHOD_FILL_CENTER
#define W3	DIMMING_METHOD_INTERPOLATION
#define W4	DIMMING_METHOD_FILL_HBM

//#define lcd_to_master(a)	(a->dsim_dev->master)
//#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

#define LDI_CASET		0x2A
#define LDI_PASET		0x2B
#define LDI_MTP1		0xB1
#define LDI_MTP2		0xBC
#define LDI_MTP3		0xC7
#define LDI_MTP4		0xC8
#define LDI_WHITE_COLOR	0xA1
#define LDI_CHIP_ID		0xD6
#define LDI_ELVSS		0xB6
#define LDI_GAMMA		0xCA

#define LDI_MTP4_MAX_PARA	47
#define CELL_ID_LEN		22
#define WHITE_COLOR_LEN	4
#define LDI_MTP1_LEN		6
#define LDI_MTP2_LEN		12
#define LDI_MTP3_LEN		4
#define LDI_MTP4_LEN		32
#define LDI_ELVSS_LEN		1
#define LDI_CHIP_LEN		5

#define GAMMA_CMD_CNT		36
#define AOR_ELVSS_CMD_CNT	3
#define HBM_ELVSS_CMD_CNT	26
#define MIN_ACL			0
#define MAX_ACL			3

#define SELF_IMAGE_CNT		7
#define REFRESH_60HZ		60
#define CANDELA_COUNT		74
#define RGB_COUNT		3

#define S6E36W2_MTP_ADDR		0xC8
#define S6E36W2_MTP_SIZE		35

#define S6E36W2_ID_REG			0x04
#define S6E36W2_ID_LEN			3

#define S6E36W2_RDDPM_ADDR		0x0A
#define S6E36W2_RDDSM_ADDR		0x0E
#define S6E36W2_MTP_DATE_SIZE	S6E36W2_MTP_SIZE + 12

#define S6E36W2_COORDINATE_LEN	4
#define S6E36W2_COORDINATE_REG	0xA1

#define S6E36W2_CODE_REG		0xD6
#define S6E36W2_CODE_LEN		5

#define S6E36W2_TSET_REG					0xB8    /* TSET: Global para 8th */
#define S6E36W2_TSET_LEN					8
#define S6E36W2_TSET_MINUS_OFFSET			0x04


#define S6E36W2_ELVSS_REG					0xB6
#define S6E36W2_ELVSS_LEN					1      /* elvss: Global para 22th */
#define S6E36W2_ELVSS_CMD_CNT				3

#define S6E36W2_ELVSS_START			3    /* elvss: Global para 2th */
#define S6E36W2_ELVSS_TSET_POS		S6E36W2_ELVSS_LEN
#define S6E36W2_ELVSS_TEMPERATURE_POS		22

#define S6E36W2_AID_CMD_CNT					11 /* AID length = REG(1) +address(8) +reg_length(2) */
#define S6E36W2_AID_LEN					10 /* AID length = REG(1) +address(8) +reg_length(2) */

#define S6E36W2_HBMGAMMA_REG		0xB4
#define S6E36W2_HBMGAMMA_LEN		31

#define S6E36W2_VINT_LEN					2

#define PANEL_DISCONNEDTED		0
#define PANEL_CONNECTED			1

#define OLED_CMD_GAMMA_CNT		36
#define BRIGHTNESS_LEVEL_40NIT		40

struct SmtDimInfo {
	unsigned int br;
	unsigned int refBr;
	const unsigned int *cGma;
	signed char *rTbl;
	signed char *cTbl;
	unsigned char *aid;
	unsigned char *elvCaps;
	unsigned char *elv;
	unsigned char gamma[OLED_CMD_GAMMA_CNT];
	unsigned int way;
	signed char* elvss_offset;
};

static const short center_gamma[NUM_VREF][CI_MAX] = {
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x100, 0x100, 0x100 },
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB1,
	0x00, 0x0C
};

static const unsigned char SEQ_VINT_SET[] = {
    0xF4,
    0xBB,                   /* VINT */
    0x1E                    /* 360nit */
};

static const unsigned char VINT_TABLE[] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19,	0x1A, 0x1B, 0x1C, 0x1D, 0x1E
};

static const unsigned int VINT_DIM_TABLE[] = {
	6, 7, 8, 9,	10, 11, 12, 13,
	14, 15, 16, 17, 19, 20, 21
};

static const unsigned char TEST_KEY_ON_0[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char TEST_KEY_OFF_0[] = {
	0xF0,
	0xA5, 0xA5,
};

static const unsigned char TEST_KEY_ON_1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char TEST_KEY_OFF_1[] = {
	0xF1,
	0xA5, 0xA5,
};

static const unsigned char AID_UPDATE[] = {
	0xF7,
	0x03, 0x00,
};

static const unsigned char HIDDEN_KEY_ON[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char HIDDEN_KEY_OFF[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SLPIN[] = {
	0x10,
	0x00, 0x00,
};

static const unsigned char SLPOUT[] = {
	0x11,
	0x00, 0x00,
};

static const unsigned char PTLON[] = {
	0x12,
	0x00, 0x00,
};

static const unsigned char NORON[] = {
	0x13,
	0x00, 0x00,
};

static const unsigned char DISPOFF[] = {
	0x28,
	0x00, 0x00,
};

static const unsigned char DISPON[] = {
	0x29,
	0x00, 0x00,
};

static const unsigned char PTLAR[] = {
	0x30,
	0x00, 0x00, 0x01, 0xDF,
};

static const unsigned char TEOFF[] = {
	0x34,
	0x00, 0x00,
};

static const unsigned char TEON[] = {
	0x35,
	0x00, 0x00,
};

static const unsigned char IDMOFF[] = {
	0x38,
	0x00, 0x00,
};

static const unsigned char IDMON[] = {
	0x39,
	0x00, 0x00,
};

static const unsigned char WRCABC_OFF[] = {
	0x55,
	0x00, 0x00,
};

static const unsigned char TEMP_OFFSET_GPARA[] = {
	0xB0,
	0x07, 0x00,
};

static const unsigned char MPS_TEMP_ON[] = {
	0xB6,
	0x8C, 0x00,
};

static const unsigned char MPS_TEMP_OFF[] = {
	0xB6,
	0x88, 0x00,
};

static const unsigned char MPS_TSET_1[] = {
	0xB8,
	0x80, 0x00,
};

static const unsigned char MPS_TSET_2[] = {
	0xB8,
	0x8A, 0x00,
};

static const unsigned char MPS_TSET_3[] = {
	0xB8,
	0x94, 0x00,
};

static const unsigned char DEFAULT_GPARA[] = {
	0xB0,
	0x00, 0x00,
};

static const unsigned char ELVSS_DFLT_GPARA[] = {
	0xB0,
	0x18, 0x00,
};

static const unsigned char MTP1_GPARA[] = {
	0xB0,
	0x05, 0x00,
};

static const unsigned char MTP3_GPARA[] = {
	0xB0,
	0x04, 0x00,
};

static const unsigned char LTPS1_GPARA[] = {
	0xB0,
	0x0F, 0x00,
};

static const unsigned char LTPS2_GPARA[] = {
	0xB0,
	0x54, 0x00,
};

static const unsigned char TEMP_OFFSET[] = {
	0xB6,
	0x00, 0x00, 0x00, 0x05,
	0x05, 0x0C, 0x0C, 0x0C,
	0x0C,
};

static const unsigned char HBM_ELVSS[] = {
	0xB6,
	0x88, 0x11,
};

static const unsigned char HBM_VINT[] = {
	0xF4,
	0x77, 0x0A,
};

static const unsigned char ACL_8P[] = {
	0xB5,
	0x51, 0x99, 0x0A, 0x0A, 0x0A,
};

static const unsigned char HBM_ACL_ON_OLD[] = {
	0xB4,
	0x25,
};

static const unsigned char HBM_ACL_ON[] = {
	0xB4,
	0x25,
};

static const unsigned char HBM_ACL_OFF[] = {
	0xB4,
	0x21,
};

static const unsigned char HBM_ON[] = {
	0x53,
	0xC0,
};

static const unsigned char HBM_OFF[] = {
	0x53,
	0x00,
};

static const unsigned char ACL_ON[] = {
	0x55,
	0x02,
};

static const unsigned char ACL_OFF[] = {
	0x55,
	0x00,
};

static const unsigned char SET_ALPM_FRQ[] = {
	0xBB,
	0x00, 0x00, 0x00, 0x00,
	0x01, 0xE0, 0x47, 0x49,
	0x55, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0A,
};

static const unsigned char GAMMA_600[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x00,
};

static const unsigned char LTPS_TIMING1[] = {
	0xCB,
	0x87, 0x00,
};

static const unsigned char LTPS_TIMING2[] = {
	0xCB,
	0x00, 0x87, 0x69, 0x1A,
	0x69, 0x1A, 0x00, 0x00,
	0x08, 0x03, 0x03, 0x00,
	0x02, 0x02, 0x0F, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F,
};

static const unsigned char IGNORE_EOT[] = {
	0xE7,
	0xEF, 0x67, 0x03, 0xAF,
	0x47,
};

static const unsigned char GRAY_TUNE[] = {
	0xE7,
	0x00, 0x00, 0x00, 0x07,
	0xFF, 0x07, 0xFF, 0x01,
	0x00, 0x01, 0x00, 0xF0,
	0xE0, 0xD0, 0xC0, 0xB0,
	0xA0, 0x90, 0x80, 0x70,
	0x60, 0x50, 0x40, 0x30,
	0x20, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xB3,
	0x4C, 0xB3, 0x4C, 0xB3,
	0x4C, 0x69, 0x96, 0x69,
	0x96, 0x69, 0x96, 0xE2,
	0x1D, 0xE2, 0x1D, 0xE2,
	0x1D, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00,
};

static const unsigned char GRAY_NEGATIVE_TUNE[] = {
	0xE7,
	0x00, 0x00, 0x00, 0x07,
	0xFF, 0x07, 0xFF, 0x01,
	0x00, 0x01, 0x00, 0xF0,
	0xE0, 0xD0, 0xC0, 0xB0,
	0xA0, 0x90, 0x80, 0x70,
	0x60, 0x50, 0x40, 0x30,
	0x20, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x4C,
	0xB3, 0x4C, 0xB3, 0x4C,
	0xB3, 0x96, 0x69, 0x96,
	0x69, 0x96, 0x69, 0x1D,
	0xE2, 0x1D, 0xE2, 0x1D,
	0xE2, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF,
};

static const unsigned char NEGATIVE_TUNE[] = {
	0xE7,
	0x00, 0x00, 0x00, 0x07,
	0xFF, 0x07, 0xFF, 0x01,
	0x00, 0x01, 0x00, 0xF0,
	0xE0, 0xD0, 0xC0, 0xB0,
	0xA0, 0x90, 0x80, 0x70,
	0x60, 0x50, 0x40, 0x30,
	0x20, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF, 0xFF,
	0x00, 0x00, 0xFF, 0x00,
	0xff, 0x00, 0xFF, 0xFF,
	0x00, 0x00, 0xFF, 0x00,
	0xFF, 0x00, 0xFF,
};

static const unsigned char OUTDOOR_TUNE[] = {
	0xE7,
	0x00, 0xff, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0xff, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0xff, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0x00, 0x01, 0x88,
	0x01, 0x88, 0x01, 0x88,
	0x05, 0x90, 0x05, 0x90,
	0x05, 0x90, 0x05, 0x90,
	0x0c, 0x98, 0x0c, 0x98,
	0x0c, 0x98, 0x0c, 0x98,
	0x18, 0xa0, 0x18, 0xa0,
	0x18, 0xa0, 0x18, 0xa0,
	0x18, 0xa0, 0x48, 0xb5,
	0x40, 0xb2, 0x31, 0xae,
	0x29, 0x1d, 0x54, 0x16,
	0x87, 0x0f, 0x00, 0xFF,
	0x01, 0x00,
};

static const unsigned char MDNIE_GRAY_ON[] = {
	0xE6,
	0x01, 0x00, 0x0C, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x01,
};

static const unsigned char MDNIE_OUTD_ON[] = {
	0xE6,
	0x01, 0x00, 0x33, 0x05,
};

static const unsigned char MDNIE_NEGATIVE_ON[] = {
	0xE6,
	0x01, 0x00, 0x0C, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x01,
};

static const unsigned char MDNIE_GRAY_NEGATIVE_ON[] = {
	0xE6,
	0x01, 0x00, 0x0C, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x01,
};

static const unsigned char MDNIE_CTL_OFF[] = {
	0xE6,
	0x00, 0x00,
};

static const unsigned char SET_DC_VOL[] = {
	0xF5,
	0xC2, 0x03, 0x0B, 0x1B,
	0x7D, 0x57, 0x22, 0x0A,
};

static const unsigned char ELVSS_PARAM[] = {
	0xB0,
	0x02,
};

static const unsigned char VINT_PARAM[] = {
	0xB0,
	0x03, 0x00,
};

static const unsigned char PANEL_UPDATE[] = {
	0xF7,
	0x03
};

#ifdef CONFIG_COPR_SUPPORT
static const unsigned char COPR_ROI_SET[] = {
	0xEB,
	0x03, 0x48, 0xFF, 0x39,
	0x10, 0x9A, 0x00, 0xDB,
	0x00, 0xCA, 0x01, 0x08,
};
#endif

static const unsigned char ETC_GPARA[] = {
	0xB0,
	0x06, 0x00,
};

static const unsigned char ETC_SET[] = {
	0xFE,
	0x05, 0x00,
};

static const unsigned char AUTO_CLK_ON[] = {
	0xB9,
	0xBE, 0x07, 0x7D, 0x00,
	0x3B, 0x41, 0x00, 0x00,
	0x0A, 0x04, 0x08, 0x00,
};

static const unsigned char AUTO_CLK_OFF[] = {
	0xB9,
	0xA0, 0x07, 0x7D, 0x00,
	0x3B, 0x41, 0x00, 0x00,
	0x0A, 0x04, 0x08, 0x00,
};

static const unsigned char ALPM_ETC[] = {
	0xBB,
	0x90, 0x00,
};

static const unsigned char ALPM_ETC_EXIT[] = {
	0xBB,
	0x91, 0x00,
};

static const unsigned char NORMAL_ON[] = {
	0x53,
	0x00, 0x00,
};

static const unsigned char HLPM_PRE_ON_0[] = {
	0xF5,
	0x09, 0x04, 0x4C, 0x08,
	0x40, 0x20, 0x10, 0x31,
	0xF6
};

static const unsigned char HLPM_PRE_ON_1[] = {
	0xF6,
	0x11, 0x03, 0x77, 0x38,
	0x00, 0x0B, 0x0F, 0x00,
	0x4F, 0x03, 0x01, 0x9F
};

static const unsigned char HLPM_PRE_ON_2[] = {
	0xBB,
	0x09, 0x00, 0x10, 0x10,
	0x01, 0x20, 0x87, 0x8A,
	0xB4, 0xE7, 0xE7, 0xE7,
	0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5,
	0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF,
	0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9,
	0x33, 0x04, 0xC0
};

/* HLPM GAMMA FOR ID3 == 0x24 PANEL*/
static const unsigned char HLPM_PRE_ON_24[] = {
	0xBB,
	0x09, 0x01, 0x0A, 0x4B,
	0x80, 0x00, 0x7C, 0x7F,
	0x9F, 0xE2, 0xE2, 0xE3,
	0xDD, 0xDD, 0xDE, 0xC7,
	0xC9, 0xCC, 0xC8, 0xD0,
	0xD1, 0xD5, 0xD7, 0xDF,
	0xDB, 0xDE, 0xDF, 0xBE,
	0xCF, 0xBC, 0xC6, 0xC6,
	0xC6, 0x80, 0x80, 0x80,
	0x33, 0x04, 0xC0
};

static const unsigned char HLPM_PRE_ON[] = {
	0xBB,
	0x09
};

static const unsigned char HLPM_PRE_ON_3[] = {
	0xC7,
	0x20, 0x00
};

static const unsigned char HLPM_40NIT_ON[] = {
	0x53,
	0x02
};

static const unsigned char HLPM_5NIT_ON[] = {
	0x53,
	0x03
};

static const unsigned char ALPM_ON[] = {
	0x53,
	0x02, 0x00,
};

#ifdef CONFIG_EXYNOS_HLPM_TEST_SUPPORT
static const unsigned char HLPM_10_BB[] = {
	0xBB,
	0x09, 0x00, 0x04, 0x04, 0x01, 0x20, 0x48, 0x4C,
	0x70, 0xEA, 0xEA, 0xEA, 0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5, 0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF, 0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9, 0x33, 0x04, 0xC0
};

static const unsigned char HLPM_20_BB[] = {
	0xBB,
	0x09, 0x00, 0x04, 0x04, 0x01, 0x20, 0x61, 0x64,
	0x8A, 0xEA, 0xEA, 0xEA, 0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5, 0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF, 0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9, 0x33, 0x04, 0xC0
};

static const unsigned char HLPM_40_BB[] = {
	0xBB,
	0x09, 0x00, 0x04, 0x04, 0x01, 0x20, 0x78, 0x7A,
	0xA0, 0xEA, 0xEA, 0xEA, 0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5, 0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF, 0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9, 0x33, 0x04, 0xC0
};

static const unsigned char HLPM_60_BB[] = {
	0xBB,
	0x09, 0x00, 0x04, 0x04, 0x01, 0x20, 0x87, 0x8A,
	0xB4, 0xE7, 0xE7, 0xE7, 0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5, 0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF, 0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9, 0x33, 0x04, 0xC0
};

static const unsigned char HLPM_20_AOR_BB[] = {
	0xBB,
	0x09, 0x00, 0xF4, 0xF4, 0x01, 0x20, 0x87, 0x8A,
	0xB4, 0xE7, 0xE7, 0xE7, 0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5, 0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF, 0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9, 0x33, 0x04, 0xC0
};

static const unsigned char HLPM_10_AOR_BB[] = {
	0xBB,
	0x09, 0x11, 0x3F, 0x3F, 0x01, 0x20, 0x87, 0x8A,
	0xB4, 0xE7, 0xE7, 0xE7, 0xDF, 0xDF, 0xDF, 0xD0,
	0xD0, 0xD0, 0xC0, 0xD5, 0xD0, 0xDE, 0xEA, 0xD9,
	0xDF, 0xEF, 0xD9, 0xDF, 0xEF, 0xD9, 0xDF, 0xEF,
	0xD9, 0xDF, 0xEF, 0xD9, 0x33, 0x04, 0xC0
};

static const unsigned char HLPM_F5[] = {
	0xF5,
	0x09, 0x04, 0x4C, 0x08, 0x40, 0x20, 0x10, 0x31,
	0xF6,
};

static const unsigned char HLPM_F6[] = {
	0xF6,
	0x11, 0x05,
};

static const unsigned char HLPM_C7[] = {
	0xC7,
	0x20,
};

static const unsigned char HLPM_UPDATE[] = {
	0xF7,
	0x03,
};

static const unsigned char HLPM_20_B0[] = {
	0xB0,
	0x51,
};

static const unsigned char HLPM_20_CB[] = {
	0xCB,
	0x82,
};

static const unsigned char HLPM_20_B0_2[] = {
	0xB0,
	0x5B,
};

static const unsigned char HLPM_20_CB_2[] = {
	0xCB,
	0x04, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x01
};

static const unsigned char HLPM_10_AOR_B0[] = {
	0xB0,
	0x04,
};

static const unsigned char HLPM_10_AOR_BB_2[] = {
	0xBB,
	0x01,
};

static const unsigned char HLPM_10_AOR_F7[] = {
	0xF7,
	0x03,
};

static const unsigned char NOMAL_DC_BB[] = {
	0xBB,
	0x19,
};

static const unsigned char NOMAL_DC_CB[] = {
	0xBB,
	0x20, 0x01, 0x01, 0x01, 0x40, 0x01, 0x01, 0x33, 0x01, 0x23,
	0x45, 0x00, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x7A, 0x28, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x2E, 0x00, 0x00, 0x02, 0x04, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0xCF, 0x10, 0x0D, 0x14, 0x98,
	0x17, 0xD6, 0x1B, 0x1A, 0x19, 0xF3, 0xF3, 0xD3, 0xD3, 0xC4,
	0xC5, 0x42, 0xD4, 0xD8, 0xD7, 0xF6, 0xFB, 0x1A, 0x19, 0x13,
	0x13, 0x13, 0x13, 0x00, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x86, 0x28, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x02,
	0x04, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x22, 0x22, 0x01,
};

static const unsigned char NOMAL_DC_C8[] = {
	0xC8,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char NOMAL_DC_60_CA[] = {
	0xCA,
	0x00, 0x8D, 0x00, 0x8F, 0x00, 0xB5, 0xE3, 0xE3, 0xE4, 0xDD,
	0xDE, 0xDF, 0xC0, 0xC3, 0xC5, 0xC5, 0xC6, 0xC8, 0xC7, 0xDA,
	0xD4, 0xD1, 0xD8, 0xD7, 0xB7, 0xC6, 0xBC, 0xD3, 0xE9, 0xD3,
	0xDC, 0xF0, 0xD0, 0x33, 0x04,
};

static const unsigned char NOMAL_DC_40_CA[] = {
	0xCA,
	0x00, 0x7D, 0x00, 0x7F, 0x00, 0xA4, 0xE3, 0xE4, 0xE4, 0xDD,
	0xDE, 0xDF, 0xBF, 0xC4, 0xC6, 0xC0, 0xC9, 0xC8, 0xCC, 0xDA,
	0xD3, 0xCB, 0xD8, 0xCD, 0xB7, 0xC6, 0xBC, 0xD3, 0xE9, 0xD3,
	0xDC, 0xF0, 0xD0, 0x33, 0x04,
};

static const unsigned char NOMAL_DC_20_CA[] = {
	0xCA,
	0x00, 0x62, 0x00, 0x65, 0x00, 0x87, 0xE5, 0xE5, 0xE7, 0xDD,
	0xDE, 0xDF, 0xBC, 0xC5, 0xC5, 0xBC, 0xCB, 0xC5, 0xC7, 0xDC,
	0xD4, 0xD1, 0xD8, 0xD7, 0xB7, 0xC6, 0xBC, 0xD3, 0xE9, 0xD3,
	0xDC, 0xF0, 0xD0, 0x33, 0x04,
};

static const unsigned char NOMAL_DC_10_CA[] = {
	0xCA,
	0x00, 0x4D, 0x00, 0x50, 0x00, 0x71, 0xE2, 0xE4, 0xE5, 0xDD,
	0xE0, 0xE1, 0xB9, 0xC5, 0xC2, 0xC0, 0xD5, 0xCA, 0xCC, 0xDA,
	0xD3, 0xCB, 0xD8, 0xCD, 0xB7, 0xC6, 0xBC, 0xD3, 0xE9, 0xD3,
	0xDC, 0xF0, 0xD0, 0x33, 0x04,
};

static const unsigned char NOMAL_DC_B6[] = {
	0xB6,
	0x19, 0xC8, 0x27, 0x01, 0x01, 0x34, 0x67, 0x9A, 0xCD, 0x01,
	0x22, 0x33, 0x44, 0x00, 0x00, 0x05, 0x5C, 0xCC, 0x0C, 0x01,
	0x11, 0x11, 0x10, 0x00,
};

static const unsigned char NOMAL_DC_F7[] = {
	0xF7,
	0x03,
};
#endif
static const unsigned char MCD_TEST_ON[] = {
	0xCB,
	0x20, 0x01, 0x01, 0x01, 0x40,
	0x01, 0x01, 0x23, 0x60, 0x66,
	0x06, 0x00, 0x00, 0x05, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x78, 0x1E, 0x00, 0x00,
	0x00, 0x00, 0x05, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xAB,
	0x8A, 0x24, 0x03, 0x00, 0x70,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0xCF, 0x10, 0x0D, 0x14, 0x98,
	0x17, 0xD6, 0x1B, 0x1A, 0x19,
	0xF3, 0xF3, 0xD3, 0xD3, 0xC4,
	0xC5, 0x42, 0xD4, 0xD8, 0xD7,
	0xF6, 0xFB, 0x1A, 0x19, 0x13,
	0x13, 0x13, 0x13, 0x00, 0x00,
	0x05, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x64, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x05, 0x00, 0x02,
	0x02, 0x0D, 0x0D, 0x0D, 0x0D,
	0x0D, 0x0D, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char MCD_TIMMING_UPDATE[] = {
	0xF7,
	0x02, 0x00,
};

static const unsigned char MCD_TEST_OFF[] = {
	0xCB,
	0x20, 0x01, 0x01, 0x01, 0x40,
	0x01, 0x01, 0x33, 0x01, 0x23,
	0x45, 0x00, 0x00, 0x05, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x5e, 0x50, 0x00, 0x00,
	0x00, 0x00, 0x05, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x6a, 0x47,
	0x00, 0x00, 0x02, 0x02, 0x0d,
	0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
	0xcf, 0x10, 0x0d, 0x14, 0x98,
	0x17, 0xd6, 0x1b, 0x1a, 0x19,
	0xf3, 0xf3, 0xd3, 0xd3, 0xc4,
	0xc5, 0x42, 0xd4, 0xd8, 0xd7,
	0xf6, 0xfb, 0x1a, 0x19, 0x13,
	0x13, 0x13, 0x13, 0x00, 0x00,
	0x05, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x64, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x05, 0x00, 0x02,
	0x02, 0x0d, 0x0d, 0x0d, 0x0d,
	0x0d, 0x0d, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned int br_convert[DIMMING_COUNT] = {
	1, 3, 5, 7, 9, 11, 13,	15, 17, 20,
	22, 24, 26, 28, 30, 31, 32, 34, 36, 37,
	38, 40, 42, 43, 44, 46, 48, 50, 52, 54,
	55, 56, 58, 60, 62, 64, 65, 66, 68, 70,
	72, 73, 75, 76, 77, 78, 80, 82, 84, 85,
	86, 88, 90, 91, 91, 92, 92, 92, 93, 93,
	93, 94, 94, 94, 95, 96, 96, 97, 97, 98,
	98, 99, 99, 100,
};

static const unsigned int gma2p20 [256] = {
         0,          5,         23,         57,        107,        175,        262,        367,
       493,        638,        805,        992,       1202,       1433,       1687,       1963,
      2263,       2586,       2932,       3303,       3697,       4116,       4560,       5028,
      5522,       6041,       6585,       7155,       7751,       8373,       9021,       9696,
     10398,      11126,      11881,      12664,      13473,      14311,      15175,      16068,
     16988,      17936,      18913,      19918,      20951,      22013,      23104,      24223,
     25371,      26549,      27755,      28991,      30257,      31551,      32876,      34230,
     35614,      37029,      38473,      39947,      41452,      42987,      44553,      46149,
     47776,      49433,      51122,      52842,      54592,      56374,      58187,      60032,
     61907,      63815,      65754,      67725,      69727,      71761,      73828,      75926,
     78057,      80219,      82414,      84642,      86901,      89194,      91518,      93876,
     96266,      98689,     101145,     103634,     106156,     108711,     111299,     113921,
    116576,     119264,     121986,     124741,     127530,     130352,     133209,     136099,
    139022,     141980,     144972,     147998,     151058,     154152,     157281,     160444,
    163641,     166872,     170138,     173439,     176774,     180144,     183549,     186989,
    190463,     193972,     197516,     201096,     204710,     208360,     212044,     215764,
    219520,     223310,     227137,     230998,     234895,     238828,     242796,     246800,
    250840,     254916,     259027,     263175,     267358,     271577,     275833,     280124,
    284452,     288816,     293216,     297653,     302125,     306635,     311180,     315763,
    320382,     325037,     329729,     334458,     339223,     344026,     348865,     353741,
    358654,     363604,     368591,     373615,     378676,     383775,     388910,     394083,
    399293,     404541,     409826,     415148,     420508,     425905,     431340,     436813,
    442323,     447871,     453456,     459080,     464741,     470440,     476177,     481952,
    487765,     493616,     499505,     505432,     511398,     517401,     523443,     529523,
    535642,     541798,     547994,     554227,     560499,     566810,     573159,     579547,
    585973,     592438,     598942,     605484,     612066,     618686,     625345,     632043,
    638779,     645555,     652370,     659224,     666117,     673049,     680020,     687031,
    694081,     701170,     708298,     715465,     722672,     729919,     737205,     744530,
    751895,     759300,     766744,     774227,     781751,     789314,     796917,     804559,
    812241,     819964,     827726,     835528,     843370,     851252,     859174,     867136,
    875138,     883180,     891262,     899385,     907547,     915750,     923993,     932277,
    940601,     948965,     957370,     965815,     974300,     982826,     991393,    1000000,

};

static const unsigned int gma2p15 [256] = {
         0,          7,         30,         71,        132,        213,        315,        439,
       586,        754,        946,       1161,       1400,       1663,       1950,       2262,
      2599,       2961,       3348,       3761,       4199,       4663,       5154,       5671,
      6214,       6784,       7381,       8005,       8656,       9335,      10040,      10774,
     11535,      12324,      13141,      13986,      14859,      15761,      16691,      17649,
     18637,      19653,      20698,      21772,      22875,      24007,      25169,      26360,
     27581,      28831,      30111,      31421,      32760,      34130,      35529,      36959,
     38419,      39909,      41429,      42980,      44562,      46174,      47817,      49490,
     51195,      52930,      54696,      56494,      58322,      60182,      62073,      63995,
     65948,      67933,      69950,      71998,      74078,      76189,      78333,      80508,
     82715,      84954,      87224,      89528,      91863,      94230,      96630,      99062,
    101526,     104022,     106552,     109113,     111708,     114334,     116994,     119686,
    122411,     125169,     127960,     130784,     133641,     136530,     139453,     142409,
    145399,     148421,     151477,     154566,     157688,     160844,     164034,     167257,
    170513,     173803,     177127,     180484,     183875,     187300,     190759,     194252,
    197778,     201339,     204933,     208562,     212224,     215921,     219652,     223417,
    227217,     231050,     234918,     238821,     242757,     246729,     250734,     254775,
    258849,     262959,     267103,     271282,     275495,     279743,     284026,     288344,
    292697,     297084,     301507,     305964,     310457,     314984,     319547,     324145,
    328778,     333446,     338149,     342888,     347661,     352471,     357315,     362195,
    367110,     372061,     377047,     382069,     387126,     392219,     397348,     402512,
    407712,     412948,     418219,     423526,     428869,     434248,     439663,     445113,
    450600,     456122,     461681,     467275,     472906,     478572,     484275,     490014,
    495789,     501600,     507448,     513332,     519252,     525208,     531201,     537230,
    543296,     549398,     555536,     561711,     567923,     574171,     580455,     586777,
    593134,     599529,     605960,     612428,     618933,     625474,     632052,     638668,
    645319,     652008,     658734,     665497,     672296,     679133,     686006,     692917,
    699865,     706850,     713872,     720931,     728027,     735160,     742331,     749539,
    756784,     764066,     771386,     778743,     786138,     793569,     801039,     808545,
    816089,     823671,     831290,     838947,     846641,     854373,     862143,     869950,
    877794,     885677,     893597,     901555,     909550,     917584,     925655,     933764,
    941911,     950095,     958318,     966578,     974877,     983213,     991588,    1000000,

};

#endif /* __s6e36w2x01_PARAM_H__ */

