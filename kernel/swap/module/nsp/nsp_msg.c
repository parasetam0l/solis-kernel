/*
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/sched.h>
#include <writer/swap_msg.h>
#include "nsp_msg.h"


struct nsp_msg_struct {
	u32 pid;
	u32 stage;
	u64 begin_time;
	u64 end_time;
} __packed;


void nsp_msg(enum nsp_msg_stage stage, u64 begin_time, u64 end_time)
{
	struct swap_msg *m;
	struct nsp_msg_struct *nsp;

	m = swap_msg_get(MSG_NSP);

	nsp = (struct nsp_msg_struct *)swap_msg_payload(m);
	nsp->pid = (u32)current->tgid;
	nsp->stage = (u32)stage;
	nsp->begin_time = begin_time;
	nsp->end_time = end_time;

	swap_msg_flush(m, sizeof(*nsp));
	swap_msg_put(m);
}
