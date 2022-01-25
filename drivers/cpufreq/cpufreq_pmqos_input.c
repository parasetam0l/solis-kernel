
#include <linux/input.h>

struct input_dev touch_booster;
static int touch_booster_state;
const char touch_booster_name[] = "TOUCH_BOOSTER";

struct input_dev rotary_booster;
const char rotary_booster_name[] = "ROTARY_BOOSTER";
static int touch_boost_initialized;

struct mutex tb_muxtex_lock;
struct delayed_work tb_work_off;
struct delayed_work tb_work_chg;

/* TOUCH */
#if defined(TRM_TOUCH_BOOSTER_EN)
static struct pm_qos_request touch_press_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request touch_move_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request touch_release_qos_array[NUMBER_OF_LOCK];
#endif

#if defined(TOUCH_WAKEUP_BOOSTER)
/* Touch Wakeup Booster */
static int touch_wakeup_initialized;
struct mutex touch_wakeup_booter_lock;
struct delayed_work touch_wakeup_booster_off_work;
static struct pm_qos_request pm_qos_cpufreq_touch_wakeup;
static struct pm_qos_request pm_qos_cpuonline_touch_wakeup;

extern int suspend_state;

void touch_wakeup_booster_turn_on(void)
{
	if (!suspend_state)
		return;

	if (!touch_wakeup_initialized)
		return;

	mutex_lock(&touch_wakeup_booter_lock);

	pr_info("%s\n", __func__);

	if (!pm_qos_request_active(&pm_qos_cpufreq_touch_wakeup)) {
		pm_qos_add_request(&pm_qos_cpufreq_touch_wakeup
			, PM_QOS_CLUSTER0_FREQ_MIN, touch_wakeup_min_cpu_freq);
		if (!pm_qos_request_active(&pm_qos_cpuonline_touch_wakeup)) {
			pm_qos_add_request(&pm_qos_cpuonline_touch_wakeup,
					PM_QOS_CPU_ONLINE_MIN, 2);
		}

		schedule_delayed_work(&touch_wakeup_booster_off_work,
				msecs_to_jiffies(TOUCH_WAKEUP_BOOSTER_DELAY));
	}

	mutex_unlock(&touch_wakeup_booter_lock);
}

static void touch_wakeup_off_work_func(struct work_struct *work)
{
	mutex_lock(&touch_wakeup_booter_lock);

	if (pm_qos_request_active(&pm_qos_cpufreq_touch_wakeup))
		pm_qos_remove_request(&pm_qos_cpufreq_touch_wakeup);

	if (pm_qos_request_active(&pm_qos_cpuonline_touch_wakeup))
		pm_qos_remove_request(&pm_qos_cpuonline_touch_wakeup);

	mutex_unlock(&touch_wakeup_booter_lock);

	pr_info("%s\n", __func__);
}
#endif

#if defined(HARD_KEY_WAKEUP_BOOSTER)
/* Hard Key Wakeup Booster */
static int hard_key_wakeup_initialized;
struct mutex hard_key_wakeup_booster_lock;
struct delayed_work hard_key_wakeup_booster_off_work;
static struct pm_qos_request pm_qos_cpufreq_hard_key_wakeup;
static struct pm_qos_request pm_qos_cpuonline_hard_key_wakeup;

extern int suspend_state;
int hard_key_wakeup_boosting = 0;

void hard_key_wakeup_booster_turn_on(void)
{
	if (!suspend_state)
		return;

	if (!hard_key_wakeup_initialized)
		return;

	mutex_lock(&hard_key_wakeup_booster_lock);

	hard_key_wakeup_boosting = 1;

	pr_info("%s\n", __func__);

	if (!pm_qos_request_active(&pm_qos_cpufreq_hard_key_wakeup)) {
		pm_qos_add_request(&pm_qos_cpufreq_hard_key_wakeup,
				PM_QOS_CLUSTER0_FREQ_MIN, hard_key_wakeup_min_cpu_freq);

		if (!pm_qos_request_active(&pm_qos_cpuonline_hard_key_wakeup)) {
			pm_qos_add_request(&pm_qos_cpuonline_hard_key_wakeup,
					PM_QOS_CPU_ONLINE_MIN, 2);
		}

		schedule_delayed_work(&hard_key_wakeup_booster_off_work,
				msecs_to_jiffies(KEY_BOOSTER_DELAY));
	}

	hard_key_wakeup_boosting = 0;

	mutex_unlock(&hard_key_wakeup_booster_lock);
}

