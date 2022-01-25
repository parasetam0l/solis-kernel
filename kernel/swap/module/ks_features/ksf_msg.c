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
#include <writer/kernel_operations.h>
#include "ksf_msg.h"


#define KSF_PREFIX	KERN_INFO "[KSF] "





/* ============================================================================
 * =                       MSG_SYSCALL_* (ENTRY/EXIT)                         =
 * ============================================================================
 */
struct msg_sys_header {
	u32 pid;
	u32 tid;
	u32 probe_type;
	u64 pc_addr;
	u64 caller_pc_addr;
	u32 cpu_num;
} __packed;

struct msg_sys_entry {
	struct msg_sys_header h;
	u32 cnt_args;
	char args[0];
} __packed;

struct msg_sys_exit {
	struct msg_sys_header h;
	char ret_val[0];
} __packed;


static void pack_header(struct msg_sys_header *h, unsigned long func_addr,
			unsigned long ret_addr, enum probe_t type)
{
	struct task_struct *task = current;

	h->pid = task->tgid;
	h->tid = task->pid;
	h->probe_type = (u32)type;
	h->pc_addr = func_addr;
	h->caller_pc_addr = ret_addr;
	h->cpu_num = raw_smp_processor_id();
}

static void pack_entry_header(struct msg_sys_entry *e, struct pt_regs *regs,
			      unsigned long func_addr, enum probe_t type,
			      const char *fmt)
{
	pack_header(&e->h, func_addr, get_regs_ret_func(regs), type);
	e->cnt_args = strlen(fmt);
}

static void pack_exit_header(struct msg_sys_exit *e, unsigned long func_addr,
			     unsigned long ret_addr, enum probe_t type)
{
	pack_header(&e->h, func_addr, ret_addr, type);
}

void ksf_msg_entry(struct pt_regs *regs, unsigned long func_addr,
		   enum probe_t type, const char *fmt)
{
	int ret;
	struct swap_msg *m;
	struct msg_sys_entry *ent;
	size_t size;

	m = swap_msg_get(MSG_SYSCALL_ENTRY);

	ent = swap_msg_payload(m);
	pack_entry_header(ent, regs, func_addr, type, fmt);

