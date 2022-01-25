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
 *  2016    Vyacheslav Cherkashin
 */


#include <linux/kconfig.h>
#include <kprobe/swap_kprobes.h>
#include <kprobe/swap_kprobes_deps.h>
#include <ksyms/ksyms.h>
#include <writer/kernel_operations.h>
#include "us_slot_manager.h"
#include "sspt/sspt.h"
#include "sspt/sspt_filter.h"
#include "helper.h"


static atomic_t stop_flag = ATOMIC_INIT(0);


/*
 ******************************************************************************
 *                               do_page_fault()                              *
 ******************************************************************************
 */

struct pf_data {
	unsigned long addr;

	struct pt_regs *pf_regs;
	unsigned long save_pc;
};

static int entry_handler_pf(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct pf_data *data = (struct pf_data *)ri->data;

#if defined(CONFIG_ARM)
	data->addr = swap_get_karg(regs, 0);
	data->pf_regs = (struct pt_regs *)swap_get_karg(regs, 2);
	data->save_pc = data->pf_regs->ARM_pc;
#elif defined(CONFIG_X86_32)
	data->addr = read_cr2();
	data->pf_regs = (struct pt_regs *)swap_get_karg(regs, 0);
	data->save_pc = data->pf_regs->ip;
#elif defined(CONFIG_ARM64)
	data->addr = swap_get_karg(regs, 0);
	data->pf_regs = (struct pt_regs *)swap_get_karg(regs, 2);
	data->save_pc = data->pf_regs->pc;
	/* FIXME: to implement */
#else
	#error "this architecture is not supported"
#endif /* CONFIG_arch */

	if (data->addr) {
		int ret = 0;
		struct sspt_proc *proc;

		proc = sspt_proc_get_by_task(current);
		if (proc) {
			if (proc->r_state_addr == data->addr) {
				/* skip ret_handler_pf() for current task */
				ret = 1;
			}

			sspt_proc_put(proc);
		}

		return ret;
	}

	return 0;
}

static unsigned long cb_pf(void *data)
{
	unsigned long addr = *(unsigned long *)data;

	hh_page_fault(addr);

	return 0;
}

/* Detects when IPs are really loaded into phy mem and installs probes. */
static int ret_handler_pf(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct task_struct *task = current;
	struct pf_data *data = (struct pf_data *)ri->data;
	unsigned long addr;
	int ret;

	if (is_kthread(task))
		return 0;

	/* skip fixup page_fault */
#if defined(CONFIG_ARM)
	if (data->save_pc != data->pf_regs->ARM_pc)
		return 0;
#elif defined(CONFIG_X86_32)
	if (data->save_pc != data->pf_regs->ip)
		return 0;
#elif defined(CONFIG_ARM64)
	if (data->save_pc != data->pf_regs->pc)
		return 0;
#endif /* CONFIG_arch */

	/* TODO: check return value */
	addr = data->addr;
	ret = set_jump_cb((unsigned long)ri->ret_addr, regs, cb_pf,
			  &addr, sizeof(addr));

	if (ret == 0)
		ri->ret_addr = (unsigned long *)get_jump_addr();

	return 0;
}

static struct kretprobe mf_kretprobe = {
	.entry_handler = entry_handler_pf,
	.handler = ret_handler_pf,
	.data_size = sizeof(struct pf_data)
};

static int register_mf(void)
{
	int ret;

	ret = swap_register_kretprobe(&mf_kretprobe);
	if (ret)
		pr_err("swap_register_kretprobe(handle_mm_fault) ret=%d!\n", ret);

	return ret;
}

static void unregister_mf(void)
{
	swap_unregister_kretprobe(&mf_kretprobe);
}





/*
 ******************************************************************************
 *                              copy_process()                                *
 ******************************************************************************
 */
static atomic_t rm_uprobes_child_cnt = ATOMIC_INIT(0);

static unsigned long cb_clean_child(void *data)
{
	struct task_struct *parent = current;
	struct task_struct *child = *(struct task_struct **)data;

	hh_clean_task(parent, child);

	atomic_dec(&rm_uprobes_child_cnt);
	return 0;
}

