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

/*************************************************************************/
/* SSP Kernel -> HAL input evnet function                                */
/*************************************************************************/

void report_meta_data(struct ssp_data *data, struct sensor_value *s)
{
	pr_info("[SSP]: %s - what: %d, sensor: %d\n", __func__,
		s->meta_data.what, s->meta_data.sensor);

	if (s->meta_data.sensor == ACCELEROMETER_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
		s16 accel_buf[3];
		memset(accel_buf, 0, sizeof(s16) * 3);
		input_report_rel(data->acc_input_dev, REL_X, accel_buf[0]);
		input_report_rel(data->acc_input_dev, REL_Y, accel_buf[1]);
		input_report_rel(data->acc_input_dev, REL_Z, accel_buf[2]);
		input_sync(data->acc_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == GYROSCOPE_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
		int gyro_buf[3];
		memset(gyro_buf, 0, sizeof(int) * 3);
		input_report_rel(data->gyro_input_dev, REL_RX, gyro_buf[0]);
		input_report_rel(data->gyro_input_dev, REL_RY, gyro_buf[1]);
		input_report_rel(data->gyro_input_dev, REL_RZ, gyro_buf[2]);
		input_sync(data->gyro_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == GYRO_UNCALIB_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
		s16 uncal_gyro_buf[6];
		memset(uncal_gyro_buf, 0, sizeof(s16) * 6);
		input_report_rel(data->uncalib_gyro_input_dev, REL_RX, uncal_gyro_buf[0]);
		input_report_rel(data->uncalib_gyro_input_dev, REL_RY, uncal_gyro_buf[1]);
		input_report_rel(data->uncalib_gyro_input_dev, REL_RZ, uncal_gyro_buf[2]);
		input_report_rel(data->uncalib_gyro_input_dev, REL_HWHEEL, uncal_gyro_buf[3]);
		input_report_rel(data->uncalib_gyro_input_dev, REL_DIAL, uncal_gyro_buf[4]);
		input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL, uncal_gyro_buf[5]);
		input_sync(data->uncalib_gyro_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == GEOMAGNETIC_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
		s16 mag_buf[3];
		memset(mag_buf, 0, sizeof(s16) * 3);
		input_report_rel(data->mag_input_dev, REL_RX, mag_buf[0]);
		input_report_rel(data->mag_input_dev, REL_RY, mag_buf[1]);
		input_report_rel(data->mag_input_dev, REL_RZ, mag_buf[2]);
		input_sync(data->mag_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == GEOMAGNETIC_UNCALIB_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
		s16 uncal_mag_buf[6];
		memset(uncal_mag_buf, 0, sizeof(s16) * 6);
		input_report_rel(data->uncal_mag_input_dev, REL_RX, uncal_mag_buf[0]);
		input_report_rel(data->uncal_mag_input_dev, REL_RY, uncal_mag_buf[1]);
		input_report_rel(data->uncal_mag_input_dev, REL_RZ, uncal_mag_buf[2]);
		input_report_rel(data->uncal_mag_input_dev, REL_HWHEEL, uncal_mag_buf[3]);
		input_report_rel(data->uncal_mag_input_dev, REL_DIAL, uncal_mag_buf[4]);
		input_report_rel(data->uncal_mag_input_dev, REL_WHEEL, uncal_mag_buf[5]);
		input_sync(data->uncal_mag_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == PRESSURE_SENSOR) {
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
		int pressure_buf[3];
		memset(pressure_buf, 0, sizeof(int) * 3);
		/* pressure */
		input_report_rel(data->pressure_input_dev, REL_HWHEEL, pressure_buf[0]);
		/* sealevel */
		input_report_rel(data->pressure_input_dev, REL_DIAL, pressure_buf[2]);
		/* temperature */
		input_report_rel(data->pressure_input_dev, REL_WHEEL, pressure_buf[1]);
		input_sync(data->pressure_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == ROTATION_VECTOR) {
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
		int rot_buf[5];
		memset(rot_buf, 0, sizeof(int) * 5);
		input_report_rel(data->rot_input_dev, REL_X, rot_buf[0]);
		input_report_rel(data->rot_input_dev, REL_Y, rot_buf[1]);
		input_report_rel(data->rot_input_dev, REL_Z, rot_buf[2]);
		input_report_rel(data->rot_input_dev, REL_RX, rot_buf[3]);
		input_report_rel(data->rot_input_dev, REL_RY, rot_buf[4] + 1);
		input_sync(data->rot_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else if (s->meta_data.sensor == GAME_ROTATION_VECTOR) {
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
		int grot_buf[5];
		memset(grot_buf, 0, sizeof(int) * 5);
		input_report_rel(data->game_rot_input_dev, REL_X, grot_buf[0]);
		input_report_rel(data->game_rot_input_dev, REL_Y, grot_buf[1]);
		input_report_rel(data->game_rot_input_dev, REL_Z, grot_buf[2]);
		input_report_rel(data->game_rot_input_dev, REL_RX, grot_buf[3]);
		input_report_rel(data->game_rot_input_dev, REL_RY, grot_buf[4] + 1);
		input_sync(data->game_rot_input_dev);
#else
		pr_warn("[SSP]:%s : There are no input_dev!!\n", __func__);
#endif
	} else {
#ifdef CONFIG_TIZEN
		/* Current all Tizen_FW does not use meta_input event/interface
		 * So just return them without toss to platform each actual data
		 */
		pr_info("[SSP]: TizenFW does not use meta data!!\n");
#else
		input_report_rel(data->meta_input_dev, REL_DIAL, s->meta_data.what);
		input_report_rel(data->meta_input_dev, REL_HWHEEL, s->meta_data.sensor + 1);
		input_sync(data->meta_input_dev);
#endif
	}
}

#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
void report_acc_data(struct ssp_data *data, struct sensor_value *accdata)
{
	data->buf[ACCELEROMETER_SENSOR].x = accdata->x;
	data->buf[ACCELEROMETER_SENSOR].y = accdata->y;
	data->buf[ACCELEROMETER_SENSOR].z = accdata->z;

	input_report_rel(data->acc_input_dev, REL_X,
		data->buf[ACCELEROMETER_SENSOR].x);
	input_report_rel(data->acc_input_dev, REL_Y,
		data->buf[ACCELEROMETER_SENSOR].y);
	input_report_rel(data->acc_input_dev, REL_Z,
		data->buf[ACCELEROMETER_SENSOR].z);
	input_report_rel(data->acc_input_dev, REL_Z + 1,
		(accdata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->acc_input_dev, REL_Z + 2,
		(accdata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->acc_input_dev, REL_Z + 3,
		(accdata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->acc_input_dev, REL_Z + 4,
		accdata->timestamp & 0x000000000000ffff);
	input_sync(data->acc_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, accel=[%d %d %d %lld]\n", __func__,
		accdata->x, accdata->y, accdata->z, accdata->timestamp);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
void report_gyro_data(struct ssp_data *data, struct sensor_value *gyrodata)
{
	int lTemp[3] = {0,};

	data->buf[GYROSCOPE_SENSOR].x = gyrodata->x;
	data->buf[GYROSCOPE_SENSOR].y = gyrodata->y;
	data->buf[GYROSCOPE_SENSOR].z = gyrodata->z;

	lTemp[0] = (int)data->buf[GYROSCOPE_SENSOR].x;
	lTemp[1] = (int)data->buf[GYROSCOPE_SENSOR].y;
	lTemp[2] = (int)data->buf[GYROSCOPE_SENSOR].z;

	input_report_rel(data->gyro_input_dev, REL_RX, lTemp[0]);
	input_report_rel(data->gyro_input_dev, REL_RY, lTemp[1]);
	input_report_rel(data->gyro_input_dev, REL_RZ, lTemp[2]);
	input_report_rel(data->gyro_input_dev, REL_RZ + 1,
		(gyrodata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->gyro_input_dev, REL_RZ + 2,
		(gyrodata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->gyro_input_dev, REL_RZ + 3,
		(gyrodata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->gyro_input_dev, REL_RZ + 4,
		gyrodata->timestamp & 0x000000000000ffff);
	input_sync(data->gyro_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, gyro=[%d %d %d %lld]\n", __func__,
		gyrodata->x, gyrodata->y, gyrodata->z, gyrodata->timestamp);
#endif
}

void report_uncalib_gyro_data(struct ssp_data *data,
	struct sensor_value *gyrodata)
{
	data->buf[GYRO_UNCALIB_SENSOR].uncal_x = gyrodata->uncal_x;
	data->buf[GYRO_UNCALIB_SENSOR].uncal_y = gyrodata->uncal_y;
	data->buf[GYRO_UNCALIB_SENSOR].uncal_z = gyrodata->uncal_z;
	data->buf[GYRO_UNCALIB_SENSOR].offset_x = gyrodata->offset_x;
	data->buf[GYRO_UNCALIB_SENSOR].offset_y = gyrodata->offset_y;
	data->buf[GYRO_UNCALIB_SENSOR].offset_z = gyrodata->offset_z;

	input_report_rel(data->uncalib_gyro_input_dev, REL_RX,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncalib_gyro_input_dev, REL_RY,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncalib_gyro_input_dev, REL_RZ,
		data->buf[GYRO_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncalib_gyro_input_dev, REL_HWHEEL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncalib_gyro_input_dev, REL_DIAL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL,
		data->buf[GYRO_UNCALIB_SENSOR].offset_z);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL + 1,
		(gyrodata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL + 2,
		(gyrodata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL + 3,
		(gyrodata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->uncalib_gyro_input_dev, REL_WHEEL + 4,
		gyrodata->timestamp & 0x000000000000ffff);
	input_sync(data->uncalib_gyro_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, uncal gyro=[%d %d %d], offset=[%d %d %d], time=[%lld]\n",
		__func__, gyrodata->uncal_x, gyrodata->uncal_y,
		gyrodata->uncal_z, gyrodata->offset_x, gyrodata->offset_y,
		gyrodata->offset_z, gyrodata->timestamp);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
void report_geomagnetic_raw_data(struct ssp_data *data,
	struct sensor_value *magrawdata)
{
	data->buf[GEOMAGNETIC_RAW].x = magrawdata->x;
	data->buf[GEOMAGNETIC_RAW].y = magrawdata->y;
	data->buf[GEOMAGNETIC_RAW].z = magrawdata->z;

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, mag raw=[%d %d %d]\n", __func__,
		magrawdata->x, magrawdata->y, magrawdata->z);
#endif
}

void report_mag_data(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_SENSOR].cal_x = magdata->cal_x;
	data->buf[GEOMAGNETIC_SENSOR].cal_y = magdata->cal_y;
	data->buf[GEOMAGNETIC_SENSOR].cal_z = magdata->cal_z;
	data->buf[GEOMAGNETIC_SENSOR].accuracy = magdata->accuracy;

	input_report_rel(data->mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_SENSOR].cal_x);
	input_report_rel(data->mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_SENSOR].cal_y);
	input_report_rel(data->mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_SENSOR].cal_z);
	input_report_rel(data->mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_SENSOR].accuracy + 1);
	input_sync(data->mag_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, mag=[%d %d %d], accuray=%d]\n", __func__,
		magdata->cal_x, magdata->cal_y, magdata->cal_z,
		magdata->accuracy);
#endif
}

void report_mag_uncaldata(struct ssp_data *data, struct sensor_value *magdata)
{
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x = magdata->uncal_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y = magdata->uncal_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z = magdata->uncal_z;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x = magdata->offset_x;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y = magdata->offset_y;
	data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z = magdata->offset_z;

	input_report_rel(data->uncal_mag_input_dev, REL_RX,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_x);
	input_report_rel(data->uncal_mag_input_dev, REL_RY,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_y);
	input_report_rel(data->uncal_mag_input_dev, REL_RZ,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].uncal_z);
	input_report_rel(data->uncal_mag_input_dev, REL_HWHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_x);
	input_report_rel(data->uncal_mag_input_dev, REL_DIAL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_y);
	input_report_rel(data->uncal_mag_input_dev, REL_WHEEL,
		data->buf[GEOMAGNETIC_UNCALIB_SENSOR].offset_z);
	input_sync(data->uncal_mag_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, uncal mag=[%d %d %d], offset=[%d %d %d]\n",
		__func__, magdata->uncal_x, magdata->uncal_y,
		magdata->uncal_z, magdata->offset_x, magdata->offset_y,
		magdata->offset_z);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
void report_pressure_data(struct ssp_data *data, struct sensor_value *predata)
{
	int temp[3] = {0,};
	data->buf[PRESSURE_SENSOR].pressure[0] =
		predata->pressure[0] - data->iPressureCal;
	data->buf[PRESSURE_SENSOR].pressure[1] = predata->pressure[1];

	temp[0] = data->buf[PRESSURE_SENSOR].pressure[0];
	temp[1] = data->buf[PRESSURE_SENSOR].pressure[1];
	temp[2] = data->sealevelpressure;

	/* pressure */
	input_report_rel(data->pressure_input_dev, REL_HWHEEL, temp[0]);
	/* sealevel */
	input_report_rel(data->pressure_input_dev, REL_DIAL, temp[2]);
	/* temperature */
	input_report_rel(data->pressure_input_dev, REL_WHEEL, temp[1]);
	input_report_rel(data->pressure_input_dev, REL_WHEEL + 1,
		(predata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->pressure_input_dev, REL_WHEEL + 2,
		(predata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->pressure_input_dev, REL_WHEEL + 3,
		(predata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->pressure_input_dev, REL_WHEEL + 4,
		predata->timestamp & 0x000000000000ffff);
	input_sync(data->pressure_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, pressure=%d, temp=%d, sealevel=%d, time=%lld\n", __func__,
		temp[0], temp[1], temp[2], predata->timestamp);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
#if defined(CONFIG_SENSORS_SSP_TSL2584)
void report_light_data(struct ssp_data *data, struct sensor_value *lightdata)
{
	u16 ch0_raw, ch1_raw;

	ch0_raw = (((u16)lightdata->ch0_upper << 8) & 0xff00) | (((u16)lightdata->ch0_lower) & 0x00ff);
	ch1_raw = (((u16)lightdata->ch1_upper << 8) & 0xff00) | (((u16)lightdata->ch1_lower) & 0x00ff);

	data->buf[LIGHT_SENSOR].ch0_lower = lightdata->ch0_lower;
	data->buf[LIGHT_SENSOR].ch0_upper = lightdata->ch0_upper;
	data->buf[LIGHT_SENSOR].ch1_lower = lightdata->ch1_lower;
	data->buf[LIGHT_SENSOR].ch1_upper = lightdata->ch1_upper;
	data->buf[LIGHT_SENSOR].gain = lightdata->gain;

	data->lux = light_get_lux(ch0_raw,  ch1_raw, lightdata->gain);
	input_report_rel(data->light_input_dev, REL_RX, data->lux + 1);

	input_report_rel(data->light_input_dev, (REL_RX + 1),
		(lightdata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->light_input_dev, (REL_RX + 2),
		(lightdata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->light_input_dev, (REL_RX + 3),
		(lightdata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->light_input_dev, (REL_RX + 4),
		(lightdata->timestamp & 0x000000000000ffff));
	input_sync(data->light_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, lux=%d, ch0=[%d %d]=%d, ch1=[%d %d]=%d, gian=%d, time=%lld\n",
		__func__, data->lux, lightdata->ch0_lower, lightdata->ch0_upper,
		ch0_raw, lightdata->ch1_lower, lightdata->ch1_upper, ch1_raw,
		lightdata->gain, lightdata->timestamp);
#endif
}
#elif defined(CONFIG_SENSORS_SSP_CM3323)
void report_light_data(struct ssp_data *data, struct sensor_value *lightdata)
{
	data->buf[LIGHT_SENSOR].r = lightdata->r;
	data->buf[LIGHT_SENSOR].g = lightdata->g;
	data->buf[LIGHT_SENSOR].b = lightdata->b;
	data->buf[LIGHT_SENSOR].w = lightdata->w;

	input_report_rel(data->light_input_dev, REL_HWHEEL,
		data->buf[LIGHT_SENSOR].r + 1);
	input_report_rel(data->light_input_dev, REL_DIAL,
		data->buf[LIGHT_SENSOR].g + 1);
	input_report_rel(data->light_input_dev, REL_WHEEL,
		data->buf[LIGHT_SENSOR].b + 1);
	input_report_rel(data->light_input_dev, REL_MISC,
		data->buf[LIGHT_SENSOR].w + 1);

	input_report_rel(data->light_input_dev, (REL_MISC + 1),
		(lightdata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->light_input_dev, (REL_MISC + 2),
		(lightdata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->light_input_dev, (REL_MISC + 3),
		(lightdata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->light_input_dev, (REL_MISC + 4),
		(lightdata->timestamp & 0x000000000000ffff));
	input_sync(data->light_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, r=%d, g=%d, b=%d, w=%d time=%lld\n", __func__,
		lightdata->r, lightdata->g, lightdata->b, lightdata->w, lightdata->timestamp);
#endif
}
#endif
#endif

#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
void report_temp_humidity_data(struct ssp_data *data,
	struct sensor_value *temp_humi_data)
{
#ifdef CONFIG_SENSORS_SSP_SKIN_TEMP
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].skin_temp = temp_humi_data->skin_temp;
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].skin_humid = temp_humi_data->skin_humid;
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].env_temp = temp_humi_data->env_temp;
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].env_humid= temp_humi_data->env_humid;

	input_report_rel(data->temp_humi_input_dev, REL_HWHEEL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].skin_temp + 1);
	input_report_rel(data->temp_humi_input_dev, REL_DIAL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].skin_humid + 1);
	input_report_rel(data->temp_humi_input_dev, REL_WHEEL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].env_temp + 1);
	input_report_rel(data->temp_humi_input_dev, REL_MISC,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].env_humid + 1);
	input_sync(data->temp_humi_input_dev);

#if 1//def SSP_DEBUG_LOG
	pr_info("[SSP]%s, skin_temp=%d, skin_humid=%d, env_temp=%d, env_humid=%d\n",
		__func__, temp_humi_data->skin_temp, temp_humi_data->skin_humid,
		temp_humi_data->env_temp, temp_humi_data->env_humid);
#endif
#else
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].temp = temp_humi_data->temp;
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].humi = temp_humi_data->humi;
	data->buf[TEMPERATURE_HUMIDITY_SENSOR].time = temp_humi_data->time;

	/* Temperature */
	input_report_rel(data->temp_humi_input_dev, REL_HWHEEL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].temp);
	/* Humidity */
	input_report_rel(data->temp_humi_input_dev, REL_DIAL,
		data->buf[TEMPERATURE_HUMIDITY_SENSOR].humi);
	input_sync(data->temp_humi_input_dev);
	if (data->buf[TEMPERATURE_HUMIDITY_SENSOR].time)
		wake_lock_timeout(&data->ssp_wake_lock, 0.3 * HZ);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, temp=%d, humidity=%d", __func__,
		temp_humi_data->x, temp_humi_data->y);
#endif
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
void report_rot_data(struct ssp_data *data, struct sensor_value *rotdata)
{
	int rot_buf[5];

	data->buf[ROTATION_VECTOR].quat_a = rotdata->quat_a;
	data->buf[ROTATION_VECTOR].quat_b = rotdata->quat_b;
	data->buf[ROTATION_VECTOR].quat_c = rotdata->quat_c;
	data->buf[ROTATION_VECTOR].quat_d = rotdata->quat_d;
	data->buf[ROTATION_VECTOR].acc_rot = rotdata->acc_rot;

	rot_buf[0] = rotdata->quat_a;
	rot_buf[1] = rotdata->quat_b;
	rot_buf[2] = rotdata->quat_c;
	rot_buf[3] = rotdata->quat_d;
	rot_buf[4] = rotdata->acc_rot;

	input_report_rel(data->rot_input_dev, REL_X, rot_buf[0]);
	input_report_rel(data->rot_input_dev, REL_Y, rot_buf[1]);
	input_report_rel(data->rot_input_dev, REL_Z, rot_buf[2]);
	input_report_rel(data->rot_input_dev, REL_RX, rot_buf[3]);
	input_report_rel(data->rot_input_dev, REL_RY, rot_buf[4] + 1);
	input_sync(data->rot_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, quat=[%d %d %d %d], acc_rot=%d\n", __func__,
		rotdata->quat_a, rotdata->quat_b, rotdata->quat_c,
		rotdata->quat_d, rotdata->acc_rot);
#endif
}

void report_game_rot_data(struct ssp_data *data,
	struct sensor_value *grvec_data)
{
	int grot_buf[5];

	data->buf[GAME_ROTATION_VECTOR].quat_a = grvec_data->quat_a;
	data->buf[GAME_ROTATION_VECTOR].quat_b = grvec_data->quat_b;
	data->buf[GAME_ROTATION_VECTOR].quat_c = grvec_data->quat_c;
	data->buf[GAME_ROTATION_VECTOR].quat_d = grvec_data->quat_d;
	data->buf[GAME_ROTATION_VECTOR].acc_rot = grvec_data->acc_rot;

	grot_buf[0] = grvec_data->quat_a;
	grot_buf[1] = grvec_data->quat_b;
	grot_buf[2] = grvec_data->quat_c;
	grot_buf[3] = grvec_data->quat_d;
	grot_buf[4] = grvec_data->acc_rot;

	input_report_rel(data->game_rot_input_dev, REL_X, grot_buf[0]);
	input_report_rel(data->game_rot_input_dev, REL_Y, grot_buf[1]);
	input_report_rel(data->game_rot_input_dev, REL_Z, grot_buf[2]);
	input_report_rel(data->game_rot_input_dev, REL_RX, grot_buf[3]);
	input_report_rel(data->game_rot_input_dev, REL_RY, grot_buf[4] + 1);
	input_sync(data->game_rot_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, quat=[%d %d %d %d], acc_rot=%d\n", __func__,
		grvec_data->quat_a, grvec_data->quat_b, grvec_data->quat_c,
		grvec_data->quat_d, grvec_data->acc_rot);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
void report_hrm_raw_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[HRM_RAW_SENSOR].ch_a_sum = hrmdata->ch_a_sum;
	data->buf[HRM_RAW_SENSOR].ch_a_x1 = hrmdata->ch_a_x1;
	data->buf[HRM_RAW_SENSOR].ch_a_x2 = hrmdata->ch_a_x2;
	data->buf[HRM_RAW_SENSOR].ch_a_y1 = hrmdata->ch_a_y1;
	data->buf[HRM_RAW_SENSOR].ch_a_y2 = hrmdata->ch_a_y2;
	data->buf[HRM_RAW_SENSOR].ch_b_sum = hrmdata->ch_b_sum;
	data->buf[HRM_RAW_SENSOR].ch_b_x1 = hrmdata->ch_b_x1;
	data->buf[HRM_RAW_SENSOR].ch_b_x2 = hrmdata->ch_b_x2;
	data->buf[HRM_RAW_SENSOR].ch_b_y1 = hrmdata->ch_b_y1;
	data->buf[HRM_RAW_SENSOR].ch_b_y2 = hrmdata->ch_b_y2;

	input_report_rel(data->hrm_raw_input_dev, REL_X,
		data->buf[HRM_RAW_SENSOR].ch_a_sum + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Y,
		data->buf[HRM_RAW_SENSOR].ch_a_x1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_Z,
		data->buf[HRM_RAW_SENSOR].ch_a_x2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_RX,
		data->buf[HRM_RAW_SENSOR].ch_a_y1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_RY,
		data->buf[HRM_RAW_SENSOR].ch_a_y2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_RZ,
		data->buf[HRM_RAW_SENSOR].ch_b_sum + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_HWHEEL,
		data->buf[HRM_RAW_SENSOR].ch_b_x1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_DIAL,
		data->buf[HRM_RAW_SENSOR].ch_b_x2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_WHEEL,
		data->buf[HRM_RAW_SENSOR].ch_b_y1 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_MISC,
		data->buf[HRM_RAW_SENSOR].ch_b_y2 + 1);
	input_report_rel(data->hrm_raw_input_dev, REL_MISC + 1,
		(hrmdata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->hrm_raw_input_dev, REL_MISC + 2,
		(hrmdata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->hrm_raw_input_dev, REL_MISC + 3,
		(hrmdata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->hrm_raw_input_dev, REL_MISC + 4,
		hrmdata->timestamp & 0x000000000000ffff);
	input_sync(data->hrm_raw_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, [%d %d %d %d %d] [%d %d %d %d %d] time=%lld\n", __func__,
		hrmdata->ch_a_sum, hrmdata->ch_a_x1,
		hrmdata->ch_a_x2, hrmdata->ch_a_y1,
		hrmdata->ch_a_y2, hrmdata->ch_b_sum,
		hrmdata->ch_b_x1, hrmdata->ch_b_x2,
		hrmdata->ch_b_y1, hrmdata->ch_b_y2,
		hrmdata->timestamp);
#endif
}

void report_hrm_raw_fac_data(struct ssp_data *data,
	struct sensor_value *hrmdata)
{
	memcpy(data->buf[HRM_RAW_FAC_SENSOR].hrm_eol_data,
		hrmdata->hrm_eol_data, sizeof(hrmdata->hrm_eol_data));

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, [%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		__func__, hrmdata->hrm_eol_data[0], hrmdata->hrm_eol_data[1],
		hrmdata->hrm_eol_data[2], hrmdata->hrm_eol_data[3],
		hrmdata->hrm_eol_data[4], hrmdata->hrm_eol_data[5],
		hrmdata->hrm_eol_data[6], hrmdata->hrm_eol_data[7],
		hrmdata->hrm_eol_data[8], hrmdata->hrm_eol_data[9],
		hrmdata->hrm_eol_data[10], hrmdata->hrm_eol_data[11],
		hrmdata->hrm_eol_data[12], hrmdata->hrm_eol_data[13],
		hrmdata->hrm_eol_data[14], hrmdata->hrm_eol_data[15]);
#endif
}

void report_hrm_lib_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[HRM_LIB_SENSOR].hr = hrmdata->hr;
	data->buf[HRM_LIB_SENSOR].rri = hrmdata->rri;
	data->buf[HRM_LIB_SENSOR].snr = hrmdata->snr;

	input_report_rel(data->hrm_lib_input_dev, REL_X,
		data->buf[HRM_LIB_SENSOR].hr + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Y,
		data->buf[HRM_LIB_SENSOR].rri + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Z,
		data->buf[HRM_LIB_SENSOR].snr + 1);
	input_report_rel(data->hrm_lib_input_dev, REL_Z + 1,
		(hrmdata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->hrm_lib_input_dev, REL_Z + 2,
		(hrmdata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->hrm_lib_input_dev, REL_Z + 3,
		(hrmdata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->hrm_lib_input_dev, REL_Z + 4,
		hrmdata->timestamp & 0x000000000000ffff);
	input_sync(data->hrm_lib_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, hr=%d rri=%d snr=%d time=%lld\n", __func__,
		hrmdata->hr, hrmdata->rri, hrmdata->snr, hrmdata->timestamp);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
void report_front_hrm_raw_data(struct ssp_data *data,
	struct sensor_value *hrmdata)
{
	int ir_data, red_data;
	memcpy(data->buf[FRONT_HRM_RAW_SENSOR].front_hrm_raw,
		hrmdata->front_hrm_raw, sizeof(hrmdata->front_hrm_raw));

	ir_data = (hrmdata->front_hrm_raw[0] << 16 & 0x30000) |
			(hrmdata->front_hrm_raw[1] << 8) |
			(hrmdata->front_hrm_raw[2]);

	red_data = (hrmdata->front_hrm_raw[3] << 16 & 0x30000) |
			(hrmdata->front_hrm_raw[4] << 8) |
			(hrmdata->front_hrm_raw[5]);


	input_report_rel(data->front_hrm_raw_input_dev, REL_X, ir_data + 1);
	input_report_rel(data->front_hrm_raw_input_dev, REL_Y, red_data + 1);

	input_report_rel(data->front_hrm_raw_input_dev, REL_Y + 1,
		(hrmdata->timestamp & 0xffff000000000000) >> 48);
	input_report_rel(data->front_hrm_raw_input_dev, REL_Y + 2,
		(hrmdata->timestamp & 0x0000ffff00000000) >> 32);
	input_report_rel(data->front_hrm_raw_input_dev, REL_Y + 3,
		(hrmdata->timestamp & 0x00000000ffff0000) >> 16);
	input_report_rel(data->front_hrm_raw_input_dev, REL_Y + 4,
		hrmdata->timestamp & 0x000000000000ffff);
	input_sync(data->front_hrm_raw_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, [%d %d %d : %d] [%d %d %d : %d] time=%lld\n", __func__,
		hrmdata->front_hrm_raw[0], hrmdata->front_hrm_raw[1],
		hrmdata->front_hrm_raw[2], ir_data + 1,
		hrmdata->front_hrm_raw[3], hrmdata->front_hrm_raw[4],
		hrmdata->front_hrm_raw[5], red_data + 1, hrmdata->timestamp);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
void report_uv_data(struct ssp_data *data, struct sensor_value *hrmdata)
{
	data->buf[UV_SENSOR].uv_raw = hrmdata->uv_raw;
	data->buf[UV_SENSOR].hrm_temp = hrmdata->hrm_temp;

	input_report_rel(data->uv_input_dev, REL_X,
		data->buf[UV_SENSOR].uv_raw + 1);
	input_report_rel(data->uv_input_dev, REL_Y,
		data->buf[UV_SENSOR].hrm_temp + 1);
	input_sync(data->uv_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, uv_raw=%d hrm_temp=%d\n", __func__,
		hrmdata->uv_raw, hrmdata->hrm_temp);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
void report_gsr_data(struct ssp_data *data, struct sensor_value *gsrdata)
{
	data->buf[GSR_SENSOR].ohm = gsrdata->ohm;
	data->buf[GSR_SENSOR].adc = gsrdata->adc;
	data->buf[GSR_SENSOR].inj_c = gsrdata->inj_c;

	input_report_rel(data->gsr_input_dev, REL_HWHEEL, data->buf[GSR_SENSOR].ohm + 1);
	input_report_rel(data->gsr_input_dev, REL_DIAL, data->buf[GSR_SENSOR].adc + 1);
	input_report_rel(data->gsr_input_dev, REL_MISC, data->buf[GSR_SENSOR].inj_c+ 1);

	input_sync(data->gsr_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, ohm=%d adc=%d inj_c=%d\n", __func__,
		data->buf[GSR_SENSOR].ohm, data->buf[GSR_SENSOR].adc, data->buf[GSR_SENSOR].inj_c);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
void report_ecg_data(struct ssp_data *data, struct sensor_value *ecgdata)
{
	int ecg, enable_flag;
	memcpy(data->buf[ECG_SENSOR].ecg_data,
		ecgdata->ecg_data, sizeof(ecgdata->ecg_data));

	ecg = (ecgdata->ecg_data[0]) |(ecgdata->ecg_data[1] << 8) |(ecgdata->ecg_data[2] << 16);
	enable_flag = (ecgdata->ecg_data[3]) |(ecgdata->ecg_data[4] << 8) |(ecgdata->ecg_data[5] << 16);

	input_report_rel(data->ecg_input_dev, REL_HWHEEL, ecg + 1);
	input_report_rel(data->ecg_input_dev, REL_DIAL, enable_flag + 1);

	input_sync(data->ecg_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, ecg=%d, enable_flag=%d\n", __func__, ecg, enable_flag);
#endif
}
#endif

#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
void report_grip_data(struct ssp_data *data, struct sensor_value *gripdata)
{
	data->buf[GRIP_SENSOR].data1 = gripdata->data1;
	data->buf[GRIP_SENSOR].data2 = gripdata->data2;
	data->buf[GRIP_SENSOR].data3 = gripdata->data3;
	data->buf[GRIP_SENSOR].data4 = gripdata->data4;

	input_report_rel(data->grip_input_dev, REL_X,
		data->buf[GRIP_SENSOR].data1 + 1);
	input_report_rel(data->grip_input_dev, REL_Y,
		data->buf[GRIP_SENSOR].data2 + 1);
	input_report_rel(data->grip_input_dev, REL_Z,
		data->buf[GRIP_SENSOR].data3 + 1);
	input_report_rel(data->grip_input_dev, REL_RX,
		data->buf[GRIP_SENSOR].data4 + 1);
	input_sync(data->grip_input_dev);

#ifdef SSP_DEBUG_LOG
	pr_info("[SSP]%s, data1=%d, data2=%d, data3=%d, data4=%d\n",
		__func__, gripdata->data1, gripdata->data2,
		gripdata->data3, gripdata->data4);
#endif
}
#endif

void remove_event_symlink(struct ssp_data *data)
{
	if (data->gsr_input_dev)
		sensors_remove_symlink(data->gsr_input_dev);
	if (data->uv_input_dev)
		sensors_remove_symlink(data->uv_input_dev);
	if (data->front_hrm_raw_input_dev)
		sensors_remove_symlink(data->front_hrm_raw_input_dev);
	if (data->hrm_lib_input_dev)
		sensors_remove_symlink(data->hrm_lib_input_dev);
	if (data->hrm_raw_input_dev)
		sensors_remove_symlink(data->hrm_raw_input_dev);
	if (data->game_rot_input_dev)
		sensors_remove_symlink(data->game_rot_input_dev);
	if (data->rot_input_dev)
		sensors_remove_symlink(data->rot_input_dev);
	if (data->pressure_input_dev)
		sensors_remove_symlink(data->pressure_input_dev);
	if (data->uncalib_gyro_input_dev)
		sensors_remove_symlink(data->uncalib_gyro_input_dev);
	if (data->uncal_mag_input_dev)
		sensors_remove_symlink(data->uncal_mag_input_dev);
	if (data->mag_input_dev)
		sensors_remove_symlink(data->mag_input_dev);
	if (data->gyro_input_dev)
		sensors_remove_symlink(data->gyro_input_dev);
	if (data->acc_input_dev)
		sensors_remove_symlink(data->acc_input_dev);
}

int initialize_event_symlink(struct ssp_data *data)
{
	int iRet = 0;

	if (data->acc_input_dev) {
		iRet = sensors_create_symlink(data->acc_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->gyro_input_dev) {
		iRet = sensors_create_symlink(data->gyro_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->mag_input_dev) {
		iRet = sensors_create_symlink(data->mag_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->uncal_mag_input_dev) {
		iRet = sensors_create_symlink(data->uncal_mag_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->uncalib_gyro_input_dev) {
		iRet = sensors_create_symlink(data->uncalib_gyro_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->pressure_input_dev) {
		iRet = sensors_create_symlink(data->pressure_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->light_input_dev) {
		iRet = sensors_create_symlink(data->light_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->rot_input_dev) {
		iRet = sensors_create_symlink(data->rot_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->game_rot_input_dev) {
		iRet = sensors_create_symlink(data->game_rot_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->hrm_raw_input_dev) {
		iRet = sensors_create_symlink(data->hrm_raw_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->hrm_lib_input_dev) {
		iRet = sensors_create_symlink(data->hrm_lib_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->front_hrm_raw_input_dev) {
		iRet = sensors_create_symlink(data->front_hrm_raw_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->uv_input_dev) {
		iRet = sensors_create_symlink(data->uv_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->gsr_input_dev) {
		iRet = sensors_create_symlink(data->gsr_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}
	if (data->skin_temp_input_dev) {
		iRet = sensors_create_symlink(data->skin_temp_input_dev);
		if (iRet < 0)
			goto err_sysfs_create_link;
	}

	return SUCCESS;

err_sysfs_create_link:
	remove_event_symlink(data);
	pr_err("[SSP]: %s - could not create event symlink\n", __func__);

	return FAIL;
}

void remove_input_dev(struct ssp_data *data)
{
	if (data->skin_temp_input_dev) {
		input_unregister_device(data->skin_temp_input_dev);
		pr_err("[SSP]: %s, unregister skin temp input device\n",
			__func__);
	}
	if (data->ecg_input_dev) {
		input_unregister_device(data->ecg_input_dev);
		pr_err("[SSP]: %s, unregister ecg input device\n",
			__func__);
	}
	if (data->gsr_input_dev) {
		input_unregister_device(data->gsr_input_dev);
		pr_err("[SSP]: %s, unregister gsr input device\n",
			__func__);
	}
	if (data->uv_input_dev) {
		input_unregister_device(data->uv_input_dev);
		pr_err("[SSP]: %s, unregister uv input device\n", __func__);
	}
	if (data->front_hrm_raw_input_dev) {
		input_unregister_device(data->front_hrm_raw_input_dev);
		pr_err("[SSP]: %s, unregister front hrm raw input device\n",
			__func__);
	}
	if (data->hrm_lib_input_dev) {
		input_unregister_device(data->hrm_lib_input_dev);
		pr_err("[SSP]: %s, unregister hrm lib input device\n",
			__func__);
	}
	if (data->hrm_raw_input_dev) {
		input_unregister_device(data->hrm_raw_input_dev);
		pr_err("[SSP]: %s, unregister hrm raw input device\n",
			__func__);
	}
	if (data->motion_input_dev) {
		input_unregister_device(data->motion_input_dev);
		pr_err("[SSP]: %s, unregister motion input device\n", __func__);
	}
	if (data->game_rot_input_dev) {
		input_unregister_device(data->game_rot_input_dev);
		pr_err("[SSP]: %s, unregister game_rot input device\n",
			__func__);
	}
	if (data->rot_input_dev) {
		input_unregister_device(data->rot_input_dev);
		pr_err("[SSP]: %s, unregister rot input device\n", __func__);
	}
	if (data->pressure_input_dev) {
		input_unregister_device(data->pressure_input_dev);
		pr_err("[SSP]: %s, unregister pressure input device\n",
			__func__);
	}
	if (data->temp_humi_input_dev) {
		input_unregister_device(data->temp_humi_input_dev);
		pr_info("[SSP]: %s, unregister temp_humid input device",
			__func__);
	}
	if (data->light_input_dev) {
		input_unregister_device(data->light_input_dev);
		pr_err("[SSP]: %s, unregister light input device\n",
			__func__);
	}
	if (data->uncalib_gyro_input_dev) {
		input_unregister_device(data->uncalib_gyro_input_dev);
		pr_err("[SSP]: %s, unregister uncal gyro input device\n",
			__func__);
	}
	if (data->uncal_mag_input_dev) {
		input_unregister_device(data->uncal_mag_input_dev);
		pr_err("[SSP]: %s, unregister uncal mag input device\n",
			__func__);
	}
	if (data->mag_input_dev) {
		input_unregister_device(data->mag_input_dev);
		pr_err("[SSP]: %s, unregister mag input device\n", __func__);
	}
	if (data->gyro_input_dev) {
		input_unregister_device(data->gyro_input_dev);
		pr_err("[SSP]: %s, unregister gyro input device\n", __func__);
	}
	if (data->acc_input_dev) {
		input_unregister_device(data->acc_input_dev);
		pr_err("[SSP]: %s, unregister acc input device\n", __func__);
	}
}

int initialize_input_dev(struct ssp_data *data)
{
	int iRet = 0;

#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	data->acc_input_dev = input_allocate_device();
	if (data->acc_input_dev == NULL)
		goto err_initialize_input_dev;

	data->acc_input_dev->name = "accelerometer_sensor";
	input_set_capability(data->acc_input_dev, EV_REL, REL_X);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Y);
	input_set_capability(data->acc_input_dev, EV_REL, REL_Z);

	/* for timestemp reporting with batching mode */
	input_set_capability(data->acc_input_dev, EV_REL, (REL_Z + 1));
	input_set_capability(data->acc_input_dev, EV_REL, (REL_Z + 2));
	input_set_capability(data->acc_input_dev, EV_REL, (REL_Z + 3));
	input_set_capability(data->acc_input_dev, EV_REL, (REL_Z + 4));

	/* for considering batching condtiion */
	input_set_events_per_packet(data->acc_input_dev, 512);

	iRet = input_register_device(data->acc_input_dev);
	if (iRet < 0) {
		input_free_device(data->acc_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->acc_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	data->gyro_input_dev = input_allocate_device();
	if (data->gyro_input_dev == NULL)
		goto err_initialize_input_dev;

	data->gyro_input_dev->name = "gyro_sensor";
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->gyro_input_dev, EV_REL, REL_RZ);

	/* for timestemp reporting with batching mode */
	input_set_capability(data->gyro_input_dev, EV_REL, (REL_RZ + 1));
	input_set_capability(data->gyro_input_dev, EV_REL, (REL_RZ + 2));
	input_set_capability(data->gyro_input_dev, EV_REL, (REL_RZ + 3));
	input_set_capability(data->gyro_input_dev, EV_REL, (REL_RZ + 4));

	/* for considering batching condtiion */
	input_set_events_per_packet(data->gyro_input_dev, 512);

	iRet = input_register_device(data->gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->gyro_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->gyro_input_dev, data);

	data->uncalib_gyro_input_dev = input_allocate_device();
	if (data->uncalib_gyro_input_dev == NULL)
		goto err_initialize_input_dev;

	data->uncalib_gyro_input_dev->name = "uncal_gyro_sensor";
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncalib_gyro_input_dev, EV_REL, REL_WHEEL);
	iRet = input_register_device(data->uncalib_gyro_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncalib_gyro_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->uncalib_gyro_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
	data->mag_input_dev = input_allocate_device();
	if (data->mag_input_dev == NULL)
		goto err_initialize_input_dev;

	data->mag_input_dev->name = "geomagnetic_sensor";
#ifdef SAVE_MAG_LOG
	input_set_capability(data->mag_input_dev, EV_REL, REL_X);
	input_set_capability(data->mag_input_dev, EV_REL, REL_Y);
	input_set_capability(data->mag_input_dev, EV_REL, REL_Z);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->mag_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->mag_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->mag_input_dev, EV_REL, REL_WHEEL);
#else
	input_set_capability(data->mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->mag_input_dev, EV_REL, REL_HWHEEL);
#endif
	iRet = input_register_device(data->mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->mag_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->mag_input_dev, data);

	data->uncal_mag_input_dev = input_allocate_device();
	if (data->uncal_mag_input_dev == NULL)
		goto err_initialize_input_dev;

	data->uncal_mag_input_dev->name = "uncal_geomagnetic_sensor";
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RX);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RY);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->uncal_mag_input_dev, EV_REL, REL_WHEEL);

	iRet = input_register_device(data->uncal_mag_input_dev);
	if (iRet < 0) {
		input_free_device(data->uncal_mag_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->uncal_mag_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	data->pressure_input_dev = input_allocate_device();
	if (data->pressure_input_dev == NULL)
		goto err_initialize_input_dev;

	data->pressure_input_dev->name = "pressure_sensor";
	input_set_capability(data->pressure_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->pressure_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->pressure_input_dev, EV_REL, REL_WHEEL);

	/* for timestemp reporting */
	input_set_capability(data->pressure_input_dev, EV_REL, (REL_WHEEL + 1));
	input_set_capability(data->pressure_input_dev, EV_REL, (REL_WHEEL + 2));
	input_set_capability(data->pressure_input_dev, EV_REL, (REL_WHEEL + 3));
	input_set_capability(data->pressure_input_dev, EV_REL, (REL_WHEEL + 4));

	/* for considering batching condtiion */
	input_set_events_per_packet(data->pressure_input_dev, 512);

	iRet = input_register_device(data->pressure_input_dev);
	if (iRet < 0) {
		input_free_device(data->pressure_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->pressure_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
	data->light_input_dev = input_allocate_device();
	if (data->light_input_dev == NULL)
		goto err_initialize_input_dev;

	data->light_input_dev->name = "light_sensor";
#if defined(CONFIG_SENSORS_SSP_TSL2584)
	input_set_capability(data->light_input_dev, EV_REL, REL_RX);

	/* for timestemp reporting */
	input_set_capability(data->light_input_dev, EV_REL, (REL_RX + 1));
	input_set_capability(data->light_input_dev, EV_REL, (REL_RX + 2));
	input_set_capability(data->light_input_dev, EV_REL, (REL_RX + 3));
	input_set_capability(data->light_input_dev, EV_REL, (REL_RX + 4));
#elif defined(CONFIG_SENSORS_SSP_CM3323)
	input_set_capability(data->light_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->light_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->light_input_dev, EV_REL, REL_WHEEL);
	input_set_capability(data->light_input_dev, EV_REL, REL_MISC);

	/* for timestemp reporting */
	input_set_capability(data->light_input_dev, EV_REL, (REL_MISC + 1));
	input_set_capability(data->light_input_dev, EV_REL, (REL_MISC + 2));
	input_set_capability(data->light_input_dev, EV_REL, (REL_MISC + 3));
	input_set_capability(data->light_input_dev, EV_REL, (REL_MISC + 4));
#endif

	iRet = input_register_device(data->light_input_dev);
	if (iRet < 0) {
		input_free_device(data->light_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->light_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
	data->temp_humi_input_dev = input_allocate_device();
	if (data->temp_humi_input_dev == NULL)
		goto err_initialize_input_dev;

	data->temp_humi_input_dev->name = "temp_humidity_sensor";
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_WHEEL);
	input_set_capability(data->temp_humi_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->temp_humi_input_dev);
	if (iRet < 0) {
		input_free_device(data->temp_humi_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->temp_humi_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
	data->rot_input_dev = input_allocate_device();
	if (data->rot_input_dev == NULL)
		goto err_initialize_input_dev;

	data->rot_input_dev->name = "rot_sensor";
	input_set_capability(data->rot_input_dev, EV_REL, REL_X);
	input_set_capability(data->rot_input_dev, EV_REL, REL_Y);
	input_set_capability(data->rot_input_dev, EV_REL, REL_Z);
	input_set_capability(data->rot_input_dev, EV_REL, REL_RX);
	input_set_capability(data->rot_input_dev, EV_REL, REL_RY);

	iRet = input_register_device(data->rot_input_dev);
	if (iRet < 0) {
		input_free_device(data->rot_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->rot_input_dev, data);

	data->game_rot_input_dev = input_allocate_device();
	if (data->game_rot_input_dev == NULL)
		goto err_initialize_input_dev;

	data->game_rot_input_dev->name = "game_rot_sensor";
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_X);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_Y);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_Z);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_RX);
	input_set_capability(data->game_rot_input_dev, EV_REL, REL_RY);

	iRet = input_register_device(data->game_rot_input_dev);
	if (iRet < 0) {
		input_free_device(data->game_rot_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->game_rot_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_LPM_MOTION
	data->motion_input_dev = input_allocate_device();
	if (data->motion_input_dev == NULL)
		goto err_initialize_input_dev;

	data->motion_input_dev->name = "LPM_MOTION";
	input_set_capability(data->motion_input_dev, EV_KEY, KEY_HOMEPAGE);
	input_set_capability(data->motion_input_dev, EV_ABS, ABS_X);
	input_set_abs_params(data->motion_input_dev, ABS_X, 0, 3, 0, 0);

	iRet = input_register_device(data->motion_input_dev);
	if (iRet < 0) {
		input_free_device(data->motion_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->motion_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	data->hrm_raw_input_dev = input_allocate_device();
	if (data->hrm_raw_input_dev == NULL)
		goto err_initialize_input_dev;

	data->hrm_raw_input_dev->name = "hrm_raw_sensor";
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_Z);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_RX);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_RY);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_RZ);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_WHEEL);
	input_set_capability(data->hrm_raw_input_dev, EV_REL, REL_MISC);

	/* for timestemp reporting */
	input_set_capability(data->hrm_raw_input_dev, EV_REL, (REL_MISC + 1));
	input_set_capability(data->hrm_raw_input_dev, EV_REL, (REL_MISC + 2));
	input_set_capability(data->hrm_raw_input_dev, EV_REL, (REL_MISC + 3));
	input_set_capability(data->hrm_raw_input_dev, EV_REL, (REL_MISC + 4));

	iRet = input_register_device(data->hrm_raw_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_raw_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->hrm_raw_input_dev, data);

	data->hrm_lib_input_dev = input_allocate_device();
	if (data->hrm_lib_input_dev == NULL)
		goto err_initialize_input_dev;

	data->hrm_lib_input_dev->name = "hrm_lib_sensor";
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_X);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Y);
	input_set_capability(data->hrm_lib_input_dev, EV_REL, REL_Z);

	/* for timestemp reporting */
	input_set_capability(data->hrm_lib_input_dev, EV_REL, (REL_Z + 1));
	input_set_capability(data->hrm_lib_input_dev, EV_REL, (REL_Z + 2));
	input_set_capability(data->hrm_lib_input_dev, EV_REL, (REL_Z + 3));
	input_set_capability(data->hrm_lib_input_dev, EV_REL, (REL_Z + 4));

	iRet = input_register_device(data->hrm_lib_input_dev);
	if (iRet < 0) {
		input_free_device(data->hrm_lib_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->hrm_lib_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
	data->front_hrm_raw_input_dev = input_allocate_device();
	if (data->front_hrm_raw_input_dev == NULL)
		goto err_initialize_input_dev;

	data->front_hrm_raw_input_dev->name = "front_hrm_raw_sensor";
	input_set_capability(data->front_hrm_raw_input_dev, EV_REL, REL_X);
	input_set_capability(data->front_hrm_raw_input_dev, EV_REL, REL_Y);

	/* for timestemp reporting */
	input_set_capability(data->front_hrm_raw_input_dev, EV_REL, (REL_Y + 1));
	input_set_capability(data->front_hrm_raw_input_dev, EV_REL, (REL_Y + 2));
	input_set_capability(data->front_hrm_raw_input_dev, EV_REL, (REL_Y + 3));
	input_set_capability(data->front_hrm_raw_input_dev, EV_REL, (REL_Y + 4));

	iRet = input_register_device(data->front_hrm_raw_input_dev);
	if (iRet < 0) {
		input_free_device(data->front_hrm_raw_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->front_hrm_raw_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
		data->uv_input_dev = input_allocate_device();
		if (data->uv_input_dev == NULL)
			goto err_initialize_input_dev;

		data->uv_input_dev->name = "uv_sensor";
		input_set_capability(data->uv_input_dev, EV_REL, REL_X);
		input_set_capability(data->uv_input_dev, EV_REL, REL_Y);

		iRet = input_register_device(data->uv_input_dev);
		if (iRet < 0) {
			input_free_device(data->uv_input_dev);
			goto err_initialize_input_dev;
		}
		input_set_drvdata(data->uv_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
	data->gsr_input_dev = input_allocate_device();
	if (data->gsr_input_dev == NULL)
		goto err_initialize_input_dev;

	data->gsr_input_dev->name = "gsr_sensor";
	input_set_capability(data->gsr_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->gsr_input_dev, EV_REL, REL_DIAL);
	input_set_capability(data->gsr_input_dev, EV_REL, REL_MISC);

	iRet = input_register_device(data->gsr_input_dev);
	if (iRet < 0) {
		input_free_device(data->gsr_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->gsr_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
	data->ecg_input_dev = input_allocate_device();
	if (data->ecg_input_dev == NULL)
		goto err_initialize_input_dev;

	data->ecg_input_dev->name = "ecg_sensor";
	input_set_capability(data->ecg_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(data->ecg_input_dev, EV_REL, REL_DIAL);

	iRet = input_register_device(data->ecg_input_dev);
	if (iRet < 0) {
		input_free_device(data->ecg_input_dev);
		goto err_initialize_input_dev;
	}
	input_set_drvdata(data->ecg_input_dev, data);
#endif

#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
		data->grip_input_dev = input_allocate_device();
		if (data->grip_input_dev == NULL)
			goto err_initialize_input_dev;

		data->grip_input_dev->name = "grip_sensor";
		input_set_capability(data->grip_input_dev, EV_REL, REL_X);
		input_set_capability(data->grip_input_dev, EV_REL, REL_Y);
		input_set_capability(data->grip_input_dev, EV_REL, REL_Z);
		input_set_capability(data->grip_input_dev, EV_REL, REL_RX);

		iRet = input_register_device(data->grip_input_dev);
		if (iRet < 0) {
			input_free_device(data->grip_input_dev);
			goto err_initialize_input_dev;
		}
		input_set_drvdata(data->grip_input_dev, data);
#endif

	return SUCCESS;

err_initialize_input_dev:
	remove_input_dev(data);

	return ERROR;
}
