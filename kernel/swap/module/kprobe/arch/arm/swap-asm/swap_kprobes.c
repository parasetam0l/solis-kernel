/**
 * kprobe/arch/asm-arm/swap_kprobes.c
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: initial implementation for ARM/MIPS
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation; Support x86.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 * @author Alexander Shirshikov <a.shirshikov@samsung.com>: initial implementation for Thumb
 * @author Stanislav Andreev <s.andreev@samsung.com>: added time debug profiling support; BUG() message fix
 * @author Stanislav Andreev <s.andreev@samsung.com>: redesign of kprobe functionality -
 * kprobe_handler() now called via undefined instruction hooks
 * @author Stanislav Andreev <s.andreev@samsung.com>: hash tables search implemented for uprobes
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
 * Copyright (C) Samsung Electronics, 2006-2014
 *
 * @section DESCRIPTION
 *
 * SWAP kprobe implementation for ARM architecture.
 */


#include <linux/kconfig.h>

#ifdef CONFIG_SWAP_KERNEL_IMMUTABLE
# error "Kernel is immutable"
#endif /* CONFIG_SWAP_KERNEL_IMMUTABLE */


#include <linux/module.h>
#include <asm/cacheflush.h>
#include <asm/traps.h>
#include <linux/ptrace.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <ksyms/ksyms.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_kdebug.h>
#include <kprobe/swap_kprobes.h>
#include <kprobe/swap_kprobes_deps.h>
#include <arch/arm/probes/probes_arm.h>
#include "swap_kprobes.h"


static void (*__swap_register_undef_hook)(struct undef_hook *hook);
static void (*__swap_unregister_undef_hook)(struct undef_hook *hook);


/**
 * @brief Creates trampoline for kprobe.
 *
 * @param p Pointer to kp_core.
 * @param sm Pointer to slot manager
 * @return 0 on success, error code on error.
 */
int arch_kp_core_prepare(struct kp_core *p, struct slot_manager *sm)
{
	u32 *tramp;
	int ret;

	tramp = swap_slot_alloc(sm);
	if (tramp == NULL)
		return -ENOMEM;

	p->opcode = *(unsigned long *)p->addr;
	ret = make_trampoline_arm(p->addr, p->opcode, tramp);
	if (ret) {
		swap_slot_free(sm, tramp);
		return ret;
	}

	flush_icache_range((unsigned long)tramp,
			   (unsigned long)tramp + KPROBES_TRAMP_LEN);

	p->ainsn.insn = (unsigned long *)tramp;

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
	regs->ARM_pc = (unsigned long)p->ainsn.insn;
}

static void save_previous_kp_core(struct kp_core_ctlblk *kcb)
{
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
}

static int kprobe_handler(struct pt_regs *regs)
{
	struct kp_core *p, *cur;
	struct kp_core_ctlblk *kcb;
	struct kctx *ctx = current_kctx;

	if (regs->ARM_pc == sched_addr)
		switch_to_bits_set(ctx, SWITCH_TO_KP);

	kcb = kp_core_ctlblk();
	cur = kp_core_running();

	rcu_read_lock();
	p = kp_core_by_addr(regs->ARM_pc);
	if (p)
		kp_core_get(p);
	rcu_read_unlock();

	if (p) {
		if (cur) {
			/* Kprobe is pending, so we're recursing. */
			switch (kcb->kp_core_status) {
			case KPROBE_HIT_ACTIVE:
			case KPROBE_HIT_SSDONE:
				/* A pre- or post-handler probe got us here. */
				save_previous_kp_core(kcb);
				kp_core_running_set(p);
				kcb->kp_core_status = KPROBE_REENTER;
				prepare_singlestep(p, regs);
				restore_previous_kp_core(kcb);
				break;
			default:
				/* impossible cases */
				BUG();
			}
		} else {
			kp_core_running_set(p);
			kcb->kp_core_status = KPROBE_HIT_ACTIVE;

			if (!(regs->ARM_cpsr & PSR_I_BIT))
				local_irq_enable();

			if (!p->handlers.pre(p, regs)) {
				kcb->kp_core_status = KPROBE_HIT_SS;
				prepare_singlestep(p, regs);
				kp_core_running_set(NULL);
			}
		}
		kp_core_put(p);
	} else {
		goto no_kprobe;
	}

	switch_to_bits_reset(ctx, SWITCH_TO_KP);

	return 0;

no_kprobe:
	printk(KERN_INFO "no_kprobe: Not one of ours: let kernel handle it %p\n",
			(unsigned long *)regs->ARM_pc);
	return 1;
}

