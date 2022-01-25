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
#include <linux/math64.h>
#include <linux/ioctl.h>
#include <linux/compat.h>

#define BATCH_IOCTL_MAGIC		0xFC

struct batch_config {
	int64_t timeout;
	int64_t delay;
	int flag;
};

/*************************************************************************/
/* SSP data delay function                                              */
/*************************************************************************/

int get_msdelay(int64_t dDelayRate)
{
	return div_s64(dDelayRate, 1000000);
}

static void enable_sensor(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	u64 uNewEnable = 0;
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);
	int ret = 0;

	data->adDelayBuf[iSensorType] = dNewDelay;
	maxBatchReportLatency = data->batchLatencyBuf[iSensorType];
	batchOptions = data->batchOptBuf[iSensorType];

	switch (data->aiCheckStatus[iSensorType]) {
	case ADD_SENSOR_STATE:
		ssp_dbg("[SSP]: %s - add %u, New = %lldns\n",
			 __func__, 1 << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;

		ret = send_instruction(data, ADD_SENSOR,
				iSensorType, uBuf, 9);
		pr_info("[SSP], delay %d, timeout %d, flag=%d, ret%d",
			dMsDelay, maxBatchReportLatency, uBuf[8], ret);
		if (ret <= 0) {
			uNewEnable =
				(u64)atomic64_read(&data->aSensorEnable)
				& (~(u64)(1 << iSensorType));
			atomic64_set(&data->aSensorEnable, uNewEnable);

			data->aiCheckStatus[iSensorType] = NO_SENSOR_STATE;
			break;
		}

		data->aiCheckStatus[iSensorType] = RUNNING_SENSOR_STATE;

		break;
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %u, New = %lldns\n",
			__func__, 1 << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		data->aiCheckStatus[iSensorType] = ADD_SENSOR_STATE;
	}
}

static void change_sensor_delay(struct ssp_data *data,
	int iSensorType, int64_t dNewDelay)
{
	u8 uBuf[9];
	s32 maxBatchReportLatency = 0;
	s8 batchOptions = 0;
	int64_t dTempDelay = data->adDelayBuf[iSensorType];
	s32 dMsDelay = get_msdelay(dNewDelay);

	data->adDelayBuf[iSensorType] = dNewDelay;
	data->batchLatencyBuf[iSensorType] = maxBatchReportLatency;
	data->batchOptBuf[iSensorType] = batchOptions;

	switch (data->aiCheckStatus[iSensorType]) {
	case RUNNING_SENSOR_STATE:
		if (get_msdelay(dTempDelay)
			== get_msdelay(data->adDelayBuf[iSensorType]))
			break;

		ssp_dbg("[SSP]: %s - Change %u, New = %lldns\n",
			__func__, 1 << iSensorType, dNewDelay);

		memcpy(&uBuf[0], &dMsDelay, 4);
		memcpy(&uBuf[4], &maxBatchReportLatency, 4);
		uBuf[8] = batchOptions;
		send_instruction(data, CHANGE_DELAY, iSensorType, uBuf, 9);

		break;
	default:
		break;
	}
}

/*************************************************************************/
/* SSP data enable function                                              */
/*************************************************************************/

static int ssp_remove_sensor(struct ssp_data *data,
	unsigned int uChangedSensor, u64 uNewEnable)
{
	u8 uBuf[4];
	int64_t dSensorDelay = data->adDelayBuf[uChangedSensor];

	ssp_dbg("[SSP]: %s - remove sensor = %lld, current state = %lld\n",
		__func__, (u64)(1 << uChangedSensor), uNewEnable);

	data->batchLatencyBuf[uChangedSensor] = 0;
	data->batchOptBuf[uChangedSensor] = 0;

	if (uChangedSensor == ORIENTATION_SENSOR) {
		if (!(atomic64_read(&data->aSensorEnable)
			& (1 << ACCELEROMETER_SENSOR))) {
			uChangedSensor = ACCELEROMETER_SENSOR;
		} else {
			change_sensor_delay(data, ACCELEROMETER_SENSOR,
				data->adDelayBuf[ACCELEROMETER_SENSOR]);
			return 0;
		}
	} else if (uChangedSensor == ACCELEROMETER_SENSOR) {
		if (atomic64_read(&data->aSensorEnable)
			& (1 << ORIENTATION_SENSOR)) {
			change_sensor_delay(data, ORIENTATION_SENSOR,
				data->adDelayBuf[ORIENTATION_SENSOR]);
			return 0;
		}
	}

	if (!data->bSspShutdown)
		if (atomic64_read(&data->aSensorEnable) & (1 << uChangedSensor)) {
			s32 dMsDelay = get_msdelay(dSensorDelay);
			memcpy(&uBuf[0], &dMsDelay, 4);

			send_instruction(data, REMOVE_SENSOR,
				uChangedSensor, uBuf, 4);
		}
	data->aiCheckStatus[uChangedSensor] = NO_SENSOR_STATE;

	return 0;
}

