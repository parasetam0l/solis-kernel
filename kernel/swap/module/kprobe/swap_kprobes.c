/**
 * kprobe/swap_kprobes.c
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: initial implementation for ARM and MIPS
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
 * SWAP kprobe implementation. Dynamic kernel functions instrumentation.
 */

#include <linux/version.h>

#include <linux/hash.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/stop_machine.h>
#include <linux/delay.h>
#include <ksyms/ksyms.h>
#include <master/swap_initializer.h>
#include <swap-asm/swap_kprobes.h>
#include "swap_ktd.h"
#include "swap_slots.h"
#include "swap_ktd.h"
#include "swap_td_raw.h"
#include "swap_kdebug.h"
#include "swap_kprobes.h"
#include "swap_kprobes_deps.h"


#define KRETPROBE_STACK_DEPTH 64


/**
 * @var sched_addr
 * @brief Scheduler address.
 */
unsigned long sched_addr;
static unsigned long exit_addr;
static unsigned long do_group_exit_addr;
static unsigned long sys_exit_group_addr;
static unsigned long sys_exit_addr;

/**
 * @var sm
 * @brief Current slot manager. Slots are the places where trampolines are
 * located.
 */
struct slot_manager sm;

static DEFINE_SPINLOCK(kretprobe_lock);	/* Protects kretprobe_inst_table */

static struct hlist_head kprobe_table[KPROBE_TABLE_SIZE];
static struct hlist_head kretprobe_inst_table[KPROBE_TABLE_SIZE];

/**
 * @var kprobe_count
 * @brief Count of kprobes.
 */
atomic_t kprobe_count;
EXPORT_SYMBOL_GPL(kprobe_count);


static void *(*__module_alloc)(unsigned long size);
static void (*__module_free)(struct module *mod, void *module_region);

static void *wrapper_module_alloc(unsigned long size)
{
	return __module_alloc(size);
}

static void wrapper_module_free(void *module_region)
{
	__module_free(NULL, module_region);
}

static void *sm_alloc(struct slot_manager *sm)
{
	return wrapper_module_alloc(PAGE_SIZE);
}

static void sm_free(struct slot_manager *sm, void *ptr)
{
	wrapper_module_free(ptr);
}

static void init_sm(void)
{
	sm.slot_size = KPROBES_TRAMP_LEN;
	sm.alloc = sm_alloc;
	sm.free = sm_free;
	INIT_HLIST_HEAD(&sm.page_list);
}

static void exit_sm(void)
{
	/* FIXME: free */
}

static struct hlist_head *kpt_head_by_addr(unsigned long addr)
{
	return &kprobe_table[hash_ptr((void *)addr, KPROBE_HASH_BITS)];
}

static void kretprobe_assert(struct kretprobe_instance *ri,
			     unsigned long orig_ret_address,
			     unsigned long trampoline_address)
{
	if (!orig_ret_address || (orig_ret_address == trampoline_address)) {
		struct task_struct *task;
		if (ri == NULL)
			panic("kretprobe BUG!: ri = NULL\n");

		task = ri->task;

		if (task == NULL)
			panic("kretprobe BUG!: task = NULL\n");

		if (ri->rp == NULL)
			panic("kretprobe BUG!: ri->rp = NULL\n");

		panic("kretprobe BUG!: "
		      "Processing kretprobe %p @ %08lx (%d/%d - %s)\n",
		      ri->rp, ri->rp->kp.addr, ri->task->tgid,
		      ri->task->pid, ri->task->comm);
	}
}

struct kpc_data {
	struct kp_core *running;
	struct kp_core_ctlblk ctlblk;
};

struct kctx {
	struct kpc_data kpc;
	unsigned long st_flags;
};

static void ktd_cur_init(struct task_struct *task, void *data)
{
	struct kctx *ctx = (struct kctx *)data;

	memset(ctx, 0, sizeof(*ctx));
}

static void ktd_cur_exit(struct task_struct *task, void *data)
{
	struct kctx *ctx = (struct kctx *)data;

	WARN(ctx->kpc.running, "running=%p\n", ctx->kpc.running);
}

struct ktask_data ktd_cur = {
	.init = ktd_cur_init,
	.exit = ktd_cur_exit,
	.size = sizeof(struct kctx),
};

struct kctx *kctx_by_task(struct task_struct *task)
{
	return (struct kctx *)swap_ktd(&ktd_cur, task);
}

