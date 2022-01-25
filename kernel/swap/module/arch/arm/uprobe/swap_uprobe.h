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
 * Copyright (C) Samsung Electronics, 2016
 *
 * 2016         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#ifndef _SWAP_ASM_ARM_UPROBE_H
#define _SWAP_ASM_ARM_UPROBE_H


#include <linux/uaccess.h>
#include "../probes/compat_arm64.h"


struct pt_regs;
struct uprobe;
struct uretprobe;
struct uretprobe_instance;


static inline unsigned long swap_get_upc_arm(struct pt_regs *regs)
{
	return regs->ARM_pc | !!thumb_mode(regs);
}

static inline void swap_set_upc_arm(struct pt_regs *regs, unsigned long val)
{
	if (val & 1) {
		regs->ARM_pc = val & ~1UL;
		regs->ARM_cpsr |= PSR_T_BIT;
	} else {
		regs->ARM_pc = val;
		regs->ARM_cpsr &= ~PSR_T_BIT;
	}
}

static inline unsigned long swap_get_uarg_arm(struct pt_regs *regs,
					      unsigned long n)
{
	u32 *ptr, val = 0;

	switch (n) {
	case 0:
		return regs->ARM_r0;
	case 1:
		return regs->ARM_r1;
	case 2:
		return regs->ARM_r2;
	case 3:
		return regs->ARM_r3;
	default:
		ptr = (u32 *)regs->ARM_sp + n - 4;
		if (get_user(val, ptr))
			pr_err("Failed to dereference a pointer[%p]\n", ptr);
		break;
	}

	return val;
}

static inline void swap_put_uarg_arm(struct pt_regs *regs, unsigned long n,
				     unsigned long val)
{
	u32 *ptr;

	switch (n) {
	case 0:
		regs->ARM_r0 = val;
		break;
	case 1:
		regs->ARM_r1 = val;
		break;
	case 2:
		regs->ARM_r2 = val;
		break;
	case 3:
		regs->ARM_r3 = val;
		break;
	default:
		ptr = (u32 *)regs->ARM_sp + n - 4;
		if (put_user(val, ptr))
			pr_err("Failed to dereference a pointer[%p]\n", ptr);
	}
}

static inline unsigned long swap_get_uret_addr_arm(struct pt_regs *regs)
{
	return regs->ARM_lr;
}

static inline void swap_set_uret_addr_arm(struct pt_regs *regs, unsigned long v)
{
	regs->ARM_lr = v;
}

int arch_prepare_uprobe_arm(struct uprobe *p);
int arch_arm_uprobe_arm(struct uprobe *p);
void arch_disarm_uprobe_arm(struct uprobe *p, struct task_struct *task);

int prepare_uretprobe_arm(struct uretprobe_instance *ri, struct pt_regs *regs);
void set_orig_ret_addr_arm(unsigned long orig_ret_addr, struct pt_regs *regs);
void arch_opcode_analysis_uretprobe_arm(struct uretprobe *rp);
unsigned long arch_get_trampoline_addr_arm(struct uprobe *p,
					   struct pt_regs *regs);
unsigned long arch_tramp_by_ri_arm(struct uretprobe_instance *ri);
int arch_disarm_urp_inst_arm(struct uretprobe_instance *ri,
			     struct task_struct *task);


#endif /* _SWAP_ASM_ARM_UPROBE_H */

