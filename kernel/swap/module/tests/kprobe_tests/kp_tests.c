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
 * 2016         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <kprobe/swap_kprobes.h>
#include "kp_module.h"


static struct task_struct *cur_task;


static struct kprobe *kp_create(char *name,
				int (*pre_h)(struct kprobe *, struct pt_regs *))
{
	struct kprobe *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (p) {
		p->symbol_name = name;
		p->pre_handler = pre_h;
	}

	return p;
}

static void kp_free(struct kprobe *p)
{
	memset(p, 0x10, sizeof(*p));
}

#define kp_reg(ptr, name, handler) \
	do { \
		ptr = kp_create(name, handler); \
		swap_register_kprobe(ptr); \
	} while (0)

#define kp_unreg(ptr) \
	do { \
		swap_unregister_kprobe(ptr); \
		kp_free(ptr); \
		ptr = NULL; \
	} while (0)


noinline char *my_kstrdup(const char *s, gfp_t gfp)
{
	return kstrdup(s, gfp);
}

noinline void my_kfree(const void *data)
{
	kfree(data);
}




/*
 ******************************************************************************
 *                                 recursion                                  *
 ******************************************************************************
 */
static int kstrdup_cnt;
static int kfree_cnt;

static struct kprobe *kp_kstrdup;
static int kstrdup_h(struct kprobe *kp, struct pt_regs *regs)
{
	char *str;

	str = my_kstrdup("from_kfree_h", GFP_ATOMIC);
	my_kfree(str);

	++kstrdup_cnt;

	return 0;
}

static struct kprobe *kp_kfree;
static int kfree_h(struct kprobe *kp, struct pt_regs *regs)
{
	++kfree_cnt;

	return 0;
}

static void run_test_recursion(void)
{
	char *str;

	str = my_kstrdup("test_string_0", GFP_KERNEL);
	my_kfree(str);

	str = my_kstrdup("test_string_1", GFP_KERNEL);
	my_kfree(str);
}

static void do_test_recursion(void)
{
	kp_reg(kp_kfree, "my_kfree", kfree_h);
	kp_reg(kp_kstrdup, "my_kstrdup", kstrdup_h);

	run_test_recursion();

	kp_unreg(kp_kstrdup);
	kp_unreg(kp_kfree);
}


static void test_recursion(void)
{
	olog("Recursion:\n");

	kstrdup_cnt = 0;
	kfree_cnt = 0;

	do_test_recursion();

	if (kstrdup_cnt == 2 && kfree_cnt == 2) {
		olog("    OK\n");
	} else {
		olog("    ERROR: kstrdup_cnt=%d kfree_cnt=%d\n",
		       kstrdup_cnt, kfree_cnt);
	}
}




/*
 ******************************************************************************
 *            recursion and multiple handlers (Aggregate probe)               *
 ******************************************************************************
 */
static int kfree2_cnt;

static struct kprobe *kp_kfree2;
static int kfree2_h(struct kprobe *kp, struct pt_regs *regs)
{
	if (current != cur_task || in_interrupt())
		return 0;

	++kfree2_cnt;
	return 0;
}

static void pre_test_recursion_and_mh(void)
{
	kstrdup_cnt = 0;
	kfree_cnt = 0;
	kfree2_cnt = 0;
}

static void post_test_recursion_and_mh(void)
{
	if (kstrdup_cnt == 2 && kfree_cnt == 2 && kfree2_cnt == 2) {
		olog("    OK\n");
	} else {
		olog("    ERROR: kstrdup_cnt=%d kfree_cnt=%d kfree2_cnt=%d\n",
		     kstrdup_cnt, kfree_cnt, kfree2_cnt);
	}
}

static void test_recursion_and_multiple_handlers(void)
{
	olog("Recursion and multiple handlers:\n");

	pre_test_recursion_and_mh();

	kp_reg(kp_kfree2, "my_kfree", kfree2_h);
	do_test_recursion();
	kp_unreg(kp_kfree2);

	post_test_recursion_and_mh();
}

static void test_recursion_and_multiple_handlers2(void)
{
	olog("Recursion and multiple handlers [II]:\n");

	pre_test_recursion_and_mh();

	kp_reg(kp_kfree, "my_kfree", kfree_h);
	kp_reg(kp_kstrdup, "my_kstrdup", kstrdup_h);
	kp_reg(kp_kfree2, "my_kfree", kfree2_h);

	run_test_recursion();

	kp_unreg(kp_kstrdup);
	kp_unreg(kp_kfree);
	kp_unreg(kp_kfree2);

	post_test_recursion_and_mh();
}




/*
 ******************************************************************************
 *                        swap_unregister_kprobe(), sync                      *
 ******************************************************************************
 */

