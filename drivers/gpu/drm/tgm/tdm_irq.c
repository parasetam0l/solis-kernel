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
#include <tdm.h>
#include <tdm_irq.h>
#ifdef CONFIG_DRM_TDM_IRQ_EXYNOS
#include <tdm_irq_exynos.h>
#endif

#define TDM_IRQ_DEBUG_LOG
#ifdef TDM_IRQ_DEBUG_LOG
#define TDM_IRQ_LINFO_MAX 32

enum tdm_irq_log_state {
	TDM_IRQ_ON,
	TDM_IRQ_ON_DONE,
	TDM_IRQ_OFF,
	TDM_IRQ_OFF_DONE,
	TDM_IRQ_EVENT,
	TDM_IRQ_DEV,
	TDM_IRQ_LOG_MAX,
};

struct tdm_irq_debug {
	unsigned long long time;
};

atomic_t tdm_irq_log_idx[TDM_IRQ_LOG_MAX] = {
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1) };

struct tdm_irq_debug tdm_irq_log_info[TDM_IRQ_LOG_MAX][TDM_IRQ_LINFO_MAX];

void tdm_irq_save_info(enum tdm_irq_log_state log_state)
{
	int i;

	if (log_state >= TDM_IRQ_LOG_MAX)
		return;

	i = atomic_inc_return(&tdm_irq_log_idx[log_state]);
	if (i >= TDM_IRQ_LINFO_MAX) {
		atomic_set(&tdm_irq_log_idx[log_state], -1);
		i = 0;
	}

	tdm_irq_log_info[log_state][i].time = cpu_clock(raw_smp_processor_id());

	DRM_DEBUG("tdm_irq_log:state[%d %d]time[%lld]\n",
		log_state, i, tdm_irq_log_info[log_state][i].time);
}
#endif


static bool tdm_irq_vblank_enabled(struct drm_device *drm_dev,
			enum tdm_crtc_id crtc)
{
	unsigned long irqflags;
	bool ret;

	spin_lock_irqsave(&drm_dev->vbl_lock, irqflags);
	ret = drm_dev->vblank[crtc].enabled;
	spin_unlock_irqrestore(&drm_dev->vbl_lock, irqflags);

	return ret;
}

int tdm_irq_enable_vblank(struct drm_device *drm_dev, int crtc)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	int ret = 0;

#ifdef TDM_IRQ_DEBUG_LOG
	tdm_irq_save_info(TDM_IRQ_ON);
#endif

	if (crtc >= TDM_CRTC_MAX) {
		DRM_ERROR("crtc[%d]\n", crtc);
		return -EINVAL;
	}

	switch (crtc) {
	case TDM_CRTC_PRIMARY:
		if (hrtimer_active(&tdm_priv->virtual_vblank))
			DRM_INFO("[on_vb%d][activated]\n", crtc);

		if (hrtimer_start(&tdm_priv->virtual_vblank,
			tdm_priv->virtual_itv,
			HRTIMER_MODE_REL)) {
			tdm_priv->dbg_cnt = 3;
			DRM_INFO("[on_vb%d][busy]\n", crtc);
		}
		break;
	default:
		break;
	}

	DRM_DEBUG("[on_vb%d]\n", crtc);

#ifdef TDM_IRQ_DEBUG_LOG
	tdm_irq_save_info(TDM_IRQ_ON_DONE);
#endif

	return ret;
}

static void tdm_irq_disable_vblank(struct drm_device *drm_dev, int crtc)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;

#ifdef TDM_IRQ_DEBUG_LOG
	tdm_irq_save_info(TDM_IRQ_OFF);
#endif

	if (crtc >= TDM_CRTC_MAX) {
		DRM_ERROR("crtc[%d]\n", crtc);
		return;
	}

	switch (crtc) {
	case TDM_CRTC_PRIMARY:
		if (hrtimer_try_to_cancel(&tdm_priv->virtual_vblank) < 0) {
			DRM_INFO("[off_vb%d]busy\n", crtc);
			tdm_priv->dbg_cnt = 3;
		}
		break;
	default:
		break;
	}

	DRM_DEBUG("[off_vb%d]\n", crtc);

#ifdef TDM_IRQ_DEBUG_LOG
	tdm_irq_save_info(TDM_IRQ_OFF_DONE);
#endif
}

static void tdm_handle_vblank(struct drm_device *drm_dev,
			enum tdm_crtc_id crtc)
{
#ifdef TDM_IRQ_DEBUG_LOG
	tdm_irq_save_info(TDM_IRQ_EVENT);
#endif

