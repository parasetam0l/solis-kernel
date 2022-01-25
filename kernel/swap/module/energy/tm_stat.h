#ifndef _TM_STAT_H
#define _TM_STAT_H

/**
 * @file energy/tm_stat.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * @section COPYFIGHT
 * Copyright (C) Samsung Electronics, 2013
 *
 */


#include <linux/types.h>
#include <linux/time.h>


/**
 * @struct tm_stat
 * @brief Description of statistic time
 */
struct tm_stat {
	u64 timestamp;		/**< Time stamp */
	u64 running;		/**< Running time */
};

/**
 * @def DEFINE_TM_STAT
 * Initialize of tm_stat struct @hideinitializer
 */
#define DEFINE_TM_STAT(tm_name)		\
	struct tm_stat tm_name = {	\
		.timestamp = 0,		\
		.running = 0		\
	}


static inline u64 get_ntime(void)
{
	struct timespec ts;
	getnstimeofday(&ts);
	return timespec_to_ns(&ts);
}

static inline void tm_stat_init(struct tm_stat *tm)
{
	tm->timestamp = 0;
	tm->running = 0;
}

static inline void tm_stat_set_timestamp(struct tm_stat *tm, u64 time)
{
	tm->timestamp = time;
}

static inline u64 tm_stat_timestamp(struct tm_stat *tm)
{
	return tm->timestamp;
}

static inline void tm_stat_update(struct tm_stat *tm, u64 time)
{
	tm->running += time - tm->timestamp;
}

static inline u64 tm_stat_running(struct tm_stat *tm)
{
	return tm->running;
}

static inline u64 tm_stat_current_running(struct tm_stat *tm, u64 now)
{
	if (unlikely(now < tm->timestamp))
		printk(KERN_INFO "XXX %p WARNING now(%llu) < tmstmp(%llu)\n",
		       tm, now, tm->timestamp);
	return tm->timestamp ? tm->running + now - tm->timestamp : tm->running;
}

#endif /* _TM_STAT_H */
