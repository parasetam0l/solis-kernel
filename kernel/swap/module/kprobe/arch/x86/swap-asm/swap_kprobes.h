/**
 * @file kprobe/arch/asm-x86/swap_kprobes.h
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
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
 * Copyright (C) IBM Corporation, 2002, 2004
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * Arch-dependent kprobes interface for x86 arch.
 */

#ifndef _SWAP_ASM_X86_KPROBES_H
#define _SWAP_ASM_X86_KPROBES_H


#include <linux/version.h>
#include <kprobe/swap_kprobes_deps.h>

/**
 * @brief Opcode type.
 */
typedef u8 kprobe_opcode_t;

#define BREAKPOINT_INSTRUCTION          0xcc
#define RELATIVEJUMP_INSTRUCTION        0xe9

#define BP_INSN_SIZE                    1
#define MAX_INSN_SIZE                   16
#define MAX_STACK_SIZE                  64

#define MIN_STACK_SIZE(ADDR)   (((MAX_STACK_SIZE) <			  \
			(((unsigned long)current_thread_info())  \
			 + THREAD_SIZE - (ADDR)))		  \
		? (MAX_STACK_SIZE)			  \
		: (((unsigned long)current_thread_info()) \
			+ THREAD_SIZE - (ADDR)))


#define EREG(rg)                rg
#define XREG(rg)                rg
#define ORIG_EAX_REG            orig_ax


#define TF_MASK                         X86_EFLAGS_TF
#define IF_MASK	                        X86_EFLAGS_IF
#define UPROBES_TRAMP_LEN               (MAX_INSN_SIZE+sizeof(kprobe_opcode_t))
#define UPROBES_TRAMP_INSN_IDX		0
#define UPROBES_TRAMP_RET_BREAK_IDX     MAX_INSN_SIZE
#define KPROBES_TRAMP_LEN		MAX_INSN_SIZE
#define KPROBES_TRAMP_INSN_IDX          0

static inline int swap_user_mode(struct pt_regs *regs)
{
	return user_mode_vm(regs);
}


static inline unsigned long arch_get_task_pc(struct task_struct *p)
{
	/* FIXME: Not implemented yet */
	return 0;
}

static inline void arch_set_task_pc(struct task_struct *p, unsigned long val)
{
	/* FIXME: Not implemented yet */
}

static inline struct pt_regs *swap_get_syscall_uregs(unsigned long sp)
{
	return NULL; /* FIXME currently not implemented for x86 */
}

static inline unsigned long swap_get_stack_ptr(struct pt_regs *regs)
{
	return regs->EREG(sp);
}

static inline void swap_set_stack_ptr(struct pt_regs *regs, unsigned long sp)
{
	regs->EREG(sp) = sp;
}

static inline unsigned long swap_get_kpc(struct pt_regs *regs)
{
	return regs->ip;
}

static inline void swap_set_kpc(struct pt_regs *regs, unsigned long val)
{
	regs->ip = val;
}

static inline unsigned long swap_get_arg(struct pt_regs *regs, int num)
{
	unsigned long arg = 0;
	read_proc_vm_atomic(current, regs->EREG(sp) + (1 + num) * 4,
			&arg, sizeof(arg));
	return arg;
}

static inline void swap_set_arg(struct pt_regs *regs, int num,
				unsigned long val)
{
	write_proc_vm_atomic(current, regs->EREG(sp) + (1 + num) * 4,
			&val, sizeof(val));
}

/**
 * @struct prev_kp_core
 * @brief Stores previous kp_core.
 * @var prev_kp_core::kp
 * Pointer to kp_core struct.
 * @var prev_kp_core::status
 * kp_core status.
 */
struct prev_kp_core {
	struct kp_core *p;
	unsigned long status;
};

/**
 * @struct kp_core_ctlblk
 * @brief Per-cpu kp_core control block.
 * @var kp_core_ctlblk::kp_core_status
 * kp_core status.
 * @var kp_core_ctlblk::prev_kp_core
 * Previous kp_core.
 */
struct kp_core_ctlblk {
	unsigned long kp_core_status;
	struct prev_kp_core prev_kp_core;
	struct pt_regs jprobe_saved_regs;
	unsigned long kp_core_old_eflags;
	unsigned long kp_core_saved_eflags;
	unsigned long *jprobe_saved_esp;
	kprobe_opcode_t jprobes_stack[MAX_STACK_SIZE];
};


/**
 * @struct arch_specific_insn
 * @brief Architecture specific copy of original instruction.
 * @var arch_specific_insn::insn
 * Copy of the original instruction.
 * @var arch_specific_insn::boostable
 * If this flag is not 0, this kp_core can be boost when its
 * post_handler and break_handler is not set.
 */
struct arch_specific_insn {
	kprobe_opcode_t *insn;
	int boostable;
};

/**
 * @brief Entry point.
 */
typedef kprobe_opcode_t (*entry_point_t) (unsigned long, unsigned long,
					  unsigned long, unsigned long,
					  unsigned long, unsigned long);

int arch_init_module_deps(void);

struct kprobe;
struct kp_core;
struct slot_manager;
struct kretprobe_instance;

int arch_kp_core_prepare(struct kp_core *p, struct slot_manager *sm);
void arch_kp_core_arm(struct kp_core *core);
void arch_kp_core_disarm(struct kp_core *core);
int swap_setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs);
void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs);
void swap_kretprobe_trampoline(void);

void restore_previous_kp_core(struct kp_core_ctlblk *kcb);
int swap_can_boost(kprobe_opcode_t *opcodes);
static inline int arch_check_insn(struct arch_specific_insn *ainsn)
{
	return 0;
}

unsigned long swap_kernel_sp(struct pt_regs *regs);

static inline unsigned long swap_get_karg(struct pt_regs *regs, unsigned long n)
{
	switch (n) {
	case 0:
		return regs->ax;
	case 1:
		return regs->dx;
	case 2:
		return regs->cx;
	}

	/*
	 * 2 = 3 - 1
	 * 3 - arguments from registers
	 * 1 - return address saved on top of the stack
	 */
	return *((unsigned long *)swap_kernel_sp(regs) + n - 2);
}

static inline unsigned long swap_get_sarg(struct pt_regs *regs, unsigned long n)
{
	/* 1 - return address saved on top of the stack */
	return *((unsigned long *)kernel_stack_pointer(regs) + n + 1);
}

/* jumper */
typedef unsigned long (*jumper_cb_t)(void *);

unsigned long get_jump_addr(void);
int set_jump_cb(unsigned long ret_addr, struct pt_regs *regs,
		jumper_cb_t cb, void *data, size_t size);

int swap_arch_init_kprobes(void);
void swap_arch_exit_kprobes(void);

#endif /* _SWAP_ASM_X86_KPROBES_H */