static void rm_uprobes_child(struct kretprobe_instance *ri,
			     struct pt_regs *regs, struct task_struct *child)
{
	int ret;

	if (!sspt_proc_by_task(current))
		return;

	/* set jumper */
	ret = set_jump_cb((unsigned long)ri->ret_addr, regs,
			  cb_clean_child, &child, sizeof(child));
	if (ret == 0) {
		atomic_inc(&rm_uprobes_child_cnt);
		ri->ret_addr = (unsigned long *)get_jump_addr();
	} else {
		WARN_ON(1);
	}
}


static int pre_handler_cp(struct kprobe *p, struct pt_regs *regs)
{
	if (is_kthread(current))
		goto out;

	if (atomic_read(&stop_flag))
		call_mm_release(current);

out:
	return 0;
}

static atomic_t copy_process_cnt = ATOMIC_INIT(0);

static int entry_handler_cp(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	atomic_inc(&copy_process_cnt);

	return 0;
}

/* Delete uprobs in children at fork */
static int ret_handler_cp(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct task_struct *task;

	task = (struct task_struct *)regs_return_value(regs);
	if (IS_ERR(task))
		goto out;

	if (task->mm != current->mm)	/* check flags CLONE_VM */
		rm_uprobes_child(ri, regs, task);
out:
	atomic_dec(&copy_process_cnt);
	return 0;
}

static struct kretprobe cp_kretprobe = {
	.entry_handler = entry_handler_cp,
	.handler = ret_handler_cp,
};

static struct kprobe cp_kprobe = {
	.pre_handler = pre_handler_cp
};

static int register_cp(void)
{
	int ret;


	ret = swap_register_kprobe(&cp_kprobe);
	if (ret)
		pr_err("swap_register_kprobe(copy_process) ret=%d!\n", ret);

	ret = swap_register_kretprobe(&cp_kretprobe);
	if (ret) {
		pr_err("swap_register_kretprobe(copy_process) ret=%d!\n", ret);
		swap_unregister_kprobe(&cp_kprobe);
	}

	return ret;
}

static void unregister_cp(void)
{
	swap_unregister_kretprobe_top(&cp_kretprobe, 0);
	do {
		synchronize_sched();
	} while (atomic_read(&copy_process_cnt));
	swap_unregister_kretprobe_bottom(&cp_kretprobe);
	swap_unregister_kprobe(&cp_kprobe);

	do {
		synchronize_sched();
	} while (atomic_read(&rm_uprobes_child_cnt));
}





/*
 ******************************************************************************
 *                                mm_release()                                *
 ******************************************************************************
 */

/* Detects when target process removes IPs. */
static int mr_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct task_struct *task = (struct task_struct *)swap_get_karg(regs, 0);

	hh_mm_release(task);

	return 0;
}

static struct kprobe mr_kprobe = {
	.pre_handler = mr_pre_handler
};

static int register_mr(void)
{
	int ret;

	ret = swap_register_kprobe(&mr_kprobe);
	if (ret)
		pr_err("swap_register_kprobe(mm_release) ret=%d!\n", ret);

	return ret;
}

static void unregister_mr(void)
{
	swap_unregister_kprobe(&mr_kprobe);
}





/*
 ******************************************************************************
 *                                 do_munmap()                                *
 ******************************************************************************
 */
struct unmap_data {
	unsigned long start;
	size_t len;
};

static atomic_t unmap_cnt = ATOMIC_INIT(0);

static unsigned long cb_munmap(void *data)
{
	struct unmap_data *umd = (struct unmap_data *)data;

	hh_munmap(umd->start, umd->start + umd->len);

	atomic_dec(&unmap_cnt);
	return 0;
}

static int entry_handler_unmap(struct kretprobe_instance *ri,
			       struct pt_regs *regs)
{
	struct unmap_data *data = (struct unmap_data *)ri->data;

