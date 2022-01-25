#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/fs.h>

#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/us_common_file.h>

#include "preload.h"
#include "preload_module.h"
#include "preload_control.h"
#include "preload_probe.h"

struct bin_desc {
	struct list_head list;
	struct dentry *dentry;
	char *filename;
};

struct list_desc {
	struct list_head list;
	rwlock_t lock;
	int cnt;
};

static struct list_desc target = {
	.list = LIST_HEAD_INIT(target.list),
	.lock = __RW_LOCK_UNLOCKED(&target.lock),
	.cnt = 0
};

static struct list_desc ignored = {
	.list = LIST_HEAD_INIT(ignored.list),
	.lock = __RW_LOCK_UNLOCKED(&ignored.lock),
	.cnt = 0
};

static inline struct task_struct *__get_task_struct(void)
{
	return current;
}

static struct bin_desc *__alloc_binary(struct dentry *dentry, char *name,
				       int namelen)
{
	struct bin_desc *p = NULL;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	INIT_LIST_HEAD(&p->list);
	p->filename = kmalloc(namelen + 1, GFP_KERNEL);
	if (!p->filename)
		goto fail;
	memcpy(p->filename, name, namelen);
	p->filename[namelen] = '\0';
	p->dentry = dentry;

	return p;
fail:
	kfree(p);
	return NULL;
}

static void __free_binary(struct bin_desc *p)
{
	kfree(p->filename);
	kfree(p);
}

static void __free_binaries(struct list_desc *tl)
{
	struct bin_desc *p, *n;
	struct list_head rm_head;

	INIT_LIST_HEAD(&rm_head);
	write_lock(&tl->lock);
	list_for_each_entry_safe(p, n, &tl->list, list) {
		list_move(&p->list, &rm_head);
	}
	tl->cnt = 0;
	write_unlock(&tl->lock);

	list_for_each_entry_safe(p, n, &rm_head, list) {
		list_del(&p->list);
		swap_put_dentry(p->dentry);
		__free_binary(p);
	}
}

static bool __check_dentry_already_exist(struct dentry *dentry,
					 struct list_desc *tl)
{
	struct bin_desc *p;
	bool ret = false;

	read_lock(&tl->lock);
	list_for_each_entry(p, &tl->list, list) {
		if (p->dentry == dentry) {
			ret = true;
			goto out;
		}
	}
out:
	read_unlock(&tl->lock);

	return ret;
}

static int __add_binary(struct dentry *dentry, char *filename,
			struct list_desc *tl)
{
	struct bin_desc *p;
	size_t len;

	if (__check_dentry_already_exist(dentry, tl)) {
		printk(PRELOAD_PREFIX "Binary already exist\n");
		return EALREADY;
	}

	/* Filename should be < PATH_MAX */
	len = strnlen(filename, PATH_MAX);
	if (len == PATH_MAX)
		return -EINVAL;

	p = __alloc_binary(dentry, filename, len);
	if (!p)
		return -ENOMEM;

	write_lock(&tl->lock);
	list_add_tail(&p->list, &tl->list);
	tl->cnt++;
	write_unlock(&tl->lock);

	return 0;
}

static struct dentry *__get_caller_dentry(struct task_struct *task,
					  unsigned long caller)
{
	struct vm_area_struct *vma = NULL;

	if (unlikely(task->mm == NULL))
		goto get_caller_dentry_fail;

	vma = find_vma_intersection(task->mm, caller, caller + 1);
	if (unlikely(vma == NULL || vma->vm_file == NULL))
		goto get_caller_dentry_fail;

	return vma->vm_file->f_path.dentry;

get_caller_dentry_fail:

	return NULL;
}

static bool __check_if_instrumented(struct task_struct *task,
				    struct dentry *dentry)
{
	return __check_dentry_already_exist(dentry, &target);
}

static bool __is_instrumented(void *caller)
{
	struct task_struct *task = __get_task_struct();
	struct dentry *caller_dentry = __get_caller_dentry(task,
							   (unsigned long) caller);

	if (caller_dentry == NULL)
		return false;

	return __check_if_instrumented(task, caller_dentry);
}

static unsigned int __get_names(struct list_desc *tl, char ***filenames_p)
{
	unsigned int i, ret = 0;
	struct bin_desc *p;
	char **a = NULL;

	read_lock(&tl->lock);
	if (tl->cnt == 0)
		goto out;

	a = kmalloc(sizeof(*a) * tl->cnt, GFP_KERNEL);
	if (!a)
		goto out;

	i = 0;
	list_for_each_entry(p, &tl->list, list) {
		if (i >= tl->cnt)
			break;
		a[i++] = p->filename;
	}

	*filenames_p = a;
	ret = i;
out:
	read_unlock(&tl->lock);
	return ret;
}


/* Called only form handlers. If we're there, then it is instrumented. */
enum preload_call_type pc_call_type_always_inst(void *caller)
{
	if (__is_instrumented(caller))
		return INTERNAL_CALL;

	return EXTERNAL_CALL;

}

enum preload_call_type pc_call_type(struct sspt_ip *ip, void *caller)
{
	if (__is_instrumented(caller))
		return INTERNAL_CALL;

	if (ip->desc->info.pl_i.flags & SWAP_PRELOAD_ALWAYS_RUN)
		return EXTERNAL_CALL;

	return NOT_INSTRUMENTED;
}

int pc_add_instrumented_binary(char *filename)
{
	struct dentry *dentry = swap_get_dentry(filename);
	int res = 0;

	if (dentry == NULL)
		return -EINVAL;

	res = __add_binary(dentry, filename, &target);
	if (res != 0)
		swap_put_dentry(dentry);

	return res > 0 ? 0 : res;
}

int pc_clean_instrumented_bins(void)
{
	__free_binaries(&target);

	return 0;
}

int pc_add_ignored_binary(char *filename)
{
	struct dentry *dentry = swap_get_dentry(filename);
	int res = 0;

	if (dentry == NULL)
		return -EINVAL;

	res = __add_binary(dentry, filename, &ignored);
	if (res != 0)
		swap_put_dentry(dentry);

	return res > 0 ? 0 : res;
}

int pc_clean_ignored_bins(void)
{
	__free_binaries(&ignored);

	return 0;
}

unsigned int pc_get_target_names(char ***filenames_p)
{
	return __get_names(&target, filenames_p);
}

void pc_release_target_names(char ***filenames_p)
{
	kfree(*filenames_p);
}

unsigned int pc_get_ignored_names(char ***filenames_p)
{
	return __get_names(&ignored, filenames_p);
}

void pc_release_ignored_names(char ***filenames_p)
{
	kfree(*filenames_p);
}

bool pc_check_dentry_is_ignored(struct dentry *dentry)
{
	struct bin_desc *p;
	bool ret = false;

	if (dentry == NULL)
		return false;

	read_lock(&ignored.lock);

	list_for_each_entry(p, &ignored.list, list) {
		if (p->dentry == dentry) {
			ret = true;
			break;
		}
	}

	read_unlock(&ignored.lock);

	return ret;
}

int pc_init(void)
{
	return 0;
}

void pc_exit(void)
{
	__free_binaries(&target);
	__free_binaries(&ignored);
}
