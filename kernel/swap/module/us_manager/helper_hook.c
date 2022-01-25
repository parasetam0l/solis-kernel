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
 *  2016  Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 */


#include <linux/rwsem.h>
#include <swap/hook_usaux.h>
#include <kprobe/swap_ktd.h>


struct hooks_td {
	bool in_copy_process;
};

static void hooks_td_init(struct task_struct *task, void *data)
{
	struct hooks_td *td = (struct hooks_td *)data;

	td->in_copy_process = false;
}

static void hooks_td_exit(struct task_struct *task, void *data)
{
	struct hooks_td *td = (struct hooks_td *)data;

	WARN(td->in_copy_process, "in_copy_process=%d", td->in_copy_process);
}

static struct ktask_data hooks_ktd = {
	.init = hooks_td_init,
	.exit = hooks_td_exit,
	.size = sizeof(struct hooks_td),
};

static struct hooks_td *current_hooks_td(void)
{
	return (struct hooks_td *)swap_ktd(&hooks_ktd, current);
}


static atomic_t run_flag = ATOMIC_INIT(0);

static void hook_start(void)
{
	atomic_set(&run_flag, 1);
}

static void hook_stop(void)
{
	atomic_set(&run_flag, 0);
}

static bool hook_is_running(void)
{
	return atomic_read(&run_flag);
}


static void hook_page_fault(unsigned long addr)
{
	if (hook_is_running())
		hh_page_fault(addr);
}

static DECLARE_RWSEM(copy_process_sem);

static void hook_copy_process_pre(void)
{
	if (hook_is_running()) {
		down_read(&copy_process_sem);
		if (hook_is_running())
			current_hooks_td()->in_copy_process = true;
		else
			up_read(&copy_process_sem);
	}
}

static void hook_copy_process_post(struct task_struct *task)
{
	struct task_struct *parent = current;
	struct hooks_td *td = current_hooks_td();

	if (!td->in_copy_process)
		return;

	if (IS_ERR(task))
		goto out;

	/* check flags CLONE_VM */
	if (task->mm != parent->mm)
		hh_clean_task(current, task);

out:
	up_read(&copy_process_sem);
	td->in_copy_process = false;
}

static void hook_mm_release(struct task_struct *task)
{
	hh_mm_release(task);
}

static void hook_munmap(unsigned long start, unsigned long end)
{
	hh_munmap(start, end);
}

static void hook_mmap(struct file *file, unsigned long addr)
{
	hh_mmap(file, addr);
}

static void hook_set_comm(struct task_struct *task)
{
	if (hook_is_running())
		hh_set_comm(task);
}

static void hook_change_leader(struct task_struct *prev,
				struct task_struct *next)
{
	hh_change_leader(prev, next);
}

static struct hook_usaux hook_usaux = {
	.owner = THIS_MODULE,
	.page_fault = hook_page_fault,
	.copy_process_pre = hook_copy_process_pre,
	.copy_process_post = hook_copy_process_post,
	.mm_release = hook_mm_release,
	.munmap = hook_munmap,
	.mmap = hook_mmap,
	.set_comm = hook_set_comm,
	.change_leader = hook_change_leader,
};

int helper_once(void)
{
	return 0;
}

int helper_init(void)
{
	return swap_ktd_reg(&hooks_ktd);
}

void helper_uninit(void)
{
	swap_ktd_unreg(&hooks_ktd);
}

int helper_reg(void)
{
	int ret;

	ret = hook_usaux_set(&hook_usaux);
	if (ret)
		return ret;

	hook_start();
	return ret;
}

void helper_unreg_top(void)
{
	hook_stop();
}

void helper_unreg_bottom(void)
{
	/* waiting for copy_process() finishing */
	down_write(&copy_process_sem);
	up_write(&copy_process_sem);

	hook_usaux_reset();
}
