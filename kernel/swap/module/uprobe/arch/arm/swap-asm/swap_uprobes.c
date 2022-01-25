/**
 * uprobe/arch/asm-arm/swap_uprobes.c
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
 * Arch-dependent uprobe interface implementation for ARM.
 */


#include <linux/init.h>			/* need for asm/traps.h */
#include <linux/sched.h>		/* need for asm/traps.h */

#include <linux/ptrace.h>		/* need for asm/traps.h */
#include <asm/traps.h>

#include <kprobe/swap_slots.h>
#include <kprobe/swap_kprobes_deps.h>
#include <uprobe/swap_uprobes.h>
#include <arch/arm/probes/probes_arm.h>
#include <arch/arm/probes/probes_thumb.h>
#include <swap-asm/swap_kprobes.h>
#include "swap_uprobes.h"


/**
 * @brief Prepares uprobe for ARM.
 *
 * @param up Pointer to the uprobe.
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_prepare_uprobe(struct uprobe *p)
{
	int ret;

	ret = arch_prepare_uprobe_arm(p);
	if (!ret) {
		/* for uretprobe */
		add_uprobe_table(p);
	}

	return ret;
}

/**
 * @brief Analysis opcodes.
 *
 * @param rp Pointer to the uretprobe.
 * @return Void.
 */
void arch_opcode_analysis_uretprobe(struct uretprobe *rp)
{
	/* Remove retprobe if first insn overwrites lr */
	rp->thumb_noret = noret_thumb(rp->up.opcode);
	rp->arm_noret = noret_arm(rp->up.opcode);
}

/**
 * @brief Prepates uretprobe for ARM.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param regs Pointer to CPU register data.
 * @return Error code.
 */
int arch_prepare_uretprobe(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long thumb, bp_offset;

	thumb = ri->preload.use ? ri->preload.thumb : thumb_mode(regs);
	bp_offset = thumb ? 0x1b : sizeof(long) * PROBES_TRAMP_RET_BREAK_IDX;

	/* save original return address */
	ri->ret_addr = (uprobe_opcode_t *)regs->ARM_lr;

	/* replace return address with break point adddress */
	regs->ARM_lr = (unsigned long)(ri->rp->up.insn) + bp_offset;

	/* save stack pointer address */
	ri->sp = (uprobe_opcode_t *)regs->ARM_sp;

	/* Set flag of current mode */
	ri->sp = (uprobe_opcode_t *)((long)ri->sp | !!thumb_mode(regs));

	return 0;
}

unsigned long arch_tramp_by_ri(struct uretprobe_instance *ri)
{
	/* Understand function mode */
	return ((unsigned long)ri->sp & 1) ?
			((unsigned long)ri->rp->up.insn + 0x1b) :
			(unsigned long)(ri->rp->up.insn +
					PROBES_TRAMP_RET_BREAK_IDX);
}

