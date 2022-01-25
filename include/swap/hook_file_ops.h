#ifndef _LINUX_SWAP_FILE_OPS_H
#define _LINUX_SWAP_FILE_OPS_H


struct file;


#ifdef CONFIG_SWAP_HOOK_FILE_OPS

#include <linux/list.h>

struct module;

struct swap_hook_fops {
	struct hlist_node node;
	struct module *owner;
	void (*filp_close)(struct file *filp);
};

int swap_hook_fops_reg(struct swap_hook_fops *hook);
void swap_hook_fops_unreg(struct swap_hook_fops *hook);


/* private interface */
extern int swap_fops_counter;
void call_fops_filp_close(struct file *filp);

static inline void swap_fops_filp_close(struct file *filp)
{
	if (unlikely(swap_fops_counter))
		call_fops_filp_close(filp);
}

#else /* CONFIG_SWAP_HOOK_FILE_OPS */

static inline void swap_fops_filp_close(struct file *filp) {}

#endif /* CONFIG_SWAP_HOOK_FILE_OPS */


#endif /* _LINUX_SWAP_FILE_OPS_H */
