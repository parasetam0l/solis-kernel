#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/namei.h>
#include <linux/mutex.h>
#include <kprobe/swap_kprobes_deps.h>
#include <writer/kernel_operations.h>
#include <writer/swap_msg.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/sspt/sspt_page.h>
#include <us_manager/sspt/sspt_file.h>
#include <us_manager/us_common_file.h>
#include <loader/loader.h>
#include <master/swap_initializer.h>
#include "preload.h"
#include "preload_module.h"
#include "preload_debugfs.h"
#include "preload_control.h"
#include "preload_threads.h"
#include "preload_process.h"

enum {
	/* task preload flags */
	HANDLER_RUNNING = 0x1
};

struct user_ptrs {
	char *caller;
	char *orig;
	char *call_type;
};

static enum preload_status _status = PRELOAD_OFF;
static DEFINE_MUTEX(status_change);


static inline bool _is_enable_no_lock(void)
{
	return _status;
}

static inline struct vm_area_struct *__get_vma_by_addr(struct task_struct *task,
						       unsigned long caddr)
{
	struct vm_area_struct *vma = NULL;

	if ((task == NULL) || (task->mm == NULL))
		return NULL;
	vma = find_vma_intersection(task->mm, caddr, caddr + 1);

	return vma;
}

static inline bool __is_probe_non_block(struct sspt_ip *ip)
{
	if (ip->desc->info.pl_i.flags & SWAP_PRELOAD_NON_BLOCK_PROBE)
		return true;

	return false;
}

static inline bool __inverted(struct sspt_ip *ip)
{
	unsigned long flags = ip->desc->info.pl_i.flags;

	if (flags & SWAP_PRELOAD_INVERTED_PROBE)
		return true;

	return false;
}

static inline bool __check_flag_and_call_type(struct sspt_ip *ip,
					      enum preload_call_type ct)
{
	bool inverted = __inverted(ip);

	if (ct != NOT_INSTRUMENTED || inverted)
		return true;

	return false;
}

static inline bool __is_handlers_call(struct vm_area_struct *caller,
				      struct pd_t *pd)
{
	struct hd_t *hd;

	if (caller == NULL || caller->vm_file == NULL ||
	    caller->vm_file->f_path.dentry == NULL) {
		return false;
	}

	hd = lpd_get_hd(pd, caller->vm_file->f_path.dentry);
	if (hd != NULL)
		return true;

	return false;
}

static inline bool __should_drop(struct sspt_ip *ip, enum preload_call_type ct)
{
	if (ct == NOT_INSTRUMENTED)
		return true;

	return false;
}

static inline int __msg_sanitization(char *user_msg, size_t len,
				     struct user_ptrs *ptrs)
{
	if (ptrs->caller &&
	    (ptrs->caller < user_msg || ptrs->caller > user_msg + len))
		return -EINVAL;

	if (ptrs->orig &&
	    (ptrs->orig < user_msg || ptrs->orig > user_msg + len))
		return -EINVAL;

	if (ptrs->call_type &&
	    (ptrs->call_type < user_msg || ptrs->call_type > user_msg + len))
		return -EINVAL;

	return 0;
}




static unsigned long __do_preload_entry(struct uretprobe_instance *ri,
					struct pt_regs *regs,
					struct hd_t *hd)
{
	struct sspt_ip *ip = container_of(ri->rp, struct sspt_ip, retprobe);
	unsigned long offset = ip->desc->info.pl_i.handler;
	unsigned long vaddr = 0;
	unsigned long base;
	struct vm_area_struct *cvma;
	struct pd_t *pd;
	struct pt_data_t ptd;

	base = lpd_get_handlers_base(hd);
	if (base == 0)
		return 0;	/* handlers isn't mapped */

	/* jump to preloaded handler */
	vaddr = base + offset;
	ptd.caller = get_regs_ret_func(regs);
	cvma = __get_vma_by_addr(current, ptd.caller);
	ptd.call_type = pc_call_type(ip, (void *)ptd.caller);
	ptd.disable_addr = __is_probe_non_block(ip) ? ip->orig_addr : 0;
	ptd.orig = ip->orig_addr;
	pd = lpd_get_parent_pd(hd);

