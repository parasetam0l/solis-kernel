/* include/linux/sap_pid_stat.h
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
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.08.01>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __SAP_PID_STAT_H
#define __SAP_PID_STAT_H

#define SAP_STAT_SAPID_MAX 32

struct sap_pid_wakeup_stat {
	int wakeup_cnt[SAP_STAT_SAPID_MAX];
	int activity_cnt[SAP_STAT_SAPID_MAX];
};

#ifdef CONFIG_SAP_PID_STAT
int sap_stat_get_wakeup(struct sap_pid_wakeup_stat *sap_wakeup);
#else
static inline int sap_stat_get_wakeup(struct sap_pid_wakeup_stat *sap_wakeup) { return 0; }
#endif

#endif /* __SAP_PID_STAT_H */
