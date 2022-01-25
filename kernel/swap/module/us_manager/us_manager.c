/*
 *  SWAP uprobe manager
 *  modules/us_manager/us_manager.c
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
#include <linux/mutex.h>
#include <linux/stop_machine.h>
#include "pf/pf_group.h"
#include "sspt/sspt_proc.h"
#include "probes/probe_info_new.h"
#include "helper.h"
#include "us_manager.h"
#include "debugfs_us_manager.h"
#include "callbacks.h"
#include <writer/event_filter.h>
#include <master/swap_initializer.h>


static DEFINE_MUTEX(mutex_inst);
static enum status_type status = ST_OFF;


static int __do_usm_stop(void *data)
{
	get_all_procs();

	return 0;
}

static void do_usm_stop(void)
{
	int ret;

	exec_cbs(STOP_CB);
	helper_unreg_top();

	ret = stop_machine(__do_usm_stop, NULL, NULL);
	if (ret)
		printk("do_usm_stop failed: %d\n", ret);

	uninstall_all();
	helper_unreg_bottom();
	sspt_proc_check_empty();
}

static int do_usm_start(void)
{
	int ret;

	ret = helper_reg();
	if (ret)
		return ret;

	install_all();

	exec_cbs(START_CB);

	return 0;
}

/**
 * @brief Get instrumentation status
 *
 * @return Instrumentation status
 */
enum status_type usm_get_status(void)
{
	mutex_lock(&mutex_inst);
	return status;
}
EXPORT_SYMBOL_GPL(usm_get_status);

/**
 * @brief Put instrumentation status
 *
 * @param st Instrumentation status
 * @return Void
 */
void usm_put_status(enum status_type st)
{
	status = st;
	mutex_unlock(&mutex_inst);
}
EXPORT_SYMBOL_GPL(usm_put_status);

/**
 * @brief Stop instrumentation
 *
 * @return Error code
 */
int usm_stop(void)
{
	int ret = 0;

	if (usm_get_status() == ST_OFF) {
		printk(KERN_INFO "US instrumentation is not running!\n");
		ret = -EINVAL;
		goto put;
	}

	do_usm_stop();

put:
	usm_put_status(ST_OFF);

	return ret;
}
EXPORT_SYMBOL_GPL(usm_stop);

/**
 * @brief Start instrumentation
 *
 * @return Error code
 */
int usm_start(void)
{
	int ret = -EINVAL;
	enum status_type st;

	st = usm_get_status();
	if (st == ST_ON) {
		printk(KERN_INFO "US instrumentation is already run!\n");
		goto put;
	}

	ret = do_usm_start();
	if (ret == 0)
		st = ST_ON;

put:
	usm_put_status(st);

	return ret;
}
EXPORT_SYMBOL_GPL(usm_start);





/* ============================================================================
 * ===                                QUIET                                 ===
 * ============================================================================
 */
static enum quiet_type quiet = QT_ON;

/**
 * @brief Set quiet mode
 *
 * @param q Quiet mode
 * @return Void
 */
void set_quiet(enum quiet_type q)
{
	quiet = q;
}
EXPORT_SYMBOL_GPL(set_quiet);

/**
 * @brief Get quiet mode
 *
 * @return Quiet mode
 */
enum quiet_type get_quiet(void)
{
	return quiet;
}
EXPORT_SYMBOL_GPL(get_quiet);





/* ============================================================================
 * ===                              US_FILTER                               ===
 * ============================================================================
 */
static int us_filter(struct task_struct *task)
{
	struct sspt_proc *proc;

	/* FIXME: add read lock (deadlock in sampler) */
	proc = sspt_proc_by_task(task);
	if (proc)
		return sspt_proc_is_send_event(proc);

	return 0;
}

static struct ev_filter ev_us_filter = {
	.name = "traced_process_only",
	.filter = us_filter
};

static int init_us_filter(void)
{
	int ret;

	ret = event_filter_register(&ev_us_filter);
	if (ret)
		return ret;

	return event_filter_set(ev_us_filter.name);
}

static void exit_us_filter(void)
{
	event_filter_unregister(&ev_us_filter);
}





static int usm_once(void)
{
	int ret;

	ret = helper_once();

	return ret;
}

static int init_us_manager(void)
{
	int ret;

	ret = helper_init();
	if (ret)
		return ret;

	ret = sspt_proc_init();
	if (ret)
		goto uninit_helper;

	ret = pin_init();
	if (ret)
		goto uninit_proc;

	ret = init_us_filter();
	if (ret)
		goto uninit_pin;

	return 0;

uninit_pin:
	pin_exit();
uninit_proc:
	sspt_proc_uninit();
uninit_helper:
	helper_uninit();

	return ret;
}

static void exit_us_manager(void)
{
	if (status == ST_ON)
		BUG_ON(usm_stop());

	remove_all_cbs();

	exit_us_filter();
	pin_exit();
	sspt_proc_uninit();
	helper_uninit();

	WARN_ON(!pfg_is_unloadable());
}

SWAP_LIGHT_INIT_MODULE(usm_once, init_us_manager, exit_us_manager,
		       init_debugfs_us_manager, exit_debugfs_us_manager);

MODULE_LICENSE("GPL");
