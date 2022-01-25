/*
 * Copied from: arch/arm64/kernel/kprobes.c
 *
 * Kprobes support for ARM64
 *
 * Copyright (C) 2013 Linaro Limited.
 * Author: Sandeepa Prabhu <sandeepa.prabhu@linaro.org>
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


#include <linux/kconfig.h>

#ifdef CONFIG_SWAP_KERNEL_IMMUTABLE
# error "Kernel is immutable"
#endif /* CONFIG_SWAP_KERNEL_IMMUTABLE */


#include <linux/slab.h>
#include <linux/types.h>
#include <asm/cacheflush.h>
#include <asm/debug-monitors.h>
#include <ksyms/ksyms.h>
#include <kprobe/swap_ktd.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_kprobes.h>
#include <kprobe/swap_kprobes_deps.h>
#include "swap_kprobes.h"
#include "kprobes-arm64.h"
#include "dbg_interface.h"


#define BRK_BP			0x63
#define BRK_PSEUDO_SS		0x64
#define BRK64_OPCODE_BP		MAKE_BRK(BRK_BP)
#define BRK64_OPCODE_PSEUDO_SS	MAKE_BRK(BRK_PSEUDO_SS)


#ifdef CONFIG_CPU_BIG_ENDIAN
static u32 conv_inst(u32 inst)
{
	return swab32(inst);
}
#else /* CONFIG_CPU_BIG_ENDIAN */
static u32 conv_inst(u32 inst)
{
	return inst;
}
#endif /* CONFIG_CPU_BIG_ENDIAN */


static void flush_icache(unsigned long addr, size_t size)
{
	flush_icache_range(addr, addr + size);
}

static void write_u32(u32 *addr, u32 val)
{
	*addr = val;
	flush_icache((unsigned long)addr, sizeof(val));
}

void arch_kp_core_arm(struct kp_core *p)
{
	write_u32((u32 *)p->addr, conv_inst(BRK64_OPCODE_BP));
}

void arch_kp_core_disarm(struct kp_core *p)
{
	write_u32((u32 *)p->addr, p->opcode);
}


struct restore_data {
	unsigned long restore_addr;
};

static void ktd_restore_init(struct task_struct *task, void *data)
{
	struct restore_data *rdata = (struct restore_data *)data;

	rdata->restore_addr = 0;
}

static void ktd_restore_exit(struct task_struct *task, void *data)
{
	struct restore_data *rdata = (struct restore_data *)data;

	WARN(rdata->restore_addr, "restore_addr=%lx", rdata->restore_addr);
}

struct ktask_data ktd_restore = {
	.init = ktd_restore_init,
	.exit = ktd_restore_exit,
	.size = sizeof(struct restore_data),
};

static DEFINE_PER_CPU(struct restore_data, per_cpu_restore_i);
static DEFINE_PER_CPU(struct restore_data, per_cpu_restore_st);

static struct restore_data *current_restore_td(void)
{
	if (swap_in_interrupt())
		return &__get_cpu_var(per_cpu_restore_i);
	else if (switch_to_bits_get(current_kctx, SWITCH_TO_ALL))
		return &__get_cpu_var(per_cpu_restore_st);

	return (struct restore_data *)swap_ktd(&ktd_restore, current);
}


static void arch_prepare_ss_slot(struct kp_core *p)
{
	/* prepare insn slot */
	p->ainsn.insn[0] = p->opcode;
	p->ainsn.insn[1] = conv_inst(BRK64_OPCODE_PSEUDO_SS);

	flush_icache((unsigned long)p->ainsn.insn, KPROBES_TRAMP_LEN);
}

static void arch_prepare_simulate(struct kp_core *p)
{
	if (p->ainsn.prepare)
		p->ainsn.prepare(p, &p->ainsn);
}