void switch_to_bits_set(struct kctx *ctx, unsigned long mask)
{
	ctx->st_flags |= mask;
}

void switch_to_bits_reset(struct kctx *ctx, unsigned long mask)
{
	ctx->st_flags &= ~mask;
}

unsigned long switch_to_bits_get(struct kctx *ctx, unsigned long mask)
{
	return ctx->st_flags & mask;
}

static DEFINE_PER_CPU(struct kpc_data, per_cpu_kpc_data_i);
static DEFINE_PER_CPU(struct kpc_data, per_cpu_kpc_data_st);

static struct kpc_data *kp_core_data(void)
{
	struct kctx *ctx = current_kctx;

	if (swap_in_interrupt())
		return &__get_cpu_var(per_cpu_kpc_data_i);
	else if (switch_to_bits_get(ctx, SWITCH_TO_ALL))
		return &__get_cpu_var(per_cpu_kpc_data_st);

	return &ctx->kpc;
}

static int kprobe_cur_reg(void)
{
	return swap_ktd_reg(&ktd_cur);
}

static void kprobe_cur_unreg(void)
{
	swap_ktd_unreg(&ktd_cur);
}

struct kp_core *kp_core_running(void)
{
	return kp_core_data()->running;
}

void kp_core_running_set(struct kp_core *p)
{
	kp_core_data()->running = p;
}

/**
 * @brief Gets kp_core_ctlblk for the current CPU.
 *
 * @return Current CPU struct kp_core_ctlblk.
 */
struct kp_core_ctlblk *kp_core_ctlblk(void)
{
	return &kp_core_data()->ctlblk;
}

/*
 * This routine is called either:
 *	- under the kprobe_mutex - during kprobe_[un]register()
 *				OR
 *	- with preemption disabled - from arch/xxx/kernel/kprobes.c
 */

/**
 * @brief Gets kp_core.
 *
 * @param addr Probe address.
 * @return kprobe_core for addr.
 */
struct kp_core *kp_core_by_addr(unsigned long addr)
{
	struct hlist_head *head;
	struct kp_core *core;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	head = kpt_head_by_addr(addr);
	swap_hlist_for_each_entry_rcu(core, node, head, hlist) {
		if (core->addr == addr)
			return core;
	}

	return NULL;
}


static int alloc_nodes_kretprobe(struct kretprobe *rp);

/* Called with kretprobe_lock held */
static struct kretprobe_instance *get_free_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
		return ri;
	}

	if (!alloc_nodes_kretprobe(rp)) {
		swap_hlist_for_each_entry(ri, node, &rp->free_instances,
					  uflist) {
			return ri;
		}
	}

	return NULL;
}

/* Called with kretprobe_lock held */
static struct kretprobe_instance *
get_free_rp_inst_no_alloc(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->free_instances, uflist) {
		return ri;
	}

	return NULL;
}

/* Called with kretprobe_lock held */
static struct kretprobe_instance *get_used_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->used_instances, uflist) {
		return ri;
	}

	return NULL;
}

/* Called with kretprobe_lock held */
static void add_rp_inst(struct kretprobe_instance *ri)
{
	/*
	 * Remove rp inst off the free list -
	 * Add it back when probed function returns
	 */
	hlist_del(&ri->uflist);

	/* Add rp inst onto table */
	INIT_HLIST_NODE(&ri->hlist);

	hlist_add_head(&ri->hlist,
		       &kretprobe_inst_table[hash_ptr(ri->task,
						      KPROBE_HASH_BITS)]);

	/* Also add this rp inst to the used list. */
	INIT_HLIST_NODE(&ri->uflist);
	hlist_add_head(&ri->uflist, &ri->rp->used_instances);
}

/* Called with kretprobe_lock held */
static void recycle_rp_inst(struct kretprobe_instance *ri)
{
	if (ri->rp) {
		hlist_del(&ri->hlist);
		/* remove rp inst off the used list */
		hlist_del(&ri->uflist);
		/* put rp inst back onto the free list */
		INIT_HLIST_NODE(&ri->uflist);
		hlist_add_head(&ri->uflist, &ri->rp->free_instances);
	}
}

static struct hlist_head *kretprobe_inst_table_head(void *hash_key)
{
	return &kretprobe_inst_table[hash_ptr(hash_key, KPROBE_HASH_BITS)];
}

static void free_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	while ((ri = get_free_rp_inst_no_alloc(rp)) != NULL) {
		hlist_del(&ri->uflist);
		kfree(ri);
	}
}

