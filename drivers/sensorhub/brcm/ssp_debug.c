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
#include <linux/fs.h>
#include <linux/vmalloc.h>
#if SSP_SEC_DEBUG
#include <linux/sec_debug.h>
#endif

#define SSP_DEBUG_TIMER_SEC	(10 * HZ)

#define LIMIT_RESET_CNT		20
#define LIMIT_TIMEOUT_CNT	3

#define DUMP_FILE_PATH "/opt/usr/log/MCU_DUMP"
#define DEBUG_DUMP_FILE_PATH	"/opt/usr/log/SensorHubDump"
#define DEBUG_DUMP_DATA_COMPLETE 0xDD

static mm_segment_t backup_fs;

int debug_crash_dump(struct ssp_data *data, char *pchRcvDataFrame, int iLength)
{
#if SSP_SEC_DEBUG
	struct timeval cur_time;
	char strFilePath[100];
	int iRetWrite = 0, iRet = 0;
	unsigned char datacount = pchRcvDataFrame[1];
	unsigned int databodysize = iLength - 2;
	char *databody = &pchRcvDataFrame[2];
/*
	if(iLength != DEBUG_DUMP_DATA_SIZE)
	{
		ssp_errf("data length error(%d)", iLength);
		return FAIL;
	}
	else
		ssp_errf("length(%d)", databodysize);
*/
	pr_info("[SSP]%s, length(%d)", __func__, databodysize);

	if (data->bMcuDumpMode == true)	{
		wake_lock(&data->ssp_wake_lock);

		if (data->realtime_dump_file == NULL) {
			backup_fs = get_fs();
			set_fs(get_ds());

			do_gettimeofday(&cur_time);

			snprintf(strFilePath, sizeof(strFilePath), "%s%d.dump",
				DEBUG_DUMP_FILE_PATH, (int)cur_time.tv_sec);
			data->realtime_dump_file = filp_open(strFilePath,
					O_RDWR | O_CREAT | O_APPEND, 0666);

			pr_err("[SSP]%s, save_crash_dump : open file(%s)", __func__, strFilePath);

			if (IS_ERR(data->realtime_dump_file)) {
				pr_err("[SSP]%s, Can't open dump file", __func__);
				set_fs(backup_fs);
				iRet = PTR_ERR(data->realtime_dump_file);
//				filp_close(data->realtime_dump_file,
//					current->files);
				data->realtime_dump_file = NULL;
				wake_unlock(&data->ssp_wake_lock);
				return FAIL;
			}
		}

		data->total_dump_size += databodysize;
		/* ssp_errf("total receive size(%d)", data->total_dump_size); */
		iRetWrite = vfs_write(data->realtime_dump_file,
					(char __user *)databody, databodysize,
					&data->realtime_dump_file->f_pos);
		if (iRetWrite < 0) {
			pr_err("[SSP]%s, Can't write dump to file", __func__);
			wake_unlock(&data->ssp_wake_lock);
			return FAIL;
		}

		if (datacount == DEBUG_DUMP_DATA_COMPLETE) {
			pr_err("[SSP]%s, close file(size=%d)", __func__, data->total_dump_size);
			filp_close(data->realtime_dump_file, current->files);
			set_fs(backup_fs);
			data->uDumpCnt++;
			data->total_dump_size = 0;
			data->realtime_dump_file = NULL;
			data->bDumping = false;
		}

		wake_unlock(&data->ssp_wake_lock);

		/*
		if(iLength == 2*1024)
			queue_refresh_task(data, 0);
		*/
	}
#endif
	return SUCCESS;
}