int arch_kp_core_prepare(struct kp_core *p, struct slot_manager *sm)
{
	kprobe_opcode_t insn;

	/* copy instruction */
	insn = *(kprobe_opcode_t *)p->addr;
	p->opcode = insn;

	/* decode instruction */
	switch (arm_kp_core_decode_insn(insn, &p->ainsn)) {
	case INSN_REJECTED:	/* insn not supported */
		return -EINVAL;

	case INSN_GOOD_NO_SLOT:	/* insn need simulation */
		p->ainsn.insn = NULL;
		arch_prepare_simulate(p);
		break;

	case INSN_GOOD:		/* instruction uses slot */
		p->ainsn.insn = swap_slot_alloc(sm);
		if (!p->ainsn.insn)
			return -ENOMEM;

		arch_prepare_ss_slot(p);
		break;
	};

	return 0;
}

static void save_previous_kp_core(struct kp_core_ctlblk *kcb,
				  unsigned long restore_addr)
{
	kcb->prev_kp_core.p = kp_core_running();
	kcb->prev_kp_core.status = kcb->kp_core_status;
	kcb->prev_kp_core.restore_addr = restore_addr;
}

void restore_previous_kp_core(struct kp_core_ctlblk *kcb)
{
	kp_core_running_set(kcb->prev_kp_core.p);
	kcb->kp_core_status = kcb->prev_kp_core.status;
	current_restore_td()->restore_addr = kcb->prev_kp_core.restore_addr;
}

static void set_ss_context(struct kp_core_ctlblk *kcb, unsigned long addr)
{
	kcb->ss_ctx.ss_status = KP_CORE_STEP_PENDING;
	kcb->ss_ctx.match_addr = addr + sizeof(kprobe_opcode_t);
}

static void clear_ss_context(struct kp_core_ctlblk *kcb)
{
	kcb->ss_ctx.ss_status = KP_CORE_STEP_NONE;
	kcb->ss_ctx.match_addr = 0;
}

static void nop_singlestep_skip(struct pt_regs *regs)
{
	/* set return addr to next pc to continue */
	regs->pc += sizeof(kprobe_opcode_t);
}

static int post_kp_core_handler(struct kp_core_ctlblk *kcb,
				struct pt_regs *regs);

static void arch_simulate_insn(struct kp_core *p, struct pt_regs *regs)
{
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	if (p->ainsn.handler)
		p->ainsn.handler(p->opcode, (long)p->addr, regs);

	/* single step simulated, now go for post processing */
	post_kp_core_handler(kcb, regs);
}

static bool is_ss_setup(struct restore_data *restore)
{
	return !!restore->restore_addr;
}

static void setup_singlestep(struct kp_core *p, struct pt_regs *regs,
			     struct kp_core_ctlblk *kcb, int reenter)
{
	struct restore_data *restore = current_restore_td();

	if (reenter) {
		save_previous_kp_core(kcb, restore->restore_addr);
		kp_core_running_set(p);
		kcb->kp_core_status = KPROBE_REENTER;
	} else {
		kcb->kp_core_status = KPROBE_HIT_SS;
	}

	if (p->ainsn.insn) {
		unsigned long slot = (unsigned long)p->ainsn.insn;

		/* prepare for single stepping */
		restore->restore_addr = regs->pc + 4;
		regs->pc = slot;

		set_ss_context(kcb, slot);	/* mark pending ss */
	} else  {
		restore->restore_addr = 0;	/* reset */

		/* insn simulation */
		arch_simulate_insn(p, regs);
	}
}

static int reenter_kp_core(struct kp_core *p, struct pt_regs *regs,
			   struct kp_core_ctlblk *kcb)
{
	switch (kcb->kp_core_status) {
	case KPROBE_HIT_SSDONE:
	case KPROBE_HIT_ACTIVE:
		if (!p->ainsn.check_condn || p->ainsn.check_condn(p, regs)) {
			setup_singlestep(p, regs, kcb, 1);
		} else  {
			/* condition failed, it's NOP so skip stepping */
			nop_singlestep_skip(regs);
		}
		break;
	case KPROBE_HIT_SS:
		pr_warn("Unrecoverable kp_core detected at %lx\n", p->addr);
		BUG();
	default:
		WARN_ON(1);
		return 0;
	}

