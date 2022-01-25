#include <linux/errno.h>
#include <linux/module.h>
#include <linux/rwsem.h>
#include <swap/hook_signal.h>


static HLIST_HEAD(hook_head);
static DECLARE_RWSEM(hook_sem);
int __hook_signal_counter;

int hook_signal_reg(struct hook_signal *hook)
{
	if (!try_module_get(hook->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hook->node);

	down_write(&hook_sem);
	hlist_add_head(&hook->node, &hook_head);
	++__hook_signal_counter;
	up_write(&hook_sem);

	return 0;
}
EXPORT_SYMBOL_GPL(hook_signal_reg);

void hook_signal_unreg(struct hook_signal *hook)
{
	down_write(&hook_sem);
	--__hook_signal_counter;
	hlist_del(&hook->node);
	up_write(&hook_sem);

	module_put(hook->owner);
}
EXPORT_SYMBOL_GPL(hook_signal_unreg);

void __hook_signal(struct ksignal *ksig)
{
	down_read(&hook_sem);
	if (__hook_signal_counter) {
		struct hook_signal *hook;

		hlist_for_each_entry(hook, &hook_head, node)
			hook->hook(ksig);
	}
	up_read(&hook_sem);
}
