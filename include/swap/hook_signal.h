#ifndef _LINUX_SWAP_HOOK_SIGNAL_H
#define _LINUX_SWAP_HOOK_SIGNAL_H


#ifdef CONFIG_SWAP_HOOK_SIGNAL

#include <linux/list.h>
#include <linux/compiler.h>

struct module;
struct ksignal;

struct hook_signal {
	struct hlist_node node;
	struct module *owner;
	void (*hook)(struct ksignal *ksig);
};

int hook_signal_reg(struct hook_signal *hook);
void hook_signal_unreg(struct hook_signal *hook);


/* private interface */
extern int __hook_signal_counter;
void __hook_signal(struct ksignal *ksig);

static inline void swap_hook_signal(struct ksignal *ksig)
{
	if (unlikely(__hook_signal_counter))
		__hook_signal(ksig);
}

#else /* CONFIG_SWAP_HOOK_SIGNAL */

static inline void swap_hook_signal(struct ksignal *ksig) {}

#endif /* CONFIG_SWAP_HOOK_SIGNAL */


#endif /* _LINUX_SWAP_HOOK_SIGNAL_H */
