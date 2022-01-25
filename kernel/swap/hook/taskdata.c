#include <linux/errno.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <swap/hook_taskdata.h>


static HLIST_HEAD(td_head);
static DEFINE_SPINLOCK(td_lock);
int hook_taskdata_counter;

int hook_taskdata_reg(struct hook_taskdata *hook)
{
	if (!try_module_get(hook->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hook->node);

	spin_lock(&td_lock);
	hlist_add_head(&hook->node, &td_head);
	++hook_taskdata_counter;
	spin_unlock(&td_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(hook_taskdata_reg);

void hook_taskdata_unreg(struct hook_taskdata *hook)
{
	spin_lock(&td_lock);
	--hook_taskdata_counter;
	hlist_del(&hook->node);
	spin_unlock(&td_lock);

	module_put(hook->owner);
}
EXPORT_SYMBOL_GPL(hook_taskdata_unreg);

void hook_taskdata_put_task(struct task_struct *task)
{
	spin_lock(&td_lock);
	if (hook_taskdata_counter) {
		struct hook_taskdata *hook;

		hlist_for_each_entry(hook, &td_head, node)
			hook->put_task(task);
	}
	spin_unlock(&td_lock);
}
