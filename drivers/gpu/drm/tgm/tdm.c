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

static BLOCKING_NOTIFIER_HEAD(tdm_nb_list);

int tdm_nb_register(struct notifier_block *nb)
{
	DRM_INFO("%s\n", __func__);

	return blocking_notifier_chain_register(
		&tdm_nb_list, nb);
}

int tdm_nb_unregister(struct notifier_block *nb)
{
	DRM_INFO("%s\n", __func__);

	return blocking_notifier_chain_unregister(
		&tdm_nb_list, nb);
}

int tdm_nb_send_event(unsigned long val, void *v)
{
	int ret;

	DRM_DEBUG("%s:val[%ld]\n", __func__, val);

	ret = blocking_notifier_call_chain(&tdm_nb_list, val, v);

	DRM_DEBUG("%s:val[%ld]ret[0x%x]\n", __func__, val, ret);

	return ret;
}

#ifdef CONFIG_DRM_DMA_SYNC
int tdm_fence_signal(void *fence_dev, struct fence *fence)
{
	DRM_DEBUG("%s:fence[%p]\n", __func__, fence);

	if (!fence) {
		DRM_ERROR("invalid fence\n");
		return -EINVAL;
	}

	drm_fence_signal_and_put(&fence);
	fence = NULL;

	return 0;
}

struct fence *tdm_fence(void *fence_dev, struct dma_buf *buf)
{
	struct drm_device *drm_dev = (struct drm_device *)fence_dev;
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	struct reservation_object *resv = buf->resv;
	struct fence *fence = NULL;
	bool wait_resv = false;

	if (! resv) {
		DRM_ERROR("failed to get reservation_object:buf[%p]\n", buf);
		goto err;
	}

	ww_mutex_lock(&resv->lock, NULL);
	if (!reservation_object_test_signaled_rcu(resv, true))
		wait_resv = true;

	DRM_DEBUG("%s:buf[%p]%s\n", __func__, buf, wait_resv ? "[wait]" : "");

	if (wait_resv) {
		long lret;
		struct reservation_object_list *robj_list;
		struct fence *excl, *shared;

		ww_mutex_unlock(&resv->lock);
		lret = reservation_object_wait_timeout_rcu(resv, true, false,
							msecs_to_jiffies(300));
		if (!lret) {
			DRM_ERROR("timeout:buf[%p]\n", buf);
			goto err;
		} else if (lret < 0) {
			DRM_ERROR("failed to wait resv:ret[%ld]buf[%p]\n",
				lret, buf);
			goto err;
		}

		ww_mutex_lock(&resv->lock, NULL);

		robj_list = reservation_object_get_list(resv);
		excl = reservation_object_get_excl(resv);
		if (excl) {
			if (!fence_is_signaled(excl)) {
				lret = fence_wait_timeout(excl, true, msecs_to_jiffies(300));
				DRM_INFO("%s:wait:excl:buf[%p]lret[%ld]\n", __func__, buf, lret);
			}
		}

		if (robj_list) {
			int i;

			for (i = 0; i < robj_list->shared_count; i++) {
				shared = rcu_dereference_protected(
						robj_list->shared[i],
						reservation_object_held(resv));
				if (!fence_is_signaled(shared)) {
					lret = fence_wait_timeout(shared, true, msecs_to_jiffies(300));
					DRM_INFO("%s:wait:shared:idx[%d]buf[%p]lret[%ld]\n",
							__func__, i, buf, lret);
				}
			}
		}
	}

	if (reservation_object_reserve_shared(resv) < 0) {
		DRM_ERROR("failed to reserve_shared:buf[%p]\n", buf);
		goto out;
	}

	fence = drm_sw_fence_new(tdm_priv->fence_context,
				 atomic_add_return(1, &tdm_priv->fence_seqno));
	if (IS_ERR(fence)) {
		DRM_ERROR("failed to create fence:buf[%p]\n", buf);
		goto out;
	}

	reservation_object_add_shared_fence(resv, fence);
	DRM_DEBUG("%s_done:buf[%p]fence[%p]\n", __func__, buf, fence);
out:
	ww_mutex_unlock(&resv->lock);
err:
	return fence;
}
EXPORT_SYMBOL(tdm_fence);
#endif

