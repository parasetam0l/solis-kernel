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
#include "ssp.h"

#undef SSP_FIRMWARE_REVISION_BCM
#if defined(CONFIG_POP)
#define SSP_FIRMWARE_REVISION_BCM   17091100
#elif defined(CONFIG_SOLIS)
#define SSP_FIRMWARE_REVISION_BCM   17101800
#else
#define SSP_FIRMWARE_REVISION_BCM   17080700
#endif

unsigned int get_module_rev(struct ssp_data *data)
{
	return SSP_FIRMWARE_REVISION_BCM;
}

void toggle_mcu_reset(struct ssp_data *data)
{
	gpio_set_value_cansleep(data->rst, 0);

	usleep_range(1000, 1200);

	gpio_set_value_cansleep(data->rst, 1);

	msleep(50);

	if (!gpio_get_value(data->mcu_int2))
		pr_err("[SSP]: SH has entered bootloader in %s !!!!!",
			__func__);
}

#if SSP_STATUS_MONITOR
void toggle_mcu_hw_reset(struct ssp_data *data)
{
	gpio_set_value_cansleep(data->rst, 0);
	usleep_range(1000, 1200);
	pr_err("[SSP]:%s !!", __func__);

	gpio_set_value_cansleep(data->ap_int, 0); /* MCU request */

	if (data->reg_hub) {
		int ret = regulator_disable(data->reg_hub);
		if (ret)
			pr_err("[SSP] Failed to disable reg_hub regulator(%d)\n",
				ret);
		msleep(400);
		ret = regulator_enable(data->reg_hub);
		if (ret) {
			pr_err("[SSP] Failed to enable reg_hub regulator(%d)\n",
				ret);
		}
		usleep_range(1000, 1200);
	} else
		pr_err("[SSP] No VCC regulator even hw_reset is called SW reset excuted!");

	gpio_set_value_cansleep(data->rst, 1);

	if (data->reg_hub)
		msleep(1200);
	else
		msleep(50);

	gpio_set_value_cansleep(data->ap_int, 1); /*MCU request */

	if (!gpio_get_value(data->mcu_int2))
		pr_err("[SSP]: SH has entered bootloader in %s !!!!!",
			__func__);
}
#endif
