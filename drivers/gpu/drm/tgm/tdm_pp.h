/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _TDM_PP_H_
#define _TDM_PP_H_

#define for_each_pp_ops(pos)	\
	for (pos = 0; pos < TDM_OPS_MAX; pos++)
#define for_each_pp_planar(pos)	\
	for (pos = 0; pos < TDM_PLANAR_MAX; pos++)

#define PP_GET_LCD_WIDTH	_IOR('F', 302, int)
#define PP_GET_LCD_HEIGHT	_IOR('F', 303, int)
#define PP_SET_WRITEBACK	_IOW('F', 304, u32)

/* definition of state */
enum tdm_pp_state {
	PP_STATE_IDLE,
	PP_STATE_START,
	PP_STATE_STOP,
};

/*
 * A structure of command work information.
 * @work: work structure.
 * @ppdrv: current work ppdrv.
 * @c_node: command node information.
 * @ctrl: command control.
 */
struct tdm_pp_cmd_work {
	struct work_struct	work;
	struct tdm_ppdrv	*ppdrv;
	struct tdm_pp_cmd_node *c_node;
	enum tdm_pp_ctrl	ctrl;
};

/*
 * A structure of command node.
 *
 * @list: list head to command queue information.
 * @event_list: list head of event.
 * @mem_list: list head to source,destination memory queue information.
 * @lock: lock for synchronization of access to ioctl.
 * @mem_lock: lock for synchronization of access to memory nodes.
 * @event_lock: lock for synchronization of access to scheduled event.
 * @cmd_complete: completion of command.
 * @property: property information.
 * @start_work: start command work structure.
 * @stop_work: stop command work structure.
 * @event_work: event work structure.
 * @state: state of command node.
 * @filp: associated file pointer.
 */
struct tdm_pp_cmd_node {
	struct list_head	list;
	struct list_head	event_list;
	struct list_head	mem_list[TDM_OPS_MAX];
	struct mutex	lock;
	struct mutex	mem_lock;
	struct mutex	event_lock;
	struct completion	cmd_complete;
	struct tdm_pp_property	property;
	struct tdm_pp_cmd_work *start_work;
	struct tdm_pp_cmd_work *stop_work;
	struct tdm_pp_event_work *event_work;
	enum tdm_pp_state	state;
	struct drm_file	*filp;
	u32	dbg_cnt;
};

/*
 * A structure of buffer information.
 *
 * @handles: Y, Cb, Cr each gem object handle.
 * @base: Y, Cb, Cr each planar address.
 * @size: Y, Cb, Cr each planar size.
 */
struct tdm_pp_buf_info {
	unsigned long	handles[TDM_PLANAR_MAX];
	dma_addr_t	base[TDM_PLANAR_MAX];
	unsigned long	size[TDM_PLANAR_MAX];
};

/*
 * A structure of wb setting information.
 *
 * @enable: enable flag for wb.
 * @refresh: HZ of the refresh rate.
 */
struct tdm_pp_set_wb {
	__u32	enable;
	__u32	refresh;
};

/*
 * A structure of event work information.
 *
 * @work: work structure.
 * @ppdrv: current work ppdrv.
 * @buf_id: id of src, dst buffer.
 */
struct tdm_pp_event_work {
	struct work_struct	work;
	struct tdm_ppdrv *ppdrv;
	u32	buf_id[TDM_OPS_MAX];
};

/*
 * A structure of source,destination operations.
 *
 * @set_fmt: set format of image.
 * @set_transf: set transform(rotations, flip).
 * @set_size: set size of region.
 * @set_addr: set address for dma.
 */
struct tdm_pp_ops {
	int (*set_fmt)(struct device *dev, u32 fmt);
	int (*set_transf)(struct device *dev,
		enum tdm_degree degree,
		enum tdm_flip flip, bool *swap);
	int (*set_size)(struct device *dev, int swap,
		struct tdm_pos *pos, struct tdm_sz *sz);
	int (*set_addr)(struct device *dev,
		 struct tdm_pp_buf_info *buf_info, u32 buf_id,
		enum tdm_pp_buf_type buf_type);
};

