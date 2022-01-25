/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_proc.c
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
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */

#include "sspt.h"
#include "sspt_proc.h"
#include "sspt_page.h"
#include "sspt_feature.h"
#include "sspt_filter.h"
#include "../pf/proc_filters.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <kprobe/swap_ktd.h>
#include <us_manager/us_slot_manager.h>

static LIST_HEAD(proc_probes_list);
static DEFINE_RWLOCK(sspt_proc_rwlock);


struct list_head *sspt_proc_list()
{
	return &proc_probes_list;
}

/**
 * @brief Global read lock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_read_lock(void)
{
	read_lock(&sspt_proc_rwlock);
}

/**
 * @brief Global read unlock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_read_unlock(void)
{
	read_unlock(&sspt_proc_rwlock);
}

/**
 * @brief Global write lock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_write_lock(void)
{
	write_lock(&sspt_proc_rwlock);
}

/**
 * @brief Global write unlock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_write_unlock(void)
{
	write_unlock(&sspt_proc_rwlock);
}

struct ktd_proc {
	struct sspt_proc *proc;
	spinlock_t lock;
};

static void ktd_init(struct task_struct *task, void *data)
{
	struct ktd_proc *kproc = (struct ktd_proc *)data;

	kproc->proc = NULL;
	spin_lock_init(&kproc->lock);
}

static void ktd_exit(struct task_struct *task, void *data)
{
	struct ktd_proc *kproc = (struct ktd_proc *)data;

	WARN_ON(kproc->proc);
}

static struct ktask_data ktd = {
	.init = ktd_init,
	.exit = ktd_exit,
	.size = sizeof(struct ktd_proc),
};

static struct ktd_proc *kproc_by_task(struct task_struct *task)
{
	return (struct ktd_proc *)swap_ktd(&ktd, task);
}

int sspt_proc_init(void)
{
	return swap_ktd_reg(&ktd);
}

void sspt_proc_uninit(void)
{
	swap_ktd_unreg(&ktd);
}

void sspt_change_leader(struct task_struct *prev, struct task_struct *next)
{
	struct ktd_proc *prev_kproc;

	prev_kproc = kproc_by_task(prev);
	spin_lock(&prev_kproc->lock);
	if (prev_kproc->proc) {
		struct ktd_proc *next_kproc;

		next_kproc = kproc_by_task(next);
		get_task_struct(next);

		/* Change the keeper sspt_proc */
		BUG_ON(next_kproc->proc);

		spin_lock(&next_kproc->lock);
		next_kproc->proc = prev_kproc->proc;
		prev_kproc->proc = NULL;
		spin_unlock(&next_kproc->lock);

		/* Set new the task leader to sspt_proc */
		next_kproc->proc->leader = next;

		put_task_struct(prev);
	}
	spin_unlock(&prev_kproc->lock);
}

static void sspt_reset_proc(struct task_struct *task)
{
	struct ktd_proc *kproc;

	kproc = kproc_by_task(task->group_leader);
	spin_lock(&kproc->lock);
	kproc->proc = NULL;
	spin_unlock(&kproc->lock);
}





static struct sspt_proc *sspt_proc_create(struct task_struct *leader)
{
	struct sspt_proc *proc = kzalloc(sizeof(*proc), GFP_KERNEL);

	if (proc) {
		proc->feature = sspt_create_feature();
		if (proc->feature == NULL) {
			kfree(proc);
			return NULL;
		}

		INIT_LIST_HEAD(&proc->list);
		INIT_LIST_HEAD(&proc->files.head);
		init_rwsem(&proc->files.sem);
		proc->tgid = leader->tgid;
		proc->leader = leader;
		/* FIXME: change the task leader */
		proc->sm = create_sm_us(leader);
		mutex_init(&proc->filters.mtx);
		INIT_LIST_HEAD(&proc->filters.head);
		atomic_set(&proc->usage, 1);

		get_task_struct(proc->leader);

		proc->suspect.after_exec = 1;
		proc->suspect.after_fork = 0;
	}

	return proc;
}

static void sspt_proc_free(struct sspt_proc *proc)
{
	put_task_struct(proc->leader);
	free_sm_us(proc->sm);
	sspt_destroy_feature(proc->feature);
	kfree(proc);
}

