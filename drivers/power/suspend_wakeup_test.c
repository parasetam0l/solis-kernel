#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <linux/wakelock.h>

#define SUSPEND_WAKEUP_TEST_VERSION 0x00000001
/* select alarm api*/
//#define SELECT_ALARM_ANDROID_ORIENTED 1
#define SELECT_ALARM_LINUXORG_ORIENTED 1

#ifdef SELECT_ALARM_ANDROID_ORIENTED
#include <asm-generic/uaccess.h>
#include <linux/android_alarm.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#endif

#ifdef SELECT_ALARM_LINUXORG_ORIENTED
#include <linux/alarmtimer.h>
#endif

typedef struct suspend_wakeup_test{
	int interval;
	int count;
	int iteration;
	int is_run;
	struct alarm polling_alarm;
	struct wake_lock w_lock;
	struct delayed_work wakeup_work;
}suspend_wakeup;

suspend_wakeup sw;
#define LOCK_TIME 1000 /* msec */
#define INTERVAL_SEC 20
#define REPEAT_COUNT 100000000
#define GURANTEE_STABILITY_TIME 180

//#define USE_KEY_EVENT 1
//#define RUN_FROM_BOOT 1

#ifdef CONFIG_SOLIS
//#define SUSPEND_BLOCKER
#endif

#ifdef USE_KEY_EVENT
extern void gpio_keys_send_fake_powerkey(int);
#endif

unsigned int sleep_count;
unsigned int wakeup_count;

int start_test_alarm(void)
{
#ifdef SELECT_ALARM_LINUXORG_ORIENTED
        int ret = 0;
	ret = alarm_start(&sw.polling_alarm,
                    ktime_add(ktime_get_boottime(), ktime_set(sw.interval, 0)));
        if (ret < 0) {
		pr_info("%s - alarm start error!\n", __func__);
		return -1;
        }
#endif

#ifdef SELECT_ALARM_ANDROID_ORIENTED
	ktime_t low_interval = ktime_set(sw.interval, 0);
        ktime_t slack = ktime_set(10, 0);
        ktime_t next;

        next = ktime_add(alarm_get_elapsed_realtime(), low_interval);
        alarm_start_range(&sw.polling_alarm,next, ktime_add(next, slack));
#endif

#ifdef USE_KEY_EVENT
	gpio_keys_send_fake_powerkey(1);
	gpio_keys_send_fake_powerkey(0);
#endif
	return 0;
}

static void suspend_wakeup_work(struct work_struct *work)
{
	if (sw.count >= sw.iteration) {
		pr_info("%s - suspend_wakeup test done\n", __func__);

		/* Initialize variables */
		sw.count = 0;
		sw.is_run = 0;

		return ;
	}
	wake_lock_timeout(&sw.w_lock,msecs_to_jiffies(LOCK_TIME));
	 if (start_test_alarm() < 0)
		pr_info("start_test_alarm error\n");
}

#ifdef SELECT_ALARM_LINUXORG_ORIENTED
static enum alarmtimer_restart suspend_wakeup_alarm(struct alarm *alarm, ktime_t now)
{
	if (0 == sw.count) {
		sleep_count = 0;
		wakeup_count = 0;
	}
	pr_info("%s - count : %d\n", __func__, ++(sw.count));
	suspend_wakeup_work((struct work_struct *)&sw.wakeup_work);
	return ALARMTIMER_NORESTART;
}
#endif

#ifdef SELECT_ALARM_ANDROID_ORIENTED
void suspend_wakeup_alarm(struct alarm *alarm)
{
        if (0 == sw.count) {
		sleep_count = 0;
		wakeup_count = 0;
	}
	pr_info("%s - count : %d\n", __func__, ++(sw.count));
	suspend_wakeup_work((struct work_struct *)&sw.wakeup_work);
}
#endif

static void init_alarm(void)
{
	sw.interval = INTERVAL_SEC;
	sw.iteration = REPEAT_COUNT;
	wake_lock_init(&sw.w_lock, WAKE_LOCK_SUSPEND, "suspend_wake_test");

#ifdef SUSPEND_BLOCKER
	wake_lock(&sw.w_lock);
#endif

#ifdef SELECT_ALARM_LINUXORG_ORIENTED
	alarm_init(&sw.polling_alarm, ALARM_BOOTTIME,suspend_wakeup_alarm);
#endif

#ifdef SELECT_ALARM_ANDROID_ORIENTED
	alarm_init(&sw.polling_alarm, ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,suspend_wakeup_alarm);
#endif
}

static ssize_t suspend_wakeup_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0)
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", sw.is_run);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);

	return ret;
}

static ssize_t suspend_wakeup_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int temp = 0;
	sscanf(user_buf, "%d", &temp);
	if (temp != sw.is_run) {
		sw.is_run = temp;
		if (sw.is_run) {
			/* Initialize variables */
			sw.count = 0;
			suspend_wakeup_work((struct work_struct *)&sw.wakeup_work);
		}
		else {
			alarm_cancel(&sw.polling_alarm);
		}
	}
	return count;
}
static const struct file_operations enable_fops = {
	.owner = THIS_MODULE,
	.read = suspend_wakeup_read,
	.write = suspend_wakeup_write,
};

static int __init suspend_wakeup_test_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("suspend_wakeup", NULL);
	if (d) {
		if (!debugfs_create_file("enable", 0644
			, d, NULL, &enable_fops))   \
				pr_err("%s : debugfs_create_file, error\n", "enable");

		debugfs_create_u32("wakeup_interval", 0644, d, &sw.interval);
		debugfs_create_u32("wakeup_iteration", 0644, d, &sw.iteration);
		debugfs_create_u32("suspend_count", 0644, d, &sleep_count);
		debugfs_create_u32("wakeup_count", 0644, d, &wakeup_count);

	}
	init_alarm();
#ifdef RUN_FROM_BOOT
{
#ifdef SELECT_ALARM_LINUXORG_ORIENTED
	sw.is_run = RUN_FROM_BOOT;
	alarm_start(&sw.polling_alarm,
			ktime_add(ktime_get_boottime(), ktime_set(GURANTEE_STABILITY_TIME,0)));
#endif

#ifdef SELECT_ALARM_ANDROID_ORIENTED
	ktime_t next;
	ktime_t slack = ktime_set(10, 0);
	ktime_t low_interval = ktime_set(GURANTEE_STABILITY_TIME, 0);
	sw.is_run = RUN_FROM_BOOT;
	next = ktime_add(alarm_get_elapsed_realtime(), low_interval);
	alarm_start_range(&sw.polling_alarm,next, ktime_add(next, slack));
#endif
}
#endif
	return 0;
}

static void __exit suspend_wakeup_test_exit(void)
{
	pr_info("suspend_wakeup_test_exit call\n");
	alarm_cancel(&sw.polling_alarm);
	cancel_delayed_work_sync(&sw.wakeup_work);
}

module_init(suspend_wakeup_test_init);
module_exit(suspend_wakeup_test_exit);