/**
 * @brief Disarms uretprobe instance.
 *
 * @param ri Pointer to the uretprobe instance
 * @param task Pointer to the task for which the uretprobe instance
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task)
{
	struct pt_regs *uregs = task_pt_regs(ri->task);
	unsigned long ra = uregs->ARM_lr;
	unsigned long *tramp = (unsigned long *)arch_tramp_by_ri(ri);
	unsigned long *sp = (unsigned long *)((long)ri->sp & ~1);
	unsigned long *stack = sp - URETPROBE_STACK_DEPTH + 1;
	unsigned long *found = NULL;
	unsigned long *buf[URETPROBE_STACK_DEPTH];
	unsigned long vaddr = (unsigned long)ri->rp->up.addr;
	int i, retval;

	/* check stack */
	retval = read_proc_vm_atomic(task, (unsigned long)stack,
				     buf, sizeof(buf));
	if (retval != sizeof(buf)) {
		printk(KERN_INFO "---> %s (%d/%d): failed to read "
		       "stack from %08lx\n", task->comm, task->tgid, task->pid,
		       (unsigned long)stack);
		retval = -EFAULT;
		goto check_lr;
	}

	/* search the stack from the bottom */
	for (i = URETPROBE_STACK_DEPTH - 1; i >= 0; i--) {
		if (buf[i] == tramp) {
			found = stack + i;
			break;
		}
	}

	if (!found) {
		retval = -ESRCH;
		goto check_lr;
	}

	printk(KERN_INFO "---> %s (%d/%d): trampoline found at "
	       "%08lx (%08lx /%+d) - %lx, set ret_addr=%p\n",
	       task->comm, task->tgid, task->pid,
	       (unsigned long)found, (unsigned long)sp,
	       found - sp, vaddr, ri->ret_addr);
	retval = write_proc_vm_atomic(task, (unsigned long)found,
				      &ri->ret_addr,
				      sizeof(ri->ret_addr));
	if (retval != sizeof(ri->ret_addr)) {
		printk(KERN_INFO "---> %s (%d/%d): "
		       "failed to write value to %08lx",
		       task->comm, task->tgid, task->pid, (unsigned long)found);
		retval = -EFAULT;
	} else {
		retval = 0;
	}

check_lr: /* check lr anyway */
	if (ra == (unsigned long)tramp) {
		printk(KERN_INFO "---> %s (%d/%d): trampoline found at "
		       "lr = %08lx - %lx, set ret_addr=%p\n",
		       task->comm, task->tgid, task->pid, ra, vaddr, ri->ret_addr);

		swap_set_uret_addr(uregs, (unsigned long)ri->ret_addr);
		retval = 0;
	} else if (retval) {
		printk(KERN_INFO "---> %s (%d/%d): trampoline NOT found at "
		       "sp = %08lx, lr = %08lx - %lx, ret_addr=%p\n",
		       task->comm, task->tgid, task->pid,
		       (unsigned long)sp, ra, vaddr, ri->ret_addr);
	}

	return retval;
}

/**
 * @brief Jump pre-handler.
 *
 * @param p Pointer to the uprobe.
 * @param regs Pointer to CPU register data.
 * @return 0.
 */
int setjmp_upre_handler(struct uprobe *p, struct pt_regs *regs)
{
	struct ujprobe *jp = container_of(p, struct ujprobe, up);
	entry_point_t entry = (entry_point_t)jp->entry;

	if (entry) {
		entry(regs->ARM_r0, regs->ARM_r1, regs->ARM_r2,
		      regs->ARM_r3, regs->ARM_r4, regs->ARM_r5);
	} else {
		arch_ujprobe_return();
	}

	return 0;
}

/**
 * @brief Gets trampoline address.
 *
 * @param p Pointer to the uprobe.
 * @param regs Pointer to CPU register data.
 * @return Trampoline address.
 */
unsigned long arch_get_trampoline_addr(struct uprobe *p, struct pt_regs *regs)
{
	return thumb_mode(regs) ?
			(unsigned long)(p->insn) + 0x1b :
			(unsigned long)(p->insn +
					PROBES_TRAMP_RET_BREAK_IDX);
}

/**
 * @brief Restores return address.
 *
 * @param orig_ret_addr Original return address.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs)
{
	regs->ARM_lr = orig_ret_addr;
	regs->ARM_pc = orig_ret_addr & ~0x1;

	if (regs->ARM_lr & 0x1)
		regs->ARM_cpsr |= PSR_T_BIT;
	else
		regs->ARM_cpsr &= ~PSR_T_BIT;
}

/**
 * @brief Removes uprobe.
 *
 * @param up Pointer to the uprobe.
 * @return Void.
 */
void arch_remove_uprobe(struct uprobe *up)
{
	swap_slot_free(up->sm, up->insn);
}