void ssp_dump_task(struct work_struct *work)
{
#if SSP_SEC_DEBUG
	struct ssp_big *big;
	struct file *dump_file;
	struct ssp_msg *msg;
	char *buffer;
	char strFilePath[100];
	struct timeval cur_time;
	mm_segment_t fs;
	int buf_len, packet_len, residue, iRet = 0;
	int index = 0, iRetTrans = 0, iRetWrite = 0;

	big = container_of(work, struct ssp_big, work);
	pr_err("[SSP]: %s - start ssp dumping (%d)(%d)\n", __func__,
		big->data->bMcuDumpMode, big->data->uDumpCnt);
	big->data->uDumpCnt++;
	wake_lock(&big->data->ssp_wake_lock);

	fs = get_fs();
	set_fs(get_ds());

	if (big->data->bMcuDumpMode == true) {
		do_gettimeofday(&cur_time);
#ifdef CONFIG_SENSORS_SSP_ENG
		snprintf(strFilePath, sizeof(strFilePath), "%s%d.dump",
			DUMP_FILE_PATH,	(int)cur_time.tv_sec);
		dump_file = filp_open(strFilePath,
				O_RDWR | O_CREAT | O_APPEND, 0666);
#else
		snprintf(strFilePath, sizeof(strFilePath), "%s.dump",
			DUMP_FILE_PATH);
		dump_file = filp_open(strFilePath,
				O_RDWR | O_CREAT | O_TRUNC, 0666);
#endif

		if (IS_ERR(dump_file)) {
			pr_err("[SSP]: %s - Can't open dump file\n", __func__);
			set_fs(fs);
			iRet = PTR_ERR(dump_file);
			wake_unlock(&big->data->ssp_wake_lock);
			kfree(big);
			return;
		}
	} else
		dump_file = NULL;

	buf_len = big->length > DATA_PACKET_SIZE ? DATA_PACKET_SIZE : big->length;
	buffer = kzalloc(buf_len, GFP_KERNEL);
	residue = big->length;

	while (residue > 0) {
		packet_len = residue > DATA_PACKET_SIZE ? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = packet_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		iRetTrans = ssp_spi_sync(big->data, msg, 1000);
		if (iRetTrans != SUCCESS) {
			pr_err("[SSP]: %s - Fail to receive data %d (%d)\n",
				__func__, iRetTrans,residue);
			break;
		}
		if (big->data->bMcuDumpMode == true) {
			iRetWrite = vfs_write(dump_file, (char __user *)buffer,
				packet_len, &dump_file->f_pos);
			if (iRetWrite < 0) {
				pr_err("[SSP]: %s - Can't write dump to file\n",
					__func__);
				break;
			}
		}
		residue -= packet_len;
	}


	if (big->data->bMcuDumpMode == true) {
		if (iRetTrans != SUCCESS || iRetWrite < 0) {	/* error case */
			char FAILSTRING[100];
			snprintf(FAILSTRING, sizeof(FAILSTRING),
				"FAIL OCCURED(%d)(%d)(%d)", iRetTrans,
				iRetWrite, big->length);
			vfs_write(dump_file, (char __user *) FAILSTRING,
				strlen(FAILSTRING), &dump_file->f_pos);
		}

		filp_close(dump_file, current->files);
	}

	big->data->bDumping = false;

	set_fs(fs);

	wake_unlock(&big->data->ssp_wake_lock);
	kfree(buffer);
	kfree(big);
#endif
	pr_err("[SSP]: %s done\n", __func__);
}

