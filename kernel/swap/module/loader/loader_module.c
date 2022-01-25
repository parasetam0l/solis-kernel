#include <linux/module.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mman.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <us_manager/sspt/sspt_proc.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/callbacks.h>
#include <us_manager/usm_hook.h>
#include <writer/kernel_operations.h>
#include <master/swap_initializer.h>
#include "loader_defs.h"
#include "loader_debugfs.h"
#include "loader_module.h"
#include "loader.h"
#include "loader_storage.h"
#include "loader_pd.h"


struct us_priv {
	struct pt_regs regs;
	unsigned long arg0;
	unsigned long arg1;
	unsigned long raddr;
	unsigned long origin;
};

static atomic_t dentry_balance = ATOMIC_INIT(0);

enum loader_status_t {
	SWAP_LOADER_NOT_READY = 0,
	SWAP_LOADER_READY = 1,
	SWAP_LOADER_RUNNING = 2
};

static enum loader_status_t __loader_status = SWAP_LOADER_NOT_READY;

static int __loader_cbs_start_h = -1;
static int __loader_cbs_stop_h = -1;



bool loader_module_is_running(void)
{
	if (__loader_status == SWAP_LOADER_RUNNING)
		return true;

	return false;
}

bool loader_module_is_ready(void)
{
	if (__loader_status == SWAP_LOADER_READY)
		return true;

	return false;
}

bool loader_module_is_not_ready(void)
{
	if (__loader_status == SWAP_LOADER_NOT_READY)
		return true;

	return false;
}

void loader_module_set_ready(void)
{
	__loader_status = SWAP_LOADER_READY;
}

void loader_module_set_running(void)
{
	__loader_status = SWAP_LOADER_RUNNING;
}

void loader_module_set_not_ready(void)
{
	__loader_status = SWAP_LOADER_NOT_READY;
}

static inline void __prepare_ujump(struct uretprobe_instance *ri,
				   struct pt_regs *regs,
				   unsigned long vaddr)
{
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
	ri->preload.use = true;
	ri->preload.thumb = !!thumb_mode(regs);
#endif /* defined(CONFIG_ARM) || defined(CONFIG_ARM64) */

	swap_set_upc(regs, vaddr);
}

static inline void __save_uregs(struct uretprobe_instance *ri,
				struct pt_regs *regs)
{
	struct us_priv *priv = (struct us_priv *)ri->data;

	priv->regs = *regs;
	priv->arg0 = swap_get_uarg(regs, 0);
	priv->arg1 = swap_get_uarg(regs, 1);
	priv->raddr = swap_get_uret_addr(regs);
}

static inline void __restore_uregs(struct uretprobe_instance *ri,
				   struct pt_regs *regs)
{
	struct us_priv *priv = (struct us_priv *)ri->data;

	*regs = priv->regs;
	swap_put_uarg(regs, 0, priv->arg0);
	swap_put_uarg(regs, 1, priv->arg1);
	swap_set_uret_addr(regs, priv->raddr);
#ifdef CONFIG_X86_32
	/* need to do it only on x86 */
	regs->EREG(ip) -= 1;
#endif /* CONFIG_X86_32 */
	/* we have just restored the registers => no need to do it in
	 * trampoline_uprobe_handler */
	ri->ret_addr = NULL;
}

