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



#include <arch/arm/probes/probes.h>
#include <arch/arm/probes/probes_arm.h>
#include <arch/arm/probes/probes_thumb.h>
#include <uprobe/swap_uprobes.h>
#include <kprobe/swap_kprobes_deps.h>
#include <kprobe/swap_slots.h>
#include "../probes/compat_arm64.h"


#define PTR_TO_U32(x)	((u32)(unsigned long)(x))
#define U32_TO_PTR(x)	((void *)(unsigned long)(x))

#define flush_insns(addr, size) \
		flush_icache_range((unsigned long)(addr), \
				   (unsigned long)(addr) + (size))


/**
 * @brief Prepares uprobe for ARM.
 *
 * @param up Pointer to the uprobe.
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_prepare_uprobe_arm(struct uprobe *p)
{
	int ret;
	struct task_struct *task = p->task;
	unsigned long vaddr = (unsigned long)p->addr;
	u32 insn;
	u32 tramp[UPROBES_TRAMP_LEN];
	u32 __user *utramp;
	enum { tramp_len = sizeof(tramp) };

	if (!read_proc_vm_atomic(task, vaddr & ~1, &insn, sizeof(insn))) {
		printk(KERN_ERR "failed to read memory %lx!\n", vaddr);
		return -EINVAL;
	}

	ret = make_tramp(&p->ainsn.insn, vaddr, insn, tramp, tramp_len);
	if (ret) {
		pr_err("failed to make tramp, addr=%p\n", p->addr);
		return ret;
	}

	utramp = swap_slot_alloc(p->sm);
	if (utramp == NULL) {
		printk(KERN_INFO "Error: swap_slot_alloc failed (%08lx)\n",
		       vaddr);
		return -ENOMEM;
	}

	if (!write_proc_vm_atomic(p->task, (unsigned long)utramp, tramp,
				  tramp_len)) {
		pr_err("failed to write memory tramp=%p!\n", utramp);
		swap_slot_free(p->sm, utramp);
		return -EINVAL;
	}

	flush_insns(utramp, tramp_len);
	p->insn = utramp;
	p->opcode = insn;

	return 0;
}

int arch_arm_uprobe_arm(struct uprobe *p)
{
	int ret;
	unsigned long vaddr = (unsigned long)p->addr & ~((unsigned long)1);
	int thumb_mode = (unsigned long)p->addr & 1;
	int len = 4 >> thumb_mode;	/* if thumb_mode then len = 2 */
	unsigned long insn = thumb_mode ? BREAK_THUMB : BREAK_ARM;

	ret = write_proc_vm_atomic(p->task, vaddr, &insn, len);
	if (!ret) {
		pr_err("failed to write memory tgid=%u addr=%08lx len=%d\n",
		       p->task->tgid, vaddr, len);

		return -EACCES;
	} else {
		flush_insns(vaddr, len);
	}

	return 0;
}

void arch_disarm_uprobe_arm(struct uprobe *p, struct task_struct *task)
{
	int ret;

	unsigned long vaddr = (unsigned long)p->addr & ~((unsigned long)1);
	int thumb_mode = (unsigned long)p->addr & 1;
	int len = 4 >> thumb_mode;	/* if thumb_mode then len = 2 */

	ret = write_proc_vm_atomic(task, vaddr, &p->opcode, len);
	if (!ret) {
		pr_err("Failed to write memory tgid=%u addr=%08lx len=%d\n",
		       task->tgid, vaddr, len);
	} else {
		flush_insns(vaddr, len);
	}
}

/**
 * @brief Prepates uretprobe for ARM.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param regs Pointer to CPU register data.
 * @return Error code.
 */
