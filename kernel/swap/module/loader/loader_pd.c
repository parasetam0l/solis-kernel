#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/hardirq.h>
#include <linux/list.h>
#include <us_manager/us_manager_common.h>
#include <us_manager/sspt/sspt_proc.h>
#include "loader_pd.h"
#include "loader.h"
#include "loader_debugfs.h"
#include "loader_storage.h"
#include "loader_defs.h"


struct pd_t {
	unsigned long loader_base;
	unsigned long data_page;
	struct list_head handlers;
	bool is_pthread_init;
};

struct hd_t {
	struct list_head list;
	struct dentry *dentry;
	enum ps_t state;
	struct pd_t *parent;
	unsigned long base;
	unsigned long offset;
	void __user *handle;
	long attempts;
};


static inline bool check_vma(struct vm_area_struct *vma, struct dentry *dentry)
{
	struct file *file = vma->vm_file;

	return (file && (vma->vm_flags & VM_EXEC) &&
		(file->f_path.dentry == dentry));
}

static inline unsigned long __get_loader_base(struct pd_t *pd)
{
	return pd->loader_base;
}

static inline void __set_loader_base(struct pd_t *pd,
				     unsigned long addr)
{
	pd->loader_base = addr;
}

static inline unsigned long __get_data_page(struct pd_t *pd)
{
	return pd->data_page;
}

static inline void __set_data_page(struct pd_t *pd, unsigned long page)
{
	pd->data_page = page;
}



static inline enum ps_t __get_state(struct hd_t *hd)
{
	return hd->state;
}

static inline void __set_state(struct hd_t *hd, enum ps_t state)
{
	hd->state = state;
}

static inline unsigned long __get_handlers_base(struct hd_t *hd)
{
	return hd->base;
}

static inline void __set_handlers_base(struct hd_t *hd,
				       unsigned long addr)
{
	hd->base = addr;
}

static inline unsigned long __get_offset(struct hd_t *hd)
{
	return hd->offset;
}

static inline void *__get_handle(struct hd_t *hd)
{
	return hd->handle;
}

static inline void __set_handle(struct hd_t *hd, void __user *handle)
{
	hd->handle = handle;
}

static inline long __get_attempts(struct hd_t *hd)
{
	return hd->attempts;
}

static inline void __set_attempts(struct hd_t *hd, long attempts)
{
	hd->attempts = attempts;
}



static struct vm_area_struct *find_vma_by_dentry(struct mm_struct *mm,
						 struct dentry *dentry)
{
	struct vm_area_struct *vma;

	for (vma = mm->mmap; vma; vma = vma->vm_next)
		if (check_vma(vma, dentry))
			return vma;

	return NULL;
}

static struct pd_t *__create_pd(void)
{
	struct pd_t *pd;
	unsigned long page;

	pd = kzalloc(sizeof(*pd), GFP_ATOMIC);
	if (pd == NULL)
		return NULL;

	down_write(&current->mm->mmap_sem);
	page = __swap_do_mmap(NULL, 0, PAGE_SIZE, PROT_READ | PROT_WRITE,
			      MAP_ANONYMOUS | MAP_PRIVATE, 0);
	up_write(&current->mm->mmap_sem);
	if (IS_ERR_VALUE(page)) {
		printk(KERN_ERR LOADER_PREFIX
		       "Cannot alloc page for %u\n", current->tgid);
		goto create_pd_fail;
	}

	__set_data_page(pd, page);
	pd->is_pthread_init = true;
	INIT_LIST_HEAD(&pd->handlers);

	return pd;

create_pd_fail:
	kfree(pd);

	return NULL;
}

static size_t __copy_path(char *src, unsigned long page, unsigned long offset)
{
	unsigned long dest = page + offset;
	size_t len = strnlen(src, PATH_MAX);

	/* set handler path */
	if (copy_to_user((void __user *)dest, src, len) != 0) {
		printk(KERN_ERR LOADER_PREFIX
		       "Cannot copy string to user!\n");
		return 0;
	}

	return len;
}

static void __set_ld_mapped(struct pd_t *pd, struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	struct dentry *ld = ld_get_loader_dentry();

	down_read(&mm->mmap_sem);
	if (ld) {
		vma = find_vma_by_dentry(mm, ld);
		if (vma)
			__set_loader_base(pd, vma->vm_start);
	}
	up_read(&mm->mmap_sem);
}

static void __set_handler_mapped(struct hd_t *hd, struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	struct dentry *handlers = hd->dentry;

	down_read(&mm->mmap_sem);
	if (handlers) {
		vma = find_vma_by_dentry(mm, handlers);
		if (vma) {
			__set_handlers_base(hd, vma->vm_start);
			__set_state(hd, LOADED);
			goto set_handler_mapped_out;
		}
	}
	__set_state(hd, NOT_LOADED);

set_handler_mapped_out:
	up_read(&mm->mmap_sem);
}


static int __get_handlers(struct pd_t *pd, struct task_struct *task)
{
	struct list_head *handlers = NULL;
	struct bin_info_el *bin;
	struct hd_t *hd;
	unsigned long offset = 0;
	size_t len;
	int ret = 0;

	handlers = ls_get_handlers();
	if (handlers == NULL)
		return -EINVAL;

	list_for_each_entry(bin, handlers, list) {
		len = __copy_path(bin->path, pd->data_page, offset);
		if (len == 0) {
			ret = -EINVAL;
			goto get_handlers_out;
		}

		hd = kzalloc(sizeof(*hd), GFP_ATOMIC);
		if (hd == NULL) {
			printk(KERN_ERR LOADER_PREFIX "No atomic mem!\n");
			ret = -ENOMEM;
			goto get_handlers_out;
		}


		INIT_LIST_HEAD(&hd->list);
		hd->parent = pd;
		hd->dentry = bin->dentry;
		hd->offset = offset;
		__set_handler_mapped(hd, task->mm);
		__set_attempts(hd, LOADER_MAX_ATTEMPTS);
		list_add_tail(&hd->list, &pd->handlers);

		/* inc handlers path's on page */
		offset += len + 1;
	}

get_handlers_out:
	/* TODO Cleanup already created */
	ls_put_handlers();

	return ret;
}



