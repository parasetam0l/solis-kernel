/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DEBUG_H
#define FIMC_IS_DEBUG_H

#include "fimc-is-video.h"

#define DEBUG_SENTENCE_MAX		300

#ifdef DBG_DRAW_DIGIT
#define DBG_DIGIT_CNT		20	/* Max count of total digit */
#define DBG_DIGIT_W		16
#define DBG_DIGIT_H		32
#define DBG_DIGIT_MARGIN_W_INIT	128
#define DBG_DIGIT_MARGIN_H_INIT	64
#define DBG_DIGIT_MARGIN_W	8
#define DBG_DIGIT_MARGIN_H	2
#define DBG_DIGIT_TAG(row, col, queue, frame, digit)	\
	do {		\
		ulong addr;	\
		u32 width, height, pixelformat, bitwidth;		\
		addr = queue->buf_kva[frame->index][0];			\
		width = queue->framecfg.width;				\
		height = queue->framecfg.height;			\
		pixelformat = queue->framecfg.format->pixelformat;	\
		bitwidth = queue->framecfg.format->bitwidth;		\
		fimc_is_draw_digit(addr, width, height, pixelformat, bitwidth,	\
				row, col, digit);			\
	} while(0)
#else
#define DBG_DIGIT_TAG(row, col, queue, frame, digit)
#endif

enum fimc_is_debug_state {
	FIMC_IS_DEBUG_OPEN
};

enum dbg_dma_dump_type {
	DBG_DMA_DUMP_IMAGE,
	DBG_DMA_DUMP_META,
};

struct fimc_is_debug {
	struct dentry		*root;
	struct dentry		*logfile;
	struct dentry		*imgfile;

	/* log dump */
	size_t			read_vptr;
	struct fimc_is_minfo	*minfo;

	/* debug message */
	size_t			dsentence_pos;
	char			dsentence[DEBUG_SENTENCE_MAX];

	unsigned long		state;
};

extern struct fimc_is_debug fimc_is_debug;

int fimc_is_debug_probe(void);
int fimc_is_debug_open(struct fimc_is_minfo *minfo);
int fimc_is_debug_close(void);

void fimc_is_dmsg_init(void);
void fimc_is_dmsg_concate(const char *fmt, ...);
char *fimc_is_dmsg_print(void);
void fimc_is_print_buffer(char *buffer, size_t len);
int fimc_is_debug_dma_dump(struct fimc_is_queue *queue, u32 index, u32 vid, u32 type);
#ifdef DBG_DRAW_DIGIT
void fimc_is_draw_digit(ulong addr, int width, int height, u32 pixelformat,
		u32 bitwidth, int row_index, int col_index, u64 digit);
#endif

int imgdump_request(ulong cookie, ulong kvaddr, size_t size);

#endif
