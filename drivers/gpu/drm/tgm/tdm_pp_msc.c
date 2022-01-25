/*
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 * Authors:
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *	Jonggab Park <jonggab.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>

#include <drm/drmP.h>
#include <drm/tgm_drm.h>
#include "tdm_pp.h"
#include "tdm_pp_msc.h"
#include "tdm_pp_msc_regs.h"

/*
 * SC stands for SCaler and
 * supports image scaler/rotator and input/output DMA operations.
 * input DMA reads image data from the memory.
 * output DMA writes image data to memory.
 * SC supports image rotation and image effect functions.
 *
 * M2M operation : supports crop/scale/rotation/csc/blending/
 * dithering/color fill so on.
 * Memory ----> SC H/W ----> Memory.
 */

/*
 * TODO
 * 1. check suspend/resume api if needed.
 * 2. need to check use case platform_device_id.
 * 3. check src/dst size with, height.
 * 4. need to add supported list in prop_list.
 * 5. how can we fix different C src set_size.
 */

#define SC_MAX_DEVS	1
#define SC_MAX_SRC		8
#define SC_MAX_DST		32
#define SC_RESET_TIMEOUT	50
#define SC_CLK_RATE	166750000
#define SC_REG_SZ		32
#define SC_WIDTH_ITU_709	1280
#define SC_CROP_MAX		16384
#define SC_CROP_MIN		16
#define SC_SCALE_MAX	16384
#define SC_SCALE_MIN		16
#define SC_COEF_RATIO	7
#define SC_COEF_PHASE	9
#define SC_COEF_ATTR	16
#define SC_COEF_H_8T	8
#define SC_COEF_V_4T	4
#define SC_COEF_DEPTH	3
#define SC_POS_ALIGN	2
#define SC_FMT_SHIFT	1
#define SC_UP_MAX		SC_RATIO(1, 16)
#define SC_DOWN_MIN		SC_RATIO(4, 1)
#define SC_DOWN_SWMIN		SC_RATIO(16, 1)
#define SCALE_RATIO_CONST(x, y) (u32)((1048576ULL * (x)) / (y))
#define SC_RATIO(x, y)						\
({									\
		u32 ratio;						\
		if (__builtin_constant_p(x) && __builtin_constant_p(y))	{\
			ratio = SCALE_RATIO_CONST(x, y);		\
		} else if ((x) < 2048) {				\
			ratio = (u32)((1048576UL * (x)) / (y));		\
		} else {						\
			unsigned long long dividend = 1048576ULL;	\
			dividend *= x;					\
			do_div(dividend, y);				\
			ratio = (u32)dividend;				\
		}							\
		ratio;							\
})

#define get_sc_context(dev)	platform_get_drvdata(to_platform_device(dev))
#define get_ctx_from_ppdrv(ppdrv)	\
container_of(ppdrv, struct sc_context, ppdrv)
#define sc_read(offset)		readl(ctx->regs + (offset))
#define sc_write(cfg, offset)	writel(cfg, ctx->regs + (offset))
#define ROUNDUP(x, y)		((((x) + ((y) - 1)) / (y)) * (y))

/* definition of csc type */
enum sc_csc_type {
	CSC_TYPE_NO,
	CSC_TYPE_Y2R,
	CSC_TYPE_R2Y,
};

enum sc_version {
	SC_VER_3250,
	SC_VER_5260,
	SC_VER_5410,
};

/*
 * A structure of scaler.
 *
 * @range: narrow, wide.
 * @hratio: the scaler's horizontal ratio.
 * @vratio: the scaler's vertical ratio.
 */
struct sc_scaler {
	bool	range;
	unsigned long hratio;
	unsigned long vratio;
};

/*
 * A structure of sc context.
 *
 * @ppdrv: prepare initialization using ppdrv.
 * @regs_res: register resources.
 * @regs: memory mapped io registers.
 * @sc_clk: sc a clock.
 * @sc_clk: sc p clock.
 * @sc: scaler infomations.
 * @id: sc id.
 * @irq: irq number.
 * @cur_buf_id: id of current buffer.
 * @nplanar: number of planar.
 * @pre_multi: premultiplied format.
 * @rotation: supports rotation of src.
 */
struct sc_context {
	struct tdm_ppdrv	ppdrv;
	struct resource	*regs_res;
	void __iomem	*regs;
	struct clk	*aclk;
	struct clk	*pclk;
	struct clk	*clk_chld;
	struct clk	*clk_parn;
	struct sc_scaler	sc;
	int	irq;
	int	cur_buf_id[TDM_OPS_MAX];
	int	nplanar[TDM_OPS_MAX];
	bool	pre_multi;
	bool	rotation;
};

