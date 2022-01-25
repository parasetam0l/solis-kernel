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

#ifndef _TBM_GEM_H_
#define _TBM_GEM_H_
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/tgm_drm.h>
#include <tbm.h>

#ifdef CONFIG_DRM_TBM_GEM_ION
#include <tbm_gem_ion.h>
#endif

#define to_tbm_gem_obj(x)	container_of(x,\
			struct tbm_gem_object, base)

#define IS_TBM_NONCONTIG_BUF(f)		(f & TBM_BO_NONCONTIG)
#define IS_TBM_CACHABLE_BUF(f)           (f & TBM_BO_CACHABLE)

#ifdef CONFIG_DRM_DMA_SYNC
struct tbm_gem_fence {
	struct fence		base;
	spinlock_t		lock;
	struct timer_list	timer;
};
#endif

struct tbm_gem_object {
	struct drm_gem_object		base;
	struct tbm_gem_buf	*buffer;
	unsigned long			size;
	unsigned int			flags;
#ifdef CONFIG_DRM_DMA_SYNC
	struct mutex	pending_fence_lock;
	struct fence	*pending_fence;
#endif
};

struct tbm_gem_info_data {
	struct drm_file *filp;
	struct seq_file *m;
};

struct tbm_gem_buf *tbm_init_buf(struct drm_device *dev,
				unsigned int size);
struct tbm_gem_object *tbm_gem_obj_init(struct drm_device *dev,
				unsigned long size);
void tbm_fini_buf(struct drm_device *dev,
				struct tbm_gem_buf *buffer);
int tbm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int tbm_gem_mmap_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);
int tbm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv);
int tbm_gem_info(struct seq_file *m, void *data);
dma_addr_t *tbm_gem_get_dma_addr(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp);
void tbm_gem_put_dma_addr(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp);
unsigned long tbm_gem_get_size(struct drm_device *drm_dev,
						unsigned int gem_handle,
						struct drm_file *filp);
int tbm_gem_init(struct drm_device *drm_dev);
int tbm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle);
int tbm_gem_cpu_prep_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);
int tbm_gem_cpu_fini_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file);
struct dma_buf  *tbm_gem_get_dma_buf(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp);
int tbm_gem_object_unreference(struct drm_gem_object *obj);
#endif /* _TBM_GEM_H_ */