/**
 * @brief Trap handler.
 *
 * @param regs Pointer to CPU register data.
 * @param instr Instruction.
 * @return kprobe_handler result.
 */
int kprobe_trap_handler(struct pt_regs *regs, unsigned int instr)
{
	int ret;

	if (likely(instr == BREAK_ARM)) {
		ret = kprobe_handler(regs);
	} else {
		struct kp_core *p;

		rcu_read_lock();
		p = kp_core_by_addr(regs->ARM_pc);

		/* skip false exeption */
		ret = p && (p->opcode == instr) ? 0 : 1;
		rcu_read_unlock();
	}

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
	entry_point_t entry = (entry_point_t)jp->entry;

	if (entry) {
		entry(regs->ARM_r0, regs->ARM_r1, regs->ARM_r2,
		      regs->ARM_r3, regs->ARM_r4, regs->ARM_r5);
	} else {
		swap_jprobe_return();
	}

	return 0;
}

/**
 * @brief Jprobe return stub.
 *
 * @return Void.
 */
void swap_jprobe_return(void)
{
}
EXPORT_SYMBOL_GPL(swap_jprobe_return);

/**
 * @brief Break handler stub.
 *
 * @param p Pointer to fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return 0.
 */
int swap_longjmp_break_handler (struct kprobe *p, struct pt_regs *regs)
{
	return 0;
}
EXPORT_SYMBOL_GPL(swap_longjmp_break_handler);

#ifdef CONFIG_STRICT_MEMORY_RWX
#include "memory_rwx.h"

static void write_u32(unsigned long addr, unsigned long val)
{
	mem_rwx_write_u32(addr, val);
}
#else /* CONFIG_STRICT_MEMORY_RWX */
static void write_u32(unsigned long addr, unsigned long val)
{
	*(long *)addr = val;
	flush_icache_range(addr, addr + sizeof(long));
}
#endif /* CONFIG_STRICT_MEMORY_RWX */

/**
 * @brief Arms kprobe.
 *
 * @param p Pointer to target kprobe.
 * @return Void.
 */
void arch_kp_core_arm(struct kp_core *core)
{
	write_u32(core->addr, BREAK_ARM);
}

/**
 * @brief Disarms kprobe.
 *
 * @param p Pointer to target kprobe.
 * @return Void.
 */
void arch_kp_core_disarm(struct kp_core *core)
{
	write_u32(core->addr, core->opcode);
}

/**
 * @brief Kretprobe trampoline. Provides jumping to probe handler.
 *
 * @return Void.
 */
