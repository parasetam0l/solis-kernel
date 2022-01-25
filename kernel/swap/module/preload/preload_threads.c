#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/list.h>
#include <kprobe/swap_ktd.h>
#include "preload.h"
#include "preload_threads.h"

struct preload_td {
	struct list_head slots;
	unsigned long flags;
};

struct thread_slot {
	struct list_head list;
	struct list_head disabled_addrs;

	unsigned long caller;
	unsigned long orig;
	unsigned char call_type;
	bool drop;   /* TODO Workaround, remove when will be possible to install
		      * several us probes at the same addr. */
};

struct disabled_addr {
	struct list_head list;
	unsigned long addr;
};

static void preload_ktd_init(struct task_struct *task, void *data)
{
	struct preload_td *td = (struct preload_td *)data;

	INIT_LIST_HEAD(&td->slots);
	td->flags = 0;
}

static void preload_ktd_exit(struct task_struct *task, void *data)
{
	/* TODO: to be implement */
}

static struct ktask_data preload_ktd = {
	.init = preload_ktd_init,
	.exit = preload_ktd_exit,
	.size = sizeof(struct preload_td),
};


static inline struct preload_td *get_preload_td(struct task_struct *task)
{
	return (struct preload_td *)swap_ktd(&preload_ktd, task);
}

unsigned long pt_get_flags(struct task_struct *task)
{
	return get_preload_td(task)->flags;
}

void pt_set_flags(struct task_struct *task, unsigned long flags)
{
	get_preload_td(task)->flags = flags;
}


static inline bool __is_addr_found(struct disabled_addr *da,
				   unsigned long addr)
{
	if (da->addr == addr)
		return true;

	return false;
}

static inline void __remove_from_disable_list(struct disabled_addr *da)
{
	list_del(&da->list);
	kfree(da);
}

static inline void __remove_whole_disable_list(struct thread_slot *slot)
{
	struct disabled_addr *da, *n;

	list_for_each_entry_safe(da, n, &slot->disabled_addrs, list)
		__remove_from_disable_list(da);
}

static inline void __init_slot(struct thread_slot *slot)
{
	slot->caller = 0;
	slot->orig = 0;
	slot->call_type = 0;
	slot->drop = false;
	INIT_LIST_HEAD(&slot->disabled_addrs);
}

static inline void __reinit_slot(struct thread_slot *slot)
{
	__remove_whole_disable_list(slot);
	__init_slot(slot);
}

static inline void __set_slot(struct thread_slot *slot,
			      struct task_struct *task, unsigned long caller,
			      unsigned long orig, unsigned char call_type,
			      bool drop)
{
	slot->caller = caller;
	slot->orig = orig;
	slot->call_type = call_type;
	slot->drop = drop;
}

static inline int __add_to_disable_list(struct thread_slot *slot,
					unsigned long disable_addr)
{
	struct disabled_addr *da = kmalloc(sizeof(*da), GFP_ATOMIC);

	if (da == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&da->list);
	da->addr = disable_addr;
	list_add_tail(&da->list, &slot->disabled_addrs);

	return 0;
}

static inline struct disabled_addr *__find_disabled_addr(struct thread_slot *slot,
							 unsigned long addr)
{
	struct disabled_addr *da;

	list_for_each_entry(da, &slot->disabled_addrs, list)
		if (__is_addr_found(da, addr))
			return da;

	return NULL;
}

/* Adds a new slot */
static inline struct thread_slot *__grow_slot(void)
{
	struct thread_slot *tmp = kmalloc(sizeof(*tmp), GFP_ATOMIC);

	if (tmp == NULL)
		return NULL;

	INIT_LIST_HEAD(&tmp->list);
	__init_slot(tmp);

	return tmp;
}

/* Free slot */
static void __clean_slot(struct thread_slot *slot)
{
	list_del(&slot->list);
	kfree(slot);
}

/* There is no list_last_entry in Linux 3.10 */
#ifndef list_last_entry
#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)
#endif /* list_last_entry */

static inline struct thread_slot *__get_task_slot(struct task_struct *task)
{
	struct preload_td *td = get_preload_td(task);

	return list_empty(&td->slots) ? NULL :
		list_last_entry(&td->slots, struct thread_slot, list);
}




int pt_set_data(struct task_struct *task, struct pt_data_t *data)
{
	struct preload_td *td = get_preload_td(task);
	struct thread_slot *slot;
	unsigned long caller, disable_addr, orig;
	unsigned char call_type;
	bool drop;
	int ret = 0;

	caller = data->caller;
	disable_addr = data->disable_addr;
	orig = data->orig;
	call_type = data->call_type;
	drop = data->drop;

	slot = __grow_slot();
	if (slot == NULL) {
		ret = -ENOMEM;
		goto set_data_done;
	}

	if ((disable_addr != 0) &&
	    (__add_to_disable_list(slot, disable_addr) != 0)) {
		printk(KERN_ERR PRELOAD_PREFIX "Cannot alloc memory!\n");
		ret = -ENOMEM;
		goto set_data_done;
	}

	__set_slot(slot, task, caller, orig, call_type, drop);
	list_add_tail(&slot->list, &td->slots);

set_data_done:
	return ret;
}

int pt_get_caller(struct task_struct *task, unsigned long *caller)
{
	struct thread_slot *slot;

	slot = __get_task_slot(task);
	if (slot != NULL) {
		*caller = slot->caller;
		return 0;
	}

	return -EINVAL;
}

int pt_get_orig(struct task_struct *task, unsigned long *orig)
{
	struct thread_slot *slot;

	slot = __get_task_slot(task);
	if (slot != NULL) {
		*orig = slot->caller;
		return 0;
	}

	return -EINVAL;
}

int pt_get_call_type(struct task_struct *task,
				  unsigned char *call_type)
{
	struct thread_slot *slot;

	slot = __get_task_slot(task);
	if (slot != NULL) {
		*call_type = slot->call_type;
		return 0;
	}

	return -EINVAL;
}

int pt_get_drop(struct task_struct *task)
{
	struct thread_slot *slot;

	slot = __get_task_slot(task);
	if (slot != NULL)
		return (int)slot->drop;

	return -EINVAL;
}

bool pt_check_disabled_probe(struct task_struct *task, unsigned long addr)
{
	struct thread_slot *slot;
	bool ret = false;

	slot = __get_task_slot(task);
	if (slot != NULL)
		ret = __find_disabled_addr(slot, addr) == NULL ? false : true;

	return ret;
}

void pt_enable_probe(struct task_struct *task, unsigned long addr)
{
	struct thread_slot *slot;
	struct disabled_addr *da;

	slot = __get_task_slot(task);
	if (slot == NULL) {
		printk(KERN_ERR PRELOAD_PREFIX "Error! Slot not found!\n");
		goto enable_probe_failed;
	}

	da = __find_disabled_addr(slot, addr);
	if (da != NULL)
		__remove_from_disable_list(da);

enable_probe_failed:
	return; /* make gcc happy: cannot place label right before '}' */
}

int pt_put_data(struct task_struct *task)
{
	struct thread_slot *slot;
	int ret = 0;

	slot = __get_task_slot(task);
	if (slot != NULL) {
		__reinit_slot(slot);
		__clean_slot(slot); /* remove from list */
	}

	return ret;
}

int pt_init(void)
{
	return swap_ktd_reg(&preload_ktd);
}

void pt_exit(void)
{
	swap_ktd_unreg(&preload_ktd);
}
