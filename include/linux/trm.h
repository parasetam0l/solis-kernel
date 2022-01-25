#ifndef _TRM_H_
#define _TRM_H_

#include <linux/pm_qos.h>
#include <linux/input.h>

#define TRM_INPUT_BOOSTER_EN     1
#define ROTARY_BOOSTER           1

#if defined(BUILD_ERROR)
#define HARD_KEY_WAKEUP_BOOSTER  1
#define TOUCH_WAKEUP_BOOSTER     1
#endif

#if defined(CONFIG_MACH_VOLT)
#define BACK_KEY_BOOSTER         1
#endif

#if defined(TRM_INPUT_BOOSTER_EN)
#define TRM_TOUCH_BOOSTER_EN     1
#endif

enum {
	KERNEL_RESERVED00,
	KERNEL_RESERVED01,

	NUMBER_OF_LOCK = (KERNEL_RESERVED01 + 50),
};

#if defined(TRM_TOUCH_BOOSTER_EN)
enum {
	TOUCH_BOOSTER_NONE,
	TOUCH_BOOSTER_PRESS,
	TOUCH_BOOSTER_MOVE,
	TOUCH_BOOSTER_RELEASE,
	TOUCH_BOOSTER_RELEASE_ALL,
	TOUCH_BOOSTER_OFF,
};

extern struct input_dev touch_booster;
extern const char touch_booster_name[];
#endif

#if defined(ROTARY_BOOSTER)
enum {
	ROTORY_BOOSTER_TURN,
};

extern struct input_dev rotary_booster;
extern const char rotary_booster_name[];
#endif

int set_pmqos_data(struct pm_qos_request *any_qos_array, int pmqos_type, const char *buf);

bool cpufreq_get_touch_boost_en(void);
unsigned int cpufreq_get_touch_boost_press(void);
unsigned int cpufreq_get_touch_boost_move(void);
unsigned int cpufreq_get_touch_boost_release(void);

unsigned int touch_cpu_get_online_min(void);

void cpufreq_release_all_cpu_lock(int lock_type);

unsigned int cpu_gov_get_up_level(void);
unsigned int cpu_freq_get_threshold(void);

void touch_booster_press(void);
void touch_booster_release(void);
void touch_booster_release_all(void);

void rotary_booster_turn_on(void);
void back_key_booster_turn_on(void);
void hard_key_wakeup_booster_turn_on(void);
void touch_wakeup_booster_turn_on(void);

#if defined (CONFIG_SLP_BUSY_LEVEL)
void update_cpu_busy_level(void);
#endif

#endif /* _TRM_H_ */
