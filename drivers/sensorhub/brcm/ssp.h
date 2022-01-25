/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SSP_PRJ_H__
#define __SSP_PRJ_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/wakelock.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/battery/sec_battery.h>
#ifndef CONFIG_OF
#include <linux/ssp_platformdata.h>
#endif

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif
#include "factory/ssp_factory.h"
#include "bbdpl/bbd.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#undef CONFIG_HAS_EARLYSUSPEND
#endif

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
#include "ssp_sensorhub.h"
#endif

/* For communication with sensorhub */
#include "ssp_protocol.h"

#define SSP_DBG		1
#ifdef CONFIG_SEC_DEBUG
#define SSP_SEC_DEBUG	1
#else
#define SSP_SEC_DEBUG	0
#endif

#define SUCCESS		1
#define FAIL		0
#define ERROR		-1

#define FACTORY_DATA_MAX	100

#if SSP_DBG
#define SSP_FUNC_DBG 1
#define SSP_DATA_DBG 0

/* ssp mcu device ID */
#define DEVICE_ID		0x55

#define ssp_dbg(format, ...) do { \
	printk(KERN_INFO format, ##__VA_ARGS__); \
	} while (0)
#else
#define ssp_dbg(format, ...)
#endif

#if SSP_FUNC_DBG
#define func_dbg() do { \
	printk(KERN_INFO "[SSP]: %s\n", __func__); \
	} while (0)
#else
#define func_dbg()
#endif

#if SSP_DATA_DBG
#define data_dbg(format, ...) do { \
	printk(KERN_INFO format, ##__VA_ARGS__); \
	} while (0)
#else
#define data_dbg(format, ...)
#endif

#define SSP_SW_RESET_TIME	3000
#define DEFUALT_POLLING_DELAY	(200 * NSEC_PER_MSEC)
#define PROX_AVG_READ_NUM	80
#define DEFAULT_RETRIES		3
#define DATA_PACKET_SIZE	960

#define DEFAULT_BRCM_MODEL_NAME	"BCM4774IUB2G"

#define SSP_STATUS_MONITOR	0
#if SSP_STATUS_MONITOR
#define SSP_MONITOR_TIME	4
extern int current_cable_type;
#endif

/* SSP Binary Type */
enum {
	KERNEL_BINARY = 0,
	KERNEL_CRASHED_BINARY,
	UMS_BINARY,
};

/*
 * SENSOR_DELAY_SET_STATE
 * Check delay set to avoid sending ADD instruction twice
 */
enum {
	INITIALIZATION_STATE = 0,
	NO_SENSOR_STATE,
	ADD_SENSOR_STATE,
	RUNNING_SENSOR_STATE,
};

/* Firmware download STATE */
enum {
	FW_DL_STATE_FAIL = -1,
	FW_DL_STATE_NONE = 0,
	FW_DL_STATE_NEED_TO_SCHEDULE,
	FW_DL_STATE_SCHEDULED,
	FW_DL_STATE_DOWNLOADING,
	FW_DL_STATE_SYNC,
	FW_DL_STATE_DONE,
};

/* for MSG2SSP_AP_GET_THERM */
enum {
	ADC_BATT = 0,
	ADC_CHG,
};

enum {
	SENSORS_BATCH_DRY_RUN               = 0x00000001,
	SENSORS_BATCH_WAKE_UPON_FIFO_FULL   = 0x00000002
};

enum {
	META_DATA_FLUSH_COMPLETE = 1,
};

#define SSP_INVALID_REVISION		99999
#define SSP_INVALID_REVISION2		0xFFFFFF

/* Gyroscope DPS */
#define GYROSCOPE_DPS250		250
#define GYROSCOPE_DPS500		500
#define GYROSCOPE_DPS1000		1000
#define GYROSCOPE_DPS2000		2000

/* Gesture Sensor Current */
#define DEFUALT_IR_CURRENT		100

/* kernel -> ssp manager cmd*/
#define SSP_LIBRARY_SLEEP_CMD		(1 << 5)
#define SSP_LIBRARY_LARGE_DATA_CMD	(1 << 6)
#define SSP_LIBRARY_WAKEUP_CMD		(1 << 7)

/* voice data */
#define TYPE_WAKE_UP_VOICE_SERVICE		0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_AM	0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_GRAMMER	0x02

