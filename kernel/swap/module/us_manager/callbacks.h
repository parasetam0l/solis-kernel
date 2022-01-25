#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

enum callback_t {
	START_CB = 0,
	STOP_CB,
};

/* Gets callback type (on start or on stop) and function pointer.
 * Returns positive callback's handle that is used to unregister on success,
 * negative error code otherwise.
 * Exported function. */
int us_manager_reg_cb(enum callback_t cbt, void (*func)(void));

/* Gets handle and unregisters function with this handle.
 * Exported function. */
void us_manager_unreg_cb(int handle);

/* Used to execute callbacks when start/stop is occuring. */
void exec_cbs(enum callback_t cbt);

/* Removes all callbacks */
void remove_all_cbs(void);

#endif /* __CALLBACKS_H__ */
