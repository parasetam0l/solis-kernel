/*
 * samsung driver for providing gsim data
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

enum sec_gsim_boot_type {
	SEC_GSIM_BOOT_TYPE_NORMAL = 0,
	SEC_GSIM_BOOT_TYPE_CHARGING,
	SEC_GSIM_BOOT_TYPE_SILENT,
	SEC_GSIM_BOOT_TYPE_RECOVERY,
	SEC_GSIM_BOOT_TYPE_FOTA,
	SEC_GSIM_BOOT_TYPE_RW_UPDATE,
	SEC_GSIM_BOOT_TYPE_POWEROFF_WATCH,

	/* add new boot type to below */
	SEC_GSIM_BOOT_TYPE_RESERVED_7,
	SEC_GSIM_BOOT_TYPE_RESERVED_8,
	SEC_GSIM_BOOT_TYPE_RESERVED_9,
	SEC_GSIM_BOOT_TYPE_RESERVED_10,
	SEC_GSIM_BOOT_TYPE_RESERVED_11,
	SEC_GSIM_BOOT_TYPE_RESERVED_12,
	SEC_GSIM_BOOT_TYPE_RESERVED_13,
	SEC_GSIM_BOOT_TYPE_RESERVED_14,
	SEC_GSIM_BOOT_TYPE_RESERVED_15,

	/* total 16 items */
	SEC_GSIM_BOOT_TYPE_MAX,
};

enum sec_gsim_ext_boot_type {
	SEC_GSIM_EXT_BOOT_TYPE_KERNEL_PANIC = 0,
	SEC_GSIM_EXT_BOOT_TYPE_CP_CRASH,
	SEC_GSIM_EXT_BOOT_TYPE_SECURE_FAIL,
	SEC_GSIM_EXT_BOOT_TYPE_WATCHDOG_RESET,
	SEC_GSIM_EXT_BOOT_TYPE_MANUAL_RESET,
	SEC_GSIM_EXT_BOOT_TYPE_SMPL,
	SEC_GSIM_EXT_BOOT_TYPE_POWER_RESET,

	/* add new extended boot type to below */
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_7,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_8,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_9,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_10,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_11,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_12,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_13,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_14,
	SEC_GSIM_EXT_BOOT_TYPE_RESERVED_15,

	/* total 16 items */
	SEC_GSIM_EXT_BOOT_TYPE_MAX,
};

static const char *sec_gsim_boot_type_property[SEC_GSIM_BOOT_TYPE_MAX] = {
	[SEC_GSIM_BOOT_TYPE_NORMAL]         = "boot,normal",
	[SEC_GSIM_BOOT_TYPE_CHARGING]       = "boot,charging",
	[SEC_GSIM_BOOT_TYPE_SILENT]         = "boot,silent",
	[SEC_GSIM_BOOT_TYPE_RECOVERY]       = "boot,recovery",
	[SEC_GSIM_BOOT_TYPE_FOTA]           = "boot,fota",
	[SEC_GSIM_BOOT_TYPE_RW_UPDATE]      = "boot,rwupdate",
	[SEC_GSIM_BOOT_TYPE_POWEROFF_WATCH] = "boot,pow",
};

static const char *sec_gsim_ext_boot_type_property[SEC_GSIM_EXT_BOOT_TYPE_MAX] = {
	[SEC_GSIM_EXT_BOOT_TYPE_KERNEL_PANIC]   = "boot,kernel-panic",
	[SEC_GSIM_EXT_BOOT_TYPE_CP_CRASH]       = "boot,cp-crash",
	[SEC_GSIM_EXT_BOOT_TYPE_SECURE_FAIL]    = "boot,secure-fail",
	[SEC_GSIM_EXT_BOOT_TYPE_WATCHDOG_RESET] = "boot,watchdog",
	[SEC_GSIM_EXT_BOOT_TYPE_MANUAL_RESET]   = "boot,manual-reset",
	[SEC_GSIM_EXT_BOOT_TYPE_SMPL]           = "boot,smpl",
	[SEC_GSIM_EXT_BOOT_TYPE_POWER_RESET]    = "boot,power-reset",
};