static inline void print_regs(const char *prefix, struct pt_regs *regs,
			      struct uretprobe_instance *ri, struct hd_t *hd)
{
	struct dentry *dentry = lpd_get_dentry(hd);

#if defined(CONFIG_ARM)
	printk(LOADER_PREFIX "%s[%d/%d] %s (%d) %s addr(%08lx), "
	       "r0(%08lx), r1(%08lx), r2(%08lx), r3(%08lx), "
	       "r4(%08lx), r5(%08lx), r6(%08lx), r7(%08lx), "
	       "sp(%08lx), lr(%08lx), pc(%08lx)\n",
	       current->comm, current->tgid, current->pid,
	       dentry != NULL ? (char *)(dentry->d_name.name) :
				(char *)("NULL"),
	       (int)lpd_get_state(hd),
	       prefix, (unsigned long)ri->rp->up.addr,
	       regs->ARM_r0, regs->ARM_r1, regs->ARM_r2, regs->ARM_r3,
	       regs->ARM_r4, regs->ARM_r5, regs->ARM_r6, regs->ARM_r7,
	       regs->ARM_sp, regs->ARM_lr, regs->ARM_pc);
#elif defined(CONFIG_X86_32)
	printk(LOADER_PREFIX "%s[%d/%d] %s (%d) %s addr(%08lx), "
	       "ip(%08lx), arg0(%08lx), arg1(%08lx), raddr(%08lx)\n",
	       current->comm, current->tgid, current->pid,
	       dentry != NULL ? (char *)(dentry->d_name.name) :
				(char *)("NULL"),
	       (int)lpd_get_state(hd),
	       prefix, (unsigned long)ri->rp->up.addr,
	       regs->EREG(ip), swap_get_uarg(regs, 0), swap_get_uarg(regs, 1),
	       swap_get_uret_addr(regs));
#elif defined(CONFIG_ARM64)
	printk(LOADER_PREFIX "%s[%d/%d] %s (%d) %s addr(%016lx), "
	       "x0(%016lx), x1(%016lx), x2(%016lx), x3(%016lx), "
	       "x4(%016lx), x5(%016lx), x6(%016lx), x7(%016lx), "
	       "sp(%016lx), lr(%016lx), pc(%016lx)\n",
	       current->comm, current->tgid, current->pid,
	       dentry != NULL ? (char *)(dentry->d_name.name) :
				(char *)("NULL"),
	       (int)lpd_get_state(hd),
	       prefix, (unsigned long)ri->rp->up.addr,
	       (long)regs->regs[0], (long)regs->regs[1],
	       (long)regs->regs[2], (long)regs->regs[3],
	       (long)regs->regs[4], (long)regs->regs[5],
	       (long)regs->regs[6], (long)regs->regs[7],
	       (long)regs->sp, swap_get_uret_addr(regs), (long)regs->pc);
	(void)dentry;
#else /* CONFIG_arch */
# error "this architecture is not supported"
#endif /* CONFIG_arch */
}

static inline unsigned long get_r_state_addr(struct vm_area_struct *linker_vma)
{
	unsigned long start_addr;
	unsigned long offset = ld_r_state_offset();

	if (linker_vma == NULL)
		return 0;

	start_addr = linker_vma->vm_start;

	return (offset ? start_addr + offset : 0);
}

static struct vm_area_struct *__get_linker_vma(struct task_struct *task)
{
	struct vm_area_struct *vma = NULL;
	struct bin_info *ld_info;

	ld_info = ls_get_linker_info();
	if (ld_info == NULL) {
		printk(LOADER_PREFIX "Cannot get linker info [%u %u %s]!\n",
		       task->tgid, task->pid, task->comm);
		return NULL;
	}

	for (vma = task->mm->mmap; vma; vma = vma->vm_next) {
		if (vma->vm_file && vma->vm_flags & VM_EXEC
		    && vma->vm_file->f_path.dentry == ld_info->dentry) {
				ls_put_linker_info(ld_info);
				return vma;
		}
	}

	ls_put_linker_info(ld_info);
	return NULL;
}









static bool __is_proc_mmap_mappable(struct task_struct *task)
{
	struct vm_area_struct *linker_vma = __get_linker_vma(task);
	struct sspt_proc *proc;
	unsigned long r_state_addr;
	unsigned int state;

	if (linker_vma == NULL)
		return false;

	r_state_addr = get_r_state_addr(linker_vma);
	if (r_state_addr == 0)
		return false;

	proc = sspt_proc_get_by_task(task);
	if (proc) {
		proc->r_state_addr = r_state_addr;
		sspt_proc_put(proc);
	}

	if (get_user(state, (unsigned long *)r_state_addr))
		return false;

	return !state;
}

static bool __should_we_load_handlers(struct task_struct *task,
					 struct pt_regs *regs)
{
	if (!__is_proc_mmap_mappable(task))
		return false;

	return true;
}





