/*
 * Samsung Tizen development team
 *
 * drivers/pinctrl/sec-pinctrl-dvs.c
 *
 * Drivers for appying gpio config prepare to samsung gpio debugging & verification.
 *
 * Copyright (C) 2016, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include "../core.h"

static int sec_pinctrl_dvs_remove(struct platform_device *pdev)
{
	return 0;
}

static int sec_pinctrl_suspend(struct device *dev)
{
	return 0;
}

static int sec_pinctrl_resume(struct device *dev)
{
	return 0;
}

static struct of_device_id sec_pinctrl_dvs_match_table[] = {
	{ .compatible = "sec_pinctrl_dvs",},
	{},
};
MODULE_DEVICE_TABLE(of, sec_pinctrl_dvs_match_table);

static const struct dev_pm_ops sec_pinctrl_pm_ops= {
	.suspend = sec_pinctrl_suspend,
	.resume = sec_pinctrl_resume,
};

static int sec_pinctrl_dvs_probe(struct platform_device *pdev)
{
	pr_info("%s\n", __func__);
	return 0;
}


static struct platform_driver sec_pinctrl_dvs = {
	.probe = sec_pinctrl_dvs_probe,
	.remove = sec_pinctrl_dvs_remove,
	.driver = {
		.name = "sec_pinctrl_dvs",
		.owner = THIS_MODULE,
		.pm = &sec_pinctrl_pm_ops,
		.of_match_table = sec_pinctrl_dvs_match_table,
	},
};

static int __init sec_pinctrl_dvs_init(void)
{
	int ret;
	ret = platform_driver_register(&sec_pinctrl_dvs);

	return ret;
}

static void __exit sec_pinctrl_dvs_exit(void)
{
	platform_driver_unregister(&sec_pinctrl_dvs);
}

postcore_initcall(sec_pinctrl_dvs_init);
module_exit(sec_pinctrl_dvs_exit);

MODULE_AUTHOR("hunsup.jung@samsung.com");
MODULE_DESCRIPTION("Setting the unmanaged pin for GPIO DVS");
MODULE_LICENSE("GPL");