void ssp_temp_task(struct work_struct *work) {
#ifdef CONFIG_SENSORS_SSP_BBD
	pr_err("[SSPBBD]:TODO:%s()\n", __func__);
#else
	struct ssp_big *big;
	struct ssp_msg *msg;
	char *buffer;
	int buf_len, packet_len, residue, iRet = 0, index = 0, i = 0, buffindex = 0;

	big = container_of(work, struct ssp_big, work);
	buf_len = big->length > DATA_PACKET_SIZE ? DATA_PACKET_SIZE : big->length;
	buffer = kzalloc(buf_len, GFP_KERNEL);
	residue = big->length;
#ifdef CONFIG_SENSORS_SSP_SHTC1
	mutex_lock(&big->data->bulk_temp_read_lock);
	if (big->data->bulk_buffer == NULL)
		big->data->bulk_buffer = kzalloc(sizeof(struct shtc1_buffer),
				GFP_KERNEL);
	big->data->bulk_buffer->len = big->length / 12;
#endif
	while (residue > 0) {
		packet_len = residue > DATA_PACKET_SIZE ? DATA_PACKET_SIZE : residue;

		msg = kzalloc(sizeof(*msg), GFP_KERNEL);
		msg->cmd = MSG2SSP_AP_GET_BIG_DATA;
		msg->length = packet_len;
		msg->options = AP2HUB_READ | (index++ << SSP_INDEX);
		msg->data = big->addr;
		msg->buffer = buffer;
		msg->free_buffer = 0;

		iRet = ssp_spi_sync(big->data, msg, 1000);
		if (iRet != SUCCESS) {
			pr_err("[SSP]: %s - Fail to receive data %d\n", __func__, iRet);
			break;
		}
		// 12 = 1 chunk size for ks79.shin
		// order is thermistor Bat, thermistor PA, Temp, Humidity, Baro, Gyro
		// each data consist of 2bytes
		i = 0;
		while (packet_len - i >= 12) {
			ssp_dbg("[SSP]: %s %d %d %d %d %d %d", __func__,
					*((s16 *) (buffer + i + 0)), *((s16 *) (buffer + i + 2)),
					*((s16 *) (buffer + i + 4)), *((s16 *) (buffer + i + 6)),
					*((s16 *) (buffer + i + 8)), *((s16 *) (buffer +i + 10)));
#ifdef CONFIG_SENSORS_SSP_SHTC1
			big->data->bulk_buffer->batt[buffindex] = *((u16 *) (buffer + i + 0));
			big->data->bulk_buffer->chg[buffindex] = *((u16 *) (buffer + i + 2));
			big->data->bulk_buffer->temp[buffindex] = *((s16 *) (buffer + i + 4));
			big->data->bulk_buffer->humidity[buffindex] = *((u16 *) (buffer + i + 6));
			big->data->bulk_buffer->baro[buffindex] = *((s16 *) (buffer + i + 8));
			big->data->bulk_buffer->gyro[buffindex] = *((s16 *) (buffer + i + 10));
			buffindex++;
			i += 12;
#else
			buffindex++;
			i += 12;//6 ??
#endif
		}

		residue -= packet_len;
	}
#ifdef CONFIG_SENSORS_SSP_SHTC1
	if (iRet == SUCCESS)
		report_bulk_comp_data(big->data);
	mutex_unlock(&big->data->bulk_temp_read_lock);
#endif
	kfree(buffer);
	kfree(big);
	ssp_dbg("[SSP]: %s done\n", __func__);
#endif
}

/*************************************************************************/
/* SSP Debug timer function                                              */
/*************************************************************************/

int print_mcu_debug(char *pchRcvDataFrame, int *pDataIdx,
		int iRcvDataFrameLength)
{
	int iLength = pchRcvDataFrame[(*pDataIdx)++];
	int cur = *pDataIdx;

	if (iLength > iRcvDataFrameLength - *pDataIdx || iLength <= 0) {
		ssp_dbg("[SSP]: MSG From MCU - invalid debug length(%u/%d/%d)\n",
			iLength, iRcvDataFrameLength, cur);
		return iLength ? iLength : ERROR;
	}

	ssp_dbg("[SSP]: MSG From MCU - %s\n", &pchRcvDataFrame[*pDataIdx]);
	*pDataIdx += iLength;
	return 0;
}

