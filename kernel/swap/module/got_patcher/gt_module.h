#ifndef __GT_MODULE_H__
#define __GT_MODULE_H__

#include <linux/types.h>

enum gt_status {
	GT_ON,
	GT_OFF
};

/* Linker data */
int gtm_set_linker_path(char *path);
int gtm_set_fixup_off(unsigned long offset);
int gtm_set_reloc_off(unsigned long offset);
int gtm_switch(enum gt_status stat);
enum gt_status gtm_status(void);

/* Target processes data */
int gtm_add_by_path(char *path);
int gtm_add_by_pid(pid_t pid);
int gtm_add_by_id(char *id);
int gtm_del_by_path(char *path);
int gtm_del_by_pid(pid_t pid);
int gtm_del_by_id(char *id);
int gtm_del_all(void);
/* Returns len on success, error code on fail.
 * Allocates memory with malloc(), caller should free it */
ssize_t gtm_get_targets(char **targets);

/* Handlers data */
int gtm_set_handler_path(char *path);
int gtm_set_handler_fixup_off(unsigned long offset);
int gtm_set_handler_reloc_off(unsigned long offset);

/* Pthread data */
int gtm_set_pthread_path(char *path);
int gtm_set_init_off(unsigned long offset);

#endif /* __GT_MODULE_H__ */
