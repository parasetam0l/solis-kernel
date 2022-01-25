/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __MIFRAMMAN_H
#define __MIFRAMMAN_H

#include <linux/mutex.h>

/* TODO:  Needs to define the max mem */

struct miframman;

void miframman_init(struct miframman *ram, void *start_dram, size_t size_pool);
void *miframman_alloc(struct miframman *ram, size_t nbytes, size_t align);
void miframman_free(struct miframman *ram, void *mem);
void miframman_deinit(struct miframman *ram);

#define MIFRAMMAN_MAXMEM        (4 * 1024 * 1024)
#define MIFRAMMAN_BLOCK_SIZE    (2 * 1024)

#define MIFRAMMAN_NUM_BLOCKS    ((MIFRAMMAN_MAXMEM) / (MIFRAMMAN_BLOCK_SIZE))

#define BLOCK_FREE      0
#define BLOCK_INUSE     1
#define BLOCK_BOUND     2

/* Inclusion in core.c treat it as opaque */
struct miframman {
	void         *start_dram;
	size_t       size_pool;
	char         bitmap[MIFRAMMAN_NUM_BLOCKS]; /* Zero initialized-> all blocks free */
	u32          num_blocks;
	u32          free_mem;
	struct mutex lock;
};
#endif