static void kp_core_remove(struct kp_core *core)
{
	/* TODO: check boostable for x86 and MIPS */
	swap_slot_free(&sm, core->ainsn.insn);
}

static void kp_core_wait(struct kp_core *p)
{
	int ms = 1;

	while (atomic_read(&p->usage)) {
		msleep(ms);
		ms += ms < 7 ? 1 : 0;
	}
}

static struct kp_core *kp_core_create(unsigned long addr)
{
	struct kp_core *core;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (core) {
		INIT_HLIST_NODE(&core->hlist);
		core->addr = addr;
		atomic_set(&core->usage, 0);
		rwlock_init(&core->handlers.lock);
	}

	return core;
}

static void kp_core_free(struct kp_core *core)
{
	WARN_ON(atomic_read(&core->usage));
	kfree(core);
}

static int pre_handler_one(struct kp_core *core, struct pt_regs *regs)
{
	int ret = 0;
	struct kprobe *p = core->handlers.kps[0];

	if (p && p->pre_handler)
		ret = p->pre_handler(p, regs);

	return ret;
}

static int pre_handler_multi(struct kp_core *core, struct pt_regs *regs)
{
	int i, ret = 0;

	/* TODO: add sync use kprobe */
	for (i = 0; i < ARRAY_SIZE(core->handlers.kps); ++i) {
		struct kprobe *p = core->handlers.kps[i];

		if (p && p->pre_handler) {
			ret = p->pre_handler(p, regs);
			if (ret)
				break;
		}
	}

	return ret;
}

static int kp_core_add_kprobe(struct kp_core *core, struct kprobe *p)
{
	int i, ret = 0;
	unsigned long flags;
	struct kp_handlers *h = &core->handlers;

	write_lock_irqsave(&h->lock, flags);
	if (h->pre == NULL) {
		h->pre = pre_handler_one;
	} else if (h->pre == pre_handler_one) {
		h->pre = pre_handler_multi;
	}

	for (i = 0; i < ARRAY_SIZE(core->handlers.kps); ++i) {
		if (core->handlers.kps[i])
			continue;

		core->handlers.kps[i] = p;
		goto unlock;
	}

	pr_err("all kps slots is busy\n");
	ret = -EBUSY;
unlock:
	write_unlock_irqrestore(&h->lock, flags);
	return ret;
}

static void kp_core_del_kprobe(struct kp_core *core, struct kprobe *p)
{
	int i, cnt = 0;
	unsigned long flags;
	struct kp_handlers *h = &core->handlers;

	write_lock_irqsave(&h->lock, flags);
	for (i = 0; i < ARRAY_SIZE(h->kps); ++i) {
		if (h->kps[i] == p)
			h->kps[i] = NULL;

		if (h->kps[i] == NULL)
			++cnt;
	}
	write_unlock_irqrestore(&h->lock, flags);

	if (cnt == ARRAY_SIZE(h->kps)) {
		arch_kp_core_disarm(core);
		synchronize_sched();

		hlist_del_rcu(&core->hlist);
		synchronize_rcu();

		kp_core_wait(core);
		kp_core_remove(core);
		kp_core_free(core);
	}
}

static DEFINE_MUTEX(kp_mtx);
/**
 * @brief Registers kprobe.
 *
 * @param p Pointer to the target kprobe.
 * @return 0 on success, error code on error.
 */
int swap_register_kprobe(struct kprobe *p)
{
	struct kp_core *core;
	unsigned long addr;
	int ret = 0;
	/*
	 * If we have a symbol_name argument look it up,
	 * and add it to the address.  That way the addr
	 * field can either be global or relative to a symbol.
	 */
	if (p->symbol_name) {
		if (p->addr)
			return -EINVAL;
		p->addr = swap_ksyms(p->symbol_name);
	}

	if (!p->addr)
		return -EINVAL;

	addr = p->addr + p->offset;

	mutex_lock(&kp_mtx);
	core = kp_core_by_addr(addr);
	if (core == NULL) {
		core = kp_core_create(addr);
		if (core == NULL) {
			pr_err("Out of memory\n");
			ret = -ENOMEM;
			goto unlock;
		}

		ret = arch_kp_core_prepare(core, &sm);
		if (ret)
			goto unlock;

		ret = kp_core_add_kprobe(core, p);
		if (ret) {
			kp_core_free(core);
			goto unlock;
		}

		hlist_add_head_rcu(&core->hlist, kpt_head_by_addr(core->addr));
		arch_kp_core_arm(core);
	} else {
		ret = kp_core_add_kprobe(core, p);
	}

unlock:
	mutex_unlock(&kp_mtx);
	return ret;
}
EXPORT_SYMBOL_GPL(swap_register_kprobe);