	/* jump only if caller is instumented and it is not a system lib -
	 * this leads to some errors
	 */
	if (cvma != NULL && cvma->vm_file != NULL &&
	    !pc_check_dentry_is_ignored(cvma->vm_file->f_path.dentry) &&
	    __check_flag_and_call_type(ip, ptd.call_type) &&
	    !__is_handlers_call(cvma, pd)) {

		ptd.drop = __should_drop(ip, ptd.call_type);
		if (pt_set_data(current, &ptd) != 0)
			printk(PRELOAD_PREFIX "Error! Failed to store data for %d/%d\n",
			       current->tgid, current->pid);
		/* args are not changed */
		loader_module_prepare_ujump(ri, regs, vaddr);
		if (ptd.disable_addr == 0)
			pt_set_flags(current, HANDLER_RUNNING);
	}

	return vaddr;
}

static int preload_us_entry(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct pd_t *pd = lpd_get_by_task(current);
	struct hd_t *hd;
	unsigned long old_pc = swap_get_upc(regs);
	unsigned long flags = pt_get_flags(current);
	struct sspt_ip *ip = container_of(ri->rp, struct sspt_ip, retprobe);
	unsigned long vaddr = 0;
	struct dentry *dentry = ip->desc->info.pl_i.dentry;

	if (dentry == NULL)
		goto out_set_orig;

	if ((flags & HANDLER_RUNNING) ||
	    pt_check_disabled_probe(current, ip->orig_addr))
		goto out_set_orig;

	hd = lpd_get_hd(pd, dentry);
	if (hd == NULL)
		goto out_set_orig;

	if ((lpd_get_state(hd) == NOT_LOADED ||
	    lpd_get_state(hd) == FAILED) && lpd_get_init_state(pd))
		vaddr = loader_not_loaded_entry(ri, regs, pd, hd);
	else if (lpd_get_state(hd) == LOADED)
		vaddr =__do_preload_entry(ri, regs, hd);

out_set_orig:
	loader_set_priv_origin(ri, vaddr);

	/* PC change check */
	return old_pc != swap_get_upc(regs);
}

static void __do_preload_ret(struct uretprobe_instance *ri, struct hd_t *hd)
{
	struct sspt_ip *ip = container_of(ri->rp, struct sspt_ip, retprobe);
	unsigned long flags = pt_get_flags(current);
	unsigned long offset = ip->desc->info.pl_i.handler;
	unsigned long vaddr = 0;

	if ((flags & HANDLER_RUNNING) ||
	    pt_check_disabled_probe(current, ip->orig_addr)) {
		bool non_blk_probe = __is_probe_non_block(ip);

		/* drop the flag if the handler has completed */
		vaddr = lpd_get_handlers_base(hd) + offset;
		if (vaddr && (loader_get_priv_origin(ri) == vaddr)) {
			if (pt_put_data(current) != 0)
				printk(PRELOAD_PREFIX "Error! Failed to put "
				       "caller slot for %d/%d\n", current->tgid,
				       current->pid);
			if (!non_blk_probe) {
				flags &= ~HANDLER_RUNNING;
				pt_set_flags(current, flags);
			}
		}
	}
}