void reset_mcu(struct ssp_data *data)
{
#if SSP_STATUS_MONITOR
	data->bRefreshing = true;
#endif

	func_dbg();
	data->uResetCnt++;
	data->bSspReady = false;
	ssp_enable(data, false);
	clean_pending_list(data);
#if SSP_STATUS_MONITOR
	if ((data->reg_hub) && ((current_cable_type == POWER_SUPPLY_TYPE_MAINS)
		|| (current_cable_type == POWER_SUPPLY_TYPE_HV_MAINS)))
		toggle_mcu_hw_reset(data);
	else
#endif
		bbd_mcu_reset();

#if SSP_STATUS_MONITOR
	if ((data->reg_hub) && ((current_cable_type == POWER_SUPPLY_TYPE_MAINS)
		|| (current_cable_type == POWER_SUPPLY_TYPE_HV_MAINS)))
		queue_refresh_task(data, 0);
#endif
}

void sync_sensor_state(struct ssp_data *data)
{
	unsigned char uBuf[9] = {0,};
	unsigned int uSensorCnt;
	int iRet = 0;

	pr_info("[SSP]%s\n", __func__);

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
	iRet = set_hrm_calibration(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_hrm_cal failed\n", __func__);
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
	iRet = set_pressure_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_pressure_cal failed\n", __func__);
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
	iRet = set_gyro_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_gyro_cal failed\n", __func__);
#endif

#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
	iRet = set_accel_cal(data);
	if (iRet < 0)
		pr_err("[SSP]: %s - set_accel_cal failed\n", __func__);
#endif
	udelay(10);

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++) {
		if (atomic64_read(&data->aSensorEnable) & (1 << uSensorCnt)) {
			s32 dMsDelay =
				get_msdelay(data->adDelayBuf[uSensorCnt]);

			memcpy(&uBuf[0], &dMsDelay, 4);
			memcpy(&uBuf[4], &data->batchLatencyBuf[uSensorCnt], 4);
			uBuf[8] = data->batchOptBuf[uSensorCnt];
			pr_info("[SSP]%s, enable sensor(%d)\n", __func__,
				uSensorCnt);
			send_instruction(data, ADD_SENSOR, uSensorCnt, uBuf, 9);
			udelay(10);
		}
	}
}

static void print_sensordata(struct ssp_data *data, unsigned int uSensor)
{
	switch (uSensor) {
	case ACCELEROMETER_SENSOR:
	case GYROSCOPE_SENSOR:
	case GEOMAGNETIC_RAW:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].x, data->buf[uSensor].y,
			data->buf[uSensor].z,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GEOMAGNETIC_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].cal_x, data->buf[uSensor].cal_y,
			data->buf[uSensor].cal_y, data->buf[uSensor].accuracy,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GEOMAGNETIC_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y, data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case PRESSURE_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].pressure[0],
			data->buf[uSensor].pressure[1],
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case LIGHT_SENSOR:
#if defined(CONFIG_SENSORS_SSP_TSL2584)
		ssp_dbg("[SSP] %u : %u, %u, %u, %u, %u(%ums)\n", uSensor,
			data->buf[uSensor].ch0_lower, data->buf[uSensor].ch0_upper,
			data->buf[uSensor].ch1_lower, data->buf[uSensor].ch1_upper,
			data->buf[uSensor].gain, get_msdelay(data->adDelayBuf[uSensor]));
#elif defined(CONFIG_SENSORS_SSP_CM3323)
		ssp_dbg("[SSP] %u : %u, %u, %u, %u(%ums)\n", uSensor,
			data->buf[uSensor].r, data->buf[uSensor].g,
			data->buf[uSensor].b, data->buf[uSensor].w,
			get_msdelay(data->adDelayBuf[uSensor]));
#endif
		break;
	case TEMPERATURE_HUMIDITY_SENSOR:
#ifdef CONFIG_SENSORS_SSP_SKIN_TEMP
		ssp_dbg("%u : %d, %d, %d, %d(%ums)", uSensor,
			data->buf[uSensor].skin_temp, data->buf[uSensor].skin_humid,
			data->buf[uSensor].env_temp, data->buf[uSensor].env_humid,
			get_msdelay(data->adDelayBuf[uSensor]));