void __naked swap_kretprobe_trampoline(void)
{
	__asm__ __volatile__ (
		"stmdb	sp!, {r0 - r11}\n"
		"mov	r0, sp\n"		/* struct pt_regs -> r0 */
		"bl	swap_trampoline_handler\n"
		"mov	lr, r0\n"
		"ldmia	sp!, {r0 - r11}\n"
		"bx	lr\n"
		: : : "memory");
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
	unsigned long *ptr_ret_addr;

	/* for __switch_to probe */
	if ((unsigned long)ri->rp->kp.addr == sched_addr) {
		struct thread_info *tinfo = (struct thread_info *)regs->ARM_r2;

		ptr_ret_addr = (unsigned long *)&tinfo->cpu_context.pc;
		ri->sp = NULL;
		ri->task = tinfo->task;
		switch_to_bits_set(kctx_by_task(tinfo->task), SWITCH_TO_RP);
	} else {
		ptr_ret_addr = (unsigned long *)&regs->ARM_lr;
		ri->sp = (unsigned long *)regs->ARM_sp;
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
	unsigned long r0;

	jumper_cb_t cb;
	char data[0];
};

static unsigned long __used get_r0(struct cb_data *data)
{
	return data->r0;
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

/* FIXME: restore condition flags */

/**
 * @brief Jumper trampoline.
 *
 * @return Void.
 */
void jump_trampoline(void);
__asm(
	"jump_trampoline:\n"

	"push	{r0 - r12}\n"
	"mov	r1, r0\n"	/* data --> r1 */
	"bl	get_r0\n"
	"str	r0, [sp]\n"	/* restore r0 */
	"mov	r0, r1\n"	/* data --> r0 */
	"bl	jump_handler\n"
	"mov	lr, r0\n"
	"pop	{r0 - r12}\n"
	"bx	lr\n"
);

/**
 * @brief Get jumper address.
 *
 * @return Jumper address.
 */
unsigned long get_jump_addr(void)
{
	return (unsigned long)&jump_trampoline;
}
EXPORT_SYMBOL_GPL(get_jump_addr);

/**
 * @brief Set jumper probe callback.
 *
 * @param ret_addr Jumper probe return address.
 * @param regs Pointer to CPU registers data.
 * @param cb Jumper callback of jumper_cb_t type.
 * @param data Data that should be stored in cb_data.
 * @param size Size of the data.
 * @return 0.
 */
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
	cb_data->r0 = regs->ARM_r0;

	/* save cb_data to r0 */
	regs->ARM_r0 = (long)cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(set_jump_cb);




/**
 * @brief Registers hook on specified instruction.
 *
 * @param hook Pointer to struct undef_hook.
 * @return Void.
 */
void swap_register_undef_hook(struct undef_hook *hook)
{
	__swap_register_undef_hook(hook);
}
EXPORT_SYMBOL_GPL(swap_register_undef_hook);

/**
 * @brief Unregisters hook.
 *
 * @param hook Pointer to struct undef_hook.
 * @return Void.
 */
void swap_unregister_undef_hook(struct undef_hook *hook)
{
	__swap_unregister_undef_hook(hook);
}
EXPORT_SYMBOL_GPL(swap_unregister_undef_hook);

/* kernel probes hook */
static struct undef_hook undef_ho_k = {
	.instr_mask	= 0,
	.instr_val	= 0,
	.cpsr_mask	= MODE_MASK,
	.cpsr_val	= SVC_MODE,
	.fn		= kprobe_trap_handler
};

/**
 * @brief Arch-dependend module deps initialization stub.
 *
 * @return 0.
 */
int arch_init_module_deps(void)
{
	const char *sym;
#ifdef CONFIG_STRICT_MEMORY_RWX
	int ret;

	ret = mem_rwx_once();
	if (ret)
		return ret;
#endif /* CONFIG_STRICT_MEMORY_RWX */

	sym = "register_undef_hook";
	__swap_register_undef_hook = (void *)swap_ksyms(sym);
	if (__swap_register_undef_hook == NULL)
		goto not_found;

	sym = "unregister_undef_hook";
	__swap_unregister_undef_hook = (void *)swap_ksyms(sym);
	if (__swap_unregister_undef_hook == NULL)
		goto not_found;

	return 0;

not_found:
	printk(KERN_INFO "ERROR: symbol '%s' not found\n", sym);
	return -ESRCH;
}

/**
 * @brief Initializes kprobes module for ARM arch.
 *
 * @return 0 on success, error code on error.
 */
int swap_arch_init_kprobes(void)
{
	swap_register_undef_hook(&undef_ho_k);

	return 0;
}

/**
 * @brief Uninitializes kprobe module.
 *
 * @return Void.
 */
void swap_arch_exit_kprobes(void)
{
	swap_unregister_undef_hook(&undef_ho_k);
}


/* export symbol for probes_arm.h */
EXPORT_SYMBOL_GPL(noret_arm);
EXPORT_SYMBOL_GPL(make_trampoline_arm);

#include <arch/arm/probes/tramps_arm.h>
/* export symbol for tramps_arm.h */
EXPORT_SYMBOL_GPL(gen_insn_execbuf);
EXPORT_SYMBOL_GPL(pc_dep_insn_execbuf);
EXPORT_SYMBOL_GPL(b_r_insn_execbuf);
EXPORT_SYMBOL_GPL(b_cond_insn_execbuf);
EXPORT_SYMBOL_GPL(blx_off_insn_execbuf);
