#include <linux/rwsem.h>
#include <linux/module.h>
#include <swap/hook_energy.h>
#include <linux/list.h>


struct swap_hook_energy *swap_nrg_hook;
static DECLARE_RWSEM(energy_hook_sem);

struct swap_hook_energy *swap_hook_energy_get(void)
{
	struct swap_hook_energy *hook = NULL;
	down_read(&energy_hook_sem);
	if (swap_nrg_hook)
		hook = swap_nrg_hook;
	else
		up_read(&energy_hook_sem);

	return hook;
}
void swap_hook_energy_put(void)
{
	up_read(&energy_hook_sem);
}

int swap_hook_energy_set(struct swap_hook_energy *hook)
{
	int ret = 0;

	down_write(&energy_hook_sem);
	if (swap_nrg_hook) {
		ret = -EBUSY;
		goto unlock;
	}

	swap_nrg_hook = hook;

unlock:
	up_write(&energy_hook_sem);
	return ret;
}
EXPORT_SYMBOL_GPL(swap_hook_energy_set);

void swap_hook_energy_unset(void)
{
	down_write(&energy_hook_sem);
	swap_nrg_hook = NULL;
	up_write(&energy_hook_sem);
}
EXPORT_SYMBOL_GPL(swap_hook_energy_unset);
