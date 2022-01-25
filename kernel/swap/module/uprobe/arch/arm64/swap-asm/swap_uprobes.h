#ifndef _ASM_ARM64_UPROBES_H
#define _ASM_ARM64_UPROBES_H

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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/types.h>
#include <linux/ptrace.h>
#include <arch/arm/probes/probes.h>
#include <arch/arm/uprobe/swap_uprobe.h>


#define UP_TRAMP_INSN_CNT  3 /* | opcode | ss_bp | urp_bp | */
#define UPROBES_TRAMP_LEN  (UP_TRAMP_INSN_CNT * 4) /* 4 - instruction size */
#define URP_RET_BREAK_IDX  2


struct uprobe;
struct arch_insn;
struct uretprobe;
struct task_struct;
struct uretprobe_instance;


typedef unsigned long (uprobes_pstate_check_t)(unsigned long pstate);
typedef unsigned long (uprobes_condition_check_t)(struct uprobe *p,
						  struct pt_regs *regs);
typedef void (uprobes_prepare_t)(struct uprobe *p, struct arch_insn *asi);
typedef void (uprobes_handler_t)(u32 opcode, long addr, struct pt_regs *regs);


struct arch_insn {
	/* arm */
	struct arch_insn_arm insn;

	/* arm64 */
	uprobes_pstate_check_t *pstate_cc;
	uprobes_condition_check_t *check_condn;
	uprobes_prepare_t *prepare;
	uprobes_handler_t *handler;
};


typedef u32 uprobe_opcode_t;

static inline u32 swap_get_float(struct pt_regs *regs, unsigned long n)
{
	u32 *ptr;
	register unsigned long w0 asm ("w0");

	switch (n) {
	case 0:
		asm volatile("fmov w0, s0");
		break;
	case 1:
		asm volatile("fmov w0, s1");
		break;
	case 2:
		asm volatile("fmov w0, s2");
		break;
	case 3:
		asm volatile("fmov w0, s3");
		break;
	case 4:
		asm volatile("fmov w0, s4");
		break;
	case 5:
		asm volatile("fmov w0, s5");
		break;
	case 6:
		asm volatile("fmov w0, s6");
		break;
	case 7:
		asm volatile("fmov w0, s7");
		break;
	default:
		w0 = 0;
		ptr = (u32 *)((u64 *)regs->sp + n - 8);
		if (get_user(w0, ptr))
			pr_err("failed to dereference a pointer\n");

		break;
	}

	return w0;
}

static inline u64 swap_get_double(struct pt_regs *regs, unsigned long n)
{
	u64 *ptr;
	register unsigned long x0 asm ("x0");

	switch (n) {
	case 0:
		asm volatile("fmov x0, d0");
		break;
	case 1:
		asm volatile("fmov x0, d1");
		break;
	case 2:
		asm volatile("fmov x0, d2");
		break;
	case 3:
		asm volatile("fmov x0, d3");
		break;
	case 4:
		asm volatile("fmov x0, d4");
		break;
	case 5:
		asm volatile("fmov x0, d5");
		break;
	case 6:
		asm volatile("fmov x0, d6");
		break;
	case 7:
		asm volatile("fmov x0, d7");
		break;
	default:
		x0 = 0;
		ptr = (u64 *)regs->sp + n - 8;
		if (get_user(x0, ptr))
			pr_err("failed to dereference a pointer\n");

		break;
	}

	return x0;
}

static inline u32 swap_get_urp_float(struct pt_regs *regs)
{
	return swap_get_float(regs, 0);
}

static inline u64 swap_get_urp_double(struct pt_regs *regs)
{
	return swap_get_double(regs, 0);
}

static inline unsigned long swap_get_upc_arm64(struct pt_regs *regs)
{
	return regs->pc;
}

static inline void swap_set_upc_arm64(struct pt_regs *regs, unsigned long val)
{
	regs->pc = val;
}

static inline unsigned long swap_get_uarg_arm64(struct pt_regs *regs,
						unsigned long n)
{
	u64 *ptr, val;

	if (n < 8)
		return regs->regs[n];

	ptr = (u64 *)regs->sp + n - 8;
	if (get_user(val, ptr))
		pr_err("failed to dereference a pointer, ptr=%p\n", ptr);

	return val;
}

static inline void swap_put_uarg_arm64(struct pt_regs *regs, unsigned long n,
				       unsigned long val)
{
	if (n < 8) {
		regs->regs[n] = val;
	} else {
		u64 *ptr = (u64 *)regs->sp + n - 8;
		if (put_user(val, ptr))
			pr_err("Failed to dereference a pointer, ptr=%p\n", ptr);
	}
}

static inline unsigned long swap_get_uret_addr_arm64(struct pt_regs *regs)
{
	return regs->regs[30];
}

static inline void swap_set_uret_addr_arm64(struct pt_regs *regs,
					    unsigned long val)
{
	regs->regs[30] = val;
}

static inline unsigned long swap_get_upc(struct pt_regs *regs)
{
	if (compat_user_mode(regs))
		return swap_get_upc_arm(regs);
	else
		return swap_get_upc_arm64(regs);
}

static inline void swap_set_upc(struct pt_regs *regs, unsigned long val)
{
	if (compat_user_mode(regs))
		swap_set_upc_arm(regs, val);
	else
		swap_set_upc_arm64(regs, val);
}

static inline unsigned long swap_get_uarg(struct pt_regs *regs, unsigned long n)
{
	if (compat_user_mode(regs))
		return swap_get_uarg_arm(regs, n);
	else
		return swap_get_uarg_arm64(regs, n);
}

static inline void swap_put_uarg(struct pt_regs *regs, unsigned long n,
				 unsigned long val)
{
	if (compat_user_mode(regs))
		return swap_put_uarg_arm(regs, n, val);
	else
		return swap_put_uarg_arm64(regs, n, val);
}

static inline unsigned long swap_get_uret_addr(struct pt_regs *regs)
{
	if (compat_user_mode(regs))
		return swap_get_uret_addr_arm(regs);
	else
		return swap_get_uret_addr_arm64(regs);
}

static inline void swap_set_uret_addr(struct pt_regs *regs, unsigned long val)
{
	if (compat_user_mode(regs))
		swap_set_uret_addr_arm(regs, val);
	else
		swap_set_uret_addr_arm64(regs, val);
}

int arch_prepare_uprobe(struct uprobe *p);
void arch_remove_uprobe(struct uprobe *p);

static inline int setjmp_upre_handler(struct uprobe *p, struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

static inline int longjmp_break_uhandler(struct uprobe *p,
					 struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}


int arch_arm_uprobe(struct uprobe *p);
void arch_disarm_uprobe(struct uprobe *p, struct task_struct *task);


unsigned long arch_get_trampoline_addr(struct uprobe *p, struct pt_regs *regs);
void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs);
int arch_prepare_uretprobe(struct uretprobe_instance *ri,
			   struct pt_regs *regs);

void arch_opcode_analysis_uretprobe(struct uretprobe *rp);
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task);

static inline void arch_ujprobe_return(void)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
}

int swap_arch_init_uprobes(void);
void swap_arch_exit_uprobes(void);


#endif /* _ASM_ARM64_UPROBES_H */