#else
		ssp_dbg("%u : %d, %d, %d (%ums)", uSensor,
			data->buf[uSensor].temp, data->buf[uSensor].humi,
			data->buf[uSensor].time,
			get_msdelay(data->adDelayBuf[uSensor]));
#endif
		break;
	case GAME_ROTATION_VECTOR:
	case ROTATION_VECTOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d (%ums, %dms)\n", uSensor,
			data->buf[uSensor].quat_a, data->buf[uSensor].quat_b,
			data->buf[uSensor].quat_c, data->buf[uSensor].quat_d,
			data->buf[uSensor].acc_rot,
			get_msdelay(data->adDelayBuf[uSensor]),
			data->batchLatencyBuf[uSensor]);
		break;
	case GYRO_UNCALIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].uncal_x, data->buf[uSensor].uncal_y,
			data->buf[uSensor].uncal_z, data->buf[uSensor].offset_x,
			data->buf[uSensor].offset_y,
			data->buf[uSensor].offset_z,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case HRM_RAW_SENSOR:
	case FRONT_HRM_RAW_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d, %d, %d, %d, %d(%ums)\n",
			uSensor,	data->buf[uSensor].ch_a_sum , data->buf[uSensor].ch_a_x1,
			data->buf[uSensor].ch_a_x2 , data->buf[uSensor].ch_a_y1,
			data->buf[uSensor].ch_a_y2 , data->buf[uSensor].ch_b_sum,
			data->buf[uSensor].ch_b_x1 , data->buf[uSensor].ch_b_x2,
			data->buf[uSensor].ch_b_y1 , data->buf[uSensor].ch_b_y2,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case HRM_RAW_FAC_SENSOR:
	case FRONT_HRM_RAW_FAC_SENSOR:
		ssp_dbg("[SSP] %u : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d(%ums)\n",
			uSensor, data->buf[uSensor].hrm_eol_data[0], data->buf[uSensor].hrm_eol_data[1],
			data->buf[uSensor].hrm_eol_data[2], data->buf[uSensor].hrm_eol_data[3],
			data->buf[uSensor].hrm_eol_data[4], data->buf[uSensor].hrm_eol_data[5],
			data->buf[uSensor].hrm_eol_data[6], data->buf[uSensor].hrm_eol_data[7],
			data->buf[uSensor].hrm_eol_data[8], data->buf[uSensor].hrm_eol_data[9],
			data->buf[uSensor].hrm_eol_data[10], data->buf[uSensor].hrm_eol_data[11],
			data->buf[uSensor].hrm_eol_data[12], data->buf[uSensor].hrm_eol_data[13],
			data->buf[uSensor].hrm_eol_data[14], data->buf[uSensor].hrm_eol_data[15],
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case HRM_LIB_SENSOR:
	case FRONT_HRM_LIB_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].hr , data->buf[uSensor].rri,
			data->buf[uSensor].snr,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case UV_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d(%ums)\n", uSensor,
			data->buf[uSensor].uv_raw , data->buf[uSensor].hrm_temp,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case GSR_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d (%ums)\n", uSensor,
			data->buf[uSensor].ohm, data->buf[uSensor].adc, data->buf[uSensor].inj_c,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	case ECG_SENSOR:
#if 1
		ssp_dbg("[SSP] %u : %d, %d, %d, %d, %d, %d(%ums)\n", uSensor,
			data->buf[uSensor].ecg_data[0], data->buf[uSensor].ecg_data[1],
			data->buf[uSensor].ecg_data[2], data->buf[uSensor].ecg_data[3],
			data->buf[uSensor].ecg_data[4], data->buf[uSensor].ecg_data[5],
			get_msdelay(data->adDelayBuf[uSensor]));
#else
		ssp_dbg("[SSP] %u : %d, %d(%ums)\n", uSensor,
			data->buf[uSensor].ecg, data->buf[uSensor].enable_flag,
			get_msdelay(data->adDelayBuf[uSensor]));
