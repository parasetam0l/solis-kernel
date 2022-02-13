/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	JinYoung Jeon <jy0.jeon@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TBM_GEM_ION_H_
#define _TBM_GEM_ION_H_

#include <linux/kernel.h>
#include <linux/dma-buf.h>
#include <linux/exynos_ion.h>
#include <linux/exynos_iovmm.h>
#include <linux/ion.h>
#include <drm/drmP.h>
#include <tgm_drv.h>

struct gem_private {
	struct ion_client	*tbm_ion_client;
};

struct tbm_gem_buf {
	dma_addr_t		dma_addr;
	struct sg_table		*sgt;
	struct page		**pages;
	unsigned long		page_size;
	unsigned long		size;
	struct ion_handle	*ion_handle;
	bool			pfnmap;
	unsigned int		bufcount;
	struct drm_gem_object *obj;
	dma_addr_t		iova;
};

 int tbm_gem_ion_init(struct drm_device *drm_dev);
#endif /* _TBM_GEM_ION_H_ */