/**
 * @brief Unregistes kprobe.
 *
 * @param kp Pointer to the target kprobe.
 * @return Void.
 */
void swap_unregister_kprobe(struct kprobe *p)
{
	unsigned long addr = p->addr + p->offset;
	struct kp_core *core;

	mutex_lock(&kp_mtx);
	core = kp_core_by_addr(addr);
	BUG_ON(core == NULL);

	kp_core_del_kprobe(core, p);
	mutex_unlock(&kp_mtx);

	/* Set 0 addr for reusability if symbol_name is used */
	if (p->symbol_name)
		p->addr = 0;
}
EXPORT_SYMBOL_GPL(swap_unregister_kprobe);

/**
 * @brief Registers jprobe.
 *
 * @param jp Pointer to the target jprobe.
 * @return swap_register_kprobe result.
 */
int swap_register_jprobe(struct jprobe *jp)
{
	/* Todo: Verify probepoint is a function entry point */
	jp->kp.pre_handler = swap_setjmp_pre_handler;

	return swap_register_kprobe(&jp->kp);
}
EXPORT_SYMBOL_GPL(swap_register_jprobe);

/**
 * @brief Unregisters jprobe.
 *
 * @param jp Pointer to the target jprobe.
 * @return Void.
 */
void swap_unregister_jprobe(struct jprobe *jp)
{
	swap_unregister_kprobe(&jp->kp);
}
EXPORT_SYMBOL_GPL(swap_unregister_jprobe);

/*
 * This kprobe pre_handler is registered with every kretprobe. When probe
 * hits it will set up the return probe.
 */
static int pre_handler_kretprobe(struct kprobe *p, struct pt_regs *regs)
{
	struct kretprobe *rp = container_of(p, struct kretprobe, kp);
	struct kretprobe_instance *ri;
	unsigned long flags = 0;

	/* TODO: consider to only swap the RA
	 * after the last pre_handler fired */
	spin_lock_irqsave(&kretprobe_lock, flags);

	/* TODO: test - remove retprobe after func entry but before its exit */
	ri = get_free_rp_inst(rp);
	if (ri != NULL) {
		int skip = 0;

		ri->rp = rp;
		ri->task = current;

		if (rp->entry_handler)
			skip = rp->entry_handler(ri, regs);

		if (skip) {
			add_rp_inst(ri);
			recycle_rp_inst(ri);
		} else {
			swap_arch_prepare_kretprobe(ri, regs);
			add_rp_inst(ri);
		}
	} else {
		++rp->nmissed;
	}

	spin_unlock_irqrestore(&kretprobe_lock, flags);

	return 0;
}

/**
 * @brief Trampoline probe handler.
 *
 * @param p Pointer to the fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return orig_ret_address
 */
