/*
 * Copyright (C) 2011 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/of.h>
#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif
#include <linux/switch.h>
#include <linux/battery/sec_charging_common.h>

struct jig_notifier_platform_data {
	struct	notifier_block jig_nb;
};

static int of_jig_notifier_dt(struct device *dev,
		struct jig_notifier_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (!np)
		return -EINVAL;

	return 0;
}

int is_jig_attached;
int muic_get_jig_state(void)
{
	return is_jig_attached;
}
EXPORT_SYMBOL(muic_get_jig_state);

#if defined(CONFIG_MUIC_NOTIFIER)
#ifdef CONFIG_SWITCH
static struct switch_dev switch_jig = {
	.name = "jig_cable",
};
#endif /* CONFIG_SWITCH */

static int jig_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	pr_info("%s action=%lu, attached_dev=%d\n",
		__func__, action, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			is_jig_attached = 0;
#ifdef CONFIG_SWITCH
			switch_set_state(&switch_jig, 0);
#endif
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			is_jig_attached = 1;
#ifdef CONFIG_SWITCH
			switch_set_state(&switch_jig, 1);
#endif
		} else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	return 0;
}
#endif /* CONFIG_MUIC_NOTIFIER */

static int jig_notifier_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct jig_notifier_platform_data *pdata = NULL;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct jig_notifier_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = of_jig_notifier_dt(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to get device of_node\n");
			return ret;
		}

		pdev->dev.platform_data = pdata;
	} else {
		pdata = pdev->dev.platform_data;
		dev_info(&pdev->dev, "Used traditional platform_data\n");
	}

#ifdef CONFIG_MUIC_NOTIFIER
#ifdef CONFIG_SWITCH
	ret = switch_dev_register(&switch_jig);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register jig switch(%d)\n", ret);
		return ret;
	}
#endif
	muic_notifier_register(&pdata->jig_nb, jig_handle_notification,
			       MUIC_NOTIFY_DEV_USB);
#endif

	dev_info(&pdev->dev, "jig notifier probe\n");
	return 0;
}

static int jig_notifier_remove(struct platform_device *pdev)
{
#if defined(CONFIG_MUIC_NOTIFIER)
	struct jig_notifier_platform_data *pdata = dev_get_platdata(&pdev->dev);

	muic_notifier_unregister(&pdata->jig_nb);
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id jig_notifier_dt_ids[] = {
	{ .compatible = "samsung,jig-notifier",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, jig_notifier_dt_ids);
#endif

static struct platform_driver jig_notifier_driver = {
	.probe		= jig_notifier_probe,
	.remove		= jig_notifier_remove,
	.driver		= {
		.name	= "jig_notifier",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(jig_notifier_dt_ids),
#endif
	},
};

static int __init jig_notifier_init(void)
{
	return platform_driver_register(&jig_notifier_driver);
}

static void __init jig_notifier_exit(void)
{
	platform_driver_unregister(&jig_notifier_driver);
}

late_initcall(jig_notifier_init);
module_exit(jig_notifier_exit);

MODULE_DESCRIPTION("JIG notifier");
MODULE_LICENSE("GPL");
