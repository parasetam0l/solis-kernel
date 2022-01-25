/*
 *  SWAP uprobe manager
 *  modules/us_manager/pf/proc_filters.c
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


#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mm_types.h>
#include <linux/string.h>
#include <linux/fs.h>
#include "proc_filters.h"
#include <us_manager/sspt/sspt.h>


#define VOIDP2PID(x)	((pid_t)(unsigned long)(x))
#define PID2VOIDP(x)	((void *)(unsigned long)(x))

static int check_dentry(struct task_struct *task, struct dentry *dentry)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm = task->mm;

	if (mm == NULL)
		return 0;

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (check_vma(vma) && vma->vm_file->f_path.dentry == dentry)
			return 1;
	}

	return 0;
}

static struct task_struct *call_by_dentry(struct proc_filter *self,
					 struct task_struct *task)
{
	struct dentry *dentry = (struct dentry *)self->data;

	if (!dentry || check_dentry(task, dentry))
		return task;

	return NULL;
}

static inline void free_by_dentry(struct proc_filter *self)
{
	return;
}

static struct task_struct *call_by_tgid(struct proc_filter *self,
				       struct task_struct *task)
{
	pid_t tgid = VOIDP2PID(self->data);

	if (task->tgid == tgid)
		return task;

	return NULL;
}

static inline void free_by_tgid(struct proc_filter *self)
{
	return;
}

static struct task_struct *call_by_comm(struct proc_filter *self,
				       struct task_struct *task)
{
	struct task_struct *parent;
	char *comm = (char *)self->data;
	size_t len = strnlen(comm, TASK_COMM_LEN);

	if (!strncmp(comm, task->group_leader->comm, len))
		return task;

	parent = task->parent;
	if (parent && !strncmp(comm, parent->comm, len))
		return task;

	return NULL;
}

static inline void free_by_comm(struct proc_filter *self)
{
	kfree(self->data);
}

/* Dumb call. Each task is exactly what we are looking for :) */
static struct task_struct *call_dumb(struct proc_filter *self,
				     struct task_struct *task)
{
	return task;
}

/**
 * @brief Filling pf_group struct by dentry
 *
 * @param pf Pointer to the proc_filter struct
 * @param dentry Dentry
 * @param priv Private data
 * @return Void
 */
void set_pf_by_dentry(struct proc_filter *pf, struct dentry *dentry, void *priv)
{
	pf->call = &call_by_dentry;
	pf->data = (void *)dentry;
	pf->priv = priv;
}

/**
 * @brief Filling pf_group struct by TGID
 *
 * @param pf Pointer to the proc_filter struct
 * @param tgid Thread group ID
 * @param priv Private data
 * @return Void
 */
void set_pf_by_tgid(struct proc_filter *pf, pid_t tgid, void *priv)
{
	pf->call = &call_by_tgid;
	pf->data = PID2VOIDP(tgid);
	pf->priv = priv;
}

/**
 * @brief Fill proc_filter struct for given comm
 *
 * @param pf Pointer to the proc_filter struct
 * @param comm Task comm
 * @param priv Private data
 * @return 0 on suceess, error code on error.
 */
int set_pf_by_comm(struct proc_filter *pf, char *comm, void *priv)
{
	size_t len = strnlen(comm, TASK_COMM_LEN);
	char *new_comm = kmalloc(len, GFP_KERNEL);

	if (new_comm == NULL)
		return -ENOMEM;

	/* copy comm */
	memcpy(new_comm, comm, len - 1);
	new_comm[len - 1] = '\0';

	pf->call = &call_by_comm;
	pf->data = new_comm;
	pf->priv = priv;

	return 0;
}

/**
 * @brief Filling pf_group struct for each process
 *
 * @param pf Pointer to the proc_filter struct
 * @param priv Private data
 * @return Void
 */
void set_pf_dumb(struct proc_filter *pf, void *priv)
{
	pf->call = &call_dumb;
	pf->data = NULL;
	pf->priv = priv;
}

/**
 * @brief Free proc_filter struct
 *
 * @param filter Pointer to the proc_filter struct
 * @return Void
 */
void free_pf(struct proc_filter *filter)
{
	if (filter->call == &call_by_dentry)
		free_by_dentry(filter);
	else if (filter->call == &call_by_tgid)
		free_by_tgid(filter);
	else if (filter->call == &call_by_comm)
		free_by_comm(filter);
}

/**
 * @brief Check pf_group struct by dentry
 *
 * @param filter Pointer to the proc_filter struct
 * @param dentry Dentry
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int check_pf_by_dentry(struct proc_filter *filter, struct dentry *dentry)
{
	return filter->data == (void *)dentry &&
	       filter->call == &call_by_dentry;
}

/**
 * @brief Check pf_group struct by TGID
 *
 * @param filter Pointer to the proc_filter struct
 * @param tgid Thread group ID
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int check_pf_by_tgid(struct proc_filter *filter, pid_t tgid)
{
	return filter->data == PID2VOIDP(tgid)
	    && filter->call == &call_by_tgid;
}

/**
 * @brief Check proc_filter struct by comm
 *
 * @param filter Pointer to the proc_filter struct
 * @param comm Task comm
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int check_pf_by_comm(struct proc_filter *filter, char *comm)
{
	return ((filter->call == &call_by_comm) && (filter->data != NULL) &&
		(!strncmp(filter->data, comm, TASK_COMM_LEN - 1)));
}

/**
 * @brief Dumb check always true if filter is a dumb one
 *
 * @param filter Pointer to the proc_filter struct
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int check_pf_dumb(struct proc_filter *filter)
{
	return filter->call == &call_dumb;
}

/**
 * @brief Get priv from pf_group struct
 *
 * @param filter Pointer to the proc_filter struct
 * @return Pointer to the priv
 */
void *get_pf_priv(struct proc_filter *filter)
{
	return filter->priv;
}

/* Check function for call_page_fault() and other frequently called
filter-check functions. It is used to call event-oriented and long-term filters
only on specified events, but not every time memory map is changed. When
iteraiting over the filters list, call this function on each step passing here
pointer on filter. If it returns 1 then the filter should not be called. */
int ignore_pf(struct proc_filter *filter)
{
	return filter->call == &call_by_comm;
}
