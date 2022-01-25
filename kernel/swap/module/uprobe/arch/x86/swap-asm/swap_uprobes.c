/**
 * uprobe/arch/asm-x86/swap_uprobes.c
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
 * Arch-dependent uprobe interface implementation for x86.
 */


#include <linux/kdebug.h>

#include <kprobe/swap_slots.h>
#include <kprobe/swap_td_raw.h>
#include <kprobe/swap_kprobes.h>
#include <uprobe/swap_uprobes.h>

#include "swap_uprobes.h"


struct save_context {
	struct pt_regs save_regs;
	struct pt_regs *ptr_regs;
	unsigned long val;
	int (*handler)(struct uprobe *, struct pt_regs *);
};

/**
 * @struct uprobe_ctlblk
 * @brief Uprobe control block
 */
struct uprobe_ctlblk {
	unsigned long flags;            /**< Flags */
	struct uprobe *p;               /**< Pointer to the uprobe */

	struct save_context ctx;
};


static struct td_raw td_raw;


static unsigned long trampoline_addr(struct uprobe *up)
{
	return (unsigned long)(up->insn + UPROBES_TRAMP_RET_BREAK_IDX);
}

unsigned long arch_tramp_by_ri(struct uretprobe_instance *ri)
{
	return trampoline_addr(&ri->rp->up);
}

static struct uprobe_ctlblk *current_ucb(void)
{
	return (struct uprobe_ctlblk *)swap_td_raw(&td_raw, current);
}

static struct save_context *current_ctx(void)
{
	return &current_ucb()->ctx;
}

static struct uprobe *get_current_probe(void)
{
	return current_ucb()->p;
}

static void set_current_probe(struct uprobe *p)
{
	current_ucb()->p = p;
}

static void save_current_flags(struct pt_regs *regs)
{
	current_ucb()->flags = regs->flags;
}

static void restore_current_flags(struct pt_regs *regs, unsigned long flags)
{
	regs->flags &= ~IF_MASK;
	regs->flags |= flags & IF_MASK;
}

/**
 * @brief Prepares uprobe for x86.
 *
 * @param up Pointer to the uprobe.
 * @return 0 on success,\n
 * -1 on error.
 */
int arch_prepare_uprobe(struct uprobe *p)
{
	struct task_struct *task = p->task;
	u8 tramp[UPROBES_TRAMP_LEN + BP_INSN_SIZE];	/* BP for uretprobe */
	enum { call_relative_opcode = 0xe8 };

	if (!read_proc_vm_atomic(task, (unsigned long)p->addr,
				 tramp, MAX_INSN_SIZE)) {
		printk(KERN_ERR "failed to read memory %p!\n", p->addr);
		return -EINVAL;
	}
	/* TODO: this is a workaround */
	if (tramp[0] == call_relative_opcode) {
		printk(KERN_INFO "cannot install probe: 1st instruction is call\n");
		return -EINVAL;
	}

	tramp[UPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;

	p->opcode = tramp[0];
	p->ainsn.boostable = swap_can_boost(tramp) ? 0 : -1;

	p->insn = swap_slot_alloc(p->sm);
	if (p->insn == NULL) {
		printk(KERN_ERR "trampoline out of memory\n");
		return -ENOMEM;
	}

	if (!write_proc_vm_atomic(task, (unsigned long)p->insn,
				  tramp, sizeof(tramp))) {
		swap_slot_free(p->sm, p->insn);
		printk(KERN_INFO "failed to write memory %p!\n", tramp);
		return -EINVAL;
	}

	/* for uretprobe */
	add_uprobe_table(p);

	return 0;
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
	unsigned long args[6];

	/* FIXME some user space apps crash if we clean interrupt bit */
	/* regs->EREG(flags) &= ~IF_MASK; */
	trace_hardirqs_off();

	/* read first 6 args from stack */
	if (!read_proc_vm_atomic(current, regs->EREG(sp) + 4,
				 args, sizeof(args)))
		printk(KERN_WARNING
		       "failed to read user space func arguments %lx!\n",
		       regs->sp + 4);

	if (entry)
		entry(args[0], args[1], args[2], args[3], args[4], args[5]);
	else
		arch_ujprobe_return();

	return 0;
}

/**
 * @brief Prepares uretprobe for x86.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
int arch_prepare_uretprobe(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	/* Replace the return addr with trampoline addr */
	unsigned long ra = trampoline_addr(&ri->rp->up);
	unsigned long ret_addr;
	ri->sp = (kprobe_opcode_t *)regs->sp;

	if (get_user(ret_addr, (unsigned long *)regs->sp)) {
		pr_err("failed to read user space func ra %lx addr=%p!\n",
		       regs->sp, ri->rp->up.addr);
		return -EINVAL;
	}

	if (put_user(ra, (unsigned long *)regs->sp)) {
		pr_err("failed to write user space func ra %lx!\n", regs->sp);
		return -EINVAL;
	}

	ri->ret_addr = (uprobe_opcode_t *)ret_addr;

	return 0;
}

