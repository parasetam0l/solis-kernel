#ifndef _SWAP_HOOK_SYSCALL_H
#define _SWAP_HOOK_SYSCALL_H


struct pt_regs;
struct task_struct;


#include <linux/list.h>
#include <linux/errno.h>


struct hook_syscall {
	struct hlist_node node;
	void (*entry)(struct hook_syscall *self, struct pt_regs *regs);
	void (*exit)(struct hook_syscall *self, struct pt_regs *regs);
};

#ifdef CONFIG_SWAP_HOOK_SYSCALL

int hook_syscall_reg(struct hook_syscall *hook, unsigned long nr_call);
void hook_syscall_unreg(struct hook_syscall *hook);

# ifdef CONFIG_COMPAT
int hook_syscall_reg_compat(struct hook_syscall *hook, unsigned long nr_call);
static inline void hook_syscall_unreg_compat(struct hook_syscall *hook)
{
	hook_syscall_unreg(hook);
}
# endif /* CONFIG_COMPAT */

#else /* CONFIG_SWAP_HOOK_SYSCALL */

static inline int hook_syscall_reg(struct hook_syscall *hook,
				   unsigned long nr_call)
{
	return -ENOSYS;
}

static inline void hook_syscall_unreg(struct hook_syscall *hook) {}

# ifdef CONFIG_COMPAT
static inline int hook_syscall_reg_compat(struct hook_syscall *hook,
					  unsigned long nr_call)
{
	return -ENOSYS;
}

static inline void hook_syscall_unreg_compat(struct hook_syscall *hook) {}
# endif /* CONFIG_COMPAT */

#endif /* CONFIG_SWAP_HOOK_SYSCALL */


#endif /* _SWAP_HOOK_SYSCALL_H */