/**
 * @brief Remove sspt_proc struct
 *
 * @param proc remove object
 * @return Void
 */

/* called with sspt_proc_write_lock() */
void sspt_proc_cleanup(struct sspt_proc *proc)
{
	struct sspt_file *file, *n;

	sspt_proc_del_all_filters(proc);

	down_write(&proc->files.sem);
	list_for_each_entry_safe(file, n, &proc->files.head, list) {
		list_del(&file->list);
		sspt_file_free(file);
	}
	up_write(&proc->files.sem);

	sspt_destroy_feature(proc->feature);

	free_sm_us(proc->sm);
	sspt_reset_proc(proc->leader);
	sspt_proc_put(proc);
}

struct sspt_proc *sspt_proc_get(struct sspt_proc *proc)
{
	atomic_inc(&proc->usage);

	return proc;
}

void sspt_proc_put(struct sspt_proc *proc)
{
	if (atomic_dec_and_test(&proc->usage)) {
		if (proc->__mm) {
			mmput(proc->__mm);
			proc->__mm = NULL;
		}
		if (proc->__task) {
			put_task_struct(proc->__task);
			proc->__task = NULL;
		}

		WARN_ON(kproc_by_task(proc->leader)->proc);

		put_task_struct(proc->leader);
		kfree(proc);
	}
}
EXPORT_SYMBOL_GPL(sspt_proc_put);

struct sspt_proc *sspt_proc_by_task(struct task_struct *task)
{
	return kproc_by_task(task->group_leader)->proc;
}
EXPORT_SYMBOL_GPL(sspt_proc_by_task);

struct sspt_proc *sspt_proc_get_by_task(struct task_struct *task)
{
	struct ktd_proc *kproc = kproc_by_task(task->group_leader);
	struct sspt_proc *proc;

	spin_lock(&kproc->lock);
	proc = kproc->proc;
	if (proc)
		sspt_proc_get(proc);
	spin_unlock(&kproc->lock);

	return proc;
}
EXPORT_SYMBOL_GPL(sspt_proc_get_by_task);

/**
 * @brief Call func() on each proc (no lock)
 *
 * @param func Callback
 * @param data Data for callback
 * @return Void
 */
void on_each_proc_no_lock(void (*func)(struct sspt_proc *, void *), void *data)
{
	struct sspt_proc *proc, *tmp;

	list_for_each_entry_safe(proc, tmp, &proc_probes_list, list) {
		func(proc, data);
	}
}

/**
 * @brief Call func() on each proc
 *
 * @param func Callback
 * @param data Data for callback
 * @return Void
 */
void on_each_proc(void (*func)(struct sspt_proc *, void *), void *data)
{
	sspt_proc_read_lock();
	on_each_proc_no_lock(func, data);
	sspt_proc_read_unlock();
}
EXPORT_SYMBOL_GPL(on_each_proc);

/**
 * @brief Get sspt_proc by task or create sspt_proc
 *
 * @param task Pointer on the task_struct struct
 * @param priv Private data
 * @return Pointer on the sspt_proc struct
 */
struct sspt_proc *sspt_proc_get_by_task_or_new(struct task_struct *task)
{
	static DEFINE_MUTEX(local_mutex);
	struct ktd_proc *kproc;
	struct sspt_proc *proc;
	struct task_struct *leader = task->group_leader;

	kproc = kproc_by_task(leader);
	if (kproc->proc)
		goto out;

	proc = sspt_proc_create(leader);

	spin_lock(&kproc->lock);
	if (kproc->proc == NULL) {
		sspt_proc_get(proc);
		kproc->proc = proc;
		proc = NULL;

		sspt_proc_write_lock();
		list_add(&kproc->proc->list, &proc_probes_list);
		sspt_proc_write_unlock();
	}
	spin_unlock(&kproc->lock);

	if (proc)
		sspt_proc_free(proc);

out:
	return kproc->proc;
}

/**
 * @brief Check sspt_proc on empty
 *
 * @return Pointer on the sspt_proc struct
 */
void sspt_proc_check_empty(void)
{
	WARN_ON(!list_empty(&proc_probes_list));
}