	return 1;
}

static int post_kp_core_handler(struct kp_core_ctlblk *kcb,
				struct pt_regs *regs)
{
	struct kp_core *cur = kp_core_running();
	struct restore_data *restore = current_restore_td();

	if (cur == NULL) {
		WARN_ON(1);
		return 0;
	}

	/* return addr restore if non-branching insn */
	if (is_ss_setup(restore)) {
		regs->pc = restore->restore_addr;
		restore->restore_addr = 0;
		kp_core_put(cur);
	} else {
		WARN_ON(1);
	}

	/* restore back original saved kp_core variables and continue */
	if (kcb->kp_core_status == KPROBE_REENTER) {
		restore_previous_kp_core(kcb);
	} else  { /* call post handler */
		kcb->kp_core_status = KPROBE_HIT_SSDONE;
		kp_core_running_set(NULL);
	}

	return 1;
}

static enum dbg_code kprobe_handler(struct pt_regs *regs, unsigned int esr)
{
	struct kp_core *p, *cur;
	struct kp_core_ctlblk *kcb;
	unsigned long addr = regs->pc;
	struct restore_data *restore = current_restore_td();

	kcb = kp_core_ctlblk();
	cur = kp_core_running();

	rcu_read_lock();
	p = kp_core_by_addr(addr);
	if (p)
		kp_core_get(p);
	rcu_read_unlock();

	if (p) {
		if (cur && reenter_kp_core(p, regs, kcb)) {
			if (!is_ss_setup(restore))
				kp_core_put(p);

			return DBG_HANDLED;
		} else if (!p->ainsn.check_condn ||
			p->ainsn.check_condn(p, regs)) {
			/* Probe hit and conditional execution check ok. */
			kp_core_running_set(p);
			kcb->kp_core_status = KPROBE_HIT_ACTIVE;

			if (!(regs->pstate & PSR_I_BIT))
				local_irq_enable();

			if (!p->handlers.pre(p, regs)) {
				kcb->kp_core_status = KPROBE_HIT_SS;
				setup_singlestep(p, regs, kcb, 0);
			}

			local_irq_disable();
		} else {
			/*
			 * Breakpoint hit but conditional check failed,
			 * so just skip handling since it is NOP.
			 */
			nop_singlestep_skip(regs);
		}

		if (!is_ss_setup(restore))
			kp_core_put(p);
	} else if (*(kprobe_opcode_t *)addr != BRK64_OPCODE_BP) {
		/*
		 * The breakpoint instruction was removed right
		 * after we hit it.  Another cpu has removed
		 * either a probepoint or a debugger breakpoint
		 * at this address.  In either case, no further
		 * handling of this interrupt is appropriate.
		 * Return back to original instruction, and continue.
		 */
	} else {
		pr_info("no_kprobe: pc=%llx\n", regs->pc);
	}

	return DBG_HANDLED;
}

static enum dbg_code kp_core_ss_hit(struct kp_core_ctlblk *kcb,
				    unsigned long addr)
{
	if ((kcb->ss_ctx.ss_status == KP_CORE_STEP_PENDING)
	    && (kcb->ss_ctx.match_addr == addr)) {
		/* clear pending ss */
		clear_ss_context(kcb);
		return DBG_HANDLED;
	}

	/* not ours, kp_cores should ignore it */
	return DBG_ERROR;
}

static enum dbg_code kprobe_ss_handler(struct pt_regs *regs, unsigned int esr)
{
	enum dbg_code ret;
	struct kp_core_ctlblk *kcb = kp_core_ctlblk();

	/* check, and return error if this is not our step */
	ret = kp_core_ss_hit(kcb, regs->pc);
	if (ret == DBG_HANDLED) {
		/* single step complete, call post handlers */
		post_kp_core_handler(kcb, regs);
	}

	return ret;
}

