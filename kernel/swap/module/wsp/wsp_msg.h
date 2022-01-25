#ifndef _WSP_MSG_H
#define _WSP_MSG_H

/*
 * wsp/wsp_msg.h
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

#include <linux/types.h>

enum wsp_id {
	WSP_RES_LOAD_BEGIN = 0x0001,
	WSP_RES_LOAD_END   = 0x0002,
	WSP_RES_PROC_BEGIN = 0x0003,
	WSP_RES_PROC_END   = 0x0004,
	WSP_DRAW_BEGIN     = 0x0005,
	WSP_DRAW_END       = 0x0006
};

void wsp_msg(enum wsp_id id, u32 res_id, const char *path);

#endif /* _WSP_MSG_H */
