/**
 * arch/asm-x86/swap_kprobes.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 * @author Stanislav Andreev <s.andreev@samsung.com>: added time debug profiling support; BUG() message fix
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
 *
 * @section DESCRIPTION
 *
 * SWAP krpobes arch-dependend part for x86.
 */


#include <linux/kconfig.h>

#ifdef CONFIG_SWAP_KERNEL_IMMUTABLE
# error "Kernel is immutable"
#endif /* CONFIG_SWAP_KERNEL_IMMUTABLE */


#include<linux/module.h>
#include <linux/kdebug.h>

#include "swap_kprobes.h"
#include <kprobe/swap_kprobes.h>
#include <kprobe/swap_td_raw.h>
#include <kprobe/swap_kdebug.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_kprobes_deps.h>


static int (*swap_fixup_exception)(struct pt_regs *regs);
static void *(*swap_text_poke)(void *addr, const void *opcode, size_t len);
static void (*swap_show_registers)(struct pt_regs *regs);


#define SWAP_SAVE_REGS_STRING			\
	/* Skip cs, ip, orig_ax and gs. */	\
	"subl $16, %esp\n"			\
	"pushl %fs\n"				\
	"pushl %es\n"				\
	"pushl %ds\n"				\
	"pushl %eax\n"				\
	"pushl %ebp\n"				\
	"pushl %edi\n"				\
	"pushl %esi\n"				\
	"pushl %edx\n"				\
	"pushl %ecx\n"				\
	"pushl %ebx\n"
#define SWAP_RESTORE_REGS_STRING		\
	"popl %ebx\n"				\
	"popl %ecx\n"				\
	"popl %edx\n"				\
	"popl %esi\n"				\
	"popl %edi\n"				\
	"popl %ebp\n"				\
	"popl %eax\n"				\
	/* Skip ds, es, fs, gs, orig_ax, and ip. Note: don't pop cs here*/\
	"addl $24, %esp\n"


/*
 * Function return probe trampoline:
 *      - init_kprobes() establishes a probepoint here
 *      - When the probed function returns, this probe
 *        causes the handlers to fire
 */
__asm(
	".global swap_kretprobe_trampoline\n"
	"swap_kretprobe_trampoline:\n"
	"pushf\n"
	SWAP_SAVE_REGS_STRING
	"movl %esp, %eax\n"
	"call swap_trampoline_handler\n"
	/* move eflags to cs */
	"movl 56(%esp), %edx\n"
	"movl %edx, 52(%esp)\n"
	/* replace saved flags with true return address. */
	"movl %eax, 56(%esp)\n"
	SWAP_RESTORE_REGS_STRING
	"popf\n"
	"ret\n"
);

/* insert a jmp code */
static __always_inline void set_jmp_op(void *from, void *to)
{
	struct __arch_jmp_op {
		char op;
		long raddr;
	} __packed * jop;
	jop = (struct __arch_jmp_op *) from;
	jop->raddr = (long) (to) - ((long) (from) + 5);
	jop->op = RELATIVEJUMP_INSTRUCTION;
}

/**
 * @brief Check if opcode can be boosted.
 *
 * @param opcodes Opcode to check.
 * @return Non-zero if opcode can be boosted.
 */