/* Scaling coefficient value */
static const int sc_coef_8t[SC_COEF_RATIO][SC_COEF_ATTR][SC_COEF_H_8T] = {
	{
		/* 8:8  or zoom-in */
		{0, 0, 0, 0, 128, 0, 0, 0},
		{0, 1, -2, 7, 127, -6, 2, -1},
		{0, 1, -5, 16, 125, -12, 4, -1},
		{0, 2, -8, 25, 120, -15, 5, -1},
		{-1, 3, -10, 35, 114, -18, 6, -1},
		{-1, 4, -13, 46, 107, -20, 6, -1},
		{-1, 5, -16, 57, 99, -21, 7, -2},
		{-1, 5, -18, 68, 89, -20, 6, -1},
		{-1, 6, -20, 79, 79, -20, 6, -1},
		{-1, 6, -20, 89, 68, -18, 5, -1},
		{-2, 7, -21, 99, 57, -16, 5, -1},
		{-1, 6, -20, 107, 46, -13, 4, -1},
		{-1, 6, -18, 114, 35, -10, 3, -1},
		{-1, 5, -15, 120, 25, -8, 2, 0},
		{-1, 4, -12, 125, 16, -5, 1, 0},
		{-1, 2, -6, 127, 7, -2, 1, 0}
	}, {
		/* 8:7 Zoom-out */
		{0, 3, -8, 13,	111, 14, -8, 3},
		{-1, 3, -10, 21, 112, 7, -6, 2},
		{-1, 4, -12, 28, 110, 1, -4, 2},
		{-1, 4, -13, 36, 106, -3, -2, 1},
		{-1, 4, -15, 44, 103, -7, -1, 1},
		{-1, 4, -16, 53, 97, -11, 1, 1},
		{-1, 4, -16, 61, 91, -13, 2, 0},
		{-1, 4, -17, 69, 85, -15, 3, 0},
		{0, 3, -16, 77, 77, -16, 3, 0},
		{0, 3, -15, 85, 69, -17, 4, -1},
		{0, 2, -13, 91, 61, -16, 4, -1},
		{1, 1, -11, 97, 53, -16, 4, -1},
		{1, -1, -7, 103, 44, -15, 4, -1},
		{1, -2, -3, 106, 36, -13, 4, -1},
		{2, -4, 1, 110,	28, -12, 4, -1},
		{2, -6, 7, 112,	21, -10, 3, -1}
	}, {
		/* 8:6 Zoom-out */
		{0, 2, -11, 25, 96, 25, -11, 2},
		{0, 2, -12, 31, 96, 19, -10, 2},
		{0, 2, -12, 37, 94, 14, -9, 2},
		{0, 1, -12, 43, 92, 10, -8, 2},
		{0, 1, -12, 49, 90, 5, -7, 2},
		{1, 0, -12, 55, 86, 1, -5, 2},
		{1, -1, -11, 61, 82, -2, -4, 2},
		{1, -1, -9, 67, 77, -5, -3, 1},
		{1, -2, -7, 72, 72, -7, -2, 1},
		{1, -3, -5, 77, 67, -9, -1, 1},
		{2, -4, -2, 82, 61, -11, -1, 1},
		{2, -5, 1, 86, 55, -12, 0, 1},
		{2, -7, 5, 90, 49, -12, 1, 0},
		{2, -8, 10, 92, 43, -12, 1, 0},
		{2, -9, 14, 94, 37, -12, 2, 0},
		{2, -10, 19, 96, 31, -12, 2, 0}
	}, {
		/* 8:5 Zoom-out */
		{0, -1, -8, 33, 80, 33, -8, -1},
		{1, -2, -7, 37, 80, 28, -8, -1},
		{1, -2, -7, 41, 79, 24, -8, 0},
		{1, -3, -6, 46, 78, 20, -8, 0},
		{1, -3, -4, 50, 76, 16, -8, 0},
		{1, -4, -3, 54, 74, 13, -7, 0},
		{1, -5, -1, 58, 71, 10, -7, 1},
		{1, -5, 1, 62, 68, 6, -6, 1},
		{1, -6, 4, 65, 65, 4, -6, 1},
		{1, -6, 6, 68, 62, 1, -5, 1},
		{1, -7, 10, 71, 58, -1, -5, 1},
		{0, -7, 13, 74, 54, -3, -4, 1},
		{0, -8, 16, 76, 50, -4, -3, 1},
		{0, -8, 20, 78, 46, -6, -3, 1},
		{0, -8, 24, 79, 41, -7, -2, 1},
		{-1, -8, 28, 80, 37, -7, -2, 1}
	}, {
		/* 8:4 Zoom-out */
		{0, -3, 0, 35, 64, 35, 0, -3},
		{0, -3, 1, 38, 64, 32, -1, -3},
		{0, -3, 2, 41, 63, 29, -2, -2},
		{0, -4, 4, 43, 63, 27, -3, -2},
		{0, -4, 6, 46, 61, 24, -3, -2},
		{0, -4, 7, 49, 60, 21, -3, -2},
		{-1, -4, 9, 51, 59, 19, -4, -1},
		{-1, -4, 12, 53, 57, 16, -4, -1},
		{-1, -4, 14, 55, 55, 14, -4, -1},
		{-1, -4, 16, 57, 53, 12, -4, -1},
		{-1, -4, 19, 59, 51, 9, -4, -1},
		{-2, -3, 21, 60, 49, 7, -4, 0},
		{-2, -3, 24, 61, 46, 6, -4, 0},
		{-2, -3, 27, 63, 43, 4, -4, 0},
		{-2, -2, 29, 63, 41, 2, -3, 0},
		{-3, -1, 32, 64, 38, 1, -3, 0}
	}, {
		/* 8:3 Zoom-out */
		{0, -1, 8, 33, 48, 33, 8, -1},
		{-1, -1, 9, 35, 49, 31, 7, -1},
		{-1, -1, 10, 36, 49, 30, 6, -1},
		{-1, -1, 12, 38, 48, 28, 5, -1},
		{-1, 0, 13, 39, 48, 26, 4, -1},
		{-1, 0, 15, 41, 47, 24, 3, -1},
		{-1, 0, 16, 42, 47, 23, 2, -1},
		{-1, 1, 18, 43, 45, 21, 2, -1},
		{-1, 1, 19, 45, 45, 19, 1, -1},
		{-1, 2, 21, 45, 43, 18, 1, -1},
		{-1, 2, 23, 47, 42, 16, 0, -1},
		{-1, 3, 24, 47, 41, 15, 0, -1},
		{-1, 4, 26, 48, 39, 13, 0, -1},
		{-1, 5, 28, 48, 38, 12, -1, -1},
		{-1, 6, 30, 49, 36, 10, -1, -1},
		{-1, 7, 31, 49, 35, 9, -1, -1}
	},

	{	/* 8:2 Zoom-out */
		{0, 2, 13, 30, 38, 30, 13, 2},
		{0, 3, 14, 30, 38, 29, 12, 2},
		{0, 3, 15, 31, 38, 28, 11, 2},
		{0, 4, 16, 32, 38, 26, 10, 2},
		{0, 4, 17, 33, 37, 26, 10, 1},
		{0, 5, 18, 34, 37, 24, 9, 1},
		{0, 5, 19, 34, 37, 24, 8, 1},
		{1, 6, 20, 35, 36, 22, 7, 1},
		{1, 6, 21, 36, 36, 21, 6, 1},
		{1, 7, 22, 36, 35, 20, 6, 1},
		{1, 8, 24, 37, 34, 19, 5, 0},
		{1, 9, 24, 37, 34, 18, 5, 0},
		{1, 10, 26, 37, 33, 17, 4, 0},
		{2, 10, 26, 38, 32, 16, 4, 0},
		{2, 11, 28, 38, 31, 15, 3, 0},
		{2, 12, 29, 38, 30, 14, 3, 0}
	}
};