/*************************************************************************/
/* ssp Sysfs                                                             */
/*************************************************************************/

static ssize_t show_enable_irq(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - %d\n", __func__, !data->bSspShutdown);

	return snprintf(buf, PAGE_SIZE, "%d\n", !data->bSspShutdown);
}

static ssize_t set_enable_irq(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u8 dTemp;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtou8(buf, 10, &dTemp) < 0)
		return -1;

	pr_info("[SSP] %s - %d start\n", __func__, dTemp);
	if (dTemp) {
		reset_mcu(data);
		enable_debug_timer(data);
	} else if (!dTemp) {
		disable_debug_timer(data);
		ssp_enable(data, 0);
	} else
		pr_err("[SSP] %s - invalid value\n", __func__);
	pr_info("[SSP] %s - %d end\n", __func__, dTemp);
	return size;
}

static ssize_t show_sensors_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	ssp_dbg("[SSP]: %s - cur_enable = %llu\n", __func__,
		(u64)(atomic64_read(&data->aSensorEnable)));

	return snprintf(buf, PAGE_SIZE, "%llu\n",
		(u64)(atomic64_read(&data->aSensorEnable)));
}

static ssize_t set_sensors_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	u64 uNewEnable = 0;
	unsigned int uChangedSensor = 0;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	uNewEnable = (u64)dTemp;
	ssp_dbg("[SSP]: %s - new_enable = %llu, old_enable = %lu\n", __func__,
		 uNewEnable, atomic64_read(&data->aSensorEnable));

	if ((uNewEnable != atomic64_read(&data->aSensorEnable)) &&
		!(data->uSensorState &
		(uNewEnable - atomic64_read(&data->aSensorEnable)))) {
		pr_info("[SSP] %s - %llu is not connected(sensor state: 0x%llx)\n",
			__func__,
			uNewEnable - atomic64_read(&data->aSensorEnable),
			data->uSensorState);
		return -EINVAL;
	}

	if (uNewEnable == atomic64_read(&data->aSensorEnable))
		return size;

	for (uChangedSensor = 0; uChangedSensor < SENSOR_MAX;
		uChangedSensor++) {
		if ((atomic64_read(&data->aSensorEnable) & (1 << uChangedSensor))
		!= (uNewEnable & (1 << uChangedSensor))) {

			if (!(uNewEnable & (1 << uChangedSensor))) {
				data->reportedData[uChangedSensor] = false;

				ssp_remove_sensor(data, uChangedSensor,
					uNewEnable); /* disable */
			} else { /* Change to ADD_SENSOR_STATE from KitKat */
				data->aiCheckStatus[uChangedSensor]
					= ADD_SENSOR_STATE;

				enable_sensor(data, uChangedSensor,
					data->adDelayBuf[uChangedSensor]);
			}
		}
	}
	atomic64_set(&data->aSensorEnable, uNewEnable);

	return size;
}

static ssize_t set_cal_data(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dTemp;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	if (unlikely(data->bSspShutdown)) {
		pr_err("[SSP]%s, stop sending cal data(shutdown)", __func__);
		return -EBUSY;
	}

	if (dTemp == 1) {
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
		accel_open_calibration(data);
		set_accel_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
		gyro_open_calibration(data);
		set_gyro_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
		pressure_open_calibration(data);
		set_pressure_cal(data);
#endif

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
		hrm_open_calibration(data);
		set_hrm_calibration(data);
#endif
	}

	return size;
}