static void hard_key_wakeup_off_work_func(struct work_struct *work)
{
	mutex_lock(&hard_key_wakeup_booster_lock);

	if (pm_qos_request_active(&pm_qos_cpufreq_hard_key_wakeup))
		pm_qos_remove_request(&pm_qos_cpufreq_hard_key_wakeup);

	if (pm_qos_request_active(&pm_qos_cpuonline_hard_key_wakeup))
		pm_qos_remove_request(&pm_qos_cpuonline_hard_key_wakeup);

	mutex_unlock(&hard_key_wakeup_booster_lock);

	pr_info("%s\n", __func__);
}
#endif

#if defined(BACK_KEY_BOOSTER)
/* Back Key Booster */
static int back_key_initialized;
struct mutex back_key_booster_lock;
struct delayed_work back_key_booster_off_work;
static struct pm_qos_request pm_qos_cpufreq_back_key;

void back_key_booster_turn_on(void)
{
	if (!back_key_initialized)
		return;

	mutex_lock(&back_key_booster_lock);

	pr_info("%s\n", __func__);

	if (!pm_qos_request_active(&pm_qos_cpufreq_back_key)) {
		pm_qos_add_request(&pm_qos_cpufreq_back_key,
				PM_QOS_CLUSTER0_FREQ_MIN, back_key_min_cpu_freq);

		schedule_delayed_work(&back_key_booster_off_work,
				msecs_to_jiffies(BACK_KEY_BOOSTER_DELAY));
	}

	mutex_unlock(&back_key_booster_lock);
}

static void back_key_off_work_func(struct work_struct *work)
{
	mutex_lock(&back_key_booster_lock);

	if (pm_qos_request_active(&pm_qos_cpufreq_back_key))
		pm_qos_remove_request(&pm_qos_cpufreq_back_key);

	mutex_unlock(&back_key_booster_lock);

	pr_info("%s\n", __func__);
}
#endif

#if defined(ROTARY_BOOSTER)
#include <linux/pm_qos.h>

struct delayed_work rotary_booster_off_work;
static struct pm_qos_request pm_qos_rotary_cpufreq;

struct mutex rotary_dvfs_lock;
struct mutex rotary_off_dvfs_lock;
static int rotary_initialized;

static void rotary_off_work_func(struct work_struct *work)
{
	mutex_lock(&rotary_off_dvfs_lock);

	if (pm_qos_request_active(&pm_qos_rotary_cpufreq))
		pm_qos_remove_request(&pm_qos_rotary_cpufreq);

	mutex_unlock(&rotary_off_dvfs_lock);
}

void rotary_booster_turn_on(void)
{
	if (!rotary_initialized)
		return;

#if defined(CONFIG_SLP_INPUT_REC)
	__slp_store_input_history(&rotary_booster, ROTORY_BOOSTER_TURN, 0, 0);
#endif

#if defined(ROTARY_BOOSTER)
	mutex_lock(&rotary_dvfs_lock);

	if (!pm_qos_request_active(&pm_qos_rotary_cpufreq)) {
		pm_qos_add_request(&pm_qos_rotary_cpufreq,
				PM_QOS_CLUSTER0_FREQ_MIN, rotary_min_cpu_freq);

		schedule_delayed_work(&rotary_booster_off_work,
				msecs_to_jiffies(ROTARY_BOOSTER_DELAY));
	} else {
		cancel_delayed_work_sync(&rotary_booster_off_work);
		schedule_delayed_work(&rotary_booster_off_work,
				msecs_to_jiffies(ROTARY_BOOSTER_DELAY));
	}

	mutex_unlock(&rotary_dvfs_lock);
#endif

}
#endif

