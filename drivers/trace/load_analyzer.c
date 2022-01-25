/*
 *  Copyright (C)  2011 Samsung Electronics co. ltd
 *    Yong-U Baek <yu.baek@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/vmalloc.h>
#include <linux/cpuidle.h>
#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define CONFIG_LOAD_ANALYZER_INTERNAL	1
#include <linux/load_analyzer.h>

#if defined(CONFIG_SLP_CHECK_BUS_LOAD) && defined(CONFIG_CLK_MON)
#include <linux/clk_mon.h>
#endif

#include "load_analyzer_util.c"
#include "load_analyzer_cpu.c"
#include "load_analyzer_exynos7270.c"

#if defined (CONFIG_SLP_CURRENT_MONITOR)
#include "load_analyzer_current.c"
#endif

#if defined (CONFIG_CHECK_WORK_HISTORY)
#include "load_analyzer_workqueue.c"
#endif

#if defined (CONFIG_SLP_CHECK_PHY_ADDR)
#include "load_analyzer_addr.c"
#endif

#if defined (CONFIG_SLP_MINI_TRACER)
#include "load_analyzer_mini.c"
#endif

#if defined (CONFIG_SLP_CPU_TESTER)
#include "load_analyzer_cputester.c"
#endif

#if defined (CONFIG_SLP_INPUT_REC)
#include "load_analyzer_input.c"
#endif

enum {
	NORMAL_MODE,
	RECOVERY_MODE,
};

static int before_cpu_task_history_num;
static int before_cpu_load_history_num;

#if defined(CONFIG_SLP_INPUT_REC)
static int before_input_rec_history_num;
#endif

static int cpu_load_analyzer_init(int mode)
{
	if (mode == RECOVERY_MODE) {
		cpu_task_history_num = before_cpu_task_history_num;
		cpu_load_history_num = before_cpu_load_history_num;
#if defined(CONFIG_SLP_INPUT_REC)
		input_rec_history_num = before_input_rec_history_num;
#endif
	}

	cpu_load_freq_history = vmalloc(CPU_LOAD_FREQ_HISTORY_SIZE);
	if (cpu_load_freq_history == NULL)
		return -ENOMEM;

	cpu_load_freq_history_view = vmalloc(CPU_LOAD_FREQ_HISTORY_SIZE);
	if (cpu_load_freq_history_view == NULL)
		return -ENOMEM;

	cpu_task_history = vmalloc(CPU_TASK_HISTORY_SIZE);
	if (cpu_task_history == NULL)
		return -ENOMEM;

	cpu_task_history_view = vmalloc(CPU_TASK_HISTORY_SIZE);
	if (cpu_task_history_view == NULL)
		return -ENOMEM;

#if defined (CONFIG_CHECK_WORK_HISTORY)
	cpu_work_history =	vmalloc(CPU_WORK_HISTORY_SIZE);
	if (cpu_work_history == NULL)
		return -ENOMEM;

	cpu_work_history_view = vmalloc(CPU_WORK_HISTORY_SIZE);
	if (cpu_work_history_view == NULL)
		return -ENOMEM;
#endif

#if defined (CONFIG_SLP_INPUT_REC)
	input_rec_history =	vmalloc(INPUT_REC_HISTORY_SIZE);
	if (input_rec_history == NULL)
		return -ENOMEM;

	input_rec_history_view = vmalloc(INPUT_REC_HISTORY_SIZE);
	if (input_rec_history_view == NULL)
		return -ENOMEM;
#endif

	if (store_killed_init() != 0)
		return -ENOMEM;

	memset(cpu_load_freq_history, 0, CPU_LOAD_FREQ_HISTORY_SIZE);
	memset(cpu_load_freq_history_view, 0, CPU_LOAD_FREQ_HISTORY_SIZE);
	memset(cpu_task_history, 0, CPU_TASK_HISTORY_SIZE);
	memset(cpu_task_history_view, 0, CPU_TASK_HISTORY_SIZE);

#if defined (CONFIG_CHECK_WORK_HISTORY)
	memset(cpu_work_history, 0, CPU_WORK_HISTORY_SIZE);
	memset(cpu_work_history_view, 0, CPU_WORK_HISTORY_SIZE);
#endif
	store_killed_memset();

#if 0
	pr_info("LA size %d %d %d sum %d", CPU_LOAD_FREQ_HISTORY_SIZE,
		CPU_TASK_HISTORY_SIZE, CPU_WORK_HISTORY_SIZE,
		(CPU_LOAD_FREQ_HISTORY_SIZE \
		+ CPU_TASK_HISTORY_SIZE + CPU_WORK_HISTORY_SIZE) * 2);
#endif

#if defined (CONFIG_CHECK_WORK_HISTORY)
	cpu_work_history_onoff = 1;
#endif

#if defined (CONFIG_SLP_INPUT_REC)
	input_rec_history_onoff = 1;
#endif
	cpu_task_history_onoff = 1;

	saved_task_info_onoff = 1;

	return 0;
}

static int cpu_load_analyzer_exit(void)
{
	before_cpu_task_history_num = cpu_task_history_num;
	before_cpu_load_history_num = cpu_load_history_num;

#if defined(CONFIG_SLP_INPUT_REC)
	before_input_rec_history_num = input_rec_history_num;
#endif

	cpu_task_history_onoff = 0;

#if defined (CONFIG_CHECK_WORK_HISTORY)
	cpu_work_history_onoff = 0;
#endif
#if defined (CONFIG_SLP_INPUT_REC)
	input_rec_history_onoff = 0;
#endif
	saved_task_info_onoff = 0;

	msleep(1); /* to prevent wrong access to released memory */

	if (cpu_load_freq_history != NULL)
		vfree(cpu_load_freq_history);

	if (cpu_load_freq_history_view != NULL)
		vfree(cpu_load_freq_history_view);

	if (cpu_task_history != NULL)
		vfree(cpu_task_history);
	if (cpu_task_history_view != NULL)
		vfree(cpu_task_history_view);