static ssize_t show_acc_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ACCELEROMETER_SENSOR]);
}

static ssize_t set_acc_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	if ((atomic64_read(&data->aSensorEnable) & (1 << ORIENTATION_SENSOR)) &&
		(data->adDelayBuf[ORIENTATION_SENSOR] < dNewDelay))
		data->adDelayBuf[ACCELEROMETER_SENSOR] = dNewDelay;
	else
		change_sensor_delay(data, ACCELEROMETER_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_gyro_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GYROSCOPE_SENSOR]);
}

static ssize_t set_gyro_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GYROSCOPE_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_uncalib_gyro_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GYRO_UNCALIB_SENSOR]);
}

static ssize_t set_uncalib_gyro_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GYRO_UNCALIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_mag_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_SENSOR]);
}

static ssize_t set_mag_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_uncal_mag_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GEOMAGNETIC_UNCALIB_SENSOR]);
}

static ssize_t set_uncal_mag_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GEOMAGNETIC_UNCALIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_pressure_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[PRESSURE_SENSOR]);
}

static ssize_t set_pressure_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, PRESSURE_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_light_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[LIGHT_SENSOR]);
}

static ssize_t set_light_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, LIGHT_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_temp_humi_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data  = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[TEMPERATURE_HUMIDITY_SENSOR]);
}

static ssize_t set_temp_humi_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data  = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, TEMPERATURE_HUMIDITY_SENSOR, dNewDelay);
	return size;
}

static ssize_t show_rot_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[ROTATION_VECTOR]);
}

static ssize_t set_rot_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, ROTATION_VECTOR, dNewDelay);

	return size;
}

static ssize_t show_game_rot_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[GAME_ROTATION_VECTOR]);
}

static ssize_t set_game_rot_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, GAME_ROTATION_VECTOR, dNewDelay);

	return size;
}

static ssize_t show_hrm_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_RAW_SENSOR]);
}

static ssize_t set_hrm_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_RAW_SENSOR, dNewDelay);
	change_sensor_delay(data, HRM_RAW_FAC_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_hrm_lib_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[HRM_LIB_SENSOR]);
}

static ssize_t set_hrm_lib_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, HRM_LIB_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_front_hrm_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[FRONT_HRM_RAW_SENSOR]);
}

static ssize_t set_front_hrm_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, FRONT_HRM_RAW_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_uv_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		data->adDelayBuf[UV_SENSOR]);
}

static ssize_t set_uv_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *data = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(data, UV_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_gsr_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", sdev->adDelayBuf[GSR_SENSOR]);
}

static ssize_t set_gsr_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, GSR_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_ecg_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", sdev->adDelayBuf[ECG_SENSOR]);
}

static ssize_t set_ecg_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, ECG_SENSOR, dNewDelay);

	return size;
}

static ssize_t show_grip_delay(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *sdev = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n",
		sdev->adDelayBuf[GRIP_SENSOR]);
}

static ssize_t set_grip_delay(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int64_t dNewDelay;
	struct ssp_data *sdev = dev_get_drvdata(dev);

	if (kstrtoll(buf, 10, &dNewDelay) < 0)
		return -EINVAL;

	change_sensor_delay(sdev, GRIP_SENSOR, dNewDelay);

	return size;
}

static DEVICE_ATTR(mcu_rev, S_IRUGO, mcu_revision_show, NULL);
static DEVICE_ATTR(mcu_name, S_IRUGO, mcu_model_name_show, NULL);
static DEVICE_ATTR(mcu_update, S_IRUGO, mcu_update_kernel_bin_show, NULL);
static DEVICE_ATTR(mcu_update2, S_IRUGO,
	mcu_update_kernel_crashed_bin_show, NULL);