/* Sensors's reporting mode */
#define REPORT_MODE_CONTINUOUS 0
#define REPORT_MODE_ON_CHANGE  1
#define REPORT_MODE_SPECIAL    2
#define REPORT_MODE_UNKNOWN    3

/* Factory data length */
#define ACCEL_FACTORY_DATA_LENGTH		1
#define GYRO_FACTORY_DATA_LENGTH			36
#define MAGNETIC_FACTORY_DATA_LENGTH	26
#define PRESSURE_FACTORY_DATA_LENGTH		1
#define MCU_FACTORY_DATA_LENGTH			5
#define GYRO_TEMP_FACTORY_DATA_LENGTH	2
#define GYRO_DPS_FACTORY_DATA_LENGTH	1
#define MCU_SLEEP_FACTORY_DATA_LENGTH	FACTORY_DATA_MAX
#define HRM_LIB_VERSION_INFO_LENGTH		10
#define HRM_IR_LEVEL_THRESHOLD_LENGTH	4

/* SENSOR_TYPE */
enum {
	ACCELEROMETER_SENSOR = 0,		/* [0]=1 */
	GYROSCOPE_SENSOR,				/* [1]=2 */
	GEOMAGNETIC_UNCALIB_SENSOR,	/* [2]=4 */
	GEOMAGNETIC_RAW,				/* [3]=8 */
	GEOMAGNETIC_SENSOR,			/* [4]=16 */
	PRESSURE_SENSOR,				/* [5]=32 */
	GESTURE_SENSOR,				/* [6]=64 */
	PROXIMITY_SENSOR,				/* [7]=128 */
	TEMPERATURE_HUMIDITY_SENSOR,/* [8]=256 */
	LIGHT_SENSOR,					/* [9]=512 */
	PROXIMITY_RAW,				/* [10]=1024 */
	ORIENTATION_SENSOR,			/* [11]=2048 */
	STEP_DETECTOR,					/* [12]=4096 */
	SIG_MOTION_SENSOR,			/* [13]=8192 */
	GYRO_UNCALIB_SENSOR,			/* [14]=16384 */
	GAME_ROTATION_VECTOR,		/* [15]=32768 */
	ROTATION_VECTOR,				/* [16]=65536 */
	STEP_COUNTER,					/* [17]=131072 */
	HRM_RAW_SENSOR,				/* [18]=262144 */
	HRM_RAW_FAC_SENSOR,			/* [19]=524288 */
	HRM_LIB_SENSOR,				/* [20]=1048576 */
	TILT_MOTION,					/* [21]=2097152 */
	UV_SENSOR,					/* [22]=4194304 */
	PIR_SENSOR,					/* [23]=8388608 */
	PIR_RAW_SENSOR,				/* [24]=16777216 */
	FRONT_HRM_RAW_SENSOR,		/* [25]=33554432 */
	FRONT_HRM_RAW_FAC_SENSOR,	/* [26]=67108864 */
	FRONT_HRM_LIB_SENSOR,			/* [27]=134217728 */
	GSR_SENSOR,					/* [28]=268435456 */
	ECG_SENSOR,					/* [29]=536870912 */
	GRIP_SENSOR,					/* [30]=1073741824 */
	GRIP_RAW_SENSOR,				/* [31]=2147483648 */
	SENSOR_MAX,
};

#define SENSOR_REPORT_MODE { \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_SPECIAL, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_CONTINUOUS, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_UNKNOWN, \
	REPORT_MODE_UNKNOWN, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_ON_CHANGE, \
	REPORT_MODE_CONTINUOUS, }

#define SENSOR_DATA_LEN    { \
	6, 6, 12, 6, 7, 6, 20, 3, 5, 10, \
	2, 9, 0, 1, 1, 12, 17, 17, 4, 0, \
	0, 0, 0, 1, 12, 6, 1, 6, }

struct meta_data_event {
	s32 what;
	s32 sensor;
} __attribute__((__packed__));

