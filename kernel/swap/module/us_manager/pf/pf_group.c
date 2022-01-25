/*
 *  SWAP uprobe manager
 *  modules/us_manager/pf/pf_group.c
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013	 Vyacheslav Cherkashin: SWAP us_manager implement
 *
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/namei.h>
#include <linux/mman.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include "pf_group.h"
#include "proc_filters.h"
#include "../sspt/sspt_filter.h"
#include "../us_manager_common.h"
#include <us_manager/img/img_proc.h>
#include <us_manager/img/img_file.h>
#include <us_manager/img/img_ip.h>
#include <us_manager/sspt/sspt_proc.h>
#include <us_manager/helper.h>
#include <us_manager/us_common_file.h>
#include <task_ctx/task_ctx.h>


struct pf_group {
	struct list_head list;
	struct img_proc *i_proc;
	struct proc_filter filter;
	struct pfg_msg_cb *msg_cb;
	atomic_t usage;

	spinlock_t pl_lock;	/* for proc_list */
	struct list_head proc_list;
};

struct pl_struct {
	struct list_head list;
	struct sspt_proc *proc;
};


static atomic_t pfg_counter = ATOMIC_INIT(0);

static LIST_HEAD(pfg_list);
static DECLARE_RWSEM(pfg_list_sem);
static DECLARE_RWSEM(uninstall_sem);

static void pfg_list_rlock(void)
{
	down_read(&pfg_list_sem);
}

static void pfg_list_runlock(void)
{
	up_read(&pfg_list_sem);
}

static void pfg_list_wlock(void)
{
	down_write(&pfg_list_sem);
}

static void pfg_list_wunlock(void)
{
	up_write(&pfg_list_sem);
}


/* struct pl_struct */
static struct pl_struct *create_pl_struct(struct sspt_proc *proc)
{
	struct pl_struct *pls = kmalloc(sizeof(*pls), GFP_ATOMIC);

	if (pls) {
		INIT_LIST_HEAD(&pls->list);
		pls->proc = sspt_proc_get(proc);
	}

	return pls;
}

static void free_pl_struct(struct pl_struct *pls)
{
	sspt_proc_put(pls->proc);
	kfree(pls);
}
/* struct pl_struct */

static struct pf_group *pfg_create(void)
{
	struct pf_group *pfg = kmalloc(sizeof(*pfg), GFP_ATOMIC);

	if (pfg == NULL)
		return NULL;

	pfg->i_proc = img_proc_create();
	if (pfg->i_proc == NULL)
		goto create_pfg_fail;

	INIT_LIST_HEAD(&pfg->list);
	memset(&pfg->filter, 0, sizeof(pfg->filter));
	spin_lock_init(&pfg->pl_lock);
	INIT_LIST_HEAD(&pfg->proc_list);
	pfg->msg_cb = NULL;
	atomic_set(&pfg->usage, 1);

	atomic_inc(&pfg_counter);
	return pfg;

create_pfg_fail:

	kfree(pfg);

	return NULL;
}

static void pfg_free(struct pf_group *pfg)
{
	struct pl_struct *pl, *n;

	img_proc_free(pfg->i_proc);
	free_pf(&pfg->filter);
	list_for_each_entry_safe(pl, n, &pfg->proc_list, list) {
		sspt_proc_del_filter(pl->proc, pfg);
		free_pl_struct(pl);
	}

	atomic_dec(&pfg_counter);
	kfree(pfg);
}

bool pfg_is_unloadable(void)
{
	return !(atomic_read(&pfg_counter) + !img_proc_is_unloadable());
}

static int pfg_add_proc(struct pf_group *pfg, struct sspt_proc *proc)
{
	struct pl_struct *pls;

	pls = create_pl_struct(proc);
	if (pls == NULL)
		return -ENOMEM;

	spin_lock(&pfg->pl_lock);
	list_add(&pls->list, &pfg->proc_list);
	spin_unlock(&pfg->pl_lock);

	return 0;
}

