#ifndef __SSPT_FILTER_H__
#define __SSPT_FILTER_H__

#include <linux/types.h>

struct pf_group;
struct sspt_proc;

struct sspt_filter {
	struct list_head list;
	struct sspt_proc *proc;
	struct pf_group *pfg;
	bool pfg_is_inst;
};

struct sspt_filter *sspt_filter_create(struct sspt_proc *proc,
				       struct pf_group *pfg);
void sspt_filter_free(struct sspt_filter *fl);

#endif /* __SSPT_FILTER_H__ */
