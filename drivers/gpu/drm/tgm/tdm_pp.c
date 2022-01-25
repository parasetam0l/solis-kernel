/*
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include <drm/drmP.h>
#include <drm/tgm_drm.h>
#include "tgm_drv.h"
#include "tbm_gem.h"
#include "tdm_pp.h"

/*
 * TODO
 * 1. expand command control id.
 * 2. integrate	property and config.
 * 3. removed send_event id check routine.
 * 4. compare send_event id if needed.
 * 5. free subdrv_remove notifier callback list if needed.
 * 6. need to check subdrv_open about multi-open.
 * 7. need to power_on implement power and sysmmu ctrl.
 */
#define PP_STR_LEN	16
#define get_pp_context(dev)	platform_get_drvdata(to_platform_device(dev))
#define pp_is_m2m_cmd(c)	(c == PP_CMD_M2M)

/*
 * A structure of event.
 *
 * @base: base of event.
 * @event: pp event.
 */
struct tdm_pp_send_event {
	struct drm_pending_event	base;
	struct tdm_pp_event	event;
};

/*
 * A structure of memory node.
 *
 * @list: list head to memory queue information.
 * @ops_id: id of operations.
 * @prop_id: id of property.
 * @buf_id: id of buffer.
 * @buf_info: gem objects and dma address, size.
 * @filp: a pointer to drm_file.
 */
struct tdm_pp_mem_node {
	struct list_head	list;
	enum tdm_ops_id	ops_id;
	u32	prop_id;
	u32	buf_id;
	struct tdm_pp_buf_info	buf_info;
#ifdef CONFIG_DRM_DMA_SYNC
	struct fence *fence;
#endif
};

/*
 * A structure of pp context.
 *
 * @subdrv: prepare initialization using subdrv.
 * @pp_lock: lock for synchronization of access to pp_idr.
 * @prop_lock: lock for synchronization of access to prop_idr.
 * @pp_idr: pp driver idr.
 * @prop_idr: property idr.
 * @event_workq: event work queue.
 * @cmd_workq: command work queue.
 */
struct pp_context {
	struct tgm_subdrv	subdrv;
	struct mutex	pp_lock;
	struct mutex	prop_lock;
	struct idr	pp_idr;
	struct idr	prop_idr;
	struct workqueue_struct	*event_workq;
	struct workqueue_struct	*cmd_workq;
};

static LIST_HEAD(tdm_ppdrv_list);
static DEFINE_MUTEX(tdm_ppdrv_lock);
static BLOCKING_NOTIFIER_HEAD(tdm_ppnb_list);

int tdm_ppdrv_register(struct tdm_ppdrv *ppdrv)
{
	mutex_lock(&tdm_ppdrv_lock);
	list_add_tail(&ppdrv->drv_list, &tdm_ppdrv_list);
	mutex_unlock(&tdm_ppdrv_lock);

	return 0;
}

int tdm_ppdrv_unregister(struct tdm_ppdrv *ppdrv)
{
	mutex_lock(&tdm_ppdrv_lock);
	list_del(&ppdrv->drv_list);
	mutex_unlock(&tdm_ppdrv_lock);

	return 0;
}

static int pp_create_id(struct idr *id_idr, struct mutex *lock, void *obj)
{
	int ret;

	mutex_lock(lock);
	ret = idr_alloc(id_idr, obj, 1, 0, GFP_KERNEL);
	mutex_unlock(lock);

	return ret;
}

static void pp_remove_id(struct idr *id_idr, struct mutex *lock, u32 id)
{
	mutex_lock(lock);
	idr_remove(id_idr, id);
	mutex_unlock(lock);
}

static void *pp_find_obj(struct idr *id_idr, struct mutex *lock, u32 id)
{
	void *obj;

	mutex_lock(lock);
	obj = idr_find(id_idr, id);
	mutex_unlock(lock);

	return obj;
}

static int pp_check_driver(struct tdm_ppdrv *ppdrv,
			    struct tdm_pp_property *property)
{
	if (ppdrv->dedicated || (!pp_is_m2m_cmd(property->cmd) &&
				  !pm_runtime_suspended(ppdrv->dev)))
		return -EBUSY;

	if (ppdrv->check_property &&
	    ppdrv->check_property(ppdrv->dev, property))
		return -EINVAL;

	return 0;
}

static struct tdm_ppdrv *pp_find_driver(struct pp_context *ctx,
		struct tdm_pp_property *property)
{
	struct tdm_ppdrv *ppdrv;
	u32 pp_id = property->pp_id;
	int ret;

	if (pp_id) {
		ppdrv = pp_find_obj(&ctx->pp_idr, &ctx->pp_lock, pp_id);
		if (!ppdrv) {
			DRM_DEBUG("pp%d driver not found\n", pp_id);
			return ERR_PTR(-ENODEV);
		}

		ret = pp_check_driver(ppdrv, property);
		if (ret < 0) {
			DRM_DEBUG("pp%d driver check error %d\n", pp_id, ret);
			return ERR_PTR(ret);
		} else
			return ppdrv;

	} else {
		list_for_each_entry(ppdrv, &tdm_ppdrv_list, drv_list) {
			ret = pp_check_driver(ppdrv, property);
			if (ret == 0)
				return ppdrv;
		}

		DRM_DEBUG("cannot find driver suitable for given property.\n");
	}

	return ERR_PTR(-ENODEV);
}

static struct tdm_ppdrv *pp_find_drv_by_handle(u32 prop_id)
{
	struct tdm_ppdrv *ppdrv;
	struct tdm_pp_cmd_node *c_node;
	int count = 0;

	DRM_DEBUG_KMS("prop_id[%d]\n", prop_id);

	/*
	 * This case is search pp driver by prop_id handle.
	 * sometimes, pp subsystem find driver by prop_id.
	 * e.g PAUSE state, queue buf, command control.
	 */
	list_for_each_entry(ppdrv, &tdm_ppdrv_list, drv_list) {
		DRM_DEBUG_KMS("count[%d]ppdrv[%p]\n", count++, ppdrv);

		mutex_lock(&ppdrv->cmd_lock);
		list_for_each_entry(c_node, &ppdrv->cmd_list, list) {
			if (c_node->property.prop_id == prop_id) {
				mutex_unlock(&ppdrv->cmd_lock);
				return ppdrv;
			}
		}
		mutex_unlock(&ppdrv->cmd_lock);
	}

	return ERR_PTR(-ENODEV);
}

int tdm_pp_get_permission(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	bool *enable = data;

	DRM_INFO("%s\n", __func__);

	*enable = true;

	return 0;
}

int tdm_pp_get_property(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct tgm_drv_file_private *file_priv = file->driver_priv;
	struct device *dev = file_priv->pp_dev;
	struct pp_context *ctx = get_pp_context(dev);
	struct tdm_pp_prop_list *prop_list = data;
	struct tdm_ppdrv *ppdrv;
	int count = 0;

	if (!ctx) {
		DRM_ERROR("invalid context.\n");
		return -EINVAL;
	}

	if (!prop_list) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("pp_id[%d]\n", prop_list->pp_id);

	if (!prop_list->pp_id) {
		list_for_each_entry(ppdrv, &tdm_ppdrv_list, drv_list)
			count++;

		/*
		 * Supports ppdrv list count for user application.
		 * First step user application getting ppdrv count.
		 * and second step getting ppdrv capability using pp_id.
		 */
		prop_list->count = count;
	} else {
		/*
		 * Getting ppdrv capability by pp_id.
		 * some device not supported wb, output interface.
		 * so, user application detect correct pp driver
		 * using this ioctl.
		 */
		ppdrv = pp_find_obj(&ctx->pp_idr, &ctx->pp_lock,
						prop_list->pp_id);
		if (!ppdrv) {
			DRM_ERROR("not found pp%d driver.\n",
					prop_list->pp_id);
			return -ENODEV;
		}

		*prop_list = ppdrv->prop_list;
	}

	return 0;
}