void touch_booster_move(struct work_struct *work)
{
	if (!touch_boost_initialized)
		return;

#if defined(CONFIG_SLP_INPUT_REC)
	__slp_store_input_history(&touch_booster, TOUCH_BOOSTER_MOVE, 0, 0);
#endif

	if (touch_booster_state == TOUCH_BOOSTER_PRESS) {
		mutex_lock(&tb_muxtex_lock);

		touch_booster_move_sub();

		mutex_unlock(&tb_muxtex_lock);
	}
}

void touch_booster_press(void)
{
	if (!touch_boost_initialized)
		return;

#if defined(CONFIG_SLP_INPUT_REC)
	__slp_store_input_history(&touch_booster, TOUCH_BOOSTER_PRESS, 0, 0);
#endif

	mutex_lock(&tb_muxtex_lock);
	cancel_delayed_work(&tb_work_off);

	if ((touch_booster_state == TOUCH_BOOSTER_RELEASE)
			&& (cpufreq_get_touch_boost_en() == 1)) {
		touch_booster_press_sub();
		schedule_delayed_work(&tb_work_chg,
				msecs_to_jiffies(TOUCH_BOOSTER_CHG_TIME));


		touch_booster_state = TOUCH_BOOSTER_PRESS;
	}

	mutex_unlock(&tb_muxtex_lock);

}

