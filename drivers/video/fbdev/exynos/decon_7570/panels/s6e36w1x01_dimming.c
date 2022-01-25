/* drivers/video/exynos/s6e36w1x01_dimming.c
 *
 * MIPI-DSI based S6E36W1X01 AMOLED panel driver.
 *
 * Taeheon Kim, <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include "s6e36w1x01_dimming.h"
/*#define SMART_DIMMING_DEBUG*/
#define RGB_COMPENSATION 33

static unsigned int ref_gamma[NUM_VREF][CI_MAX] = {
	{0x00, 0x00, 0x00},
	{0x00, 0x00, 0x00},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

const static unsigned short ref_cd_tbl[MAX_GAMMA_CNT] = {
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 113, 113, 113, 113, 113, 113, 113, 113,
	113, 113, 127, 137, 143, 149, 161, 166, 184, 192,
	203, 214, 228, 240, 244, 244, 244, 244, 244, 244,
	256, 263, 288, 300, 320, 337, 360
};

const static int gradation_shift[MAX_GAMMA_CNT][NUM_VREF] = {
/*VT V0 V3 V11 V23 V35 V51 V87 V151 V203 V255*/
	{0, 0, 13, 11, 9, 8, 6, 5, 3, 2, 0},
	{0, 0, 14, 11, 9, 8, 7, 6, 4, 4, 0},
	{0, 0, 9, 10, 9, 8, 6, 5, 4, 4, 0},
	{0, 0, 9, 10, 8, 8, 6, 5, 4, 4, 0},
	{0, 0, 10, 10, 8, 7, 6, 5, 3, 4, 0},
	{0, 0, 8, 9, 8, 7, 6, 5, 3, 4, 0},
	{0, 0, 9, 9, 7, 6, 5, 4, 3, 4, 0},
	{0, 0, 7, 8, 7, 6, 5, 4, 3, 4, 0},
	{0, 0, 7, 8, 6, 5, 4, 3, 3, 3, 0},
	{0, 0, 6, 7, 6, 5, 4, 3, 3, 3, 0},
	{0, 0, 8, 7, 6, 5, 4, 3, 3, 4, 0},
	{0, 0, 7, 7, 6, 5, 4, 3, 3, 3, 0},
	{0, 0, 8, 6, 5, 5, 3, 3, 2, 3, 0},
	{0, 0, 5, 6, 5, 4, 3, 3, 2, 3, 0},
	{0, 0, 4, 5, 5, 4, 3, 3, 2, 3, 0},
	{0, 0, 7, 5, 5, 4, 3, 2, 2, 3, 0},
	{0, 0, 6, 4, 4, 3, 2, 2, 2, 3, 0},
	{0, 0, 7, 4, 4, 3, 3, 2, 2, 3, 0},
	{0, 0, 6, 4, 4, 3, 3, 2, 2, 3, 0},
	{0, 0, 1, 4, 4, 3, 3, 2, 2, 3, 0},
	{0, 0, 2, 3, 4, 3, 2, 2, 2, 3, 0},
	{0, 0, 5, 3, 3, 3, 2, 2, 2, 3, 0},
	{0, 0, 2, 3, 3, 3, 2, 2, 2, 3, 0},
	{0, 0, 1, 3, 3, 2, 2, 2, 2, 3, 0},
	{0, 0, 3, 2, 3, 2, 2, 2, 2, 3, 0},
	{0, 0, 2, 2, 3, 2, 2, 2, 2, 3, 0},
	{0, 0, 1, 2, 2, 2, 2, 2, 2, 3, 0},
	{0, 0, 1, 0, 2, 2, 1, 2, 1, 3, 0},
	{0, 0, 2, 1, 2, 2, 2, 2, 2, 3, 0},
	{0, 0, 1, 1, 2, 2, 2, 1, 2, 3, 0},
	{0, 0, 1, 1, 2, 2, 1, 1, 1, 3, 0},
	{0, 0, 1, 0, 2, 2, 1, 1, 1, 3, 0},
	{0, 0, 1, -1, 2, 1, 2, 2, 3, 3, 0},
	{0, 0, 1, 0, 2, 1, 1, 2, 2, 2, 0},
	{0, 0, 1, 0, 1, 2, 2, 3, 3, 3, 0},
	{0, 0, 3, 0, 2, 1, 2, 2, 3, 3, 0},
	{0, 0, 1, 1, 2, 1, 2, 3, 3, 2, 0},
	{0, 0, 1, -1, 1, 2, 2, 3, 3, 2, 0},
	{0, 0, 2, 0, 2, 1, 3, 4, 4, 2, 0},
	{0, 0, 1, 0, 1, 2, 2, 4, 4, 3, 0},
	{0, 0, 1, 1, 2, 2, 3, 4, 4, 3, 0},
	{0, 0, 1, 1, 1, 2, 3, 4, 5, 3, 0},
	{0, 0, 2, 0, 2, 3, 3, 5, 6, 4, 0},
	{0, 0, 1, 0, 1, 2, 3, 3, 4, 3, 0},
	{0, 0, 1, 0, 2, 2, 2, 3, 4, 4, 0},
	{0, 0, 1, 0, 2, 2, 2, 3, 4, 4, 0},
	{0, 0, 1, 0, 2, 2, 2, 5, 5, 4, 0},
	{0, 0, 1, 0, 2, 2, 2, 3, 5, 3, 0},
	{0, 0, 1, 0, 2, 2, 2, 3, 5, 3, 0},
	{0, 0, 1, 0, 2, 2, 2, 3, 4, 3, 0},
	{0, 0, 0, -1, 1, 2, 2, 3, 4, 2, 0},
	{0, 0, 0, -1, 1, 2, 2, 4, 4, 3, 0},
	{0, 0, 0, -1, 1, 1, 2, 3, 3, 2, 0},
	{0, 0, 0, 0, 2, 2, 2, 4, 4, 3, 0},
	{0, 0, 0, 0, 1, 1, 2, 2, 3, 1, 0},
	{0, 0, 0, 0, 1, 2, 1, 2, 2, 1, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset[MAX_GAMMA_CNT][RGB_COMPENSATION] = {
	/* Dummy R0 R3 R11 R23 R35 R51 R87 R151 R203 R255(RGB)*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 18, 3, 4, 19, 6, -4, 8, -6, 1, 12, 1, 1, 5, 0, -2, 1, -1, 0, 2, 1, 3, 4, 5},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -30, 7, -15, -26, 4, -9, -15, 3, -8, -9, 3, -7, -4, 1, -3, -3, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -35, 7, -16, -23, 4, -9, -13, 3, -6, -8, 3, -6, -3, 1, -2, -3, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -34, 6, -14, -27, 5, -10, -12, 3, -6, -8, 2, -6, -3, 1, -2, -3, 0, -1, -1, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -31, 6, -14, -22, 4, -8, -15, 2, -6, -8, 2, -6, -4, 0, -2, -4, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -35, 7, -16, -22, 4, -8, -12, 2, -6, -8, 2, -5, -4, 0, -2, -4, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -34, 6, -14, -22, 4, -10, -12, 2, -5, -7, 2, -5, -3, 0, -1, -4, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -35, 7, -16, -20, 4, -8, -12, 2, -5, -8, 2, -6, -5, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -30, 6, -13, -20, 3, -8, -10, 2, -5, -7, 1, -4, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -36, 7, -16, -18, 3, -7, -10, 2, -4, -6, 1, -4, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -36, 7, -15, -15, 2, -6, -10, 2, -4, -6, 1, -4, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -34, 6, -14, -16, 3, -6, -9, 1, -4, -5, 1, -3, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -34, 7, -15, -15, 2, -6, -8, 2, -4, -5, 1, -3, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -32, 7, -14, -12, 2, -4, -9, 2, -5, -5, 1, -3, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -36, 8, -16, -12, 2, -4, -7, 2, -4, -4, 1, -2, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -35, 7, -16, -12, 1, -4, -7, 2, -4, -4, 0, -2, -4, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 24, 5, 5, 18, 15, -2, 4, -2, -1, 8, 2, 1, 0, 1, -1, 2, 3, 0, 1, 1, -2, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -33, 6, -14, -10, 2, -4, -6, 1, -2, -4, 1, -2, -3, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -32, 6, -14, -10, 1, -4, -5, 1, -2, -4, 0, -2, -3, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -28, 6, -12, -7, 1, -2, -6, 1, -2, -3, 0, -2, -3, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -31, 6, -13, -6, 1, -2, -6, 0, -2, -3, 0, -2, -3, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -28, 5, -12, -9, 1, -4, -5, 0, -2, -2, 0, -1, -3, 0, 1, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -25, 4, -10, -7, 1, -2, -5, 0, -2, -3, 0, -2, -2, 0, 2, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -21, 4, -9, -7, 0, -2, -4, 0, -2, -2, 0, -1, -2, 0, 2, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -24, 4, -8, -6, 0, -2, -3, 0, -1, -2, 0, -1, -2, 0, 2, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -22, 3, -8, -5, 0, 0, -2, 0, 0, -4, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -20, 3, -6, -4, 0, 0, -3, 0, -1, -3, 0, 1, -1, 0, 0, -1, 0, 0, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 27, 34, 26, 3, 6, 5, -5, 1, -4, 3, 4, 1, 0, 0, 0, 0, 1, 1, -1, -2, -1, 1, 1, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -20, 1, -4, -3, 0, 1, -1, 0, 0, -4, 0, 0, -1, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -16, 1, -2, -3, 0, 0, -1, 0, 0, -3, 0, 1, -1, 0, 0, 0, 0, 1, -1, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -14, 0, -1, -3, 0, 0, -2, 0, 0, -4, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -13, 0, 1, -1, 0, 1, -2, 0, 0, -4, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 39, 31, 39, -1, 3, 1, -3, 1, -1, 2, 2, 1, 0, -1, 1, -3, -2, -1, 0, 0, 1, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -13, 0, -2, 0, 0, 1, -3, 0, -1, -5, 1, -3, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -10, 0, 1, 0, 0, 2, -2, 0, 0, -2, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -12, 0, -2, 0, 0, 0, -3, 0, -1, -3, 0, -2, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -8, 0, 0, -2, 0, 0, -2, 0, -1, -4, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 2, 0, 0, 2, -2, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 24, 26, -4, -1, -2, -1, 2, 0, -1, -2, -2, -1, -1, -1, -1, 0, 0, 0, -2, -1, -1, -2, -3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -8, 0, 0, 0, 0, 1, -1, 0, 0, -1, 1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 0, 0, -2, 0, 0, -3, 0, -1, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -6, 0, 0, -1, 0, 0, 0, 0, 0, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 18, 20, 19, -4, 0, -1, -3, -1, -1, 2, 2, 3, 0, -1, -1, -1, -1, -1, -2, -3, -2, 1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, -2, 0, 0, -1, 0, 0, -2, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -4, 0, 1, -2, 0, 0, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -1, 2, -1, 0, 0, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 13, 15, 14, -1, 0, 0, -1, 1, -1, 3, 4, 2, -2, -4, -2, -2, 0, -1, -2, -2, -2, 3, 4, 3},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, 3, -1, 0, 0, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 4, -1, 0, 0, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 4, 0, 0, 0, -4, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 3, 3, 3, -2, -3, -3, 1, 1, 1, -1, -1, -1, 1, 1, 0, -1, 0, -1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 14, 25, -6, -8, -7, -1, 1, 1, 3, 2, 2, -2, -3, -1, 0, 0, -1, -1, -1, -2, 1, 2, 1},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

const static unsigned int vref_index[NUM_VREF] = {
	TBL_INDEX_VT,
	TBL_INDEX_V0,
	TBL_INDEX_V3,
	TBL_INDEX_V11,
	TBL_INDEX_V23,
	TBL_INDEX_V35,
	TBL_INDEX_V51,
	TBL_INDEX_V87,
	TBL_INDEX_V151,
	TBL_INDEX_V203,
	TBL_INDEX_V255,
};

const static int vreg_element_max[NUM_VREF] = {
	0x0f,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0x1ff,
};

const static struct v_constant fix_const[NUM_VREF] = {
	{.nu = 0,	.de = 860},
	{.nu = 0,	.de = 256},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 72,	.de = 860},
};

const static short vt_trans_volt[16] = {
	6144, 6058, 5973, 5887, 5801, 5715, 5630, 5544,
	5458, 5372, 5158, 5087, 5015, 4944, 4872, 4815
};

const static short v0_trans_volt[256] = {
	0, 4, 8, 12, 16, 20, 24, 28,
	32, 36, 40, 44, 48, 52, 56, 60,
	64, 68, 72, 76, 80, 84, 88, 92,
	96, 100, 104, 108, 112, 116, 120, 124,
	128, 132, 136, 140, 144, 148, 152, 156,
	160, 164, 168, 172, 176, 180, 184, 188,
	192, 196, 200, 204, 208, 212, 216, 220,
	224, 228, 232, 236, 240, 244, 248, 252,
	256, 260, 264, 268, 272, 276, 280, 284,
	288, 292, 296, 300, 304, 308, 312, 316,
	320, 324, 328, 332, 336, 340, 344, 348,
	352, 356, 360, 364, 368, 372, 376, 380,
	384, 388, 392, 396, 400, 404, 408, 412,
	416, 420, 424, 428, 432, 436, 440, 444,
	448, 452, 456, 460, 464, 468, 472, 476,
	480, 484, 488, 492, 496, 500, 504, 508,
	512, 516, 520, 524, 528, 532, 536, 540,
	544, 548, 552, 556, 560, 564, 568, 572,
	576, 580, 584, 588, 592, 596, 600, 604,
	608, 612, 616, 620, 624, 628, 632, 636,
	640, 644, 648, 652, 656, 660, 664, 668,
	672, 676, 680, 684, 688, 692, 696, 700,
	704, 708, 712, 716, 720, 724, 728, 732,
	736, 740, 744, 748, 752, 756, 760, 764,
	768, 772, 776, 780, 784, 788, 792, 796,
	800, 804, 808, 812, 816, 820, 824, 828,
	832, 836, 840, 844, 848, 852, 856, 860,
	864, 868, 872, 876, 880, 884, 888, 892,
	896, 900, 904, 908, 912, 916, 920, 924,
	928, 932, 936, 940, 944, 948, 952, 956,
	960, 964, 968, 972, 976, 980, 984, 988,
	992, 996, 1000, 1004, 1008, 1012, 1016, 1020
};

const static short v255_trans_volt[512] = {
	5630, 5622, 5615, 5608, 5601, 5594, 5587, 5580,
	5572, 5565, 5558, 5551, 5544, 5537, 5530, 5522,
	5515, 5508, 5501, 5494, 5487, 5480, 5472, 5465,
	5458, 5451, 5444, 5437, 5430, 5422, 5415, 5408,
	5401, 5394, 5387, 5380, 5372, 5365, 5358, 5351,
	5344, 5337, 5330, 5322, 5315, 5308, 5301, 5294,
	5287, 5280, 5272, 5265, 5258, 5251, 5244, 5237,
	5230, 5222, 5215, 5208, 5201, 5194, 5187, 5180,
	5172, 5165, 5158, 5151, 5144, 5137, 5130, 5122,
	5115, 5108, 5101, 5094, 5087, 5080, 5072, 5065,
	5058, 5051, 5044, 5037, 5030, 5022, 5015, 5008,
	5001, 4994, 4987, 4979, 4972, 4965, 4958, 4951,
	4944, 4937, 4929, 4922, 4915, 4908, 4901, 4894,
	4887, 4879, 4872, 4865, 4858, 4851, 4844, 4837,
	4829, 4822, 4815, 4808, 4801, 4794, 4787, 4779,
	4772, 4765, 4758, 4751, 4744, 4737, 4729, 4722,
	4715, 4708, 4701, 4694, 4687, 4679, 4672, 4665,
	4658, 4651, 4644, 4637, 4629, 4622, 4615, 4608,
	4601, 4594, 4587, 4579, 4572, 4565, 4558, 4551,
	4544, 4537, 4529, 4522, 4515, 4508, 4501, 4494,
	4487, 4479, 4472, 4465, 4458, 4451, 4444, 4437,
	4429, 4422, 4415, 4408, 4401, 4394, 4387, 4379,
	4372, 4365, 4358, 4351, 4344, 4337, 4329, 4322,
	4315, 4308, 4301, 4294, 4287, 4279, 4272, 4265,
	4258, 4251, 4244, 4237, 4229, 4222, 4215, 4208,
	4201, 4194, 4186, 4179, 4172, 4165, 4158, 4151,
	4144, 4136, 4129, 4122, 4115, 4108, 4101, 4094,
	4086, 4079, 4072, 4065, 4058, 4051, 4044, 4036,
	4029, 4022, 4015, 4008, 4001, 3994, 3986, 3979,
	3972, 3965, 3958, 3951, 3944, 3936, 3929, 3922,
	3915, 3908, 3901, 3894, 3886, 3879, 3872, 3865,
	3858, 3851, 3844, 3836, 3829, 3822, 3815, 3808,
	3801, 3794, 3786, 3779, 3772, 3765, 3758, 3751,
	3744, 3736, 3729, 3722, 3715, 3708, 3701, 3694,
	3686, 3679, 3672, 3665, 3658, 3651, 3644, 3636,
	3629, 3622, 3615, 3608, 3601, 3594, 3586, 3579,
	3572, 3565, 3558, 3551, 3544, 3536, 3529, 3522,
	3515, 3508, 3501, 3494, 3486, 3479, 3472, 3465,
	3458, 3451, 3443, 3436, 3429, 3422, 3415, 3408,
	3401, 3393, 3386, 3379, 3372, 3365, 3358, 3351,
	3343, 3336, 3329, 3322, 3315, 3308, 3301, 3293,
	3286, 3279, 3272, 3265, 3258, 3251, 3243, 3236,
	3229, 3222, 3215, 3208, 3201, 3193, 3186, 3179,
	3172, 3165, 3158, 3151, 3143, 3136, 3129, 3122,
	3115, 3108, 3101, 3093, 3086, 3079, 3072, 3065,
	3058, 3051, 3043, 3036, 3029, 3022, 3015, 3008,
	3001, 2993, 2986, 2979, 2972, 2965, 2958, 2951,
	2943, 2936, 2929, 2922, 2915, 2908, 2901, 2893,
	2886, 2879, 2872, 2865, 2858, 2851, 2843, 2836,
	2829, 2822, 2815, 2808, 2801, 2793, 2786, 2779,
	2772, 2765, 2758, 2751, 2743, 2736, 2729, 2722,
	2715, 2708, 2701, 2693, 2686, 2679, 2672, 2665,
	2658, 2650, 2643, 2636, 2629, 2622, 2615, 2608,
	2600, 2593, 2586, 2579, 2572, 2565, 2558, 2550,
	2543, 2536, 2529, 2522, 2515, 2508, 2500, 2493,
	2486, 2479, 2472, 2465, 2458, 2450, 2443, 2436,
	2429, 2422, 2415, 2408, 2400, 2393, 2386, 2379,
	2372, 2365, 2358, 2350, 2343, 2336, 2329, 2322,
	2315, 2308, 2300, 2293, 2286, 2279, 2272, 2265,
	2258, 2250, 2243, 2236, 2229, 2222, 2215, 2208,
	2200, 2193, 2186, 2179, 2172, 2165, 2158, 2150,
	2143, 2136, 2129, 2122, 2115, 2108, 2100, 2093,
	2086, 2079, 2072, 2065, 2058, 2050, 2043, 2036,
	2029, 2022, 2015, 2008, 2000, 1993, 1986, 1979
};

const static short v3_v203_trans_volt[256] = {
	205, 208, 211, 214, 218, 221, 224, 227,
	230, 234, 237, 240, 243, 246, 250, 253,
	256, 259, 262, 266, 269, 272, 275, 278,
	282, 285, 288, 291, 294, 298, 301, 304,
	307, 310, 314, 317, 320, 323, 326, 330,
	333, 336, 339, 342, 346, 349, 352, 355,
	358, 362, 365, 368, 371, 374, 378, 381,
	384, 387, 390, 394, 397, 400, 403, 406,
	410, 413, 416, 419, 422, 426, 429, 432,
	435, 438, 442, 445, 448, 451, 454, 458,
	461, 464, 467, 470, 474, 477, 480, 483,
	486, 490, 493, 496, 499, 502, 506, 509,
	512, 515, 518, 522, 525, 528, 531, 534,
	538, 541, 544, 547, 550, 554, 557, 560,
	563, 566, 570, 573, 576, 579, 582, 586,
	589, 592, 595, 598, 602, 605, 608, 611,
	614, 618, 621, 624, 627, 630, 634, 637,
	640, 643, 646, 650, 653, 656, 659, 662,
	666, 669, 672, 675, 678, 682, 685, 688,
	691, 694, 698, 701, 704, 707, 710, 714,
	717, 720, 723, 726, 730, 733, 736, 739,
	742, 746, 749, 752, 755, 758, 762, 765,
	768, 771, 774, 778, 781, 784, 787, 790,
	794, 797, 800, 803, 806, 810, 813, 816,
	819, 822, 826, 829, 832, 835, 838, 842,
	845, 848, 851, 854, 858, 861, 864, 867,
	870, 874, 877, 880, 883, 886, 890, 893,
	896, 899, 902, 906, 909, 912, 915, 918,
	922, 925, 928, 931, 934, 938, 941, 944,
	947, 950, 954, 957, 960, 963, 966, 970,
	973, 976, 979, 982, 986, 989, 992, 995,
	998, 1002, 1005, 1008, 1011, 1014, 1018, 1021,
};

const static short int_tbl_v0_v3[2] = {
	341, 683,
};

const static short int_tbl_v3_v11[7] = {
	128, 256, 384, 512, 640, 768, 896,
};

const static short int_tbl_v11_v23[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const static short int_tbl_v23_v35[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const static short int_tbl_v35_v51[15] = {
	64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960,
};

const static short int_tbl_v51_v87[35] = {
	28, 57, 85, 114, 142, 171, 199, 228,
	256, 284, 313, 341, 370, 398, 427, 455,
	484, 512, 540, 569, 597, 626, 654, 683,
	711, 740, 768, 796, 825, 853, 882, 910,
	939, 967, 996,
};

const static short int_tbl_v87_v151[63] = {
	16, 32, 48, 64, 80, 96, 112, 128,
	144, 160, 176, 192, 208, 224, 240, 256,
	272, 288, 304, 320, 336, 352, 368, 384,
	400, 416, 432, 448, 464, 480, 496, 512,
	528, 544, 560, 576, 592, 608, 624, 640,
	656, 672, 688, 704, 720, 736, 752, 768,
	784, 800, 816, 832, 848, 864, 880, 896,
	912, 928, 944, 960, 976, 992, 1008,
};

const static short int_tbl_v151_v203[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const static short int_tbl_v203_v255[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

static const int gamma_tbl[256] = {
	0, 7, 31, 75, 138, 224, 331, 461,
	614, 791, 992, 1218, 1468, 1744, 2045, 2372,
	2725, 3105, 3511, 3943, 4403, 4890, 5404, 5946,
	6516, 7114, 7740, 8394, 9077, 9788, 10528, 11297,
	12095, 12922, 13779, 14665, 15581, 16526, 17501, 18507,
	19542, 20607, 21703, 22829, 23986, 25174, 26392, 27641,
	28921, 30232, 31574, 32947, 34352, 35788, 37255, 38754,
	40285, 41847, 43442, 45068, 46727, 48417, 50140, 51894,
	53682, 55501, 57353, 59238, 61155, 63105, 65088, 67103,
	69152, 71233, 73348, 75495, 77676, 79890, 82138, 84418,
	86733, 89080, 91461, 93876, 96325, 98807, 101324, 103874,
	106458, 109075, 111727, 114414, 117134, 119888, 122677, 125500,
	128358, 131250, 134176, 137137, 140132, 143163, 146227, 149327,
	152462, 155631, 158835, 162074, 165348, 168657, 172002, 175381,
	178796, 182246, 185731, 189251, 192807, 196398, 200025, 203688,
	207385, 211119, 214888, 218693, 222533, 226410, 230322, 234270,
	238254, 242274, 246330, 250422, 254550, 258714, 262914, 267151,
	271423, 275732, 280078, 284459, 288878, 293332, 297823, 302351,
	306915, 311516, 316153, 320827, 325538, 330285, 335069, 339890,
	344748, 349643, 354575, 359544, 364549, 369592, 374672, 379789,
	384943, 390134, 395363, 400629, 405932, 411272, 416650, 422065,
	427517, 433007, 438534, 444099, 449702, 455342, 461020, 466735,
	472488, 478279, 484107, 489973, 495878, 501819, 507799, 513817,
	519872, 525966, 532098, 538267, 544475, 550721, 557005, 563327,
	569687, 576085, 582522, 588997, 595510, 602062, 608651, 615280,
	621946, 628652, 635395, 642177, 648998, 655857, 662755, 669691,
	676667, 683680, 690733, 697824, 704954, 712122, 719330, 726576,
	733861, 741186, 748549, 755951, 763391, 770871, 778390, 785948,
	793545, 801182, 808857, 816571, 824325, 832118, 839950, 847821,
	855732, 863682, 871671, 879700, 887768, 895875, 904022, 912208,
	920434, 928699, 937004, 945349, 953733, 962156, 970619, 979122,
	987665, 996247, 1004869, 1013531, 1022233, 1030974, 1039755, 1048576
};

const static int gamma_multi_tbl[256] = {
	0, 2, 11, 26, 49, 79, 116, 162,
	216, 278, 349, 428, 516, 613, 719, 834,
	958, 1091, 1234, 1386, 1548, 1719, 1900, 2091,
	2291, 2501, 2721, 2951, 3191, 3441, 3701, 3972,
	4252, 4543, 4844, 5156, 5478, 5810, 6153, 6506,
	6870, 7245, 7630, 8026, 8433, 8850, 9278, 9717,
	10167, 10628, 11100, 11583, 12077, 12582, 13097, 13625,
	14163, 14712, 15273, 15844, 16427, 17022, 17627, 18244,
	18872, 19512, 20163, 20826, 21500, 22185, 22882, 23591,
	24311, 25043, 25786, 26541, 27308, 28086, 28876, 29678,
	30492, 31317, 32154, 33003, 33864, 34737, 35622, 36518,
	37426, 38347, 39279, 40224, 41180, 42148, 43129, 44121,
	45126, 46142, 47171, 48212, 49265, 50331, 51408, 52498,
	53600, 54714, 55840, 56979, 58130, 59294, 60469, 61657,
	62858, 64071, 65296, 66534, 67784, 69046, 70321, 71609,
	72909, 74221, 75547, 76884, 78234, 79597, 80973, 82361,
	83761, 85174, 86600, 88039, 89490, 90954, 92431, 93920,
	95422, 96937, 98465, 100005, 101559, 103125, 104703, 106295,
	107900, 109517, 111148, 112791, 114447, 116116, 117798, 119493,
	121201, 122921, 124655, 126402, 128162, 129935, 131721, 133520,
	135332, 137157, 138995, 140846, 142710, 144588, 146478, 148382,
	150299, 152229, 154172, 156129, 158098, 160081, 162077, 164087,
	166109, 168145, 170194, 172256, 174332, 176421, 178523, 180639,
	182768, 184910, 187066, 189235, 191417, 193613, 195822, 198045,
	200281, 202530, 204793, 207069, 209359, 211662, 213979, 216309,
	218653, 221010, 223381, 225765, 228163, 230575, 233000, 235438,
	237891, 240356, 242836, 245329, 247835, 250356, 252889, 255437,
	257998, 260573, 263162, 265764, 268380, 271009, 273653, 276310,
	278981, 281665, 284364, 287076, 289802, 292541, 295295, 298062,
	300843, 303638, 306447, 309269, 312106, 314956, 317820, 320698,
	323590, 326496, 329416, 332349, 335297, 338258, 341233, 344223,
	347226, 350243, 353274, 356319, 359379, 362452, 365539, 368640
};
const static unsigned char lookup_tbl[361] = {
        0, 16, 23, 28, 31, 35, 38, 41, 43, 46,
        48, 50, 52, 54, 56, 58, 60, 62, 63, 65,
        66, 68, 69, 71, 72, 74, 75, 76, 78, 79,
        80, 82, 83, 84, 85, 86, 87, 89, 90, 91,
        92, 93, 94, 95, 96, 97, 98, 99, 100, 101,
        102, 103, 104, 105, 106, 106, 107, 108, 109, 110,
        111, 112, 113, 113, 114, 115, 116, 117, 117, 118,
        119, 120, 121, 121, 122, 123, 124, 124, 125, 126,
        127, 127, 128, 129, 130, 130, 131, 132, 132, 133,
        134, 135, 135, 136, 137, 137, 138, 139, 139, 140,
        141, 141, 142, 142, 143, 144, 144, 145, 146, 146,
        147, 148, 148, 149, 149, 150, 151, 151, 152, 152,
        153, 154, 154, 155, 155, 156, 156, 157, 158, 158,
        159, 159, 160, 160, 161, 162, 162, 163, 163, 164,
        164, 165, 165, 166, 167, 167, 168, 168, 169, 169,
        170, 170, 171, 171, 172, 172, 173, 173, 174, 174,
        175, 175, 176, 176, 177, 177, 178, 178, 179, 179,
        180, 180, 181, 181, 182, 182, 183, 183, 184, 184,
        185, 185, 186, 186, 187, 187, 188, 188, 188, 189,
        189, 190, 190, 191, 191, 192, 192, 193, 193, 194,
        194, 194, 195, 195, 196, 196, 197, 197, 198, 198,
        198, 199, 199, 200, 200, 201, 201, 202, 202, 202,
        203, 203, 204, 204, 205, 205, 205, 206, 206, 207,
        207, 207, 208, 208, 209, 209, 210, 210, 210, 211,
        211, 212, 212, 212, 213, 213, 214, 214, 214, 215,
        215, 216, 216, 216, 217, 217, 218, 218, 218, 219,
        219, 220, 220, 220, 221, 221, 222, 222, 222, 223,
        223, 223, 224, 224, 225, 225, 225, 226, 226, 226,
        227, 227, 228, 228, 228, 229, 229, 229, 230, 230,
        231, 231, 231, 232, 232, 232, 233, 233, 234, 234,
        234, 235, 235, 235, 236, 236, 236, 237, 237, 238,
        238, 238, 239, 239, 239, 240, 240, 240, 241, 241,
        241, 242, 242, 242, 243, 243, 244, 244, 244, 245,
        245, 245, 246, 246, 246, 247, 247, 247, 248, 248,
        248, 249, 249, 249, 250, 250, 250, 251, 251, 251,
        252, 252, 252, 253, 253, 253, 254, 254, 254, 255,
        255
};

static int s6e36w1x01_calc_vt_volt(int gamma)
{
	int max;

	max = sizeof(vt_trans_volt) >> 2;
	if (gamma > max) {
		pr_warn("%s: exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int s6e36w1x01_calc_v0_volt(struct smart_dimming *dimming, int color)
{
	int ret, v3, gamma;

	gamma = dimming->gamma[V0][color];

	if (gamma > vreg_element_max[V0]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V0];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	v3 = dimming->volt[TBL_INDEX_V3][color];

	ret = (MULTIPLE_VREGOUT << 10) -
		((MULTIPLE_VREGOUT - v3) * (int)v0_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v3_volt(struct smart_dimming *dimming, int color)
{
	int ret, v11, gamma;

	gamma = dimming->gamma[V3][color];

	if (gamma > vreg_element_max[V3]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V3];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (MULTIPLE_VREGOUT << 10) -
		((MULTIPLE_VREGOUT - v11) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v11_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v23, gamma;

	gamma = dimming->gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (vt << 10) - ((vt - v23) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v23_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v35, gamma;

	gamma = dimming->gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (vt << 10) - ((vt - v35) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v35_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v51, gamma;

	gamma = dimming->gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (vt << 10) - ((vt - v51) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v51_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v87, gamma;

	gamma = dimming->gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (vt << 10) - ((vt - v87) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v87_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v151, gamma;

	gamma = dimming->gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (vt << 10) -
		((vt - v151) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v151_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v203, gamma;

	gamma = dimming->gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (vt << 10) - ((vt - v203) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v203_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v255, gamma;

	gamma = dimming->gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (vt << 10) - ((vt - v255) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int s6e36w1x01_calc_v255_volt(struct smart_dimming *dimming, int color)
{
	int ret, gamma;

	gamma = dimming->gamma[V255][color];

	if (gamma > vreg_element_max[V255]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int s6e36w1x01_calc_inter_v0_v3(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v0, v3, ratio;

	ratio = (int)int_tbl_v0_v3[gray];

	v0 = dimming->volt[TBL_INDEX_V0][color];
	v3 = dimming->volt[TBL_INDEX_V3][color];

	ret = (v0 << 10) - ((v0 - v3) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v3_v11(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v3, v11, ratio;

	ratio = (int)int_tbl_v3_v11[gray];
	v3 = dimming->volt[TBL_INDEX_V3][color];
	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (v3 << 10) - ((v3 - v11) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v11_v23(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = dimming->volt[TBL_INDEX_V11][color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (v11 << 10) - ((v11 - v23) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v23_v35(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = dimming->volt[TBL_INDEX_V23][color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (v23 << 10) - ((v23 - v35) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v35_v51(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = dimming->volt[TBL_INDEX_V35][color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (v35 << 10) - ((v35 - v51) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v51_v87(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = dimming->volt[TBL_INDEX_V51][color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (v51 << 10) - ((v51 - v87) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v87_v151(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = dimming->volt[TBL_INDEX_V87][color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (v87 << 10) - ((v87 - v151) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v151_v203(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = dimming->volt[TBL_INDEX_V151][color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (v151 << 10) - ((v151 - v203) * ratio);

	return ret >> 10;
}

static int s6e36w1x01_calc_inter_v203_v255(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = dimming->volt[TBL_INDEX_V203][color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (v203 << 10) - ((v203 - v255) * ratio);

	return ret >> 10;
}

void s6e36w1x01_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j;
	int temp, tmp;
	u8 pos = 0;
	u8 s_v255[3]={0,};

	tmp = (data[31] & 0xf0) >> 4;
	s_v255[0] = (tmp >> 3) & 0x1;
	s_v255[1] = (tmp >> 2) & 0x1;
	s_v255[2] = (tmp >> 1) & 0x1;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((s_v255[j] & 0x01) ? -1 : 1) * data[pos];
		dimming->gamma[V255][j] = ref_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos ++;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((data[pos] & 0x80) ? -1 : 1) *
					(data[pos] & 0x7f);
			dimming->gamma[i][j] = ref_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	temp =data[pos] & 0xf;
	dimming->gamma[VT][CI_RED] = ref_gamma[VT][CI_RED] + temp;
	dimming->mtp[VT][CI_RED] = temp;

	temp = (data[pos] & 0xf0) >> 4;
	dimming->gamma[VT][CI_GREEN] = ref_gamma[VT][CI_GREEN] + temp;
	dimming->mtp[VT][CI_GREEN] = temp;

	temp =data[++pos] & 0xf;
	dimming->gamma[VT][CI_BLUE] = ref_gamma[VT][CI_BLUE] + temp;
	dimming->mtp[VT][CI_BLUE] = temp;

	pr_info("%s:MTP OFFSET\n", __func__);
	for (i = VT; i<= V255; i++)
		pr_info("%d %d %d\n", dimming->mtp[i][0],
				dimming->mtp[i][1],dimming->mtp[i][2]);

	pr_debug("MTP+ Center gamma\n");
	for (i = VT; i<= V255; i++)
		pr_debug("%d %d %d\n", dimming->gamma[i][0],
			dimming->gamma[i][1], dimming->gamma[i][2]);
}

int s6e36w1x01_generate_volt_tbl(struct smart_dimming *dimming)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;
	int calc_seq[NUM_VREF] = {
		V255, V203, V151, V87, V51, V35, V23, V11, V3, V0};
	int (*calc_volt_point[NUM_VREF])(struct smart_dimming *, int) = {
		NULL,
		s6e36w1x01_calc_v0_volt,
		s6e36w1x01_calc_v3_volt,
		s6e36w1x01_calc_v11_volt,
		s6e36w1x01_calc_v23_volt,
		s6e36w1x01_calc_v35_volt,
		s6e36w1x01_calc_v51_volt,
		s6e36w1x01_calc_v87_volt,
		s6e36w1x01_calc_v151_volt,
		s6e36w1x01_calc_v203_volt,
		s6e36w1x01_calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct smart_dimming *, int, int)  = {
		NULL,
		NULL,
		s6e36w1x01_calc_inter_v0_v3,
		s6e36w1x01_calc_inter_v3_v11,
		s6e36w1x01_calc_inter_v11_v23,
		s6e36w1x01_calc_inter_v23_v35,
		s6e36w1x01_calc_inter_v35_v51,
		s6e36w1x01_calc_inter_v51_v87,
		s6e36w1x01_calc_inter_v87_v151,
		s6e36w1x01_calc_inter_v151_v203,
		s6e36w1x01_calc_inter_v203_v255,
	};
#ifdef SMART_DIMMING_DEBUG
	long temp[CI_MAX];
#endif
	for (i = 0; i < CI_MAX; i++)
		dimming->volt_vt[i] =
			s6e36w1x01_calc_vt_volt(dimming->gamma[VT][i]);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF - 1; j++) {
		seq = calc_seq[j];
		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				dimming->volt[index][i] =
					calc_volt_point[seq](dimming, i);
		}
	}

	index = 1;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL)
				dimming->volt[i][j] =
				calc_inter_volt[index](dimming, gray, j);
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("============= VT Voltage ===============\n");
	for (i = 0; i < CI_MAX; i++)
		temp[i] = dimming->volt_vt[i] >> 10;

	pr_info("R : %d : %ld G : %d : %ld B : %d : %ld.\n",
				dimming->gamma[VT][0], temp[0],
				dimming->gamma[VT][1], temp[1],
				dimming->gamma[VT][2], temp[2]);

	pr_info("=================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		for (j = 0; j < CI_MAX; j++)
			temp[j] = dimming->volt[i][j] >> 10;

		pr_info("V%d R : %d : %ld G : %d : %ld B : %d : %ld\n", i,
					dimming->volt[i][0], temp[0],
					dimming->volt[i][1], temp[1],
					dimming->volt[i][2], temp[2]);
	}
#endif
	return ret;
}


static int s6e36w1x01_lookup_volt_index(struct smart_dimming *dimming, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray >> 20;
	index = (int)lookup_tbl[temp];
#ifdef SMART_DIMMING_DEBUG
	pr_info("============== look up index ================\n");
	pr_info("gray : %d : %d, index : %d\n", gray, temp, index);
#endif
	exit = 1;
	i = 0;
	while (exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_GAMMA)
			index_h = MAX_GAMMA;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h)
			exit = 0;
		i++;
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("base index : %d, cnt : %d\n",
			lookup_tbl[index_l], cnt_l + cnt_h);
#endif
	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;
	temp = gamma_multi_tbl[index] << 10;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
	pr_info("temp : %d, gray : %d, p_delta : %d\n", temp, gray, p_delta);
#endif
	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] << 10;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
		pr_info("temp : %d, gray : %d, delta : %d\n",
				temp, gray, delta);
#endif
		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("ret : %d\n", ret);
#endif
	return ret;
}

static int s6e36w1x01_calc_reg_v0(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V0][color] - MULTIPLE_VREGOUT;
	t2 = dimming->look_volt[V3][color] - MULTIPLE_VREGOUT;

	ret = ((t1 * fix_const[V0].de) / t2) - fix_const[V0].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v3(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V3][color] - MULTIPLE_VREGOUT;
	t2 = dimming->look_volt[V11][color] - MULTIPLE_VREGOUT;

	ret = ((t1 * fix_const[V3].de) / t2) - fix_const[V3].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v11(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V11][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V23][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V11].de)) / t2) - fix_const[V11].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v23(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V23][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V35][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V23].de)) / t2) - fix_const[V23].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v35(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V35][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V51][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V35].de)) / t2) - fix_const[V35].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v51(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V51][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V87][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V51].de)) / t2) - fix_const[V51].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v87(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V87][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V151][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V87].de)) / t2) - fix_const[V87].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v151(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V151][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V203][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V151].de)) / t2) - fix_const[V151].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v203(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V203][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V255][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V203].de)) / t2) - fix_const[V203].nu;

	return ret;
}

static int s6e36w1x01_calc_reg_v255(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1;

	t1 = MULTIPLE_VREGOUT - dimming->look_volt[V255][color];

	ret = ((t1 * fix_const[V255].de) / MULTIPLE_VREGOUT) -
			fix_const[V255].nu;

	return ret;
}

int s6e36w1x01_get_gamma(struct smart_dimming *dimming,
				int index_br, unsigned char *result)
{
	int i, j;
	int ret = 0;
	int gray, index, shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br;
	int *color_shift_table = NULL;
	int (*calc_reg[NUM_VREF])(struct smart_dimming *, int)  = {
		NULL,
		s6e36w1x01_calc_reg_v0,
		s6e36w1x01_calc_reg_v3,
		s6e36w1x01_calc_reg_v11,
		s6e36w1x01_calc_reg_v23,
		s6e36w1x01_calc_reg_v35,
		s6e36w1x01_calc_reg_v51,
		s6e36w1x01_calc_reg_v87,
		s6e36w1x01_calc_reg_v151,
		s6e36w1x01_calc_reg_v203,
		s6e36w1x01_calc_reg_v255,
	};

	br = ref_cd_tbl[index_br];

	if (br > MAX_GAMMA)
		br = MAX_GAMMA;

	for (i = V0; i < NUM_VREF; i++) {
		/* get reference shift value */
		shift = gradation_shift[index_br][i];
		gray = gamma_tbl[vref_index[i]] * br;
		index = s6e36w1x01_lookup_volt_index(dimming, gray);
		index = index + shift;
#ifdef SMART_DIMMING_DEBUG
		pr_info("index : %d\n", index);
#endif
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				dimming->look_volt[i][j] =
					dimming->volt[index][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("volt : %d : %d\n",
					dimming->look_volt[i][j],
					dimming->look_volt[i][j] >> 10);
#endif
			}
		}
	}

	for (i = V0; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;
				color_shift_table = rgb_offset[index_br];
				c_shift = color_shift_table[index];
				gamma_int[i][j] =
					(calc_reg[i](dimming, j) + c_shift) -
					dimming->mtp[i][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("gamma : %d, shift : %d\n",
						gamma_int[i][j], c_shift);
#endif
				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;
			}
		}
	}

	for (j = 0; j < CI_MAX; j++)
		gamma_int[VT][j] = dimming->gamma[VT][j] - dimming->mtp[VT][j];

	index = 0;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] =
					gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else {
				result[index++] =
					(unsigned char)gamma_int[i][j];
			}
		}
	}

	result[index++] = (gamma_int[V0][CI_GREEN] << 4) | gamma_int[V0][CI_RED];
	result[index++] = gamma_int[V0][CI_BLUE];

	return ret;
}