#ifdef CONFIG_DRM_TDM_DPMS_CTRL
void tdm_free_dpms_event(struct drm_pending_event *event)
{
	DRM_INFO("%s:base[%p]\n", "free_dpms_evt", event);

	kfree(event);
}

void *tdm_get_dpms_event(struct tdm_private *tdm_priv,
		struct drm_file *file_priv, struct tdm_control_dpms *req)
{
	struct drm_device *dev = tdm_priv->drm_dev;
	struct tdm_send_dpms_event *e = NULL;
	unsigned long flags;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	bool early_dpms = tdm_priv->early_dpms;
#endif

	e = kzalloc(sizeof(*e), GFP_NOWAIT);
	if (!e) {
		DRM_ERROR("failed to allocate event.\n");
#ifdef CONFIG_DISPLAY_EARLY_DPMS
		if (early_dpms) {
			goto out;
		}
#endif
		spin_lock_irqsave(&dev->event_lock, flags);
		file_priv->event_space += sizeof(e->event);
		spin_unlock_irqrestore(&dev->event_lock, flags);
		goto out;
	}

	e->event.base.type = TDM_DPMS_EVENT;
	e->event.base.length = sizeof(e->event);
	e->event.crtc_id = req->crtc_id;
	e->event.dpms = req->dpms;
	e->event.user_data = req->user_data;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if (early_dpms)
		goto out;
#endif
	e->base.event = &e->event.base;
	e->base.file_priv = file_priv;
	e->base.destroy =  tdm_free_dpms_event;

	DRM_INFO("%s:base[%p]dpms[%d]data[0x%x]\n",
		"get_dpms_evt", &e->base, e->event.dpms,
		e->event.user_data);

out:
	return e;
}

void tdm_put_dpms_event(struct tdm_private *tdm_priv,
		struct tdm_send_dpms_event *e)
{
	struct drm_device *dev = tdm_priv->drm_dev;
	unsigned long flags;

	DRM_INFO("%s:base[%p]dpms[%d]data[0x%x]\n",
		"put_dpms_evt", &e->base, e->event.dpms,
		e->event.user_data);

	spin_lock_irqsave(&dev->event_lock, flags);
	list_add_tail(&e->base.link, &e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return;
}

static void tdm_set_dpms(struct tdm_private *tdm_priv, enum tdm_crtc_id crtc_id,
					int mode)
{
	struct tdm_nb_event event;
	int ret;

	DRM_INFO("%s:crtc_id[%d]mode[%d -> %d]\n", __func__,
		crtc_id, tdm_priv->dpms[crtc_id], mode);

	event.data = &mode;

	ret = tdm_nb_send_event(TDM_NOTI_DPMS_CTRL, (void *)&event);
	if (ret == NOTIFY_BAD) {
		DRM_ERROR("failed to TDM_NOTI_DPMS_CTRL:mode[%d]\n", mode);
		return;
	}

	switch (mode) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
		drm_vblank_on(tdm_priv->drm_dev, crtc_id);
		break;
	case DRM_MODE_DPMS_OFF:
		drm_vblank_off(tdm_priv->drm_dev, crtc_id);
		break;
	default:
		DRM_ERROR("invalid mode[%d]\n", mode);
		break;
	}

	tdm_priv->dpms[crtc_id] = mode;
	DRM_INFO("%s:crtc_id[%d]dpms[%d]done\n", __func__, crtc_id, tdm_priv->dpms[crtc_id]);

	return;
}

