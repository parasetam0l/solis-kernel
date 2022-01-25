/**
 * sampler/sampler_timer.c
 * @author Alexander Aksenov <a.aksenov@samsung.com>
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
 * Sampler based on common timers.
 */



#include "sampler_timers.h"



static unsigned long sampler_timer_quantum;
static DEFINE_PER_CPU(struct timer_list, swap_timer);
static int swap_timer_running;

/**
 * @brief Restarts sampling.
 *
 * @param timer Pointer to timer_list struct.
 * @return 0.
 */
restart_ret sampler_timers_restart(swap_timer *timer)
{
	restart_ret ret;

	mod_timer_pinned((struct timer_list *)timer,
		     jiffies + sampler_timer_quantum);
	ret = 0;

	return ret;
}

/**
 * @brief Sets running flag true.
 *
 * @return Void.
 */
void sampler_timers_set_run(void)
{
	swap_timer_running = 1;
}

/**
 * @brief Sets running flag false.
 *
 * @return Void.
 */
void sampler_timers_set_stop(void)
{
	swap_timer_running = 0;
}

/**
 * @brief Starts timer sampling.
 *
 * @param restart_func Pointer to restart function.
 * @return Void.
 */
void sampler_timers_start(void *restart_func)
{
	struct timer_list *timer = &__get_cpu_var(swap_timer);

	if (!swap_timer_running)
		return;

	init_timer(timer);
	timer->data = (unsigned long)timer;
	timer->function = restart_func;

	mod_timer_pinned(timer, jiffies + sampler_timer_quantum);
}

/**
 * @brief Stops timer sampling.
 *
 * @param cpu Online CPUs.
 * @return Void.
 */
void sampler_timers_stop(int cpu)
{
	struct timer_list *timer = &per_cpu(swap_timer, cpu);

	if (!swap_timer_running)
		return;
	del_timer_sync(timer);
}

/**
 * @brief Sets timer quantum.
 *
 * @param timer_quantum Timer quantum.
 * @return Void.
 */
void sampler_timers_set_quantum(unsigned int timer_quantum)
{
	sampler_timer_quantum = timer_quantum;
}
