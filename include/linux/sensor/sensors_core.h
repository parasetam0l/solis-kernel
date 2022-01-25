/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _SENSORS_CORE_H_
#define _SENSORS_CORE_H_

#ifdef CONFIG_SENSORS_SSP
int sensors_create_symlink(struct input_dev *inputdev);
void sensors_remove_symlink(struct input_dev *inputdev);
#else
int sensors_create_symlink(struct kobject *target, const char *name);
void sensors_remove_symlink(struct kobject *target, const char *name);
void remap_sensor_data(s16 *val, u32 idx);
#endif
int sensors_register(struct device *dev, void *drvdata,
	struct device_attribute *attr[], char *name);
void sensors_unregister(struct device *dev, struct device_attribute *attr[]);
void destroy_sensor_class(void);

#endif