unsigned long swap_trampoline_handler(struct pt_regs *regs)
{
	struct kretprobe_instance *ri = NULL;
	struct hlist_head *head;
	unsigned long flags, orig_ret_address = 0;
	unsigned long trampoline_address;

	struct kp_core_ctlblk *kcb;

	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	trampoline_address = (unsigned long)&swap_kretprobe_trampoline;

	kcb = kp_core_ctlblk();

	spin_lock_irqsave(&kretprobe_lock, flags);

	/*
	 * We are using different hash keys (current and mm) for finding kernel
	 * space and user space probes.  Kernel space probes can change mm field
	 * in task_struct.  User space probes can be shared between threads of
	 * one process so they have different current but same mm.
	 */
	head = kretprobe_inst_table_head(current);

#ifdef CONFIG_X86
	regs->XREG(cs) = __KERNEL_CS | get_kernel_rpl();
	regs->EREG(ip) = trampoline_address;
	regs->ORIG_EAX_REG = 0xffffffff;
#endif

	/*
	 * It is possible to have multiple instances associated with a given
	 * task either because an multiple functions in the call path
	 * have a return probe installed on them, and/or more then one
	 * return probe was registered for a target function.
	 *
	 * We can handle this because:
	 *     - instances are always inserted at the head of the list
	 *     - when multiple return probes are registered for the same
	 *       function, the first instance's ret_addr will point to the
	 *       real return address, and all the rest will point to
	 *       kretprobe_trampoline
	 */
	swap_hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task != current)
			/* another task is sharing our hash bucket */
			continue;
		if (ri->rp && ri->rp->handler) {
			/*
			 * Set fake current probe, we don't
			 * want to go into recursion
			 */
			kp_core_running_set((struct kp_core *)0xfffff);
			kcb->kp_core_status = KPROBE_HIT_ACTIVE;
			ri->rp->handler(ri, regs);
			kp_core_running_set(NULL);
		}

		orig_ret_address = (unsigned long)ri->ret_addr;
		recycle_rp_inst(ri);
		if (orig_ret_address != trampoline_address)
			/*
			 * This is the real return address. Any other
			 * instances associated with this task are for
			 * other calls deeper on the call stack
			 */
			break;
	}
	kretprobe_assert(ri, orig_ret_address, trampoline_address);

	if (kcb->kp_core_status == KPROBE_REENTER)
		restore_previous_kp_core(kcb);
	else
		kp_core_running_set(NULL);

	switch_to_bits_reset(current_kctx, SWITCH_TO_RP);
	spin_unlock_irqrestore(&kretprobe_lock, flags);

	/*
	 * By returning a non-zero value, we are telling
	 * kprobe_handler() that we don't want the post_handler
	 * to run (and have re-enabled preemption)
	 */

	return orig_ret_address;
}

#define SCHED_RP_NR 200
#define COMMON_RP_NR 10

static int alloc_nodes_kretprobe(struct kretprobe *rp)
{
	int alloc_nodes;
	struct kretprobe_instance *inst;
	int i;

	DBPRINTF("Alloc aditional mem for retprobes");

	if (rp->kp.addr == sched_addr) {
		rp->maxactive += SCHED_RP_NR; /* max (100, 2 * NR_CPUS); */
		alloc_nodes = SCHED_RP_NR;
	} else {
#if 1/* def CONFIG_PREEMPT */
		rp->maxactive += max(COMMON_RP_NR, 2 * NR_CPUS);
#else
		rp->maxacpptive += NR_CPUS;
#endif
		alloc_nodes = COMMON_RP_NR;
	}

	for (i = 0; i < alloc_nodes; i++) {
		inst = kmalloc(sizeof(*inst) + rp->data_size, GFP_ATOMIC);
		if (inst == NULL) {
			free_rp_inst(rp);
			return -ENOMEM;
		}
		INIT_HLIST_NODE(&inst->uflist);
		hlist_add_head(&inst->uflist, &rp->free_instances);
	}

	DBPRINTF("addr=%p, *addr=[%lx %lx %lx]", rp->kp.addr,
		  (unsigned long) (*(rp->kp.addr)),
		  (unsigned long) (*(rp->kp.addr + 1)),
		  (unsigned long) (*(rp->kp.addr + 2)));
	return 0;
}

/**
 * @brief Registers kretprobes.
 *
 * @param rp Pointer to the target kretprobe.
 * @return 0 on success, error code on error.
 */
int swap_register_kretprobe(struct kretprobe *rp)
{
	int ret = 0;
	struct kretprobe_instance *inst;
	int i;
	DBPRINTF("START");

	rp->kp.pre_handler = pre_handler_kretprobe;

	/* Pre-allocate memory for max kretprobe instances */
	if (rp->kp.addr == exit_addr) {
		rp->kp.pre_handler = NULL; /* not needed for do_exit */
		rp->maxactive = 0;
	} else if (rp->kp.addr == do_group_exit_addr) {
		rp->kp.pre_handler = NULL;
		rp->maxactive = 0;
	} else if (rp->kp.addr == sys_exit_group_addr) {
		rp->kp.pre_handler = NULL;
		rp->maxactive = 0;
	} else if (rp->kp.addr == sys_exit_addr) {
		rp->kp.pre_handler = NULL;
		rp->maxactive = 0;
	} else if (rp->maxactive <= 0) {
#if 1/* def CONFIG_PREEMPT */
		rp->maxactive = max(COMMON_RP_NR, 2 * NR_CPUS);
#else
		rp->maxactive = NR_CPUS;
#endif
	}
	INIT_HLIST_HEAD(&rp->used_instances);
	INIT_HLIST_HEAD(&rp->free_instances);
	for (i = 0; i < rp->maxactive; i++) {
		inst = kmalloc(sizeof(*inst) + rp->data_size, GFP_KERNEL);
		if (inst == NULL) {
			free_rp_inst(rp);
			return -ENOMEM;
		}
		INIT_HLIST_NODE(&inst->uflist);
		hlist_add_head(&inst->uflist, &rp->free_instances);
	}

	DBPRINTF("addr=%p, *addr=[%lx %lx %lx]", rp->kp.addr,
		  (unsigned long) (*(rp->kp.addr)),
		  (unsigned long) (*(rp->kp.addr + 1)),
		  (unsigned long) (*(rp->kp.addr + 2)));
	rp->nmissed = 0;
	/* Establish function entry probe point */
	ret = swap_register_kprobe(&rp->kp);
	if (ret != 0)
		free_rp_inst(rp);

	DBPRINTF("addr=%p, *addr=[%lx %lx %lx]", rp->kp.addr,
		  (unsigned long) (*(rp->kp.addr)),
		  (unsigned long) (*(rp->kp.addr + 1)),
		  (unsigned long) (*(rp->kp.addr + 2)));

	return ret;
}
EXPORT_SYMBOL_GPL(swap_register_kretprobe);