#if defined (CONFIG_CHECK_WORK_HISTORY)
	if (cpu_work_history != NULL)
		vfree(cpu_work_history);

	if (cpu_work_history_view != NULL)
		vfree(cpu_work_history_view);
#endif

#if defined (CONFIG_SLP_INPUT_REC)
	if (input_rec_history != NULL)
		vfree(input_rec_history);

	if (input_rec_history_view != NULL)
		vfree(input_rec_history_view);
#endif

	store_killed_exit();

	return 0;
}

enum {
	SCHED_HISTORY_NUM,
	INPUT_REC_NUM,
	GOVERNOR_HISTORY_NUM,
};

static int load_analyzer_reset(int mode, int value)
{
	int ret = 0;

	cpu_load_analyzer_exit();

	switch (mode) {
		case SCHED_HISTORY_NUM:
			cpu_task_history_num = value;
			break;
#if defined(CONFIG_SLP_INPUT_REC)
		case INPUT_REC_NUM:
			input_rec_history_num = value;
			break;
#endif
		case GOVERNOR_HISTORY_NUM:
			cpu_load_history_num = value;
			break;

		default:
			break;
	}

	ret = cpu_load_analyzer_init(NORMAL_MODE);
	if (ret < 0) {
		cpu_load_analyzer_init(RECOVERY_MODE);
		return -EINVAL;
	}

	return 0;
}

static int saved_load_analyzer_data_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", cpu_load_history_num);

	return ret;
}

static ssize_t saved_load_analyzer_data_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,
			saved_load_analyzer_data_read_sub);

	return size_for_copy;
}

#define CPU_LOAD_SCHED_NUM_MIN	1000
#define CPU_LOAD_SCHED_NUM_MAX	1000000
static ssize_t saved_sched_num_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int saved_sched_num = 0;

	saved_sched_num = atoi(user_buf);
	if ((saved_sched_num < CPU_LOAD_SCHED_NUM_MIN)
		&& (saved_sched_num > CPU_LOAD_SCHED_NUM_MAX)) {

		pr_info("Wrong range value saved_sched_num = %d\n", saved_sched_num);

		return -EINVAL;
	}

	if (load_analyzer_reset(SCHED_HISTORY_NUM, saved_sched_num) < 0)
		return -EINVAL;

	return count;
}


