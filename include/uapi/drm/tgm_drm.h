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

#ifndef _UAPI_TGM_DRM_H_
#define _UAPI_TGM_DRM_H_

#include "drm.h"

enum e_tbm_gem_mem_type {
	/* Physically Continuous memory and used as default. */
	TBM_BO_CONTIG	= 0 << 0,
	/* Physically Non-Continuous memory. */
	TBM_BO_NONCONTIG	= 1 << 0,
	/* non-cachable mapping and used as default. */
	TBM_BO_NONCACHABLE	= 0 << 1,
	/* cachable mapping. */
	TBM_BO_CACHABLE	= 1 << 1,
	/* write-combine mapping. */
	TBM_BO_WC		= 1 << 2,
	TBM_BO_MASK		= TBM_BO_NONCONTIG | TBM_BO_CACHABLE |
					TBM_BO_WC
};

enum tgm_planer {
	TGM_PLANAR_Y,
	TGM_PLANAR_CB,
	TGM_PLANAR_CR,
	TGM_PLANAR_MAX,
};

enum tdm_crtc_id {
	TDM_CRTC_PRIMARY,
	TDM_CRTC_VIRTUAL,
	TDM_CRTC_MAX,
};

enum tdm_dpms_type {
	DPMS_SYNC_WORK = 0x0,
	DPMS_EVENT_DRIVEN = 0x1,
};

struct tdm_control_dpms {
	enum tdm_crtc_id	crtc_id;
	__u32	dpms;
	__u32	user_data;
	enum tdm_dpms_type	type;
};

struct tdm_control_dpms_event {
	struct drm_event	base;
	enum tdm_crtc_id	crtc_id;
	__u32	dpms;
	__u32	user_data;
};

struct tbm_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};

struct tbm_gem_mmap {
	unsigned int handle;
	unsigned int pad;
	uint64_t size;
	uint64_t mapped;
};