enum ps_t lpd_get_state(struct hd_t *hd)
{
	if (hd == NULL)
		return 0;

	return __get_state(hd);
}
EXPORT_SYMBOL_GPL(lpd_get_state);

void lpd_set_state(struct hd_t *hd, enum ps_t state)
{
	if (hd == NULL) {
		printk(LOADER_PREFIX "%d: No handler data! Current %d %s\n",
		       __LINE__, current->tgid, current->comm);
		return;
	}

	__set_state(hd, state);
}

unsigned long lpd_get_loader_base(struct pd_t *pd)
{
	if (pd == NULL)
		return 0;

	return __get_loader_base(pd);
}

void lpd_set_loader_base(struct pd_t *pd, unsigned long vaddr)
{
	__set_loader_base(pd, vaddr);
}

unsigned long lpd_get_handlers_base(struct hd_t *hd)
{
	if (hd == NULL)
		return 0;

	return __get_handlers_base(hd);
}
EXPORT_SYMBOL_GPL(lpd_get_handlers_base);

void lpd_set_handlers_base(struct hd_t *hd, unsigned long vaddr)
{
	__set_handlers_base(hd, vaddr);
}

char __user *lpd_get_path(struct pd_t *pd, struct hd_t *hd)
{
	unsigned long page = __get_data_page(pd);
	unsigned long offset = __get_offset(hd);

	return (char __user *)(page + offset);
}



void *lpd_get_handle(struct hd_t *hd)
{
	if (hd == NULL)
		return NULL;

	return __get_handle(hd);
}

void lpd_set_handle(struct hd_t *hd, void __user *handle)
{
	if (hd == NULL) {
		printk(LOADER_PREFIX "%d: No handler data! Current %d %s\n",
		       __LINE__, current->tgid, current->comm);
		return;
	}

	__set_handle(hd, handle);
}

long lpd_get_attempts(struct hd_t *hd)
{
	if (hd == NULL)
		return -EINVAL;

	return __get_attempts(hd);
}

void lpd_dec_attempts(struct hd_t *hd)
{
	long attempts;

	if (hd == NULL) {
		printk(LOADER_PREFIX "%d: No handler data! Current %d %s\n",
		       __LINE__, current->tgid, current->comm);
		return;
	}

	attempts = __get_attempts(hd);
	attempts--;
	__set_attempts(hd, attempts);
}

struct dentry *lpd_get_dentry(struct hd_t *hd)
{
	return hd->dentry;
}

struct pd_t *lpd_get_parent_pd(struct hd_t *hd)
{
	return hd->parent;
}
EXPORT_SYMBOL_GPL(lpd_get_parent_pd);

struct pd_t *lpd_get(struct sspt_proc *proc)
{
	return (struct pd_t *)proc->private_data;
}
EXPORT_SYMBOL_GPL(lpd_get);

struct pd_t *lpd_get_by_task(struct task_struct *task)
{
	struct sspt_proc *proc = sspt_proc_by_task(task);

	return lpd_get(proc);
}
EXPORT_SYMBOL_GPL(lpd_get_by_task);

struct hd_t *lpd_get_hd(struct pd_t *pd, struct dentry *dentry)
{
	struct hd_t *hd;

	list_for_each_entry(hd, &pd->handlers, list) {
		if (hd->dentry == dentry)
			return hd;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(lpd_get_hd);

/* TODO Move it to GOT patcher, only it uses this */
bool lpd_get_init_state(struct pd_t *pd)
{
	return pd->is_pthread_init;
}
EXPORT_SYMBOL_GPL(lpd_get_init_state);

/* TODO Move it to GOT patcher, only it uses this */
void lpd_set_init_state(struct pd_t *pd, bool state)
{
	pd->is_pthread_init = state;
}
EXPORT_SYMBOL_GPL(lpd_set_init_state);

static struct pd_t *do_create_pd(struct task_struct *task)
{
	struct pd_t *pd;
	int ret;

	pd = __create_pd();
	if (pd == NULL) {
		ret = -ENOMEM;
		goto create_pd_exit;
	}

	ret = __get_handlers(pd, task);
	if (ret)
		goto free_pd;

	__set_ld_mapped(pd, task->mm);

	return pd;

free_pd:
	kfree(pd);

create_pd_exit:
	printk(KERN_ERR LOADER_PREFIX "do_pd_create_pd: error=%d\n", ret);
	return NULL;
}

static void *pd_create(struct sspt_proc *proc)
{
	struct pd_t *pd;

	pd = do_create_pd(proc->leader);

	return (void *)pd;
}

static void pd_destroy(struct sspt_proc *proc, void *data)
{
	/* FIXME: to be implemented */
}

struct sspt_proc_cb pd_cb = {
	.priv_create = pd_create,
	.priv_destroy = pd_destroy
};

int lpd_init(void)
{
	int ret;

	ret = sspt_proc_cb_set(&pd_cb);

	return ret;
}

void lpd_uninit(void)
{
	sspt_proc_cb_set(NULL);

	/* TODO Cleanup */
}
