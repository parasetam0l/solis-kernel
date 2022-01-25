#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/limits.h>
#include <kprobe/swap_ktd.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/sspt/sspt_page.h>
#include <us_manager/sspt/sspt_file.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/probes/probe_info_new.h>
#include <us_manager/us_common_file.h>
#include <loader/loader.h>
#include <master/swap_initializer.h>
#include "gt.h"
#include "gt_module.h"
#include "gt_debugfs.h"


enum task_id_t {
	GT_SET_BY_DENTRY,
	GT_SET_BY_PID,
	GT_SET_BY_ID,
};

struct l_probe_el {
	struct list_head list;

	struct pf_group *pfg;
	enum task_id_t task_id;
	union {
		pid_t tgid;
		struct dentry *dentry;
		char *id;
	};

	struct probe_new p_fixup;
	struct probe_new p_reloc;
	struct probe_new p_init;
};

struct bin_data_t {
	struct dentry *dentry;
	unsigned long off;
};

typedef unsigned long (*rh_t)(struct uretprobe_instance *, struct pt_regs *,
			      struct hd_t *);

/* Typedef for compare function that removes data from list */
typedef bool (*cf_t)(struct l_probe_el*, void *);

static LIST_HEAD(_reg_probes);
static LIST_HEAD(_unreg_probes);
static DEFINE_MUTEX(_linker_probes_lock);

static enum gt_status _status = GT_OFF;
static struct bin_data_t _linker_fixup;
static struct bin_data_t _linker_reloc;
static struct bin_data_t _handler_fixup;
static struct bin_data_t _handler_reloc;
static struct bin_data_t _pthread_init;


static inline void _lock_probes_list(void)
{
	mutex_lock(&_linker_probes_lock);
}

static inline void _unlock_probes_list(void)
{
	mutex_unlock(&_linker_probes_lock);
}

static inline bool _is_enable(void)
{
	return _status == GT_ON;
}

static inline bool _is_bin_data_available(struct bin_data_t *bin_data)
{
	return (bin_data->dentry && bin_data->off);
}

static inline bool _is_linker_data_available(void)
{
	return _is_bin_data_available(&_linker_fixup) &&
	       _is_bin_data_available(&_linker_reloc);
}

static inline bool _is_handler_data_available(void)
{
	return _is_bin_data_available(&_handler_fixup) &&
	       _is_bin_data_available(&_handler_reloc);
}

static inline bool _is_pthread_data_available(void)
{
	return _is_bin_data_available(&_pthread_init);
}




/* ===========================================================================
 * =                           TASK DATA MANIPULATION                        =
 * ===========================================================================
 */

static void gt_ktd_init(struct task_struct *task, void *data)
{
	bool *in_handler = (bool *)data;

	*in_handler = false;
}

static void gt_ktd_exit(struct task_struct *task, void *data)
{
}

static struct ktask_data gt_ktd = {
	.init = gt_ktd_init,
	.exit = gt_ktd_exit,
	.size = sizeof(bool),
};

static inline bool _is_in_handler(void)
{
	return *(bool *)swap_ktd(&gt_ktd, current);
}

static inline void _set_in_handler(bool is_in_handler)
{
	*(bool *)swap_ktd(&gt_ktd, current) = is_in_handler;
}

static inline bool _check_by_dentry(struct l_probe_el *l_probe, void *data)
{
	struct dentry *dentry = (struct dentry *)data;

	if (l_probe->task_id == GT_SET_BY_DENTRY &&
	    l_probe->dentry == dentry)
		return true;

	return false;
}

static inline bool _check_by_pid(struct l_probe_el *l_probe, void *data)
{
	pid_t tgid = *(pid_t *)data;

	if (l_probe->task_id == GT_SET_BY_PID &&
	    l_probe->tgid == tgid)
		return true;

	return false;
}

static inline bool _check_by_id(struct l_probe_el *l_probe, void *data)
{
	char *id = (char *)data;

	if (l_probe->task_id == GT_SET_BY_ID &&
	    strncmp(l_probe->id, id, PATH_MAX))
		return true;

	return false;
}

static inline bool _del_all(struct l_probe_el *l_probe, void *data)
{
	return true;
}

/* ===========================================================================
 * =                            LINKER HANDLERS                              =
 * ===========================================================================
 */

static unsigned long _redirect_to_handler(struct uretprobe_instance *ri,
					  struct pt_regs *regs,
					  struct hd_t *hd, unsigned long off)
{
	unsigned long base;
	unsigned long vaddr;