static int swap_disarm_krp_inst(struct kretprobe_instance *ri);

static void swap_disarm_krp(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(ri, node, &rp->used_instances, uflist) {
		if (swap_disarm_krp_inst(ri) != 0) {
			printk(KERN_INFO "%s (%d/%d): cannot disarm "
			       "krp instance (%08lx)\n",
			       ri->task->comm, ri->task->tgid, ri->task->pid,
			       rp->kp.addr);
		}
	}
}


struct unreg_krp_args {
	struct kretprobe **rps;
	size_t size;
	int rp_disarm;
};

static int __swap_unregister_kretprobes_top(void *data)
{
	struct unreg_krp_args *args = data;
	struct kretprobe **rps = args->rps;
	size_t size = args->size;
	int rp_disarm = args->rp_disarm;
	unsigned long flags;
	const size_t end = ((size_t) 0) - 1;

	for (--size; size != end; --size) {
		if (rp_disarm) {
			spin_lock_irqsave(&kretprobe_lock, flags);
			swap_disarm_krp(rps[size]);
			spin_unlock_irqrestore(&kretprobe_lock, flags);
		}
	}

	return 0;
}

/**
 * @brief Kretprobes unregister top. Unregisters kprobes.
 *
 * @param rps Pointer to the array of pointers to the target kretprobes.
 * @param size Size of rps array.
 * @param rp_disarm Disarm flag. If set kretprobe is disarmed.
 * @return Void.
 */
void swap_unregister_kretprobes_top(struct kretprobe **rps, size_t size,
				   int rp_disarm)
{
	struct unreg_krp_args args = {
		.rps = rps,
		.size = size,
		.rp_disarm = rp_disarm,
	};
	const size_t end = ((size_t)0) - 1;

	for (--size; size != end; --size)
		swap_unregister_kprobe(&rps[size]->kp);

	if (rp_disarm) {
		int ret;

		ret = stop_machine(__swap_unregister_kretprobes_top,
				   &args, NULL);
		if (ret)
			pr_err("%s failed (%d)\n", __func__, ret);
	} else {
		__swap_unregister_kretprobes_top(&args);
	}
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobes_top);

/**
 * @brief swap_unregister_kretprobes_top wrapper for a single kretprobe.
 *
 * @param rp Pointer to the target kretprobe.
 * @param rp_disarm Disarm flag.
 * @return Void.
 */
void swap_unregister_kretprobe_top(struct kretprobe *rp, int rp_disarm)
{
	swap_unregister_kretprobes_top(&rp, 1, rp_disarm);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobe_top);

/**
 * @brief Kretprobe unregister bottom. Here is kretprobe memory is released.
 *
 * @param rp Pointer to the target kretprobe.
 * @return Void.
 */
void swap_unregister_kretprobe_bottom(struct kretprobe *rp)
{
	unsigned long flags;
	struct kretprobe_instance *ri;

	spin_lock_irqsave(&kretprobe_lock, flags);

	while ((ri = get_used_rp_inst(rp)) != NULL)
		recycle_rp_inst(ri);
	free_rp_inst(rp);

	spin_unlock_irqrestore(&kretprobe_lock, flags);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobe_bottom);