static void pp_print_property(struct tdm_pp_property *property,
		int idx)
{
	struct tdm_pp_config *config = &property->config[idx];
	struct tdm_pos *pos = &config->pos;
	struct tdm_sz *sz = &config->sz;

	DRM_INFO("pp:prop_id[%d]ops[%s]fmt[0x%x]\n",
		property->prop_id, idx ? "dst" : "src", config->fmt);

	DRM_INFO("pp:pos[%d %d %d %d]sz[%d %d]f[%d]r[%d]\n",
		pos->x, pos->y, pos->w, pos->h,
		sz->hsize, sz->vsize, config->flip, config->degree);
}

static int pp_find_and_set_property(struct tdm_pp_property *property)
{
	struct tdm_ppdrv *ppdrv;
	struct tdm_pp_cmd_node *c_node;
	u32 prop_id = property->prop_id;

	DRM_INFO("%s:prop_id[%d]\n", __func__, prop_id);

	ppdrv = pp_find_drv_by_handle(prop_id);
	if (IS_ERR(ppdrv)) {
		DRM_ERROR("failed to get pp driver.\n");
		return -EINVAL;
	}

	if (ppdrv->check_property &&
	    ppdrv->check_property(ppdrv->dev, property)) {
		DRM_ERROR("not support property.\n");
		return -EINVAL;
	}

	/*
	 * Find command node using command list in ippdrv.
	 * when we find this command no using prop_id.
	 * return property information set in this command node.
	 */
	list_for_each_entry(c_node, &ppdrv->cmd_list, list) {
		if ((c_node->property.prop_id == prop_id) &&
		    (c_node->state == PP_STATE_STOP)) {
			DRM_DEBUG_KMS("%s:found cmd[%d]ppdrv[%p]\n",
				__func__, property->cmd, ppdrv);

			c_node->property = *property;
			c_node->dbg_cnt = 2;
			return 0;
		}
	}

	DRM_ERROR("failed to search property.\n");

	return -EINVAL;
}

static struct tdm_pp_cmd_work *pp_create_cmd_work(void)
{
	struct tdm_pp_cmd_work *cmd_work;

	cmd_work = kzalloc(sizeof(*cmd_work), GFP_KERNEL);
	if (!cmd_work)
		return ERR_PTR(-ENOMEM);

	INIT_WORK((struct work_struct *)cmd_work, pp_sched_cmd);

	return cmd_work;
}

static struct tdm_pp_event_work *pp_create_event_work(void)
{
	struct tdm_pp_event_work *event_work;

	event_work = kzalloc(sizeof(*event_work), GFP_KERNEL);
	if (!event_work)
		return ERR_PTR(-ENOMEM);

	INIT_WORK(&event_work->work, pp_sched_event);

	return event_work;
}

int tdm_pp_set_property(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct tgm_drv_file_private *file_priv = file->driver_priv;
	struct device *dev = file_priv->pp_dev;
	struct pp_context *ctx = get_pp_context(dev);
	struct tdm_pp_property *property = data;
	struct tdm_ppdrv *ppdrv;
	struct tdm_pp_cmd_node *c_node;
	u32 prop_id;
	int ret, i;

	if (!ctx) {
		DRM_ERROR("invalid context.\n");
		return -EINVAL;
	}

	if (!property) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	prop_id = property->prop_id;

	/*
	 * This is log print for user application property.
	 * user application set various property.
	 */
	for_each_pp_ops(i)
		pp_print_property(property, i);

	/*
	 * set property ioctl generated new prop_id.
	 * but in this case already asigned prop_id using old set property.
	 * e.g PAUSE state. this case supports find current prop_id and use it
	 * instead of allocation.
	 */
	if (prop_id)
		return pp_find_and_set_property(property);

	/* find pp driver using pp id */
	ppdrv = pp_find_driver(ctx, property);
	if (IS_ERR(ppdrv)) {
		DRM_ERROR("failed to get pp driver.\n");
		return -EINVAL;
	}

	/* allocate command node */
	c_node = kzalloc(sizeof(*c_node), GFP_KERNEL);
	if (!c_node)
		return -ENOMEM;

	ret = pp_create_id(&ctx->prop_idr, &ctx->prop_lock, c_node);
	if (ret < 0) {
		DRM_ERROR("failed to create id.\n");
		goto err_clear;
	}
	property->prop_id = ret;

	DRM_INFO("%s:prop_id[%d]cmd[%d]ppdrv[%p]type[%d]\n", __func__,
		property->prop_id, property->cmd, ppdrv, property->type);

	/* stored property information and ppdrv in private data */
	c_node->property = *property;
	c_node->state = PP_STATE_IDLE;
	c_node->filp = file;

	if (property->type & PP_EVENT_DRIVEN) {
		/*
		 * create single thread for pp command and event.
		 * PP supports command thread for user process.
		 * user process make command node using set property ioctl.
		 * and make start_work and send this work to command thread.
		 * and then this command thread start property.
		 */
		c_node->start_work = pp_create_cmd_work();
		if (IS_ERR_OR_NULL(c_node->start_work)) {
			DRM_ERROR("failed to create start work.\n");
			goto err_remove_id;
		}

		c_node->stop_work = pp_create_cmd_work();
		if (IS_ERR_OR_NULL(c_node->stop_work)) {
			DRM_ERROR("failed to create stop work.\n");
			goto err_free_start;
		}
	}

	c_node->event_work = pp_create_event_work();
	if (IS_ERR(c_node->event_work)) {
		DRM_ERROR("failed to create event work.\n");
		goto err_free_stop;
	}

	mutex_init(&c_node->lock);
	mutex_init(&c_node->mem_lock);
	mutex_init(&c_node->event_lock);
	init_completion(&c_node->cmd_complete);
	complete(&c_node->cmd_complete);

	for_each_pp_ops(i)
		INIT_LIST_HEAD(&c_node->mem_list[i]);

	INIT_LIST_HEAD(&c_node->event_list);
	mutex_lock(&ppdrv->cmd_lock);
	list_add_tail(&c_node->list, &ppdrv->cmd_list);
	mutex_unlock(&ppdrv->cmd_lock);

	/* make dedicated state without m2m */
	if (!pp_is_m2m_cmd(property->cmd))
		ppdrv->dedicated = true;

	c_node->dbg_cnt = 2;

	return 0;

err_free_stop:
	if (property->type & PP_EVENT_DRIVEN)
		kfree(c_node->stop_work);
err_free_start:
	if (property->type & PP_EVENT_DRIVEN)
		kfree(c_node->start_work);
err_remove_id:
	pp_remove_id(&ctx->prop_idr, &ctx->prop_lock, property->prop_id);
err_clear:
	kfree(c_node);
	return ret;
}