static const char task_name[] = "my_task";

static int is_my_task(void)
{
	return !strcmp(task_name, current->comm);
}

static int find_module_cnt;

static struct kprobe *kp_find_module;
static int find_module_h(struct kprobe *kp, struct pt_regs *regs)
{
	if (is_my_task()) {
		might_sleep();
		++find_module_cnt;

		/* sleep 0.5 sec */
		msleep(500);
		schedule();

		++find_module_cnt;
	}

	return 0;
}

static int kthread_my_fn(void *data)
{
	find_module("o_lo_lo");
	find_module("o_lo_lo");

	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}


static void do_test_sync_unreg(unsigned int ms)
{
	struct task_struct *task;

	kp_reg(kp_find_module, "find_module", find_module_h);

	task = kthread_run(kthread_my_fn, NULL, task_name);
	if (IS_ERR(task)) {
		olog("ERROR: kthread_run()\n");
		goto unreg;
	}

	/* waiting for kthread_my_fn() call */
	msleep(ms);
unreg:
	kp_unreg(kp_find_module);
	if (!IS_ERR(task))
		kthread_stop(task);
}

static void test_sync_unreg(void)
{
	olog("Unreg kp:\n");

	find_module_cnt = 0;

	do_test_sync_unreg(200);

	if (find_module_cnt == 2) {
		olog("    OK\n");
	} else {
		olog("    ERROR: find_module_cnt=%d\n", find_module_cnt);
	}
}



/*
 ******************************************************************************
 *             swap_unregister_kprobe(), sync and multiple handlers           *
 ******************************************************************************
 */
static int find_module2_cnt;

static struct kprobe *kp_find_module2;
static int find_module2_h(struct kprobe *kp, struct pt_regs *regs)
{
	if (is_my_task()) {
		++find_module2_cnt;

		/* sleep 0.5 sec */
		msleep(500);

		++find_module2_cnt;
	}

	return 0;
}

static void pre_test_sync_unreg_and_mh(void)
{
	find_module_cnt = 0;
	find_module2_cnt = 0;
}

static void post_test_sync_unreg_and_mh(int cnt, int cnt2)
{
	if (find_module_cnt == cnt && find_module2_cnt == cnt2) {
		olog("    OK\n");
	} else {
		olog("    ERROR: find_module_cnt=%d find_module2_cnt=%d\n",
		     find_module_cnt, find_module2_cnt);
	}
}

static void do_test_sync_unreg_and_mh(unsigned int ms)
{
	struct task_struct *task;

	kp_reg(kp_find_module, "find_module", find_module_h);
	kp_reg(kp_find_module2, "find_module", find_module2_h);

	task = kthread_run(kthread_my_fn, NULL, task_name);
	if (IS_ERR(task)) {
		olog("ERROR: kthread_run()\n");
		goto unreg;
	}

	/* waiting for kthread_my_fn() call */
	msleep(ms);
unreg:
	kp_unreg(kp_find_module2);
	kp_unreg(kp_find_module);
	if (!IS_ERR(task))
		kthread_stop(task);
}

static void test_sync_unreg_and_multiple_handlers(void)
{
	olog("Unreg kp and multiple handlers:\n");

	pre_test_sync_unreg_and_mh();

	do_test_sync_unreg_and_mh(700);

	post_test_sync_unreg_and_mh(2, 2);
}

static void do_test_sync_unreg_and_mh2(unsigned int ms)
{
	struct task_struct *task;

	kp_reg(kp_find_module, "find_module", find_module_h);
	kp_reg(kp_find_module2, "find_module", find_module2_h);

	task = kthread_run(kthread_my_fn, NULL, task_name);
	if (IS_ERR(task)) {
		olog("ERROR: kthread_run()\n");
		goto unreg;
	}

	/* waiting for kthread_my_fn() call */
	msleep(ms);
unreg:
	kp_unreg(kp_find_module);
	kp_unreg(kp_find_module2);
	if (!IS_ERR(task))
		kthread_stop(task);
}

static void test_sync_unreg_and_multiple_handlers2(void)
{
	olog("Unreg kp and multiple handlers [II]:\n");

	pre_test_sync_unreg_and_mh();

	do_test_sync_unreg_and_mh2(700);

	post_test_sync_unreg_and_mh(2, 2);
}

int kp_tests_run(void)
{
	cur_task = current;

	test_recursion();
	test_recursion_and_multiple_handlers();
	test_recursion_and_multiple_handlers2();
	// add 3

	test_sync_unreg();
	test_sync_unreg_and_multiple_handlers();
	test_sync_unreg_and_multiple_handlers2();

	return 0;
}
