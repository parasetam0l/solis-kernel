#ifndef __LOADER__
#define __LOADER__

struct pd_t;
struct hd_t;
struct uretprobe_instance;
struct task_struct;

/* process loader states */
enum ps_t {
	NOT_LOADED,
	LOADING,
	LOADED,
	FAILED,
	ERROR
};

void loader_module_prepare_ujump(struct uretprobe_instance *ri,
				  struct pt_regs *regs, unsigned long addr);

unsigned long loader_not_loaded_entry(struct uretprobe_instance *ri,
				       struct pt_regs *regs, struct pd_t *pd,
				       struct hd_t *hd);
void loader_loading_ret(struct uretprobe_instance *ri, struct pt_regs *regs,
			 struct pd_t *pd, struct hd_t *hd);
void loader_failed_ret(struct uretprobe_instance *ri, struct pt_regs *regs,
			struct pd_t *pd, struct hd_t *hd);

void loader_set_rp_data_size(struct uretprobe *rp);
void loader_set_priv_origin(struct uretprobe_instance *ri, unsigned long addr);
unsigned long loader_get_priv_origin(struct uretprobe_instance *ri);
int loader_add_handler(const char *path);


struct pd_t *lpd_get(struct sspt_proc *proc);
struct pd_t *lpd_get_by_task(struct task_struct *task);
struct hd_t *lpd_get_hd(struct pd_t *pd, struct dentry *dentry);

bool lpd_get_init_state(struct pd_t *pd);
void lpd_set_init_state(struct pd_t *pd, bool state);

struct dentry *lpd_get_dentry(struct hd_t *hd);
struct pd_t *lpd_get_parent_pd(struct hd_t *hd);
enum ps_t lpd_get_state(struct hd_t *hd);
unsigned long lpd_get_handlers_base(struct hd_t *hd);
void *lpd_get_handle(struct hd_t *hd);
long lpd_get_attempts(struct hd_t *hd);
void lpd_dec_attempts(struct hd_t *hd);


#endif /* __LOADER__ */
