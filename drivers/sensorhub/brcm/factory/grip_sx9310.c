/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "../ssp.h"

#define	VENDOR		"SEMTECH"
#define	CHIP_ID		"SX9310"

static ssize_t grip_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t grip_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(vendor, S_IRUGO, grip_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, grip_name_show, NULL);

static struct device_attribute *grip_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	NULL,
};

void initialize_sx9310_grip_factorytest(struct ssp_data *data)
{
	sensors_register(data->uv_device, data, grip_attrs, "grip_sensor");
}

void remove_sx9310_grip_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->uv_device, grip_attrs);
}
