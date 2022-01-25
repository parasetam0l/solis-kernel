#ifndef __LOADER_PD_H__
#define __LOADER_PD_H__

#include <loader/loader.h>

struct pd_t;
struct hd_t;
struct sspt_proc;
struct dentry;
struct list_head;


unsigned long lpd_get_loader_base(struct pd_t *pd);
void lpd_set_loader_base(struct pd_t *pd, unsigned long vaddr);

void lpd_set_state(struct hd_t *hd, enum ps_t state);
void lpd_set_handlers_base(struct hd_t *hd, unsigned long vaddr);
void lpd_set_handle(struct hd_t *hd, void __user *handle);

char __user *lpd_get_path(struct pd_t *pd, struct hd_t *hd);

int lpd_init(void);
void lpd_uninit(void);


#endif /* __LOADER_PD_H__*/
