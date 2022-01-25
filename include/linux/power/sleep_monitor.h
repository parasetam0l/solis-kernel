#ifndef _SLEEP_MONITOR_H
#define _SLEEP_MONITOR_H

#include <linux/random.h>
#include <linux/ktime.h>

extern struct dentry *slp_mon_d;
extern unsigned int special_key;

/* Debugging */
enum SLEEP_MONITOR_DEBUG_LEVEL {
	SLEEP_MONITOR_DEBUG_LABEL = BIT(0),
	SLEEP_MONITOR_DEBUG_INFO = BIT(1),
	SLEEP_MONITOR_DEBUG_ERR = BIT(2),
	SLEEP_MONITOR_DEBUG_DEVICE = BIT(3),
	SLEEP_MONITOR_DEBUG_DEBUG = BIT(4),
	SLEEP_MONITOR_DEBUG_WORK = BIT(5),
	SLEEP_MONITOR_DEBUG_INIT_TIMER = BIT(6),
	SLEEP_MONITOR_DEBUG_READ_SUS_RES_TIME = BIT(7)
};

#define sleep_mon_dbg(debug_level_mask, fmt, ...)               \
	do {                                    \
		if (debug_level & debug_level_mask) \
		pr_info("%s[%d]" fmt, SLEEP_MONITOR_DEBUG_PREFIX,   \
				debug_level_mask, ##__VA_ARGS__);       \
	} while (0)

/* Each device info */
typedef struct sleep_monitor_device {
	struct sleep_monitor_ops *sm_ops;
	void *priv;
	int check_level;
	bool skip_device;
	char *device_name;
	ktime_t sus_res_time[2];
} sleep_monitor_device;

/* Sleep monitor's ops structure */
struct sleep_monitor_ops{
	int (*read_cb_func)(void *priv, unsigned int *raw_val, int check_level, int caller_type);
	int (*read64_cb_func)(void *priv, long long *raw_val, int check_level, int caller_type);
};

/* Assign each device's index number */
enum SLEEP_MONITOR_DEVICE {
/* CAUTION!!! Need to sync with sleep_monitor_device  in sleep_monitor.c */
	SLEEP_MONITOR_BT = 0,
	SLEEP_MONITOR_WIFI = 1,
	SLEEP_MONITOR_WIFI1 = 2,
	SLEEP_MONITOR_IRQ = 3,
	SLEEP_MONITOR_BATTERY = 4,
	SLEEP_MONITOR_NFC = 5,
	SLEEP_MONITOR_SENSOR = 6,
	SLEEP_MONITOR_SENSOR1 = 7,
	SLEEP_MONITOR_AUDIO = 8,
	SLEEP_MONITOR_SAPA = 9,
	SLEEP_MONITOR_SAPA1 = 10,
	SLEEP_MONITOR_SAPB = 11,
	SLEEP_MONITOR_SAPB1 = 12,
	SLEEP_MONITOR_CONHR = 13,
	SLEEP_MONITOR_KEY = 14,
	SLEEP_MONITOR_DEV15 = 15,
	SLEEP_MONITOR_CPU_UTIL = 16,
	SLEEP_MONITOR_LCD = 17,
	SLEEP_MONITOR_TSP = 18,
	SLEEP_MONITOR_ROTARY = 19,
	SLEEP_MONITOR_REGULATOR = 20,
	SLEEP_MONITOR_REGULATOR1 = 21,
	SLEEP_MONITOR_PMDOMAINS = 22,
	SLEEP_MONITOR_CP = 23,
	SLEEP_MONITOR_CP1 = 24,
	SLEEP_MONITOR_MST = 25,
	SLEEP_MONITOR_CPUIDLE = 26,
	SLEEP_MONITOR_TEMP = 27,
	SLEEP_MONITOR_TEMPMAX = 28,
	SLEEP_MONITOR_TCP = 29,
	SLEEP_MONITOR_SYS_TIME = 30,
	SLEEP_MONITOR_RTC_TIME = 31,
	SLEEP_MONITOR_WS = 32,
	SLEEP_MONITOR_WS1 = 33,
	SLEEP_MONITOR_WS2 = 34,
	SLEEP_MONITOR_WS3 = 35,
	SLEEP_MONITOR_SLWL = 36,
	SLEEP_MONITOR_SLWL1 = 37,
	SLEEP_MONITOR_SLWL2 = 38,
	SLEEP_MONITOR_SLWL3 = 39,
	SLEEP_MONITOR_NUM_MAX,
};

/* Return device status from each device */
enum SLEEP_MONITOR_DEVICE_STATUS {
	DEVICE_ERR_1 = -1,
	DEVICE_POWER_OFF = 0,
	DEVICE_ON_LOW_POWER = 1,
	DEVICE_ON_ACTIVE1 = 2,
	DEVICE_ON_ACTIVE2 = 3,

	/* ADD HERE */

	DEVICE_UNKNOWN = 15,
};

/* Device return result whether via hw or not */
enum SLEEP_MONITOR_DEVICE_CALLER_TYPE {
	SLEEP_MONITOR_CALL_SUSPEND = 0,
	SLEEP_MONITOR_CALL_RESUME = 1,
	SLEEP_MONITOR_CALL_IRQ_LIST = 2,
	SLEEP_MONITOR_CALL_WS_LIST = 3,
	SLEEP_MONITOR_CALL_SLWL_LIST = 4,
	SLEEP_MONITOR_CALL_INIT = 5,
	SLEEP_MONITOR_CALL_POFF = 6,
	SLEEP_MONITOR_CALL_DUMP = 7,
	SLEEP_MONITOR_CALL_ETC = 8,
};

/* Device return result whether via hw or not */
enum SLEEP_MONITOR_DEVICE_CHECK_LEVEL {
	SLEEP_MONITOR_CHECK_SOFT = 0,
	SLEEP_MONITOR_CHECK_HARD = 10 ,
};

/* Define boolean value */
enum SLEEP_MONITOR_BOOLEAN {
	SLEEP_MONITOR_BOOLEAN_FALSE = 0,
	SLEEP_MONITOR_BOOLEAN_TRUE = 1,
};

#define SLEEP_MONITOR_DEBUG_PREFIX "[slp_mon]"
#define SLEEP_MONITOR_BIT_INT_SIZE 32
#define SLEEP_MONITOR_DEVICE_BIT_WIDTH 4
#define SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE \
	SLEEP_MONITOR_BIT_INT_SIZE/SLEEP_MONITOR_DEVICE_BIT_WIDTH
#define SLEEP_MONITOR_NUM_DEVICE_RAW_VAL 8
#define SLEEP_MONITOR_ONE_LINE_RAW_SIZE 90
#define SLEEP_MONITOR_AFTER_BOOTING_TIME 15 	/* sec */
#define SLEEP_MONITOR_MAX_RETRY_CNT 5

#define SLEEP_MONITOR_GROUP_SIZE \
	(((SLEEP_MONITOR_NUM_MAX) % ( SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE) == 0) \
	?((SLEEP_MONITOR_NUM_MAX) / ( SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE)) \
	:((SLEEP_MONITOR_NUM_MAX) / ( SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE) + 1))

#define SLEEP_MONITOR_STORE_BUFF 16*1024 /* 16K */
#define SLEEP_MONITOR_SUSPEND_RESUME_PAIR_BUFF 1335


#ifdef CONFIG_SLEEP_MONITOR
extern int sleep_monitor_register_ops(void *dev, struct sleep_monitor_ops *ops, int device_type);
extern int sleep_monitor_unregister_ops(int device_type);
extern int sleep_monitor_get_pretty(int *pretty_group, int type);
extern int sleep_monitor_get_raw_value(int *raw_value);
extern char* get_type_marker(int type);
extern void sleep_monitor_store_buf(char* buf, int ret, enum SLEEP_MONITOR_BOOLEAN);
extern void sleep_monitor_update_req(void);
#else
static inline int sleep_monitor_register_ops(void *dev, struct sleep_monitor_ops *ops, int device_type){}
static inline int sleep_monitor_unregister_ops(int device_type){}
static inline int sleep_monitor_get_pretty(int *pretty_group, int type){}
static inline int sleep_monitor_get_raw_value(int *raw_value){}
static char* get_type_marker(int type){}
static void sleep_monitor_store_buf(char* buf, int ret, enum SLEEP_MONITOR_BOOLEAN){}
static void sleep_monitor_update_req(void){}
#endif /* CONFIG_SLEEP_MONITOR */

#endif /* _SLEEP_MONITOR_H */