void tdm_dpms_work_ops(struct work_struct *work)
{
	struct tdm_dpms_work *dpms_work =
		(struct tdm_dpms_work *)work;
	struct tdm_send_dpms_event *e = dpms_work->event;
	struct tdm_private *tdm_priv = dpms_work->tdm_priv;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	bool early_dpms = dpms_work->early_dpms;
#endif

	mutex_lock(&tdm_priv->dpms_lock);

	DRM_INFO("%s:base[%p]con_id[%d]dpms[%d]data[0x%x]\n",
		"dpms_work", &e->base, e->event.crtc_id,
		e->event.dpms, e->event.user_data);

	tdm_set_dpms(tdm_priv, e->event.crtc_id, e->event.dpms);

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if (early_dpms)
		tdm_free_dpms_event(&e->base);
	else
#endif
	tdm_put_dpms_event(tdm_priv, e);

	complete_all(&tdm_priv->dpms_comp);

	DRM_INFO("%s:base[%p]done\n", "dpms_work", &e->base);

	mutex_unlock(&tdm_priv->dpms_lock);

	return;
}

int tdm_handle_dpms(struct tdm_private *tdm_priv,
		struct tdm_control_dpms *req, struct drm_file *file)
{
	int ret = 0;

	if (req->type == DPMS_EVENT_DRIVEN) {
		struct tdm_dpms_work *dpms_work;
		struct tdm_send_dpms_event *e =
			tdm_get_dpms_event(tdm_priv, file, req);

		if (!e) {
			ret = -ENOMEM;
			goto out;
		}

		reinit_completion(&tdm_priv->dpms_comp);

		dpms_work = tdm_priv->dpms_work;
		dpms_work->event = e;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
		dpms_work->early_dpms = tdm_priv->early_dpms;

		DRM_INFO("%s[%d]%s\n", "handle_dpms", req->dpms,
			dpms_work->early_dpms ? "[early]" : "");
#endif
		schedule_work((struct work_struct *)dpms_work);
	} else
		tdm_set_dpms(tdm_priv, req->crtc_id, req->dpms);

out:
	return ret;
}

int tdm_dpms_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file)
{
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	struct tdm_control_dpms *req = data;
	int ret = 0;

	DRM_INFO("[%s][%d][%s]\n", __func__, req->dpms,
		req->type ? "ASYNC" : "SYNC");

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	tdm_priv->early_dpms = false;
#endif

	ret = tdm_handle_dpms(tdm_priv, req, file);

	DRM_INFO("[%s][%d][%s]ret[%d]\n", __func__, req->dpms,
		req->type ? "ASYNC" : "SYNC", ret);

	return ret;
}
#endif

#ifdef CONFIG_DISPLAY_EARLY_DPMS
static ATOMIC_NOTIFIER_HEAD(display_early_dpms_nb_list);

int display_early_dpms_nb_register(struct notifier_block *nb)
{
	int ret;

	ret = atomic_notifier_chain_register(
		&display_early_dpms_nb_list, nb);

	DRM_INFO("%s:ret[%d]\n", __func__, ret);

	return ret;
}

int display_early_dpms_nb_unregister(struct notifier_block *nb)
{
	int ret;

	ret = atomic_notifier_chain_unregister(
		&display_early_dpms_nb_list, nb);

	DRM_INFO("%s:ret[%d]\n", __func__, ret);

	return ret;
}

int display_early_dpms_nb_send_event(unsigned long val, void *v)
{
	int ret;

	ret = atomic_notifier_call_chain(
		&display_early_dpms_nb_list, val, v);

	DRM_DEBUG("%s:ret[%d]\n", __func__, ret);

	return ret;
}