#endif
		break;
	case GRIP_SENSOR:
		ssp_dbg("[SSP] %u : %d, %d, %d %d(%ums)\n", uSensor,
			data->buf[uSensor].data1, data->buf[uSensor].data2,
			data->buf[uSensor].data3, data->buf[uSensor].data4,
			get_msdelay(data->adDelayBuf[uSensor]));
		break;
	default:
		ssp_dbg("[SSP] Wrong sensorCnt: %u\n", uSensor);
		break;
	}
}

static void recovery_mcu(struct ssp_data *data)
{
	if (data->uComFailCnt < LIMIT_RESET_CNT) {
		pr_info("[SSP] : %s - uTimeOutCnt(%u), pending(%u)\n", __func__,
			data->uTimeOutCnt, !list_empty(&data->pending_list));
		data->uComFailCnt++;
#if SSP_STATUS_MONITOR
		if (data->bRefreshing) {
			pr_err("[SSP] : %s MCU is refreshing another route....."
				"Wait 2sec\n", __func__);
			msleep(2000);
		} else
#endif
		reset_mcu(data);
	} else {
		ssp_enable(data, false);
	}

	data->uTimeOutCnt = 0;
}

static void debug_work_func(struct work_struct *work)
{
	unsigned int uSensorCnt;
	struct ssp_data *data = container_of(work, struct ssp_data, work_debug);
	int bbd_rbuf_cnt;
	int bcm_wbuf_cnt;

	bbd_rbuf_cnt = bbd_on_read_buf_cnt(1);
	bcm_wbuf_cnt = bcm_spi_rw_buf_cnt(1);

	ssp_dbg("[SSP]: %s(%u) - Sensor state: 0x%llx, RC: %u, CC: %u, DC: %u, TC: %u, RB_CNT: %d, WB_CNT: %d\n",
		__func__, data->uIrqCnt, data->uSensorState, data->uResetCnt,
			data->uComFailCnt, data->uDumpCnt, data->uTimeOutCnt,
				bbd_rbuf_cnt, bcm_wbuf_cnt);

	for (uSensorCnt = 0; uSensorCnt < SENSOR_MAX; uSensorCnt++)
		if ((atomic64_read(&data->aSensorEnable) & (1 << uSensorCnt))
			|| data->batchLatencyBuf[uSensorCnt])
			print_sensordata(data, uSensorCnt);

	if (((atomic64_read(&data->aSensorEnable) & (1 << ACCELEROMETER_SENSOR))
		&& (data->batchLatencyBuf[ACCELEROMETER_SENSOR] == 0)
		&& (data->uIrqCnt == 0) && (data->uTimeOutCnt > 0))
		|| (data->uTimeOutCnt > LIMIT_TIMEOUT_CNT))
		recovery_mcu(data);

	data->uIrqCnt = 0;
}

