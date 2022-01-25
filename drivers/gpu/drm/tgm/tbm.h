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

#ifndef _TBM_H_
#define _TBM_H_
#include <tbm_gem.h>
#ifdef CONFIG_DRM_TBM_GEM_ION
#include <tbm_gem_ion.h>
#endif

struct tbm_private {
	int num_size;
	int (*gem_bufer_alloc) (struct drm_device *dev, unsigned int flags,
			struct tbm_gem_buf *buf);
	void (*gem_bufer_dealloc) (struct drm_device *dev, unsigned int flags,
			struct tbm_gem_buf *buf);
	int (*gem_prime_handle_to_fd) (struct drm_device *dev,
			struct drm_file *file_priv, uint32_t handle,
			uint32_t flags, int *prime_fd);
	int (*gem_prime_fd_to_handle) (struct drm_device *dev,
			struct drm_file *file_priv, int prime_fd, uint32_t *handle);
	void * (*gem_get_dma_addr)(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp);
	void (*gem_put_dma_addr)(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp);
	void * (*gem_get_dma_buf)(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp);
	struct gem_private *gem_priv;
};

extern int tbm_init(struct drm_device *drm_dev);

#endif /* _TBM_H_ */
