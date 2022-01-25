#ifndef _LINUX_SWAP_HOOK_SWITCH_TO_H
#define _LINUX_SWAP_HOOK_SWITCH_TO_H


#ifdef CONFIG_SWAP_HOOK_SWITCH_TO

#include <linux/compiler.h>


struct swap_hook_ctx {
	struct hlist_node node;
	void (*hook)(struct task_struct *prev, struct task_struct *next);
};


extern int ctx_hook_nr;

int swap_hook_ctx_reg(struct swap_hook_ctx *hook);
void swap_hook_ctx_unreg(struct swap_hook_ctx *hook);


/* private interface */
void swap_hook_ctx_call(struct task_struct *prev, struct task_struct *next);

static inline void swap_hook_switch_to(struct task_struct *prev,
				       struct task_struct *next)
{
	if (unlikely(ctx_hook_nr)) {
		swap_hook_ctx_call(prev, next);
	}
}

#else /* CONFIG_SWAP_HOOK_SWITCH_TO */

static inline void swap_hook_switch_to(struct task_struct *prev,
				       struct task_struct *next)
{
}
#endif /* CONFIG_SWAP_HOOK_SWITCH_TO */

#endif /* _LINUX_SWAP_HOOK_SWITCH_TO_H */