static int pp_set_planar_addr(struct tdm_pp_buf_info *buf_info,
		u32 fmt, struct tdm_sz *sz)
{
	dma_addr_t *base[TDM_PLANAR_MAX];
	uint64_t size[TDM_PLANAR_MAX];
	uint64_t ofs[TDM_PLANAR_MAX];
	bool bypass = false;
	uint64_t tsize = 0;
	int i;

	for_each_pp_planar(i) {
		base[i] = &buf_info->base[i];
		size[i] = buf_info->size[i];
		ofs[i] = 0;
		tsize += size[i];
		if (size[i])
			DRM_DEBUG_KMS("%s:base[%d][0x%llx]s[%d][%llu]\n",
				__func__, i, *base[i], i, size[i]);
	}

	if (!tsize) {
		DRM_INFO("%s:failed to get buffer size.\n", __func__);
		return 0;
	}

	switch (fmt) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		ofs[0] = sz->hsize * sz->vsize;
		ofs[1] = ofs[0] >> 1;
		if (*base[0] && *base[1]) {
			if (size[0] + size[1] < ofs[0] + ofs[1])
				goto err_info;
			bypass = true;
		}
		break;
	case DRM_FORMAT_NV12MT:
		ofs[0] = ALIGN(ALIGN(sz->hsize, 128) *
				ALIGN(sz->vsize, 32), SZ_8K);
		ofs[1] = ALIGN(ALIGN(sz->hsize, 128) *
				ALIGN(sz->vsize >> 1, 32), SZ_8K);
		if (*base[0] && *base[1]) {
			if (size[0] + size[1] < ofs[0] + ofs[1])
				goto err_info;
			bypass = true;
		}
		break;
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		ofs[0] = sz->hsize * sz->vsize;
		ofs[1] = ofs[2] = ofs[0] >> 2;
		if (*base[0] && *base[1] && *base[2]) {
			if (size[0]+size[1]+size[2] < ofs[0]+ofs[1]+ofs[2])
				goto err_info;
			bypass = true;
		}
		break;
	case DRM_FORMAT_XRGB8888:
		ofs[0] = sz->hsize * sz->vsize << 2;
		if (*base[0]) {
			if (size[0] < ofs[0])
				goto err_info;
		}
		bypass = true;
		break;
	default:
		bypass = true;
		break;
	}

	if (!bypass) {
		*base[1] = *base[0] + ofs[0];
		if (ofs[1] && ofs[2])
			*base[2] = *base[1] + ofs[1];
	}

	DRM_DEBUG_KMS("%s:y[0x%llx],cb[0x%llx],cr[0x%llx]\n", __func__,
		*base[0], *base[1], *base[2]);

	return 0;

err_info:
	DRM_ERROR("invalid size for fmt[0x%x]\n", fmt);

	for_each_pp_planar(i) {
		base[i] = &buf_info->base[i];
		size[i] = buf_info->size[i];

		DRM_ERROR("base[%d][0x%llx]s[%d][%llu]ofs[%d][%llu]\n",
			i, *base[i], i, size[i], i, ofs[i]);
	}

	return -EINVAL;
}

static int pp_put_mem_node(struct drm_device *drm_dev,
		struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_mem_node *m_node)
{
	struct tdm_ppdrv *ppdrv;
	int i;

	if (!m_node) {
		DRM_ERROR("invalid dequeue node.\n");
		return -EFAULT;
	}

	DRM_DEBUG("%s:m_node[%p][%s]\n", __func__, m_node,
		m_node->ops_id ? "dst" : "src");

	ppdrv = pp_find_drv_by_handle(m_node->prop_id);
	if (IS_ERR(ppdrv)) {
		DRM_ERROR("failed to get pp driver.\n");
		return -EFAULT;
	}

#ifdef CONFIG_DRM_DMA_SYNC
	if (!IS_ERR(m_node->fence)) {
		if (tdm_fence_signal(drm_dev, m_node->fence)) {
			DRM_ERROR("tdm_fence_signal{%s]\n",
				m_node->ops_id ? "dst" : "src");
			c_node->dbg_cnt = 2;
		}
	} else
		DRM_INFO("%s:failed to get fence[%s]\n", __func__,
			m_node->ops_id ? "dst" : "src");
#endif

	/* put gem buffer */
	for_each_pp_planar(i) {
		unsigned long handle = m_node->buf_info.handles[i];

		if (handle)
			tbm_gem_put_dma_addr(drm_dev, ppdrv->dev,
				handle, c_node->filp);
	}

	list_del(&m_node->list);
	kfree(m_node);

	return 0;
}

static struct tdm_pp_mem_node
		*pp_get_mem_node(struct drm_device *drm_dev,
		struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_queue_buf *qbuf)
{
	struct tdm_pp_mem_node *m_node;
	struct tdm_pp_buf_info *buf_info;
	struct tdm_ppdrv *ppdrv;
	struct tdm_sz *sz;
	int i, fmt;

	ppdrv = pp_find_drv_by_handle(qbuf->prop_id);
	if (IS_ERR(ppdrv)) {
		DRM_ERROR("failed to get pp driver.\n");
		return ERR_PTR(-EFAULT);
	}

	m_node = kzalloc(sizeof(*m_node), GFP_KERNEL);
	if (!m_node)
		return ERR_PTR(-ENOMEM);

	buf_info = &m_node->buf_info;

	/* operations, buffer id */
	m_node->ops_id = qbuf->ops_id;
	m_node->prop_id = qbuf->prop_id;
	m_node->buf_id = qbuf->buf_id;
	INIT_LIST_HEAD(&m_node->list);

	DRM_DEBUG("%s:m_node[%p][%s]\n", __func__, m_node,
		m_node->ops_id ? "dst" : "src");

	if (!qbuf->handle[TDM_PLANAR_Y])
		DRM_INFO("%s:invalid handle\n", __func__);

	for_each_pp_planar(i) {
		DRM_DEBUG_KMS("i[%d]handle[%d]\n", i, qbuf->handle[i]);

		/* get dma address by handle */
		if (qbuf->handle[i]) {
			dma_addr_t *addr;
			unsigned long size;

			addr = tbm_gem_get_dma_addr(drm_dev, ppdrv->dev,
					qbuf->handle[i], c_node->filp);
			if (IS_ERR(addr)) {
				DRM_ERROR("failed to get addr.\n");
				pp_put_mem_node(drm_dev, c_node, m_node);
				return ERR_PTR(-EFAULT);
			}

			size = tbm_gem_get_size(drm_dev,
						qbuf->handle[i], c_node->filp);
			if (!size) {
				DRM_ERROR("failed to get size.\n");
				pp_put_mem_node(drm_dev, c_node, m_node);
				return ERR_PTR(-EINVAL);
			}

			buf_info->handles[i] = qbuf->handle[i];
			buf_info->base[i] = *addr;
			buf_info->size[i] = size;
			DRM_DEBUG_KMS("i[%d]base[0x%x]hdl[%ld]sz[%ld]\n", i,
				      (int)buf_info->base[i],
				      buf_info->handles[i], buf_info->size[i]);
		}
	}

	fmt = c_node->property.config[qbuf->ops_id].fmt;
	sz = &c_node->property.config[qbuf->ops_id].sz;

	if (pp_set_planar_addr(buf_info, fmt, sz)) {
		DRM_ERROR("failed to set planar addr\n");
		pp_put_mem_node(drm_dev, c_node, m_node);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&c_node->mem_lock);
	list_add_tail(&m_node->list, &c_node->mem_list[qbuf->ops_id]);
	mutex_unlock(&c_node->mem_lock);

	return m_node;
}

static void pp_clean_mem_nodes(struct drm_device *drm_dev,
			       struct tdm_pp_cmd_node *c_node, int ops)
{
	struct tdm_pp_mem_node *m_node, *tm_node;
	struct list_head *head = &c_node->mem_list[ops];

	mutex_lock(&c_node->mem_lock);

	list_for_each_entry_safe(m_node, tm_node, head, list) {
		int ret;

		ret = pp_put_mem_node(drm_dev, c_node, m_node);
		if (ret)
			DRM_ERROR("failed to put m_node.\n");
	}

	mutex_unlock(&c_node->mem_lock);
}

static void pp_free_event(struct drm_pending_event *event)
{
	kfree(event);
}

