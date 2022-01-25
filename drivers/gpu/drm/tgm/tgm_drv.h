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

#ifndef _TGM_DRV_H_
#define _TGM_DRV_H_

#include <tdm.h>
#include <tbm.h>

enum tgm_drv_dev_type {
	TGM_DRV_DEV_TYPE_NONE,
	TGM_DRV_DEV_TYPE_IRQ,
};

struct tgm_drv_private {
	struct drm_device *drm_dev;
	struct tdm_private *tdm_priv;
	struct tbm_private *tbm_priv;
};

struct tgm_drv_file_private {
	pid_t pid;
	pid_t tgid;
	struct device   *pp_dev;
};

struct tgm_subdrv {
	struct list_head list;
	struct device *dev;
	struct drm_device *drm_dev;

	int (*probe)(struct drm_device *drm_dev, struct device *dev);
	void (*remove)(struct drm_device *drm_dev, struct device *dev);
	int (*open)(struct drm_device *drm_dev, struct device *dev,
			struct drm_file *file);
	void (*close)(struct drm_device *drm_dev, struct device *dev,
			struct drm_file *file);
};

int tgm_drv_component_add(struct device *dev,
		enum tgm_drv_dev_type dev_type);
void tgm_drv_component_del(struct device *dev,
		enum tgm_drv_dev_type dev_type);

int tgm_subdrv_register(struct tgm_subdrv *drm_subdrv);
int tgm_subdrv_unregister(struct tgm_subdrv *drm_subdrv);
int tgm_device_subdrv_probe(struct drm_device *dev);
int tgm_device_subdrv_remove(struct drm_device *dev);
int tgm_subdrv_open(struct drm_device *dev, struct drm_file *file);
void tgm_subdrv_close(struct drm_device *dev, struct drm_file *file);

extern struct platform_driver pp_driver;
extern struct platform_driver pp_msc_driver;
#endif /* _TGM_DRV_H_ */
