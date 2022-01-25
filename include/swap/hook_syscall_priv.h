#ifndef _SWAP_HOOK_SYSCALL_PRIV_H
#define _SWAP_HOOK_SYSCALL_PRIV_H


struct pt_regs;
struct task_struct;


#ifdef CONFIG_SWAP_HOOK_SYSCALL

#include <linux/sched.h>

static inline void swap_hook_syscall_update(struct task_struct *p)
{
        if (test_thread_flag(TIF_SWAP_HOOK_SYSCALL))
                set_tsk_thread_flag(p, TIF_SWAP_HOOK_SYSCALL);
        else
                clear_tsk_thread_flag(p, TIF_SWAP_HOOK_SYSCALL);
}

void swap_hook_syscall_entry(struct pt_regs *regs);
void swap_hook_syscall_exit(struct pt_regs *regs);

#else /* CONFIG_SWAP_HOOK_SYSCALL */

static inline void swap_hook_syscall_update(struct task_struct *p) {}
static inline void swap_hook_syscall_entry(struct pt_regs *regs) {}
static inline void swap_hook_syscall_exit(struct pt_regs *regs) {}

#endif /* CONFIG_SWAP_HOOK_SYSCALL */


#endif /* _SWAP_HOOK_SYSCALL_PRIV_H */