static int urp_handler(struct pt_regs *regs, pid_t tgid)
{
	struct uprobe *p;
	unsigned long flags;
	unsigned long vaddr = regs->ARM_pc;
	unsigned long offset_bp = thumb_mode(regs) ?
				  0x1a :
				  4 * PROBES_TRAMP_RET_BREAK_IDX;
	unsigned long tramp_addr = vaddr - offset_bp;

	local_irq_save(flags);
	p = get_uprobe_by_insn_slot((void *)tramp_addr, tgid, regs);
	if (unlikely(p == NULL)) {
		local_irq_restore(flags);

		pr_info("no_uprobe: Not one of ours: let kernel handle it %lx\n",
			vaddr);
		return 1;
	}

	get_up(p);
	local_irq_restore(flags);
	trampoline_uprobe_handler(p, regs);
	put_up(p);

	return 0;
}
/**
 * @brief Prepares singlestep for current CPU.
 *
 * @param p Pointer to kprobe.
 * @param regs Pointer to CPU registers data.
 * @return Void.
 */
static void arch_prepare_singlestep(struct uprobe *p, struct pt_regs *regs)
{
	if (p->ainsn.insn.handler) {
		regs->ARM_pc += 4;
		p->ainsn.insn.handler(p->opcode, &p->ainsn.insn, regs);
	} else {
		regs->ARM_pc = (unsigned long)p->insn;
	}
}

/**
 * @brief Breakpoint instruction handler.
 *
 * @param regs Pointer to CPU register data.
 * @param instr Instruction.
 * @return uprobe_handler results.
 */
static int uprobe_trap_handler(struct pt_regs *regs, unsigned int instr)
{
	int ret = 0;
	struct uprobe *p;
	unsigned long flags;
	unsigned long vaddr = regs->ARM_pc | !!thumb_mode(regs);
	pid_t tgid = current->tgid;

	local_irq_save(flags);
	p = get_uprobe((uprobe_opcode_t *)vaddr, tgid);
	if (p) {
		get_up(p);
		local_irq_restore(flags);
		if (!p->pre_handler || !p->pre_handler(p, regs))
			arch_prepare_singlestep(p, regs);
		put_up(p);
	} else {
		local_irq_restore(flags);
		ret = urp_handler(regs, tgid);

		/* check ARM/THUMB CPU mode matches installed probe mode */
		if (ret) {
			vaddr ^= 1;

			local_irq_save(flags);
			p = get_uprobe((uprobe_opcode_t *)vaddr, tgid);
			if (p) {
				get_up(p);
				local_irq_restore(flags);
				pr_err("invalid mode: thumb=%d addr=%p insn=%08x\n",
				       !!thumb_mode(regs), p->addr, p->opcode);
				ret = 0;

				disarm_uprobe(p, current);
				put_up(p);
			} else {
				local_irq_restore(flags);
			}
		}
	}

	return ret;
}

/* userspace probes hook (arm) */
static struct undef_hook undef_hook_for_us_arm = {
	.instr_mask	= 0xffffffff,
	.instr_val	= BREAK_ARM,
	.cpsr_mask	= MODE_MASK,
	.cpsr_val	= USR_MODE,
	.fn		= uprobe_trap_handler
};

/* userspace probes hook (thumb) */
static struct undef_hook undef_hook_for_us_thumb = {
	.instr_mask	= 0xffffffff,
	.instr_val	= BREAK_THUMB,
	.cpsr_mask	= MODE_MASK,
	.cpsr_val	= USR_MODE,
	.fn		= uprobe_trap_handler
};

/**
 * @brief Installs breakpoint hooks.
 *
 * @return 0.
 */
int swap_arch_init_uprobes(void)
{
	swap_register_undef_hook(&undef_hook_for_us_arm);
	swap_register_undef_hook(&undef_hook_for_us_thumb);

	return 0;
}

/**
 * @brief Uninstalls breakpoint hooks.
 *
 * @return Void.
 */
void swap_arch_exit_uprobes(void)
{
	swap_unregister_undef_hook(&undef_hook_for_us_thumb);
	swap_unregister_undef_hook(&undef_hook_for_us_arm);
}