	base = lpd_get_handlers_base(hd);
	if (base == 0)
		return 0;

	vaddr = base + off;
	loader_module_prepare_ujump(ri, regs, vaddr);
	_set_in_handler(true);

	return vaddr;
}

static unsigned long _redirect_to_fixup_handler(struct uretprobe_instance *ri,
						struct pt_regs *regs,
						struct hd_t *hd)
{
	return _redirect_to_handler(ri, regs, hd, _handler_fixup.off);
}

static unsigned long _redirect_to_reloc_handler(struct uretprobe_instance *ri,
						struct pt_regs *regs,
						struct hd_t *hd)
{
	return _redirect_to_handler(ri, regs, hd, _handler_reloc.off);
}



static int _process_eh(struct uretprobe_instance *ri, struct pt_regs *regs,
		       rh_t rh, struct dentry *dentry)
{
	struct pd_t *pd = lpd_get_by_task(current);
	struct hd_t *hd;
	unsigned long vaddr = 0;
	unsigned long old_pc = swap_get_upc(regs);

	if ((dentry == NULL) || _is_in_handler())
		goto out_set_orig;

	hd = lpd_get_hd(pd, dentry);
	if (hd == NULL)
		goto out_set_orig;

	if ((lpd_get_state(hd) == NOT_LOADED || lpd_get_state(hd) == FAILED) &&
	    lpd_get_init_state(pd))
		vaddr = loader_not_loaded_entry(ri, regs, pd, hd);
	else if (lpd_get_state(hd) == LOADED)
		vaddr = rh(ri, regs, hd);

out_set_orig:
	loader_set_priv_origin(ri, vaddr);

	/* PC change check */
	return old_pc != swap_get_upc(regs);
}

static int dl_fixup_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	return _process_eh(ri, regs, &_redirect_to_fixup_handler,
			   _handler_fixup.dentry);
}



static void _restore_exec(struct uretprobe_instance *ri, struct hd_t *hd)
{
	if (_is_in_handler())
		_set_in_handler(false);
}

static int _process_rh(struct uretprobe_instance *ri, struct pt_regs *regs,
		       rh_t rh, struct dentry *dentry)
{
	struct pd_t *pd = lpd_get_by_task(current);
	struct hd_t *hd;

	if (dentry == NULL)
		return 0;

	hd = lpd_get_hd(pd, dentry);
	if (hd == NULL)
		return 0;

	switch (lpd_get_state(hd)) {
	case NOT_LOADED:
		break;
	case LOADING:
		loader_loading_ret(ri, regs, pd, hd);
		rh(ri, regs, hd); /* TODO Think about: Possible only if we
				   * do not need _set_in_handler() */
		break;
	case LOADED:
		/* TODO Check does we need this if library is loaded
		 * with RTLD_NOW ? */
		_restore_exec(ri, hd);
		break;
	case FAILED:
		loader_failed_ret(ri, regs, pd, hd);
		break;
	case ERROR:
	default:
		break;
	}

	return 0;
}

static int dl_fixup_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	return _process_rh(ri, regs, &_redirect_to_fixup_handler,
			   _handler_fixup.dentry);
}

/* TODO Make ordinary interface. Now real data_size is set in init, because
 * it is unknown in this module during compile time. */
static struct probe_desc pin_fixup = MAKE_URPROBE(dl_fixup_eh, dl_fixup_rh, 0);


static int dl_reloc_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	return _process_eh(ri, regs, &_redirect_to_reloc_handler,
			   _handler_reloc.dentry);
}

static int dl_reloc_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	return _process_rh(ri, regs, &_redirect_to_reloc_handler,
			   _handler_reloc.dentry);
}

/* TODO Make ordinary interface. Now real data_size is set in init, because
 * it is unknown in this module during compile time. */
static struct probe_desc pin_reloc = MAKE_URPROBE(dl_reloc_eh, dl_reloc_rh, 0);


static int pthread_init_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct pd_t *pd = lpd_get_by_task(current);

	/* Wait until pthread is inited, if it is going to be inited before
	 * loading our library */
	lpd_set_init_state(pd, false);

	return 0;
}

static int pthread_init_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct pd_t *pd = lpd_get_by_task(current);

	lpd_set_init_state(pd, true);

	return 0;
}

/* TODO Make ordinary interface. Now real data_size is set in init, because
 * it is unknown in this module during compile time. */