/*
 * A structure of pp driver.
 *
 * @drv_list: list head for registed sub driver information.
 * @parent_dev: parent device information.
 * @dev: platform device.
 * @drm_dev: drm device.
 * @dedicated: dedicated pp device.
 * @ops: source, destination operations.
 * @event_workq: event work queue.
 * @c_node: current command information.
 * @cmd_list: list head for command information.
 * @cmd_lock: lock for synchronization of access to cmd_list.
 * @drv_lock: lock for synchronization of access to start operation.
 * @prop_list: property information of current pp driver.
 * @check_property: check property about format, size, buffer.
 * @reset: reset pp block.
 * @start: pp each device start.
 * @stop: pp each device stop.
 * @sched_event: work schedule handler.
 */
struct tdm_ppdrv {
	struct list_head	drv_list;
	struct device	*parent_dev;
	struct device	*dev;
	struct drm_device	*drm_dev;
	bool	dedicated;
	struct tdm_pp_ops	*ops[TDM_OPS_MAX];
	struct workqueue_struct	*event_workq;
	struct tdm_pp_cmd_node *c_node;
	struct list_head	cmd_list;
	struct mutex	cmd_lock;
	struct mutex	drv_lock;
	struct tdm_pp_prop_list prop_list;

	int (*check_property)(struct device *dev,
		struct tdm_pp_property *property);
	int (*reset)(struct device *dev);
	int (*start)(struct device *dev, enum tdm_pp_cmd cmd);
	void (*stop)(struct device *dev, enum tdm_pp_cmd cmd);
	void (*sched_event)(struct work_struct *work);
};

#ifdef CONFIG_DRM_TDM_PP
extern int tdm_ppdrv_register(struct tdm_ppdrv *ppdrv);
extern int tdm_ppdrv_unregister(struct tdm_ppdrv *ppdrv);
extern int tdm_pp_get_property(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tdm_pp_set_property(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tdm_pp_queue_buf(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tdm_pp_cmd_ctrl(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tdm_pp_get_permission(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int tdm_ppnb_register(struct notifier_block *nb);
extern int tdm_ppnb_unregister(struct notifier_block *nb);
extern int tdm_ppnb_send_event(unsigned long val, void *v);
extern void pp_sched_cmd(struct work_struct *work);
extern void pp_sched_event(struct work_struct *work);
extern int pp_start_property(struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node);
extern int pp_stop_property(struct drm_device *drm_dev,
		struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node);
#else
static inline int tdm_ppdrv_register(struct tdm_ppdrv *ppdrv)
{
	return -ENODEV;
}

static inline int tdm_ppdrv_unregister(struct tdm_ppdrv *ppdrv)
{
	return -ENODEV;
}

static inline int tdm_pp_get_property(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file_priv)
{
	return -ENOTTY;
}

static inline int tdm_pp_set_property(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file_priv)
{
	return -ENOTTY;
}

static inline int tdm_pp_queue_buf(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file)
{
	return -ENOTTY;
}

static inline int tdm_pp_cmd_ctrl(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file)
{
	return -ENOTTY;
}

static inline int tdm_pp_get_permission(struct drm_device *drm_dev,
					void *data,
					 struct drm_file *file);
{
	return -ENOTTY;
}

static inline int tdm_ppnb_register(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int tdm_ppnb_unregister(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int tdm_ppnb_send_event(unsigned long val, void *v)
{
	return -ENOTTY;
}

static inline int pp_start_property(struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node)
{
	return -ENOTTY;
}
static inline int pp_stop_property(struct drm_device *drm_dev,
		struct tdm_ppdrv *ppdrv,
		struct tdm_pp_cmd_node *c_node)
{
	return -ENOTTY;
}
#endif

#endif /* _TDM_PP_H_ */