static int pfg_del_proc(struct pf_group *pfg, struct sspt_proc *proc)
{
	struct pl_struct *pls, *pls_free = NULL;

	spin_lock(&pfg->pl_lock);
	list_for_each_entry(pls, &pfg->proc_list, list) {
		if (pls->proc == proc) {
			list_del(&pls->list);
			pls_free = pls;
			break;
		}
	}
	spin_unlock(&pfg->pl_lock);

	if (pls_free)
		free_pl_struct(pls_free);

	return !!pls_free;
}


/* called with pfg_list_lock held */
static void pfg_add_to_list(struct pf_group *pfg)
{
	list_add(&pfg->list, &pfg_list);
}

/* called with pfg_list_lock held */
static void pfg_del_from_list(struct pf_group *pfg)
{
	list_del(&pfg->list);
}


static void msg_info(struct sspt_filter *f, void *data)
{
	if (f->pfg_is_inst == false) {
		struct pfg_msg_cb *cb;

		f->pfg_is_inst = true;

		cb = pfg_msg_cb_get(f->pfg);
		if (cb) {
			struct dentry *dentry;

			dentry = (struct dentry *)f->pfg->filter.priv;

			if (cb->msg_info)
				cb->msg_info(f->proc->leader, dentry);

			if (cb->msg_status_info)
				cb->msg_status_info(f->proc->leader);
		}
	}
}

static void first_install(struct task_struct *task, struct sspt_proc *proc)
{
	down_write(&task->mm->mmap_sem);
	sspt_proc_on_each_filter(proc, msg_info, NULL);
	sspt_proc_install(proc);
	up_write(&task->mm->mmap_sem);
}

static void subsequent_install(struct task_struct *task,
			       struct sspt_proc *proc, unsigned long page_addr)
{
	down_write(&task->mm->mmap_sem);
	sspt_proc_install_page(proc, page_addr);
	up_write(&task->mm->mmap_sem);
}


/**
 * @brief Get dentry struct by path
 *
 * @param path Path to file
 * @return Pointer on dentry struct on NULL
 */
struct dentry *dentry_by_path(const char *path)
{
	struct dentry *d;

	d = swap_get_dentry(path);
	if (d)
		dput(d);

	return d;
}
EXPORT_SYMBOL_GPL(dentry_by_path);


int pfg_msg_cb_set(struct pf_group *pfg, struct pfg_msg_cb *msg_cb)
{
	if (pfg->msg_cb)
		return -EBUSY;

	pfg->msg_cb = msg_cb;

	return 0;
}
EXPORT_SYMBOL_GPL(pfg_msg_cb_set);

void pfg_msg_cb_reset(struct pf_group *pfg)
{
	pfg->msg_cb = NULL;
}
EXPORT_SYMBOL_GPL(pfg_msg_cb_reset);

struct pfg_msg_cb *pfg_msg_cb_get(struct pf_group *pfg)
{
	return pfg->msg_cb;
}

