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
#include <tdm_irq_exynos.h>

int tdm_irq_exynos_init(struct drm_device *drm_dev)
{
	int ret;
	/* FIXME: change hard-coding */
	int irq = 392, flags = IRQF_TRIGGER_RISING | IRQF_SHARED;

	/* drm_irq_install() only supports IRQF_SHARED */
	ret = request_irq(irq, drm_dev->driver->irq_handler,
			flags, drm_dev->driver->name, drm_dev);
	if (ret)
		DRM_ERROR("failed to request_irq:ret[%d]\n", ret);

	return ret;
}
