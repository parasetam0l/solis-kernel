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

#ifndef _TGM_DRM_H_
#define _TGM_DRM_H_

#include <uapi/drm/tgm_drm.h>

#ifdef CONFIG_DISPLAY_EARLY_DPMS
enum display_early_dpms_cmd {
	DISPLAY_EARLY_DPMS_MODE_SET = 0,
	DISPLAY_EARLY_DPMS_COMMIT = 1,
	DISPLAY_EARLY_DPMS_VBLANK_SET = 2,
	DISPLAY_EARLY_DPMS_MODE_MAX,
};

enum display_early_dpms_id {
	DISPLAY_EARLY_DPMS_ID_PRIMARY = 0,
	DISPLAY_EARLY_DPMS_ID_MAX,
};

struct display_early_dpms_nb_event {
	int id;
	void *data;
};

int display_early_dpms_nb_register(struct notifier_block *nb);
int display_early_dpms_nb_unregister(struct notifier_block *nb);
int display_early_dpms_nb_send_event(unsigned long val, void *v);
int display_early_dpms_notifier_ctrl(struct notifier_block *this,
		unsigned long cmd, void *_data);
#endif

#endif /* _TGM_DRM_H_ */
