#ifndef _LINUX_SWAP_HOOK_USAUX_H
#define _LINUX_SWAP_HOOK_USAUX_H


#ifdef CONFIG_SWAP_HOOK_USAUX

#include <linux/compiler.h>

struct file;
struct module;
struct task_struct;

struct hook_usaux {
	struct module *owner;
	void (*page_fault)(unsigned long addr);
	void (*copy_process_pre)(void);
	void (*copy_process_post)(struct task_struct *task);
	void (*mm_release)(struct task_struct *task);
	void (*munmap)(unsigned long start, unsigned long end);
	void (*mmap)(struct file *file, unsigned long addr);
	void (*set_comm)(struct task_struct *task);
	void (*change_leader)(struct task_struct *p, struct task_struct *n);
};


extern struct hook_usaux *hook_usaux_user;

int hook_usaux_set(struct hook_usaux *hook);
void hook_usaux_reset(void);


/* private interface */
struct hook_usaux *swap_hook_usaux_get(void);
void swap_hook_usaux_put(struct hook_usaux *hook);


#define SWAP_HOOK_USAUX_CALL(hook_name, ...) \
	if (unlikely(hook_usaux_user)) { \
		struct hook_usaux *hook = swap_hook_usaux_get(); \
		if (hook) { \
			hook->hook_name(__VA_ARGS__); \
			swap_hook_usaux_put(hook); \
		} \
	}

#else /* CONFIG_SWAP_HOOK_USAUX */

#define SWAP_HOOK_USAUX_CALL(hook_name, ...)

#endif /* CONFIG_SWAP_HOOK_USAUX */


static inline void swap_usaux_page_fault(unsigned long addr)
{
	SWAP_HOOK_USAUX_CALL(page_fault, addr);
}

static inline void swap_usaux_copy_process_pre(void)
{
	SWAP_HOOK_USAUX_CALL(copy_process_pre);
}

static inline void swap_usaux_copy_process_post(struct task_struct *task)
{
	SWAP_HOOK_USAUX_CALL(copy_process_post, task);
}

static inline void swap_usaux_mm_release(struct task_struct *task)
{
	SWAP_HOOK_USAUX_CALL(mm_release, task);
}

static inline void swap_usaux_munmap(unsigned long start, unsigned long end)
{
	SWAP_HOOK_USAUX_CALL(munmap, start, end);
}

static inline void swap_usaux_mmap(struct file *file, unsigned long addr)
{
	SWAP_HOOK_USAUX_CALL(mmap, file, addr);
}

static inline void swap_usaux_set_comm(struct task_struct *task)
{
	SWAP_HOOK_USAUX_CALL(set_comm, task);
}

static inline void swap_usaux_change_leader(struct task_struct *prev,
				     struct task_struct *next)
{
	SWAP_HOOK_USAUX_CALL(change_leader, prev, next);
}

#endif /* _LINUX_SWAP_HOOK_USAUX_H */
