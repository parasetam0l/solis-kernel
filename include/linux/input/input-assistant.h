#ifndef INPUT_ASSISTANT_H
#define INPUT_ASSISTANT_H

#ifdef CONFIG_SEC_DEBUG
extern int sec_debug_get_debug_level(void);
#endif

enum mkey_check_option {
	MKEY_CHECK_AUTO,
	MKEY_CHECK_AWAYS
};

struct input_assistant_mkey {
	enum mkey_check_option option;
	unsigned int type;
	unsigned int code;
};

struct input_assistant_mmap {
	struct input_assistant_mkey *mkey_map;
	unsigned int num_mkey;
};

struct input_assistant_pdata {
	struct input_assistant_mmap *mmap;
	unsigned int num_map;
	char **support_dev_name;
	unsigned int support_dev_num;
};

struct input_assistant_data {
	struct platform_device *pdev;
	struct input_assistant_pdata *pdata;
	struct wake_lock wake_lock;
};

struct input_assistant_tsp {
	bool update;
	int tracking_id;
	int finger_id;
	int x;
	int y;
	int z;
	int wmajor;
	int wminor;
	int palm;
};
#endif /* INPUT_ASSISTANT_H */