struct sensor_value {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		struct {		/*calibrated mag, gyro*/
			s16 cal_x;
			s16 cal_y;
			s16 cal_z;
			u8 accuracy;
		};
		struct {		/*uncalibrated mag, gyro*/
			s16 uncal_x;
			s16 uncal_y;
			s16 uncal_z;
			s16 offset_x;
			s16 offset_y;
			s16 offset_z;
		};
		struct {		/* rotation vector */
			s32 quat_a;
			s32 quat_b;
			s32 quat_c;
			s32 quat_d;
			u8 acc_rot;
		};
#if defined(CONFIG_SENSORS_SSP_TSL2584)
		struct {		/* light sensor */
			u8 ch0_lower;
			u8 ch0_upper;
			u8 ch1_lower;
			u8 ch1_upper;
			u32 gain;
		};
#elif defined(CONFIG_SENSORS_SSP_CM3323)
		struct {
			u16 r;
			u16 g;
			u16 b;
			u16 w;
		};
#endif
		struct { /* humi/temp sensor */
			u16 temp;
			u16 humi;
			u8 time;
		};
		struct {
			u32 ch_a_sum;
			u32 ch_a_x1;
			u32 ch_a_x2;
			u32 ch_a_y1;
			u32 ch_a_y2;
			u32 ch_b_sum;
			u32 ch_b_x1;
			u32 ch_b_x2;
			u32 ch_b_y1;
			u32 ch_b_y2;
		};
		struct {
			s16 hr;
			s16 rri;
			s32 snr;
		};
/* hrm_eol_data
	ADI						TI
	frequency				ir_low_dc
	green_dc_level			ir_high_dc
	green_high_dc_level		ir_ac
	green_mid_dc_level		ir_acdc_ratio
	red_dc_levet			ir_noise
	ir_dc_level				ir_frequency
	noise_level				green_low_dc
	adc_offset[0]			green_high_dc
	adc_offset[1]			green_ac
	adc_offset[2]			green_acdc_ratio
	adc_offset[3]			green_noise
	adc_offset[4]			dummy
	adc_offset[5]			dummy
	adc_offset[6]			dummy
	adc_offset[7]			dummy
	oscRegValue				dummy
*/
		u32 hrm_eol_data[16];
		u8 front_hrm_raw[6];
		u8 ecg_data[6];
		struct {
			s16 skin_temp;
			s16 skin_humid;
			s16 env_temp;
			s16 env_humid;
		};
		struct {
			u32 uv_raw;
			u32 hrm_temp;
		};
		u8 data[20];
		s32 pressure[3];
		struct {
			u32 ohm;
			u16 adc;
			u8 inj_c;
		};
		struct {
			u32 data1;
			u16 data2;
			u16 data3;
			u8	data4;
		};
		struct meta_data_event meta_data;
	};
	u64 timestamp;
} __attribute__((__packed__));

extern struct class *sensors_event_class;

struct calibraion_data {
	s16 x;
	s16 y;
	s16 z;
};

struct hw_offset_data {
	char x;
	char y;
	char z;
};

/* ssp_msg options bit*/
#define SSP_SPI		0	/* read write mask */
#define SSP_RETURN	2	/* write and read option */
#define SSP_GYRO_DPS	3	/* gyro dps mask */
#define SSP_INDEX	3	/* data index mask */

#define SSP_SPI_MASK		(3 << SSP_SPI)	/* read write mask */
#define SSP_GYRO_DPS_MASK	(3 << SSP_GYRO_DPS)
/* dump index mask. Index is up to 8191 */
#define SSP_INDEX_MASK		(8191 << SSP_INDEX)

struct ssp_msg {
	u8 cmd;
	u16 length;
	u16 options;
	u32 data;

	struct list_head list;
	struct completion *done;
	char *buffer;
	u8 free_buffer;
	bool *dead_hook;
	bool dead;
} __attribute__((__packed__));

enum {
	AP2HUB_READ = 0,
	AP2HUB_WRITE,
	HUB2AP_WRITE,
	AP2HUB_READY,
	AP2HUB_RETURN
};

enum {
	BIG_TYPE_DUMP = 0,
	BIG_TYPE_READ_LIB,
	/*+snamy.jeong 0706 for voice model download & pcm dump*/
	BIG_TYPE_VOICE_NET,
	BIG_TYPE_VOICE_GRAM,
	BIG_TYPE_VOICE_PCM,
	/*-snamy.jeong 0706 for voice model download & pcm dump*/
	BIG_TYPE_TEMP,
	BIG_TYPE_MAX,
};

enum {
	BATCH_MODE_NONE = 0,
	BATCH_MODE_RUN,
};

struct ssp_time_diff {
	u16 batch_count;
	u16 batch_mode;
	u64 time_diff;
	u64 irq_diff;
	u16 batch_count_fixed;
};