static int saved_sched_num_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", cpu_task_history_num);

	return ret;
}

static ssize_t saved_sched_num_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
						,saved_sched_num_read_sub);

	return size_for_copy;
}

#if defined(CONFIG_SLP_INPUT_REC)
#define INPUT_REC_NUM_MIN	1000
#define INPUT_REC_NUM_MAX	1000000
static ssize_t saved_input_rec_num_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int saved_input_rec_num = 0;

	saved_input_rec_num = atoi(user_buf);
	if ((saved_input_rec_num < INPUT_REC_NUM_MIN)
		&& (saved_input_rec_num > INPUT_REC_NUM_MAX)) {

		pr_info("Wrong range value saved_input_rec_num = %d\n", saved_input_rec_num);
		return -EINVAL;
	}

	if (load_analyzer_reset(INPUT_REC_NUM, saved_input_rec_num) < 0)
		return -EINVAL;

	return count;
}

static int saved_input_rec_num_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", input_rec_history_num);

	return ret;
}

static ssize_t saved_input_rec_num_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos
						,saved_input_rec_num_read_sub);

	return size_for_copy;
}
#endif

#define CPU_HISTORY_NUM_MIN	1000
#define CPU_HISTORY_NUM_MAX	8000
static ssize_t saved_load_analyzer_data_write(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	int history_num;

	history_num = atoi(user_buf);

	if ((history_num < CPU_HISTORY_NUM_MIN)
		&& (history_num > CPU_HISTORY_NUM_MAX)) {

		pr_info("Wrong range value cpu_load_history_num = %d\n", history_num);

		return -EINVAL;
	}

	if (load_analyzer_reset(GOVERNOR_HISTORY_NUM, history_num) < 0)
		return -EINVAL;

	return count;
}

static int active_app_pid_read_sub(char *buf, int buf_size)
{
	int ret = 0;
	char task_name[TASK_COMM_LEN+1] = {0,};

	get_name_from_pid(task_name, saved_load_factor.active_app_pid);

	ret +=  snprintf(buf + ret, buf_size - ret, "%s\n", task_name);

	return ret;
}

static ssize_t active_app_pid_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos,
			active_app_pid_read_sub);

	return size_for_copy;
}

static ssize_t active_app_pid_write(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	int active_app_pid;
	active_app_pid = atoi(user_buf);

	store_external_load_factor(ACTIVE_APP_PID, active_app_pid);

	return count;
}

#define UN_KNOWN		0x00
#define TASK_HISTORY	0x01
#define WORK_HISTORY	0x02
#define INPUT_HISTORY	0x04

int data_part_str_to_num(char *str)
{
	int ret =0;

	if (strstr(str, "task") != NULL)
		ret += TASK_HISTORY;

	if (strstr(str, "work") != NULL)
		ret += WORK_HISTORY;

	if (strstr(str, "input") != NULL)
		ret += INPUT_HISTORY;

	return ret;
}

static ssize_t save_data_to_file_write(struct file *file,
		const char __user *user_buf, size_t count,
		loff_t *ppos)
{
	//int save_type;
	char file_path[100] = {0,};
	void *data = NULL;
	int size = 0, save_type;

	save_type = data_part_str_to_num((char *)user_buf);

	if (save_type && TASK_HISTORY) {

	}

	if (save_type && WORK_HISTORY) {

	}

	if (save_type && INPUT_HISTORY) {
#if defined (CONFIG_SLP_INPUT_REC)
		input_rec_save_data();
#endif
	}

	if (save_type != UN_KNOWN) {
		/* default save data + */
		strcpy(file_path, "/opt/usr/media/cpu_load_freq_history.dat");
		data = cpu_load_freq_history_view;
		size = CPU_LOAD_FREQ_HISTORY_SIZE;
		save_data_to_file(file_path, data, size);

		strcpy(file_path, "/opt/usr/media/cpu_load_freq_history_view_cnt");
		data = &cpu_load_freq_history_view_cnt;
		size = sizeof(unsigned int);
		save_data_to_file(file_path, data, size);
		/* default save data - */
	}

	return count;
}

