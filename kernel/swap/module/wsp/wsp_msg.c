/*
 * wsp/wsp_msg.c
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

#include <linux/sched.h>
#include <linux/string.h>
#include <writer/swap_msg.h>
#include "wsp_msg.h"

/*
 * MSG_WSP (payload):
 * +-------------+----------+----------+
 * | name        | type     | length   |
 * +-------------+----------+----------+
 * | PID         | int      |      4   |
 * | wsp_id      | int      |      4   |
 * | wsp_payload | variable | variable |
 * +-------------+----------+----------+

 * wsp_id:
 *   - WSP_RES_LOAD_BEGIN = 0x0001
 *   - WSP_RES_LOAD_END   = 0x0002
 *   - WSP_RES_PROC_BEGIN = 0x0003
 *   - WSP_RES_PROC_END   = 0x0004
 *   - WSP_DRAW_BEGIN     = 0x0005
 *   - WSP_DRAW_END       = 0x0006
 *
 * wsp_payload:
 *
 * 1. WSP_RES_LOAD_BEGIN:
 *         +--------+--------+----------+
 *         | name   | type   |  length  |
 *         +--------+--------+----------+
 *         | res_id | int    |     4    |
 *         | path   | string | variable |
 *         +--------+--------+----------+
 *
 * 2. WSP_RES_LOAD_END, WSP_RES_PROC_BEGIN, WSP_RES_PROC_END:
 *         +--------+--------+----------+
 *         | name   | type   |  length  |
 *         +--------+--------+----------+
 *         | res_id | int    |     4    |
 *         +--------+--------+----------+
 *
 * 3. WSP_DRAW_BEGIN, WSP_DRAW_END:
 *         no wsp_payload
 */

static int pack_wsp_msg(void *data, size_t size, enum wsp_id id,
			u32 res_id, const char *path)
{
	size_t len;
	const size_t old_size = size;

	/* write PID */
	*(u32 *)data = (u32)current->tgid;
	data += 4;
	size -= 4;

	/* write wsp_id */
	*(u32 *)data = (u32)id;
	data += 4;
	size -= 4;

	/* pack wsp_payload */
	switch (id) {
	case WSP_RES_LOAD_BEGIN:
		len = strlen(path) + 1;
		if (size < len + 4)
			return -ENOMEM;

		/* '+ 4' - skip space for res_id */
		memcpy(data + 4, path, len);
		size -= len;
	case WSP_RES_LOAD_END:
	case WSP_RES_PROC_BEGIN:
	case WSP_RES_PROC_END:
		/* write res_id */
		*(u32 *)data = res_id;
		size -= 4;
		break;

	case WSP_DRAW_BEGIN:
	case WSP_DRAW_END:
		break;

	default:
		pr_err("unknown wsp_id: id=%u\n", (unsigned int)id);
		return -EINVAL;
	}

	return old_size - size;
}

void wsp_msg(enum wsp_id id, u32 res_id, const char *path)
{
	int ret;
	void *data;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_WSP);
	data = swap_msg_payload(m);
	size = swap_msg_size(m);
	ret = pack_wsp_msg(data, size, id, res_id, path);
	if (ret < 0) {
		pr_err("error MSG_WSP packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, ret);

put_msg:
	swap_msg_put(m);
}