static struct brk_hook dbg_bp = {
	.spsr_mask = PSR_MODE_MASK,
	.spsr_val = PSR_MODE_EL1h,
	.esr_mask = DBG_BRK_ESR_MASK,
	.esr_val = DBG_BRK_ESR(BRK_BP),
	.fn = kprobe_handler,
};

static struct brk_hook dbg_ss = {
	.spsr_mask = PSR_MODE_MASK,
	.spsr_val = PSR_MODE_EL1h,
	.esr_mask = DBG_BRK_ESR_MASK,
	.esr_val = DBG_BRK_ESR(BRK_PSEUDO_SS),
	.fn = kprobe_ss_handler,
};





/* ============================================================================
 * =                               KRETPROBE                                  =
 * ============================================================================
 */
void swap_kretprobe_trampoline(void);
__asm(
	".text\n"
	".global swap_kretprobe_trampoline\n"
	"swap_kretprobe_trampoline:\n"
	"stp	x6, x7, [sp,#-16]!\n"
	"stp	x4, x5, [sp,#-16]!\n"
	"stp	x2, x3, [sp,#-16]!\n"
	"stp	x0, x1, [sp,#-16]!\n"
	"mov	x0, sp\n"			/* struct pt_regs (x0..x7) */
	"bl	swap_trampoline_handler\n"
	"mov	x30, x0\n"			/* set real lr */
	"ldp	x0, x1, [sp],#16\n"
	"ldp	x2, x3, [sp],#16\n"
	"ldp	x4, x5, [sp],#16\n"
	"ldp	x6, x7, [sp],#16\n"
	"ret\n"
);

void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	ri->ret_addr = (unsigned long *)regs->regs[30];	/* lr */
	regs->regs[30] = (unsigned long)&swap_kretprobe_trampoline;
	ri->sp = (unsigned long *)regs->sp;
}





/* ============================================================================
 * =                                 JUMPER                                   =
 * ============================================================================
 */
struct cb_data {
	unsigned long ret_addr;
	unsigned long x0;

	jumper_cb_t cb;
	char data[0];
};

static unsigned long __used get_x0(struct cb_data *data)
{
	return data->x0;
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
	".text\n"
	"jump_trampoline:\n"

	"stp	x6, x7, [sp,#-16]!\n"
	"stp	x4, x5, [sp,#-16]!\n"
	"stp	x2, x3, [sp,#-16]!\n"
	"stp	x0, x1, [sp,#-16]!\n"
	"mov	x1, x0\n"		/* data --> x1 */
	"bl     get_x0\n"
	"str    x0, [sp]\n"		/* restore x0 */
	"mov    x0, x1\n"		/* data --> x0 */
	"bl     jump_handler\n"
	"mov    x30, x0\n"		/* set lr */
	"ldp	x0, x1, [sp],#16\n"
	"ldp	x2, x3, [sp],#16\n"
	"ldp	x4, x5, [sp],#16\n"
	"ldp	x6, x7, [sp],#16\n"
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
	cb_data->x0 = regs->regs[0];

	/* save cb_data to x0 */
	regs->regs[0] = (long)cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(set_jump_cb);





/* ============================================================================
 * =                          ARCH INIT/EXIT KPROBES                          =
 * ============================================================================
 */
int arch_init_module_deps(void)
{
	return 0;
}

int swap_arch_init_kprobes(void)
{
	int ret;

	ret = swap_ktd_reg(&ktd_restore);
	if (ret)
		return ret;

	ret = dbg_iface_init();
	if (ret)
		swap_ktd_unreg(&ktd_restore);

	dbg_brk_hook_reg(&dbg_ss);
	dbg_brk_hook_reg(&dbg_bp);

	return 0;
}

void swap_arch_exit_kprobes(void)
{
	dbg_brk_hook_unreg(&dbg_ss);
	dbg_brk_hook_unreg(&dbg_bp);
	dbg_iface_uninit();
	swap_ktd_unreg(&ktd_restore);
}
