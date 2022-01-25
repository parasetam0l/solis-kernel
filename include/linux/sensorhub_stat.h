/* include/linux/sensorhub_stat.h
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
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
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2015>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.08.17>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __SENSORHUB_STAT_H
#define __SENSORHUB_STAT_H

#define SENSORHUB_LIB_MAX 64

#define DATAFRAME_GPS_TIME_START		3
#define DATAFRAME_GPS_TIME_END			6
#define DATAFRAME_GPS_LAST_USER_START	7
#define DATAFRAME_GPS_LAST_USER_END		14
#define DATAFRAME_GPS_EXT				15

struct sensorhub_wakeup_stat {
	int wakeup_cnt[SENSORHUB_LIB_MAX];
	int ap_wakeup_cnt[SENSORHUB_LIB_MAX];
};

struct sensorhub_gps_stat {
	int dataframe_size;
	int gps_time;
	unsigned long long last_gps_user;
	unsigned char last_gps_ext;
};

/* Contains definitions for sensorhub resource tracking per sensorhub lib number. */
#ifdef CONFIG_SENSORHUB_STAT
int sensorhub_stat_rcv(const char *dataframe, int size);
int sensorhub_stat_get_wakeup(struct sensorhub_wakeup_stat *sh_wakeup);
int sensorhub_stat_get_gps_info(struct sensorhub_gps_stat *sh_gps);
#else
static inline int sensorhub_stat_rcv(const char *dataframe, int size) { return 0; }
static inline int sensorhub_stat_get_wakeup(struct sensorhub_wakeup_stat *sh_wakeup) { return 0; }
static inline int sensorhub_stat_get_gps_info(struct sensorhub_gps_stat *sh_gps) { return 0; }
#endif

#endif /* __SENSORHUB_STAT_H */