/**
 * @brief Get pf_group struct by dentry
 *
 * @param dentry Dentry of file
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_by_dentry(struct dentry *dentry, void *priv)
{
	struct pf_group *pfg;

	pfg_list_wlock();
	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_by_dentry(&pfg->filter, dentry)) {
			atomic_inc(&pfg->usage);
			goto unlock;
		}
	}

	pfg = pfg_create();
	if (pfg == NULL)
		goto unlock;

	set_pf_by_dentry(&pfg->filter, dentry, priv);

	pfg_add_to_list(pfg);

unlock:
	pfg_list_wunlock();
	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_by_dentry);

/**
 * @brief Get pf_group struct by TGID
 *
 * @param tgid Thread group ID
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_by_tgid(pid_t tgid, void *priv)
{
	struct pf_group *pfg;

	pfg_list_wlock();
	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_by_tgid(&pfg->filter, tgid)) {
			atomic_inc(&pfg->usage);
			goto unlock;
		}
	}

	pfg = pfg_create();
	if (pfg == NULL)
		goto unlock;

	set_pf_by_tgid(&pfg->filter, tgid, priv);

	pfg_add_to_list(pfg);

unlock:
	pfg_list_wunlock();
	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_by_tgid);

/**
 * @brief Get pf_group struct by comm
 *
 * @param comm Task comm
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_by_comm(char *comm, void *priv)
{
	int ret;
	struct pf_group *pfg;

	pfg_list_wlock();
	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_by_comm(&pfg->filter, comm)) {
			atomic_inc(&pfg->usage);
			goto unlock;
		}
	}

	pfg = pfg_create();
	if (pfg == NULL)
		goto unlock;

	ret = set_pf_by_comm(&pfg->filter, comm, priv);
	if (ret) {
		printk(KERN_ERR "ERROR: set_pf_by_comm, ret=%d\n", ret);
		pfg_free(pfg);
		pfg = NULL;
		goto unlock;
	}

	pfg_add_to_list(pfg);
unlock:
	pfg_list_wunlock();
	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_by_comm);

/**
 * @brief Get pf_group struct for each process
 *
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_dumb(void *priv)
{
	struct pf_group *pfg;

	pfg_list_wlock();
	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_dumb(&pfg->filter)) {
			atomic_inc(&pfg->usage);
			goto unlock;
		}
	}

	pfg = pfg_create();
	if (pfg == NULL)
		goto unlock;

	set_pf_dumb(&pfg->filter, priv);

	pfg_add_to_list(pfg);

unlock:
	pfg_list_wunlock();
	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_dumb);

/**
 * @brief Put pf_group struct
 *
 * @param pfg Pointer to the pf_group struct
 * @return Void
 */
void put_pf_group(struct pf_group *pfg)
{
	if (atomic_dec_and_test(&pfg->usage)) {
		pfg_list_wlock();
		pfg_del_from_list(pfg);
		pfg_list_wunlock();

		pfg_free(pfg);
	}
}
EXPORT_SYMBOL_GPL(put_pf_group);

/**
 * @brief Register prober for pf_grpup struct
 *
 * @param pfg Pointer to the pf_group struct
 * @param dentry Dentry of file
 * @param offset Function offset
 * @param probe_info Pointer to the related probe_info struct
 * @return pointer to the img_ip struct or error
 */
struct img_ip *pf_register_probe(struct pf_group *pfg, struct dentry *dentry,
				 unsigned long offset, struct probe_desc *pd)
{
	return img_proc_add_ip(pfg->i_proc, dentry, offset, pd);
}
EXPORT_SYMBOL_GPL(pf_register_probe);

/**
 * @brief Unregister prober from pf_grpup struct
 *
 * @param pfg Pointer to the pf_group struct
 * @param ip Pointer to the img_ip struct
 * @return Void
 */
void pf_unregister_probe(struct pf_group *pfg, struct img_ip *ip)
{
	WARN(IS_ERR_OR_NULL(ip), "invalid img_ip");
	img_proc_del_ip(pfg->i_proc, ip);
}
EXPORT_SYMBOL_GPL(pf_unregister_probe);

static int check_task_on_filters(struct task_struct *task)
{
	int ret = 0;
	struct pf_group *pfg;

	pfg_list_rlock();
	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_task_f(&pfg->filter, task)) {
			ret = 1;
			goto unlock;
		}
	}

unlock:
	pfg_list_runlock();
	return ret;
}

enum pf_inst_flag {
	PIF_NONE,
	PIF_FIRST,
	PIF_SECOND,
	PIF_ADD_PFG
};

static enum pf_inst_flag pfg_check_task(struct task_struct *task)
{
	struct pf_group *pfg;
	struct sspt_proc *proc = NULL;
	enum pf_inst_flag flag = PIF_NONE;

