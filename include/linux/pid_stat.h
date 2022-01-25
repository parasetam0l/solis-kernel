/* include/linux/pid_stat.h
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
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
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.05.16>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 * 1.2   Hunsup Jung <hunsup.jung@samsung.com>       <2017.06.12>              *
 *                                                   Just release version 1.2  *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __PID_STAT_H
#define __PID_STAT_H

#ifdef CONFIG_ENERGY_MONITOR
#define PID_STAT_ARRAY_SIZE 4
struct pid_stat_monitor {
	char name[TASK_COMM_LEN];
	unsigned int transmit;
	unsigned int count;
};

void get_pid_stat_monitor(struct pid_stat_monitor *pid_stat_mon, int size);
#endif

#ifdef CONFIG_PID_STAT
int pid_stat_tcp_snd(pid_t pid, int size);
int pid_stat_tcp_rcv(pid_t pid, int size);
#else
static inline int pid_stat_tcp_snd(pid, size) { return 0; }
static inline int pid_stat_tcp_rcv(pid, size) { return 0; }
#endif

#endif /* __SAP_PID_STAT_H */