static int pp_get_event(struct drm_device *drm_dev,
		struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_queue_buf *qbuf)
{
	struct tdm_pp_send_event *e;
	unsigned long flags;

	DRM_DEBUG_KMS("ops_id[%d]buf_id[%d]\n", qbuf->ops_id, qbuf->buf_id);

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e) {
		spin_lock_irqsave(&drm_dev->event_lock, flags);
		c_node->filp->event_space += sizeof(e->event);
		spin_unlock_irqrestore(&drm_dev->event_lock, flags);
		return -ENOMEM;
	}

	/* make event */
	e->event.base.type = TDM_PP_EVENT;
	e->event.base.length = sizeof(e->event);
	e->event.user_data = qbuf->user_data;
	e->event.prop_id = qbuf->prop_id;
	e->event.buf_id[TDM_OPS_DST] = qbuf->buf_id;
	e->base.event = &e->event.base;
	e->base.file_priv = c_node->filp;
	e->base.destroy = pp_free_event;
	mutex_lock(&c_node->event_lock);
	list_add_tail(&e->base.link, &c_node->event_list);
	mutex_unlock(&c_node->event_lock);

	return 0;
}

static void pp_put_event(struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_queue_buf *qbuf)
{
	struct tdm_pp_send_event *e, *te;
	int count = 0;

	mutex_lock(&c_node->event_lock);
	list_for_each_entry_safe(e, te, &c_node->event_list, base.link) {
		DRM_DEBUG_KMS("count[%d]e[%p]\n", count++, e);

		/*
		 * qbuf == NULL condition means all event deletion.
		 * stop operations want to delete all event list.
		 * another case delete only same buf id.
		 */
		if (!qbuf) {
			/* delete list */
			list_del(&e->base.link);
			kfree(e);
		}

		/* compare buffer id */
		if (qbuf && (qbuf->buf_id ==
		    e->event.buf_id[TDM_OPS_DST])) {
			/* delete list */
			list_del(&e->base.link);
			kfree(e);
			goto out_unlock;
		}
	}

out_unlock:
	mutex_unlock(&c_node->event_lock);
}

static void pp_clean_cmd_node(struct pp_context *ctx,
				struct tdm_pp_cmd_node *c_node)
{
	int i;

	/* cancel works */
	if (c_node->property.type & PP_EVENT_DRIVEN) {
		cancel_work_sync(&c_node->start_work->work);
		cancel_work_sync(&c_node->stop_work->work);
	}

	cancel_work_sync(&c_node->event_work->work);

	/* put event */
	pp_put_event(c_node, NULL);

	for_each_pp_ops(i)
		pp_clean_mem_nodes(ctx->subdrv.drm_dev, c_node, i);

	/* delete list */
	list_del(&c_node->list);

	pp_remove_id(&ctx->prop_idr, &ctx->prop_lock,
			c_node->property.prop_id);

	/* destroy mutex */
	mutex_destroy(&c_node->lock);
	mutex_destroy(&c_node->mem_lock);
	mutex_destroy(&c_node->event_lock);

	/* free command node */
	if (c_node->property.type & PP_EVENT_DRIVEN) {
		/* free command node */
		kfree(c_node->start_work);
		kfree(c_node->stop_work);
	}

	kfree(c_node->event_work);
	kfree(c_node);
}

