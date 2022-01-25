/**
 * @file parser/msg_buf.h
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
 * Message buffer interface declaration.
 */


#ifndef _MSG_BUF_H
#define _MSG_BUF_H

#include <linux/types.h>

/**
 * @struct msg_buf
 * @brief Stores pointers to the message buffer.
 */
struct msg_buf {
	char *begin;    /**< Pointer to the beginning of the buffer. */
	char *end;      /**< Pointer to the end of the buffer. */
	char *ptr;      /**< Buffer iterator. */
};

int init_mb(struct msg_buf *mb, size_t size);
void uninit_mb(struct msg_buf *mb);

int cmp_mb(struct msg_buf *mb, size_t size);
size_t remained_mb(struct msg_buf *mb);
int is_end_mb(struct msg_buf *mb);

int get_u8(struct msg_buf *mb, u8 *val);
int get_u32(struct msg_buf *mb, u32 *val);
int get_u64(struct msg_buf *mb, u64 *val);

int get_string(struct msg_buf *mb, char **str);
void put_string(const char *str);

#endif /* _MSG_BUF_H */