static bool get_long(struct task_struct *task,
		     unsigned long vaddr, unsigned long *val)
{
	return sizeof(*val) != read_proc_vm_atomic(task, vaddr,
						   val, sizeof(*val));
}

static bool put_long(struct task_struct *task,
		     unsigned long vaddr, unsigned long *val)
{
	return sizeof(*val) != write_proc_vm_atomic(task, vaddr,
						    val, sizeof(*val));
}

/**
 * @brief Disarms uretprobe on x86 arch.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param task Pointer to the task for which the probe.
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task)
{
	unsigned long ret_addr;
	unsigned long sp = (unsigned long)ri->sp;
	unsigned long tramp_addr = trampoline_addr(&ri->rp->up);

	if (get_long(task, sp, &ret_addr)) {
		printk(KERN_INFO "---> %s (%d/%d): failed to read stack from %08lx\n",
		       task->comm, task->tgid, task->pid, sp);
		return -EFAULT;
	}

	if (tramp_addr == ret_addr) {
		if (put_long(task, sp, (unsigned long *)&ri->ret_addr)) {
			printk(KERN_INFO "---> %s (%d/%d): failed to write "
			       "orig_ret_addr to %08lx",
			       task->comm, task->tgid, task->pid, sp);
			return -EFAULT;
		}
	} else {
		printk(KERN_INFO "---> %s (%d/%d): trampoline NOT found at sp = %08lx\n",
		       task->comm, task->tgid, task->pid, sp);
		return -ENOENT;
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
	return trampoline_addr(p);
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
	regs->EREG(ip) = orig_ret_addr;
}

/**
 * @brief Removes uprobe.
 *
 * @param up Pointer to the target uprobe.
 * @return Void.
 */
void arch_remove_uprobe(struct uprobe *p)
{
	swap_slot_free(p->sm, p->insn);
}

int arch_arm_uprobe(struct uprobe *p)
{
	int ret;
	uprobe_opcode_t insn = BREAKPOINT_INSTRUCTION;
	unsigned long vaddr = (unsigned long)p->addr;

	ret = write_proc_vm_atomic(p->task, vaddr, &insn, sizeof(insn));
	if (!ret) {
		pr_err("arch_arm_uprobe: failed to write memory tgid=%u vaddr=%08lx\n",
		       p->task->tgid, vaddr);

		return -EACCES;
	}

	return 0;
}

void arch_disarm_uprobe(struct uprobe *p, struct task_struct *task)
{
	int ret;
	unsigned long vaddr = (unsigned long)p->addr;

	ret = write_proc_vm_atomic(task, vaddr, &p->opcode, sizeof(p->opcode));
	if (!ret) {
		pr_err("arch_disarm_uprobe: failed to write memory tgid=%u, vaddr=%08lx\n",
		       task->tgid, vaddr);
	}
}

static void set_user_jmp_op(void *from, void *to)
{
	struct __arch_jmp_op {
		char op;
		long raddr;
	} __packed jop;

	jop.raddr = (long)(to) - ((long)(from) + 5);
	jop.op = RELATIVEJUMP_INSTRUCTION;

	if (put_user(jop.op, (char *)from) ||
	    put_user(jop.raddr, (long *)(from + 1)))
		pr_err("failed to write jump opcode to user space %p\n", from);
}

