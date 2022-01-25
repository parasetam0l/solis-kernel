#include <linux/list.h>
#include <linux/slab.h>
#include "sspt_filter.h"
#include "sspt_proc.h"
#include "../pf/pf_group.h"


struct sspt_filter *sspt_filter_create(struct sspt_proc *proc,
				       struct pf_group *pfg)
{
	struct sspt_filter *fl;

	fl = kmalloc(sizeof(*fl), GFP_ATOMIC);
	if (fl == NULL)
		return NULL;

	INIT_LIST_HEAD(&fl->list);

	fl->proc = proc;
	fl->pfg = pfg;
	fl->pfg_is_inst = false;

	return fl;
}

void sspt_filter_free(struct sspt_filter *fl)
{
	if (fl->pfg_is_inst) {
		struct pfg_msg_cb *cb = pfg_msg_cb_get(fl->pfg);

		if (cb && cb->msg_term)
			cb->msg_term(fl->proc->leader);
	}

	kfree(fl);
}
