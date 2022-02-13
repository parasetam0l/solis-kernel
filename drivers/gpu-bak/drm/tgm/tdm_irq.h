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

#ifndef _TDM_IRQ_H_
#define _TDM_IRQ_H_

#define VBLANK_INTERVAL(x) (NSEC_PER_SEC / (x))
#define VBLANK_DEF_HZ	60

extern int tdm_irq_init(struct drm_device *dev);

#endif /* _TDM_IRQ_H_ */