	if (!drm_handle_vblank(drm_dev, crtc))
		DRM_INFO("%s[%d]disabled\n", __func__, crtc);

	return;
 }

static irqreturn_t tdm_irq_handler(int irq, void *dev_id)
{
	struct drm_device *drm_dev = (struct drm_device *)dev_id;
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	enum tdm_crtc_id crtc = TDM_CRTC_PRIMARY;
	bool enabled = tdm_irq_vblank_enabled(drm_dev, crtc);

#ifdef TDM_IRQ_DEBUG_LOG
	tdm_irq_save_info(TDM_IRQ_DEV);
#endif

#ifdef CONFIG_ENABLE_DEFAULT_TRACERS
	if (tracing_is_on()) {
		static bool t;

		__trace_printk(0, "C|-50|VSYNC|%d\n", t ? --t : ++t);
	}
#endif

	if (hrtimer_active(&tdm_priv->virtual_vblank))
		tdm_priv->virtual_rm =
			hrtimer_get_remaining(&tdm_priv->virtual_vblank);
	else
		tdm_priv->virtual_rm = ktime_set(0, 0);

	if (tdm_priv->dbg_cnt)
		DRM_INFO("[vs][%s]r[%d]rm[%lld]\n", enabled ? "on" : "off",
			atomic_read(&drm_dev->vblank[crtc].refcount),
			ktime_to_ms(tdm_priv->virtual_rm));

	return IRQ_HANDLED;
}

static void tdm_irq_preinstall(struct drm_device *drm_dev)
{
	DRM_DEBUG("%s\n", __func__);
}

static int tdm_irq_postinstall(struct drm_device *drm_dev)
{
	DRM_DEBUG("%s\n", __func__);

	return 0;
}

static void tdm_irq_uninstall(struct drm_device *drm_dev)
{
	DRM_INFO("%s\n", __func__);
}

static enum hrtimer_restart tdm_handle_virtual_vblank(struct hrtimer *timer)
{
	struct tdm_private *tdm_priv  = container_of(timer, struct tdm_private,
					       virtual_vblank);
	struct drm_device *drm_dev = tdm_priv->drm_dev;
	enum tdm_crtc_id crtc = TDM_CRTC_PRIMARY;
	enum hrtimer_restart ret = HRTIMER_NORESTART;
	ktime_t interval = ktime_sub(tdm_priv->virtual_itv,
			tdm_priv->virtual_rm);
	bool enabled = tdm_irq_vblank_enabled(drm_dev, crtc);

	if (tdm_priv->dbg_cnt) {
		DRM_INFO("[vb][%s]r[%d]itv[%lld]\n", enabled ? "on" : "off",
			atomic_read(&drm_dev->vblank[crtc].refcount),
			ktime_to_ms(interval));
		tdm_priv->dbg_cnt--;
	}

	if (enabled) {
		tdm_handle_vblank(drm_dev, crtc);
		hrtimer_forward_now(&tdm_priv->virtual_vblank, interval);
		ret = HRTIMER_RESTART;
	}

	tdm_priv->virtual_rm = ktime_set(0, 0);

	return ret;
}

int tdm_irq_init(struct drm_device *drm_dev)
{
	struct drm_driver *drm_drv = drm_dev->driver;
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	int ret;

	ret = drm_vblank_init(drm_dev, tdm_priv->num_crtcs);
	if (ret) {
		DRM_ERROR("failed to init vblank.\n");
		return ret;
	}

	drm_drv->get_vblank_counter = drm_vblank_count;
	drm_drv->get_irq = drm_platform_get_irq;
	drm_drv->enable_vblank = tdm_irq_enable_vblank;
	drm_drv->disable_vblank = tdm_irq_disable_vblank;
	drm_drv->irq_handler = tdm_irq_handler;
	drm_drv->irq_preinstall = tdm_irq_preinstall;
	drm_drv->irq_postinstall = tdm_irq_postinstall;
	drm_drv->irq_uninstall = tdm_irq_uninstall;
	drm_dev->irq_enabled = true;

	hrtimer_init(&tdm_priv->virtual_vblank, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	tdm_priv->virtual_vblank.function = tdm_handle_virtual_vblank;
	tdm_priv->virtual_itv = ktime_set(0, VBLANK_INTERVAL(VBLANK_DEF_HZ));

	drm_dev->vblank_disable_allowed = 1;
	drm_dev->max_vblank_count = 0xffffff; /* only 24 bits of frame count */

#ifdef CONFIG_DRM_TDM_IRQ_EXYNOS
	ret = tdm_irq_exynos_init(drm_dev);
#endif

	return ret;
}