static void mmap_handler(struct sspt_proc *proc, struct vm_area_struct *vma)
{
	struct pd_t *pd;
	unsigned long vaddr = vma->vm_start;
	struct dentry *dentry = vma->vm_file->f_path.dentry;

	pd = lpd_get(proc);
	if (pd == NULL) {
		printk(LOADER_PREFIX "%d: No process data! Current %d %s\n",
		       __LINE__, current->tgid, current->comm);
		return;
	}

	if (dentry == ld_get_loader_dentry()) {
		lpd_set_loader_base(pd, vaddr);
	} else {
		struct hd_t *hd;

		hd = lpd_get_hd(pd, dentry);
		if (hd)
			lpd_set_handlers_base(hd, vaddr);
	}
}


static struct usm_hook usm_hook = {
	.owner = THIS_MODULE,
	.mmap = mmap_handler,
};

static bool mmap_rp_inst = false;
static DEFINE_MUTEX(mmap_rp_mtx);

static void loader_start_cb(void)
{
	int res;

	mutex_lock(&mmap_rp_mtx);
	res = usm_hook_reg(&usm_hook);
	if (res != 0)
		pr_err(LOADER_PREFIX "Cannot register usm_hook\n");
	else
		mmap_rp_inst = true;
	mutex_unlock(&mmap_rp_mtx);
}

static void loader_stop_cb(void)
{
	mutex_lock(&mmap_rp_mtx);
	if (mmap_rp_inst) {
		usm_hook_unreg(&usm_hook);
		mmap_rp_inst = false;
	}
	mutex_unlock(&mmap_rp_mtx);
}

static unsigned long __not_loaded_entry(struct uretprobe_instance *ri,
					struct pt_regs *regs,
					struct pd_t *pd, struct hd_t *hd)
{
	char __user *path = NULL;
	unsigned long vaddr = 0;
	unsigned long base;

	/* if linker is still doing its work, we do nothing */
	if (!__should_we_load_handlers(current, regs))
		return 0;

	base = lpd_get_loader_base(pd);
	if (base == 0)
		return 0;   /* loader isn't mapped */

	/* jump to loader code if ready */
	vaddr = base + ld_get_loader_offset();
	if (vaddr) {
		/* save original regs state */
		__save_uregs(ri, regs);
		print_regs("ORIG", regs, ri, hd);

		path = lpd_get_path(pd, hd);

		/* set dlopen args: filename, flags */
		swap_put_uarg(regs, 0, (unsigned long)path);
		swap_put_uarg(regs, 1, 2 /* RTLD_NOW */);

		/* do the jump to dlopen */
		__prepare_ujump(ri, regs, vaddr);
		/* set new state */
		lpd_set_state(hd, LOADING);
	}

	return vaddr;
}

static void __loading_ret(struct uretprobe_instance *ri, struct pt_regs *regs,
			  struct pd_t *pd, struct hd_t *hd)
{
	struct us_priv *priv = (struct us_priv *)ri->data;
	unsigned long vaddr = 0;

	/* check if loading has been completed */
	vaddr = lpd_get_loader_base(pd) +
		ld_get_loader_offset();
	if (vaddr && (priv->origin == vaddr)) {
		lpd_set_handle(hd,
				      (void __user *)regs_return_value(regs));

		/* restore original regs state */
		__restore_uregs(ri, regs);
		print_regs("REST", regs, ri, hd);
		/* check if loading done */

		if (lpd_get_handle(hd)) {
			lpd_set_state(hd, LOADED);
		} else {
			lpd_dec_attempts(hd);
			lpd_set_state(hd, FAILED);
		}
	}
}

static void __failed_ret(struct uretprobe_instance *ri, struct pt_regs *regs,
			 struct pd_t *pd, struct hd_t *hd)
{
	if (lpd_get_attempts(hd))
		lpd_set_state(hd, NOT_LOADED);
}



unsigned long loader_not_loaded_entry(struct uretprobe_instance *ri,
				       struct pt_regs *regs, struct pd_t *pd,
				       struct hd_t *hd)
{
	return __not_loaded_entry(ri, regs, pd, hd);
}
EXPORT_SYMBOL_GPL(loader_not_loaded_entry);

