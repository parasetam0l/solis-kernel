/**
 * parser/cpu_ctrl.c
 * @author Vasiliy Ulyanov <v.ulyanov@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * @section DESCRIPTION
 *
 * CPU controls implementation.
 */

#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <ksyms/ksyms.h>

#ifdef CONFIG_SMP
static void (*swap_cpu_maps_update_begin)(void);
static void (*swap_cpu_maps_update_done)(void);
static int (*swap_cpu_down)(unsigned int, int);
static int (*swap_cpu_up)(unsigned int, int);

/**
 * @brief Disables nonboot CPUs lock.
 *
 * @param mask Pointer to CPU mask struct.
 * @return 0 on success, error code on error.
 */
int swap_disable_nonboot_cpus_lock(struct cpumask *mask)
{
	int boot_cpu, cpu;
	int ret = 0;

	swap_cpu_maps_update_begin();
	cpumask_clear(mask);

	boot_cpu = cpumask_first(cpu_online_mask);

	for_each_online_cpu(cpu) {
		if (cpu == boot_cpu)
			continue;
		ret = swap_cpu_down(cpu, 0);
		if (ret == 0)
			cpumask_set_cpu(cpu, mask);
		printk(KERN_INFO "===> SWAP CPU[%d] down(%d)\n", cpu, ret);
	}

	WARN_ON(num_online_cpus() > 1);
	return ret;
}

/**
 * @brief Enables nonboot CPUs unlock.
 *
 * @param mask Pointer to CPU mask struct.
 * @return 0 on success, error code on error.
 */
int swap_enable_nonboot_cpus_unlock(struct cpumask *mask)
{
	int cpu, ret = 0;

	if (cpumask_empty(mask))
		goto out;

	for_each_cpu(cpu, mask) {
		ret = swap_cpu_up(cpu, 0);
		printk(KERN_INFO "===> SWAP CPU[%d] up(%d)\n", cpu, ret);
	}

out:
	swap_cpu_maps_update_done();

	return ret;
}

/**
 * @brief Intializes CPU controls.
 *
 * @return 0 on success, error code on error.
 */
int init_cpu_deps(void)
{
	const char *sym = "cpu_maps_update_begin";

	swap_cpu_maps_update_begin = (void *)swap_ksyms(sym);
	if (!swap_cpu_maps_update_begin)
		goto not_found;

	sym = "cpu_maps_update_done";
	swap_cpu_maps_update_done = (void *)swap_ksyms(sym);
	if (!swap_cpu_maps_update_done)
		goto not_found;

	sym = "_cpu_up";
	swap_cpu_up = (void *)swap_ksyms(sym);
	if (!swap_cpu_up)
		goto not_found;

	sym = "_cpu_down";
	swap_cpu_down = (void *)swap_ksyms(sym);
	if (!swap_cpu_down)
		goto not_found;

	return 0;

not_found:
	printk(KERN_INFO "ERROR: symbol %s(...) not found\n", sym);
	return -ESRCH;
}

#endif /* CONFIG_SMP */