static void debug_timer_func(unsigned long ptr)
{
	struct ssp_data *data = (struct ssp_data *)ptr;

	queue_work(data->debug_wq, &data->work_debug);
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void enable_debug_timer(struct ssp_data *data)
{
	mod_timer(&data->debug_timer,
		round_jiffies_up(jiffies + SSP_DEBUG_TIMER_SEC));
}

void disable_debug_timer(struct ssp_data *data)
{
	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
}

int initialize_debug_timer(struct ssp_data *data)
{
	setup_timer(&data->debug_timer, debug_timer_func, (unsigned long)data);

	data->debug_wq = create_singlethread_workqueue("ssp_debug_wq");
	if (!data->debug_wq)
		return ERROR;

	INIT_WORK(&data->work_debug, debug_work_func);
	return SUCCESS;
}

unsigned int  ssp_check_sec_dump_mode() /* if returns true dump mode on */
{
/* DSKIM
	if (sec_debug_level.en.kernel_fault == 1)
		return 1;
	else
*/
		return 0;
}

static void crash_dump_work_func(struct work_struct *work)
{
	struct ssp_dump *dump =
		container_of(work, struct ssp_dump, work);

	debug_crash_dump(dump->data, dump->dump_data,
		dump->length);

	vfree(dump->dump_data);
	kfree(dump);
}

int handle_dump_data(struct ssp_data *data, char *dump_data, int length)
{
	struct ssp_dump *dump = kzalloc(sizeof(*dump), GFP_KERNEL);
	dump->data = data;
	dump->length = length;
	dump->dump_data = vmalloc(length);
	if (!dump->dump_data) {
		pr_err("[SSP]%s, memory allocation failed\n", __func__);
		return -ENOMEM;
	}

	memcpy(dump->dump_data, dump_data, length);

	INIT_WORK(&dump->work, crash_dump_work_func);
	queue_work(data->dump_wq, &dump->work);

	return SUCCESS;
}

#if SSP_STATUS_MONITOR
static int check_abnormal_status(struct ssp_data *data, unsigned int uSensor)
{
	static s16 pre_buff[3] = {0, };
	int ret = 0;

	if (pre_buff[0] == data->buf[uSensor].x)
		if (pre_buff[1] == data->buf[uSensor].y)
			if (pre_buff[2] == data->buf[uSensor].z) {
				pr_err("[SSP] %s-Sensor[%d] data not changed!!\n",
					__func__, uSensor);
				if (data->batchLatencyBuf[ACCELEROMETER_SENSOR]
					>= (SSP_MONITOR_TIME * 1000))
					pr_err("[SSP]:- batchLatencyBuf[%d]ms pass...!!!\n",
					data->batchLatencyBuf[ACCELEROMETER_SENSOR]);
				else {
					ret = -1;
					pre_buff[0] = 0;
					pre_buff[1] = 0;
					pre_buff[2] = 0;
				}
				return ret;
			}

	pre_buff[0] = data->buf[uSensor].x;
	pre_buff[1] = data->buf[uSensor].y;
	pre_buff[2] = data->buf[uSensor].z;

	return ret;
}

static void debug_polling_func(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
		struct ssp_data, polling_work);

	if (data->bSspShutdown) {
		pr_err("[SSP] : %s MCU is disabled...\n", __func__);
		goto out;
	}
	if (data->bRefreshing) {
		pr_err("[SSP] : %s MCU is refreshing another route.....\n",
			__func__);
		goto out;
	}

#if 0 /* Check raised IRQ count number */
	if (atomic_read(&data->aSensorEnable) & (!data->uSubIrqCnt)) {
		pr_err("[SSP] : %s(%u) aSensorEnable:0x%x. No irp happened. MCU reset now!\n",
			__func__, data->uSubIrqCnt,
			(unsigned int)atomic_read(&data->aSensorEnable));
		if (data->bSspShutdown == false) {
			/* reset_mcu(data); */
			goto out;
		} else
			pr_err("[SSP] : %s MCU is shutdowned. Could not reset.\n",
			__func__);
		goto out;
	}
#endif
#if 1 /* Check if Acc data is the same as previous or not. */
	/* Check if acc sensor keep working */
	if (atomic64_read(&data->aSensorEnable) & (1 << ACCELEROMETER_SENSOR))
		if (check_abnormal_status(data, ACCELEROMETER_SENSOR)) {
			pr_err("[SSP] : Acc not working. MCU reset now...\n");
			reset_mcu(data);
		}
#endif
	data->uSubIrqCnt = 0;

out:
	schedule_delayed_work(&data->polling_work,
		msecs_to_jiffies(SSP_MONITOR_TIME * 1000));
}

int initialize_polling_work(struct ssp_data *data)
{
	INIT_DELAYED_WORK(&data->polling_work, debug_polling_func);
	pr_info("[SSP] : %s finished\n", __func__);

	return SUCCESS;
}
#endif