struct ssp_data {
	struct input_dev *acc_input_dev;
	struct input_dev *gyro_input_dev;
	struct input_dev *motion_input_dev;
	struct input_dev *mag_input_dev;
	struct input_dev *uncal_mag_input_dev;
	struct input_dev *uncalib_gyro_input_dev;
	struct input_dev *pressure_input_dev;
	struct input_dev *light_input_dev;
	struct input_dev *temp_humi_input_dev;
	struct input_dev *rot_input_dev;
	struct input_dev *game_rot_input_dev;
	struct input_dev *hrm_raw_input_dev;
	struct input_dev *hrm_lib_input_dev;
	struct input_dev *meta_input_dev;
	struct input_dev *front_hrm_raw_input_dev;
	struct input_dev *uv_input_dev;
	struct input_dev *gsr_input_dev;
	struct input_dev *ecg_input_dev;
	struct input_dev *grip_input_dev;
	struct input_dev *skin_temp_input_dev;

	struct spi_device *spi;
	struct wake_lock ssp_wake_lock;
	struct wake_lock ssp_comm_wake_lock;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct workqueue_struct *dump_wq;
	struct workqueue_struct *lpm_motion_wq;
	struct work_struct work_debug;
	struct work_struct work_lpm_motion;

	struct workqueue_struct *bbd_on_packet_wq;
	struct work_struct work_bbd_on_packet;
	struct workqueue_struct *bbd_mcu_ready_wq;
	struct work_struct work_bbd_mcu_ready;

#ifdef SSP_BBD_USE_SEND_WORK
    struct workqueue_struct *bbd_send_packet_wq;
    struct work_struct work_bbd_send_packet;
    struct ssp_msg *bbd_send_msg;
    unsigned short bbd_msg_options;
#endif  /* SSP_BBD_USE_SEND_WORK  */

#if SSP_STATUS_MONITOR
	struct delayed_work polling_work;
#endif
	struct calibraion_data accelcal;
	struct calibraion_data gyrocal;
	struct hw_offset_data magoffset;
	u32 hrmcal[16];
	struct sensor_value buf[SENSOR_MAX];
	struct device *sen_dev;
	struct device *mcu_device;
	struct device *acc_device;
	struct device *gyro_device;
	struct device *mag_device;
	struct device *prs_device;
	struct device *light_device;
	struct device *temphumidity_device;
	struct device *hrm_device;
	struct device *front_hrm_device;
	struct device *uv_device;
	struct device *gsr_device;
	struct device *ecg_device;
	struct device *grip_device;

	struct delayed_work work_firmware;
	struct delayed_work work_refresh;
	struct miscdevice shtc1_device;
	struct miscdevice batch_io_device;

/*snamy.jeong@samsung.com temporary code for voice data sending to mcu*/
	struct device *voice_device;

	bool bSspShutdown;
	bool bSspReady;
	bool bAccelAlert;
	bool bGeomagneticRawEnabled;
	bool bMcuDumpMode;
	bool bBinaryChashed;
	bool bProbeIsDone;
	bool bDumping;
	bool bTimeSyncing;
	bool bLpModeEnabled;
	bool bHandlingIrq;
	bool bBarcodeEnabled;
	bool cameraGyroSyncMode;
	u64 lastTimestamp[SENSOR_MAX];
	u64 lastModTimestamp[SENSOR_MAX];
	int data_len[SENSOR_MAX];
	int report_mode[SENSOR_MAX];
	bool reportedData[SENSOR_MAX];
	bool skipEventReport;
#if SSP_STATUS_MONITOR
	bool bRefreshing;
#endif

	unsigned char uFuseRomData[3];
	unsigned char uMagCntlRegData;
	char *pchLibraryBuf;
	int iIrq;
	int iLibraryLength;
	int aiCheckStatus[SENSOR_MAX];

	unsigned int uComFailCnt;
	unsigned int uResetCnt;
	unsigned int uTimeOutCnt;
	unsigned int uIrqCnt;
#if SSP_STATUS_MONITOR
	unsigned int uSubIrqCnt;
#endif
	unsigned int uDumpCnt;

	int sealevelpressure;
	unsigned int uGyroDps;
	u64 uSensorState;
	unsigned int uCurFirmRev;
	unsigned int uFactoryProxAvg[4];
	char uLastResumeState;
	char uLastAPState;
	s32 iPressureCal;

