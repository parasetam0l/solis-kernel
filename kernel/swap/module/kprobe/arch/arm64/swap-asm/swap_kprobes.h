#ifndef _ASM_ARM64_KPROBES_H
#define _ASM_ARM64_KPROBES_H

/*
 * Copied from: arch/arm64/kernel/kprobes.h
 *
 * Copyright (C) 2013 Linaro Limited.
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/types.h>


#define MAX_INSN_SIZE		2
#define KPROBES_TRAMP_LEN	(MAX_INSN_SIZE * 4)  /* 4 - instruction size */


struct kprobe;
struct kp_core;
struct slot_manager;
struct arch_specific_insn;
struct kretprobe_instance;


typedef u32 kprobe_opcode_t;
typedef unsigned long (kp_core_pstate_check_t)(unsigned long);
typedef unsigned long (kp_core_condition_check_t)(struct kp_core *,
						  struct pt_regs *);
typedef void (kp_core_prepare_t)(struct kp_core *, struct arch_specific_insn *);
typedef void (kp_core_handler_t)(u32 opcode, long addr, struct pt_regs *);


/* architecture specific copy of original instruction */
struct arch_specific_insn {
	kprobe_opcode_t *insn;
	kp_core_pstate_check_t *pstate_cc;
	kp_core_condition_check_t *check_condn;
	kp_core_prepare_t *prepare;
	kp_core_handler_t *handler;
};

struct prev_kp_core {
	struct kp_core *p;
	unsigned int status;
	unsigned long restore_addr;	/* restore address after single step */
};

enum ss_status {
	KP_CORE_STEP_NONE,
	KP_CORE_STEP_PENDING,
};

/* Single step context for kp_core */
struct kp_core_step_ctx {
	enum ss_status ss_status;
	unsigned long match_addr;
};

/* kp_core control block */
struct kp_core_ctlblk {
	unsigned int kp_core_status;
	struct prev_kp_core prev_kp_core;
	struct kp_core_step_ctx ss_ctx;
};


static inline unsigned long swap_get_karg(struct pt_regs *regs,
					  unsigned long n)
{
	return n < 8 ?  regs->regs[n] : *(((long *)regs->sp) + (n - 8));
}

static inline unsigned long swap_get_sarg(struct pt_regs *regs,
					  unsigned long n)
{
	return swap_get_karg(regs, n);
}

static inline unsigned long swap_get_kpc(struct pt_regs *regs)
{
	return regs->pc;
}

static inline void swap_set_kpc(struct pt_regs *regs, unsigned long val)
{
	regs->pc = val;
}


void arch_kp_core_arm(struct kp_core *p);
void arch_kp_core_disarm(struct kp_core *p);

int arch_kp_core_prepare(struct kp_core *p, struct slot_manager *sm);
void restore_previous_kp_core(struct kp_core_ctlblk *kcb);

void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs);
void swap_kretprobe_trampoline(void);


static inline unsigned long arch_get_task_pc(struct task_struct *p)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0xdeadc0de;
}

static inline void arch_set_task_pc(struct task_struct *p, unsigned long val)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
}

static inline int swap_setjmp_pre_handler(struct kprobe *p,
					  struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

/* jumper */
typedef unsigned long (*jumper_cb_t)(void *);

unsigned long get_jump_addr(void);
int set_jump_cb(unsigned long ret_addr, struct pt_regs *regs,
		jumper_cb_t cb, void *data, size_t size);


int arch_init_module_deps(void);

int swap_arch_init_kprobes(void);
void swap_arch_exit_kprobes(void);


#endif /* _ASM_ARM64_KPROBES_H */