	data->start = swap_get_karg(regs, 1);
	data->len = (size_t)PAGE_ALIGN(swap_get_karg(regs, 2));

	atomic_inc(&unmap_cnt);
	return 0;
}

static int ret_handler_unmap(struct kretprobe_instance *ri,
			     struct pt_regs *regs)
{
	int ret;

	if (regs_return_value(regs)) {
		atomic_dec(&unmap_cnt);
		return 0;
	}

	ret = set_jump_cb((unsigned long)ri->ret_addr, regs, cb_munmap,
			  (struct unmap_data *)ri->data,
			  sizeof(struct unmap_data));
	if (ret == 0) {
		ri->ret_addr = (unsigned long *)get_jump_addr();
	} else {
		WARN_ON(1);
		atomic_dec(&unmap_cnt);
	}

	return 0;
}

static struct kretprobe unmap_kretprobe = {
	.entry_handler = entry_handler_unmap,
	.handler = ret_handler_unmap,
	.data_size = sizeof(struct unmap_data)
};

static int register_unmap(void)
{
	int ret;

	ret = swap_register_kretprobe(&unmap_kretprobe);
	if (ret)
		pr_err("swap_register_kprobe(do_munmap) ret=%d!\n", ret);

	return ret;
}

static void unregister_unmap(void)
{
	swap_unregister_kretprobe_top(&unmap_kretprobe, 0);
	do {
		synchronize_sched();
	} while (atomic_read(&unmap_cnt));
	swap_unregister_kretprobe_bottom(&unmap_kretprobe);
}





/*
 ******************************************************************************
 *                               do_mmap_pgoff()                              *
 ******************************************************************************
 */
static int ret_handler_mmap(struct kretprobe_instance *ri,
			    struct pt_regs *regs)
{
	unsigned long addr = regs_return_value(regs);

	hh_mmap(NULL, addr);

	return 0;
}

static struct kretprobe mmap_kretprobe = {
	.handler = ret_handler_mmap
};

static int register_mmap(void)
{
	int ret;

	ret = swap_register_kretprobe(&mmap_kretprobe);
	if (ret)
		pr_err("swap_register_kretprobe(do_mmap_pgoff) ret=%d!\n", ret);

	return ret;
}

static void unregister_mmap(void)
{
	swap_unregister_kretprobe(&mmap_kretprobe);
}





/*
 ******************************************************************************
 *                               set_task_comm()                              *
 ******************************************************************************
 */
struct comm_data {
	struct task_struct *task;
};

static unsigned long cb_check_and_install(void *data)
{
	hh_set_comm(current);

	return 0;
}

static int entry_handler_comm(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	struct comm_data *data = (struct comm_data *)ri->data;

	data->task = (struct task_struct *)swap_get_karg(regs, 0);

	return 0;
}

static int ret_handler_comm(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct task_struct *task;
	int ret;

	if (is_kthread(current))
		return 0;

	task = ((struct comm_data *)ri->data)->task;
	if (task != current)
		return 0;

	ret = set_jump_cb((unsigned long)ri->ret_addr, regs,
			  cb_check_and_install, NULL, 0);
	if (ret == 0)
		ri->ret_addr = (unsigned long *)get_jump_addr();

	return 0;
}

static struct kretprobe comm_kretprobe = {
	.entry_handler = entry_handler_comm,
	.handler = ret_handler_comm,
	.data_size = sizeof(struct comm_data)
};

static int register_comm(void)
{
	int ret;

	ret = swap_register_kretprobe(&comm_kretprobe);
	if (ret)
		pr_err("swap_register_kretprobe(set_task_comm) ret=%d!\n", ret);

	return ret;
}

static void unregister_comm(void)
{
	swap_unregister_kretprobe(&comm_kretprobe);
}




/*
 ******************************************************************************
 *                               release_task()                               *
 ******************************************************************************
 */
static int release_task_h(struct kprobe *p, struct pt_regs *regs)
{
	struct task_struct *task = (struct task_struct *)swap_get_karg(regs, 0);
	struct task_struct *cur = current;

	if (cur->flags & PF_KTHREAD)
		return 0;

	/* EXEC: change group leader */
	if (cur != task && task->pid == cur->pid)
		hh_change_leader(task, cur);

	return 0;
}

