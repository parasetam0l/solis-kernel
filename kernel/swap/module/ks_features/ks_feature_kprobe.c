/**
 * @author Vyacheslav Cherkashin: SWAP ks_features implement
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
 * Copyright (C) Samsung Electronics, 2016
 *
 * @section DESCRIPTION
 *
 *  SWAP kernel features
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <ksyms/ksyms.h>
#include <kprobe/swap_kprobes.h>
#include <master/swap_initializer.h>
#include <writer/event_filter.h>
#include "ksf_msg.h"
#include "ks_features.h"
#include "features_data.c"
#include "ks_features_data.h"


/* ========================= HANDLERS ========================= */
static int entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;

	if (rp && check_event(current)) {
		struct ks_probe *ksp = container_of(rp, struct ks_probe, rp);
		const char *fmt = ksp->args;
		const unsigned long addr = ksp->rp.kp.addr;
		enum probe_t type = ksp->type;

		ksf_msg_entry(regs, addr, type, fmt);
	}

	return 0;
}

static int ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct kretprobe *rp = ri->rp;

	if (rp && check_event(current)) {
		struct ks_probe *ksp = container_of(rp, struct ks_probe, rp);
		const unsigned long func_addr = rp->kp.addr;
		const unsigned long ret_addr = (unsigned long)ri->ret_addr;
		enum probe_t type = ksp->type;

		ksf_msg_exit(regs, func_addr, ret_addr, type, 'x');
	}

	return 0;
}

/* ========================= HANDLERS ========================= */





static int register_syscall(size_t id)
{
	int ret;

	if (ksp[id].rp.kp.addr == 0)
		return 0;

	ksp[id].rp.entry_handler = entry_handler;
	ksp[id].rp.handler = ret_handler;

	ret = swap_register_kretprobe(&ksp[id].rp);

	return ret;
}

static int unregister_syscall(size_t id)
{
	if (ksp[id].rp.kp.addr == 0)
		return 0;

	swap_unregister_kretprobe(&ksp[id].rp);

	return 0;
}

static int unregister_multiple_syscalls(size_t *id_p, size_t cnt)
{
	struct kretprobe **rpp;
	const size_t end = ((size_t) 0) - 1;
	size_t i = 0, id;
	int ret = 0;

	if (cnt == 1)
		return unregister_syscall(id_p[0]);

	rpp = kmalloc(sizeof(*rpp) * cnt, GFP_KERNEL);
	--cnt;
	if (rpp == NULL) {
		for (; cnt != end; --cnt) {
			ret = unregister_syscall(id_p[cnt]);
			if (ret)
				return ret;
		}
		return ret;
	}

	for (; cnt != end; --cnt) {
		id = id_p[cnt];
		if (ksp[id].rp.kp.addr) {
				rpp[i] = &ksp[id].rp;
				++i;
		}
	}

	swap_unregister_kretprobes(rpp, i);
	kfree(rpp);

	return 0;
}

static int init_syscalls(void)
{
	size_t i;
	unsigned long addr, ni_syscall;
	const char *name;

	ni_syscall = swap_ksyms("sys_ni_syscall");

	for (i = 0; i < syscall_cnt; ++i) {
		name = get_sys_name(i);
		addr = swap_ksyms(name);
		if (addr == 0) {
			pr_err("ERROR: %s() not found\n", name);
		} else if (ni_syscall == addr) {
			pr_err("WARN: %s is not install\n", name);
			addr = 0;
		}

		ksp[i].rp.kp.addr = addr;
	}

	return 0;
}