static void sspt_proc_add_file(struct sspt_proc *proc, struct sspt_file *file)
{
	down_write(&proc->files.sem);
	list_add(&file->list, &proc->files.head);
	file->proc = proc;
	up_write(&proc->files.sem);
}

/**
 * @brief Get sspt_file from sspt_proc by dentry or new
 *
 * @param proc Pointer on the sspt_proc struct
 * @param dentry Dentry of file
 * @return Pointer on the sspt_file struct
 */
struct sspt_file *sspt_proc_find_file_or_new(struct sspt_proc *proc,
					     struct dentry *dentry)
{
	struct sspt_file *file;

	file = sspt_proc_find_file(proc, dentry);
	if (file == NULL) {
		file = sspt_file_create(dentry, 10);
		if (file)
			sspt_proc_add_file(proc, file);
	}

	return file;
}

/**
 * @brief Get sspt_file from sspt_proc by dentry
 *
 * @param proc Pointer on the sspt_proc struct
 * @param dentry Dentry of file
 * @return Pointer on the sspt_file struct
 */
struct sspt_file *sspt_proc_find_file(struct sspt_proc *proc,
				      struct dentry *dentry)
{
	struct sspt_file *file;

	down_read(&proc->files.sem);
	list_for_each_entry(file, &proc->files.head, list) {
		if (dentry == file->dentry)
			goto unlock;
	}
	file = NULL;

unlock:
	up_read(&proc->files.sem);

	return file;
}

/**
 * @brief Install probes on the page to monitored process
 *
 * @param proc Pointer on the sspt_proc struct
 * @param page_addr Page address
 * @return Void
 */
void sspt_proc_install_page(struct sspt_proc *proc, unsigned long page_addr)
{
	struct mm_struct *mm = proc->leader->mm;
	struct vm_area_struct *vma;

	vma = find_vma_intersection(mm, page_addr, page_addr + 1);
	if (vma && check_vma(vma)) {
		struct dentry *dentry = vma->vm_file->f_path.dentry;
		struct sspt_file *file = sspt_proc_find_file(proc, dentry);
		if (file) {
			sspt_file_set_mapping(file, vma);
			sspt_file_install(file);
		}
	}
}

/**
 * @brief Install probes to monitored process
 *
 * @param proc Pointer on the sspt_proc struct
 * @return Void
 */
void sspt_proc_install(struct sspt_proc *proc)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = proc->leader->mm;

	proc->first_install = 1;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (check_vma(vma)) {
			struct dentry *dentry = vma->vm_file->f_path.dentry;
			struct sspt_file *file =
				sspt_proc_find_file(proc, dentry);
			if (file) {
				sspt_file_set_mapping(file, vma);
				sspt_file_install(file);
			}
		}
	}
}

/**
 * @brief Uninstall probes to monitored process
 *
 * @param proc Pointer on the sspt_proc struct
 * @param task Pointer on the task_struct struct
 * @param flag Action for probes
 * @return Error code
 */
int sspt_proc_uninstall(struct sspt_proc *proc,
			struct task_struct *task,
			enum US_FLAGS flag)
{
	int err = 0;
	struct sspt_file *file;

	down_read(&proc->files.sem);
	list_for_each_entry(file, &proc->files.head, list) {
		err = sspt_file_uninstall(file, task, flag);
		if (err != 0) {
			printk(KERN_INFO "ERROR sspt_proc_uninstall: err=%d\n",
			       err);
			break;
		}
	}
	up_read(&proc->files.sem);

	return err;
}

static int intersection(unsigned long start_a, unsigned long end_a,
			unsigned long start_b, unsigned long end_b)
{
	return start_a < start_b ?
			end_a > start_b :
			start_a < end_b;
}

/**
 * @brief Get sspt_file list by region (remove sspt_file from sspt_proc list)
 *
 * @param proc Pointer on the sspt_proc struct
 * @param head[out] Pointer on the head list
 * @param start Region start
 * @param len Region length
 * @return Error code
 */
int sspt_proc_get_files_by_region(struct sspt_proc *proc,
				  struct list_head *head,
				  unsigned long start, unsigned long end)
{
	int ret = 0;
	struct sspt_file *file, *n;

	down_write(&proc->files.sem);
	list_for_each_entry_safe(file, n, &proc->files.head, list) {
		if (intersection(file->vm_start, file->vm_end, start, end)) {
			ret = 1;
			list_move(&file->list, head);
		}
	}
	up_write(&proc->files.sem);

	return ret;
}

