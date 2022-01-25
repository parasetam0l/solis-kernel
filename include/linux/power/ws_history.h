/*
 * Copyright (C) 2016 SAMSUNG, Inc.
 * Hunsup Jung<hunsup.jung@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _WS_HISTORY_H
#define _WS_HISTORY_H

#define WS_HISTORY_ARRAY_SIZE	4
#define WS_HISTORY_NAME_LENGTH	16

#ifdef CONFIG_SLEEP_MONITOR
#define SLP_MON_TIME_INTERVAL_MS	60000
#define SLP_MON_WS_IDX_BIT			24
#define SLP_MON_WS_TIME_MAX			(BIT(SLP_MON_WS_IDX_BIT) - 1)
#endif

extern void update_ws_history_prv_time(void);
extern int add_ws_history(const char *name, ktime_t prevent_time);

#endif /* _WS_HISTORY_H */
