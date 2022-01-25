#ifndef _LINUX_SWAP_HOOK_TASKDATA_H
#define _LINUX_SWAP_HOOK_TASKDATA_H


#ifdef CONFIG_SWAP_HOOK_TASKDATA

#include <linux/list.h>
#include <linux/compiler.h>

struct module;
struct task_struct;

struct hook_taskdata {
	struct hlist_node node;
	struct module *owner;
	void (*put_task)(struct task_struct *task);
};

int hook_taskdata_reg(struct hook_taskdata *hook);
void hook_taskdata_unreg(struct hook_taskdata *hook);

/* private interface */
extern int hook_taskdata_counter;
void hook_taskdata_put_task(struct task_struct *task);

static inline void swap_taskdata_put_task(struct task_struct *task)
{
	if (unlikely(hook_taskdata_counter))
		hook_taskdata_put_task(task);
}

#else /* CONFIG_SWAP_HOOK_TASKDATA */

static inline void swap_taskdata_put_task(struct task_struct *task) {}

#endif /* CONFIG_SWAP_HOOK_TASKDATA */


#endif /* _LINUX_SWAP_HOOK_TASKDATA_H */
