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

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
#include <writer/swap_msg.h>
#include <writer/event_filter.h>
#include <swap-asm/swap_uprobes.h>
#include "web_msg.h"

#define WEB_PREFIX      KERN_INFO "[WEB_PROF] "
/* TODO: develop method for obtaining this data during build... */
#define LINE_NUMBER_OFFSET		16
#define FUNCTION_NAME_OFFSET		8
#define SOURCE_FILE_NAME_OFFSET		12

static long pack_str_form_user(void *data, size_t size, const char __user *s)
{
	long ret, len;

	if (!size)
		return -ENOMEM;

	ret = strncpy_from_user(data, s, size);
	if (ret < 0) {
		pr_err(WEB_PREFIX "failed to get userspace string s=%p\n", s);
		return ret;

	} else if (ret == size) {
		pr_warn(WEB_PREFIX "user string is very long ret=%ld\n", ret);
		len = ret;
	} else {
		len = ret + 1;
	}

	((char *)data)[len - 1] = '\0';

	return len;
}

void web_sample_msg(struct pt_regs *regs)
{
	struct task_struct *task = current;
	struct swap_msg *m;
	void *p;
	size_t old_size, size;
	long ret;
	void __user *obj_ptr;
	int line;
	int __user *line_number_ptr;
	const char __user **func_name_ptr;
	const char __user *func_name;
	const char __user **file_name_ptr;
	const char __user *file_name;

	if (!check_event(task))
		return;

	/* Get opbject pointer */
	obj_ptr = (void __user *)swap_get_uarg(regs, 1);

	m = swap_msg_get(MSG_WEB_PROFILING);
	p = swap_msg_payload(m);
	old_size = size = swap_msg_size(m);

	/* Type */
	*(u8 *)p = WEB_MSG_SAMPLING;
	p += sizeof(u8);
	size -= sizeof(u8);

	/* PID */
	*(u32 *)p = task->tgid;
	p += sizeof(u32);
	size -= sizeof(u32);

	/* TID */
	*(u32 *)p = task->pid;
	p += sizeof(u32);
	size -= sizeof(u32);

	/* Line number (in source file) */
	line_number_ptr = obj_ptr + LINE_NUMBER_OFFSET;
	if (get_user(line, line_number_ptr)) {
		pr_err("failed to get line number\n");
		goto out;
	}
	*(u32 *)p = (u32)line;
	p += sizeof(u32);
	size -= sizeof(u32);

	/* Get function name string pointer */
	func_name_ptr = obj_ptr + FUNCTION_NAME_OFFSET;
	if (get_user(func_name, func_name_ptr)) {
		pr_err("failed to get function name\n");
		goto out;
	}
	ret = pack_str_form_user(p, size, func_name);
	if (ret < 0)
		goto out;
	p += ret;
	size -= ret;

	/* Get source file name string pointer */
	file_name_ptr = obj_ptr + SOURCE_FILE_NAME_OFFSET;
	if (get_user(file_name, file_name_ptr)) {
		pr_err("failed to get file name\n");
		goto out;
	}
	ret = pack_str_form_user(p, size, file_name);
	if (ret < 0)
		goto out;
	size -= ret;

	swap_msg_flush(m, old_size - size);

out:
	swap_msg_put(m);
}
