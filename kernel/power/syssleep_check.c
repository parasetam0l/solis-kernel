#include <linux/syscore_ops.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/suspend.h>
#include "power.h"

#define SYSSLEEP_TIMEOUT (3)
#define SYSSLEEP_MONITOR_TIME (100)

static struct timer_list syssleep_timer;

static bool syssleep_check_flag;

static int prev_suspend_count;
static struct timespec prev_boot_time;

static void syssleep_timer_handler(unsigned long data)
{
	pr_err("%s: fail to suspend during %d sec.\n", __func__, SYSSLEEP_TIMEOUT);

	pm_print_active_wakeup_sources();
}

ssize_t show_syssleep_check(char *buf)
{
	struct timespec boot_time, sub_time;
	int suspend_count;

	get_monotonic_boottime(&boot_time);
	sub_time = timespec_sub(boot_time, prev_boot_time);

	suspend_count = suspend_stats.success;

	pr_info("%s: flag[%d], sus_cnt[%d] - prev_sus_cnt[%d] = [%d], "
		"during %3lu.%03lu secs.\n", __func__,
		syssleep_check_flag, suspend_count, prev_suspend_count,
		suspend_count - prev_suspend_count,
		sub_time.tv_sec, sub_time.tv_nsec/NSEC_PER_MSEC);

	return sprintf(buf, "flag[%d], sus_cnt[%d] - prev_sus_cnt[%d] = [%d], "
			"during %3lu.%03lu secs.\n",
			syssleep_check_flag, suspend_count, prev_suspend_count,
			suspend_count - prev_suspend_count,
			sub_time.tv_sec, sub_time.tv_nsec/NSEC_PER_MSEC);
}

int store_syssleep_check(const char *buf)
{
	pr_info("%s : %s\n", __func__, (buf)?buf:"(NULL)");

	syssleep_check_flag = true;
	prev_suspend_count = suspend_stats.success;
	del_timer(&syssleep_timer);

	get_monotonic_boottime(&prev_boot_time);

	syssleep_timer.expires = jiffies + SYSSLEEP_TIMEOUT * HZ;
	add_timer(&syssleep_timer);

	return 0;
}

static void syssleep_check_syscore_resume(void)
{
	struct timespec boot_time, sub_time;
	int suspend_count;

	if (!syssleep_check_flag)
		return;

	del_timer(&syssleep_timer);

	get_monotonic_boottime(&boot_time);
	sub_time = timespec_sub(boot_time, prev_boot_time);

	suspend_count = suspend_stats.success;

	pr_info("%s : %dth resume. (after %3lu.%03lu sec)\n",
			__func__, suspend_count - prev_suspend_count,
			sub_time.tv_sec, sub_time.tv_nsec/NSEC_PER_MSEC);

	if (sub_time.tv_sec >= SYSSLEEP_MONITOR_TIME) {
		pr_info("%s : Stop monitoring.\n", __func__);
		syssleep_check_flag = false;
	}
}

static int syssleep_check_syscore_suspend(void)
{
	struct timespec boot_time, sub_time;
	int suspend_count;

	if (!syssleep_check_flag)
		return 0;

	get_monotonic_boottime(&boot_time);
	sub_time = timespec_sub(boot_time, prev_boot_time);

	suspend_count = suspend_stats.success;

	pr_info("%s : %dth suspend. (after %3lu.%03lu sec)\n",
			__func__, suspend_count - prev_suspend_count,
			sub_time.tv_sec, sub_time.tv_nsec/NSEC_PER_MSEC);

	if (sub_time.tv_sec >= SYSSLEEP_MONITOR_TIME) {
		pr_info("%s : Stop monitoring.\n", __func__);
		syssleep_check_flag = false;
	}

	return 0;
}

static struct syscore_ops syssleep_check_syscore_ops = {
	.suspend = syssleep_check_syscore_suspend,
	.resume = syssleep_check_syscore_resume,
};

static int syssleep_check_syscore_init(void)
{
	memset(&syssleep_timer, 0, sizeof(struct timer_list));
	register_syscore_ops(&syssleep_check_syscore_ops);

	init_timer(&syssleep_timer);
	syssleep_timer.function = syssleep_timer_handler;
	syssleep_timer.data = 0;

	return 0;
}

static void syssleep_check_syscore_exit(void)
{
	del_timer(&syssleep_timer);
	unregister_syscore_ops(&syssleep_check_syscore_ops);
}

module_init(syssleep_check_syscore_init);
module_exit(syssleep_check_syscore_exit);