static struct probe_desc pin_pinit = MAKE_URPROBE(pthread_init_eh,
						 pthread_init_rh, 0);


static int _register_probe_el_no_lock(struct l_probe_el *l_probe)
{
	int ret;

	/* register 'fix_up' */
	l_probe->p_fixup.desc = &pin_fixup;
	l_probe->p_fixup.offset = _linker_fixup.off;
	ret = pin_register(&l_probe->p_fixup, l_probe->pfg,
			   _linker_fixup.dentry);
	if (ret != 0) {
		printk(GT_PREFIX "Error register linker fixup probe. "
				 "Linker dentry %s, "
				 "dl_fixup() offset = 0x%lx\n",
				 _linker_fixup.dentry->d_name.name,
				 _linker_fixup.off);
		return ret;
	}

	/* register 'reloc' */
	l_probe->p_reloc.desc = &pin_reloc;
	l_probe->p_reloc.offset = _linker_reloc.off;
	ret = pin_register(&l_probe->p_reloc, l_probe->pfg,
			   _linker_reloc.dentry);
	if (ret != 0) {
		printk(GT_PREFIX "Error register linker reloc probe. "
				  "Linker dentry %s, "
				 "dl_reloc() offset = 0x%lx\n",
				 _linker_reloc.dentry->d_name.name,
				 _linker_reloc.off);

		goto reg_probe_el_fail;
	}

	/* register 'init' */
	l_probe->p_init.desc = &pin_pinit;
	l_probe->p_init.offset = _pthread_init.off;
	ret = pin_register(&l_probe->p_init, l_probe->pfg,
			   _pthread_init.dentry);
	if (ret != 0) {
		printk(GT_PREFIX "Error register pthread minimal init. "
				 "Pthread dentry %s, "
				 "__pthread_minimal_init() offset = 0x%lx\n",
				 _pthread_init.dentry->d_name.name,
				 _pthread_init.off);

		goto reg_probe_pthread_fail;
	}

	return 0;

reg_probe_pthread_fail:
	pin_unregister(&l_probe->p_reloc, l_probe->pfg);

reg_probe_el_fail:
	pin_unregister(&l_probe->p_fixup, l_probe->pfg);

	return ret;
}

static int _unregister_probe_el_no_lock(struct l_probe_el *l_probe)
{
	pin_unregister(&l_probe->p_init, l_probe->pfg);
	pin_unregister(&l_probe->p_reloc, l_probe->pfg);
	pin_unregister(&l_probe->p_fixup, l_probe->pfg);

	return 0;
}

static int _disable_got_patcher(void)
{
	struct l_probe_el *l_probe;
	int ret = 0;

	if (!_is_linker_data_available())
		return -EINVAL;

	_lock_probes_list();

	list_for_each_entry(l_probe, &_reg_probes, list) {
		ret = _unregister_probe_el_no_lock(l_probe);
		if (ret == 0)
			list_move_tail(&l_probe->list, &_unreg_probes);
	}

	_unlock_probes_list();

	_status = GT_OFF;

	return ret;
}

static int _enable_got_patcher(void)
{
	struct l_probe_el *l_probe;
	int ret;

	if (!_is_linker_data_available() || !_is_handler_data_available() ||
	    !_is_pthread_data_available())
		return -EINVAL;

	_lock_probes_list();

	list_for_each_entry(l_probe, &_unreg_probes, list) {
		ret = _register_probe_el_no_lock(l_probe);
		if (ret == 0)
			list_move_tail(&l_probe->list, &_reg_probes);
		else
			goto enable_patcher_fail_unlock;
	}

	_unlock_probes_list();

	_status = GT_ON;

	return 0;
enable_patcher_fail_unlock:
	_unlock_probes_list();

	printk(GT_PREFIX "Error registering linker probes, disabling...");
	_disable_got_patcher();

	return ret;
}


/* ===========================================================================
 * =                           LIST MANIPULATIONS                           =
 * ===========================================================================
 */
static struct l_probe_el *_create_linker_probe_el(void)
{
	struct l_probe_el *l_probe;

	l_probe = kzalloc(sizeof(*l_probe), GFP_KERNEL);
	if (l_probe == NULL)
		return NULL;

	INIT_LIST_HEAD(&l_probe->list);

	return l_probe;
}