static ssize_t load_data_from_file_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	//int save_type;
	char file_path[100] = {0,};
	void *data = NULL;
	int size = 0, load_type;

	load_type = data_part_str_to_num((char *)user_buf);

	if (load_type && TASK_HISTORY) {

	}

	if (load_type && WORK_HISTORY) {

	}

	if (load_type && INPUT_HISTORY) {
#if defined (CONFIG_SLP_INPUT_REC)
		input_rec_load_data();
#endif
	}

	if (load_type != UN_KNOWN) {
		/* default load data  + */
		strcpy(file_path, "/opt/usr/media/cpu_load_freq_history_view_cnt");
		data = &cpu_load_freq_history_view_cnt;
		size = sizeof(unsigned int);
		load_data_from_file(data, file_path, size);

		strcpy(file_path, "/opt/usr/media/cpu_load_freq_history.dat");
		data = cpu_load_freq_history_view;
		size = CPU_LOAD_FREQ_HISTORY_SIZE;
		load_data_from_file(data, file_path, size);
		/* default load data  - */

#if defined(CONFIG_SLP_INPUT_REC)
		b_input_load_data = 1;
#endif
	}

	return count;
}

#if defined(CONFIG_CHECK_NOT_CPUIDLE_CAUSE)
static const struct file_operations not_lpa_cause_check_fops = {
	.owner = THIS_MODULE,
	.read = not_lpa_cause_check,
};
#endif

static const struct file_operations debug_value_fops = {
	.owner = THIS_MODULE,
	.read = debug_value_read,
	.write =debug_value_write,
};

static const struct file_operations active_app_pid_fops = {
	.owner = THIS_MODULE,
	.read = active_app_pid_read,
	.write =active_app_pid_write,
};

static const struct file_operations saved_sched_num_fops = {
	.owner = THIS_MODULE,
	.read = saved_sched_num_read,
	.write =saved_sched_num_write,
};

#if defined(CONFIG_SLP_INPUT_REC)
static const struct file_operations saved_input_rec_num_fops = {
	.owner = THIS_MODULE,
	.read = saved_input_rec_num_read,
	.write = saved_input_rec_num_write,
};
#endif

static const struct file_operations saved_load_analyzer_data_fops = {
	.owner = THIS_MODULE,
	.read = saved_load_analyzer_data_read,
	.write =saved_load_analyzer_data_write,
};

static const struct file_operations save_data_to_file_fops = {
	.owner = THIS_MODULE,
	.write =save_data_to_file_write,
};

static const struct file_operations load_data_from_file_fops = {
	.owner = THIS_MODULE,
	.write =load_data_from_file_write,
};