static DEVICE_ATTR(mcu_update_ums, S_IRUGO, mcu_update_ums_bin_show, NULL);
static DEVICE_ATTR(mcu_reset, S_IRUGO, mcu_reset_show, NULL);
static DEVICE_ATTR(mcu_ready, S_IRUGO, mcu_ready_show, NULL);
static DEVICE_ATTR(mcu_dump, S_IRUGO, mcu_dump_show, NULL);
static DEVICE_ATTR(mcu_sensorstate, S_IRUGO, mcu_sensor_state, NULL);
static DEVICE_ATTR(mcu_test, S_IRUGO | S_IWUSR | S_IWGRP,
	mcu_factorytest_show, mcu_factorytest_store);
static DEVICE_ATTR(mcu_sleep_test, S_IRUGO | S_IWUSR | S_IWGRP,
	mcu_sleep_factorytest_show, mcu_sleep_factorytest_store);
static DEVICE_ATTR(fota_rotate_info, S_IRUGO, mcu_fota_rotate_status_show, NULL);


static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	show_sensors_enable, set_sensors_enable);
static DEVICE_ATTR(enable_irq, S_IRUGO | S_IWUSR | S_IWGRP,
	show_enable_irq, set_enable_irq);
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
static DEVICE_ATTR(accel_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_acc_delay, set_acc_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
static DEVICE_ATTR(gyro_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gyro_delay, set_gyro_delay);
static DEVICE_ATTR(uncal_gyro_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncalib_gyro_delay, set_uncalib_gyro_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
static DEVICE_ATTR(mag_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_delay, set_mag_delay);
static DEVICE_ATTR(uncal_mag_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncal_mag_delay, set_uncal_mag_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
static DEVICE_ATTR(pressure_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_pressure_delay, set_pressure_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
static DEVICE_ATTR(light_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_light_delay, set_light_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
static DEVICE_ATTR(temp_humid_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_temp_humi_delay, set_temp_humi_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
static DEVICE_ATTR(rot_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_rot_delay, set_rot_delay);
static DEVICE_ATTR(game_rot_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_game_rot_delay, set_game_rot_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
static DEVICE_ATTR(hrm_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_delay, set_hrm_delay);
static DEVICE_ATTR(hrm_lib_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_lib_delay, set_hrm_lib_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
static DEVICE_ATTR(front_hrm_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_front_hrm_delay, set_front_hrm_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
static DEVICE_ATTR(uv_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uv_delay, set_uv_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
static DEVICE_ATTR(gsr_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gsr_delay, set_gsr_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
static DEVICE_ATTR(ecg_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_ecg_delay, set_ecg_delay);
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
static DEVICE_ATTR(grip_poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_grip_delay, set_grip_delay);
#endif

static DEVICE_ATTR(set_cal_data, S_IWUSR | S_IWGRP,
	NULL, set_cal_data);

static struct device_attribute *mcu_attrs[] = {
	&dev_attr_enable,
	&dev_attr_mcu_rev,
	&dev_attr_mcu_name,
	&dev_attr_mcu_test,
	&dev_attr_mcu_reset,
	&dev_attr_mcu_ready,
	&dev_attr_mcu_sensorstate,
	&dev_attr_mcu_dump,
	&dev_attr_mcu_update,
	&dev_attr_mcu_update2,
	&dev_attr_mcu_update_ums,
	&dev_attr_mcu_sleep_test,
	&dev_attr_fota_rotate_info,
	&dev_attr_enable_irq,
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	&dev_attr_accel_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	&dev_attr_gyro_poll_delay,
	&dev_attr_uncal_gyro_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
	&dev_attr_mag_poll_delay,
	&dev_attr_uncal_mag_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	&dev_attr_pressure_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
	&dev_attr_light_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
	&dev_attr_temp_humid_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
	&dev_attr_rot_poll_delay,
	&dev_attr_game_rot_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	&dev_attr_hrm_poll_delay,
	&dev_attr_hrm_lib_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
	&dev_attr_front_hrm_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
	&dev_attr_uv_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
	&dev_attr_gsr_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
	&dev_attr_ecg_poll_delay,
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
	&dev_attr_grip_poll_delay,
#endif
	&dev_attr_set_cal_data,
	NULL,
};

static struct device_attribute dev_attr_input_accel_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_acc_delay, set_acc_delay);
static struct device_attribute dev_attr_input_gyro_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gyro_delay, set_gyro_delay);
static struct device_attribute dev_attr_input_uncalib_gyro_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncalib_gyro_delay, set_uncalib_gyro_delay);
static struct device_attribute dev_attr_input_mag_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_mag_delay, set_mag_delay);
static struct device_attribute dev_attr_input_uncal_mag_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uncal_mag_delay, set_uncal_mag_delay);
static struct device_attribute dev_attr_input_pressure_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_pressure_delay, set_pressure_delay);
static struct device_attribute dev_attr_input_light_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_light_delay, set_light_delay);
static struct device_attribute dev_attr_input_temp_humi_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_temp_humi_delay, set_temp_humi_delay);
static struct device_attribute dev_attr_input_rot_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_rot_delay, set_rot_delay);
static struct device_attribute dev_attr_input_game_rot_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_game_rot_delay, set_game_rot_delay);
static struct device_attribute dev_attr_input_hrm_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_delay, set_hrm_delay);
static struct device_attribute dev_attr_input_hrm_lib_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_hrm_lib_delay, set_hrm_lib_delay);
static struct device_attribute dev_attr_input_front_hrm_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_front_hrm_delay, set_front_hrm_delay);
static struct device_attribute dev_attr_input_uv_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_uv_delay, set_uv_delay);
static struct device_attribute dev_attr_input_gsr_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_gsr_delay, set_gsr_delay);
static struct device_attribute dev_attr_input_ecg_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_ecg_delay, set_ecg_delay);
static struct device_attribute dev_attr_input_grip_poll_delay
	= __ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
	show_grip_delay, set_grip_delay);