struct kprobe release_task_kp = {
	.pre_handler = release_task_h,
};

static int reg_release_task(void)
{
	return swap_register_kprobe(&release_task_kp);
}

static void unreg_release_task(void)
{
	swap_unregister_kprobe(&release_task_kp);
}





/**
 * @brief Registration of helper
 *
 * @return Error code
 */
int helper_reg(void)
{
	int ret = 0;

	atomic_set(&stop_flag, 0);

	/* tracking group leader changing */
	ret = reg_release_task();
	if (ret)
		return ret;

	/*
	 * install probe on 'set_task_comm' to detect when field comm struct
	 * task_struct changes
	 */
	ret = register_comm();
	if (ret)
		goto unreg_rel_task;

	/* install probe on 'do_munmap' to detect when for remove US probes */
	ret = register_unmap();
	if (ret)
		goto unreg_comm;

	/* install probe on 'mm_release' to detect when for remove US probes */
	ret = register_mr();
	if (ret)
		goto unreg_unmap;

	/* install probe on 'copy_process' to disarm children process */
	ret = register_cp();
	if (ret)
		goto unreg_mr;

	/* install probe on 'do_mmap_pgoff' to detect when mapping file */
	ret = register_mmap();
	if (ret)
		goto unreg_cp;

	/*
	 * install probe on 'handle_mm_fault' to detect when US pages will be
	 * loaded
	 */
	ret = register_mf();
	if (ret)
		goto unreg_mmap;

	return ret;

unreg_mmap:
	unregister_mmap();

unreg_cp:
	unregister_cp();

unreg_mr:
	unregister_mr();

unreg_unmap:
	unregister_unmap();

unreg_comm:
	unregister_comm();

unreg_rel_task:
	unreg_release_task();

	return ret;
}

/**
 * @brief Unegistration of helper bottom
 *
 * @return Void
 */
void helper_unreg_top(void)
{
	unregister_mf();
	atomic_set(&stop_flag, 1);
}

/**
 * @brief Unegistration of helper top
 *
 * @return Void
 */
void helper_unreg_bottom(void)
{
	unregister_mmap();
	unregister_cp();
	unregister_mr();
	unregister_unmap();
	unregister_comm();
	unreg_release_task();
}

/**
 * @brief Initialization of helper
 *
 * @return Error code
 */
int helper_once(void)
{
	const char *sym;

	sym = "do_page_fault";
	mf_kretprobe.kp.addr = swap_ksyms(sym);
	if (mf_kretprobe.kp.addr == 0)
		goto not_found;

	sym = "copy_process";
	cp_kretprobe.kp.addr = swap_ksyms_substr(sym);
	if (cp_kretprobe.kp.addr == 0)
		goto not_found;
	cp_kprobe.addr = cp_kretprobe.kp.addr;

	sym = "mm_release";
	mr_kprobe.addr = swap_ksyms(sym);
	if (mr_kprobe.addr == 0)
		goto not_found;

	sym = "do_munmap";
	unmap_kretprobe.kp.addr = swap_ksyms(sym);
	if (unmap_kretprobe.kp.addr == 0)
		goto not_found;

	sym = "do_mmap_pgoff";
	mmap_kretprobe.kp.addr = swap_ksyms(sym);
	if (mmap_kretprobe.kp.addr == 0)
		goto not_found;

	sym = "__set_task_comm";
	comm_kretprobe.kp.addr = swap_ksyms(sym);
	if (comm_kretprobe.kp.addr == 0)
		goto not_found;

	sym = "release_task";
	release_task_kp.addr = swap_ksyms(sym);
	if (release_task_kp.addr == 0)
		goto not_found;

	return 0;

not_found:
	pr_err("ERROR: symbol '%s' not found\n", sym);
	return -ESRCH;
}

int helper_init(void)
{
	return 0;
}

void helper_uninit(void)
{
}
