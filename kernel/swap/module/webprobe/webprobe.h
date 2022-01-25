#ifndef _WEBPROBE_H
#define _WEBPROBE_H

/**
 * @file webprobe/webprobe_prof.h
 * @author Anastasia Lyupa <a.lyupa@samsung.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * @section COPYRIGHT
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 * Profiling for webprobe
 */

enum web_prof_addr_t {
	INSPSERVER_START = 1,
	TICK_PROBE
};

int web_prof_enable(void);
int web_prof_disable(void);
bool web_prof_enabled(void);
u64 *web_prof_addr_ptr(enum web_prof_addr_t type);
int web_prof_data_set(char *app_path, char *app_id);

#endif /* _WEBPROBE_H */
