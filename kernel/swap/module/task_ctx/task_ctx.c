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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/sched.h>
#include <linux/module.h>
#include <linux/kconfig.h>
#include <linux/completion.h>
#include <ksyms/ksyms.h>
#include <kprobe/swap_ktd.h>
#include <master/swap_initializer.h>
#include "task_ctx.h"


enum { WAIT_TIMEOUT = 1500 };	/* max waiting time the signal delivery */

struct call_task {
	taskctx_t func;
	void *data;

	bool is_running;
	struct completion comp0;
	struct completion comp1;
};


static void (*swap_signal_wake_up_state)(struct task_struct *t,
					 unsigned int state);

static struct sighand_struct *swap_lock_task_sighand(struct task_struct *tsk,
						     unsigned long *flags)
{
	struct sighand_struct *sighand;

	for (;;) {
		local_irq_save(*flags);
		rcu_read_lock();

		sighand = rcu_dereference(tsk->sighand);
		if (unlikely(sighand == NULL)) {
			rcu_read_unlock();
			local_irq_restore(*flags);
			break;
		}

		spin_lock(&sighand->siglock);
		if (likely(sighand == tsk->sighand)) {
			rcu_read_unlock();
			break;
		}
		spin_unlock(&sighand->siglock);

		rcu_read_unlock();
		local_irq_restore(*flags);
	}

	return sighand;
}

static inline void swap_unlock_task_sighand(struct task_struct *tsk,
					    unsigned long *flags)
{
        spin_unlock_irqrestore(&tsk->sighand->siglock, *flags);
}

static int fake_signal_wake_up(struct task_struct *p)
{
	unsigned long flags;

	if (swap_lock_task_sighand(p, &flags) == NULL)
		return -ESRCH;

	swap_signal_wake_up_state(p, 0);
	swap_unlock_task_sighand(p, &flags);

	return 0;
}


static void ktd_init(struct task_struct *task, void *data)
{
	struct call_task **call = (struct call_task **)data;

	*call = NULL;
}

static void ktd_exit(struct task_struct *task, void *data)
{
	struct call_task **call = (struct call_task **)data;

	WARN(*call, "call is not NULL");
}

static struct ktask_data ktd = {
	.init = ktd_init,
	.exit = ktd_exit,
	.size = sizeof(struct call_task *),
};

static void call_set(struct task_struct *task, struct call_task *call)
{
	*(struct call_task **)swap_ktd(&ktd, task) = call;
}

static struct call_task *call_get(struct task_struct *task)
{
	return *(struct call_task **)swap_ktd(&ktd, task);
}


int taskctx_run(struct task_struct *task, taskctx_t func, void *data)
{
	if (IS_ENABLED(CONFIG_SWAP_KERNEL_IMMUTABLE))
		return -ENOSYS;

	if (task == current) {
		func(data);
	} else {
		int ret;
		unsigned long jiff;
		struct call_task call = {
			.func = func,
			.data = data,
			.comp0 = COMPLETION_INITIALIZER(call.comp0),
			.comp1 = COMPLETION_INITIALIZER(call.comp1),
			.is_running = false,
		};

		/* check task possibility to receive signals */
		if (task->flags & (PF_KTHREAD | PF_EXITING | PF_SIGNALED))
			return -EINVAL;

		/* set call by task */
		call_set(task, &call);

		ret = fake_signal_wake_up(task);
		if (ret) {
			/* reset call by task */
			call_set(task, NULL);
			pr_err("cannot send signal to task[%u %u %s] flags=%08x state=%08lx\n",
			       task->tgid, task->pid, task->comm,
			       task->flags, task->state);
			return ret;
		}

		jiff = msecs_to_jiffies(WAIT_TIMEOUT);
		wait_for_completion_timeout(&call.comp0, jiff);

		/* reset call by task */
		call_set(task, NULL);

		/* wait the return from sig_pre_handler() */
		synchronize_sched();

		if (call.is_running)
			wait_for_completion(&call.comp1);

	}

	return 0;
}
EXPORT_SYMBOL_GPL(taskctx_run);


static void sig_handler(void)
{
	struct call_task *call = call_get(current);

	if (call) {
		call_set(current, NULL);
		call->is_running = true;

		complete(&call->comp0);
		call->func(call->data);
		complete(&call->comp1);
	}
}


#ifdef CONFIG_SWAP_HOOK_SIGNAL
# include <swap/hook_signal.h>
void hook_handler(struct ksignal *ksig)
{
	sig_handler();
}

struct hook_signal hook_signal = {
	.owner = THIS_MODULE,
	.hook = hook_handler,
};

static int signal_reg(void)
{
	int ret = hook_signal_reg(&hook_signal);
	if (ret)
		pr_err("Cannot register hook_signal, ret=%d\n", ret);

	return ret;
}

static void signal_unreg(void)
{
	hook_signal_unreg(&hook_signal);
}

static int signal_once(void)
{
	return 0;
}

#else /* !CONFIG_SWAP_HOOK_SIGNAL */
# include <kprobe/swap_kprobes.h>

static int sig_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	sig_handler();

	return 0;
}

static struct kprobe sig_kprobe = {
	.pre_handler = sig_pre_handler,
};

static int signal_reg(void)
{
	int ret = swap_register_kprobe(&sig_kprobe);
	if (ret)
		pr_err("register sig_kprobe ret=%d\n", ret);

	return ret;
}

static void signal_unreg(void)
{
	swap_unregister_kprobe(&sig_kprobe);
}

static int signal_once(void)
{
	const char *sym;

	sym = "signal_wake_up_state";
	swap_signal_wake_up_state = (void *)swap_ksyms(sym);
	if (swap_signal_wake_up_state == NULL)
		goto not_found;

	sym = "get_signal";
	sig_kprobe.addr = swap_ksyms(sym);
	if (sig_kprobe.addr == 0)
		goto not_found;

	return 0;

not_found:
	printk("Cannot find address for '%s'!\n", sym);
	return -ESRCH;
}
#endif /* CONFIG_SWAP_HOOK_SIGNAL */


static int use_cnt = 0;
static DEFINE_MUTEX(use_lock);

int taskctx_get(void)
{
	int ret = 0;

	mutex_lock(&use_lock);
	if (use_cnt == 0) {
		ret = signal_reg();
		if (ret)
			goto unlock;
	}

	++use_cnt;

unlock:
	mutex_unlock(&use_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(taskctx_get);

void taskctx_put(void)
{
	mutex_lock(&use_lock);
	if (use_cnt == 0) {
		WARN_ON("call put_task_context() without get_task_context");
		goto unlock;
	}

	--use_cnt;
	if (use_cnt == 0)
		signal_unreg();

unlock:
	mutex_unlock(&use_lock);
}
EXPORT_SYMBOL_GPL(taskctx_put);


static int taskctx_once(void)
{
	return signal_once();
}

static int taskctx_init(void)
{
	return swap_ktd_reg(&ktd);
}

static void taskctx_uninit(void)
{
	WARN(use_cnt, "use_cnt=%d\n", use_cnt);
	swap_ktd_unreg(&ktd);
}

SWAP_LIGHT_INIT_MODULE(taskctx_once, taskctx_init, taskctx_uninit, NULL, NULL);

MODULE_LICENSE("GPL");
