/*
 * driver/gpio/gpio_dvs_plugin.c
 *
 * A driver program to monitor the state of gpio during initialisation
 * and first achieved sleep.
 *
 * Copyright (C) 2016 Samsung Electronics co. ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include "secgpio_dvs.h"

#define GPIO_STATE_MAXLENGTH  10 /*max length of the string out */
#define GPIO_BUF_EXTRA_SIZE 300 /*size of header and extra informatin */
#define SIZE_PER_LINE 71	/*size of one content line */

#define GPIODVS_DEVICE_NAME "secgpio_dvs"
#define GPIODVS_NAME_LENGTH 11

extern unsigned int system_rev;
extern struct bus_type platform_bus_type;

static int buf_size;

static ssize_t gpio_result_to_readable
	(unsigned short gpio_result, char *buf, ssize_t size)
{
	char direction[GPIO_STATE_MAXLENGTH], resistor[GPIO_STATE_MAXLENGTH],
	state[GPIO_STATE_MAXLENGTH];

	switch ((gpio_result >> 4) & 0x3F) {
	case 0x00:
		strlcpy(resistor, "NP", sizeof(resistor));
		break;
	case 0x01:
		strlcpy(resistor, "PD", sizeof(resistor));
		break;
	case 0x02:
		strlcpy(resistor, "PU", sizeof(resistor));
		break;
	case 0x03:
		strlcpy(resistor, "KEEP", sizeof(resistor));
		break;
	case 0x3F:
		strlcpy(resistor, "ERR", sizeof(resistor));
		break;
	default:
		snprintf(resistor, sizeof(resistor), "??(%x)", (gpio_result >> 4) & 0x3F);
	}

	switch ((gpio_result >> 10) & 0x3F) {
	case 0x00:
		strlcpy(direction, "FUNC", sizeof(direction));
		break;
	case 0x01:
		strlcpy(direction, "IN", sizeof(direction));
		break;
	case 0x02:
		strlcpy(direction, "OUT", sizeof(direction));
		break;
	case 0x03:
		strlcpy(direction, "INT", sizeof(direction));
		break;
	case 0x04:
		strlcpy(direction, "PREV", sizeof(direction));
		break;
	case 0x05:
		strlcpy(direction, "HI-Z", sizeof(direction));
		break;
	case 0x3E:
		strlcpy(direction, "RSV", sizeof(direction));
		break;
	case 0x3F:
		strlcpy(direction, "ERR", sizeof(direction));
		break;
	default:
		snprintf(direction, sizeof(direction), "??(%x)", (gpio_result >> 10) & 0x3F);
	}

	switch (gpio_result & 0x0F) {
	case 0x00:
		strlcpy(state, "L", sizeof(state));
		break;
	case 0x01:
		strlcpy(state, "H", sizeof(state));
		break;
	case 0x0E:
		strlcpy(state, "UNKNOWN", sizeof(state));
		break;
	case 0x0F:
		strlcpy(state, "ERR", sizeof(state));
		break;
	default:
		snprintf(state, sizeof(state), "??(%x)", gpio_result & 0x0F);
	}
	size += snprintf(buf+size, buf_size - size, \
		"%10s%10s%10s"\
		, direction, resistor, state);

	return size;
}