static int preload_us_ret(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct pd_t *pd = lpd_get_by_task(current);
	struct sspt_ip *ip = container_of(ri->rp, struct sspt_ip, retprobe);
	struct dentry *dentry = ip->desc->info.pl_i.dentry;
	struct hd_t *hd;

	if (dentry == NULL)
		return 0;

	hd = lpd_get_hd(pd, dentry);
	if (hd == NULL)
		return 0;

	switch (lpd_get_state(hd)) {
	case NOT_LOADED:
		/* loader has not yet been mapped... just ignore */
		break;
	case LOADING:
		loader_loading_ret(ri, regs, pd, hd);
		break;
	case LOADED:
		__do_preload_ret(ri, hd);
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





static void __write_data_on_demand(char *user, char *msg, size_t len,
				   struct user_ptrs *ptrs,
				   unsigned long caller_addr)
{
	unsigned long caller_ptr = 0, orig_ptr = 0, ct_ptr = 0;
	unsigned long caller = 0, orig = 0;
	unsigned char call_type = 0;
	int ret;

	/* Evaluate addresses whereto write data as:
	 * pointer from user - user buffer beggining + kernel buffer beginning
	 *  If incoming pointer is NULL - do not write
	 */

	if (ptrs->orig) {
		orig_ptr = (unsigned long)(ptrs->orig - user + msg);

		ret = pt_get_orig(current, &orig);
		if (ret) {
			orig = 0xbadbeef;
			printk(PRELOAD_PREFIX "No orig for %d/%d\n",
			       current->tgid, current->pid);
		}

		/* Types should be the same as in preload lib!!! */
		*(uint64_t *)orig_ptr = (uint64_t)orig;
	}

	if (caller_addr && ptrs->caller && ptrs->call_type) {
		caller_ptr = (unsigned long)(ptrs->caller - user + msg);
		ct_ptr = (unsigned long)(ptrs->call_type - user + msg);

		caller = caller_addr;
		call_type =
		    pc_call_type_always_inst((void *)caller);

		/* Types should be the same as in preload lib!!! */
		*(uint64_t *)caller_ptr = (uint64_t)caller;
		*(uint32_t *)ct_ptr = (uint32_t)call_type;

		return;
	}

	if (ptrs->caller) {
		caller_ptr = (unsigned long)(ptrs->caller - user + msg);

		ret = pt_get_caller(current, &caller);
		if (ret) {
			caller = 0xbadbeef;
			printk(PRELOAD_PREFIX "No caller for %d/%d\n",
			       current->tgid, current->pid);
		}

		/* Types should be the same as in preload lib!!! */
		*(uint64_t *)caller_ptr = (uint64_t)caller;
	}

	if (ptrs->call_type) {
		ct_ptr = (unsigned long)(ptrs->call_type - user + msg);

		ret = pt_get_call_type(current, &call_type);
		if (ret) {
			call_type = 0xff;
			printk(PRELOAD_PREFIX "No call type for %d/%d\n",
			       current->tgid, current->pid);
		}

		/* Types should be the same as in preload lib!!! */
		*(uint32_t *)ct_ptr = (uint32_t)call_type;
	}
}

static int write_msg_handler(struct uprobe *p, struct pt_regs *regs)
{
	struct user_ptrs ptrs;
	char *user_buf, *buf;
	size_t len;
	unsigned long caller_addr;
	int ret;

	/* FIXME: swap_get_uarg uses get_user(), it might sleep */
	user_buf = (char *)swap_get_uarg(regs, 0);
	len = swap_get_uarg(regs, 1);
	ptrs.call_type = (char *)swap_get_uarg(regs, 2);
	ptrs.caller = (char *)swap_get_uarg(regs, 3);
	caller_addr = swap_get_uarg(regs, 4);
	ptrs.orig = (char *)swap_get_uarg(regs, 5);

	if (ptrs.caller || ptrs.call_type || ptrs.orig) {
		ret = __msg_sanitization(user_buf, len, &ptrs);
		if (ret != 0) {
			printk(PRELOAD_PREFIX "Invalid message pointers!\n");
			return 0;
		}
		ret = pt_get_drop(current);
		if (ret > 0)
			return 0;
	}

	buf = kmalloc(len, GFP_ATOMIC);
	if (buf == NULL) {
		printk(PRELOAD_PREFIX "No mem for buffer! Size = %zd\n", len);
		return 0;
	}

	ret = read_proc_vm_atomic(current, (unsigned long)user_buf, buf, len);
	if (ret < 0) {
		printk(PRELOAD_PREFIX "Cannot copy data from userspace! Size = "
				      "%zd ptr 0x%lx ret %d\n", len,
				      (unsigned long)user_buf, ret);
		goto write_msg_fail;
	}

	__write_data_on_demand(user_buf, buf, len, &ptrs, caller_addr);

	ret = swap_msg_raw(buf, len);
	if (ret != len)
		printk(PRELOAD_PREFIX "Error writing probe lib message\n");

write_msg_fail:
	kfree(buf);

	return 0;
}

static int get_caller_handler(struct uprobe *p, struct pt_regs *regs)
{
	unsigned long caller;
	int ret;

	ret = pt_get_caller(current, &caller);
	if (ret != 0) {
		caller = 0xbadbeef;
		printk(PRELOAD_PREFIX "Error! Cannot get caller address for "
		       "%d/%d\n", current->tgid, current->pid);
	}

	swap_put_uarg(regs, 0, caller);

	return 0;
}

static int get_call_type_handler(struct uprobe *p, struct pt_regs *regs)
{
	unsigned char call_type;
	int ret;

	ret = pt_get_call_type(current, &call_type);
	if (ret != 0) {
		call_type = 0xff;
		printk(PRELOAD_PREFIX "Error! Cannot get call type for %d/%d\n",
		       current->tgid, current->pid);
	}

	swap_put_uarg(regs, 0, call_type);

	return 0;
}





int pm_get_caller_init(struct sspt_ip *ip)
{
	struct uprobe *up = &ip->uprobe;

	up->pre_handler = get_caller_handler;

	return 0;
}

void pm_get_caller_exit(struct sspt_ip *ip)
{
}

int pm_get_call_type_init(struct sspt_ip *ip)
{
	struct uprobe *up = &ip->uprobe;

	up->pre_handler = get_call_type_handler;

	return 0;
}

void pm_get_call_type_exit(struct sspt_ip *ip)
{
}

int pm_write_msg_init(struct sspt_ip *ip)
{
	struct uprobe *up = &ip->uprobe;

	up->pre_handler = write_msg_handler;

	return 0;
}

void pm_write_msg_exit(struct sspt_ip *ip)
{
}



int pm_uprobe_init(struct sspt_ip *ip)
{
	struct uretprobe *rp = &ip->retprobe;
	struct dentry *dentry;
	const char *path = ip->desc->info.pl_i.path;

	rp->entry_handler = preload_us_entry;
	rp->handler = preload_us_ret;

	/* Get dentry and set it in probe info struct */
	dentry = swap_get_dentry(path);
	if (dentry == NULL) {
		pr_warn(PRELOAD_PREFIX "Error! Cannot get handler %s\n", path);
		return -EINVAL;
	}
	ip->desc->info.pl_i.dentry = dentry;

	/* Add handler to loader */
	loader_add_handler(path);

	/* FIXME actually additional data_size is needed only when we jump
	 * to dlopen */
	loader_set_rp_data_size(rp);

	return 0;
}

void pm_uprobe_exit(struct sspt_ip *ip)
{
	struct dentry *dentry = ip->desc->info.pl_i.dentry;

	WARN_ON(!dentry);

	if (dentry)
		swap_put_dentry(dentry);
}

int pm_switch(enum preload_status stat)
{
	int ret = 0;

	mutex_lock(&status_change);
	switch (stat) {
	case PRELOAD_ON:
		if (_is_enable_no_lock())
			goto pm_switch_unlock;

		ret = pp_enable();
		if (!ret)
			_status = PRELOAD_ON;
		break;
	case PRELOAD_OFF:
		if (!_is_enable_no_lock())
			goto pm_switch_unlock;

		pp_disable();
		_status = PRELOAD_OFF;
		break;
	default:
		ret = -EINVAL;
	}

pm_switch_unlock:
	mutex_unlock(&status_change);

	return ret;
}

enum preload_status pm_status(void)
{
	enum preload_status s;

	mutex_lock(&status_change);
	s = _status;
	mutex_unlock(&status_change);

	return s;
}

static int pm_init(void)
{
	int ret;

	ret = pd_init();
	if (ret)
		goto out_err;

	ret = pc_init();
	if (ret)
		goto control_init_fail;

	ret = pt_init();
	if (ret)
		goto threads_init_fail;

	ret = register_preload_probes();
	if (ret)
		goto probes_register_fail;

	return 0;

probes_register_fail:
	pt_exit();

threads_init_fail:
	pc_exit();

control_init_fail:
	pd_exit();

out_err:
	return ret;
}

static void pm_exit(void)
{
	unregister_preload_probes();
	pt_exit();
	pc_exit();
	pd_exit();
}

SWAP_LIGHT_INIT_MODULE(NULL, pm_init, pm_exit, NULL, NULL);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP Preload Module");
MODULE_AUTHOR("Alexander Aksenov <a.aksenov@samsung.com>");