static void initialize_mcu_factorytest(struct ssp_data *data)
{
	sensors_register(data->mcu_device, data, mcu_attrs, "ssp_sensor");
}

static void remove_mcu_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->mcu_device, mcu_attrs);
}

static void remove_input_poll_delay_sysfs(struct ssp_data *data)
{
	if (data->grip_input_dev)
		device_remove_file(&data->grip_input_dev->dev,
			&dev_attr_input_grip_poll_delay);
	if (data->ecg_input_dev)
		device_remove_file(&data->ecg_input_dev->dev,
			&dev_attr_input_ecg_poll_delay);
	if (data->gsr_input_dev)
		device_remove_file(&data->gsr_input_dev->dev,
			&dev_attr_input_gsr_poll_delay);
	if (data->uv_input_dev)
		device_remove_file(&data->uv_input_dev->dev,
			&dev_attr_input_uv_poll_delay);
	if (data->front_hrm_raw_input_dev)
		device_remove_file(&data->front_hrm_raw_input_dev->dev,
			&dev_attr_input_front_hrm_poll_delay);
	if (data->hrm_raw_input_dev)
		device_remove_file(&data->hrm_raw_input_dev->dev,
			&dev_attr_input_hrm_poll_delay);
	if (data->hrm_lib_input_dev)
		device_remove_file(&data->hrm_lib_input_dev->dev,
			&dev_attr_input_hrm_lib_poll_delay);
	if (data->game_rot_input_dev)
		device_remove_file(&data->game_rot_input_dev->dev,
			&dev_attr_input_game_rot_poll_delay);
	if (data->rot_input_dev)
		device_remove_file(&data->rot_input_dev->dev,
			&dev_attr_input_game_rot_poll_delay);
	if (data->temp_humi_input_dev)
		device_remove_file(&data->temp_humi_input_dev->dev,
			&dev_attr_input_temp_humi_poll_delay);
	if (data->light_input_dev)
		device_remove_file(&data->light_input_dev->dev,
			&dev_attr_input_light_poll_delay);
	if (data->pressure_input_dev)
		device_remove_file(&data->pressure_input_dev->dev,
			&dev_attr_input_pressure_poll_delay);
	if (data->uncalib_gyro_input_dev)
		device_remove_file(&data->uncalib_gyro_input_dev->dev,
			&dev_attr_input_uncalib_gyro_poll_delay);
	if (data->uncal_mag_input_dev)
		device_remove_file(&data->uncal_mag_input_dev->dev,
			&dev_attr_input_uncal_mag_poll_delay);
	if (data->mag_input_dev)
		device_remove_file(&data->mag_input_dev->dev,
			&dev_attr_input_mag_poll_delay);
	if (data->gyro_input_dev)
		device_remove_file(&data->gyro_input_dev->dev,
			&dev_attr_input_gyro_poll_delay);
	if (data->acc_input_dev)
		device_remove_file(&data->acc_input_dev->dev,
			&dev_attr_input_accel_poll_delay);
}

