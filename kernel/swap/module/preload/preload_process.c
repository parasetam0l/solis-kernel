#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <us_manager/us_common_file.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/sspt/sspt_page.h>
#include <us_manager/sspt/sspt_file.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/probes/probe_info_new.h>
#include <loader/loader.h>
#include "preload.h"
#include "preload_process.h"


enum task_id_t {
	SET_BY_PATH,
	SET_BY_PID,
	SET_BY_ID
};

struct process_t {
	struct list_head list;
	struct pf_group *pfg;
	enum task_id_t idt;
	union {
		struct dentry *dentry;
		pid_t pid;
		char *id;
	};
	struct probe_new p_init;
};

struct bin_data_t {
	struct dentry *dentry;
	unsigned long off;
};

/* For process list elements checker */
typedef struct process_t *(*checker_t)(struct process_t *, void *);

static LIST_HEAD(_reg_proc_list);
static LIST_HEAD(_unreg_proc_list);
static DEFINE_MUTEX(_proc_list_lock);

static struct bin_data_t _pthread_init;

static inline void _lock_proc_list(void)
{
	mutex_lock(&_proc_list_lock);
}

static inline void _unlock_proc_list(void)
{
	mutex_unlock(&_proc_list_lock);
}

static inline struct process_t *_check_by_dentry(struct process_t *proc,
						 void *data)
{
	struct dentry *dentry = data;

	if (proc->idt == SET_BY_PATH && proc->dentry == dentry)
		return proc;

	return NULL;
}

static inline struct process_t *_check_by_pid(struct process_t *proc,
					      void *data)
{
	pid_t pid = *(pid_t *)data;

	if (proc->idt == SET_BY_PID && proc->pid == pid)
		return proc;

	return NULL;
}

static inline struct process_t *_check_by_id(struct process_t *proc, void *data)
{
	char *id = data;

	if (proc->idt == SET_BY_ID && proc->id == id)
		return proc;

	return NULL;
}

static inline bool _is_pthread_data_available(void)
{
	return (_pthread_init.dentry && _pthread_init.off);
}




static int pthread_init_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct pd_t *pd = lpd_get_by_task(current);

	lpd_set_init_state(pd, false);

	return 0;
}

static int pthread_init_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct pd_t *pd = lpd_get_by_task(current);

	lpd_set_init_state(pd, true);

	return 0;
}

static struct probe_desc pin_pinit = MAKE_URPROBE(pthread_init_eh,
						  pthread_init_rh, 0);

static int _proc_reg_no_lock(struct process_t *proc)
{
	int ret;

	proc->p_init.desc = &pin_pinit;
	proc->p_init.offset = _pthread_init.off;
	ret = pin_register(&proc->p_init, proc->pfg, _pthread_init.dentry);
	if (ret)
		pr_warn(PRELOAD_PREFIX "Can't register pthread init probe\n");

	return ret;
}

static void _proc_unreg_no_lock(struct process_t *proc)
{
	pin_unregister(&proc->p_init, proc->pfg);
}

static struct process_t *_proc_create(struct pf_group *pfg)
{
	struct process_t *proc;

