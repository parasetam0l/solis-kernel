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

#include <drm/drmP.h>
#include <tgm_drv.h>
#include <tbm.h>
#include <tbm_gem.h>

int tbm_init(struct drm_device *drm_dev)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tbm_private *tbm_priv;

	tbm_priv = kzalloc(sizeof(*tbm_priv), GFP_KERNEL);
	if (!tbm_priv) {
		DRM_ERROR("failed to alloc tbm dev private data.\n");
		return -ENOMEM;
	}

	dev_priv->tbm_priv = tbm_priv;

	return tbm_gem_init(drm_dev);
}