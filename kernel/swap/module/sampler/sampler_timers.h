/*
 *  SWAP sampler
 *  modules/sampler/sampler_timers.h
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013	 Alexander Aksenov <a.aksenov@samsung.com>: SWAP sampler porting
 *
 */



#ifndef __SAMPLER_TIMERS_H__
#define __SAMPLER_TIMERS_H__


/* ===================== INCLUDE ==================== */

#if defined(CONFIG_HIGH_RES_TIMERS)

#include <linux/hrtimer.h>

#else /* CONFIG_HIGH_RES_TIMERS */

#include <linux/timer.h>

#endif /* CONFIG_HIGH_RES_TIMERS */

/* ==================== TYPE DEFS =================== */

#if defined(CONFIG_HIGH_RES_TIMERS)

typedef struct hrtimer   swap_timer;
typedef enum hrtimer_restart   restart_ret;

#else /* CONFIG_HIGH_RES_TIMERS */

typedef struct timer_list   swap_timer;
typedef int   restart_ret;

#endif /* CONFIG_HIGH_RES_TIMERS */


/* ====================== FUNCS ===================== */

restart_ret sampler_timers_restart(swap_timer *timer);
void sampler_timers_stop(int cpu);
void sampler_timers_start(void *unused);
void sampler_timers_set_quantum(unsigned int timer_quantum);
void sampler_timers_set_run(void);
void sampler_timers_set_stop(void);

#endif /* __SAMPLER_TIMERS_H__ */
