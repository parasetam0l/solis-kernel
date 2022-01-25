#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/module.h>
#include "callbacks.h"

static LIST_HEAD(cbs_list);
static DEFINE_MUTEX(cbs_mutex);
static int cur_handle = 0;

struct cb_item {
	struct list_head list;
	enum callback_t type;
	int handle;
	void (*func)(void);
};

static inline void __lock_cbs_list(void)
{
	mutex_lock(&cbs_mutex);
}

static inline void __unlock_cbs_list(void)
{
	mutex_unlock(&cbs_mutex);
}

static inline int __get_new_handle(void)
{
	return cur_handle++;
}

static inline void __free_cb(struct cb_item *cb)
{
	list_del(&cb->list);
	kfree(cb);
}

static struct cb_item *__get_cb_by_handle(int handle)
{
	struct cb_item *cb;

	list_for_each_entry(cb, &cbs_list, list)
		if (cb->handle == handle)
			return cb;

	return NULL;
}


/**
 * @brief Executes callbacks on start/stop
 *
 * @param cbt Callback type
 * @return Void
 */
void exec_cbs(enum callback_t cbt)
{
	struct cb_item *cb;

	__lock_cbs_list();

	list_for_each_entry(cb, &cbs_list, list)
		if (cb->type == cbt)
			cb->func();

	__unlock_cbs_list();
}

/**
 * @brief Removes all callbacks from list
 *
 * @return Void
 */
void remove_all_cbs(void)
{
	struct cb_item *cb, *n;

	__lock_cbs_list();

	list_for_each_entry_safe(cb, n, &cbs_list, list)
		__free_cb(cb);

	__unlock_cbs_list();
}

/**
 * @brief Registers callback on event
 *
 * @param cbt Callback type
 * @param func Callback function
 * @return Handle on succes, error code on error
 */
int us_manager_reg_cb(enum callback_t cbt, void (*func)(void))
{
	struct cb_item *cb;
	int handle;

	cb = kmalloc(sizeof(*cb), GFP_KERNEL);
	if (cb == NULL)
		return -ENOMEM;

	handle = __get_new_handle();

	INIT_LIST_HEAD(&cb->list);
	cb->type = cbt;
	cb->handle = handle;
	cb->func = func;

	__lock_cbs_list();
	list_add_tail(&cb->list, &cbs_list);
	__unlock_cbs_list();

	return handle;
}
EXPORT_SYMBOL_GPL(us_manager_reg_cb);

/**
 * @brief Unegisters callback by handle
 *
 * @param handle Callback handle
 * @return Void
 */
void us_manager_unreg_cb(int handle)
{
	struct cb_item *cb;

	__lock_cbs_list();

	cb = __get_cb_by_handle(handle);
	if (cb == NULL)
		goto handle_not_found;

	__free_cb(cb);

handle_not_found:
	__unlock_cbs_list();
}
EXPORT_SYMBOL_GPL(us_manager_unreg_cb);