/**
 * @brief Insert sspt_file to sspt_proc list
 *
 * @param proc Pointer on the sspt_proc struct
 * @param head Pointer on the head list
 * @return Void
 */
void sspt_proc_insert_files(struct sspt_proc *proc, struct list_head *head)
{
	down_write(&proc->files.sem);
	list_splice(head, &proc->files.head);
	up_write(&proc->files.sem);
}

/**
 * @brief Add sspt_filter to sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @param pfg Pointer to pf_group struct
 * @return Void
 */
void sspt_proc_add_filter(struct sspt_proc *proc, struct pf_group *pfg)
{
	struct sspt_filter *f;

	f = sspt_filter_create(proc, pfg);
	if (f)
		list_add(&f->list, &proc->filters.head);
}

/**
 * @brief Remove sspt_filter from sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @param pfg Pointer to pf_group struct
 * @return Void
 */
void sspt_proc_del_filter(struct sspt_proc *proc, struct pf_group *pfg)
{
	struct sspt_filter *fl, *tmp;

	mutex_lock(&proc->filters.mtx);
	list_for_each_entry_safe(fl, tmp, &proc->filters.head, list) {
		if (fl->pfg == pfg) {
			list_del(&fl->list);
			sspt_filter_free(fl);
		}
	}
	mutex_unlock(&proc->filters.mtx);
}

/**
 * @brief Remove all sspt_filters from sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @return Void
 */
void sspt_proc_del_all_filters(struct sspt_proc *proc)
{
	struct sspt_filter *fl, *tmp;

	mutex_lock(&proc->filters.mtx);
	list_for_each_entry_safe(fl, tmp, &proc->filters.head, list) {
		list_del(&fl->list);
		sspt_filter_free(fl);
	}
	mutex_unlock(&proc->filters.mtx);
}

/**
 * @brief Check if sspt_filter is already in sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @param pfg Pointer to pf_group struct
 * @return Boolean
 */
bool sspt_proc_is_filter_new(struct sspt_proc *proc, struct pf_group *pfg)
{
	struct sspt_filter *fl;

	list_for_each_entry(fl, &proc->filters.head, list)
		if (fl->pfg == pfg)
			return false;

	return true;
}

void sspt_proc_on_each_filter(struct sspt_proc *proc,
			      void (*func)(struct sspt_filter *, void *),
			      void *data)
{
	struct sspt_filter *fl;

	list_for_each_entry(fl, &proc->filters.head, list)
		func(fl, data);
}

void sspt_proc_on_each_ip(struct sspt_proc *proc,
			  void (*func)(struct sspt_ip *, void *), void *data)
{
	struct sspt_file *file;

	down_read(&proc->files.sem);
	list_for_each_entry(file, &proc->files.head, list)
		sspt_file_on_each_ip(file, func, data);
	up_read(&proc->files.sem);
}

static void is_send_event(struct sspt_filter *f, void *data)
{
	bool *is_send = (bool *)data;

	if (!*is_send && f->pfg_is_inst)
		*is_send = !!pfg_msg_cb_get(f->pfg);
}

bool sspt_proc_is_send_event(struct sspt_proc *proc)
{
	bool is_send = false;

	/* FIXME: add read lock (deadlock in sampler) */
	sspt_proc_on_each_filter(proc, is_send_event, (void *)&is_send);

	return is_send;
}


static struct sspt_proc_cb *proc_cb;

int sspt_proc_cb_set(struct sspt_proc_cb *cb)
{
	if (cb && proc_cb)
		return -EBUSY;

	proc_cb = cb;

	return 0;
}
EXPORT_SYMBOL_GPL(sspt_proc_cb_set);

void sspt_proc_priv_create(struct sspt_proc *proc)
{
	if (proc_cb && proc_cb->priv_create)
		proc->private_data = proc_cb->priv_create(proc);
}

void sspt_proc_priv_destroy(struct sspt_proc *proc)
{
	if (proc->first_install && proc_cb && proc_cb->priv_destroy)
		proc_cb->priv_destroy(proc, proc->private_data);
}
