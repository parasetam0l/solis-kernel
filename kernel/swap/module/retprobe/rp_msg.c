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
#include <linux/module.h>
#include <writer/swap_msg.h>
#include <writer/kernel_operations.h>
#include "rp_msg.h"


#define RP_PREFIX      KERN_INFO "[RP] "


struct msg_entry {
	u32 pid;
	u32 tid;
	u64 pc_addr;
	u64 caller_pc_addr;
	u32 cpu_num;
	u32 cnt_args;
	char args[0];
} __packed;

struct msg_exit {
	u32 pid;
	u32 tid;
	u64 pc_addr;
	u64 caller_pc_addr;
	u32 cpu_num;
	char ret_val[0];
} __packed;


void rp_msg_entry(struct pt_regs *regs, unsigned long func_addr,
		  const char *fmt)
{
	int ret;
	struct task_struct *task = current;
	struct swap_msg *m;
	struct msg_entry *ent;
	void *p;
	size_t size;

	m = swap_msg_get(MSG_FUNCTION_ENTRY);
	p = swap_msg_payload(m);

	ent = p;
	ent->pid = task->tgid;
	ent->tid = task->pid;
	ent->pc_addr = func_addr;
	ent->caller_pc_addr = get_regs_ret_func(regs);
	ent->cpu_num = raw_smp_processor_id();
	ent->cnt_args = strlen(fmt);

	size = swap_msg_size(m);
	ret = swap_msg_pack_args(p + sizeof(*ent), size - sizeof(*ent),
				 fmt, regs);
	if (ret < 0) {
		printk(RP_PREFIX "ERROR: arguments packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ent) + ret);

put_msg:
	swap_msg_put(m);
}
EXPORT_SYMBOL_GPL(rp_msg_entry);

void rp_msg_exit(struct pt_regs *regs, unsigned long func_addr,
		 char ret_type, unsigned long ret_addr)
{
	int ret;
	struct task_struct *task = current;
	struct swap_msg *m;
	struct msg_exit *ext;
	void *p;
	size_t size;

	m = swap_msg_get(MSG_FUNCTION_EXIT);
	p = swap_msg_payload(m);

	ext = p;
	ext->pid = task->tgid;
	ext->tid = task->pid;
	ext->pc_addr = func_addr;
	ext->caller_pc_addr = ret_addr;
	ext->cpu_num = raw_smp_processor_id();

	size = swap_msg_size(m);
	ret = swap_msg_pack_ret_val(p + sizeof(*ext), size - sizeof(*ext),
				    ret_type, regs);
	if (ret < 0) {
		printk(RP_PREFIX "ERROR: return value packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, sizeof(*ext) + ret);

put_msg:
	swap_msg_put(m);
}
EXPORT_SYMBOL_GPL(rp_msg_exit);
