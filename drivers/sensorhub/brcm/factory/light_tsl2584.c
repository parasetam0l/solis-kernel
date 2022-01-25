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

#define	VENDOR		"AMS"
#define	CHIP_ID		"TSL2584"

#define	TSL258X_LUX_CALC_OVER_FLOW		65535
#define MPOW                    0
#define MFACT				(1 << MPOW)

#define DGF		2745000
#define COFF_B	1083
#define COFF_C	988
#define COFF_D	1586
/*
#define DF         24.91
#define COFF_A       (u32)(100 * MFACT)
#define COFF_B       (u32)(147 * MFACT)
#define COFF_C       (u32)(90 * MFACT)
#define COFF_D       (u32)(149 * MFACT)

#define COFF_A_GLASS       (u32)((DF * COFF_A))
#define COFF_B_GLASS       (u32)((DF * COFF_B))
#define COFF_C_GLASS       (u32)((DF * COFF_C))
#define COFF_D_GLASS       (u32)((DF * COFF_D))

// set the coefficients the TSL2584TSV equation will use
#define TSL2584TSV_COFF_A COFF_A_GLASS
#define TSL2584TSV_COFF_B COFF_B_GLASS
#define TSL2584TSV_COFF_C COFF_C_GLASS
#define TSL2584TSV_COFF_D COFF_D_GLASS

struct taos_lux {
 unsigned int ratio;
 unsigned int coeffA;
 unsigned int coeffB;
 unsigned int coeffC;
 unsigned int coeffD;
};

static struct taos_lux taos_device_lux_tsl2584tsv[] = {
 { 0, TSL2584TSV_COFF_A, TSL2584TSV_COFF_B, TSL2584TSV_COFF_C, TSL2584TSV_COFF_D }
};
*/
#define max_lux(a,b) (((a) > (b)) ? (a) : (b))
#define min_lux(a,b) (((a) < (b)) ? (a) : (b))

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
uint32_t light_get_lux(uint16_t ch0, uint16_t ch1, int32_t gain)
{
	static uint32_t lux = 0;
	static int32_t ch0lux, ch1lux;

	if (ch0 == 0) {
		lux = 0;
		goto exit;
	}

	ch0lux = (int32_t)((DGF / 1000) * (ch0 - (ch1 * COFF_B / 1000)) / gain);
	ch1lux = (int32_t)((DGF / 1000) * ((ch0 * COFF_C / 1000) - (ch1 * COFF_D / 1000)) / gain);

	if (ch0lux < 0)
		ch0lux = 0;
	if (ch1lux < 0)
		ch1lux = 0;

	lux = min_lux(ch0lux, ch1lux);
	if (lux > TSL258X_LUX_CALC_OVER_FLOW)
		lux = TSL258X_LUX_CALC_OVER_FLOW;

exit:
	return lux;
}

static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->lux);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u16 ch0_raw = ((data->buf[LIGHT_SENSOR].ch0_upper << 8) |
		(data->buf[LIGHT_SENSOR].ch0_lower));
	u16 ch1_raw = ((data->buf[LIGHT_SENSOR].ch1_upper << 8) |
		(data->buf[LIGHT_SENSOR].ch1_lower));

	return snprintf(buf, PAGE_SIZE, "%u %u\n", ch0_raw, ch1_raw);
}

static DEVICE_ATTR(vendor, S_IRUGO, light_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, light_name_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

void initialize_tsl2584_light_factorytest(struct ssp_data *data)
{
	sensors_register(data->light_device, data, light_attrs, "light_sensor");
}

void remove_tsl2584_light_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->light_device, light_attrs);
}