void touch_booster_release(void)
{
	if (!touch_boost_initialized)
		return;

#if defined(CONFIG_SLP_INPUT_REC)
	__slp_store_input_history(&touch_booster, TOUCH_BOOSTER_RELEASE, 0, 0);
#endif

	if (touch_booster_state == TOUCH_BOOSTER_PRESS) {
		touch_booster_release_sub();

		cancel_delayed_work(&tb_work_chg);
		schedule_delayed_work(&tb_work_off,
			msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
	}
}

void touch_booster_off(struct work_struct *work)
{
	if (!touch_boost_initialized)
		return;

	mutex_lock(&tb_muxtex_lock);

	touch_booster_off_sub();
	touch_booster_state = TOUCH_BOOSTER_RELEASE;

	mutex_unlock(&tb_muxtex_lock);
}

void touch_booster_release_all(void)
{
	if (!touch_boost_initialized)
		return;

#if defined(CONFIG_SLP_INPUT_REC)
	__slp_store_input_history(&touch_booster, TOUCH_BOOSTER_RELEASE_ALL, 0, 0);
#endif

	mutex_lock(&tb_muxtex_lock);

	cancel_delayed_work(&tb_work_off);
	cancel_delayed_work(&tb_work_chg);
	schedule_work(&tb_work_off.work);

	mutex_unlock(&tb_muxtex_lock);
}

#if defined(TRM_TOUCH_BOOSTER_EN)
static bool  touch_boost_en_value = 1;
static ssize_t show_touch_boost_en(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;

	ret =  sprintf(buf, "%d\n", touch_boost_en_value);

	return ret;
}

static ssize_t store_touch_boost_en(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{

	int input;
	input = atoi(buf);

	if ((input == 0 ) || (input == 1 ))
		touch_boost_en_value = input;

	return count;
}


bool cpufreq_get_touch_boost_en(void)
{
	return touch_boost_en_value;
}

static unsigned int touch_boost_press_value = PM_QOS_TOUCH_PRESS_DEFAULT_VALUE;

static ssize_t show_touch_boost_press(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int ret = 0;

	ret =  sprintf(buf, "%d\n", cpufreq_get_touch_boost_press());

	return ret;
}

static ssize_t store_touch_boost_press(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	set_pmqos_data(touch_press_qos_array, PM_QOS_TOUCH_PRESS, buf);

	return count;
}

unsigned int cpufreq_get_touch_boost_press(void)
{
	touch_boost_press_value = pm_qos_request(PM_QOS_TOUCH_PRESS);

	return touch_boost_press_value;
}

static unsigned int touch_boost_move_value = PM_QOS_TOUCH_MOVE_DEFAULT_VALUE;

static ssize_t show_touch_boost_move(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int ret = 0;

	ret = sprintf(buf, "%d\n", cpufreq_get_touch_boost_move());

	return ret;
}

static ssize_t store_touch_boost_move(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{

	set_pmqos_data(touch_move_qos_array, PM_QOS_TOUCH_MOVE, buf);

	return count;
}

unsigned int cpufreq_get_touch_boost_move(void)
{
	touch_boost_move_value = pm_qos_request(PM_QOS_TOUCH_MOVE);

	return touch_boost_move_value;
}

static unsigned int touch_boost_release_value = PM_QOS_TOUCH_RELEASE_DEFAULT_VALUE;

static ssize_t show_touch_boost_release(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int ret = 0;

	ret =  sprintf(buf, "%d\n", cpufreq_get_touch_boost_release());

	return ret;
}

static ssize_t store_touch_boost_release(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{
	set_pmqos_data(touch_release_qos_array, PM_QOS_TOUCH_RELEASE, buf);

	return count;
}

unsigned int cpufreq_get_touch_boost_release(void)
{
	touch_boost_release_value = pm_qos_request(PM_QOS_TOUCH_RELEASE);

	return touch_boost_release_value;
}

static int touch_cpu_online_min_value;

unsigned int touch_cpu_get_online_min(void)
{
	return touch_cpu_online_min_value;
}

static ssize_t show_touch_cpu_online_min(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	unsigned int ret = 0;

	ret =  sprintf(buf, "%d\n", touch_cpu_get_online_min());

	return ret;
}

static ssize_t __ref store_touch_cpu_online_min(struct kobject *a, struct attribute *b,
		const char *buf, size_t count)
{
	int lock_value = 0;

	if (strstr(buf, "-1")!=NULL)
		lock_value = -1;
	else
		lock_value = atoi(buf);

	if ((lock_value > 0) && (lock_value <= NR_CPUS))
		touch_cpu_online_min_value = lock_value;
	else if (lock_value == -1)
		touch_cpu_online_min_value = lock_value;

	return count;
}
#endif

#if defined(TRM_TOUCH_BOOSTER_EN)
define_one_root_rw(touch_boost_en);
define_one_root_rw(touch_boost_press);
define_one_root_rw(touch_boost_move);
define_one_root_rw(touch_boost_release);
define_one_root_rw(touch_cpu_online_min);
#endif

void input_booster_init(void)
{
	/* STEP 1 : TOUCH BOOSTER INIT */
	touch_booster.name = touch_booster_name;
	mutex_init(&tb_muxtex_lock);

	INIT_DELAYED_WORK(&tb_work_off, touch_booster_off);
	INIT_DELAYED_WORK(&tb_work_chg, touch_booster_move);
	touch_booster_state = TOUCH_BOOSTER_RELEASE;
	touch_boost_initialized = 1;

	/* STEP 2 : ROTARY BOOSTER INIT */
#if defined(ROTARY_BOOSTER)
	rotary_booster.name = rotary_booster_name;
	mutex_init(&rotary_dvfs_lock);
	mutex_init(&rotary_off_dvfs_lock);
	INIT_DELAYED_WORK(&rotary_booster_off_work, rotary_off_work_func);
	rotary_initialized = 1;
#endif

#if defined(BACK_KEY_BOOSTER)
	mutex_init(&back_key_booster_lock);
	INIT_DELAYED_WORK(&back_key_booster_off_work, back_key_off_work_func);
	back_key_initialized = 1;
#endif

#if defined(HARD_KEY_WAKEUP_BOOSTER)
	mutex_init(&hard_key_wakeup_booster_lock);
	INIT_DELAYED_WORK(&hard_key_wakeup_booster_off_work, hard_key_wakeup_off_work_func);
	hard_key_wakeup_initialized = 1;
#endif

#if defined(TOUCH_WAKEUP_BOOSTER)
	mutex_init(&touch_wakeup_booter_lock);
	INIT_DELAYED_WORK(&touch_wakeup_booster_off_work, touch_wakeup_off_work_func);
	touch_wakeup_initialized = 1;
#endif
}