	atomic64_t aSensorEnable;
	atomic_t apShutdownProgress;
	int64_t adDelayBuf[SENSOR_MAX];
	s32 batchLatencyBuf[SENSOR_MAX];
	s8 batchOptBuf[SENSOR_MAX];

	int (*set_mcu_reset)(int);
	void (*get_sensor_data[SENSOR_MAX])(char *, int *,
		struct sensor_value *);
	void (*report_sensor_data[SENSOR_MAX])(struct ssp_data *,
		struct sensor_value *);
	int (*check_lpmode)(void);
	atomic_t eol_enable;
	int (*hrm_sensor_power)(int);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	struct ssp_sensorhub_data *hub_data;
#endif
	int ap_rev;
	int accel_position;
	int mag_position;
	int fw_dl_state;
	u8 mag_matrix_size;
	u8 *mag_matrix;
	const char *fw_name;
	struct mutex comm_mutex;
	struct mutex pending_mutex;

	/* for factory sysfs nodes reliability */
	struct mutex sysfs_op_mtx;

	u32 lux;

	int mcu_int1;
	int mcu_int2;
	int ap_int;
	int rst;
	int chg;
	struct regulator *reg_hub;
	struct regulator *reg_sns;	/* regulator ctnl for each sensor */

	/* for VDD_AUX_IN of BCM477x, optional */
	struct regulator *reg_aux;

	struct list_head pending_list;
	void (*ssp_big_task[BIG_TYPE_MAX])(struct work_struct *);
	u64 timestamp;
	struct file *realtime_dump_file;
	int total_dump_size;
#ifdef CONFIG_SLEEP_MONITOR
	long long service_mask;
#endif
	u8 lpm_int_mode;
	s8 lpm_rotation_info;

	const char *model_name;
};

struct ssp_big {
	struct ssp_data *data;
	struct work_struct work;
	u32 length;
	u32 addr;
};

struct ssp_dump {
	struct ssp_data *data;
	struct work_struct work;
	char *dump_data;
	u32 length;
};

extern struct ssp_data *sensorhub_data;

void ssp_enable(struct ssp_data *, bool);
int ssp_spi_async(struct ssp_data *, struct ssp_msg *);
int ssp_spi_sync(struct ssp_data *, struct ssp_msg *, int);
void clean_pending_list(struct ssp_data *);
void toggle_mcu_reset(struct ssp_data *);
#if SSP_STATUS_MONITOR
void toggle_mcu_hw_reset(struct ssp_data *);
#endif
int initialize_mcu(struct ssp_data *);
int initialize_input_dev(struct ssp_data *);
int initialize_sysfs(struct ssp_data *);
void initialize_function_pointer(struct ssp_data *);

void report_meta_data(struct ssp_data *, struct sensor_value *);
#ifdef CONFIG_SENSORS_SSP_ACCELEROMETER_SENSOR
int accel_open_calibration(struct ssp_data *);
int set_accel_cal(struct ssp_data *);
void report_acc_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_GYRO_SENSOR
int gyro_open_calibration(struct ssp_data *);
int set_gyro_cal(struct ssp_data *);
int save_gyro_caldata(struct ssp_data *, s16 *);
void report_gyro_data(struct ssp_data *, struct sensor_value *);
void report_uncalib_gyro_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC_SENSOR
void report_mag_data(struct ssp_data *, struct sensor_value *);
void report_mag_uncaldata(struct ssp_data *, struct sensor_value *);
void report_geomagnetic_raw_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_PRESSURE_SENSOR
int pressure_open_calibration(struct ssp_data *);
int set_pressure_cal(struct ssp_data *);
void report_pressure_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_LIGHT_SENSOR
void report_light_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_TEMP_HUMID_SENSOR
void report_temp_humidity_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_ROT_VECTOR_SENSOR
void report_rot_data(struct ssp_data *, struct sensor_value *);
void report_game_rot_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_HRM_SENSOR
int hrm_open_calibration(struct ssp_data *);
int set_hrm_calibration(struct ssp_data *);
void report_hrm_raw_data(struct ssp_data *, struct sensor_value *);
void report_hrm_raw_fac_data(struct ssp_data *, struct sensor_value *);
void report_hrm_lib_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_FRONT_HRM_SENSOR
void report_front_hrm_raw_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_UV_SENSOR
void report_uv_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_GSR_SENSOR
void report_gsr_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_ECG_SENSOR
void report_ecg_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_GRIP_SENSOR
void report_grip_data(struct ssp_data *, struct sensor_value *);
#endif

