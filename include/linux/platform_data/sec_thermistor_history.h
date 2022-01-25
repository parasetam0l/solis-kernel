#define SEC_THERM_NAME_LEN 12

#define SEC_THERM_TEMP_MIN -99
#define SEC_THERM_TEMP_MAX 999

struct sec_therm_history_info {
	int round;
	int max;
	int min;
    int cnt;
    int sum;
	int reset;
};

enum sec_therm_history_group {
	SEC_THERM_HISTORY_BOOT,
#ifdef CONFIG_ENERGY_MONITOR
	SEC_THERM_HISTORY_ENERGY,
#endif
#ifdef CONFIG_SLEEP_MONITOR
	SEC_THERM_HISTORY_SLEEP,
#endif
	MAX_SEC_THERM_GROUP_NUM
};

enum sec_therm_history_device {
	SEC_THERM_HISTORY_AP_THERM,
	SEC_THERM_HISTORY_BATT_THERM,
	SEC_THERM_HISTORY_CP_THERM,
	MAX_SEC_THERM_DEVICE_NUM
};

void sec_therm_history_remove(int device_num);

void sec_therm_history_update(int device_num, int temp);

void sec_therm_history_reset(struct sec_therm_history_info *history);

void sec_therm_history_device_init(int device_num, char* name);

int sec_therm_his_get_temp_cb(void *priv, unsigned int *raw, int chk_lv, int caller_type);


#ifdef CONFIG_ENERGY_MONITOR
int get_sec_therm_history_energy_mon(int type, struct sec_therm_history_info *eng_history);
#endif

extern char sec_therm_dev_name[MAX_SEC_THERM_DEVICE_NUM][SEC_THERM_NAME_LEN];