void loader_loading_ret(struct uretprobe_instance *ri, struct pt_regs *regs,
			 struct pd_t *pd, struct hd_t *hd)
{
	__loading_ret(ri, regs, pd, hd);
}
EXPORT_SYMBOL_GPL(loader_loading_ret);

void loader_failed_ret(struct uretprobe_instance *ri, struct pt_regs *regs,
			struct pd_t *pd, struct hd_t *hd)
{
	__failed_ret(ri, regs, pd, hd);
}
EXPORT_SYMBOL_GPL(loader_failed_ret);

void loader_module_prepare_ujump(struct uretprobe_instance *ri,
				  struct pt_regs *regs, unsigned long addr)
{
	__prepare_ujump(ri, regs, addr);
}
EXPORT_SYMBOL_GPL(loader_module_prepare_ujump);

void loader_set_rp_data_size(struct uretprobe *rp)
{
	rp->data_size = sizeof(struct us_priv);
}
EXPORT_SYMBOL_GPL(loader_set_rp_data_size);

void loader_set_priv_origin(struct uretprobe_instance *ri, unsigned long addr)
{
	struct us_priv *priv = (struct us_priv *)ri->data;

	priv->origin = addr;
}
EXPORT_SYMBOL_GPL(loader_set_priv_origin);

unsigned long loader_get_priv_origin(struct uretprobe_instance *ri)
{
	struct us_priv *priv = (struct us_priv *)ri->data;

	return priv->origin;
}
EXPORT_SYMBOL_GPL(loader_get_priv_origin);


int loader_set(void)
{
	if (loader_module_is_running())
		return -EBUSY;

	return 0;
}

void loader_unset(void)
{
	mutex_lock(&mmap_rp_mtx);
	if (mmap_rp_inst) {
		usm_hook_unreg(&usm_hook);
		mmap_rp_inst = false;
	}
	mutex_unlock(&mmap_rp_mtx);

	/*module_put(THIS_MODULE);*/
	loader_module_set_not_ready();
}

int loader_add_handler(const char *path)
{
	return ls_add_handler(path);
}
EXPORT_SYMBOL_GPL(loader_add_handler);


static int loader_module_init(void)
{
	int ret;

	ret = ld_init();
	if (ret)
		goto out_err;

	ret = ls_init();
	if (ret)
		goto exit_debugfs;

	ret = lpd_init();
	if (ret)
		goto exit_storage;

	/* TODO do not forget to remove set (it is just for debugging) */
	ret = loader_set();
	if (ret)
		goto exit_pd;

	__loader_cbs_start_h = us_manager_reg_cb(START_CB, loader_start_cb);
	if (__loader_cbs_start_h < 0)
		goto exit_start_cb;

	__loader_cbs_stop_h = us_manager_reg_cb(STOP_CB, loader_stop_cb);
	if (__loader_cbs_stop_h < 0)
		goto exit_stop_cb;

	return 0;

exit_stop_cb:
	us_manager_unreg_cb(__loader_cbs_start_h);

exit_start_cb:
	loader_unset();

exit_pd:
	lpd_uninit();

exit_storage:
	ls_exit();

exit_debugfs:
	ld_exit();

out_err:
	return ret;
}

static void loader_module_exit(void)
{
	int balance;

	us_manager_unreg_cb(__loader_cbs_start_h);
	us_manager_unreg_cb(__loader_cbs_stop_h);
	loader_unset();
	lpd_uninit();
	ls_exit();
	ld_exit();

	balance = atomic_read(&dentry_balance);
	atomic_set(&dentry_balance, 0);

	WARN(balance, "Bad GET/PUT dentry balance: %d\n", balance);
}

SWAP_LIGHT_INIT_MODULE(NULL, loader_module_init, loader_module_exit,
		       NULL, NULL);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP Loader Module");
MODULE_AUTHOR("Vasiliy Ulyanov <v.ulyanov@samsung.com>"
              "Alexander Aksenov <a.aksenov@samsung.com>");
