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

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define VENDOR		"TI"
#define CHIP_ID		"ADS1292IRSMR"

static ssize_t ecg_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t ecg_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static DEVICE_ATTR(name, S_IRUGO, ecg_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, ecg_vendor_show, NULL);

static struct device_attribute *ecg_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	NULL,
};

void initialize_ads1292_ecg_factorytest(struct ssp_data *data)
{
	sensors_register(data->ecg_device,
		data, ecg_attrs, "ecg_sensor");
}

void remove_ads1292_ecg_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->ecg_device, ecg_attrs);
}