static void _destroy_linker_probe_el_no_lock(struct l_probe_el *l_probe)
{
	if (l_probe->pfg)
		put_pf_group(l_probe->pfg);

	if (l_probe->task_id == GT_SET_BY_DENTRY && l_probe->dentry != NULL)
		swap_put_dentry(l_probe->dentry);

	if (l_probe->task_id == GT_SET_BY_ID && l_probe->id != NULL)
		kfree(l_probe->id);

	kfree(l_probe);
}

static void _clean_linker_probes(void)
{
	struct l_probe_el *l_probe, *n;
	int ret;

	_lock_probes_list();

	list_for_each_entry_safe(l_probe, n, &_reg_probes, list) {
		ret = _unregister_probe_el_no_lock(l_probe);
		if (ret != 0)
			printk(GT_PREFIX "Cannot remove linker probe!\n");
		list_del(&l_probe->list);
		_destroy_linker_probe_el_no_lock(l_probe);
	}

	list_for_each_entry_safe(l_probe, n, &_unreg_probes, list) {
		list_del(&l_probe->list);
		_destroy_linker_probe_el_no_lock(l_probe);
	}

	_unlock_probes_list();
}


/* ===========================================================================
 * =                          STRING MANIPULATIONS                           =
 * ===========================================================================
 */

static size_t _get_dentry_len(struct dentry *dentry)
{
	return strnlen(dentry->d_name.name, PATH_MAX);
}

static size_t _get_pid_len(pid_t pid)
{
	/* FIXME Return constant - maximum digits for 10-based unsigned long on
	 * 64 bit architecture to avoid complicated evaluations. */
	 return 20;
}

static size_t _get_id_len(char *id)
{
	return strnlen(id, PATH_MAX);
}

static size_t _get_item_len(struct l_probe_el *l_probe)
{
	if (l_probe->task_id == GT_SET_BY_DENTRY)
		return _get_dentry_len(l_probe->dentry);
	else if (l_probe->task_id == GT_SET_BY_PID)
		return _get_pid_len(l_probe->tgid);
	else if (l_probe->task_id == GT_SET_BY_ID)
		return _get_id_len(l_probe->id);

	printk(GT_PREFIX "Error! Unknown process identificator!\n");
	return 0;
}

static char *_copy_dentry_item(char *dest, struct dentry *dentry)
{
	size_t len;

	len = strnlen(dentry->d_name.name, PATH_MAX);
	memcpy(dest, dentry->d_name.name, len);

	return (dest + len);
}

static char *_copy_pid_item(char *dest, pid_t pid)
{
	int ret;
	size_t len;

	len = _get_pid_len(pid);

	ret = snprintf(dest, len, "%lu", (unsigned long)pid);
	if (ret >= len)
		printk(GT_PREFIX "PID string is truncated!\n");

	return (dest + ret);
}

static char *_copy_id_item(char *dest, char *id)
{
	size_t len;

	len = strnlen(id, PATH_MAX);
	memcpy(dest, id, len);

	return (dest + len);
}

static char *_copy_item(char *dest, struct l_probe_el *l_probe)
{
	if (l_probe->task_id == GT_SET_BY_DENTRY)
		return _copy_dentry_item(dest, l_probe->dentry);
	else if (l_probe->task_id == GT_SET_BY_PID)
		return _copy_pid_item(dest, l_probe->tgid);
	else if (l_probe->task_id == GT_SET_BY_ID)
		return _copy_id_item(dest, l_probe->id);

	printk(GT_PREFIX "Error! Unknown process identificator!\n");
	return dest;
}

static char *_add_separator(char *dest)
{
	const char sep = ':';

	*dest = sep;

	return (dest + 1);
}

static int _add_to_list(struct l_probe_el *l_probe)
{
	int ret = 0;

	_lock_probes_list();
	if (_is_enable()) {
		ret = _register_probe_el_no_lock(l_probe);
		if (ret == 0)
			list_add_tail(&l_probe->list, &_reg_probes);
	} else {
		list_add_tail(&l_probe->list, &_unreg_probes);
	}

	_unlock_probes_list();

	return ret;
}

static int _remove_from_list(cf_t cb, void *data)
{
	struct l_probe_el *l_probe, *n;
	int ret = 0;

	_lock_probes_list();

	list_for_each_entry_safe(l_probe, n, &_reg_probes, list) {
		if (cb(l_probe, data)) {
			ret = _unregister_probe_el_no_lock(l_probe);
			if (ret == 0) {
				list_del(&l_probe->list);
				_destroy_linker_probe_el_no_lock(l_probe);
			}
			goto remove_from_list_unlock;
		}
	}

	list_for_each_entry_safe(l_probe, n, &_unreg_probes, list) {
		if (cb(l_probe, data)) {
			list_del(&l_probe->list);
			_destroy_linker_probe_el_no_lock(l_probe);
			goto remove_from_list_unlock;
		}
	}

remove_from_list_unlock:
	_unlock_probes_list();

	return ret;
}