static ssize_t show_gpio_readable
	(struct gpio_dvs_t *gdvs, char *buf, ssize_t size)
{
	unsigned int i = 0;
	bool is_first_sleep_achieved = gdvs->check_sleep;

	/* Header GPIO state table */
	size += snprintf(buf + size, buf_size - size, \
			"%s", "===================================" \
			"===================================\n");

	size += snprintf(buf + size, buf_size - size, \
			"HW Rev: 0x%02x %s", system_rev, "\n");
	size += snprintf(buf + size, buf_size - size, \
			"GPIO_CNT: %d %s", gdvs->count, "\n");
	size += snprintf(buf + size, buf_size - size, \
			"INIT CHECK: %s %s", \
			gdvs->check_init ? "Done" : "Not yet", "\n");
	size += snprintf(buf + size, buf_size - size, \
			"SLEEP CHECK: %s %s", \
			gdvs->check_sleep ? "Done" : "Not yet", "\n\n");

	size += snprintf(buf + size, buf_size - size,
			"%5s%30s%5s%30s\n",
			"", "----------INIT STATE----------" \
			, "", "----------SLEEP STATE---------");
	size += snprintf(buf + size , buf_size - size,
			"%5s%10s%10s%10s" \
			"%5s%10s%10s%10s\n" \
			, "No.", "IN/OUT", "PU/PD", "STATE" \
			, "", "IN/OUT", "PU/PD", "STATE");

	/* Content GPIO state table */
	for (i = 0; i < gdvs->count; i++) {
		size += snprintf(buf+size, buf_size - size, "%5d", i + 1);
		size = gpio_result_to_readable(gdvs->result->init[i], buf, size);
		size += snprintf(buf+size, buf_size - size, "%5s", "");
		if (is_first_sleep_achieved) {
			size = gpio_result_to_readable(gdvs->result->sleep[i], buf, size);
		} else {
			size += snprintf(buf+size, buf_size - size, \
				"%10s%10s%10s",\
				"N.A.", "N.A.", "N.A.");
		}
		size += snprintf(buf+size, buf_size - size, "\n");
	}

	return size;
}

static int gpiodvs_dev_name_match(struct device *dev, void *data)
{
	return !strncmp(dev_name(dev), data, GPIODVS_NAME_LENGTH);
}

#ifdef CONFIG_OF
const static struct of_device_id secgpio_dvs_dt_match[] = {
	{ .compatible = "samsung,exynos7570-secgpio-dvs",
		.data = (void *)&exynos7570_secgpio_dvs },
	{ },
};
MODULE_DEVICE_TABLE(of, secgpio_dvs_dt_match);
#endif

static ssize_t gpio_dvs_show_gpio_state(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{

	static char *buf;
	unsigned int ret = 0, size_for_copy = count;
	static unsigned int rest_size;
	struct device *dev;
	struct gpio_dvs_t *gdvs;
#ifdef CONFIG_OF
	const struct of_device_id *match;
	struct device_node *node;
#endif

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		dev = bus_find_device(&platform_bus_type, NULL, GPIODVS_DEVICE_NAME, gpiodvs_dev_name_match);
		if (!dev)
			return -EFAULT;
#ifdef CONFIG_OF
		node = dev->of_node;
		match = of_match_node(secgpio_dvs_dt_match, node);
		if(match && match->data) {
			gdvs = (struct gpio_dvs_t *)match->data;
#else
		if (dev && dev->platform_data) {
			gdvs = dev_get_platdata(dev);
#endif
			buf_size = SIZE_PER_LINE * gdvs->count + GPIO_BUF_EXTRA_SIZE;
			if (buf_size > PAGE_SIZE * 4)
				pr_warning("Buffer exceed PAGE_SIZE, size: %d; gpio count: %d\n ", \
				buf_size, gdvs->count);

			buf = kmalloc(buf_size, GFP_KERNEL);
			if (!buf)
				return -ENOMEM;
			ret = show_gpio_readable(gdvs, buf, ret);
		} else {
			buf = kmalloc(SIZE_PER_LINE, GFP_KERNEL);
			if (!buf)
				return -ENOMEM;
			ret = snprintf(buf, SIZE_PER_LINE, "%s", \
				"Can not retrive GPIO DVS information\n");
		}

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret - size_for_copy;
		}
	} else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		kfree(buf);

	return size_for_copy;
}


static const struct  file_operations gpio_dvs_fops = {
	.owner = THIS_MODULE,
	.read = gpio_dvs_show_gpio_state,
};

static int __init gpio_dvs_plugin_init(void)
{
	int ret = 0;

	if (!debugfs_create_file("gpio_dvs_show", 0444
	, NULL, NULL, &gpio_dvs_fops))
		pr_err("%s: debugfs_create_file, error\n", "gpio_dvs");

	return ret;
}

late_initcall(gpio_dvs_plugin_init);

MODULE_AUTHOR("Phan Dinh Phuong <phuong.phd@samsung.com>");
MODULE_DESCRIPTION("Gpio DVS plugin");
MODULE_LICENSE("GPL");
