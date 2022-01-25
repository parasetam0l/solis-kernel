/**
 * fbiprobe/fbi_msg.c
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Packing and writing data.
 */


#include <linux/types.h>
#include <linux/string.h>
#include <writer/swap_msg.h>

struct msg_fbi {
	u32 var_id;
	u32 size;
	char var_data[0];
} __packed;


static char *pack_fbi_info(char *payload, unsigned long var_id, size_t size,
			   char *msg_buf)
{
	struct msg_fbi *fbi_m = (struct msg_fbi *)payload;

	fbi_m->var_id = var_id;
	fbi_m->size = size;
	if (size != 0) {
		/* FIXME Possible out of buffer! */
		memcpy(&fbi_m->var_data, msg_buf, size);
	}

	/*
	 * If size is 0 that mean we cannot get data for this probe.
	 * But we pack it like error code
	 */

	return payload + sizeof(struct msg_fbi) + size;
}

void fbi_msg(unsigned long var_id, size_t size, char *msg_buf)
{
	struct swap_msg *m;
	void *p;
	void *buf_end;

	m = swap_msg_get(MSG_FBI);
	p = swap_msg_payload(m);

	buf_end = pack_fbi_info(p, var_id, size, msg_buf);

	swap_msg_flush(m, buf_end - p);

	swap_msg_put(m);
}
