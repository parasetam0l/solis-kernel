/**
 * sampler/swap_sampler_module.c
 * @author Andreev S.V.: SWAP Sampler implementation
 * @author Alexander Aksenov <a.aksenov@samsung.com>: SWAP sampler porting
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Timer-based sampling module.
 */

#include <linux/ptrace.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/module.h>

#include "swap_sampler_module.h"
#include "swap_sampler_errors.h"
#include "kernel_operations.h"
#include "sampler_timers.h"


static BLOCKING_NOTIFIER_HEAD(swap_sampler_notifier_list);
static swap_sample_cb_t sampler_cb;

static restart_ret swap_timer_restart(swap_timer *timer)
{
	sampler_cb(task_pt_regs(current));

	return sampler_timers_restart(timer);
}

static int swap_timer_start(void)
{
	get_online_cpus();
	sampler_timers_set_run();

	on_each_cpu(sampler_timers_start, swap_timer_restart, 1);
	put_online_cpus();

	return E_SS_SUCCESS;
}

static void swap_timer_stop(void)
{
	int cpu;

	get_online_cpus();

	for_each_online_cpu(cpu)
		sampler_timers_stop(cpu);
	sampler_timers_set_stop();
	put_online_cpus();
}

static int swap_cpu_notify(struct notifier_block *self,
				    unsigned long action, void *hcpu)
{
	long cpu = (long) hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		smp_call_function_single(cpu, sampler_timers_start,
				 swap_timer_restart, 1);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		sampler_timers_stop(cpu);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata swap_cpu_notifier = {
	.notifier_call = swap_cpu_notify,
};

static int do_swap_sampler_start(unsigned int timer_quantum)
{
	if (timer_quantum <= 0)
		return -EINVAL;

	sampler_timers_set_quantum(timer_quantum);
	swap_timer_start();

	return 0;
}

static void do_swap_sampler_stop(void)
{
	swap_timer_stop();
}

static DEFINE_MUTEX(mutex_run);
static int sampler_run;


/**
 * @brief Starts sampling with specified timer quantum.
 *
 * @param timer_quantum Timer quantum for sampling.
 * @return 0 on success, error code on error.
 */
int swap_sampler_start(unsigned int timer_quantum, swap_sample_cb_t cb)
{
	int ret = -EINVAL;

	mutex_lock(&mutex_run);
	if (sampler_run) {
		printk(KERN_INFO "sampler profiling is already run!\n");
		goto unlock;
	}

	sampler_cb = cb;

	ret = do_swap_sampler_start(timer_quantum);
	if (ret == 0)
		sampler_run = 1;

unlock:
	mutex_unlock(&mutex_run);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_sampler_start);


/**
 * @brief Stops sampling.
 *
 * @return 0 on success, error code on error.
 */
int swap_sampler_stop(void)
{
	int ret = 0;

	mutex_lock(&mutex_run);
	if (sampler_run == 0) {
		printk(KERN_INFO "energy profiling is not running!\n");
		ret = -EINVAL;
		goto unlock;
	}

	do_swap_sampler_stop();

	sampler_run = 0;
unlock:
	mutex_unlock(&mutex_run);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_sampler_stop);

static int __init sampler_init(void)
{
	int retval;

	retval = register_hotcpu_notifier(&swap_cpu_notifier);
	if (retval) {
		print_err("Error of register_hotcpu_notifier()\n");
		return retval;
	}

	print_msg("Sample ininitialization success\n");

	return E_SS_SUCCESS;
}

static void __exit sampler_exit(void)
{
	if (sampler_run)
		do_swap_sampler_stop();

	unregister_hotcpu_notifier(&swap_cpu_notifier);

	print_msg("Sampler uninitialized\n");
}

module_init(sampler_init);
module_exit(sampler_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP sampling module");
MODULE_AUTHOR("Andreev S.V., Aksenov A.S.");