int display_early_dpms_notifier_ctrl(struct notifier_block *this,
			unsigned long cmd, void *_data)
{
	struct tdm_private *tdm_priv = container_of(this,
		struct tdm_private, nb_dpms_ctrl);
	struct display_early_dpms_nb_event *event =
		(struct display_early_dpms_nb_event *)_data;
	enum display_early_dpms_id crtc = event->id;
	int dpms = tdm_priv->dpms[crtc], ret = 0;

	switch (cmd) {
	case DISPLAY_EARLY_DPMS_MODE_SET: {
		bool early_dpms = (bool)event->data;

		DRM_INFO("%s:set:crtc[%d]mode[%d]dpms[%d]\n", "early_dpms",
			crtc, early_dpms, dpms);

		if (!tdm_priv->early_dpms && dpms == DRM_MODE_DPMS_OFF)
			tdm_priv->early_dpms = early_dpms;
	}
		break;
	case DISPLAY_EARLY_DPMS_COMMIT: {
		bool on = (bool)event->data;

		DRM_INFO("%s:commit:crtc[%d]on[%d]mode[%d]dpms[%d]\n", "early_dpms",
			crtc, on, tdm_priv->early_dpms, dpms);

#ifdef CONFIG_DRM_TDM_DPMS_CTRL
		if ((on && tdm_priv->early_dpms && dpms == DRM_MODE_DPMS_OFF &&
			pm_try_early_complete()) || (!on && dpms == DRM_MODE_DPMS_STANDBY)) {

			struct tdm_control_dpms req;

			req.crtc_id = crtc;
			req.dpms = on ? DRM_MODE_DPMS_STANDBY : DRM_MODE_DPMS_OFF;
			req.type = on ? DPMS_EVENT_DRIVEN : DPMS_SYNC_WORK;

			ret = tdm_handle_dpms(tdm_priv, &req, NULL);
		}
#endif
		tdm_priv->early_dpms = false;
	}
		break;
	case DISPLAY_EARLY_DPMS_VBLANK_SET: {
		bool on = (bool)event->data;

		DRM_INFO("%s:vbl:crtc[%d]on[%d]dpms[%d]\n", "early_dpms",
			crtc, on, dpms);

		if (on && dpms == DRM_MODE_DPMS_ON)
			drm_vblank_on(tdm_priv->drm_dev, crtc);
		else if(!on && dpms == DRM_MODE_DPMS_ON)
			drm_vblank_off(tdm_priv->drm_dev, crtc);
	}
		break;
	default:
		DRM_ERROR("unsupported cmd[0x%x]\n", (int)cmd);
		ret = -EINVAL;
		break;
	}

	if (ret)
		DRM_ERROR("cmd[%d]ret[%d]\n", (int)cmd, ret);

	return ret;
}
#endif

int tdm_init(struct drm_device *drm_dev)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv;
#ifdef CONFIG_DRM_DMA_SYNC
	struct tdm_nb_event event;
#endif
#ifdef CONFIG_DRM_TDM_DPMS_CTRL
	struct tdm_dpms_work *dpms_work;
#endif

	tdm_priv = kzalloc(sizeof(*tdm_priv), GFP_KERNEL);
	if (!tdm_priv) {
		DRM_ERROR("failed to alloc tdm dev private data.\n");
		return -ENOMEM;
	}

	/* FIXME: change hard-coding to device tree */
	tdm_priv->num_crtcs = 1;
	DRM_DEBUG("%s:num_crtcs[%d]\n", __func__, tdm_priv->num_crtcs);

	tdm_priv->drm_dev = drm_dev;
	dev_priv->tdm_priv = tdm_priv;

#ifdef CONFIG_DRM_DMA_SYNC
	tdm_priv->fence_context = fence_context_alloc(1);
	atomic_set(&tdm_priv->fence_seqno, 0);
	event.data = drm_dev;
	tdm_nb_send_event(TDM_NOTI_REGISTER, (void *)&event);
#endif

#ifdef CONFIG_DRM_TDM_DPMS_CTRL
	dpms_work = kzalloc(sizeof(*dpms_work), GFP_KERNEL);
	if (!dpms_work) {
		DRM_ERROR("failed to alloc dpms_work.\n");
		kfree(tdm_priv);
		return -ENOMEM;
	}

	dpms_work->tdm_priv = tdm_priv;
	tdm_priv->dpms_work = dpms_work;
	INIT_WORK((struct work_struct *)tdm_priv->dpms_work,
		tdm_dpms_work_ops);

	init_completion(&tdm_priv->dpms_comp);
	mutex_init(&tdm_priv->dpms_lock);
#endif
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	tdm_priv->nb_dpms_ctrl.notifier_call = display_early_dpms_notifier_ctrl;
	if (display_early_dpms_nb_register(&tdm_priv->nb_dpms_ctrl))
		DRM_ERROR("failed to register for early dpms\n");
#endif

	return tdm_irq_init(drm_dev);
}