static bool pp_check_mem_list(struct tdm_pp_cmd_node *c_node)
{
	bool ret = true;

	switch (c_node->property.cmd) {
	case PP_CMD_WB:
		if (list_empty(&c_node->mem_list[TDM_OPS_DST]))
			ret = false;
		break;
	case PP_CMD_OUTPUT:
		if (list_empty(&c_node->mem_list[TDM_OPS_SRC]))
			ret = false;
		break;
	case PP_CMD_M2M: {
		struct list_head *head;
		struct tdm_pp_mem_node *m_node;
		int i, max = 10, count[TDM_OPS_MAX] = { 0, };

		for_each_pp_ops(i) {
			/* source/destination memory list */
			head = &c_node->mem_list[i];

			if (list_empty(head)) {
				ret = false;
				continue;
			}

			/* find memory node entry */
			list_for_each_entry(m_node, head, list) {
				DRM_DEBUG("%s:%s,count[%d]m_node[%p]\n",
					__func__, i ? "dst" : "src",
					count[i], m_node);
				count[i]++;
			}

			if (count[i] >= max)
				ret = false;
		}

		if (!ret) {
			DRM_INFO("%s:invalid mem list[%d %d]\n", __func__,
				count[TDM_OPS_SRC], count[TDM_OPS_DST]);
			c_node->dbg_cnt = 2;
		}
	}
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static struct tdm_pp_mem_node
		*pp_find_mem_node(struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_queue_buf *qbuf)
{
	struct tdm_pp_mem_node *m_node;
	struct list_head *head;
	int count = 0;

	DRM_DEBUG_KMS("buf_id[%d]\n", qbuf->buf_id);

	/* source/destination memory list */
	head = &c_node->mem_list[qbuf->ops_id];

	/* find memory node from memory list */
	list_for_each_entry(m_node, head, list) {
		DRM_DEBUG_KMS("count[%d]m_node[%p]\n", count++, m_node);

		/* compare buffer id */
		if (m_node->buf_id == qbuf->buf_id)
			return m_node;
	}

	return NULL;
}

static int pp_set_mem_node(struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_mem_node *m_node)
{
	struct tdm_pp_ops *ops = NULL;
#ifdef CONFIG_DRM_DMA_SYNC
	struct dma_buf *dma_buf;
#endif
	int ret = 0;

	if (!m_node) {
		DRM_ERROR("invalid queue node.\n");
		return -EFAULT;
	}

	DRM_DEBUG("%s:m_node[%p][%s]\n", __func__, m_node,
		m_node->ops_id ? "dst" : "src");

	/* get operations callback */
	ops = ppdrv->ops[m_node->ops_id];
	if (!ops) {
		DRM_ERROR("not support ops.\n");
		return -EFAULT;
	}


	if (!ops->set_addr) {
		DRM_ERROR("not support set_addr.\n");
		return -EFAULT;
	}

#ifdef CONFIG_DRM_DMA_SYNC
	dma_buf = tbm_gem_get_dma_buf(ppdrv->drm_dev, ppdrv->dev,
			m_node->buf_info.handles[0], c_node->filp);
	if (!IS_ERR(dma_buf)) {
		m_node->fence = tdm_fence(ppdrv->drm_dev, dma_buf);
		if (IS_ERR(m_node->fence))
			DRM_INFO("%s:failed to get fence[%s]\n", __func__,
				m_node->ops_id ? "dst" : "src");
	} else
		DRM_INFO("%s:failed to get dma_buf[%s]\n", __func__,
			m_node->ops_id ? "dst" : "src");
#endif

	/* set address and enable irq */
	ret = ops->set_addr(ppdrv->dev, &m_node->buf_info,
		m_node->buf_id, PP_BUF_ENQUEUE);
	if (ret) {
		DRM_ERROR("failed to set addr.\n");
		return ret;
	}

	return ret;
}

static void pp_handle_cmd_work(struct device *dev,
		struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_work *cmd_work,
		struct tdm_pp_cmd_node *c_node)
{
	struct pp_context *ctx = get_pp_context(dev);

	cmd_work->ppdrv = ppdrv;
	cmd_work->c_node = c_node;
	queue_work(ctx->cmd_workq, &cmd_work->work);
}

static int pp_queue_buf_with_run(struct device *dev,
		struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_mem_node *m_node,
		struct tdm_pp_queue_buf *qbuf)
{
	struct tdm_ppdrv *ppdrv;
	struct tdm_pp_property *property;
	struct tdm_pp_ops *ops;
	int ret;

	ppdrv = pp_find_drv_by_handle(qbuf->prop_id);
	if (IS_ERR(ppdrv)) {
		DRM_ERROR("failed to get pp driver.\n");
		return -EFAULT;
	}

	ops = ppdrv->ops[qbuf->ops_id];
	if (!ops) {
		DRM_ERROR("failed to get ops.\n");
		return -EFAULT;
	}

	property = &c_node->property;

	if (c_node->state != PP_STATE_START) {
		DRM_DEBUG_KMS("bypass for invalid state.\n");
		return 0;
	}

	mutex_lock(&c_node->mem_lock);
	if (!pp_check_mem_list(c_node)) {
		mutex_unlock(&c_node->mem_lock);
		DRM_DEBUG_KMS("empty memory.\n");
		return 0;
	}

	/*
	 * If set destination buffer and enabled clock,
	 * then m2m operations need start operations at queue_buf
	 */
	if (pp_is_m2m_cmd(property->cmd)) {
		if (property->type & PP_EVENT_DRIVEN) {
			struct tdm_pp_cmd_work *cmd_work = c_node->start_work;

			cmd_work->ctrl = PP_CTRL_PLAY;
			pp_handle_cmd_work(dev, ppdrv, cmd_work, c_node);
		} else {
			mutex_lock(&ppdrv->drv_lock);

			ret = pp_start_property(ppdrv, c_node);
			if (ret) {
				DRM_INFO("%s:failed to start property:id[%d]\n"
				, __func__, c_node->property.prop_id);
				pp_stop_property(ppdrv->drm_dev, ppdrv, c_node);
			}

			mutex_unlock(&ppdrv->drv_lock);
		}
	} else {
		ret = pp_set_mem_node(ppdrv, c_node, m_node);
		if (ret) {
			mutex_unlock(&c_node->mem_lock);
			DRM_ERROR("failed to set m node.\n");
			return ret;
		}
	}
	mutex_unlock(&c_node->mem_lock);

	return 0;
}

static void pp_clean_queue_buf(struct drm_device *drm_dev,
		struct tdm_pp_cmd_node *c_node,
		struct tdm_pp_queue_buf *qbuf)
{
	struct tdm_pp_mem_node *m_node, *tm_node;

	/* delete list */
	mutex_lock(&c_node->mem_lock);
	list_for_each_entry_safe(m_node, tm_node,
		&c_node->mem_list[qbuf->ops_id], list) {
		if (m_node->buf_id == qbuf->buf_id &&
		    m_node->ops_id == qbuf->ops_id)
			pp_put_mem_node(drm_dev, c_node, m_node);
	}
	mutex_unlock(&c_node->mem_lock);
}

int tdm_pp_queue_buf(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct tgm_drv_file_private *file_priv = file->driver_priv;
	struct device *dev = file_priv->pp_dev;
	struct pp_context *ctx = get_pp_context(dev);
	struct tdm_pp_queue_buf *qbuf = data;
	struct tdm_pp_cmd_node *c_node;
	struct tdm_pp_mem_node *m_node;
	int ret;

	if (!qbuf) {
		DRM_ERROR("invalid buf parameter.\n");
		return -EINVAL;
	}

	if (qbuf->ops_id >= TDM_OPS_MAX) {
		DRM_ERROR("invalid ops parameter.\n");
		return -EINVAL;
	}

	/* find command node */
	c_node = pp_find_obj(&ctx->prop_idr, &ctx->prop_lock,
		qbuf->prop_id);
	if (!c_node || c_node->filp != file) {
		DRM_ERROR("failed to get command node.\n");
		return -ENODEV;
	}

	if (c_node->dbg_cnt)
		DRM_INFO("%s:prop_id[%d][%s]buf_id[%d][%s]\n",
			__func__, qbuf->prop_id, qbuf->ops_id ? "dst" : "src",
			qbuf->buf_id, qbuf->buf_type ? "dq" : "eq");

	/* buffer control */
	switch (qbuf->buf_type) {
	case PP_BUF_ENQUEUE:
		/* get memory node */
		m_node = pp_get_mem_node(drm_dev, c_node, qbuf);
		if (IS_ERR(m_node)) {
			DRM_ERROR("failed to get m_node.\n");
			return PTR_ERR(m_node);
		}

		/*
		 * first step get event for destination buffer.
		 * and second step when M2M case run with destination buffer
		 * if needed.
		 */
		if (qbuf->ops_id == TDM_OPS_DST) {
			/* get event for destination buffer */
			ret = pp_get_event(drm_dev, c_node, qbuf);
			if (ret) {
				DRM_ERROR("failed to get event.\n");
				goto err_clean_node;
			}

			/*
			 * M2M case run play control for streaming feature.
			 * other case set address and waiting.
			 */
			ret = pp_queue_buf_with_run(dev, c_node, m_node, qbuf);
			if (ret) {
				DRM_ERROR("failed to run command.\n");
				goto err_clean_node;
			}
		}
		break;
	case PP_BUF_DEQUEUE:
		mutex_lock(&c_node->lock);

		/* put event for destination buffer */
		if (qbuf->ops_id == TDM_OPS_DST)
			pp_put_event(c_node, qbuf);

		pp_clean_queue_buf(drm_dev, c_node, qbuf);

		mutex_unlock(&c_node->lock);
		break;
	default:
		DRM_ERROR("invalid buffer control.\n");
		return -EINVAL;
	}

	return 0;

err_clean_node:
	DRM_ERROR("clean memory nodes.\n");

	pp_clean_queue_buf(drm_dev, c_node, qbuf);
	return ret;
}

static bool tdm_pp_check_valid(struct device *dev,
		enum tdm_pp_ctrl ctrl, enum tdm_pp_state state)
{
	if (ctrl != PP_CTRL_PLAY) {
		if (pm_runtime_suspended(dev)) {
			DRM_ERROR("pm:runtime_suspended.\n");
			goto err_status;
		}
	}

	switch (ctrl) {
	case PP_CTRL_PLAY:
		if (state != PP_STATE_IDLE)
			goto err_status;
		break;
	case PP_CTRL_STOP:
		break;
	case PP_CTRL_PAUSE:
		if (state != PP_STATE_START)
			goto err_status;
		break;
	case PP_CTRL_RESUME:
		if (state != PP_STATE_STOP)
			goto err_status;
		break;
	default:
		DRM_ERROR("invalid state.\n");
		goto err_status;
	}

	return true;

err_status:
	DRM_INFO("%s:ctrl[%d]state[%d]\n", __func__, ctrl, state);
	return false;
}

int tdm_pp_cmd_ctrl(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct tgm_drv_file_private *file_priv = file->driver_priv;
	struct tdm_ppdrv *ppdrv = NULL;
	struct device *dev = file_priv->pp_dev;
	struct pp_context *ctx = get_pp_context(dev);
	struct tdm_pp_cmd_ctrl *cmd_ctrl = data;
	struct tdm_pp_cmd_node *c_node;
	struct tdm_pp_property *property;
	int ret = 0;

	if (!ctx) {
		DRM_ERROR("invalid context.\n");
		return -EINVAL;
	}

	if (!cmd_ctrl) {
		DRM_ERROR("invalid control parameter.\n");
		return -EINVAL;
	}

	ppdrv = pp_find_drv_by_handle(cmd_ctrl->prop_id);
	if (IS_ERR(ppdrv)) {
		DRM_ERROR("failed to get pp driver.\n");
		return PTR_ERR(ppdrv);
	}

	c_node = pp_find_obj(&ctx->prop_idr, &ctx->prop_lock,
		cmd_ctrl->prop_id);
	if (!c_node || c_node->filp != file) {
		DRM_ERROR("invalid command node list.\n");
		return -ENODEV;
	}

	property = &c_node->property;

	if (!tdm_pp_check_valid(ppdrv->dev, cmd_ctrl->ctrl,
	    c_node->state)) {
		DRM_DEBUG("invalid state.\n");
		return -EINVAL;
	}

	switch (cmd_ctrl->ctrl) {
	case PP_CTRL_PLAY:
		DRM_INFO("%s:PLAY:prop_id[%d]\n", __func__,
			cmd_ctrl->prop_id);

		if (pm_runtime_suspended(ppdrv->dev))
			pm_runtime_get_sync(ppdrv->dev);

		c_node->state = PP_STATE_START;

		mutex_lock(&ppdrv->drv_lock);
		ret = pp_start_property(ppdrv, c_node);
		if (ret) {
			DRM_INFO("%s:failed to start property:id[%d]\n"
			, __func__, property->prop_id);
			mutex_unlock(&ppdrv->drv_lock);
			goto err;
		}
		mutex_unlock(&ppdrv->drv_lock);
		break;
	case PP_CTRL_STOP:
		DRM_INFO("%s:STOP:prop_id[%d]\n", __func__,
			cmd_ctrl->prop_id);

		mutex_lock(&ppdrv->drv_lock);
		ret = pp_stop_property(ppdrv->drm_dev, ppdrv,
			c_node);
		if (ret) {
			DRM_ERROR("failed to stop property.\n");
			mutex_unlock(&ppdrv->drv_lock);
			goto err;
		}
		mutex_unlock(&ppdrv->drv_lock);

		c_node->state = PP_STATE_STOP;
		ppdrv->dedicated = false;
		mutex_lock(&ppdrv->cmd_lock);
		pp_clean_cmd_node(ctx, c_node);

		if (list_empty(&ppdrv->cmd_list))
			pm_runtime_put_sync(ppdrv->dev);
		mutex_unlock(&ppdrv->cmd_lock);
		break;
	case PP_CTRL_PAUSE:
		if (c_node->dbg_cnt)
			DRM_INFO("%s:PAUSE:prop_id[%d]\n", __func__,
				cmd_ctrl->prop_id);

		mutex_lock(&ppdrv->drv_lock);
		ret = pp_stop_property(ppdrv->drm_dev, ppdrv,
			c_node);
		if (ret) {
			DRM_ERROR("failed to stop property.\n");
			mutex_unlock(&ppdrv->drv_lock);
			goto err;
		}
		mutex_unlock(&ppdrv->drv_lock);

		c_node->state = PP_STATE_STOP;
		break;
	case PP_CTRL_RESUME:
		if (c_node->dbg_cnt)
			DRM_INFO("%s:RESUME:prop_id[%d]\n", __func__,
				cmd_ctrl->prop_id);

		c_node->state = PP_STATE_START;

		mutex_lock(&ppdrv->drv_lock);
		ret = pp_start_property(ppdrv, c_node);
		if (ret) {
			DRM_INFO("%s:failed to start property:id[%d]\n"
			, __func__, property->prop_id);
			mutex_unlock(&ppdrv->drv_lock);
			goto err;
		}
		mutex_unlock(&ppdrv->drv_lock);
		break;
	default:
		DRM_ERROR("could not support this state currently.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("done ctrl[%d]prop_id[%d]\n",
		cmd_ctrl->ctrl, cmd_ctrl->prop_id);
err:
	return ret;
}

int tdm_ppnb_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
		&tdm_ppnb_list, nb);
}

int tdm_ppnb_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
		&tdm_ppnb_list, nb);
}