static int initialize_input_poll_delay_sysfs(struct ssp_data *data)
{
	int iRet;

	if (data->acc_input_dev) {
		iRet = device_create_file(&data->acc_input_dev->dev,
					&dev_attr_input_accel_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->gyro_input_dev) {
		iRet = device_create_file(&data->gyro_input_dev->dev,
					&dev_attr_input_gyro_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->mag_input_dev) {
		iRet = device_create_file(&data->mag_input_dev->dev,
					&dev_attr_input_mag_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->uncal_mag_input_dev) {
		iRet = device_create_file(&data->uncal_mag_input_dev->dev,
					&dev_attr_input_uncal_mag_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->uncalib_gyro_input_dev) {
		iRet = device_create_file(&data->uncalib_gyro_input_dev->dev,
				&dev_attr_input_uncalib_gyro_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->pressure_input_dev) {
		iRet = device_create_file(&data->pressure_input_dev->dev,
					&dev_attr_input_pressure_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->light_input_dev) {
		iRet = device_create_file(&data->light_input_dev->dev,
				&dev_attr_input_light_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->temp_humi_input_dev) {
		iRet = device_create_file(&data->temp_humi_input_dev->dev,
				&dev_attr_input_temp_humi_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->rot_input_dev) {
		iRet = device_create_file(&data->rot_input_dev->dev,
			&dev_attr_input_rot_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->game_rot_input_dev) {
		iRet = device_create_file(&data->game_rot_input_dev->dev,
			&dev_attr_input_game_rot_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->hrm_raw_input_dev) {
		iRet = device_create_file(&data->hrm_raw_input_dev->dev,
					&dev_attr_input_hrm_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->hrm_lib_input_dev) {
		iRet = device_create_file(&data->hrm_lib_input_dev->dev,
					&dev_attr_input_hrm_lib_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->front_hrm_raw_input_dev) {
		iRet = device_create_file(&data->front_hrm_raw_input_dev->dev,
					&dev_attr_input_front_hrm_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->uv_input_dev) {
		iRet = device_create_file(&data->uv_input_dev->dev,
					&dev_attr_input_uv_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->gsr_input_dev) {
		iRet = device_create_file(&data->gsr_input_dev->dev,
				&dev_attr_input_gsr_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->ecg_input_dev) {
		iRet = device_create_file(&data->ecg_input_dev->dev,
				&dev_attr_input_ecg_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	if (data->grip_input_dev) {
		iRet = device_create_file(&data->grip_input_dev->dev,
				&dev_attr_input_grip_poll_delay);
		if (iRet < 0)
			goto err_create_poll_delay;
	}

	return SUCCESS;

err_create_poll_delay:
	remove_input_poll_delay_sysfs(data);

	return ERROR;
}

static long ssp_batch_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct ssp_data *data
		= container_of(file->private_data,
			struct ssp_data, batch_io_device);

	struct batch_config batch;

	void __user *argp = (void __user *)arg;
	int retries = 2;
	int ret = 0;
	int sensor_type, ms_delay;
	int timeout_ms = 0;
	u8 uBuf[9];

	sensor_type = (cmd & 0xFF);

	if ((cmd >> 8 & 0xFF) != BATCH_IOCTL_MAGIC) {
		pr_err("[SSP] Invalid BATCH CMD %x\n", cmd);
		return -EINVAL;
	}

	while (retries--) {
		ret = copy_from_user(&batch, argp, sizeof(batch));
		if (likely(!ret))
			break;
	}
	if (unlikely(ret)) {
		pr_err("[SSP] batch ioctl err(%d)\n", ret);
		return -EINVAL;
	}

	pr_info("[SSP] batch ioctl [arg] delay:%lld, timeout:%lld, flag:0x%0x\n",
		batch.delay, batch.timeout, batch.flag);

	ms_delay = get_msdelay(batch.delay);
	timeout_ms = div_s64(batch.timeout, 1000000);
	memcpy(&uBuf[0], &ms_delay, 4);
	memcpy(&uBuf[4], &timeout_ms, 4);
	uBuf[8] = batch.flag;

	/* For arg input checking */
	if ((batch.timeout < 0) || (batch.delay <= 0)) {
		pr_err("[SSP] Invalid batch values!!\n");
		return -EINVAL;
	} else if ((batch.timeout != 0) && (timeout_ms == 0)) {
		pr_err("[SSP] Invalid batch timeout range!!\n");
		return -EINVAL;
	} else if ((batch.delay != 0) && (ms_delay == 0)) {
		pr_err("[SSP] Invalid batch delay range!!\n");
		return -EINVAL;
	}

	if (batch.timeout) { /* add or dry */

		if(!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* real batch, NOT DRY, change delay */
			ret = 1;
			/* if sensor is not running state, enable will be called.
			   MCU return fail when receive chage delay inst during NO_SENSOR STATE */
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE) {
				ret = send_instruction_sync(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			}
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = batch.delay;
			}
		} else { /* real batch, DRY RUN */
			ret = send_instruction_sync(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			if (ret > 0) { // ret 1 is success
				data->batchOptBuf[sensor_type] = (u8)batch.flag;
				data->batchLatencyBuf[sensor_type] = timeout_ms;
				data->adDelayBuf[sensor_type] = batch.delay;
			}
		}
	} else { /* remove batch or normal change delay, remove or add will be called. */

		if (!(batch.flag & SENSORS_BATCH_DRY_RUN)) { /* no batch, NOT DRY, change delay */
			data->batchOptBuf[sensor_type] = 0;
			data->batchLatencyBuf[sensor_type] = 0;
			data->adDelayBuf[sensor_type] = batch.delay;
			if (data->aiCheckStatus[sensor_type] == RUNNING_SENSOR_STATE) {
				send_instruction(data, CHANGE_DELAY, sensor_type, uBuf, 9);
			}
		}
	}

	pr_info("[SSP] batch %d: delay %lld(ms_delay:%d), timeout %lld(timeout_ms:%d), flag %d, ret %d \n",
		sensor_type, batch.delay, ms_delay, batch.timeout, timeout_ms, batch.flag, ret);
	if (!batch.timeout)
		return 0;
	if (ret <= 0)
		return -EINVAL;
	else
		return 0;
}

#ifdef CONFIG_COMPAT
static long ssp_batch_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return ssp_batch_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static struct file_operations ssp_batch_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.unlocked_ioctl = ssp_batch_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ssp_batch_compat_ioctl,
#endif
};

int initialize_sysfs(struct ssp_data *data)
{
	initialize_factorytest(data);

	initialize_mcu_factorytest(data);
	if (initialize_input_poll_delay_sysfs(data) != SUCCESS)
		goto err_init_input_sysfs;

	data->batch_io_device.minor = MISC_DYNAMIC_MINOR;
	data->batch_io_device.name = "batch_io";
	data->batch_io_device.fops = &ssp_batch_fops;
	if (misc_register(&data->batch_io_device))
		goto err_batch_io_dev;

	return SUCCESS;

err_batch_io_dev:
	remove_input_poll_delay_sysfs(data);
err_init_input_sysfs:
	remove_mcu_factorytest(data);
	remove_factorytest(data);

	return ERROR;
}

void remove_sysfs(struct ssp_data *data)
{
	misc_deregister(&data->batch_io_device);

	remove_input_poll_delay_sysfs(data);

	remove_factorytest(data);

	remove_mcu_factorytest(data);

	destroy_sensor_class();
}