	size = swap_msg_size(m) - sizeof(*ent);
	ret = swap_msg_pack_args(ent->args, size, fmt, regs);
	if (ret < 0) {
		printk(KSF_PREFIX "ERROR: arguments packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ent) + ret);

put_msg:
	swap_msg_put(m);
}

void ksf_msg_exit(struct pt_regs *regs, unsigned long func_addr,
		  unsigned long ret_addr, enum probe_t type, char ret_type)
{
	int ret;
	struct swap_msg *m;
	struct msg_sys_exit *ext;
	size_t size;

	m = swap_msg_get(MSG_SYSCALL_EXIT);

	ext = swap_msg_payload(m);
	pack_exit_header(ext, func_addr, ret_addr, type);

	size = swap_msg_size(m) - sizeof(*ext);
	ret = swap_msg_pack_ret_val(ext->ret_val, size, ret_type, regs);
	if (ret < 0) {
		printk(KSF_PREFIX "ERROR: ret value packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ext) + ret);

put_msg:
	swap_msg_put(m);
}





/* ============================================================================
 * =                    MSG_FILE_FUNCTION_* (ENTRY/EXIT)                      =
 * ============================================================================
 */
struct msg_file_entry {
	u32 pid;
	u32 tid;
	u32 fd;
	u32 event_type;
	char file_path[0];
} __packed;

enum file_info {
	FI_GENIRAL = 0,
	FI_OPEN = 1,
	FI_LOCK = 2
};

static int pack_file_entry_head(void *data, size_t size, enum file_info info,
				int fd, enum file_api_t api, const char *path)
{
	struct msg_file_entry *ent = (struct msg_file_entry *)data;
	struct task_struct *task = current;
	size_t len, old_size = size;

	ent->pid = task->tgid;
	ent->tid = task->pid;
	ent->fd = fd;
	ent->event_type = api;

	size -= sizeof(*ent);
	len = strlen(path);
	if (size < len + 1)
		return -ENOMEM;

	memcpy(ent->file_path, path, len);
	ent->file_path[len] = '\0';

	size -= len + 1;
	data += old_size - size;

	if (size < 4)
		return -ENOMEM;

	*((u32 *)data) = (u32)info;
	size -= 4;

	return old_size - size;
}



void ksf_msg_file_entry(int fd, enum file_api_t api, const char *path)
{
	int ret;
	void *p;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_FILE_FUNCTION_ENTRY);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	ret = pack_file_entry_head(p, size, FI_GENIRAL, fd, api, path);
	if (ret < 0) {
		printk(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}

	swap_msg_flush(m, ret);

put_msg:
	swap_msg_put(m);
}

void ksf_msg_file_entry_open(int fd, enum file_api_t api, const char *path,
			     const char __user *ofile)
{
	long n;
	int ret;
	void *p;
	size_t size;
	struct swap_msg *m;

	m = swap_msg_get(MSG_FILE_FUNCTION_ENTRY);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	ret = pack_file_entry_head(p, size, FI_OPEN, fd, api, path);
	if (ret < 0) {
		printk(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}

	size -= ret;
	p += ret;

	n = strncpy_from_user(p, ofile, size);
	if (n < 0) {
		printk(KSF_PREFIX "cannot copy ofile\n");
		goto put_msg;
	}

	swap_msg_flush(m, ret + n + 1);

put_msg:
	swap_msg_put(m);
}

struct lock_arg {
	u32 type;
	u32 whence;
	u64 start;
	u64 len;
} __packed;

void ksf_msg_file_entry_lock(int fd, enum file_api_t api, const char *path,
			     int type, int whence, s64 start, s64 len)
{
	int ret;
	void *p;
	size_t size;
	struct swap_msg *m;
	struct lock_arg *arg;

	m = swap_msg_get(MSG_FILE_FUNCTION_ENTRY);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	ret = pack_file_entry_head(p, size, FI_LOCK, fd, api, path);
	if (ret < 0) {
		printk(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}

	size -= ret;
	p += ret;

	if (size < sizeof(*arg)) {
		printk(KSF_PREFIX "buffer is too small\n");
		goto put_msg;
	}

	arg = (struct lock_arg *)p;
	arg->type = (u32)type;
	arg->whence = (u32)whence;
	arg->start = (u64)start;
	arg->len = (u64)len;

	swap_msg_flush(m, ret + sizeof(*arg));

put_msg:
	swap_msg_put(m);
}


struct msg_file_exit {
	u32 pid;
	u32 tid;
	char ret_val[0];
} __packed;

void ksf_msg_file_exit(struct pt_regs *regs, char ret_type)
{
	struct task_struct *task = current;
	int ret;
	struct swap_msg *m;
	struct msg_file_exit *ext;
	size_t size;

	m = swap_msg_get(MSG_FILE_FUNCTION_EXIT);

	ext = swap_msg_payload(m);
	ext->pid = task->tgid;
	ext->tid = task->pid;

	size = swap_msg_size(m) - sizeof(*ext);
	ret = swap_msg_pack_ret_val(ext->ret_val, size, ret_type, regs);
	if (ret < 0) {
		printk(KSF_PREFIX "ERROR: ret value packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ext) + ret);

put_msg:
	swap_msg_put(m);
}





/* ============================================================================
 * =                    MSG_FILE_FUNCTION_* (ENTRY/EXIT)                      =
 * ============================================================================
 */
struct msg_context_switch {
	u64 pc_addr;
	u32 pid;
	u32 tid;
	u32 cpu_num;
} __packed;

static void context_switch(struct task_struct *task, enum swap_msg_id id)
{
	struct swap_msg *m;
	struct msg_context_switch *mcs;
	void *p;

	m = swap_msg_get(id);
	p = swap_msg_payload(m);

	mcs = p;
	mcs->pc_addr = 0;
	mcs->pid = task->tgid;
	mcs->tid = task->pid;
	mcs->cpu_num = raw_smp_processor_id();

	swap_msg_flush_wakeupoff(m, sizeof(*mcs));
	swap_msg_put(m);
}

void ksf_switch_entry(struct task_struct *task)
{
	context_switch(task, MSG_CONTEXT_SWITCH_ENTRY);
}

void ksf_switch_exit(struct task_struct *task)
{
	context_switch(task, MSG_CONTEXT_SWITCH_EXIT);
}