int tdm_ppnb_send_event(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(
		&tdm_ppnb_list, val, v);
}

static int pp_set_property(struct tdm_ppdrv *ppdrv,
		struct tdm_pp_property *property)
{
	struct tdm_pp_ops *ops = NULL;
	bool swap = false;
	int ret, i;

	if (!property) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("prop_id[%d]\n", property->prop_id);

	/* reset h/w block */
	if (ppdrv->reset &&
	    ppdrv->reset(ppdrv->dev)) {
		return -EINVAL;
	}

	/* set source,destination operations */
	for_each_pp_ops(i) {
		struct tdm_pp_config *config =
			&property->config[i];

		ops = ppdrv->ops[i];
		if (!ops || !config) {
			DRM_ERROR("not support ops and config.\n");
			return -EINVAL;
		}

		/* set format */
		if (ops->set_fmt) {
			ret = ops->set_fmt(ppdrv->dev, config->fmt);
			if (ret)
				return ret;
		}

		/* set transform for rotation, flip */
		if (ops->set_transf) {
			ret = ops->set_transf(ppdrv->dev, config->degree,
				config->flip, &swap);
			if (ret)
				return ret;
		}

		/* set size */
		if (ops->set_size) {
			ret = ops->set_size(ppdrv->dev, swap, &config->pos,
				&config->sz);
			if (ret)
				return ret;
		}
	}

	return 0;
}

void pp_wait_completion(struct tdm_pp_cmd_node *c_node)
{
	int ret;

	DRM_DEBUG("%s\n", __func__);

	ret = wait_for_completion_timeout(
		&c_node->cmd_complete, msecs_to_jiffies(120));
	if (ret < 0)
		DRM_ERROR("failed to wait completion:ret[%d]\n", ret);
	else if (!ret)
		DRM_ERROR("timeout\n");
}

int pp_start_property(struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node)
{
	struct tdm_pp_mem_node *m_node;
	struct tdm_pp_property *property = &c_node->property;
	struct list_head *head;
	int ret, i;

	DRM_DEBUG_KMS("prop_id[%d]\n", property->prop_id);

	/* store command info in ppdrv */
	ppdrv->c_node = c_node;

	mutex_lock(&c_node->mem_lock);
	if (!pp_check_mem_list(c_node)) {
		DRM_DEBUG_KMS("empty memory.\n");
		ret = -ENOMEM;
		goto err_unlock;
	}

	reinit_completion(&c_node->cmd_complete);

	/* set current property in ppdrv */
	ret = pp_set_property(ppdrv, property);
	if (ret) {
		DRM_ERROR("failed to set property.\n");
		ppdrv->c_node = NULL;
		goto err_unlock;
	}

	/* check command */
	switch (property->cmd) {
	case PP_CMD_M2M:
		for_each_pp_ops(i) {
			/* source/destination memory list */
			head = &c_node->mem_list[i];

			m_node = list_first_entry(head,
				struct tdm_pp_mem_node, list);

			DRM_DEBUG_KMS("m_node[%p]\n", m_node);

			ret = pp_set_mem_node(ppdrv, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to set m node.\n");
				goto err_unlock;
			}
		}
		break;
	case PP_CMD_WB:
		/* destination memory list */
		head = &c_node->mem_list[TDM_OPS_DST];

		list_for_each_entry(m_node, head, list) {
			ret = pp_set_mem_node(ppdrv, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to set m node.\n");
				goto err_unlock;
			}
		}
		break;
	case PP_CMD_OUTPUT:
		/* source memory list */
		head = &c_node->mem_list[TDM_OPS_SRC];

		list_for_each_entry(m_node, head, list) {
			ret = pp_set_mem_node(ppdrv, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to set m node.\n");
				goto err_unlock;
			}
		}
		break;
	default:
		DRM_ERROR("invalid operations.\n");
		ret = -EINVAL;
		goto err_unlock;
	}
	mutex_unlock(&c_node->mem_lock);

	DRM_DEBUG_KMS("cmd[%d]\n", property->cmd);

	/* start operations */
	if (ppdrv->start) {
		ret = ppdrv->start(ppdrv->dev, property->cmd);
		if (ret) {
			DRM_ERROR("failed to start ops.\n");
			ppdrv->c_node = NULL;
			return ret;
		}
	}

	pp_wait_completion(c_node);

	return 0;

err_unlock:
	mutex_unlock(&c_node->mem_lock);
	ppdrv->c_node = NULL;
	return ret;
}

