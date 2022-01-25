#include <linux/rwsem.h>
#include <linux/module.h>
#include <swap/hook_usaux.h>


struct hook_usaux *hook_usaux_user;
static DECLARE_RWSEM(hook_sem);

struct hook_usaux *swap_hook_usaux_get(void)
{
	struct hook_usaux *hook = NULL;

	down_read(&hook_sem);
	if (hook_usaux_user) {
		hook = hook_usaux_user;
		__module_get(hook->owner);
	} else {
		up_read(&hook_sem);
	}

	return hook;
}
void swap_hook_usaux_put(struct hook_usaux *hook)
{
	module_put(hook->owner);
	up_read(&hook_sem);
}

int hook_usaux_set(struct hook_usaux *hook)
{
	int ret = 0;

	down_write(&hook_sem);
	if (hook_usaux_user) {
		ret = -EBUSY;
		goto unlock;
	}

	if (!try_module_get(hook->owner)) {
		ret = -ENODEV;
		goto unlock;
	}

	hook_usaux_user = hook;

unlock:
	up_write(&hook_sem);

	return ret;
}
EXPORT_SYMBOL_GPL(hook_usaux_set);

void hook_usaux_reset(void)
{
	down_write(&hook_sem);
	if (hook_usaux_user)
		module_put(hook_usaux_user->owner);
	hook_usaux_user = NULL;
	up_write(&hook_sem);
}
EXPORT_SYMBOL_GPL(hook_usaux_reset);