static void resume_execution(struct uprobe *p,
			     struct pt_regs *regs,
			     unsigned long flags)
{
	unsigned long *tos, tos_dword = 0;
	unsigned long copy_eip = (unsigned long)p->insn;
	unsigned long orig_eip = (unsigned long)p->addr;
	uprobe_opcode_t insns[2];

	regs->EREG(flags) &= ~TF_MASK;

	tos = (unsigned long *)&tos_dword;
	if (get_user(tos_dword, (unsigned long *)regs->sp)) {
		pr_err("failed to read from user space sp=%lx!\n", regs->sp);
		return;
	}

	if (get_user(*(unsigned short *)insns, (unsigned short *)p->insn)) {
		pr_err("failed to read first 2 opcodes %p!\n", p->insn);
		return;
	}

	switch (insns[0]) {
	case 0x9c: /* pushfl */
		*tos &= ~(TF_MASK | IF_MASK);
		*tos |= flags & (TF_MASK | IF_MASK);
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

		if (put_user(tos_dword, (unsigned long *)regs->sp)) {
			pr_err("failed to write dword to sp=%lx\n", regs->sp);
			return;
		}

		goto no_change;
	case 0xff:
		if ((insns[1] & 0x30) == 0x10) {
			/*
			 * call absolute, indirect
			 * Fix return addr; eip is correct.
			 * But this is not boostable
			 */
			*tos = orig_eip + (*tos - copy_eip);

			if (put_user(tos_dword, (unsigned long *)regs->sp)) {
				pr_err("failed to write dword to sp=%lx\n", regs->sp);
				return;
			}

			goto no_change;
		} else if (((insns[1] & 0x31) == 0x20) || /* jmp near, absolute
							   * indirect */
			   ((insns[1] & 0x31) == 0x21)) {
			/* jmp far, absolute indirect */
			/* eip is correct. And this is boostable */
			p->ainsn.boostable = 1;
			goto no_change;
		}
	case 0xf3:
		if (insns[1] == 0xc3)
			/* repz ret special handling: no more changes */
			goto no_change;
		break;
	default:
		break;
	}

	if (put_user(tos_dword, (unsigned long *)regs->sp)) {
		pr_err("failed to write dword to sp=%lx\n", regs->sp);
		return;
	}

	if (p->ainsn.boostable == 0) {
		if ((regs->EREG(ip) > copy_eip) && (regs->EREG(ip) - copy_eip) +
		    5 < MAX_INSN_SIZE) {
			/*
			 * These instructions can be executed directly if it
			 * jumps back to correct address.
			 */
			set_user_jmp_op((void *) regs->EREG(ip),
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

static void prepare_tramp(struct uprobe *p, struct pt_regs *regs)
{
	regs->ip = (unsigned long)p->insn;
}

static void prepare_ss(struct pt_regs *regs)
{
	/* set single step mode */
	regs->flags |= TF_MASK;
	regs->flags &= ~IF_MASK;
}


static unsigned long resume_userspace_addr;

static void __rcu_nmi_enter(void) {}

static void __used __up_handler(void)
{
	struct pt_regs *regs = current_ctx()->ptr_regs;
	struct thread_info *tinfo = current_thread_info();
	struct uprobe *p = current_ucb()->p;

	/* restore KS regs */
	*regs = current_ctx()->save_regs;

	/* call handler */
	current_ctx()->handler(p, regs);

	/* resume_userspace */
	asm volatile (
		"movl %0, %%esp\n"
		"movl %1, %%ebp\n"
		"jmpl *%2\n"
	        : /* No outputs. */
	        : "r" (regs), "r" (tinfo) , "r" (resume_userspace_addr)
	);
}

void up_handler(void);
__asm(
	"up_handler:\n"
	/* skip hex tractor-driver bytes to make some free space (skip regs) */
	"sub $0x300, %esp\n"
	"jmp __up_handler\n"
);

static int exceptions_handler(struct pt_regs *regs,
			      int (*handler)(struct uprobe *, struct pt_regs *))
{
	/* save regs */
	current_ctx()->save_regs = *regs;
	current_ctx()->ptr_regs = regs;

	/* set handler */
	current_ctx()->handler = handler;

	/* setup regs to return to KS */
	regs->ip = (unsigned long)up_handler;
	regs->ds = __USER_DS;
	regs->es = __USER_DS;
	regs->fs = __KERNEL_PERCPU;
	regs->cs = __KERNEL_CS | get_kernel_rpl();
	regs->gs = 0;
	regs->flags = X86_EFLAGS_IF | X86_EFLAGS_FIXED;

	/*
	 * Here rcu_nmi_enter() call is needed, because we change
	 * US context to KS context as a result rcu_nmi_exit() will
	 * be called on exiting exception and rcu_nmi_enter() and
	 * rcu_nmi_exit() calls must be consistent
	 */
	__rcu_nmi_enter();

	return 1;
}

static int uprobe_handler_retprobe(struct uprobe *p, struct pt_regs *regs)
{
	int ret;

	ret = trampoline_uprobe_handler(p, regs);
	set_current_probe(NULL);
	put_up(p);

	return ret;
}

static int uprobe_handler_part2(struct uprobe *p, struct pt_regs *regs)
{
	if (p->pre_handler && !p->pre_handler(p, regs)) {
		prepare_tramp(p, regs);
		if (p->ainsn.boostable == 1 && !p->post_handler)
			goto exit_and_put_up;

		save_current_flags(regs);
		set_current_probe(p);
		prepare_ss(regs);

		return 1;
	}

exit_and_put_up:
	set_current_probe(NULL);
	put_up(p);
	return 1;
}

static int uprobe_handler_atomic(struct pt_regs *regs)
{
	pid_t tgid = current->tgid;
	unsigned long vaddr = regs->ip - 1;
	struct uprobe *p = get_uprobe((void *)vaddr, tgid);

	if (p) {
		get_up(p);
		if (p->pre_handler) {
			set_current_probe(p);
			exceptions_handler(regs, uprobe_handler_part2);
		} else {
			uprobe_handler_part2(p, regs);
		}
	} else {
		unsigned long tramp_vaddr;

		tramp_vaddr = vaddr - UPROBES_TRAMP_RET_BREAK_IDX;
		p = get_uprobe_by_insn_slot((void *)tramp_vaddr, tgid, regs);
		if (p == NULL) {
			pr_info("no_uprobe\n");
			return 0;
		}

		set_current_probe(p);
		get_up(p);
		exceptions_handler(regs, uprobe_handler_retprobe);
	}

	return 1;
}

static int post_uprobe_handler(struct uprobe *p, struct pt_regs *regs)
{
	unsigned long flags = current_ucb()->flags;

	resume_execution(p, regs, flags);
	restore_current_flags(regs, flags);

	/* reset current probe */
	set_current_probe(NULL);
	put_up(p);

	return 1;
}

static int post_uprobe_handler_atomic(struct pt_regs *regs)
{
	struct uprobe *p = get_current_probe();

	if (p) {
		exceptions_handler(regs, post_uprobe_handler);
	} else {
		pr_info("task[%u %u %s] current uprobe is not found\n",
			current->tgid, current->pid, current->comm);
	}

	return !!p;
}

static int uprobe_exceptions_notify(struct notifier_block *self,
				    unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *)data;
	int ret = NOTIFY_DONE;

	if (args->regs == NULL || !swap_user_mode(args->regs))
		return ret;

	switch (val) {
#ifdef CONFIG_KPROBES
	case DIE_INT3:
#else
	case DIE_TRAP:
#endif
		if (uprobe_handler_atomic(args->regs))
			ret = NOTIFY_STOP;
		break;
	case DIE_DEBUG:
		if (post_uprobe_handler_atomic(args->regs))
			ret = NOTIFY_STOP;
		break;
	default:
		break;
	}

	return ret;
}

static struct notifier_block uprobe_exceptions_nb = {
	.notifier_call = uprobe_exceptions_notify,
	.priority = INT_MAX
};

struct up_valid_struct {
	struct uprobe *p;
	bool found;
};

static int __uprobe_is_valid(struct uprobe *p, void *data)
{
	struct up_valid_struct *valid = (struct up_valid_struct *)data;

	if (valid->p == p) {
		valid->found = true;
		return 1;
	}

	return 0;
}

static bool uprobe_is_valid(struct uprobe *p)
{
	struct up_valid_struct valid = {
		.p = p,
		.found = false,
	};

	for_each_uprobe(__uprobe_is_valid, (void *)&valid);

	return valid.found;
}

static int do_exit_handler(struct kprobe *kp, struct pt_regs *regs)
{
	struct uprobe *p;

	p = get_current_probe();
	if (p && uprobe_is_valid(p)) {
		set_current_probe(NULL);
		put_up(p);
	}

	return 0;
}

static struct kprobe kp_do_exit = {
	.pre_handler = do_exit_handler
};

/**
 * @brief Registers notify.
 *
 * @return register_die_notifier result.
 */
int swap_arch_init_uprobes(void)
{
	int ret;
	const char *sym;

	sym = "resume_userspace";
	resume_userspace_addr = swap_ksyms(sym);
	if (resume_userspace_addr == 0)
		goto not_found;

	sym = "do_exit";
	kp_do_exit.addr = swap_ksyms(sym);
	if (kp_do_exit.addr == 0)
		goto not_found;


	ret = swap_td_raw_reg(&td_raw, sizeof(struct uprobe_ctlblk));
	if (ret)
		return ret;

	ret = register_die_notifier(&uprobe_exceptions_nb);
	if (ret)
		goto unreg_td;

	ret = swap_register_kprobe(&kp_do_exit);
	if (ret)
		goto unreg_exeption;

	return 0;

unreg_exeption:
	unregister_die_notifier(&uprobe_exceptions_nb);
unreg_td:
	swap_td_raw_unreg(&td_raw);
	return ret;

not_found:
	pr_err("symbol '%s' not found\n", sym);
	return -ESRCH;
}

/**
 * @brief Unregisters notify.
 *
 * @return Void.
 */
void swap_arch_exit_uprobes(void)
{
	swap_unregister_kprobe(&kp_do_exit);
	unregister_die_notifier(&uprobe_exceptions_nb);
	swap_td_raw_unreg(&td_raw);
}

