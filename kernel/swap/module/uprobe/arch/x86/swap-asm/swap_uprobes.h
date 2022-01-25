/**
 * @file uprobe/arch/asm-x86/swap_uprobes.h
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial
 * implementation; Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for
 * separating core and arch parts
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
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * Arch-dependent uprobe interface declaration.
 */

#ifndef _X86_SWAP_UPROBES_H
#define _X86_SWAP_UPROBES_H


#include <swap-asm/swap_kprobes.h>      /* FIXME: for UPROBES_TRAMP_LEN */


struct uprobe;
struct uretprobe;
struct uretprobe_instance;

typedef u8 uprobe_opcode_t;

/**
 * @struct arch_insn
 * @brief Architecture depend copy of original instruction.
 * @var arch_insn::insn
 * Copy of the original instruction.
 * @var arch_insn::boostable
 * If this flag is not 0, this kprobe can be boost when its
 * post_handler and break_handler is not set.
 */
struct arch_insn {
	int boostable;
};


static inline u32 swap_get_urp_float(struct pt_regs *regs)
{
	u32 st0;

	asm volatile ("fstps	%0" : "=m" (st0));

	return st0;
}

static inline u64 swap_get_urp_double(struct pt_regs *regs)
{
	u64 st1;

	asm volatile ("fstpl	%0" : "=m" (st1));

	return st1;
}

static inline void arch_ujprobe_return(void)
{
}

int arch_prepare_uprobe(struct uprobe *up);
int setjmp_upre_handler(struct uprobe *p, struct pt_regs *regs);
static inline int longjmp_break_uhandler(struct uprobe *p, struct pt_regs *regs)
{
	return 0;
}

static inline int arch_opcode_analysis_uretprobe(struct uretprobe *rp)
{
	return 0;
}

int arch_prepare_uretprobe(struct uretprobe_instance *ri, struct pt_regs *regs);
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task);
unsigned long arch_get_trampoline_addr(struct uprobe *p, struct pt_regs *regs);
void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs);
void arch_remove_uprobe(struct uprobe *up);
int arch_arm_uprobe(struct uprobe *p);
void arch_disarm_uprobe(struct uprobe *p, struct task_struct *task);

static inline unsigned long swap_get_upc(struct pt_regs *regs)
{
	return regs->ip;
}

static inline void swap_set_upc(struct pt_regs *regs, unsigned long val)
{
	regs->ip = val;
}

static inline unsigned long swap_get_ustack_val(struct pt_regs *regs,
						unsigned long n)
{
	u32 *ptr, addr = 0;

	ptr = (u32 *)regs->sp + n;
	if (get_user(addr, ptr))
		pr_err("Failed to dereference a pointer, ptr=%p\n", ptr);

	return addr;
}

static inline void swap_set_ustack_val(struct pt_regs *regs, unsigned long n,
				       unsigned long val)
{
	u32 *ptr;

	ptr = (u32 *)regs->sp + n;
	if (put_user(val, ptr))
		pr_err("Failed to dereference a pointer, ptr=%p\n", ptr);
}

static inline unsigned long swap_get_uarg(struct pt_regs *regs, unsigned long n)
{
	/* 1 - return address saved on top of the stack */
	return swap_get_ustack_val(regs, n + 1);
}

static inline void swap_put_uarg(struct pt_regs *regs, unsigned long n,
				 unsigned long val)
{
	/* 1 - return address saved on top of the stack */
	swap_set_ustack_val(regs, n + 1, val);
}

static inline unsigned long swap_get_uret_addr(struct pt_regs *regs)
{
	return swap_get_ustack_val(regs, 0);
}

static inline void swap_set_uret_addr(struct pt_regs *regs, unsigned long val)
{
	swap_set_ustack_val(regs, 0, val);
}


int swap_arch_init_uprobes(void);
void swap_arch_exit_uprobes(void);

#endif /* _X86_SWAP_UPROBES_H */
