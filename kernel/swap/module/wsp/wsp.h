#ifndef _WSP_H
#define _WSP_H

/*
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Web startup profiling
 */

enum wsp_mode {
	WSP_ON,
	WSP_OFF
};

int wsp_set_addr(const char *name, unsigned long offset);

int wsp_set_mode(enum wsp_mode mode);
enum wsp_mode wsp_get_mode(void);

void wsp_set_webapp_path(char *path, size_t len);
void wsp_set_chromium_path(char *path, size_t len);

int wsp_init(void);
void wsp_exit(void);

#endif /* _WSP_H */