struct tbm_gem_info {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

struct tbm_gem_cpu_access {
	unsigned int handle;
	unsigned int reserved;
};

enum tdm_ops_id {
	TDM_OPS_SRC,
	TDM_OPS_DST,
	TDM_OPS_MAX,
};

struct tdm_sz {
	__u32	hsize;
	__u32	vsize;
};

struct tdm_pos {
	__u32	x;
	__u32	y;
	__u32	w;
	__u32	h;
};

enum tdm_flip {
	TDM_FLIP_NONE = (0 << 0),
	TDM_FLIP_VERTICAL = (1 << 0),
	TDM_FLIP_HORIZONTAL = (1 << 1),
	TDM_FLIP_BOTH = TDM_FLIP_VERTICAL |
			TDM_FLIP_HORIZONTAL,
};

enum tdm_degree {
	TDM_DEGREE_0,
	TDM_DEGREE_90,
	TDM_DEGREE_180,
	TDM_DEGREE_270,
};

enum tdm_planer {
	TDM_PLANAR_Y,
	TDM_PLANAR_CB,
	TDM_PLANAR_CR,
	TDM_PLANAR_MAX,
};

/**
 * A structure for pp supported property list.
 *
 * @version: version of this structure.
 * @pp_id: id of pp driver.
 * @count: count of pp driver.
 * @writeback: flag of writeback supporting.
 * @flip: flag of flip supporting.
 * @degree: flag of degree information.
 * @csc: flag of csc supporting.
 * @crop: flag of crop supporting.
 * @scale: flag of scale supporting.
 * @refresh_min: min hz of refresh.
 * @refresh_max: max hz of refresh.
 * @crop_min: crop min resolution.
 * @crop_max: crop max resolution.
 * @scale_min: scale min resolution.
 * @scale_max: scale max resolution.
 */
struct tdm_pp_prop_list {
	__u32	version;
	__u32	pp_id;
	__u32	count;
	__u32	writeback;
	__u32	flip;
	__u32	degree;
	__u32	csc;
	__u32	crop;
	__u32	scale;
	__u32	refresh_min;
	__u32	refresh_max;
	__u32	reserved;
	struct tdm_sz	crop_min;
	struct tdm_sz	crop_max;
	struct tdm_sz	scale_min;
	struct tdm_sz	scale_max;
};

/**
 * A structure for pp config.
 *
 * @ops_id: property of operation directions.
 * @flip: property of mirror, flip.
 * @degree: property of rotation degree.
 * @fmt: property of image format.
 * @sz: property of image size.
 * @pos: property of image position(src-cropped,dst-scaler).
 */
struct tdm_pp_config {
	enum tdm_ops_id ops_id;
	enum tdm_flip	flip;
	enum tdm_degree	degree;
	__u32	fmt;
	struct tdm_sz	sz;
	struct tdm_pos	pos;
};

enum tdm_pp_cmd {
	PP_CMD_NONE,
	PP_CMD_M2M,
	PP_CMD_WB,
	PP_CMD_OUTPUT,
	PP_CMD_MAX,
};

/* define of pp operation type */
enum tdm_pp_type {
	PP_SYNC_WORK = 0x0,
	PP_EVENT_DRIVEN = 0x1,
	PP_TYPE_MAX = 0x2,
};

/**
 * A structure for pp property.
 *
 * @config: source, destination config.
 * @cmd: definition of command.
 * @pp_id: id of pp driver.
 * @prop_id: id of property.
 * @refresh_rate: refresh rate.
 * @type: definition of operation type.
 */
struct tdm_pp_property {
	struct tdm_pp_config config[TDM_OPS_MAX];
	enum tdm_pp_cmd	cmd;
	__u32	pp_id;
	__u32	prop_id;
	__u32	refresh_rate;
	enum tdm_pp_type	type;
};

enum tdm_pp_buf_type {
	PP_BUF_ENQUEUE,
	PP_BUF_DEQUEUE,
};

/**
 * A structure for pp buffer operations.
 *
 * @ops_id: operation directions.
 * @buf_type: definition of buffer.
 * @prop_id: id of property.
 * @buf_id: id of buffer.
 * @handle: Y, Cb, Cr each planar handle.
 * @user_data: user data.
 */
struct tdm_pp_queue_buf {
	enum tdm_ops_id	ops_id;
	enum tdm_pp_buf_type	buf_type;
	__u32	prop_id;
	__u32	buf_id;
	__u32	handle[TDM_PLANAR_MAX];
	__u32	reserved;
	__u64	user_data;
};

enum tdm_pp_ctrl {
	PP_CTRL_PLAY,
	PP_CTRL_STOP,
	PP_CTRL_PAUSE,
	PP_CTRL_RESUME,
	PP_CTRL_MAX,
};

/**
 * A structure for pp start/stop operations.
 *
 * @prop_id: id of property.
 * @ctrl: definition of control.
 */
struct tdm_pp_cmd_ctrl {
	__u32	prop_id;
	enum tdm_pp_ctrl	ctrl;
};

struct tdm_pp_event {
	struct drm_event	base;
	__u64			user_data;
	__u32			tv_sec;
	__u32			tv_usec;
	__u32			prop_id;
	__u32			reserved;
	__u32			buf_id[TDM_OPS_MAX];
};

#define DRM_TBM_GEM_CREATE		0x00
#define DRM_TBM_GEM_MMAP		0x02
#define DRM_TBM_GEM_GET		0x04
#define DRM_TBM_GEM_CPU_PREP		0x05
#define DRM_TBM_GEM_CPU_FINI		0x06
#define DRM_TDM_DPMS_CONTROL		0x50

/* PP - Post Processing */
#define DRM_TDM_PP_GET_PROPERTY	0x30
#define DRM_TDM_PP_SET_PROPERTY	0x31
#define DRM_TDM_PP_QUEUE_BUF	0x32
#define DRM_TDM_PP_CMD_CTRL	0x33
#define DRM_TDM_PP_GET_PERMISSION	0x34

#define DRM_IOCTL_TBM_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_CREATE, struct tbm_gem_create)

#define DRM_IOCTL_TBM_GEM_MMAP		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_MMAP, struct tbm_gem_mmap)

#define DRM_IOCTL_TBM_GEM_GET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_GET,	struct tbm_gem_info)

#define DRM_IOCTL_TBM_GEM_CPU_PREP		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_CPU_PREP, struct tbm_gem_cpu_access)

#define DRM_IOCTL_TBM_GEM_CPU_FINI		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TBM_GEM_CPU_FINI, struct tbm_gem_cpu_access)

#define DRM_IOCTL_TDM_DPMS_CONTROL	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_DPMS_CONTROL, struct tdm_control_dpms)

#define DRM_IOCTL_TDM_PP_GET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_PP_GET_PROPERTY, struct tdm_pp_prop_list)

#define DRM_IOCTL_TDM_PP_SET_PROPERTY	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_PP_SET_PROPERTY, struct tdm_pp_property)

#define DRM_IOCTL_TDM_PP_QUEUE_BUF	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_PP_QUEUE_BUF, struct tdm_pp_queue_buf)

#define DRM_IOCTL_TDM_PP_CMD_CTRL		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_PP_CMD_CTRL, struct tdm_pp_cmd_ctrl)

#define DRM_IOCTL_TDM_PP_GET_PERMISSION		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TDM_PP_GET_PERMISSION, bool)

#define TDM_PP_EVENT		0x80000001
#define TDM_DPMS_EVENT		0x80000002

#endif /* _UAPI_TGM_DRM_H_ */
