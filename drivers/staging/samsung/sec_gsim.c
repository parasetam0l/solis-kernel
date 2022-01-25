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

#include <linux/device.h>
#include <linux/slab.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include "sec_gsim.h"

struct class *system_stats;
struct device *rbrs;

#define MAX_UINT_DIGIT 11
unsigned int boot_count_data[SEC_GSIM_BOOT_TYPE_MAX];
unsigned int ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_MAX];

char *sec_gsim_get_boot_type_property(enum sec_gsim_boot_type type)
{
	if (type < 0 || type >= SEC_GSIM_BOOT_TYPE_MAX)
		return NULL;

	return (char *)sec_gsim_boot_type_property[type];
}

char *sec_gsim_get_ext_boot_type_property(enum sec_gsim_ext_boot_type type)
{
	if (type < 0 || type >= SEC_GSIM_EXT_BOOT_TYPE_MAX)
		return NULL;

	return (char *)sec_gsim_ext_boot_type_property[type];
}


#define SEC_GSIM_ATTR(_name, _value) \
static ssize_t show_##_name(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	return snprintf(buf, MAX_UINT_DIGIT, "%u\n", _value); \
} \
static DEVICE_ATTR(_name, 0550, show_##_name, NULL)

SEC_GSIM_ATTR(NORMAL,         boot_count_data[SEC_GSIM_BOOT_TYPE_NORMAL]);
SEC_GSIM_ATTR(CHARGING,       boot_count_data[SEC_GSIM_BOOT_TYPE_CHARGING]);
SEC_GSIM_ATTR(SILENT,         boot_count_data[SEC_GSIM_BOOT_TYPE_SILENT]);
SEC_GSIM_ATTR(RECOVERY,       boot_count_data[SEC_GSIM_BOOT_TYPE_RECOVERY]);
SEC_GSIM_ATTR(FOTA,           boot_count_data[SEC_GSIM_BOOT_TYPE_FOTA]);
SEC_GSIM_ATTR(RW_UPDATE,      boot_count_data[SEC_GSIM_BOOT_TYPE_RW_UPDATE]);
SEC_GSIM_ATTR(POWEROFF_WATCH, boot_count_data[SEC_GSIM_BOOT_TYPE_POWEROFF_WATCH]);

SEC_GSIM_ATTR(KERNEL_PANIC,   ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_KERNEL_PANIC]);
SEC_GSIM_ATTR(CP_CRASH,       ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_CP_CRASH]);
SEC_GSIM_ATTR(SECURE_FAIL,    ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_SECURE_FAIL]);
SEC_GSIM_ATTR(WATCHDOG,       ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_WATCHDOG_RESET]);
SEC_GSIM_ATTR(MANUAL_RESET,   ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_MANUAL_RESET]);
SEC_GSIM_ATTR(SMPL,           ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_SMPL]);
SEC_GSIM_ATTR(POWER_RESET,    ext_boot_count_data[SEC_GSIM_EXT_BOOT_TYPE_POWER_RESET]);

