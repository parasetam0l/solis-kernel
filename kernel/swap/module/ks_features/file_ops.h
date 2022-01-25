#ifndef __FILE_OPS__
#define __FILE_OPS__

#include <linux/types.h>
#include <linux/printk.h>


#define FOPS_PREFIX "[FILE_OPS] "


/* TODO: add support CONFIG_SWAP_HOOK_SYSCALL */
#ifdef CONFIG_SWAP_HOOK_SYSCALL

static inline bool file_ops_is_init(void)
{
	return 0;
}

static inline bool file_ops_init(void)
{
	pr_info(FOPS_PREFIX "file_ops is not supported\n");
	return 0;
}

void file_ops_exit(void)
{
}

#else /* CONFIG_SWAP_HOOK_SYSCALL */

bool file_ops_is_init(void);
int file_ops_init(void);
void file_ops_exit(void);

#endif /* CONFIG_SWAP_HOOK_SYSCALL */

#endif /* __FILE_OPS__ */
