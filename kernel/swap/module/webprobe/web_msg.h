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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */

#ifndef _WEB_MSG_H
#define _WEB_MSG_H

struct pt_regs;

/* Web messages subtype */
enum web_msg_type {
	WEB_MSG_SAMPLING	= 0x00,
};

void web_msg_entry(struct pt_regs *regs);
void web_msg_exit(struct pt_regs *regs);
void web_sample_msg(struct pt_regs *regs);

#endif /* _WEB_MSG_H */