static int __init sec_gsim_init(void)
{
	int ret = 0;
	int l = 0;
	enum sec_gsim_boot_type type;
	enum sec_gsim_ext_boot_type ext_type;
	struct device_node *np;
	char *property;
	const __be32 *read_data;


	system_stats = class_create(THIS_MODULE, "system_stats");
	if (IS_ERR(system_stats)) {
		pr_err("Failed to create class(system_stats)!\n");
		return PTR_ERR(system_stats);
	}
	rbrs = device_create(system_stats, NULL, 0, "%s", "rbrs");
	if (IS_ERR(rbrs)) {
		class_destroy(system_stats);
		pr_err("Failed to create rbrs dev!\n");
		return PTR_ERR(rbrs);
	}

	np = of_find_node_by_path("/sec-gsim");
	if (!np) {
		pr_err("could not find /sec-gsim  node!\n");
		return 0;
	}
	for (type = 0; type < SEC_GSIM_BOOT_TYPE_MAX; type++) {
		property = sec_gsim_get_boot_type_property(type);
		if (!property)
			continue;

		read_data = of_get_property(np, property, &l);

		if (read_data != NULL)
			boot_count_data[type] = be32_to_cpu(*read_data);

		pr_info("%s %s boot_count_data[%d]: %d\n", __func__,
			property, type, boot_count_data[type]);

		if (boot_count_data[type] != 0) {
			switch (type) {
			case SEC_GSIM_BOOT_TYPE_NORMAL:
				ret = device_create_file(rbrs, &dev_attr_NORMAL);
				break;
			case SEC_GSIM_BOOT_TYPE_CHARGING:
				ret = device_create_file(rbrs, &dev_attr_CHARGING);
				break;
			case SEC_GSIM_BOOT_TYPE_SILENT:
				ret = device_create_file(rbrs, &dev_attr_SILENT);
				break;
			case SEC_GSIM_BOOT_TYPE_RECOVERY:
				ret = device_create_file(rbrs, &dev_attr_RECOVERY);
				break;
			case SEC_GSIM_BOOT_TYPE_FOTA:
				ret = device_create_file(rbrs, &dev_attr_FOTA);
				break;
			case SEC_GSIM_BOOT_TYPE_RW_UPDATE:
				ret = device_create_file(rbrs, &dev_attr_RW_UPDATE);
				break;
			case SEC_GSIM_BOOT_TYPE_POWEROFF_WATCH:
				ret = device_create_file(rbrs, &dev_attr_POWEROFF_WATCH);
				break;
			default:
				pr_info("%s Invalid type", __func__);
				break;
			}
		}
	}

	for (ext_type = 0; ext_type < SEC_GSIM_EXT_BOOT_TYPE_MAX; ext_type++) {
		property = sec_gsim_get_ext_boot_type_property(ext_type);
		if (!property)
			continue;

		read_data = of_get_property(np, property, &l);
		if (read_data != NULL)
			ext_boot_count_data[ext_type] = be32_to_cpu(*read_data);

		pr_info("%s %s ext_boot_count_data[%d]: %d\n", __func__,
			property, ext_type, ext_boot_count_data[ext_type]);

		if (ext_boot_count_data[ext_type] != 0) {
			switch (ext_type) {
			case SEC_GSIM_EXT_BOOT_TYPE_KERNEL_PANIC:
				ret = device_create_file(rbrs, &dev_attr_KERNEL_PANIC);
				break;
			case SEC_GSIM_EXT_BOOT_TYPE_CP_CRASH:
				ret = device_create_file(rbrs, &dev_attr_CP_CRASH);
				break;
			case SEC_GSIM_EXT_BOOT_TYPE_SECURE_FAIL:
				ret = device_create_file(rbrs, &dev_attr_SECURE_FAIL);
				break;
			case SEC_GSIM_EXT_BOOT_TYPE_WATCHDOG_RESET:
				ret = device_create_file(rbrs, &dev_attr_WATCHDOG);
				break;
			case SEC_GSIM_EXT_BOOT_TYPE_MANUAL_RESET:
				ret = device_create_file(rbrs, &dev_attr_MANUAL_RESET);
				break;
			case SEC_GSIM_EXT_BOOT_TYPE_SMPL:
				ret = device_create_file(rbrs, &dev_attr_SMPL);
				break;
			case SEC_GSIM_EXT_BOOT_TYPE_POWER_RESET:
				ret = device_create_file(rbrs, &dev_attr_POWER_RESET);
				break;
			default:
				pr_info("%s Invalid type", __func__);
				break;
			}
		}
	}
	of_node_put(np);

	pr_info("SEC GSIM driver]] init , ret val : %d\n", ret);
	return ret;

}

subsys_initcall(sec_gsim_init);