/**
 * @brief swap_unregister_kretprobe_bottom wrapper for several kretprobes.
 *
 * @param rps Pointer to the array of the target kretprobes pointers.
 * @param size Size of rps array.
 * @return Void.
 */
void swap_unregister_kretprobes_bottom(struct kretprobe **rps, size_t size)
{
	const size_t end = ((size_t) 0) - 1;

	for (--size; size != end; --size)
		swap_unregister_kretprobe_bottom(rps[size]);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobes_bottom);

/**
 * @brief Unregisters kretprobes.
 *
 * @param rpp Pointer to the array of the target kretprobes pointers.
 * @param size Size of rpp array.
 * @return Void.
 */
void swap_unregister_kretprobes(struct kretprobe **rpp, size_t size)
{
	swap_unregister_kretprobes_top(rpp, size, 1);

	if (!in_atomic())
		synchronize_sched();

	swap_unregister_kretprobes_bottom(rpp, size);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobes);

/**
 * @brief swap_unregister_kretprobes wrapper for a single kretprobe.
 *
 * @param rp Pointer to the target kretprobe.
 * @return Void.
 */
void swap_unregister_kretprobe(struct kretprobe *rp)
{
	swap_unregister_kretprobes(&rp, 1);
}
EXPORT_SYMBOL_GPL(swap_unregister_kretprobe);

static inline void rm_task_trampoline(struct task_struct *p,
				      struct kretprobe_instance *ri)
{
	arch_set_task_pc(p, (unsigned long)ri->ret_addr);
}

static int swap_disarm_krp_inst(struct kretprobe_instance *ri)
{
	unsigned long *tramp = (unsigned long *)&swap_kretprobe_trampoline;
	unsigned long *sp = ri->sp;
	unsigned long *found = NULL;
	int retval = -ENOENT;

	if (!sp) {
		unsigned long pc = arch_get_task_pc(ri->task);

		printk(KERN_INFO "---> [%d] %s (%d/%d): pc = %08lx, ra = %08lx, tramp= %08lx (%08lx)\n",
		       task_cpu(ri->task),
		       ri->task->comm, ri->task->tgid, ri->task->pid,
		       pc, (long unsigned int)ri->ret_addr,
		       (long unsigned int)tramp,
		       (ri->rp ? ri->rp->kp.addr : 0));

		/* __switch_to retprobe handling */
		if (pc == (unsigned long)tramp) {
			rm_task_trampoline(ri->task, ri);
			return 0;
		}

		return -EINVAL;
	}

	while (sp > ri->sp - KRETPROBE_STACK_DEPTH) {
		if (*sp == (unsigned long)tramp) {
			found = sp;
			break;
		}
		sp--;
	}

	if (found) {
		printk(KERN_INFO "---> [%d] %s (%d/%d): tramp (%08lx) "
		       "found at %08lx (%08lx /%+ld) - %08lx\n",
		       task_cpu(ri->task),
		       ri->task->comm, ri->task->tgid, ri->task->pid,
		       (long unsigned int)tramp,
		       (long unsigned int)found, (long unsigned int)ri->sp,
		       (unsigned long)(found - ri->sp), ri->rp ? ri->rp->kp.addr : 0);
		*found = (unsigned long)ri->ret_addr;
		retval = 0;
	} else {
		printk(KERN_INFO "---> [%d] %s (%d/%d): tramp (%08lx) "
		       "NOT found at sp = %08lx - %08lx\n",
		       task_cpu(ri->task),
		       ri->task->comm, ri->task->tgid, ri->task->pid,
		       (long unsigned int)tramp,
		       (long unsigned int)ri->sp,
		       ri->rp ? ri->rp->kp.addr : 0);
	}

	return retval;
}

static void krp_inst_flush(struct task_struct *task)
{
	unsigned long flags;
	struct kretprobe_instance *ri;
	struct hlist_node *tmp;
	struct hlist_head *head;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	spin_lock_irqsave(&kretprobe_lock, flags);
	head = kretprobe_inst_table_head(task);
	swap_hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task == task) {
			printk("task[%u %u %s]: flush krp_inst, ret_addr=%p\n",
				task->tgid, task->pid, task->comm,
				ri->ret_addr);
			recycle_rp_inst(ri);
		}
	}
	spin_unlock_irqrestore(&kretprobe_lock, flags);
}