int gtm_set_linker_path(char *path)
{
	struct dentry *dentry;

	dentry = swap_get_dentry(path);
	if (dentry == NULL)
		return -EINVAL;

	/* We use get_dentry only once, so use put dentry also only once */
	if (_linker_fixup.dentry != NULL ||
	    _linker_reloc.dentry != NULL) {
		if (_linker_fixup.dentry != NULL)
			swap_put_dentry(_linker_fixup.dentry);
		else
			swap_put_dentry(_linker_reloc.dentry);
	}

	_linker_fixup.dentry = dentry;
	_linker_reloc.dentry = dentry;

	return 0;
}

int gtm_set_fixup_off(unsigned long offset)
{
	_linker_fixup.off = offset;

	return 0;
}

int gtm_set_reloc_off(unsigned long offset)
{
	_linker_reloc.off = offset;

	return 0;
}

int gtm_switch(enum gt_status stat)
{
	int ret;

	switch (stat) {
	case GT_ON:
		ret = _enable_got_patcher();
		break;
	case GT_OFF:
		ret = _disable_got_patcher();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

enum gt_status gtm_status(void)
{
	return _status;
}

int gtm_add_by_path(char *path)
{
	struct dentry *dentry;
	struct pf_group *pfg;
	struct l_probe_el *l_probe;
	int ret;

	dentry = swap_get_dentry(path);
	if (dentry == NULL)
		return -EINVAL;

	pfg = get_pf_group_by_dentry(dentry, dentry);
	if (pfg == NULL) {
		ret = -ENOMEM;
		goto add_by_path_put_dentry;
	}

	l_probe = _create_linker_probe_el();
	if (l_probe == NULL) {
		ret = -ENOMEM;
		goto add_by_path_put_pfg;
	}

	l_probe->pfg = pfg;
	l_probe->task_id = GT_SET_BY_DENTRY;
	l_probe->dentry = dentry;

	ret = _add_to_list(l_probe);
	if (ret != 0)
		goto add_by_path_free;

	return 0;

add_by_path_free:
	kfree(l_probe);

add_by_path_put_pfg:
	put_pf_group(pfg);

add_by_path_put_dentry:
	swap_put_dentry(dentry);

	return ret;
}

int gtm_add_by_pid(pid_t pid)
{
	struct pf_group *pfg;
	struct l_probe_el *l_probe;
	int ret;

	/* TODO Add dentry as private data */
	pfg = get_pf_group_by_tgid(pid, NULL);
	if (pfg == NULL)
		return -ENOMEM;

	l_probe = _create_linker_probe_el();
	if (l_probe == NULL) {
		ret = -ENOMEM;
		goto add_by_pid_put_pfg;
	}

	l_probe->pfg = pfg;
	l_probe->task_id = GT_SET_BY_PID;
	l_probe->tgid = pid;

	ret = _add_to_list(l_probe);
	if (ret != 0)
		goto add_by_pid_free;

	return 0;

add_by_pid_free:
	kfree(l_probe);

add_by_pid_put_pfg:
	put_pf_group(pfg);

	return ret;
}

int gtm_add_by_id(char *id)
{
	char *new_id;
	struct pf_group *pfg;
	struct l_probe_el *l_probe;
	size_t len;
	int ret;

	len = strnlen(id, PATH_MAX);
	new_id = kmalloc(len, GFP_KERNEL);
	if (new_id == NULL)
		return -ENOMEM;

	/* TODO Add dentry as private data */
	pfg = get_pf_group_by_comm(id, NULL);
	if (pfg == NULL) {
		ret = -ENOMEM;
		goto add_by_id_free_id;
	}

	l_probe = _create_linker_probe_el();
	if (l_probe == NULL) {
		ret = -ENOMEM;
		goto add_by_id_put_pfg;
	}

	l_probe->pfg = pfg;
	l_probe->task_id = GT_SET_BY_ID;
	l_probe->id = new_id;

	ret = _add_to_list(l_probe);
	if (ret != 0)
		goto add_by_id_free;

	return 0;

add_by_id_free:
	kfree(l_probe);

add_by_id_put_pfg:
	put_pf_group(pfg);

add_by_id_free_id:
	kfree(new_id);

	return ret;
}

int gtm_del_by_path(char *path)
{
	struct dentry *dentry;
	int ret = 0;

	dentry = swap_get_dentry(path);
	if (dentry == NULL)
		return -EINVAL;

	ret = _remove_from_list(_check_by_dentry, dentry);

	return ret;
}

int gtm_del_by_pid(pid_t pid)
{
	int ret = 0;

	ret = _remove_from_list(_check_by_pid, &pid);

	return ret;
}

int gtm_del_by_id(char *id)
{
	int ret = 0;

	ret = _remove_from_list(_check_by_id, id);

	return ret;
}

int gtm_del_all(void)
{
	int ret = 0;

	ret = _remove_from_list(_del_all, NULL);

	return ret;
}

ssize_t gtm_get_targets(char **targets)
{
	struct l_probe_el *l_probe;
	char *ptr;
	size_t len = 0;

	_lock_probes_list();
	list_for_each_entry(l_probe, &_reg_probes, list) {
		/* Add separator */
		len += _get_item_len(l_probe) + 1;
	}

	list_for_each_entry(l_probe, &_unreg_probes, list) {
		/* Add separator */
		len += _get_item_len(l_probe) + 1;
	}
	_unlock_probes_list();

	*targets = kmalloc(len, GFP_KERNEL);
	if (*targets == NULL)
		return -ENOMEM;

	ptr = *targets;

	_lock_probes_list();

	list_for_each_entry(l_probe, &_reg_probes, list) {
		ptr = _copy_item(ptr, l_probe);
		ptr = _add_separator(ptr);
	}

	list_for_each_entry(l_probe, &_unreg_probes, list) {
		ptr = _copy_item(ptr, l_probe);
		ptr = _add_separator(ptr);
	}

	_unlock_probes_list();

	*targets[len - 1] = '\0';

	return len;
}

int gtm_set_handler_path(char *path)
{
	struct dentry *dentry;
	int ret;

	/* We use get_dentry only once, so use put dentry also only once */
	dentry = swap_get_dentry(path);
	if (dentry == NULL)
		return -EINVAL;

	if (_handler_fixup.dentry != NULL ||
	    _handler_reloc.dentry != NULL) {
		if (_handler_fixup.dentry != NULL)
			swap_put_dentry(_handler_fixup.dentry);
		else
			swap_put_dentry(_handler_reloc.dentry);
	}

	_handler_fixup.dentry = dentry;
	_handler_reloc.dentry = dentry;

	/* TODO Do smth with this:
	 * make interface for loader to remove handlers
	 * and add/delete when gtm_set_handler() is called
	 * Check container_of() may be useful */
	ret = loader_add_handler(path);

	return ret;
}

int gtm_set_handler_fixup_off(unsigned long offset)
{
	_handler_fixup.off = offset;

	return 0;
}

int gtm_set_handler_reloc_off(unsigned long offset)
{
	_handler_reloc.off = offset;

	return 0;
}

int gtm_set_pthread_path(char *path)
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

int gtm_set_init_off(unsigned long offset)
{
	_pthread_init.off = offset;

	return 0;
}

static int gtm_init(void)
{
	int ret;

	ret = gtd_init();
	if (ret)
		goto gtd_init_err;

	ret = swap_ktd_reg(&gt_ktd);
	if (ret)
		goto gtd_ktd_reg_err;

	return 0;

gtd_ktd_reg_err:
	gtd_exit();

gtd_init_err:
	return ret;
}

static void gtm_exit(void)
{
	swap_ktd_unreg(&gt_ktd);
	gtd_exit();

	_clean_linker_probes();

	/* We use get_dentry only once, so use put dentry also only once */
	if (_handler_fixup.dentry != NULL ||
	    _handler_reloc.dentry != NULL) {
		if (_handler_fixup.dentry != NULL)
			swap_put_dentry(_handler_fixup.dentry);
		else
			swap_put_dentry(_handler_reloc.dentry);
		_handler_reloc.dentry = NULL;
		_handler_fixup.dentry = NULL;
	}
}

SWAP_LIGHT_INIT_MODULE(NULL, gtm_init, gtm_exit, NULL, NULL);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP GOT Patcher Module");
MODULE_AUTHOR("Alexander Aksenov <a.aksenov@samsung.com>");