static const int sc_coef_4t[SC_COEF_RATIO][SC_COEF_ATTR][SC_COEF_V_4T] = {
	{
		/* 8:8  or zoom-in */
		{0, 0, 128, 0},
		{0, 5, 127, -4},
		{-1, 11, 124, -6},
		{-1, 19, 118, -8},
		{-2, 27, 111, -8},
		{-3, 37, 102, -8},
		{-4, 48, 92, -8},
		{-5, 59, 81, -7},
		{-6, 70, 70, -6},
		{-7, 81, 59, -5},
		{-8, 92, 48, -4},
		{-8, 102, 37, -3},
		{-8, 111, 27, -2},
		{-8, 118, 19, -1},
		{-6, 124, 11, -1},
		{-4, 127, 5, 0}
	}, {
		/* 8:7 Zoom-out  */
		{0, 8, 112, 8},
		{-1, 14, 111, 4},
		{-2, 20, 109, 1},
		{-2, 27, 105, -2},
		{-3, 34, 100, -3},
		{-3, 43, 93, -5},
		{-4, 51, 86, -5},
		{-4, 60, 77, -5},
		{-5, 69, 69, -5},
		{-5, 77, 60, -4},
		{-5, 86, 51, -4},
		{-5, 93, 43, -3},
		{-3, 100, 34, -3},
		{-2, 105, 27, -2},
		{1, 109, 20, -2},
		{4, 111, 14, -1}
	}, {
		/* 8:6 Zoom-out  */
		{0, 16, 96, 16},
		{-2, 21, 97, 12},
		{-2, 26, 96, 8},
		{-2, 32, 93, 5},
		{-2, 39, 89, 2},
		{-2, 46, 84, 0},
		{-3, 53, 79, -1},
		{-2, 59, 73, -2},
		{-2, 66, 66, -2},
		{-2, 73, 59, -2},
		{-1, 79, 53, -3},
		{0, 84, 46, -2},
		{2, 89, 39, -2},
		{5, 93, 32, -2},
		{8, 96, 26, -2},
		{12, 97, 21, -2}
	}, {
		/* 8:5 Zoom-out  */
		{0, 22, 84, 22},
		{-1, 26, 85, 18},
		{-1, 31, 84, 14},
		{-1, 36, 82, 11},
		{-1, 42, 79, 8},
		{-1, 47, 76, 6},
		{0, 52, 72, 4},
		{0, 58, 68, 2},
		{1, 63, 63, 1},
		{2, 68, 58, 0},
		{4, 72, 52, 0},
		{6, 76, 47, -1},
		{8, 79, 42, -1},
		{11, 82, 36, -1},
		{14, 84, 31, -1},
		{18, 85, 26, -1}
	}, {
		/* 8:4 Zoom-out  */
		{0, 26, 76, 26},
		{0, 30, 76, 22},
		{0, 34, 75, 19},
		{1, 38, 73, 16},
		{1, 43, 71, 13},
		{2, 47, 69, 10},
		{3, 51, 66, 8},
		{4, 55, 63, 6},
		{5, 59, 59, 5},
		{6, 63, 55, 4},
		{8, 66, 51, 3},
		{10, 69, 47, 2},
		{13, 71, 43, 1},
		{16, 73, 38, 1},
		{19, 75, 34, 0},
		{22, 76, 30, 0}
	}, {
		/* 8:3 Zoom-out */
		{0, 29, 70, 29},
		{2, 32, 68, 26},
		{2, 36, 67, 23},
		{3, 39, 66, 20},
		{3, 43, 65, 17},
		{4, 46, 63, 15},
		{5, 50, 61, 12},
		{7, 53, 58, 10},
		{8, 56, 56, 8},
		{10, 58, 53, 7},
		{12, 61, 50, 5},
		{15, 63, 46, 4},
		{17, 65, 43, 3},
		{20, 66, 39, 3},
		{23, 67, 36, 2},
		{26, 68, 32, 2}
	}, {
		/* 8:2 Zoom-out  */
		{0, 32, 64, 32},
		{3, 34, 63, 28},
		{4, 37, 62, 25},
		{4, 40, 62, 22},
		{5, 43, 61, 19},
		{6, 46, 59, 17},
		{7, 48, 58, 15},
		{9, 51, 55, 13},
		{11, 53, 53, 11},
		{13, 55, 51, 9},
		{15, 58, 48, 7},
		{17, 59, 46, 6},
		{19, 61, 43, 5},
		{22, 62, 40, 4},
		{25, 62, 37, 4},
		{28, 63, 34, 3}
	},
};

/*
 * A structure of csc table.
 *
 * @narrow_601: narrow 601.
 * @wide_601: wide 601.
 * @narrow_709: narrow 709.
 * @wide_709: wide 709.
 */
struct sc_csc_tab {
	int narrow_601[9];
	int wide_601[9];
	int narrow_709[9];
	int wide_709[9];
};

/* CSC(Color Space Conversion) coefficient value */
static struct sc_csc_tab sc_no_csc = {
	{ 0x200, 0x000, 0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x200 },
};

static struct sc_csc_tab sc_y2r = {
	/* (0,1) 601 Narrow */
	{ 0x254, 0x000, 0x331, 0x254, 0xF38, 0xE60, 0x254, 0x409, 0x000 },
	/* (0,1) 601 Wide */
	{ 0x200, 0x000, 0x2BE, 0x200, 0xF54, 0xE9B, 0x200, 0x377, 0x000 },
	/* (0,1) 709 Narrow */
	{ 0x254, 0x000, 0x331, 0x254, 0xF38, 0xE60, 0x254, 0x409, 0x000 },
	/* (0,1) 709 Wide */
	{ 0x200, 0x000, 0x314, 0x200, 0xFA2, 0xF15, 0x200, 0x3A2, 0x000 },
};

static struct sc_csc_tab sc_r2y = {
	/* (1,0) 601 Narrow */
	{ 0x084, 0x102, 0x032, 0xFB4, 0xF6B, 0x0E1, 0x0E1, 0xF44, 0xFDC },
	/* (1,0) 601 Wide  */
	{ 0x099, 0x12D, 0x03A, 0xFA8, 0xF52, 0x106, 0x106, 0xF25, 0xFD6 },
	/* (1,0) 709 Narrow */
	{ 0x05E, 0x13A, 0x020, 0xFCC, 0xF53, 0x0E1, 0x0E1, 0xF34, 0xFEC },
	/* (1,0) 709 Wide */
	{ 0x06D, 0x16E, 0x025, 0xFC4, 0xF36, 0x106, 0x106, 0xF12, 0xFE8 },
};

static struct sc_csc_tab *sc_csc_list[] = {
	[CSC_TYPE_NO] = &sc_no_csc,
	[CSC_TYPE_Y2R] = &sc_y2r,
	[CSC_TYPE_R2Y] = &sc_r2y,
};

#define SC_BLENDING_INV_BIT_OFFSET	0x10

/* define of blending compare */
enum sc_blending_comp {
	ONE = 0,
	SRC_A,
	SRC_C,
	DST_A,
	INV_SA = 0x11,
	INV_SC,
	INV_DA,
	ZERO = 0xff,
};

/*
 * A structure of blending value.
 *
 * @src_color: source color.
 * @src_alpha: source alpha.
 * @dst_color: destination color.
 * @dst_alpha: destination alpha.
 */
struct sc_blending_val {
	u32 src_color;
	u32 src_alpha;
	u32 dst_color;
	u32 dst_alpha;
};

static int sc_sw_reset(struct sc_context *ctx)
{
	u32 cfg;

	DRM_DEBUG("%s\n", __func__);

	/* s/w reset */
	cfg = sc_read(SCALER_CFG);
	cfg |= SCALER_CFG_SOFT_RST;
	sc_write(cfg, SCALER_CFG);

	return 0;
}

static void sc_handle_irq(struct sc_context *ctx, bool enable)
{
	u32 cfg;

	DRM_DEBUG("%s:enable[%d]\n", __func__, enable);

	cfg = sc_read(SCALER_INT_EN);

	if (enable)
		cfg = SCALER_INT_EN_ALL_v3;
	else
		cfg &= ~SCALER_INT_EN_ALL_v3;

	sc_write(cfg, SCALER_INT_EN);
}

static bool sc_check_rgb(u32 fmt)
{
	bool is_rgb = false;

	switch (fmt) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		is_rgb = true;
		break;
	default:
		break;
	}

	return is_rgb;
}