	pfg_list_rlock();
	list_for_each_entry(pfg, &pfg_list, list) {
		bool put_flag = false;

		if (check_task_f(&pfg->filter, task) == NULL)
			continue;

		if (proc == NULL) {
			proc = sspt_proc_get_by_task(task);
			put_flag = !!proc;
		}

		if (proc) {
			flag = flag == PIF_NONE ? PIF_SECOND : flag;
		} else if (task->tgid == task->pid) {
			proc = sspt_proc_get_by_task_or_new(task);
			if (proc == NULL) {
				printk(KERN_ERR "cannot create sspt_proc\n");
				break;
			}
			put_flag = true;
			flag = PIF_FIRST;
		}

		if (proc) {
			mutex_lock(&proc->filters.mtx);
				if (sspt_proc_is_filter_new(proc, pfg)) {
					img_proc_copy_to_sspt(pfg->i_proc, proc);
					sspt_proc_add_filter(proc, pfg);
					pfg_add_proc(pfg, proc);
					flag = flag == PIF_FIRST ? flag : PIF_ADD_PFG;
			}
			mutex_unlock(&proc->filters.mtx);
			if (put_flag)
				sspt_proc_put(proc);
		}
	}
	pfg_list_runlock();

	return flag;
}

static void pfg_all_del_proc(struct sspt_proc *proc)
{
	struct pf_group *pfg;

	pfg_list_rlock();
	list_for_each_entry(pfg, &pfg_list, list)
		pfg_del_proc(pfg, proc);
	pfg_list_runlock();
}

/**
 * @brief Check task and install probes on demand
 *
 * @prarm task Pointer on the task_struct struct
 * @return Void
 */
void check_task_and_install(struct task_struct *task)
{
	struct sspt_proc *proc;
	enum pf_inst_flag flag;

	flag = pfg_check_task(task);
	switch (flag) {
	case PIF_FIRST:
		proc = sspt_proc_get_by_task(task);
		if (proc) {
			sspt_proc_priv_create(proc);
			first_install(task, proc);
			sspt_proc_put(proc);
		}
		break;
	case PIF_ADD_PFG:
		proc = sspt_proc_get_by_task(task);
		if (proc) {
			first_install(task, proc);
			sspt_proc_put(proc);
		}
		break;

	case PIF_NONE:
	case PIF_SECOND:
		break;
	}
}

/**
 * @brief Check task and install probes on demand
 *
 * @prarm task Pointer on the task_struct struct
 * @param page_addr Page fault address
 * @return Void
 */
void call_page_fault(struct task_struct *task, unsigned long page_addr)
{
	struct sspt_proc *proc;
	enum pf_inst_flag flag;

	flag = pfg_check_task(task);
	switch (flag) {
	case PIF_FIRST:
		proc = sspt_proc_get_by_task(task);
		if (proc) {
			sspt_proc_priv_create(proc);
			first_install(task, proc);
			sspt_proc_put(proc);
		}
		break;
	case PIF_ADD_PFG:
		proc = sspt_proc_get_by_task(task);
		if (proc) {
			first_install(task, proc);
			sspt_proc_put(proc);
		}
		break;

	case PIF_SECOND:
		proc = sspt_proc_get_by_task(task);
		if (proc) {
			subsequent_install(task, proc, page_addr);
			sspt_proc_put(proc);
		}
		break;

	case PIF_NONE:
		break;
	}
}

/**
 * @brief Uninstall probes from the sspt_proc struct
 *
 * @prarm proc Pointer on the sspt_proc struct
 * @return Void
 */

/* called with sspt_proc_write_lock() */
void uninstall_proc(struct sspt_proc *proc)
{
	struct task_struct *task = proc->leader;

	sspt_proc_uninstall(proc, task, US_UNREGS_PROBE);
	sspt_proc_cleanup(proc);
}


static void mmr_from_exit(struct sspt_proc *proc)
{
	BUG_ON(proc->leader != current);

	sspt_proc_write_lock();
	list_del(&proc->list);
	sspt_proc_write_unlock();

	uninstall_proc(proc);

	pfg_all_del_proc(proc);
}

static void mmr_from_exec(struct sspt_proc *proc)
{
	BUG_ON(proc->leader != current);

	if (proc->suspect.after_exec) {
		sspt_proc_uninstall(proc, proc->leader, US_UNREGS_PROBE);
	} else {
		mmr_from_exit(proc);
	}
}

/**
 * @brief Remove probes from the task on demand
 *
 * @prarm task Pointer on the task_struct struct
 * @return Void
 */