static void do_put_task_handler(struct task_struct *task)
{
	/* task has died */
	krp_inst_flush(task);
	swap_ktd_put_task(task);
}

#ifdef CONFIG_SWAP_HOOK_TASKDATA

#include <swap/hook_taskdata.h>

static struct hook_taskdata put_hook = {
	.owner = THIS_MODULE,
	.put_task = do_put_task_handler,
};

static int put_task_once(void)
{
	return 0;
}

static int put_task_init(void)
{
	return hook_taskdata_reg(&put_hook);
}

static void put_task_uninit(void)
{
	hook_taskdata_unreg(&put_hook);
}

#else /* CONFIG_SWAP_HOOK_TASKDATA */

/* Handler is called the last because it is registered the first */
static int put_task_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct task_struct *t = (struct task_struct *)swap_get_karg(regs, 0);

	do_put_task_handler(t);

	return 0;
}

static struct kprobe put_task_kp = {
	.pre_handler = put_task_handler,
};

static int put_task_once(void)
{
	const char *sym = "__put_task_struct";

	put_task_kp.addr = swap_ksyms(sym);
	if (put_task_kp.addr == 0) {
		pr_err("ERROR: symbol '%s' not found\n", sym);
		return -ESRCH;
	}

	return 0;
}

static int put_task_init(void)
{
	return swap_register_kprobe(&put_task_kp);
}

static void put_task_uninit(void)
{
	swap_unregister_kprobe(&put_task_kp);
}

#endif /* CONFIG_SWAP_HOOK_TASKDATA */


static int init_module_deps(void)
{
	int ret;

	sched_addr = swap_ksyms("__switch_to");
	exit_addr = swap_ksyms("do_exit");
	sys_exit_group_addr = swap_ksyms("sys_exit_group");
	do_group_exit_addr = swap_ksyms("do_group_exit");
	sys_exit_addr = swap_ksyms("sys_exit");

	if (sched_addr == 0 ||
	    exit_addr == 0 ||
	    sys_exit_group_addr == 0 ||
	    do_group_exit_addr == 0 ||
	    sys_exit_addr == 0) {
		return -ESRCH;
	}

	ret = init_module_dependencies();
	if (ret)
		return ret;

	return arch_init_module_deps();
}

static int once(void)
{
	int i, ret;
	const char *sym;

	ret = put_task_once();
	if (ret)
		return ret;

	sym = "module_alloc";
	__module_alloc = (void *)swap_ksyms(sym);
	if (__module_alloc == NULL)
		goto not_found;

	sym = "module_free";
	__module_free = (void *)swap_ksyms(sym);
	if (__module_free == NULL)
		goto not_found;

	ret = init_module_deps();
	if (ret)
		return ret;

	/*
	 * FIXME allocate the probe table, currently defined statically
	 * initialize all list heads
	 */
	for (i = 0; i < KPROBE_TABLE_SIZE; ++i) {
		INIT_HLIST_HEAD(&kprobe_table[i]);
		INIT_HLIST_HEAD(&kretprobe_inst_table[i]);
	}

	return 0;

not_found:
	printk(KERN_INFO "ERROR: symbol '%s' not found\n", sym);
	return -ESRCH;
}

static int init_kprobes(void)
{
	int ret;

	init_sm();
	atomic_set(&kprobe_count, 0);

	ret = swap_td_raw_init();
	if (ret)
		return ret;

	ret = swap_arch_init_kprobes();
	if (ret)
		goto td_raw_uninit;

	ret = swap_ktd_init();
	if (ret)
		goto arch_kp_exit;

	ret = kprobe_cur_reg();
	if (ret)
		goto ktd_uninit;

	ret = put_task_init();
	if (ret)
		goto cur_uninit;

	return 0;

cur_uninit:
	kprobe_cur_unreg();
ktd_uninit:
	swap_ktd_uninit_top();
	swap_ktd_uninit_bottom();
arch_kp_exit:
	swap_arch_exit_kprobes();
td_raw_uninit:
	swap_td_raw_uninit();
	return ret;
}

static void exit_kprobes(void)
{
	swap_ktd_uninit_top();
	put_task_uninit();
	kprobe_cur_unreg();
	swap_ktd_uninit_bottom();
	swap_arch_exit_kprobes();
	swap_td_raw_uninit();
	exit_sm();
}

SWAP_LIGHT_INIT_MODULE(once, init_kprobes, exit_kprobes, NULL, NULL);

MODULE_LICENSE("GPL");
