/**
 * @file parser/msg_cmd.h
 * @author Vyacheslav Cherkashin
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Module's message handling interface declaration.
 */

#ifndef _MSG_CMD_H
#define _MSG_CMD_H

struct msg_buf;

int once_cmd(void);

int msg_keep_alive(struct msg_buf *mb);
int msg_start(struct msg_buf *mb);
int msg_stop(struct msg_buf *mb);
int msg_config(struct msg_buf *mb);
int msg_swap_inst_add(struct msg_buf *mb);
int msg_swap_inst_remove(struct msg_buf *mb);
int get_wrt_launcher_port(void);
void set_wrt_launcher_port(int port);

#endif /* _MSG_CMD_H */
