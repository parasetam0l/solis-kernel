/**
 * @file kprobe/arch/asm-arm/swap_kprobes.h
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>:
 *		initial implementation for ARM/MIPS
 * @author Alexey Gerenkov <a.gerenkov@samsung.com>:
 *		User-Space Probes initial implementation;
 *		Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>:
 *		redesign module for separating core and arch parts
 * @author Alexander Shirshikov <a.shirshikov@samsung.com>:
 *		 initial implementation for Thumb
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
 * ARM arch-dependent kprobes interface declaration.
 */


#ifndef _SWAP_ASM_ARM_KPROBES_H
#define _SWAP_ASM_ARM_KPROBES_H

#include <linux/sched.h>
#include <linux/compiler.h>
#include <arch/arm/probes/probes.h>

typedef unsigned long kprobe_opcode_t;


/** Kprobes trampoline length */
#define KPROBES_TRAMP_LEN              PROBES_TRAMP_LEN

/** User register offset */
#define UREGS_OFFSET 8

/**
 * @struct prev_kp_core
 * @brief Stores previous kp_core.
 * @var prev_kp_core::p
 * Pointer to kp_core struct.
 * @var prev_kp_core::status
 * Kprobe status.
 */
struct prev_kp_core {
	struct kp_core *p;
	unsigned long status;
};

/**
 * @brief Gets task pc.
 *
 * @param p Pointer to task_struct
 * @return Value in pc.
 */
static inline unsigned long arch_get_task_pc(struct task_struct *p)
{
	return task_thread_info(p)->cpu_context.pc;
}

/**
 * @brief Sets task pc.
 *
 * @param p Pointer to task_struct.
 * @param val Value that should be set.
 * @return Void.
 */
static inline void arch_set_task_pc(struct task_struct *p, unsigned long val)
{
	task_thread_info(p)->cpu_context.pc = val;
}

/**
 * @brief Gets syscall registers.
 *
 * @param sp Pointer to stack.
 * @return Pointer to CPU regs data.
 */
static inline struct pt_regs *swap_get_syscall_uregs(unsigned long sp)
{
	return (struct pt_regs *)(sp + UREGS_OFFSET);
}

/**
 * @brief Gets stack pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @return Stack address.
 */
static inline unsigned long swap_get_stack_ptr(struct pt_regs *regs)
{
	return regs->ARM_sp;
}

/**
 * @brief Sets stack pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @param sp New stack pointer value.
 * @return Void
 */
static inline void swap_set_stack_ptr(struct pt_regs *regs, unsigned long sp)
{
	regs->ARM_sp = sp;
}

/**
 * @brief Gets instruction pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @return Pointer to pc.
 */
static inline unsigned long swap_get_kpc(struct pt_regs *regs)
{
	return regs->ARM_pc | !!thumb_mode(regs);
}

/**
 * @brief Sets instruction pointer.
 *
 * @param regs Pointer to CPU registers data.
 * @param val Address that should be stored in pc.
 * @return Void.
 */
static inline void swap_set_kpc(struct pt_regs *regs, unsigned long val)
{
	if (val & 1) {
		regs->ARM_pc = val & ~1;
		regs->ARM_cpsr |= PSR_T_BIT;
	} else {
		regs->ARM_pc = val;
		regs->ARM_cpsr &= ~PSR_T_BIT;
	}
}

/**
 * @brief Gets specified argument.
 *
 * @param regs Pointer to CPU registers data.
 * @param num Number of the argument.
 * @return Argument value.
 */
static inline unsigned long swap_get_arg(struct pt_regs *regs, int num)
{
	return regs->uregs[num];
}

/**
 * @brief Sets specified argument.
 *
 * @param regs Pointer to CPU registers data.
 * @param num Number of the argument.
 * @param val New argument value.
 * @return Void.
 */
static inline void swap_set_arg(struct pt_regs *regs, int num,
				unsigned long val)
{
	regs->uregs[num] = val;
}

/**
 * @struct kp_core_ctlblk
 * @brief Per-cpu kp_core control block.
 * @var kp_core_ctlblk::kp_core_status
 * Kprobe status.
 * @var kp_core_ctlblk::prev_kp_core
 * Previous kp_core.
 */
struct kp_core_ctlblk {
	unsigned long kp_core_status;
	struct prev_kp_core prev_kp_core;
};

/**
 * @struct arch_specific_insn
 * @brief Architecture specific copy of original instruction.
 * @var arch_specific_insn::insn
 * Copy of the original instruction.
 */
struct arch_specific_insn {
	kprobe_opcode_t *insn;
};

typedef kprobe_opcode_t (*entry_point_t) (unsigned long, unsigned long,
					  unsigned long, unsigned long,
					  unsigned long, unsigned long);

struct undef_hook;

void swap_register_undef_hook(struct undef_hook *hook);
void swap_unregister_undef_hook(struct undef_hook *hook);

int arch_init_module_deps(void);

struct slot_manager;
struct kretprobe;
struct kretprobe_instance;
struct kp_core;
struct kprobe;

int arch_kp_core_prepare(struct kp_core *p, struct slot_manager *sm);
void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs);

void arch_kp_core_arm(struct kp_core *p);
void arch_kp_core_disarm(struct kp_core *p);

int swap_setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs);
int swap_longjmp_break_handler(struct kprobe *p, struct pt_regs *regs);

void restore_previous_kp_core(struct kp_core_ctlblk *kcb);

void __naked swap_kretprobe_trampoline(void);

/**
 * @brief Gets arguments of kernel functions.
 *
 * @param regs Pointer to CPU registers data.
 * @param n Number of the argument.
 * @return Argument value.
 */
static inline unsigned long swap_get_karg(struct pt_regs *regs, unsigned long n)
{
	switch (n) {
	case 0:
		return regs->ARM_r0;
	case 1:
		return regs->ARM_r1;
	case 2:
		return regs->ARM_r2;
	case 3:
		return regs->ARM_r3;
	}

	return *((unsigned long *)regs->ARM_sp + n - 4);
}

/**
 * @brief swap_get_karg wrapper.
 *
 * @param regs Pointer to CPU registers data.
 * @param n Number of the argument.
 * @return Argument value.
 */
static inline unsigned long swap_get_sarg(struct pt_regs *regs, unsigned long n)
{
	return swap_get_karg(regs, n);
}

/* jumper */
typedef unsigned long (*jumper_cb_t)(void *);

unsigned long get_jump_addr(void);
int set_jump_cb(unsigned long ret_addr, struct pt_regs *regs,
		jumper_cb_t cb, void *data, size_t size);

int swap_arch_init_kprobes(void);
void swap_arch_exit_kprobes(void);

/* void gen_insn_execbuf (void); */
/* void pc_dep_insn_execbuf (void); */
/* void gen_insn_execbuf_holder (void); */
/* void pc_dep_insn_execbuf_holder (void); */

#endif /* _SWAP_ASM_ARM_KPROBES_H */
