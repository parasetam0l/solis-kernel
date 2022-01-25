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

#define	VENDOR		"MAXIM"
#define	CHIP_ID		"MAX86902"

static ssize_t hrm_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t hrm_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(vendor, S_IRUGO, hrm_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, hrm_name_show, NULL);

static struct device_attribute *hrm_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	NULL,
};

void initialize_max86902_hrm_factorytest(struct ssp_data *data)
{
	sensors_register(data->front_hrm_device, data, hrm_attrs, "front_hrm_sensor");
}

void remove_max86902_hrm_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->front_hrm_device, hrm_attrs);
}
