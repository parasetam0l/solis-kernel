/*
 * Copyright (C) 2015-2016 Intel Corp. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/rpmb.h>

#include "rpmb-cdev.h"

static dev_t rpmb_devt;
#define RPMB_MAX_DEVS  MINORMASK

#define RPMB_DEV_OPEN    0  /** single open bit (position) */

/**
 * rpmb_open - the open function
 *
 * @inode: pointer to inode structure
 * @file: pointer to file structure
 *
 * Return: 0 on success, <0 on error
 */
static int rpmb_open(struct inode *inode, struct file *file)
{
	struct rpmb_dev *rdev;

	rdev = container_of(inode->i_cdev, struct rpmb_dev, cdev);
	if (!rdev)
		return -ENODEV;

	/* the rpmb is single open! */
	if (test_and_set_bit(RPMB_DEV_OPEN, &rdev->status))
		return -EBUSY;

	mutex_lock(&rdev->lock);

	file->private_data = rdev;

	mutex_unlock(&rdev->lock);

	return nonseekable_open(inode, file);
}

static int rpmb_release(struct inode *inode, struct file *file)
{
	struct rpmb_dev *rdev = file->private_data;

	clear_bit(RPMB_DEV_OPEN, &rdev->status);

	return 0;
}

/*
 * FIMXE: will be exported by the kernel in future version
 * helper to convert user pointers passed inside __aligned_u64 fields
 */
static void __user *u64_to_ptr(__u64 val)
{
	return (void __user *) (unsigned long) val;
}

static int rpmb_ioctl_cmd(struct rpmb_dev *rdev,
			  struct rpmb_ioc_cmd __user *ptr)
{
	struct rpmb_ioc_cmd cmd;
	struct rpmb_data rpmbd;
	struct rpmb_frame *in_frames = NULL;
	struct rpmb_frame *out_frames = NULL;
	size_t in_sz, out_sz;
	int ret;

	if (copy_from_user(&cmd, ptr, sizeof(cmd)))
		return -EFAULT;

	if (cmd.in_frames_count == 0 || cmd.out_frames_count == 0)
		return -EINVAL;

	in_sz = sizeof(struct rpmb_frame) * cmd.in_frames_count;
	in_frames = kmalloc(in_sz, GFP_KERNEL);
	if (!in_frames)
		return -ENOMEM;

	if (copy_from_user(in_frames, u64_to_ptr(cmd.in_frames_ptr), in_sz)) {
		ret = -EINVAL;
		goto out;
	}

	out_sz = sizeof(struct rpmb_frame) * cmd.out_frames_count;
	out_frames = kmalloc(out_sz, GFP_KERNEL);
	if (!out_frames) {
		ret = -ENOMEM;
		goto out;
	}

	rpmbd.req_type = cmd.req;
	rpmbd.in_frames = in_frames;
	rpmbd.out_frames = out_frames;
	rpmbd.in_frames_cnt = cmd.in_frames_count;
	rpmbd.out_frames_cnt = cmd.out_frames_count;

	ret  = rpmb_send_req(rdev, &rpmbd);
	if (ret) {
		dev_err(&rdev->dev, "Failed to process request = %d.\n", ret);
		goto out;
	}

	if (copy_to_user(u64_to_ptr(cmd.out_frames_ptr), out_frames, out_sz)) {
		ret = -EFAULT;
		goto out;
	}

	if (copy_to_user(ptr, &cmd, sizeof(cmd))) {
		ret = -EFAULT;
		goto out;
	}
out:
	kfree(in_frames);
	kfree(out_frames);
	return ret;
}

static long rpmb_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rpmb_dev *rdev = file->private_data;
	struct rpmb_ioc_cmd __user *req = (void __user *)arg;

	if (cmd != RPMB_IOC_REQ) {
		dev_err(&rdev->dev, "unsupported ioctl 0x%x.\n", cmd);
		return -ENOIOCTLCMD;
	}

	return rpmb_ioctl_cmd(rdev, req);
}

#ifdef CONFIG_COMPAT
static long compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rpmb_dev *rdev = file->private_data;
	struct rpmb_ioc_cmd __user *req = (void __user *)compat_ptr(arg);

	if (cmd != RPMB_IOC_REQ) {
		dev_err(&rdev->dev, "unsupported ioctl 0x%x.\n", cmd);
		return -ENOIOCTLCMD;
	}

	return rpmb_ioctl_cmd(rdev, req);
}
#endif /* CONFIG_COMPAT */

static const struct file_operations rpmb_fops = {
	.open           = rpmb_open,
	.release        = rpmb_release,
	.unlocked_ioctl = rpmb_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = compat_ioctl,
#endif
	.owner          = THIS_MODULE,
	.llseek         = noop_llseek,
};

void rpmb_cdev_prepare(struct rpmb_dev *rdev)
{
	rdev->dev.devt = MKDEV(MAJOR(rpmb_devt), rdev->id);
	rdev->cdev.owner = THIS_MODULE;
	cdev_init(&rdev->cdev, &rpmb_fops);
}

void rpmb_cdev_add(struct rpmb_dev *rdev)
{
	cdev_add(&rdev->cdev, rdev->dev.devt, 1);
}

void rpmb_cdev_del(struct rpmb_dev *rdev)
{
	if (rdev->dev.devt)
		cdev_del(&rdev->cdev);
}

int __init rpmb_cdev_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&rpmb_devt, 0, RPMB_MAX_DEVS, "rpmb");
	if (ret < 0)
		pr_err("unable to allocate char dev region\n");

	return ret;
}

void __exit rpmb_cdev_exit(void)
{
	if (rpmb_devt)
		unregister_chrdev_region(rpmb_devt, RPMB_MAX_DEVS);
}