int pp_stop_property(struct drm_device *drm_dev,
		struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node)
{
	struct tdm_pp_property *property = &c_node->property;
	int i;

	DRM_DEBUG_KMS("prop_id[%d]\n", property->prop_id);

	pp_wait_completion(c_node);

	/* stop operations */
	if (ppdrv->stop)
		ppdrv->stop(ppdrv->dev, property->cmd);

	/* check command */
	switch (property->cmd) {
	case PP_CMD_M2M:
		for_each_pp_ops(i)
			pp_clean_mem_nodes(drm_dev, c_node, i);
		break;
	case PP_CMD_WB:
		pp_clean_mem_nodes(drm_dev, c_node, TDM_OPS_DST);
		break;
	case PP_CMD_OUTPUT:
		pp_clean_mem_nodes(drm_dev, c_node, TDM_OPS_SRC);
		break;
	default:
		DRM_ERROR("invalid operations.\n");
		return -EINVAL;
	}

	return 0;
}

void pp_sched_cmd(struct work_struct *work)
{
	struct tdm_pp_cmd_work *cmd_work =
		container_of(work, struct tdm_pp_cmd_work, work);
	struct tdm_ppdrv *ppdrv;
	struct tdm_pp_cmd_node *c_node;
	struct tdm_pp_property *property;
	int ret;

	ppdrv = cmd_work->ppdrv;
	if (!ppdrv) {
		DRM_ERROR("invalid ppdrv list.\n");
		return;
	}

	mutex_lock(&ppdrv->drv_lock);

	c_node = cmd_work->c_node;
	if (!c_node) {
		DRM_ERROR("invalid command node list.\n");
		mutex_unlock(&ppdrv->drv_lock);
		return;
	}

	mutex_lock(&c_node->lock);

	property = &c_node->property;

	switch (cmd_work->ctrl) {
	case PP_CTRL_PLAY:
	case PP_CTRL_RESUME:
		ret = pp_start_property(ppdrv, c_node);
		if (ret)
			DRM_ERROR("failed to start property:prop_id[%d]\n",
				c_node->property.prop_id);
		break;
	case PP_CTRL_STOP:
	case PP_CTRL_PAUSE:
		ret = pp_stop_property(ppdrv->drm_dev, ppdrv,
			c_node);
		if (ret)
			DRM_ERROR("failed to stop property.\n");
		break;
	default:
		DRM_ERROR("unknown control type\n");
		break;
	}

	DRM_DEBUG_KMS("ctrl[%d] done.\n", cmd_work->ctrl);

	mutex_unlock(&c_node->lock);
	mutex_unlock(&ppdrv->drv_lock);
}

static int pp_send_event(struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node, int *buf_id)
{
	struct drm_device *drm_dev = ppdrv->drm_dev;
	struct tdm_pp_property *property = &c_node->property;
	struct tdm_pp_mem_node *m_node;
	struct tdm_pp_queue_buf qbuf;
	struct tdm_pp_send_event *e;
	struct list_head *head;
	struct timeval now;
	unsigned long flags;
	u32 tbuf_id[TDM_OPS_MAX] = {0, };
	int ret, i;

	for_each_pp_ops(i)
		DRM_DEBUG_KMS("%s buf_id[%d]\n", i ? "dst" : "src", buf_id[i]);

	if (!drm_dev) {
		DRM_ERROR("failed to get drm_dev.\n");
		return -EINVAL;
	}

	if (!property) {
		DRM_ERROR("failed to get property.\n");
		return -EINVAL;
	}

	mutex_lock(&c_node->event_lock);
	if (list_empty(&c_node->event_list)) {
		DRM_DEBUG_KMS("event list is empty.\n");
		ret = 0;
		goto err_event_unlock;
	}

	mutex_lock(&c_node->mem_lock);
	if (!pp_check_mem_list(c_node)) {
		DRM_DEBUG_KMS("empty memory.\n");
		ret = 0;
		goto err_mem_unlock;
	}

	/* check command */
	switch (property->cmd) {
	case PP_CMD_M2M:
		for_each_pp_ops(i) {
			/* source/destination memory list */
			head = &c_node->mem_list[i];

			m_node = list_first_entry(head,
				struct tdm_pp_mem_node, list);

			tbuf_id[i] = m_node->buf_id;
			DRM_DEBUG_KMS("%s buf_id[%d]\n",
				i ? "dst" : "src", tbuf_id[i]);

			ret = pp_put_mem_node(drm_dev, c_node, m_node);
			if (ret)
				DRM_ERROR("failed to put m_node.\n");
		}
		break;
	case PP_CMD_WB:
		/* clear buf for finding */
		memset(&qbuf, 0x0, sizeof(qbuf));
		qbuf.ops_id = TDM_OPS_DST;
		qbuf.buf_id = buf_id[TDM_OPS_DST];

		/* get memory node entry */
		m_node = pp_find_mem_node(c_node, &qbuf);
		if (!m_node) {
			DRM_ERROR("empty memory node.\n");
			ret = -ENOMEM;
			goto err_mem_unlock;
		}

		tbuf_id[TDM_OPS_DST] = m_node->buf_id;

		ret = pp_put_mem_node(drm_dev, c_node, m_node);
		if (ret)
			DRM_ERROR("failed to put m_node.\n");
		break;
	case PP_CMD_OUTPUT:
		/* source memory list */
		head = &c_node->mem_list[TDM_OPS_SRC];

		m_node = list_first_entry(head,
			struct tdm_pp_mem_node, list);

		tbuf_id[TDM_OPS_SRC] = m_node->buf_id;

		ret = pp_put_mem_node(drm_dev, c_node, m_node);
		if (ret)
			DRM_ERROR("failed to put m_node.\n");
		break;
	default:
		DRM_ERROR("invalid operations.\n");
		ret = -EINVAL;
		goto err_mem_unlock;
	}
	mutex_unlock(&c_node->mem_lock);

	if (tbuf_id[TDM_OPS_DST] != buf_id[TDM_OPS_DST])
		DRM_ERROR("failed to match buf_id[%d %d]prop_id[%d]\n",
			tbuf_id[1], buf_id[1], property->prop_id);

	/*
	 * command node have event list of destination buffer
	 * If destination buffer enqueue to mem list,
	 * then we make event and link to event list tail.
	 * so, we get first event for first enqueued buffer.
	 */
	e = list_first_entry(&c_node->event_list,
		struct tdm_pp_send_event, base.link);

	do_gettimeofday(&now);
	DRM_DEBUG_KMS("tv_sec[%ld]tv_usec[%ld]\n", now.tv_sec, now.tv_usec);
	e->event.tv_sec = now.tv_sec;
	e->event.tv_usec = now.tv_usec;
	e->event.prop_id = property->prop_id;

	/* set buffer id about source destination */
	for_each_pp_ops(i)
		e->event.buf_id[i] = tbuf_id[i];

	spin_lock_irqsave(&drm_dev->event_lock, flags);
	list_move_tail(&e->base.link, &e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);
	spin_unlock_irqrestore(&drm_dev->event_lock, flags);
	mutex_unlock(&c_node->event_lock);

	if (c_node->dbg_cnt) {
		DRM_INFO("%s:cmd[%d]prop_id[%d]buf_id[%d]\n", __func__,
			property->cmd, property->prop_id, tbuf_id[TDM_OPS_DST]);
		c_node->dbg_cnt--;
	}

	return 0;

err_mem_unlock:
	mutex_unlock(&c_node->mem_lock);
err_event_unlock:
	mutex_unlock(&c_node->event_lock);
	return ret;
}

