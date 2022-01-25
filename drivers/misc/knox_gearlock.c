/*
 * knox_gearlock.c - Driver for providing smc and
 * ioctl interface for KNOX protection for Samsung
 * pay on Samsung Gear.
 *
 * Copyright (C) 2015 Samsung Electronics
 * Rohan Bhutkar <r1.bhutkar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/crash_dump.h>
#include <linux/backing-dev.h>
#include <linux/bootmem.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/knox_gearlock.h>
#include <linux/proc_fs.h>
#include <linux/module.h>

#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#ifndef CONFIG_ARM64
#define MC_ARCH_EXTENSION_SEC
#endif
#endif

/*
 * Input Parameters:
 * flag_id = boolean flag id (refer to gearpay_bool_t enum)
 * set = 0 to reset, set = 1 to set counter corresponding to the flag_id
 *
 * Return: 0 on success, 1 or 2 on failure.
 */
u32 exynos_smc_gearpay(gearpay_bool_t flag_id, u32 set)
{
	register u32 reg0 __asm__("r0") = (u32)GEARPAY_SMC_ID;
	register u32 reg1 __asm__("r1") = (u32)flag_id;
	register u32 reg2 __asm__("r2") = set;
	register u32 reg3 __asm__("r3") = 0;

	__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
	/* This pseudo op is supported and required from
	* binutils 2.21 on */
	".arch_extension sec\n"
#endif
	"smc 0\n"
	: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
	);
	return reg1;
}

static int knox_gearlock_open(struct inode *inode, struct file *filp)
{
	return 0 ;
}

int g_lock_layout_active;
long knox_gearlock_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/*
	 * Switch according to the ioctl cmd received
	 */
	switch (cmd) {
	case 0:
		printk(KERN_ERR "\n%s -> Gear ioctl: PIN Entry Layout"
				" drawn, Starting Touch SMC\n",
				__func__);
		g_lock_layout_active = 1;
		break;

	case 1:
		printk(KERN_ERR "\n%s -> Gear ioctl: Stopping Touch"
				" SMC\n", __func__);
		g_lock_layout_active = 0;
		break;

	default:
		printk(KERN_ERR "\n%s -> Gear ioctl: Invalid Privacy"
				" Lock mode command\n", __func__);
		return -1;
			break;
	}

	return 0;
}
#ifdef CONFIG_COMPAT
static long knox_gearlock_compat_ioctl(struct file *filp, unsigned int cmd,
        unsigned long arg)
{
	return knox_gearlock_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif


const struct file_operations knox_gearlock_fops = {
	.owner = THIS_MODULE,
	.open	= knox_gearlock_open,
	.unlocked_ioctl  = knox_gearlock_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = knox_gearlock_compat_ioctl,
#endif
};

static struct miscdevice knox_gearlock_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "knox_gearlock",
	.fops = &knox_gearlock_fops,
	.mode = 0666,
};

static int __init knox_gearlock_init(void)
{
	int ret;
	ret = misc_register(&knox_gearlock_misc_device);
	if (ret) {
		printk(KERN_ERR"knox_gearlock_init: "
				"failed to create misc device\n");
		return ret;
	}
	return 0;
}
module_init(knox_gearlock_init);

static void __exit knox_gearlock_exit(void)
{
	misc_deregister(&knox_gearlock_misc_device);
}
module_exit(knox_gearlock_exit);

MODULE_DESCRIPTION("KNOX Samsung Pay protection driver");
MODULE_AUTHOR("Rohan Bhutkar <r1.bhutkar@samsung.com>");
MODULE_LICENSE("GPL v2");
