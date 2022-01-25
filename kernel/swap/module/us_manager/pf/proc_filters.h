/**
 * @file us_manager/pf/proc_filters.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * Copyright (C) Samsung Electronics, 2013
 */


#ifndef _PROC_FILTERS_H
#define _PROC_FILTERS_H

#include <linux/types.h>

struct dentry;
struct task_struct;

/**
 * @struct proc_filter
 * @breaf Filter for process
 */
struct proc_filter {
	/** Callback for filtering */
	struct task_struct *(*call)(struct proc_filter *self,
				    struct task_struct *task);
	void *data;		/**< Data of callback */
	void *priv;		/**< Private data */
};

/**
 * @def check_task_f @hideinitializer
 * Call filter on the task
 *
 * @param filter Pointer to the proc_filter struct
 * @param task Pointer to the task_struct struct
 */
#define check_task_f(filter, task) ((filter)->call(filter, task))

void set_pf_by_dentry(struct proc_filter *pf, struct dentry *dentry,
		      void *priv);
void set_pf_by_tgid(struct proc_filter *pf, pid_t tgid, void *priv);
int set_pf_by_comm(struct proc_filter *pf, char *comm, void *priv);
void set_pf_dumb(struct proc_filter *pf, void *priv);


int check_pf_by_dentry(struct proc_filter *filter, struct dentry *dentry);
int check_pf_by_tgid(struct proc_filter *filter, pid_t tgid);
int check_pf_by_comm(struct proc_filter *filter, char *comm);
int check_pf_dumb(struct proc_filter *filter);
void *get_pf_priv(struct proc_filter *filter);

void free_pf(struct proc_filter *filter);

int ignore_pf(struct proc_filter *filter);

#endif /* _PROC_FILTERS_H */
