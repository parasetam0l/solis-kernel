
#include <linux/cpuidle.h>

#if defined(ROTARY_BOOSTER)
#define ROTARY_BOOSTER_DELAY        200
static int rotary_min_cpu_freq = 757000;
#endif

#if defined(BACK_KEY_BOOSTER)
static int back_key_min_cpu_freq = 600000;
#define BACK_KEY_BOOSTER_DELAY      200
#endif

#if defined(HARD_KEY_WAKEUP_BOOSTER)
static int hard_key_wakeup_min_cpu_freq = 1000000;
#define KEY_BOOSTER_DELAY           200
#endif

#if defined(TOUCH_WAKEUP_BOOSTER)
static int touch_wakeup_min_cpu_freq = 1000000;
#define TOUCH_WAKEUP_BOOSTER_DELAY  200
#endif

#define TOUCH_BOOSTER_OFF_TIME      100
#define TOUCH_BOOSTER_CHG_TIME      200

static struct pm_qos_request pm_qos_cpu_req;
static struct pm_qos_request pm_qos_cpu_online_req;

unsigned int press_cpu_freq, release_cpu_freq;
int touch_cpu_online;

void touch_booster_press_sub(void)
{
	press_cpu_freq   = cpufreq_get_touch_boost_press();
	touch_cpu_online = touch_cpu_get_online_min();

	if (!pm_qos_request_active(&pm_qos_cpu_req))
		pm_qos_add_request(&pm_qos_cpu_req,
				PM_QOS_CLUSTER0_FREQ_MIN, press_cpu_freq);

	if (touch_cpu_online > 1) {
		if (!pm_qos_request_active(&pm_qos_cpu_online_req))
			pm_qos_add_request(&pm_qos_cpu_online_req,
					PM_QOS_CPU_ONLINE_MIN, touch_cpu_online);
	}

	cpuidle_set_sicd_en(0);
}

void touch_booster_move_sub(void)
{
	unsigned int move_cpu_freq = cpufreq_get_touch_boost_move();

	if (pm_qos_request_active(&pm_qos_cpu_req))
		pm_qos_update_request(&pm_qos_cpu_req, move_cpu_freq);
}

void touch_booster_release_sub(void)
{
	release_cpu_freq = cpufreq_get_touch_boost_release();

	if (pm_qos_request_active(&pm_qos_cpu_req))
		pm_qos_update_request(&pm_qos_cpu_req, release_cpu_freq);
}

void touch_booster_off_sub(void)
{
	if (pm_qos_request_active(&pm_qos_cpu_req))
		pm_qos_remove_request(&pm_qos_cpu_req);

	if (pm_qos_request_active(&pm_qos_cpu_online_req))
		pm_qos_remove_request(&pm_qos_cpu_online_req);

	cpuidle_set_sicd_en(1);
}
