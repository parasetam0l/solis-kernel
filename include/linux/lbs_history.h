/* include/linux/lbs_history.h
 *
 * Copyright (C) 2017 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.07.21>              *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __LBS_HISTORY_H
#define __LBS_HISTORY_H

#ifdef CONFIG_ENERGY_MONITOR
#define LBS_HISTORY_ARRAY_SIZE 3
struct gps_request {
	char name[TASK_COMM_LEN];
	ktime_t gps_time;
};

struct wps_request {
	char name[TASK_COMM_LEN];
	ktime_t wps_time;
};

void get_top_lbs_time(int type, struct gps_request *gps, struct wps_request *wps, int size);
#endif

#endif /* __LBS_HISTORY_H */