void call_mm_release(struct task_struct *task)
{
	struct sspt_proc *proc;

	down_read(&uninstall_sem);
	proc = sspt_proc_get_by_task(task);
	if (proc) {
		if (task->flags & PF_EXITING)
			mmr_from_exit(proc);
		else
			mmr_from_exec(proc);
		sspt_proc_put(proc);
	}
	up_read(&uninstall_sem);
}

/**
 * @brief Legacy code, it is need remove
 *
 * @param addr Page address
 * @return Void
 */
void uninstall_page(unsigned long addr)
{

}


static void install_cb(void *unused)
{
	check_task_and_install(current);
}




struct task_item {
	struct list_head list;
	struct task_struct *task;
};

static void tasks_get(struct list_head *head)
{
	struct task_item *item;
	struct task_struct *task;

	rcu_read_lock();
	for_each_process(task) {
		if (task->flags & PF_KTHREAD)
			continue;

		if (sspt_proc_by_task(task))
			continue;

		/* TODO: get rid of GFP_ATOMIC */
		item = kmalloc(sizeof(*item), GFP_ATOMIC);
		if (item == NULL) {
			WARN(1, "out of memory\n");
			goto unlock;
		}

		get_task_struct(task);
		item->task = task;
		list_add(&item->list, head);
	}

unlock:
	rcu_read_unlock();
}

static void tasks_install_and_put(struct list_head *head)
{
	struct task_item *item, *n;

	list_for_each_entry_safe(item, n, head, list) {
		int ret;
		struct task_struct *task;

		task = item->task;
		if (!check_task_on_filters(task))
			goto put_task;

		ret = taskctx_run(task, install_cb, NULL);
		if (ret) {
			pr_err("cannot tracking task[%u %u %s] ret=%d\n",
			       task->tgid, task->pid, task->comm, ret);
		}

put_task:
		put_task_struct(task);
		list_del(&item->list);
		kfree(item);
	}
}

static void do_install_all(void)
{
	LIST_HEAD(head);

	tasks_get(&head);
	tasks_install_and_put(&head);
}

/**
 * @brief Install probes on running processes
 *
 * @return Void
 */
void install_all(void)
{
	int ret;

	ret = taskctx_get();
	if (!ret) {
		do_install_all();
		taskctx_put();
	} else {
		pr_err("taskctx_get ret=%d\n", ret);
	}
}

/**
 * @brief Uninstall probes from all processes
 *
 * @return Void
 */
void uninstall_all(void)
{
	struct list_head *proc_list = sspt_proc_list();

	down_write(&uninstall_sem);
	sspt_proc_write_lock();
	while (!list_empty(proc_list)) {
		struct sspt_proc *proc;
		proc = list_first_entry(proc_list, struct sspt_proc, list);

		list_del(&proc->list);

		sspt_proc_write_unlock();
		uninstall_proc(proc);
		sspt_proc_write_lock();
	}
	sspt_proc_write_unlock();
	up_write(&uninstall_sem);
}

static void __do_get_proc(struct sspt_proc *proc, void *data)
{
	struct task_struct *task = proc->leader;

	get_task_struct(task);
	proc->__task = task;
	proc->__mm = get_task_mm(task);
}

static void __do_put_proc(struct sspt_proc *proc, void *data)
{
	if (proc->__mm) {
		mmput(proc->__mm);
		proc->__mm = NULL;
	}

	if (proc->__task) {
		put_task_struct(proc->__task);
		proc->__task = NULL;
	}
}

void get_all_procs(void)
{
	sspt_proc_read_lock();
	on_each_proc_no_lock(__do_get_proc, NULL);
	sspt_proc_read_unlock();
}

void put_all_procs(void)
{
	sspt_proc_read_lock();
	on_each_proc_no_lock(__do_put_proc, NULL);
	sspt_proc_read_unlock();
}

/**
 * @brief For debug
 *
 * @param pfg Pointer to the pf_group struct
 * @return Void
 */

/* debug */
void pfg_print(struct pf_group *pfg)
{
	img_proc_print(pfg->i_proc);
}
EXPORT_SYMBOL_GPL(pfg_print);
/* debug */