int prepare_uretprobe_arm(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long thumb, bp_offset;

	thumb = ri->preload.use ? ri->preload.thumb : thumb_mode(regs);
	bp_offset = thumb ? 0x1b : 4 * PROBES_TRAMP_RET_BREAK_IDX;

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

/**
 * @brief Restores return address.
 *
 * @param orig_ret_addr Original return address.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
void set_orig_ret_addr_arm(unsigned long orig_ret_addr, struct pt_regs *regs)
{
	regs->ARM_lr = orig_ret_addr;
	regs->ARM_pc = orig_ret_addr & ~0x1;

	if (regs->ARM_lr & 0x1)
		regs->ARM_cpsr |= PSR_T_BIT;
	else
		regs->ARM_cpsr &= ~PSR_T_BIT;
}

void arch_opcode_analysis_uretprobe_arm(struct uretprobe *rp)
{
	/* Remove retprobe if first insn overwrites lr */
	rp->thumb_noret = noret_thumb(rp->up.opcode);
	rp->arm_noret = noret_arm(rp->up.opcode);
}

unsigned long arch_get_trampoline_addr_arm(struct uprobe *p,
					   struct pt_regs *regs)
{
	return thumb_mode(regs) ?
			PTR_TO_U32(p->insn) + 0x1b :
			PTR_TO_U32(p->insn +
				   PROBES_TRAMP_RET_BREAK_IDX);
}

unsigned long arch_tramp_by_ri_arm(struct uretprobe_instance *ri)
{
	/* Understand function mode */
	return (PTR_TO_U32(ri->sp) & 1) ?
			PTR_TO_U32(ri->rp->up.insn) + 0x1b :
			PTR_TO_U32(ri->rp->up.insn +
				   PROBES_TRAMP_RET_BREAK_IDX);
}

int arch_disarm_urp_inst_arm(struct uretprobe_instance *ri,
			     struct task_struct *task)
{
	struct pt_regs *uregs = task_pt_regs(ri->task);
	u32 ra = uregs->ARM_lr;
	u32 vaddr, tramp, found = 0;
	u32 sp = PTR_TO_U32(ri->sp) & ~1;
	u32 ret_addr = PTR_TO_U32(ri->ret_addr);
	u32 stack = sp - 4 * (URETPROBE_STACK_DEPTH + 1);
	u32 buf[URETPROBE_STACK_DEPTH];
	int i, ret;

	vaddr = PTR_TO_U32(ri->rp->up.addr);
	tramp = arch_tramp_by_ri_arm(ri);

	/* check stack */
	ret = read_proc_vm_atomic(task, stack, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		pr_info("---> %s (%d/%d): failed to read stack from %08x\n",
			task->comm, task->tgid, task->pid, stack);
		ret = -EFAULT;
		goto check_lr;
	}

	/* search the stack from the bottom */
	for (i = URETPROBE_STACK_DEPTH - 1; i >= 0; i--) {
		if (buf[i] == tramp) {
			found = stack + 4 * i;
			break;
		}
	}

	if (!found) {
		ret = -ESRCH;
		goto check_lr;
	}

	pr_info("---> %s (%d/%d): trampoline found at "
		"%08x (%08x /%+d) - %x, set ret_addr=%08x\n",
		task->comm, task->tgid, task->pid,
		found, sp,
		found - sp, vaddr, ret_addr);
	ret = write_proc_vm_atomic(task, found, &ret_addr, 4);
	if (ret != 4) {
		pr_info("---> %s (%d/%d): failed to write value to %08x",
			task->comm, task->tgid, task->pid, found);
		ret = -EFAULT;
	} else {
		ret = 0;
	}

check_lr: /* check lr anyway */
	if (ra == tramp) {
		pr_info("---> %s (%d/%d): trampoline found at "
			"lr = %08x - %x, set ret_addr=%08x\n",
			task->comm, task->tgid, task->pid, ra, vaddr,
			ret_addr);

		/* set ret_addr */
		uregs->ARM_lr = ret_addr;
		ret = 0;
	} else if (ret) {
		pr_info("---> %s (%d/%d): trampoline NOT found at "
			"sp=%08x, lr=%08x - %x, ret_addr=%08x\n",
			task->comm, task->tgid, task->pid,
			sp, ra, vaddr, ret_addr);
	}

	return ret;
}