static int sc_src_set_fmt_nplanar(struct sc_context *ctx, u32 fmt)
{
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;

	DRM_DEBUG("%s:fmt[0x%x]\n", __func__, fmt);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
		ctx->nplanar[TDM_OPS_SRC] = 1;
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV12MT:
		ctx->nplanar[TDM_OPS_SRC] = 2;
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		ctx->nplanar[TDM_OPS_SRC] = 3;
		break;
	default:
		dev_err(ppdrv->dev, "inavlid number of planar 0x%x.\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static int sc_src_set_fmt(struct device *dev, u32 fmt)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	u32 cfg;

	DRM_DEBUG("%s:fmt[0x%x]\n", __func__, fmt);

	cfg = sc_read(SCALER_SRC_CFG);
	cfg &= ~(SCALER_CFG_TILE_EN|SCALER_CFG_FMT_MASK);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		cfg |= SCALER_CFG_FMT_RGB565;
		break;
	case DRM_FORMAT_ARGB1555:
		cfg |= SCALER_CFG_FMT_ARGB1555;
		break;
	case DRM_FORMAT_ARGB4444:
		cfg |= SCALER_CFG_FMT_ARGB4444;
		break;
	case DRM_FORMAT_XRGB8888:
		cfg |= (SCALER_CFG_FMT_ARGB8888 |
			SCALER_CFG_FMT_P_ARGB8888);
		break;
	case DRM_FORMAT_ARGB8888:
		cfg |= SCALER_CFG_FMT_ARGB8888;
		break;
	case DRM_FORMAT_YUYV:
		cfg |= SCALER_CFG_FMT_YUYV;
		break;
	case DRM_FORMAT_YVYU:
		cfg |= SCALER_CFG_FMT_YVYU;
		break;
	case DRM_FORMAT_UYVY:
		cfg |= SCALER_CFG_FMT_UYVY;
		break;
	case DRM_FORMAT_NV21:
		cfg |= SCALER_CFG_FMT_YCRCB420_2P;
		break;
	case DRM_FORMAT_NV61:
		cfg |= SCALER_CFG_FMT_YCRCB422_2P;
		break;
	case DRM_FORMAT_NV12:
		cfg |= SCALER_CFG_FMT_YCBCR420_2P;
		break;
	case DRM_FORMAT_NV16:
		cfg |= SCALER_CFG_FMT_YCBCR422_2P;
		break;
	case DRM_FORMAT_NV12MT:
		cfg |= (SCALER_CFG_FMT_YCBCR420_2P |
			SCALER_CFG_TILE_EN);
		break;
	case DRM_FORMAT_YUV422:
		cfg |= SCALER_CFG_FMT_YCBCR422_3P;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cfg |= SCALER_CFG_FMT_YCBCR420_3P;
		break;
	default:
		dev_err(ppdrv->dev, "invalid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_SRC_CFG);

	return sc_src_set_fmt_nplanar(ctx, fmt);
}

static int sc_src_set_transf(struct device *dev,
		enum tdm_degree degree,
		enum tdm_flip flip, bool *swap)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	u32 cfg;

	DRM_DEBUG("%s:degree[%d]flip[0x%x]\n", __func__,
		degree, flip);

	cfg = sc_read(SCALER_ROT_CFG);
	cfg &= ~(SCALER_FLIP_MASK | SCALER_ROT_MASK);

	switch (degree) {
	case TDM_DEGREE_0:
		if (flip & TDM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN;
		if (flip & TDM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN;
		break;
	case TDM_DEGREE_90:
		if (flip & TDM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN | SCALER_ROT_270;
		else if (flip & TDM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN | SCALER_ROT_270;
		else
			cfg |= SCALER_ROT_270;
		break;
	case TDM_DEGREE_180:
		cfg |= SCALER_ROT_180;
		break;
	case TDM_DEGREE_270:
		cfg |= SCALER_ROT_90;
		break;
	default:
		dev_err(ppdrv->dev, "inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_ROT_CFG);

	ctx->rotation = cfg &
		(SCALER_ROT_90 | SCALER_ROT_270) ? 1 : 0;
	*swap = ctx->rotation;

	return 0;
}

static int sc_src_set_size(struct device *dev, int swap,
		struct tdm_pos *pos, struct tdm_sz *sz)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_pos img_pos = *pos;
	u32 cfg;

	if (swap) {
		img_pos.w = pos->h;
		img_pos.h = pos->w;
	}

	DRM_DEBUG("%s:x[%d]y[%d]w[%d]h[%d]\n",
		__func__, pos->x, pos->y, pos->w, pos->h);

	/* pixel offset */
	cfg = (SCALER_SRC_YX(pos->x) |
		SCALER_SRC_YY(pos->y));
	sc_write(cfg, SCALER_SRC_Y_POS);

	cfg = (SCALER_SRC_YX(pos->x >> SC_FMT_SHIFT) |
		SCALER_SRC_YY(pos->y >> SC_FMT_SHIFT));
	sc_write(cfg, SCALER_SRC_C_POS);

	/* cropped size */
	cfg = (SCALER_SRC_W(pos->w) |
		SCALER_SRC_H(pos->h));
	sc_write(cfg, SCALER_SRC_WH);

	DRM_DEBUG("%s:swap[%d]hsize[%d]vsize[%d]\n",
		__func__, swap, sz->hsize, sz->vsize);

	/* span size */
	cfg = sc_read(SCALER_SRC_SPAN);
	cfg &= ~(SCALER_SRC_CSPAN_MASK |
		SCALER_SRC_YSPAN_MASK);

	cfg |= sz->hsize;

	if (ctx->nplanar[TDM_OPS_SRC] == 2)
		cfg |= sz->hsize << 16;

	if (ctx->nplanar[TDM_OPS_SRC] == 3)
		cfg |= (sz->hsize >> 1) << 16;

	sc_write(cfg, SCALER_SRC_SPAN);

	return 0;
}

static int sc_src_set_addr(struct device *dev,
		struct tdm_pp_buf_info *buf_info, u32 buf_id,
		enum tdm_pp_buf_type buf_type)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	struct tdm_pp_cmd_node *c_node = ppdrv->c_node;
	struct tdm_pp_property *property;
	struct tdm_pp_config *config;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;
	if (!property) {
		DRM_ERROR("failed to get property.\n");
		return -EINVAL;
	}

	DRM_DEBUG("%s:prop_id[%d]buf_id[%d]buf_type[%d]\n", __func__,
		property->prop_id, buf_id, buf_type);

	/* Set current buf_id */
	ctx->cur_buf_id[TDM_OPS_SRC] = buf_id;

	if (buf_id > SC_MAX_SRC) {
		dev_info(ppdrv->dev, "inavlid buf_id %d.\n", buf_id);
		return -ENOMEM;
	}

	/* address register set */
	switch (buf_type) {
	case PP_BUF_ENQUEUE:
		config = &property->config[TDM_OPS_SRC];

		sc_write(buf_info->base[TDM_PLANAR_Y],
			SCALER_SRC_Y_BASE);
		sc_write(buf_info->base[TDM_PLANAR_CB],
			SCALER_SRC_CB_BASE);
		sc_write(buf_info->base[TDM_PLANAR_CR],
			SCALER_SRC_CR_BASE);

		break;
	case PP_BUF_DEQUEUE:
	default:
		/* bypass */
		break;
	}

	return 0;
}

static struct tdm_pp_ops sc_src_ops = {
	.set_fmt = sc_src_set_fmt,
	.set_transf = sc_src_set_transf,
	.set_size = sc_src_set_size,
	.set_addr = sc_src_set_addr,
};

static int sc_dst_set_fmt_nplanar(struct sc_context *ctx, u32 fmt)
{
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	u32 cfg;

	DRM_DEBUG("%s:fmt[0x%x]\n", __func__, fmt);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		cfg = sc_read(SCALER_CFG);
		cfg &= ~SCALER_CFG_CSC_Y_OFFSET_DST;
		sc_write(cfg, SCALER_CFG);
		ctx->nplanar[TDM_OPS_DST] = 1;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
		ctx->nplanar[TDM_OPS_DST] = 1;
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV12MT:
		ctx->nplanar[TDM_OPS_DST] = 2;
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		ctx->nplanar[TDM_OPS_DST] = 3;
		break;
	default:
		dev_err(ppdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static int sc_dst_set_fmt(struct device *dev, u32 fmt)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	u32 cfg;

	DRM_DEBUG("%s:fmt[0x%x]\n", __func__, fmt);

	cfg = sc_read(SCALER_DST_CFG);
	cfg &= ~(SCALER_CFG_SWAP_MASK|SCALER_CFG_FMT_MASK);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		cfg |= SCALER_CFG_FMT_RGB565;
		break;
	case DRM_FORMAT_ARGB1555:
		cfg |= SCALER_CFG_FMT_ARGB1555;
		break;
	case DRM_FORMAT_ARGB4444:
		cfg |= SCALER_CFG_FMT_ARGB4444;
		break;
	case DRM_FORMAT_XRGB8888:
		cfg |= (SCALER_CFG_FMT_ARGB8888 |
			SCALER_CFG_FMT_P_ARGB8888);
		ctx->pre_multi = true;
		break;
	case DRM_FORMAT_ARGB8888:
		cfg |= SCALER_CFG_FMT_ARGB8888;
		break;
	case DRM_FORMAT_YUYV:
		cfg |= SCALER_CFG_FMT_YUYV;
		break;
	case DRM_FORMAT_YVYU:
		cfg |= SCALER_CFG_FMT_YVYU;
		break;
	case DRM_FORMAT_UYVY:
		cfg |= SCALER_CFG_FMT_UYVY;
		break;
	case DRM_FORMAT_NV21:
		cfg |= SCALER_CFG_FMT_YCRCB420_2P;
		break;
	case DRM_FORMAT_NV61:
		cfg |= SCALER_CFG_FMT_YCRCB422_2P;
		break;
	case DRM_FORMAT_NV12:
		cfg |= SCALER_CFG_FMT_YCBCR420_2P;
		break;
	case DRM_FORMAT_NV16:
		cfg |= SCALER_CFG_FMT_YCBCR422_2P;
		break;
	case DRM_FORMAT_NV12MT:
		cfg |= (SCALER_CFG_FMT_YCBCR420_2P |
			SCALER_CFG_TILE_EN);
		break;
	case DRM_FORMAT_YUV422:
		cfg |= SCALER_CFG_FMT_YCBCR422_3P;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cfg |= SCALER_CFG_FMT_YCBCR420_3P;
		break;
	default:
		dev_err(ppdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_DST_CFG);

	return sc_dst_set_fmt_nplanar(ctx, fmt);
}

static int sc_dst_set_transf(struct device *dev,
		enum tdm_degree degree,
		enum tdm_flip flip, bool *swap)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	u32 cfg;

	DRM_DEBUG("%s:degree[%d]flip[0x%x]\n", __func__,
		degree, flip);

	cfg = sc_read(SCALER_ROT_CFG);
	cfg &= ~(SCALER_FLIP_MASK | SCALER_ROT_MASK);

	switch (degree) {
	case TDM_DEGREE_0:
		if (flip & TDM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN;
		if (flip & TDM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN;
		break;
	case TDM_DEGREE_90:
		if (flip & TDM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN | SCALER_ROT_270;
		else if (flip & TDM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN | SCALER_ROT_270;
		else
			cfg |= SCALER_ROT_270;
		break;
	case TDM_DEGREE_180:
		cfg |= (SCALER_FLIP_X_EN | SCALER_FLIP_Y_EN);
		break;
	case TDM_DEGREE_270:
		cfg |= SCALER_ROT_90;
		break;
	default:
		dev_err(ppdrv->dev, "inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_ROT_CFG);

	ctx->rotation = cfg &
		(SCALER_ROT_90 | SCALER_ROT_270) ? 1 : 0;

	*swap = ctx->rotation;

	return 0;
}

static int sc_set_csc_coef(struct sc_context *ctx, struct sc_scaler *sc,
		u32 *fmt, int width)
{
	enum sc_csc_type csc_type = CSC_TYPE_NO;
	int *csc_eq_val, i, j;
	u32 cfg;
	bool itu_709;

	DRM_DEBUG("%s:sfmt[0x%x]dfmt[0x%x]range[%d]width[%d]\n", __func__,
		fmt[TDM_OPS_SRC], fmt[TDM_OPS_DST],
		sc->range, width);

	if (fmt[TDM_OPS_SRC] == fmt[TDM_OPS_DST]) {
		csc_eq_val = sc_csc_list[csc_type]->narrow_601;
	} else {
		if (width >= SC_WIDTH_ITU_709)
			itu_709 = true;
		else
			itu_709 = false;

		cfg = sc_read(SCALER_CFG);
		if (sc_check_rgb(fmt[TDM_OPS_DST])) {
			csc_type = CSC_TYPE_Y2R;
			if (sc->range)
				cfg &= ~SCALER_CFG_CSC_Y_OFFSET_SRC;
			else
				cfg |= SCALER_CFG_CSC_Y_OFFSET_SRC;
		} else {
			csc_type = CSC_TYPE_R2Y;
			if (sc->range)
				cfg |= SCALER_CFG_CSC_Y_OFFSET_DST;
			else
				cfg &= ~SCALER_CFG_CSC_Y_OFFSET_DST;
		}
		sc_write(cfg, SCALER_CFG);

		if (itu_709) {
			if (sc->range)
				csc_eq_val = sc_csc_list[csc_type]->wide_709;
			else
				csc_eq_val = sc_csc_list[csc_type]->narrow_709;
		} else {
			if (sc->range)
				csc_eq_val = sc_csc_list[csc_type]->wide_601;
			else
				csc_eq_val = sc_csc_list[csc_type]->narrow_601;
		}
	}

	for (i = 0, j = 0; i < SC_COEF_PHASE; i++, j += 4) {
		cfg = sc_read(SCALER_CSC_COEF00 + j);
		cfg &= ~SCALER_CSC_COEF_MASK;
		cfg |= csc_eq_val[i];
		sc_write(cfg, SCALER_CSC_COEF00 + j);
	}

	return 0;
}

static void sc_set_scaler_ratio(struct sc_context *ctx, struct sc_scaler *sc)
{
	u32 cfg;

	DRM_DEBUG("%s:hratio[%ld]vratio[%ld]\n",
		__func__, sc->hratio, sc->vratio);

	cfg = sc_read(SCALER_H_RATIO);
	cfg &= ~SCALER_RATIO_MASK;
	cfg |= sc->hratio;
	sc_write(cfg, SCALER_H_RATIO);

	cfg = sc_read(SCALER_V_RATIO);
	cfg &= ~SCALER_RATIO_MASK;
	cfg |= sc->vratio;
	sc_write(cfg, SCALER_V_RATIO);
}

static void sc_set_h_coef(struct sc_context *ctx, int coef)
{
	u32 phase, tab, cnt = 0;
	u32 cfg, val_h, val_l;

	DRM_DEBUG("%s:coef[%d]\n", __func__, coef);

	for (phase = 0; phase < SC_COEF_PHASE; phase++) {
		for (tab = SC_COEF_H_8T; tab > 0; tab -= 2, cnt++) {
			val_h = sc_coef_8t[coef][phase][tab - 1] & 0x1FF;
			val_l = sc_coef_8t[coef][phase][tab - 2] & 0x1FF;
			cfg = (val_h << 16) | (val_l << 0);
			sc_write(cfg, SCALER_YHCOEF + cnt * 0x4);
			sc_write(cfg, SCALER_CHCOEF + cnt * 0x4);
		}
	}
}

static void sc_set_v_coef(struct sc_context *ctx, int coef)
{
	u32 phase, tab, cnt = 0;
	u32 cfg, val_h, val_l;

	DRM_DEBUG("%s:coef[%d]\n", __func__, coef);

	for (phase = 0; phase < SC_COEF_PHASE; phase++) {
		for (tab = SC_COEF_V_4T; tab > 0; tab -= 2, cnt++) {
			val_h = sc_coef_4t[coef][phase][tab - 1] & 0x1FF;
			val_l = sc_coef_4t[coef][phase][tab - 2] & 0x1FF;
			cfg = (val_h << 16) | (val_l << 0);
			sc_write(cfg, SCALER_YVCOEF + cnt * 0x4);
			sc_write(cfg, SCALER_CVCOEF + cnt * 0x4);
		}
	}
}

static int sc_get_scale_filter(u32 ratio)
{
	int filter;

	if (ratio <= 65536)
		filter = 0;	/* 8:8 or zoom-in */
	else if (ratio <= 74898)
		filter = 1;	/* 8:7 zoom-out */
	else if (ratio <= 87381)
		filter = 2;	/* 8:6 zoom-out */
	else if (ratio <= 104857)
		filter = 3;	/* 8:5 zoom-out */
	else if (ratio <= 131072)
		filter = 4;	/* 8:4 zoom-out */
	else if (ratio <= 174762)
		filter = 5;	/* 8:3 zoom-out */
	else
		filter = 6;	/* 8:2 zoom-out */

	return filter;
}

static int sc_set_scaler_coef(struct sc_context *ctx, struct sc_scaler *sc)
{
	int hcoef, vcoef;

	DRM_DEBUG("%s\n", __func__);

	hcoef = sc_get_scale_filter(sc->hratio);
	vcoef = sc_get_scale_filter(sc->vratio);

	sc_set_h_coef(ctx, hcoef);
	sc_set_v_coef(ctx, vcoef);

	return 0;
}

static int sc_set_scaler(struct sc_context *ctx, struct sc_scaler *sc,
		struct tdm_pos *src, struct tdm_pos *dst)
{
	DRM_DEBUG("%s\n", __func__);

	if (ctx->rotation) {
		sc->hratio = SC_RATIO(src->h, dst->w);
		sc->vratio = SC_RATIO(src->w, dst->h);
	} else{
		sc->hratio = SC_RATIO(src->w, dst->w);
		sc->vratio = SC_RATIO(src->h, dst->h);
	}

	DRM_DEBUG("%s:hratio[%ld]vratio[%ld]\n",
		__func__, sc->hratio, sc->vratio);

	sc_set_scaler_coef(ctx, &ctx->sc);
	sc_set_scaler_ratio(ctx, &ctx->sc);

	return 0;
}

static int sc_dst_set_size(struct device *dev, int swap,
		struct tdm_pos *pos, struct tdm_sz *sz)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_pos img_pos = *pos;
	u32 cfg;

	DRM_DEBUG("%s:swap[%d]x[%d]y[%d]w[%d]h[%d]\n",
		__func__, swap, pos->x, pos->y, pos->w, pos->h);

	if (swap) {
		img_pos.w = pos->h;
		img_pos.h = pos->w;
	}

	/* pixel offset */
	cfg = (SCALER_DST_X(pos->x) |
		SCALER_DST_Y(pos->y));
	sc_write(cfg, SCALER_DST_POS);

	/* scaled size */
	cfg = (SCALER_DST_W(pos->w) | SCALER_DST_H(pos->h));
	sc_write(cfg, SCALER_DST_WH);

	DRM_DEBUG("%s:hsize[%d]vsize[%d]\n",
		__func__, sz->hsize, sz->vsize);

	/* span size */
	cfg = sc_read(SCALER_DST_SPAN);
	cfg &= ~(SCALER_DST_CSPAN_MASK |
		SCALER_DST_YSPAN_MASK);

	cfg |= sz->hsize;

	if (ctx->nplanar[TDM_OPS_DST] == 2)
		cfg |= sz->hsize << 16;

	if (ctx->nplanar[TDM_OPS_DST] == 3)
		cfg |= (sz->hsize >> 1) << 16;

	sc_write(cfg, SCALER_DST_SPAN);

	return 0;
}

static int sc_dst_set_addr(struct device *dev,
		struct tdm_pp_buf_info *buf_info, u32 buf_id,
		enum tdm_pp_buf_type buf_type)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	struct tdm_pp_cmd_node *c_node = ppdrv->c_node;
	struct tdm_pp_property *property;
	struct tdm_pp_config *config;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;

	DRM_DEBUG("%s:prop_id[%d]buf_id[%d]buf_type[%d]\n", __func__,
		property->prop_id, buf_id, buf_type);

	/* Set current buf_id */
	ctx->cur_buf_id[TDM_OPS_DST] = buf_id;

	/* address register set */
	switch (buf_type) {
	case PP_BUF_ENQUEUE:
		config = &property->config[TDM_OPS_DST];

		sc_write(buf_info->base[TDM_PLANAR_Y],
			SCALER_DST_Y_BASE);
		sc_write(buf_info->base[TDM_PLANAR_CB],
			SCALER_DST_CB_BASE);
		sc_write(buf_info->base[TDM_PLANAR_CR],
			SCALER_DST_CR_BASE);
		break;
	case PP_BUF_DEQUEUE:
	default:
		/* bypass */
		break;
	}

	return 0;
}

static struct tdm_pp_ops sc_dst_ops = {
	.set_fmt = sc_dst_set_fmt,
	.set_transf = sc_dst_set_transf,
	.set_size = sc_dst_set_size,
	.set_addr = sc_dst_set_addr,
};

#ifdef DEBUG
static void sc_print_reg(struct sc_context *ctx)
{
	struct resource *res = ctx->regs_res;
	char buf[256];
	int i, pos = 0;
	u32 cfg;

	pos += sprintf(buf+pos, "0x%.8x | ", res->start);
	for (i = 1; i < SC_MAX_REG + 1; i++) {
		cfg = sc_read((i-1) * sizeof(u32));
		pos += sprintf(buf+pos, "0x%.8x ", cfg);
		if (i % 4 == 0) {
			DRM_INFO("%s\n", buf);
			pos = 0;
			memset(buf, 0x0, 256);
			pos += sprintf(buf+pos, "0x%.8x | ",
				res->start + (i * sizeof(u32)));
		}
	}

	DRM_INFO("\n");
}
#endif

static int sc_clk_ctrl(struct sc_context *ctx, bool enable)
{
	int ret = 0;

	DRM_INFO("%s:enable[%d]\n", __func__, enable);

	if (enable) {
		if (!IS_ERR(ctx->clk_chld) && !IS_ERR(ctx->clk_parn)) {
			ret = clk_set_parent(ctx->clk_chld, ctx->clk_parn);
			if (ret)
				DRM_ERROR("set_parent:ret[%d]\n", ret);
		}

		if (!IS_ERR(ctx->pclk)) {
			ret = clk_prepare_enable(ctx->pclk);
			if (ret)
				DRM_ERROR("enable pclk:ret[%d]\n", ret);
		}

		if (!IS_ERR(ctx->aclk)) {
			ret = clk_prepare_enable(ctx->aclk);
			if (ret)
				DRM_ERROR("enable aclk:ret[%d]\n", ret);
		}
	} else {
		if (!IS_ERR(ctx->aclk))
			clk_disable_unprepare(ctx->aclk);
		if (!IS_ERR(ctx->pclk))
			clk_disable_unprepare(ctx->pclk);
	}

	return ret;
}

static irqreturn_t sc_irq_handler(int irq, void *dev_id)
{
	struct sc_context *ctx = dev_id;
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	struct tdm_pp_cmd_node *c_node = ppdrv->c_node;
	struct tdm_pp_property *property = &c_node->property;
	struct tdm_pp_event_work *event_work =
		c_node->event_work;
	u32 cfg;
	int *buf_id = ctx->cur_buf_id;

	DRM_DEBUG("%s\n", __func__);

#ifdef DEBUG
	sc_print_reg(ctx);
#endif

	cfg = sc_read(SCALER_INT_STATUS);
	cfg |= SCALER_INT_STATUS_FRAME_END;
	sc_write(cfg, SCALER_INT_STATUS);

	if (c_node->state == PP_STATE_STOP) {
		DRM_ERROR("invalid state:prop_id[%d]\n", property->prop_id);
		return IRQ_HANDLED;
	}

	DRM_DEBUG("%s:src buf_id[%d]dst buf_id[%d]\n", __func__,
		buf_id[TDM_OPS_SRC], buf_id[TDM_OPS_DST]);

	event_work->ppdrv = ppdrv;
	event_work->buf_id[TDM_OPS_SRC] = buf_id[TDM_OPS_SRC];
	event_work->buf_id[TDM_OPS_DST] = buf_id[TDM_OPS_DST];
	queue_work(ppdrv->event_workq, &event_work->work);

	return IRQ_HANDLED;
}

static int sc_init_capability(struct tdm_ppdrv *ppdrv)
{
	return 0;
}

static inline bool sc_check_drm_flip(enum tdm_flip flip)
{
	switch (flip) {
	case TDM_FLIP_NONE:
	case TDM_FLIP_VERTICAL:
	case TDM_FLIP_HORIZONTAL:
	case TDM_FLIP_BOTH:
		return true;
	default:
		DRM_INFO("%s:invalid flip\n", __func__);
		return false;
	}
}

static inline bool sc_check_fmt_limit(struct tdm_pp_property *property)
{
	struct tdm_pp_config *src_config =
					&property->config[TDM_OPS_SRC];
	struct tdm_pp_config *dst_config =
					&property->config[TDM_OPS_DST];
	struct tdm_pos src_pos = src_config->pos;
	struct tdm_pos dst_pos = dst_config->pos;
	unsigned int h_ratio, v_ratio, i;

	for_each_pp_ops(i) {
		if ((property->config[i].fmt == TDM_DEGREE_90) ||
			(property->config[i].fmt == TDM_DEGREE_270))
			swap(src_pos.w, src_pos.h);
	}

	h_ratio = SC_RATIO(src_pos.w, dst_pos.w);
	v_ratio = SC_RATIO(src_pos.h, dst_pos.h);

	if ((h_ratio > SC_DOWN_MIN) ||
			(h_ratio < SC_UP_MAX)) {
		DRM_INFO("%s:width scaling is out of range\n", __func__);
		return false;
	}

	if ((v_ratio > SC_DOWN_MIN) ||
			(v_ratio < SC_UP_MAX)) {
		DRM_INFO("%s:height scaling is out of range\n", __func__);
		return false;
	}

	return true;
}

static int sc_ppdrv_check_property(struct device *dev,
		struct tdm_pp_property *property)
{
	struct tdm_pp_config *config;
	struct tdm_pos *pos;
	struct tdm_sz *sz;
	int i;

	DRM_DEBUG("%s\n", __func__);

	for_each_pp_ops(i) {
		config = &property->config[i];
		pos = &config->pos;
		pos->w = ROUNDUP(pos->w, SC_POS_ALIGN);
		pos->h = ROUNDUP(pos->h, SC_POS_ALIGN);
		sz = &config->sz;

		DRM_DEBUG("sc:prop_id[%d]ops[%s]fmt[0x%x]\n",
			property->prop_id, i ? "dst" : "src", config->fmt);

		DRM_DEBUG("sc:pos[%d %d %d %d]sz[%d %d]f[%d]r[%d]\n",
			pos->x, pos->y, pos->w, pos->h,
			sz->hsize, sz->vsize, config->flip, config->degree);
	}

	return 0;
}

static int sc_ppdrv_reset(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct sc_scaler *sc = &ctx->sc;
	int ret;

	DRM_DEBUG("%s\n", __func__);

	/* reset h/w block */
	ret = sc_sw_reset(ctx);
	if (ret < 0) {
		dev_err(dev, "failed to reset hardware.\n");
		return ret;
	}

	/* scaler setting */
	memset(&ctx->sc, 0x0, sizeof(ctx->sc));
	sc->range = true;

	return 0;
}

static int sc_check_prepare(struct sc_context *ctx)
{
	DRM_DEBUG("%s\n", __func__);

	return 0;
}

static int sc_ppdrv_start(struct device *dev, enum tdm_pp_cmd cmd)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;
	struct tdm_pp_cmd_node *c_node = ppdrv->c_node;
	struct tdm_pp_property *property;
	struct tdm_pp_config *config;
	struct tdm_pos	img_pos[TDM_OPS_MAX];
	u32 fmt[TDM_OPS_MAX];
	u32 cfg;
	int ret, i;

	DRM_DEBUG("%s:cmd[%d]\n", __func__, cmd);

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;

	ret = sc_check_prepare(ctx);
	if (ret) {
		dev_err(dev, "failed to check prepare.\n");
		return ret;
	}

	sc_handle_irq(ctx, true);

	for_each_pp_ops(i) {
		config = &property->config[i];
		img_pos[i] = config->pos;
		fmt[i] = config->fmt;
	}

	switch (cmd) {
	case PP_CMD_M2M:
		/* bypass */
		break;
	default:
		ret = -EINVAL;
		dev_err(dev, "invalid operations.\n");
		return ret;
	}

	/* set scaler */
	ret = sc_set_scaler(ctx, &ctx->sc,
		&img_pos[TDM_OPS_SRC],
		&img_pos[TDM_OPS_DST]);
	if (ret) {
		dev_err(dev, "failed to set precalser.\n");
		return ret;
	}

	/* set coefficient */
	sc_set_csc_coef(ctx, &ctx->sc, fmt, img_pos[TDM_OPS_DST].w);

	cfg = sc_read(SCALER_CFG);
	cfg |= SCALER_CFG_START_CMD;
	sc_write(cfg, SCALER_CFG);

	return 0;
}

static void sc_ppdrv_stop(struct device *dev, enum tdm_pp_cmd cmd)
{
	struct sc_context *ctx = get_sc_context(dev);
	u32 cfg;

	DRM_DEBUG("%s:cmd[%d]\n", __func__, cmd);

	switch (cmd) {
	case PP_CMD_M2M:
		/* bypass */
		break;
	default:
		dev_err(dev, "invalid operations.\n");
		break;
	}

	sc_handle_irq(ctx, false);

	cfg = sc_read(SCALER_CFG);
	cfg &= ~SCALER_CFG_START_CMD;
	sc_write(cfg, SCALER_CFG);
}

static int sc_clk_get(struct sc_context *ctx, struct device *dev)
{
	ctx->aclk = devm_clk_get(dev, "gate");
	if (IS_ERR(ctx->aclk)) {
		if (PTR_ERR(ctx->aclk) != -ENOENT) {
			DRM_ERROR("failed to get gate clock: %ld\n",
			 PTR_ERR(ctx->aclk));
			return PTR_ERR(ctx->aclk);
		}
		DRM_INFO("%s:gate clock is not present\n", __func__);
	}

	ctx->pclk = devm_clk_get(dev, "gate2");
	if (IS_ERR(ctx->pclk)) {
		if (PTR_ERR(ctx->pclk) != -ENOENT) {
			DRM_ERROR("failed to get gate2 clock: %ld\n",
			 PTR_ERR(ctx->pclk));
			clk_put(ctx->aclk);
			return PTR_ERR(ctx->pclk);
		}
		DRM_INFO("%s:gate2 clock is not present\n", __func__);
	}

	ctx->clk_chld = devm_clk_get(dev, "mux_user");
	if (IS_ERR(ctx->clk_chld)) {
		if (PTR_ERR(ctx->clk_chld) != -ENOENT) {
			DRM_ERROR("failed to get mux_user clock: %ld\n",
			 PTR_ERR(ctx->clk_chld));
			clk_put(ctx->pclk);
			clk_put(ctx->aclk);
			return PTR_ERR(ctx->clk_chld);
		}
		DRM_INFO("%s:mux_user clock is not present\n", __func__);
	}

	if (!IS_ERR(ctx->clk_chld)) {
		ctx->clk_parn = devm_clk_get(dev, "mux_src");
		if (IS_ERR(ctx->clk_parn)) {
			DRM_ERROR("failed to get mux_src clock: %ld\n",
			 PTR_ERR(ctx->clk_parn));
			clk_put(ctx->clk_chld);
			clk_put(ctx->pclk);
			clk_put(ctx->aclk);
			return PTR_ERR(ctx->clk_parn);
		}
	} else
		ctx->clk_parn = ERR_PTR(-ENOENT);

	return 0;
}

static int sc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sc_context *ctx;
	struct resource *res;
	struct tdm_ppdrv *ppdrv;
	int ret = -EINVAL;

	DRM_INFO("%s\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	/* resource memory */
	ctx->regs_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!ctx->regs_res) {
		DRM_ERROR("failed to find registers.\n");
		ret = -ENOENT;
		goto err_ctx;
	}

	ctx->regs = devm_ioremap_resource(dev, ctx->regs_res);
	if (!ctx->regs) {
		DRM_ERROR("failed to map registers.\n");
		ret = -ENXIO;
		goto err_ctx;
	}

	/* resource irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		DRM_ERROR("failed to request irq resource.\n");
		goto err_get_regs;
	}

	ctx->irq = res->start;
	ret = request_threaded_irq(ctx->irq, NULL, sc_irq_handler,
		IRQF_ONESHOT, "drm_sc", ctx);
	if (ret < 0) {
		DRM_ERROR("failed to request irq.\n");
		goto err_get_regs;
	}

	ret = sc_clk_get(ctx, dev);
	if (ret) {
		DRM_ERROR("failed to clk get.\n");
		goto err_get_irq;
	}

	/* ToDo: iommu enable */
	ppdrv = &ctx->ppdrv;
	ppdrv->dev = dev;
	ppdrv->ops[TDM_OPS_SRC] = &sc_src_ops;
	ppdrv->ops[TDM_OPS_DST] = &sc_dst_ops;
	ppdrv->check_property = sc_ppdrv_check_property;
	ppdrv->reset = sc_ppdrv_reset;
	ppdrv->start = sc_ppdrv_start;
	ppdrv->stop = sc_ppdrv_stop;
	ret = sc_init_capability(ppdrv);
	if (ret < 0) {
		DRM_ERROR("failed to init property list.\n");
		goto err_clk;
	}

	DRM_INFO("%s:ppdrv[%p]\n", __func__, ppdrv);

	platform_set_drvdata(pdev, ctx);
	pm_runtime_enable(dev);

	ret = iovmm_activate(dev);
	if (ret < 0) {
		DRM_ERROR("failed to activate vmm\n");
		goto err_vmm_activate;
	}

	ret = tdm_ppdrv_register(ppdrv);
	if (ret < 0) {
		DRM_ERROR("failed to register drm sc device.\n");
		goto err_ppdrv_register;
	}

	DRM_INFO("%s:drm sc registered successfully.\n", __func__);

	return 0;

err_ppdrv_register:
	iovmm_deactivate(dev);
err_vmm_activate:
	pm_runtime_disable(dev);
err_clk:
	if (ctx->clk_parn)
		clk_put(ctx->clk_parn);
	if (ctx->clk_chld)
		clk_put(ctx->clk_chld);
	if (ctx->pclk)
		clk_put(ctx->pclk);
	if (ctx->aclk)
		clk_put(ctx->aclk);
err_get_irq:
	free_irq(ctx->irq, ctx);
err_get_regs:
	devm_iounmap(dev, ctx->regs);
err_ctx:
	devm_kfree(dev, ctx);
	return ret;
}

static int sc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sc_context *ctx = get_sc_context(dev);
	struct tdm_ppdrv *ppdrv = &ctx->ppdrv;

	tdm_ppdrv_unregister(ppdrv);

	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);

	free_irq(ctx->irq, ctx);
	devm_iounmap(dev, ctx->regs);

	clk_put(ctx->clk_parn);
	clk_put(ctx->clk_chld);
	clk_put(ctx->pclk);
	clk_put(ctx->aclk);

	devm_kfree(dev, ctx);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc_suspend(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_INFO("%s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	return sc_clk_ctrl(ctx, false);
}

static int sc_resume(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_INFO("%s\n", __func__);

	if (!pm_runtime_suspended(dev))
		return sc_clk_ctrl(ctx, true);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sc_runtime_suspend(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_INFO("%s\n", __func__);

	return  sc_clk_ctrl(ctx, false);
}

static int sc_runtime_resume(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_INFO("%s\n", __func__);

	return  sc_clk_ctrl(ctx, true);
}
#endif

static const struct dev_pm_ops sc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc_suspend, sc_resume)
	SET_RUNTIME_PM_OPS(sc_runtime_suspend, sc_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static const struct of_device_id tdm_pp_msc_dt_match[] = {
	{ .compatible = "samsung,exynos5-scaler",},
	{}
};
MODULE_DEVICE_TABLE(of, tdm_pp_msc_dt_match);
#endif

struct platform_driver pp_msc_driver = {
	.probe		= sc_probe,
	.remove		= sc_remove,
	.driver		= {
		.name	= "tdm-pp-sc",
		.owner	= THIS_MODULE,
		.pm	= &sc_pm_ops,
#ifdef CONFIG_OF
	.of_match_table = tdm_pp_msc_dt_match,
#endif
	},
};