void pp_sched_event(struct work_struct *work)
{
	struct tdm_pp_event_work *event_work =
		container_of(work, struct tdm_pp_event_work, work);
	struct tdm_ppdrv *ppdrv;
	struct tdm_pp_cmd_node *c_node;
	int ret;

	if (!event_work) {
		DRM_ERROR("failed to get event_work.\n");
		return;
	}

	DRM_DEBUG_KMS("buf_id[%d]\n", event_work->buf_id[TDM_OPS_DST]);

	ppdrv = event_work->ppdrv;
	if (!ppdrv) {
		DRM_ERROR("failed to get pp driver.\n");
		return;
	}

	c_node = ppdrv->c_node;
	if (!c_node) {
		DRM_ERROR("failed to get command node.\n");
		return;
	}

	/*
	 * PP supports command thread, event thread synchronization.
	 * If PP close immediately from user land, then PP make
	 * synchronization with command thread, so make complete event.
	 * or going out operations.
	 */
	if (c_node->state != PP_STATE_START) {
		DRM_DEBUG_KMS("bypass state[%d]prop_id[%d]\n",
			c_node->state, c_node->property.prop_id);
		goto err_completion;
	}

	ret = pp_send_event(ppdrv, c_node, event_work->buf_id);
	if (ret) {
		DRM_ERROR("failed to send event.\n");
		goto err_completion;
	}

err_completion:
	if (pp_is_m2m_cmd(c_node->property.cmd))
		complete_all(&c_node->cmd_complete);
}

static int pp_subdrv_probe(struct drm_device *drm_dev, struct device *dev)
{
	struct pp_context *ctx = get_pp_context(dev);
	struct tdm_ppdrv *ppdrv;
	int ret, count = 0;

	/* get pp driver entry */
	list_for_each_entry(ppdrv, &tdm_ppdrv_list, drv_list) {
		ppdrv->drm_dev = drm_dev;

		ret = pp_create_id(&ctx->pp_idr, &ctx->pp_lock, ppdrv);
		if (ret < 0) {
			DRM_ERROR("failed to create id.\n");
			goto err;
		}
		ppdrv->prop_list.pp_id = ret;

		DRM_DEBUG_KMS("count[%d]ppdrv[%p]pp_id[%d]\n",
			count++, ppdrv, ret);

		/* store parent device for node */
		ppdrv->parent_dev = dev;

		/* store event work queue and handler */
		ppdrv->event_workq = ctx->event_workq;
		ppdrv->sched_event = pp_sched_event;
		INIT_LIST_HEAD(&ppdrv->cmd_list);
		mutex_init(&ppdrv->cmd_lock);
		mutex_init(&ppdrv->drv_lock);
		/*ToDo: need to check drm_iommu_attach_device */
	}

	return 0;

err:
	/* get pp driver entry */
	list_for_each_entry_continue_reverse(ppdrv, &tdm_ppdrv_list,
						drv_list) {
		pp_remove_id(&ctx->pp_idr, &ctx->pp_lock,
				ppdrv->prop_list.pp_id);
	}

	return ret;
}

static void pp_subdrv_remove(struct drm_device *drm_dev, struct device *dev)
{
	struct tdm_ppdrv *ppdrv, *t;
	struct pp_context *ctx = get_pp_context(dev);

	/* get pp driver entry */
	list_for_each_entry_safe(ppdrv, t, &tdm_ppdrv_list, drv_list) {
		pp_remove_id(&ctx->pp_idr, &ctx->pp_lock,
				ppdrv->prop_list.pp_id);

		ppdrv->drm_dev = NULL;
		tdm_ppdrv_unregister(ppdrv);
	}
}

static int pp_subdrv_open(struct drm_device *drm_dev, struct device *dev,
		struct drm_file *file)
{
	struct tgm_drv_file_private *file_priv = file->driver_priv;

	file_priv->pp_dev = dev;

	DRM_DEBUG_KMS("done priv[%p]\n", dev);

	return 0;
}

static void pp_subdrv_close(struct drm_device *drm_dev, struct device *dev,
		struct drm_file *file)
{
	struct tdm_ppdrv *ppdrv = NULL;
	struct pp_context *ctx = get_pp_context(dev);
	struct tdm_pp_cmd_node *c_node, *tc_node;
	int count = 0;

	list_for_each_entry(ppdrv, &tdm_ppdrv_list, drv_list) {
		mutex_lock(&ppdrv->cmd_lock);
		list_for_each_entry_safe(c_node, tc_node,
			&ppdrv->cmd_list, list) {
			DRM_DEBUG_KMS("count[%d]ppdrv[%p]\n",
				count++, ppdrv);

			if (c_node->filp == file) {
				/*
				 * userland goto unnormal state. process killed.
				 * and close the file.
				 * so, PP didn't called stop cmd ctrl.
				 * so, we are make stop operation in this state.
				 */
				if (c_node->state == PP_STATE_START) {
					pp_stop_property(drm_dev, ppdrv,
						c_node);
					c_node->state = PP_STATE_STOP;
				}

				ppdrv->dedicated = false;
				pp_clean_cmd_node(ctx, c_node);
				if (list_empty(&ppdrv->cmd_list))
					pm_runtime_put_sync(ppdrv->dev);
			}
		}
		mutex_unlock(&ppdrv->cmd_lock);
	}
}

static int pp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pp_context *ctx;
	struct tgm_subdrv *subdrv;
	int ret;

	dev_info(dev, "%s\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mutex_init(&ctx->pp_lock);
	mutex_init(&ctx->prop_lock);

	idr_init(&ctx->pp_idr);
	idr_init(&ctx->prop_idr);

	/*
	 * create single thread for pp event
	 * PP supports event thread for PP drivers.
	 * PP driver send event_work to this thread.
	 * and PP event thread send event to user process.
	 */
	ctx->event_workq = create_singlethread_workqueue("pp_event");
	if (!ctx->event_workq) {
		dev_err(dev, "failed to create event workqueue\n");
		return -EINVAL;
	}

	/*
	 * create single thread for pp command
	 * PP supports command thread for user process.
	 * user process make command node using set property ioctl.
	 * and make start_work and send this work to command thread.
	 * and then this command thread start property.
	 */
	ctx->cmd_workq = create_singlethread_workqueue("pp_cmd");
	if (!ctx->cmd_workq) {
		dev_err(dev, "failed to create cmd workqueue\n");
		ret = -EINVAL;
		goto err_event_workq;
	}

	/* set sub driver information */
	subdrv = &ctx->subdrv;
	subdrv->dev = dev;
	subdrv->probe = pp_subdrv_probe;
	subdrv->remove = pp_subdrv_remove;
	subdrv->open = pp_subdrv_open;
	subdrv->close = pp_subdrv_close;

	platform_set_drvdata(pdev, ctx);

	ret = tgm_subdrv_register(subdrv);
	if (ret < 0) {
		DRM_ERROR("failed to register drm pp device.\n");
		goto err_cmd_workq;
	}

	dev_info(dev, "drm pp registered successfully.\n");

	return 0;

err_cmd_workq:
	destroy_workqueue(ctx->cmd_workq);
err_event_workq:
	destroy_workqueue(ctx->event_workq);
	return ret;
}

static int pp_remove(struct platform_device *pdev)
{
	struct pp_context *ctx = platform_get_drvdata(pdev);

	/* unregister sub driver */
	tgm_subdrv_unregister(&ctx->subdrv);

	/* remove,destroy pp idr */
	idr_destroy(&ctx->pp_idr);
	idr_destroy(&ctx->prop_idr);

	mutex_destroy(&ctx->pp_lock);
	mutex_destroy(&ctx->prop_lock);

	/* destroy command, event work queue */
	destroy_workqueue(ctx->cmd_workq);
	destroy_workqueue(ctx->event_workq);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tdm_pp_dt_match[] = {
			{.compatible = "tdm,tdm_pp",},
			{}
};
MODULE_DEVICE_TABLE(of, tdm_pp_dt_match);
#endif

struct platform_driver pp_driver = {
	.probe		= pp_probe,
	.remove		= pp_remove,
	.driver		= {
		.name	= "tdm-pp",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = tdm_pp_dt_match,
#endif
	},
};