#ifdef CONFIG_SENSORS_SSP_LPM_MOTION
int ssp_charging_motion(struct ssp_data *, int);
void report_key_event(struct ssp_data *);
int ssp_parse_motion(struct ssp_data *, char *, int, int);
int ssp_charging_rotation(struct ssp_data *, int);
#endif

void initialize_factorytest(struct ssp_data *);
void remove_factorytest(struct ssp_data *);
void sensors_remove_symlink(struct input_dev *);
void destroy_sensor_class(void);
int initialize_event_symlink(struct ssp_data *);
int sensors_create_symlink(struct input_dev *);
void remove_input_dev(struct ssp_data *);
void remove_sysfs(struct ssp_data *);
void remove_event_symlink(struct ssp_data *);
int ssp_send_cmd(struct ssp_data *, char, int);
int send_instruction(struct ssp_data *, u8, u8, u8 *, u16);
int send_instruction_sync(struct ssp_data *, u8, u8, u8 *, u16);
int get_batch_count(struct ssp_data *, u8);
int select_irq_msg(struct ssp_data *);
int get_chipid(struct ssp_data *);
int set_big_data_start(struct ssp_data *, u8 , u32);
int set_sensor_position(struct ssp_data *);
void sync_sensor_state(struct ssp_data *);
int get_msdelay(int64_t);
u64 get_sensor_scanning_info(struct ssp_data *);
unsigned int get_firmware_rev(struct ssp_data *);
#ifdef CONFIG_SSP_RTC
unsigned int get_rtc_diff(struct ssp_data *data);
#endif
int parse_dataframe(struct ssp_data *, char *, int);
void enable_debug_timer(struct ssp_data *);
void disable_debug_timer(struct ssp_data *);
int initialize_debug_timer(struct ssp_data *);
int handle_dump_data(struct ssp_data *, char *, int);
#if SSP_STATUS_MONITOR
int initialize_polling_work(struct ssp_data *);
#endif
int initialize_lpm_motion(struct ssp_data *);
int print_mcu_debug(char *, int *, int);
unsigned int get_module_rev(struct ssp_data *data);
void reset_mcu(struct ssp_data *);
int queue_refresh_task(struct ssp_data *data, int delay);
void convert_acc_data(s16 *);
int sensors_register(struct device *, void *,
	struct device_attribute*[], char *);
void sensors_unregister(struct device *,
	struct device_attribute*[]);
ssize_t mcu_sensor_state(struct device *, struct device_attribute *, char *);
ssize_t mcu_reset_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_ready_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_dump_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_revision_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_update_ums_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_crashed_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_factorytest_store(struct device *, struct device_attribute *,
	const char *, size_t);
ssize_t mcu_factorytest_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_model_name_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_sleep_factorytest_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_sleep_factorytest_store(struct device *,
	struct device_attribute *, const char *, size_t);
ssize_t mcu_fota_rotate_status_show(struct device *,
	struct device_attribute *, char *);
unsigned int ssp_check_sec_dump_mode(void);

void ssp_dump_task(struct work_struct *work);
void ssp_read_big_library_task(struct work_struct *work);
void ssp_send_big_library_task(struct work_struct *work);
void ssp_pcm_dump_task(struct work_struct *work);
int debug_crash_dump(struct ssp_data *, char *, int);
void ssp_temp_task(struct work_struct *work);

int callback_bbd_on_control(void *ssh_data, const char *str_ctrl);
int callback_bbd_on_mcu_ready(void *ssh_data, bool ready);
int callback_bbd_on_packet_alarm(void *ssh_data);
void bbd_on_packet_work_func(struct work_struct *work);
void bbd_mcu_ready_work_func(struct work_struct *work);
#ifdef SSP_BBD_USE_SEND_WORK
void bbd_send_packet_work_func(struct work_struct *work);
#endif  /* SSP_BBD_USE_SEND_WORK  */
int set_time(struct ssp_data *);
int get_time(struct ssp_data *);

void ssp_dump_all_status(const char *buf, int st_alarm_cnt);
#endif
