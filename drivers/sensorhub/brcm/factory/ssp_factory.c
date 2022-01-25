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
#include "ssp_factory.h"

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

int accel_open_calibration(struct ssp_data *data)
{
	int ret = 0;

	ret = lsm6dsl_accel_open_calibration(data);

	return ret;
}

int set_accel_cal(struct ssp_data *data)
{
	int ret = 0;

	ret = lsm6dsl_set_accel_cal(data);

	return ret;
}

int gyro_open_calibration(struct ssp_data *data)
{
	int ret = 0;

	ret = lsm6dsl_gyro_open_calibration(data);

	return ret;
}

int set_gyro_cal(struct ssp_data *data)
{
	int ret = 0;

	ret = lsm6dsl_set_gyro_cal(data);

	return ret;
}

int pressure_open_calibration(struct ssp_data *data)
{
	int ret = 0;

	ret = lps25h_pressure_open_calibration(data);

	return ret;
}

int set_pressure_cal(struct ssp_data *data)
{
	int ret = 0;

	ret = lps25h_set_pressure_cal(data);

	return ret;
}

int hrm_open_calibration(struct ssp_data *data)
{
	int ret = 0;

	ret = pps960_hrm_open_calibration(data);

	return ret;
}

int set_hrm_calibration(struct ssp_data *data)
{
	int ret = 0;

	ret = pps960_set_hrm_calibration(data);

	return ret;
}

int initialize_magnetic_sensor(struct ssp_data *data)
{
	int ret = SUCCESS;

	return ret;
}

void initialize_factorytest(struct ssp_data *data)
{
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	initialize_lsm6dsl_accel_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	initialize_lsm6dsl_gyro_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	initialize_lps25h_pressure_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
	initialize_tsl2584_light_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
	initialize_shtw1_skintemp_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	initialize_pps960_hrm_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
	initialize_max86902_hrm_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
	initialize_max86902_uv_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
	initialize_hm121_gsr_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
	initialize_ads1292_ecg_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
	initialize_sx9310_grip_factorytest(data);
#endif
}

void remove_factorytest(struct ssp_data *data)
{
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	remove_lsm6dsl_accel_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	remove_lsm6dsl_gyro_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	remove_lps25h_pressure_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
	remove_tsl2584_light_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
	remove_shtw1_skintemp_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	remove_pps960_hrm_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
	remove_max86902_hrm_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
	remove_max86902_uv_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
	remove_hm121_gsr_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
	remove_ads1292_ecg_factorytest(data);
#endif
#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
	remove_sx9310_grip_factorytest(data);
#endif
}