int swap_can_boost(kprobe_opcode_t *opcodes)
{
#define W(row, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
	(((b0##UL << 0x0)|(b1##UL << 0x1)|(b2##UL << 0x2)|(b3##UL << 0x3) |   \
	  (b4##UL << 0x4)|(b5##UL << 0x5)|(b6##UL << 0x6)|(b7##UL << 0x7) |   \
	  (b8##UL << 0x8)|(b9##UL << 0x9)|(ba##UL << 0xa)|(bb##UL << 0xb) |   \
	  (bc##UL << 0xc)|(bd##UL << 0xd)|(be##UL << 0xe)|(bf##UL << 0xf))    \
	 << (row % 32))
	/*
	 * Undefined/reserved opcodes, conditional jump, Opcode Extension
	 * Groups, and some special opcodes can not be boost.
	 */
	static const unsigned long twobyte_is_boostable[256 / 32] = {
		/*      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */
		W(0x00, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0) |
		W(0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		W(0x20, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) |
		W(0x30, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		W(0x40, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) |
		W(0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
		W(0x60, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1) |
		W(0x70, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1),
		W(0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) |
		W(0x90, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
		W(0xa0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) |
		W(0xb0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1),
		W(0xc0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1) |
		W(0xd0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1),
		W(0xe0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) |
		W(0xf0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0)
		/*      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */

	};
#undef W
	kprobe_opcode_t opcode;
	kprobe_opcode_t *orig_opcodes = opcodes;
retry:
	if (opcodes - orig_opcodes > MAX_INSN_SIZE - 1)
		return 0;
	opcode = *(opcodes++);

	/* 2nd-byte opcode */
	if (opcode == 0x0f) {
		if (opcodes - orig_opcodes > MAX_INSN_SIZE - 1)
			return 0;
		return test_bit(*opcodes, twobyte_is_boostable);
	}

	switch (opcode & 0xf0) {
	case 0x60:
		if (0x63 < opcode && opcode < 0x67)
			goto retry;	/* prefixes */
		/* can't boost Address-size override and bound */
		return (opcode != 0x62 && opcode != 0x67);
	case 0x70:
		return 0;	/* can't boost conditional jump */
	case 0xc0:
		/* can't boost software-interruptions */
		return (0xc1 < opcode && opcode < 0xcc) || opcode == 0xcf;
	case 0xd0:
		/* can boost AA* and XLAT */
		return (opcode == 0xd4 || opcode == 0xd5 || opcode == 0xd7);
	case 0xe0:
		/* can boost in/out and absolute jmps */
		return ((opcode & 0x04) || opcode == 0xea);
	case 0xf0:
		if ((opcode & 0x0c) == 0 && opcode != 0xf1)
			goto retry;	/* lock/rep(ne) prefix */
		/* clear and set flags can be boost */
		return (opcode == 0xf5 || (0xf7 < opcode && opcode < 0xfe));
	default:
		if (opcode == 0x26 || opcode == 0x36 || opcode == 0x3e)
			goto retry;	/* prefixes */
		/* can't boost CS override and call */
		return (opcode != 0x2e && opcode != 0x9a);
	}
}
EXPORT_SYMBOL_GPL(swap_can_boost);

/*
 * returns non-zero if opcode modifies the interrupt flag.
 */
static int is_IF_modifier(kprobe_opcode_t opcode)
{
	switch (opcode) {
	case 0xfa:		/* cli */
	case 0xfb:		/* sti */
	case 0xcf:		/* iret/iretd */
	case 0x9d:		/* popf/popfd */
		return 1;
	}
	return 0;
}

/**
 * @brief Creates trampoline for kprobe.
 *
 * @param p Pointer to kprobe.
 * @param sm Pointer to slot manager
 * @return 0 on success, error code on error.
 */
int arch_kp_core_prepare(struct kp_core *p, struct slot_manager *sm)
{
	/* insn: must be on special executable page on i386. */
	p->ainsn.insn = swap_slot_alloc(sm);
	if (p->ainsn.insn == NULL)
		return -ENOMEM;

	memcpy(p->ainsn.insn, (void *)p->addr, MAX_INSN_SIZE);

	p->opcode = *(char *)p->addr;
	p->ainsn.boostable = swap_can_boost((void *)p->addr) ? 0 : -1;

	return 0;
}

/**
 * @brief Prepares singlestep for current CPU.
 *
 * @param p Pointer to kprobe.
 * @param regs Pointer to CPU registers data.
 * @return Void.
 */
static void prepare_singlestep(struct kp_core *p, struct pt_regs *regs)
{
	regs->flags |= TF_MASK;
	regs->flags &= ~IF_MASK;

	/* single step inline if the instruction is an int3 */
	if (p->opcode == BREAKPOINT_INSTRUCTION)
		regs->ip = (unsigned long)p->addr;
	else
		regs->ip = (unsigned long)p->ainsn.insn;
}

/**
 * @brief Saves previous kprobe.
 *
 * @param kcb Pointer to kp_core_ctlblk struct whereto save current kprobe.
 * @param p_run Pointer to kprobe.
 * @return Void.
 */
static void save_previous_kp_core(struct kp_core_ctlblk *kcb, struct kp_core *cur)
{
	if (kcb->prev_kp_core.p != NULL) {
		panic("no space to save new probe[]: "
		      "task = %d/%s, prev %08lx, current %08lx, new %08lx,",
		      current->pid, current->comm, kcb->prev_kp_core.p->addr,
		      kp_core_running()->addr, cur->addr);
	}


	kcb->prev_kp_core.p = kp_core_running();
	kcb->prev_kp_core.status = kcb->kp_core_status;
}

/**
 * @brief Restores previous kp_core.
 *
 * @param kcb Pointer to kp_core_ctlblk which contains previous kp_core.
 * @return Void.
 */
void restore_previous_kp_core(struct kp_core_ctlblk *kcb)
{
	kp_core_running_set(kcb->prev_kp_core.p);
	kcb->kp_core_status = kcb->prev_kp_core.status;
	kcb->prev_kp_core.p = NULL;
	kcb->prev_kp_core.status = 0;
}

static void set_current_kp_core(struct kp_core *p, struct pt_regs *regs,
				struct kp_core_ctlblk *kcb)
{
	kp_core_running_set(p);
	kcb->kp_core_saved_eflags = kcb->kp_core_old_eflags =
		(regs->EREG(flags) & (TF_MASK | IF_MASK));
	if (is_IF_modifier(p->opcode))
		kcb->kp_core_saved_eflags &= ~IF_MASK;
}

static int setup_singlestep(struct kp_core *p, struct pt_regs *regs,
			    struct kp_core_ctlblk *kcb)
{
#if !defined(CONFIG_PREEMPT) || defined(CONFIG_PM)
	if (p->ainsn.boostable == 1) {
		/* Boost up -- we can execute copied instructions directly */
		kp_core_running_set(NULL);
		regs->ip = (unsigned long)p->ainsn.insn;

		return 1;
	}
#endif /* !CONFIG_PREEMPT */

	prepare_singlestep(p, regs);
	kcb->kp_core_status = KPROBE_HIT_SS;

	return 1;
}


struct regs_td {
	struct pt_regs *sp_regs;
	struct pt_regs regs;
};

static struct td_raw kp_tdraw;
static DEFINE_PER_CPU(struct regs_td, per_cpu_regs_td_i);
static DEFINE_PER_CPU(struct regs_td, per_cpu_regs_td_st);

static struct regs_td *current_regs_td(void)
{
	if (swap_in_interrupt())
		return &__get_cpu_var(per_cpu_regs_td_i);
	else if (switch_to_bits_get(current_kctx, SWITCH_TO_ALL))
		return &__get_cpu_var(per_cpu_regs_td_st);

	return (struct regs_td *)swap_td_raw(&kp_tdraw, current);
}

/** Stack address. */
unsigned long swap_kernel_sp(struct pt_regs *regs)
{
	struct pt_regs *sp_regs = current_regs_td()->sp_regs;

	if (sp_regs == NULL)
		sp_regs = regs;

	return kernel_stack_pointer(sp_regs);
}
EXPORT_SYMBOL_GPL(swap_kernel_sp);

void exec_trampoline(void);
void exec_trampoline_int3(void);
__asm(
	".text\n"
	"exec_trampoline:\n"
	"call	exec_handler\n"
	"exec_trampoline_int3:\n"
	"int3\n"
);

static int __used exec_handler(void)
{
	struct kp_core *p = kp_core_running();
	struct pt_regs *regs = &current_regs_td()->regs;

	return p->handlers.pre(p, regs);
}

static int after_exec_trampoline(struct pt_regs *regs)
{
	int ret = (int)regs->ax;
	struct kp_core *p = kp_core_running();
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	/*
	 * Restore regs from stack.
	 * Don't restore SP and SS registers because they are invalid (- 8)
	 */
	memcpy(regs, &current_regs_td()->regs, sizeof(*regs) - 8);

	if (ret) {
		kp_core_put(p);
		return 1;
	}

	setup_singlestep(p, regs, kcb);
	if (!(regs->flags & TF_MASK))
		kp_core_put(p);

	return 1;
}

#define KSTAT_NOT_FOUND		0x00
#define KSTAT_FOUND		0x01
#define KSTAT_PREPARE_KCB	0x02

static unsigned long kprobe_pre_handler(struct kp_core *p,
					struct pt_regs *regs,
					struct kp_core_ctlblk *kcb)
{
	int ret = KSTAT_NOT_FOUND;
	unsigned long addr = regs->ip - 1;

	/* Check we're not actually recursing */
	if (kp_core_running()) {
		if (p) {
			if (kcb->kp_core_status == KPROBE_HIT_SS &&
			    *p->ainsn.insn == BREAKPOINT_INSTRUCTION) {
				regs->flags &= ~TF_MASK;
				regs->flags |= kcb->kp_core_saved_eflags;
				goto out;
			}

			/* We have reentered the kprobe_handler(), since
			 * another probe was hit while within the handler.
			 * We here save the original kprobes variables and
			 * just single step on the instruction of the new probe
			 * without calling any user handlers.
			 */
			save_previous_kp_core(kcb, p);
			set_current_kp_core(p, regs, kcb);
			prepare_singlestep(p, regs);
			kcb->kp_core_status = KPROBE_REENTER;

			ret = KSTAT_FOUND;
			goto out;
		} else {
			if (*(char *)addr != BREAKPOINT_INSTRUCTION) {
				/* The breakpoint instruction was removed by
				 * another cpu right after we hit, no further
				 * handling of this interrupt is appropriate
				 */
				regs->EREG(ip) -= sizeof(kprobe_opcode_t);

				ret = KSTAT_FOUND;
				goto out;
			}

			goto out;
		}
	}

	if (!p) {
		if (*(char *)addr != BREAKPOINT_INSTRUCTION) {
			/*
			 * The breakpoint instruction was removed right
			 * after we hit it.  Another cpu has removed
			 * either a probepoint or a debugger breakpoint
			 * at this address.  In either case, no further
			 * handling of this interrupt is appropriate.
			 * Back up over the (now missing) int3 and run
			 * the original instruction.
			 */
			regs->EREG(ip) -= sizeof(kprobe_opcode_t);

			ret = KSTAT_FOUND;
		}

		goto out;
	}

	set_current_kp_core(p, regs, kcb);
	kcb->kp_core_status = KPROBE_HIT_ACTIVE;

	ret = KSTAT_PREPARE_KCB;
out:
	return ret;
}

static int __kprobe_handler(struct pt_regs *regs)
{
	int ret;
	struct kp_core *p;
	struct kp_core_ctlblk *kcb;
	unsigned long addr = regs->ip - 1;
	struct kctx *ctx = current_kctx;

	if (addr == sched_addr)
		switch_to_bits_set(ctx, SWITCH_TO_KP);

	kcb = kp_core_ctlblk();

	rcu_read_lock();
	p = kp_core_by_addr(addr);
	kp_core_get(p);
	rcu_read_unlock();

	if (able2resched(ctx)) {
		ret = kprobe_pre_handler(p, regs, kcb);
		if (ret == KSTAT_PREPARE_KCB) {
			struct regs_td *rtd = current_regs_td();

			/* save regs to stack */
			rtd->regs = *regs;
			rtd->sp_regs = regs;

			regs->ip = (unsigned long)exec_trampoline;
			return 1;
		}

		if (!(regs->flags & TF_MASK))
			kp_core_put(p);
	} else {
		ret = kprobe_pre_handler(p, regs, kcb);
		if (ret == KSTAT_PREPARE_KCB) {
			int rr;

			current_regs_td()->sp_regs = NULL;
			rr = p->handlers.pre(p, regs);
			if (rr) {
				switch_to_bits_reset(ctx, SWITCH_TO_KP);
				kp_core_put(p);
				return 1;
			}

			setup_singlestep(p, regs, kcb);
		}

		/*
		 * If TF is enabled then processing instruction
		 * takes place in two stages.
		 */
		if (regs->flags & TF_MASK) {
			preempt_disable();
		} else {
			switch_to_bits_reset(ctx, SWITCH_TO_KP);
			kp_core_put(p);
		}
	}

	return !!ret;
}

static int kprobe_handler(struct pt_regs *regs)
{
	int ret;

	if (regs->ip == (unsigned long)exec_trampoline_int3 + 1)
		ret = after_exec_trampoline(regs);
	else
		ret = __kprobe_handler(regs);

	return ret;
}

/**
 * @brief Probe pre handler.
 *
 * @param p Pointer to fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return 0.
 */
int swap_setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct jprobe *jp = container_of(p, struct jprobe, kp);
	kprobe_pre_entry_handler_t pre_entry;

	unsigned long addr;
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	pre_entry = (kprobe_pre_entry_handler_t) jp->pre_entry;

	kcb->jprobe_saved_regs = *regs;
	kcb->jprobe_saved_esp = (unsigned long *)swap_kernel_sp(regs);
	addr = (unsigned long)(kcb->jprobe_saved_esp);

	/* TBD: As Linus pointed out, gcc assumes that the callee
	 * owns the argument space and could overwrite it, e.g.
	 * tailcall optimization. So, to be absolutely safe
	 * we also save and restore enough stack bytes to cover
	 * the argument area. */
	memcpy(kcb->jprobes_stack, (kprobe_opcode_t *)addr,
	       MIN_STACK_SIZE(addr));
	regs->EREG(flags) &= ~IF_MASK;
	trace_hardirqs_off();

	regs->EREG(ip) = (unsigned long)(jp->entry);

	return 1;
}

/**
 * @brief Jprobe return end.
 *
 * @return Void.
 */
void swap_jprobe_return_end(void);

/**
 * @brief Jprobe return code.
 *
 * @return Void.
 */
void swap_jprobe_return(void)
{
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	asm volatile("       xchgl   %%ebx,%%esp\n"
		     "       int3\n"
		     "       .globl swap_jprobe_return_end\n"
		     "       swap_jprobe_return_end:\n"
		     "       nop\n"
		     : : "b" (kcb->jprobe_saved_esp) : "memory");
}
EXPORT_SYMBOL_GPL(swap_jprobe_return);

void arch_ujprobe_return(void)
{
}

/*
 * Called after single-stepping.  p->addr is the address of the
 * instruction whose first byte has been replaced by the "int 3"
 * instruction.  To avoid the SMP problems that can occur when we
 * temporarily put back the original opcode to single-step, we
 * single-stepped a copy of the instruction.  The address of this
 * copy is p->ainsn.insn.
 *
 * This function prepares to return from the post-single-step
 * interrupt.  We have to fix up the stack as follows:
 *
 * 0) Except in the case of absolute or indirect jump or call instructions,
 * the new eip is relative to the copied instruction.  We need to make
 * it relative to the original instruction.
 *
 * 1) If the single-stepped instruction was pushfl, then the TF and IF
 * flags are set in the just-pushed eflags, and may need to be cleared.
 *
 * 2) If the single-stepped instruction was a call, the return address
 * that is atop the stack is the address following the copied instruction.
 * We need to make it the address following the original instruction.
 *
 * This function also checks instruction size for preparing direct execution.
 */
static void resume_execution(struct kp_core *p,
			     struct pt_regs *regs,
			     struct kp_core_ctlblk *kcb)
{
	unsigned long *tos;
	unsigned long copy_eip = (unsigned long) p->ainsn.insn;
	unsigned long orig_eip = (unsigned long) p->addr;
	kprobe_opcode_t insns[2];

	regs->EREG(flags) &= ~TF_MASK;

	tos = (unsigned long *)swap_kernel_sp(regs);
	insns[0] = p->ainsn.insn[0];
	insns[1] = p->ainsn.insn[1];

	switch (insns[0]) {
	case 0x9c: /* pushfl */
		*tos &= ~(TF_MASK | IF_MASK);
		*tos |= kcb->kp_core_old_eflags;
		break;
	case 0xc2: /* iret/ret/lret */
	case 0xc3:
	case 0xca:
	case 0xcb:
	case 0xcf:
	case 0xea: /* jmp absolute -- eip is correct */
		/* eip is already adjusted, no more changes required */
		p->ainsn.boostable = 1;
		goto no_change;
	case 0xe8: /* call relative - Fix return addr */
		*tos = orig_eip + (*tos - copy_eip);
		break;
	case 0x9a: /* call absolute -- same as call absolute, indirect */
		*tos = orig_eip + (*tos - copy_eip);
		goto no_change;
	case 0xff:
		if ((insns[1] & 0x30) == 0x10) {
			/*
			 * call absolute, indirect
			 * Fix return addr; eip is correct.
			 * But this is not boostable
			 */
			*tos = orig_eip + (*tos - copy_eip);
			goto no_change;
		} else if (((insns[1] & 0x31) == 0x20) || /* jmp near, absolute
							   * indirect */
			 ((insns[1] & 0x31) == 0x21)) {
			/* jmp far, absolute indirect */
			/* eip is correct. And this is boostable */
			p->ainsn.boostable = 1;
			goto no_change;
		}
	default:
		break;
	}

	if (p->ainsn.boostable == 0) {
		if ((regs->EREG(ip) > copy_eip) &&
		    (regs->EREG(ip) - copy_eip) + 5 < MAX_INSN_SIZE) {
			/*
			 * These instructions can be executed directly if it
			 * jumps back to correct address.
			 */
			set_jmp_op((void *)regs->EREG(ip),
				   (void *)orig_eip +
				   (regs->EREG(ip) - copy_eip));
			p->ainsn.boostable = 1;
		} else {
			p->ainsn.boostable = -1;
		}
	}

	regs->EREG(ip) = orig_eip + (regs->EREG(ip) - copy_eip);

no_change:
	return;
}

/*
 * Interrupts are disabled on entry as trap1 is an interrupt gate and they
 * remain disabled thoroughout this function.
 */
static int post_kprobe_handler(struct pt_regs *regs)
{
	struct kctx *ctx = current_kctx;
	struct kp_core *cur = kp_core_running();
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	if (!cur)
		return 0;

	resume_execution(cur, regs, kcb);
	regs->flags |= kcb->kp_core_saved_eflags;
#ifndef CONFIG_X86
	trace_hardirqs_fixup_flags(regs->EREG(flags));
#endif /* CONFIG_X86 */
	/* Restore back the original saved kprobes variables and continue. */
	if (kcb->kp_core_status == KPROBE_REENTER) {
		restore_previous_kp_core(kcb);
		goto out;
	}
	kp_core_running_set(NULL);

out:
	if (!able2resched(ctx))
		swap_preempt_enable_no_resched();

	switch_to_bits_reset(ctx, SWITCH_TO_KP);
	kp_core_put(cur);

	/*
	 * if somebody else is singlestepping across a probe point, eflags
	 * will have TF set, in which case, continue the remaining processing
	 * of do_debug, as if this is not a probe hit.
	 */
	if (regs->EREG(flags) & TF_MASK)
		return 0;

	return 1;
}

static int kprobe_fault_handler(struct pt_regs *regs, int trapnr)
{
	struct kp_core *cur = kp_core_running();
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	switch (kcb->kp_core_status) {
	case KPROBE_HIT_SS:
	case KPROBE_REENTER:
		/*
		 * We are here because the instruction being single
		 * stepped caused a page fault. We reset the current
		 * kprobe and the eip points back to the probe address
		 * and allow the page fault handler to continue as a
		 * normal page fault.
		 */
		regs->ip = cur->addr;
		regs->flags |= kcb->kp_core_old_eflags;
		if (kcb->kp_core_status == KPROBE_REENTER)
			restore_previous_kp_core(kcb);
		else
			kp_core_running_set(NULL);
		break;
	case KPROBE_HIT_ACTIVE:
	case KPROBE_HIT_SSDONE:
		/*
		 * In case the user-specified fault handler returned
		 * zero, try to fix up.
		 */
		if (swap_fixup_exception(regs))
			return 1;

		/*
		 * fixup_exception() could not handle it,
		 * Let do_page_fault() fix it.
		 */
		break;
	default:
		break;
	}
	return 0;
}

static int kprobe_exceptions_notify(struct notifier_block *self,
				    unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *) data;
	int ret = NOTIFY_DONE;

	DBPRINTF("val = %ld, data = 0x%X", val, (unsigned int) data);

	if (args->regs == NULL || swap_user_mode(args->regs))
		return ret;

	DBPRINTF("switch (val) %lu %d %d", val, DIE_INT3, DIE_TRAP);
	switch (val) {
#ifdef CONFIG_KPROBES
	case DIE_INT3:
#else
	case DIE_TRAP:
#endif
		DBPRINTF("before kprobe_handler ret=%d %p",
			 ret, args->regs);
		if (kprobe_handler (args->regs))
			ret = NOTIFY_STOP;
		DBPRINTF("after kprobe_handler ret=%d %p",
			 ret, args->regs);
		break;
	case DIE_DEBUG:
		if (post_kprobe_handler(args->regs))
			ret = NOTIFY_STOP;
		break;
	case DIE_GPF:
		if (kp_core_running() &&
		    kprobe_fault_handler(args->regs, args->trapnr))
			ret = NOTIFY_STOP;
		break;
	default:
		break;
	}
	DBPRINTF("ret=%d", ret);
	/* if(ret == NOTIFY_STOP) */
	/*	handled_exceptions++; */

	return ret;
}

static struct notifier_block kprobe_exceptions_nb = {
	.notifier_call = kprobe_exceptions_notify,
	.priority = INT_MAX
};

/**
 * @brief Arms kp_core.
 *
 * @param core Pointer to target kp_core.
 * @return Void.
 */
void arch_kp_core_arm(struct kp_core *p)
{
	swap_text_poke((void *)p->addr,
		       ((unsigned char[]){BREAKPOINT_INSTRUCTION}), 1);
}

/**
 * @brief Disarms kp_core.
 *
 * @param core Pointer to target kp_core.
 * @return Void.
 */
void arch_kp_core_disarm(struct kp_core *p)
{
	swap_text_poke((void *)p->addr, &p->opcode, 1);
}

/**
 * @brief Prepares kretprobes, saves ret address, makes function return to
 * trampoline.
 *
 * @param ri Pointer to kretprobe_instance.
 * @param regs Pointer to CPU registers data.
 * @return Void.
 */
void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	unsigned long *ptr_ret_addr = (unsigned long *)swap_kernel_sp(regs);

	/* for __switch_to probe */
	if ((unsigned long)ri->rp->kp.addr == sched_addr) {
		struct task_struct *next = (struct task_struct *)swap_get_karg(regs, 1);
		ri->sp = NULL;
		ri->task = next;
		switch_to_bits_set(kctx_by_task(next), SWITCH_TO_RP);
	} else {
		ri->sp = ptr_ret_addr;
	}

	/* Save the return address */
	ri->ret_addr = (unsigned long *)*ptr_ret_addr;

	/* Replace the return addr with trampoline addr */
	*ptr_ret_addr = (unsigned long)&swap_kretprobe_trampoline;
}





/*
 ******************************************************************************
 *                                   jumper                                   *
 ******************************************************************************
 */
struct cb_data {
	unsigned long ret_addr;
	unsigned long bx;

	jumper_cb_t cb;
	char data[0];
};

static unsigned long __used get_bx(struct cb_data *data)
{
	return data->bx;
}

static unsigned long __used jump_handler(struct cb_data *data)
{
	unsigned long ret_addr = data->ret_addr;

	/* call callback */
	data->cb(data->data);

	/* FIXME: potential memory leak, when process kill */
	kfree(data);

	return ret_addr;
}

void jump_trampoline(void);
__asm(
	"jump_trampoline:\n"
	"pushf\n"
	SWAP_SAVE_REGS_STRING
	"movl	%ebx, %eax\n"	/* data --> ax */
	"call	get_bx\n"
	"movl	%eax, (%esp)\n"	/* restore bx */
	"movl	%ebx, %eax\n"	/* data --> ax */
	"call	jump_handler\n"
	/* move flags to cs */
	"movl 56(%esp), %edx\n"
	"movl %edx, 52(%esp)\n"
	/* replace saved flags with true return address. */
	"movl %eax, 56(%esp)\n"
	SWAP_RESTORE_REGS_STRING
	"popf\n"
	"ret\n"
);

unsigned long get_jump_addr(void)
{
	return (unsigned long)&jump_trampoline;
}
EXPORT_SYMBOL_GPL(get_jump_addr);

int set_jump_cb(unsigned long ret_addr, struct pt_regs *regs,
		jumper_cb_t cb, void *data, size_t size)
{
	struct cb_data *cb_data;

	cb_data = kmalloc(sizeof(*cb_data) + size, GFP_ATOMIC);
	if (cb_data == NULL)
		return -ENOMEM;

	/* save data */
	if (size)
		memcpy(cb_data->data, data, size);

	/* save info for restore */
	cb_data->ret_addr = ret_addr;
	cb_data->cb = cb;
	cb_data->bx = regs->bx;

	/* save cb_data to bx */
	regs->bx = (long)cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(set_jump_cb);





/**
 * @brief Initializes x86 module deps.
 *
 * @return 0 on success, negative error code on error.
 */
int arch_init_module_deps()
{
	const char *sym;

	sym = "fixup_exception";
	swap_fixup_exception = (void *)swap_ksyms(sym);
	if (swap_fixup_exception == NULL)
		goto not_found;

	sym = "text_poke";
	swap_text_poke = (void *)swap_ksyms(sym);
	if (swap_text_poke == NULL)
		goto not_found;

	sym = "show_regs";
	swap_show_registers = (void *)swap_ksyms(sym);
	if (swap_show_registers == NULL)
		goto not_found;

	return 0;

not_found:
	printk(KERN_INFO "ERROR: symbol %s(...) not found\n", sym);
	return -ESRCH;
}

/**
 * @brief Initializes kprobes module for ARM arch.
 *
 * @return 0 on success, error code on error.
 */
int swap_arch_init_kprobes(void)
{
	int ret;

	ret = swap_td_raw_reg(&kp_tdraw, sizeof(struct regs_td));
	if (ret)
		return ret;

	ret = register_die_notifier(&kprobe_exceptions_nb);
	if (ret)
		swap_td_raw_unreg(&kp_tdraw);

	return ret;
}

/**
 * @brief Uninitializes kprobe module.
 *
 * @return Void.
 */
void swap_arch_exit_kprobes(void)
{
	unregister_die_notifier(&kprobe_exceptions_nb);
	swap_td_raw_unreg(&kp_tdraw);
}
