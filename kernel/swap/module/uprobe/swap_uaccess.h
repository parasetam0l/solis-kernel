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


#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>


static const char *strdup_from_user(const char __user *user_s, gfp_t gfp)
{
	enum { max_str_len = 1024 };
	char *str;
	int len_s, ret;

	len_s = strnlen_user(user_s, max_str_len - 1);
	str = kmalloc(len_s + 1, gfp);
	if (str == NULL)
		return NULL;

	ret = copy_from_user(str, user_s, len_s);
	if (ret < 0) {
		kfree(str);
		return NULL;
	}

	str[len_s] = '\0';

	return str;
}