	proc = kzalloc(sizeof(*proc), GFP_KERNEL);
	if (!proc) {
		pr_warn(PRELOAD_PREFIX "No mem to alloc proccess_t struct!\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&proc->list);
	proc->pfg = pfg;

	return proc;
}

static void _proc_destroy(struct process_t *proc)
{
	if (proc->pfg)
		put_pf_group(proc->pfg);

	if (proc->idt == SET_BY_PATH)
		swap_put_dentry(proc->dentry);

	if (proc->idt == SET_BY_ID)
		kfree(proc->id);

	kfree(proc);
}

static int _add_to_list(struct process_t *proc)
{
	_lock_proc_list();
	list_add_tail(&proc->list, &_unreg_proc_list);
	_unlock_proc_list();

	return 0;
}

/* checker - used to check process list candidates, chosen by type of
 * registration (path, pid, id)
 * data - pointer to target process data, checker compares candidate with
 */

static struct process_t *_get_from_list_no_lock(checker_t checker, void *data,
						struct list_head *list)
{
	struct process_t *proc;

	list_for_each_entry(proc, list, list) {
		if (checker(proc, data))
			return proc;
	}

	return NULL;
}

static void _delete_from_list(checker_t checker, void *data)
{
	struct process_t *proc;

	_lock_proc_list();
	proc = _get_from_list_no_lock(checker, data, &_unreg_proc_list);
	if (proc) {
		list_del(&proc->list);
		_proc_destroy(proc);
		goto delete_from_list_unlock;
	}

	proc = _get_from_list_no_lock(checker, data, &_reg_proc_list);
	if (proc) {
		list_del(&proc->list);
		_proc_unreg_no_lock(proc);
		_proc_destroy(proc);
	}

delete_from_list_unlock:
	_unlock_proc_list();
}



int pp_add_by_path(const char *path)
{
	struct dentry *dentry;
	struct pf_group *pfg;
	struct process_t *proc;
	int ret;

	dentry = swap_get_dentry(path);
	if (!dentry) {
		pr_warn(PRELOAD_PREFIX "Can't get dentry for <%s>\n", path);
		return -EINVAL;
	}

	pfg = get_pf_group_by_dentry(dentry, dentry);
	if (pfg == NULL) {
		ret = -ENOMEM;
		goto by_path_put_dentry;
	}

	proc = _proc_create(pfg);
	if (!proc) {
		ret = -EINVAL;
		goto by_path_put_pfg;
	}

	proc->idt = SET_BY_PATH;
	proc->dentry = dentry;

	ret = _add_to_list(proc);
	if (ret)
		goto by_path_free;

	return 0;

by_path_free:
	kfree(proc);

by_path_put_pfg:
	put_pf_group(pfg);

by_path_put_dentry:
	swap_put_dentry(dentry);

	return ret;
}

int pp_add_by_pid(pid_t pid)
{
	struct pf_group *pfg;
	struct process_t *proc;
	int ret;

	pfg = get_pf_group_by_tgid(pid, NULL);
	if (pfg == NULL)
		return -ENOMEM;

	proc = _proc_create(pfg);
	if (!proc) {
		ret = -EINVAL;
		goto by_pid_put_pfg;
	}

	proc->idt = SET_BY_PID;
	proc->pid = pid;

	ret = _add_to_list(proc);
	if (ret)
		goto by_pid_free;

	return 0;

by_pid_free:
	kfree(proc);

by_pid_put_pfg:
	put_pf_group(pfg);

	return ret;
}

int pp_add_by_id(const char *id)
{
	char *new_id;
	struct pf_group *pfg;
	struct process_t *proc;
	int ret;

	new_id = kstrdup(id, GFP_KERNEL);
	if (!new_id)
		return -ENOMEM;

	pfg = get_pf_group_by_comm((char *)id, NULL);
	if (pfg == NULL) {
		ret = -ENOMEM;
		goto by_id_free_new_id;
	}

	proc = _proc_create(pfg);
	if (!proc) {
		ret = -EINVAL;
		goto by_id_put_pfg;
	}

	proc->idt = SET_BY_ID;
	proc->id = new_id;

	ret = _add_to_list(proc);
	if (ret)
		goto by_id_free;

	return 0;

by_id_free:
	kfree(proc);

by_id_put_pfg:
	put_pf_group(pfg);

by_id_free_new_id:
	kfree(new_id);

	return ret;
}

int pp_del_by_path(const char *path)
{
	struct dentry *dentry;

	dentry = swap_get_dentry(path);
	if (!dentry) {
		pr_warn(PRELOAD_PREFIX "No dentry for <%s>\n", path);
		return -EINVAL;
	}

	_delete_from_list(_check_by_dentry, dentry);

	swap_put_dentry(dentry);

	return 0;
}

int pp_del_by_pid(pid_t pid)
{
	_delete_from_list(_check_by_pid, &pid);

	return 0;
}

int pp_del_by_id(const char *id)
{
	_delete_from_list(_check_by_id, (void *)id);

	return 0;
}

void pp_del_all(void)
{
	struct process_t *tmp, *proc;

	_lock_proc_list();

	list_for_each_entry_safe(proc, tmp, &_unreg_proc_list, list) {
		list_del(&proc->list);
		_proc_destroy(proc);
	}

	list_for_each_entry_safe(proc, tmp, &_reg_proc_list, list) {
		list_del(&proc->list);
		_proc_unreg_no_lock(proc);
		_proc_destroy(proc);
	}

	_unlock_proc_list();
}

void pp_disable(void)
{
	struct process_t *proc;

	_lock_proc_list();

	list_for_each_entry(proc, &_reg_proc_list, list) {
		_proc_unreg_no_lock(proc);
		list_move_tail(&proc->list, &_unreg_proc_list);
	}

	_unlock_proc_list();
}

int pp_enable(void)
{
	struct process_t *proc;
	int ret;

	if (!_is_pthread_data_available())
		return -EINVAL;

	_lock_proc_list();

	list_for_each_entry(proc, &_unreg_proc_list, list) {
		ret = _proc_reg_no_lock(proc);
		if (ret)
			goto enable_pp_fail;
		else
			list_move_tail(&proc->list, &_reg_proc_list);
	}

	_unlock_proc_list();

	return 0;

enable_pp_fail:
	_unlock_proc_list();

	pr_warn(PRELOAD_PREFIX "Error register probes, disabling...\n");
	pp_disable();

	return ret;
}

int pp_set_pthread_path(const char *path)
{
	struct dentry *dentry;

	dentry = swap_get_dentry(path);
	if (dentry == NULL)
		return -EINVAL;

	if (_pthread_init.dentry != NULL)
		swap_put_dentry(_pthread_init.dentry);

	_pthread_init.dentry = dentry;

	return 0;
}

int pp_set_init_offset(unsigned long off)
{
	_pthread_init.off = off;

	return 0;
}