static int __init system_load_analyzer_init(void)
{
	int ret = 0;
	struct dentry *d;

	d = debugfs_create_dir("load_analyzer", NULL);
	if (d) {
		debugfs_cpu_bus(d);

#if defined (CONFIG_CHECK_WORK_HISTORY)
		debugfs_workqueue(d);
#endif

#if defined(CONFIG_SLP_INPUT_REC)
		debugfs_input_rec(d);
#endif

#if defined(CONFIG_CHECK_NOT_CPUIDLE_CAUSE)
		if (!debugfs_create_file("not_lpa_cause_check", 0600, d, NULL, &not_lpa_cause_check_fops))
			pr_err("%s : debugfs_create_file, error\n", "not_lpa_cause_check");
#endif

		if (!debugfs_create_file("debug_value", 0600, d, NULL, &debug_value_fops))
			pr_err("%s : debugfs_create_file, error\n", "debug_value");

#if defined(CONFIG_SLP_CHECK_PHY_ADDR)
		debugfs_addr(d);
#endif

#if defined(CONFIG_SLP_CURRENT_MONITOR)
		debugfs_current(d);
#endif

		if (!debugfs_create_file("active_app_pid", 0600, d, NULL, &active_app_pid_fops))
			pr_err("%s : debugfs_create_file, error\n", "active_app_pid");

#if defined(CONFIG_SLP_MINI_TRACER)
		debugfs_mini(d);
#endif

#if defined (CONFIG_SLP_CPU_TESTER)
		debugfs_cpu_tester(d);
#endif

		if (!debugfs_create_file("saved_sched_num", 0600, d, NULL, &saved_sched_num_fops))
			pr_err("%s : debugfs_create_file, error\n", "saved_sched_num");

#if defined(CONFIG_SLP_INPUT_REC)
		if (!debugfs_create_file("saved_input_rec_num", 0600, d, NULL, &saved_input_rec_num_fops))
			pr_err("%s : debugfs_create_file, error\n", "saved_input_rec_num");
#endif

		if (!debugfs_create_file("saved_load_analyzer_data_num", 0600, d, NULL, &saved_load_analyzer_data_fops))
			pr_err("%s : debugfs_create_file, error\n", "saved_load_analyzer_data_num");

		if (!debugfs_create_file("save_data_to_file", 0600, d, NULL, &save_data_to_file_fops))
			pr_err("%s : debugfs_create_file, error\n", "save_data_to_file");

		if (!debugfs_create_file("load_data_from_file", 0600, d, NULL, &load_data_from_file_fops))
			pr_err("%s : debugfs_create_file, error\n", "load_data_from_file");
	}

#if defined(CONFIG_LOAD_ANALYZER_PMQOS)
	pm_qos_add_request(&pm_qos_min_cpu, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&pm_qos_max_cpu, PM_QOS_CLUSTER0_FREQ_MAX, INT_MAX);
	pm_qos_add_request(&pm_qos_min_cpu_num, PM_QOS_CPU_ONLINE_MIN, 0);
	pm_qos_add_request(&pm_qos_max_cpu_num, PM_QOS_CPU_ONLINE_MAX, CPU_NUM);
#endif

#if defined (CONFIG_SLP_CPU_TESTER)
	{
		int i;

		cpu_tester_wq = alloc_workqueue("cpu_tester_wq", WQ_HIGHPRI, 0);
		if (!cpu_tester_wq) {
			printk(KERN_ERR "Failed to create cpu_tester_wq workqueue\n");
			return -EFAULT;
		}

		for (i = 0; i < CPU_NUM; i++) {
			char str[64];
			sprintf(str, "cpu_tester%d_wq", i);
			cpu_load_wq[i] = alloc_workqueue(str, WQ_UNBOUND, 0);
			if (!cpu_load_wq[i]) {
				printk(KERN_ERR "Failed to create %s workqueue\n", str);
				return -EFAULT;
			} else{
				INIT_DELAYED_WORK(&(cpu_load_tester_work[i]), cpu_load_tester_work_start);
			}
		}

		INIT_DELAYED_WORK(&cpu_tester_work, cpu_tester_work_start);
		INIT_DELAYED_WORK(&cpu_freq_tester_work, cpu_freq_tester_work_start);
		INIT_DELAYED_WORK(&cpu_idle_tester_work, cpu_idle_tester_work_start);
	}
#endif

#if defined(CONFIG_SLP_CURRENT_MONITOR)
	load_analyzer_current_init();
#endif

	if (cpu_load_analyzer_init(NORMAL_MODE) != 0)
		pr_info("[%s] cpu_load_analyzer_init ERROR", __func__);

#if defined(CONFIG_SLP_MINI_TRACER)
	if (kernel_mini_tracer_init() != 0)
		pr_info("[%s] kernel_mini_tracer_init ERROR", __func__);
#endif

	return ret;
}

static void __exit system_load_analyzer_exit(void)
{
	cpu_load_analyzer_exit();
#if defined(CONFIG_SLP_MINI_TRACER)
	kernel_mini_tracer_exit();
#endif
}

module_init(system_load_analyzer_init);
module_exit(system_load_analyzer_exit);

MODULE_AUTHOR("Yong-U, Baek <yu.baek@samsung.com>");
MODULE_DESCRIPTION("'SLP power debuger");
MODULE_LICENSE("GPL");
