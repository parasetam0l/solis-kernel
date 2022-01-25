/**
 * parser/msg_buf.c
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov
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
 * Message buffer controls implementation.
 */


#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "msg_buf.h"
#include "parser_defs.h"

/**
 * @brief Initializes message buffer.
 *
 * @param mb Pointer to message buffer struct.
 * @param size Size of the message buffer.
 * @return 0 on success, negative error code on error.
 */
int init_mb(struct msg_buf *mb, size_t size)
{
	if (size) {
		mb->begin = vmalloc(size);
		if (mb->begin == NULL) {
			printk(KERN_INFO "Cannot alloc memory!\n");
			return -ENOMEM;
		}

		mb->ptr = mb->begin;
		mb->end = mb->begin + size;
	} else
		mb->begin = mb->end = mb->ptr = NULL;

	return 0;
}

/**
 * @brief Uninitializes message buffer.
 *
 * @param mb Pointer to message buffer struct.
 * @return Void.
 */
void uninit_mb(struct msg_buf *mb)
{
	vfree(mb->begin);
}

/**
 * @brief Checks if current buffer data pointer is at the end.
 *
 * @param mb Pointer to the message buffer struct.
 * @param size Size of the message buffer.
 * @return 1 - buffer ptr is not at the end,
 * 0 - buffer ptr is at the end,
 * -1 - buffer ptr is invalid (far away of the end).
 */
int cmp_mb(struct msg_buf *mb, size_t size)
{
	char *tmp;

	tmp = mb->ptr + size;
	if (mb->end > tmp)
		return 1;
	else if (mb->end < tmp)
		return -1;

	return 0;
}

/**
 * @brief Gets remainder part of message buffer.
 *
 * @param mb Pointer to the message buffer struct.
 * @return Remainder part of message buffer.
 */
size_t remained_mb(struct msg_buf *mb)
{
	return mb->end - mb->ptr;
}

/**
 * @brief Checks whether we reached the end of the message buffer or not.
 *
 * @param mb Pointer to the message buffer struct.
 * @return 1 - if message buffer end has been reached \n
 * 0 - otherwise.
 */
int is_end_mb(struct msg_buf *mb)
{
	return mb->ptr == mb->end;
}

/**
 * @brief Reads 8 bits from message buffer and updates buffer's pointer.
 *
 * @param mb Pointer to the message buffer struct.
 * @param val Pointer to the target variable where to put read value.
 * @return 0 on success, negative error code on error.
 */
int get_u8(struct msg_buf *mb, u8 *val)
{
	if (cmp_mb(mb, sizeof(*val)) < 0)
		return -EINVAL;

	*val = *((u8 *)mb->ptr);
	mb->ptr += sizeof(*val);

	print_parse_debug("u8 ->%d;%08X\n", *val, *val);

	return 0;
}

/**
 * @brief Reads 32 bits from message buffer and updates buffer's pointer.
 *
 * @param mb Pointer to the message buffer struct.
 * @param val Pointer to the target variable where to put read value.
 * @return 0 on success, negative error code on error.
 */
int get_u32(struct msg_buf *mb, u32 *val)
{
	if (cmp_mb(mb, sizeof(*val)) < 0)
		return -EINVAL;

	*val = *((u32 *)mb->ptr);
	mb->ptr += sizeof(*val);

	print_parse_debug("u32->%d;%08X\n", *val, *val);

	return 0;
}

/**
 * @brief Reads 64 bits from message buffer and updates buffer's pointer.
 *
 * @param mb Pointer to the message buffer struct.
 * @param val Pointer to the target variable where to put read value.
 * @return 0 on success, negative error code on error.
 */
int get_u64(struct msg_buf *mb, u64 *val)
{
	if (cmp_mb(mb, sizeof(*val)) < 0)
		return -EINVAL;

	*val = *((u64 *)mb->ptr);
	mb->ptr += sizeof(*val);
	print_parse_debug("u64->%llu; 0x%016llX\n", *val, *val);

	return 0;
}

/**
 * @brief Reads null-terminated string from message buffer and updates
 * buffer's pointer.
 *
 * @param mb Pointer to the message buffer struct.
 * @param str Pointer to the target variable where to put read string.
 * @return 0 on success, negative error code on error.
 */
int get_string(struct msg_buf *mb, char **str)
{
	size_t len, len_max;
	enum { min_len_str = 1 };

	if (cmp_mb(mb, min_len_str) < 0)
		return -EINVAL;

	len_max = remained_mb(mb) - 1;
	len = strnlen(mb->ptr, len_max);

	*str = kmalloc(len + 1, GFP_KERNEL);
	if (*str == NULL)
		return -ENOMEM;

	memcpy(*str, mb->ptr, len);
	(*str)[len] = '\0';

	mb->ptr += len + 1;

	print_parse_debug("str->'%s'\n", *str);
	return 0;
}

/**
 * @brief Releases string memory allocated in get_string.
 *
 * @param str Pointer to the target string.
 * @return Void.
 */
void put_string(const char *str)
{
	kfree(str);
}
