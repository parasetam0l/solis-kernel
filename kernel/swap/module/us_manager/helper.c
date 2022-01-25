/*
 *  SWAP uprobe manager
 *  modules/us_manager/helper.c
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


#include "sspt/sspt.h"
#include "sspt/sspt_filter.h"
#include "helper.h"
#include "usm_hook.h"


/* do_page_fault() */
static void hh_page_fault(unsigned long addr)
{
	unsigned long page_addr = addr & PAGE_MASK;

	call_page_fault(current, page_addr);
}


/* copy_process() */
static void disarm_ip(struct sspt_ip *ip, void *data)
{
	struct task_struct *child = (struct task_struct *)data;
	struct uprobe *up;

	up = probe_info_get_uprobe(ip->desc->type, ip);
	if (up)
		disarm_uprobe(up, child);
}

static void hh_clean_task(struct task_struct *parent, struct task_struct *child)
{
	struct sspt_proc *proc;

	proc = sspt_proc_get_by_task(parent);
	if (proc) {
		/* disarm up for child */
		sspt_proc_on_each_ip(proc, disarm_ip, (void *)child);

		/* disarm urp for child */
		swap_uretprobe_free_task(parent, child, false);

		sspt_proc_put(proc);
	}
}


/* mm_release() */
static void hh_mm_release(struct task_struct *task)
{
	struct mm_struct *mm = task->mm;

	if (mm == NULL) {
		pr_err("mm is NULL\n");
		return;
	}

	/* TODO: this lock for synchronizing to disarm urp */
	down_write(&mm->mmap_sem);
	if (task != task->group_leader) {
		struct sspt_proc *proc;

		if (task != current) {
			pr_err("call mm_release in isn't current context\n");
			goto up_mmsem;
		}

		/* if the thread is killed we need to discard pending
		 * uretprobe instances which have not triggered yet */
		proc = sspt_proc_by_task(task);
		if (proc)
			swap_uretprobe_free_task(task, task, true);
	} else {
		call_mm_release(task);
	}

up_mmsem:
	up_write(&mm->mmap_sem);
}


/* do_munmap() */
struct msg_unmap_data {
	unsigned long start;
	unsigned long end;
};

static void msg_unmap(struct sspt_filter *f, void *data)
{
	if (f->pfg_is_inst) {
		struct pfg_msg_cb *cb = pfg_msg_cb_get(f->pfg);

		if (cb && cb->msg_unmap) {
			struct msg_unmap_data *msg_data;

			msg_data = (struct msg_unmap_data *)data;
			cb->msg_unmap(msg_data->start, msg_data->end);
		}
	}
}

static void __remove_unmap_probes(struct sspt_proc *proc,
				  unsigned long start, unsigned long end)
{
	LIST_HEAD(head);

	if (sspt_proc_get_files_by_region(proc, &head, start, end)) {
		struct sspt_file *file, *n;
		struct task_struct *task = proc->leader;

		list_for_each_entry_safe(file, n, &head, list) {
			if (file->vm_start >= end)
				continue;

			if (file->vm_start >= start)
				sspt_file_uninstall(file, task, US_UNINSTALL);
			/* TODO: else: uninstall pages: * start..file->vm_end */
		}

		sspt_proc_insert_files(proc, &head);
	}
}

static void hh_munmap(unsigned long start, unsigned long end)
{
	struct sspt_proc *proc;

	proc = sspt_proc_get_by_task(current);
	if (proc) {
		struct msg_unmap_data msg_data = {
			.start = start,
			.end = end,
		};

		__remove_unmap_probes(proc, start, end);

		/* send unmap region */
		sspt_proc_on_each_filter(proc, msg_unmap, (void *)&msg_data);

		sspt_proc_put(proc);
	}
}


/* do_mmap_pgoff() */
static void msg_map(struct sspt_filter *f, void *data)
{
	if (f->pfg_is_inst) {
		struct pfg_msg_cb *cb = pfg_msg_cb_get(f->pfg);

		if (cb && cb->msg_map)
			cb->msg_map((struct vm_area_struct *)data);
	}
}

static void hh_mmap(struct file *file, unsigned long addr)
{
	struct sspt_proc *proc;
	struct task_struct *task;
	struct vm_area_struct *vma;

	task = current->group_leader;
	if (is_kthread(task))
		return;

	if (IS_ERR_VALUE(addr))
		return;

	proc = sspt_proc_get_by_task(task);
	if (proc == NULL)
		return;

	vma = find_vma_intersection(task->mm, addr, addr + 1);
	if (vma && check_vma(vma)) {
		usm_hook_mmap(proc, vma);
		sspt_proc_on_each_filter(proc, msg_map, (void *)vma);
	}

	sspt_proc_put(proc);
}


/* set_task_comm() */
static void hh_set_comm(struct task_struct *task)
{
	if (task == current)
		check_task_and_install(current);
}


/* release_task() */
static void hh_change_leader(struct task_struct *prev,
			     struct task_struct *next)
{
	sspt_change_leader(prev, next);
}


#ifdef CONFIG_SWAP_HOOK_USAUX
# include "helper_hook.c"
#else /* CONFIG_SWAP_HOOK_USAUX */
# include "helper_kprobe.c"
#endif /* CONFIG_SWAP_HOOK_USAUX */
