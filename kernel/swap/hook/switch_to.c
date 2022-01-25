#include <linux/rwsem.h>
#include <linux/module.h>
#include <linux/list.h>
#include <swap/hook_switch_to.h>


static HLIST_HEAD(ctx_hook_head);
static DEFINE_SPINLOCK(ctx_hook_lock);
int ctx_hook_nr = 0;

static inline void hook_context_get(void)
{
	spin_lock(&ctx_hook_lock);
}
static inline void hook_context_put(void)
{
	spin_unlock(&ctx_hook_lock);
}

void swap_hook_ctx_call(struct task_struct *prev, struct task_struct *next)
{
	struct swap_hook_ctx *tmp;

	hook_context_get();
	hlist_for_each_entry(tmp, &ctx_hook_head, node) {
		tmp->hook(prev, next);
	}
	hook_context_put();
}


int swap_hook_ctx_reg(struct swap_hook_ctx *hook)
{
	int ret = 0;

	INIT_HLIST_NODE(&hook->node);
	hook_context_get();

	hlist_add_head(&hook->node, &ctx_hook_head);
	ctx_hook_nr++;

	hook_context_put();
	return ret;
}
EXPORT_SYMBOL_GPL(swap_hook_ctx_reg);

void swap_hook_ctx_unreg(struct swap_hook_ctx *hook)
{
	hook_context_get();
	ctx_hook_nr--;
	if (ctx_hook_nr < 0) {
		pr_err("ERROR: [%s:%d]: ctx_hook_nr < 0\n", __FILE__, __LINE__);
		ctx_hook_nr = 0;
	}
	hlist_del(&hook->node);
	hook_context_put();
}
EXPORT_SYMBOL_GPL(swap_hook_ctx_unreg);
