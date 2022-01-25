#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <master/swap_debugfs.h>
#include "gt.h"
#include "gt_debugfs.h"
#include "gt_module.h"

#define GT_DEFAULT_PERMS (S_IRUSR | S_IWUSR) /* u+rw */

static const char GT_FOLDER[] = "got_patcher";
static const char GT_LINKER[] = "linker";
static const char GT_PATH[] = "path";
static const char GT_FIXUP_ADDR[] = "dl_fixup_addr";
static const char GT_RELOC_ADDR[] = "dl_reloc_addr";
static const char GT_ENABLE[] = "enable";
static const char GT_BY_PATH[] = "by_path";
static const char GT_BY_PID[] = "by_pid";
static const char GT_BY_ID[] = "by_id";
static const char GT_ADD[] = "add";
static const char GT_DEL[] = "del";
static const char GT_DEL_ALL[] = "del_all";
static const char GT_LIST_TARGETS[] = "list_targets";
static const char GT_HANDLER[] = "handler";
static const char GT_HANDLER_FIXUP_OFF[] = "fixup_handler_off";
static const char GT_HANDLER_RELOC_OFF[] = "reloc_handler_off";
static const char GT_PTHREAD[] = "pthread";
static const char GT_MINIMAL_INIT[] = "minimal_init_off";

static struct dentry *gt_root;


/* Type for function that handles string grabbed from userspace */
typedef int (*sh_t)(char *);

/* Type for function that handles unsigned long grabbed from userspace */
typedef int (*ulh_t)(unsigned long);

/* Type for function that handles pid grabbed from userspace */
typedef int (*ph_t)(pid_t);


static ssize_t get_string_and_call(const char __user *buf, size_t len,
				   sh_t cb)
{
	char *string;
	ssize_t ret;

	string = kmalloc(len, GFP_KERNEL);
	if (string == NULL) {
		ret = -ENOMEM;
		goto get_string_write_out;
	}

	if (copy_from_user(string, buf, len)) {
		ret = -EINVAL;
		goto get_string_write_out;
	}

	string[len - 1] = '\0';

	ret = cb(string);

get_string_write_out:
	kfree(string);

	return ret == 0 ? len : ret;
}

static ssize_t get_ul_and_call(const char __user *buf, size_t len, ulh_t cb)
{
	ssize_t ret;
	char *ulstring;
	unsigned long ul;

	ulstring = kmalloc(len, GFP_KERNEL);
	if (ulstring == NULL) {
		ret = -ENOMEM;
		goto get_ul_write_out;
	}

	if (copy_from_user(ulstring, buf, len)) {
		ret = -EINVAL;
		goto get_ul_write_out;
	}

	ulstring[len - 1] = '\0';

	ret = kstrtoul(ulstring, 16, &ul);
	if (ret != 0)
		goto get_ul_write_out;

	ret = cb(ul);

get_ul_write_out:
	kfree(ulstring);

	return ret == 0 ? len : ret;
}

static ssize_t get_pid_and_call(const char __user *buf, size_t len, ph_t cb)
{
	ssize_t ret;
	char *pidstring;
	pid_t pid;

	pidstring = kmalloc(len, GFP_KERNEL);
	if (pidstring == NULL) {
		ret = -ENOMEM;
		goto get_pid_write_out;
	}

	if (copy_from_user(pidstring, buf, len)) {
		ret = -EINVAL;
		goto get_pid_write_out;
	}

	pidstring[len - 1] = '\0';

	ret = kstrtoul(pidstring, 10, (unsigned long *)&pid);
	if (ret != 0)
		goto get_pid_write_out;

	ret = cb(pid);

get_pid_write_out:
	kfree(pidstring);

	return ret == 0 ? len : ret;
}

/* ===========================================================================
 * =                              TARGETS                                    =
 * ===========================================================================
 */

static ssize_t handler_path_write(struct file *file, const char __user *buf,
				  size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_set_handler_path);
}

static ssize_t handler_fixup_off_write(struct file *file,
				       const char __user *buf, size_t len,
				       loff_t *ppos)
{
	return get_ul_and_call(buf, len, gtm_set_handler_fixup_off);
}

static ssize_t handler_reloc_off_write(struct file *file,
				       const char __user *buf, size_t len,
				       loff_t *ppos)
{
	return get_ul_and_call(buf, len, gtm_set_handler_reloc_off);
}

static const struct file_operations handler_path_fops = {
	.owner = THIS_MODULE,
	.write = handler_path_write,
};

static const struct file_operations handler_fixup_off_fops = {
	.owner = THIS_MODULE,
	.write = handler_fixup_off_write,
};

static const struct file_operations handler_reloc_off_fops = {
	.owner = THIS_MODULE,
	.write = handler_reloc_off_write,
};

/* ===========================================================================
 * =                              TARGETS                                    =
 * ===========================================================================
 */

static ssize_t by_path_add_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_add_by_path);
}

static ssize_t by_path_del_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_del_by_path);
}

static ssize_t by_pid_add_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	return get_pid_and_call(buf, len, gtm_add_by_pid);
}

static ssize_t by_pid_del_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	return get_pid_and_call(buf, len, gtm_del_by_pid);
}

static ssize_t by_id_add_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_add_by_id);
}

static ssize_t by_id_del_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_del_by_id);
}

static ssize_t del_all_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	ssize_t ret;

	ret = gtm_del_all();

	return ret == 0 ? len : ret;
}

static ssize_t target_read(struct file *file, char __user *buf,
			   size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *targets;

	ret = gtm_get_targets(&targets);
	if (ret < 0)
		return ret;

	ret = simple_read_from_buffer(buf, len, ppos, targets, ret);
	kfree(targets);

	return ret;
}

static const struct file_operations by_path_add_fops = {
	.owner = THIS_MODULE,
	.write = by_path_add_write,
};

static const struct file_operations by_path_del_fops = {
	.owner = THIS_MODULE,
	.write = by_path_del_write,
};

static const struct file_operations by_pid_add_fops = {
	.owner = THIS_MODULE,
	.write = by_pid_add_write,
};

static const struct file_operations by_pid_del_fops = {
	.owner = THIS_MODULE,
	.write = by_pid_del_write,
};

static const struct file_operations by_id_add_fops = {
	.owner = THIS_MODULE,
	.write = by_id_add_write,
};

static const struct file_operations by_id_del_fops = {
	.owner = THIS_MODULE,
	.write = by_id_del_write,
};

static const struct file_operations del_all_fops = {
	.owner = THIS_MODULE,
	.write = del_all_write,
};

static const struct file_operations target_fops = {
	.owner = THIS_MODULE,
	.read = target_read,
};

/* ===========================================================================
 * =                              ENABLE                                     =
 * ===========================================================================
 */

static ssize_t enable_read(struct file *file, char __user *buf,
			   size_t len, loff_t *ppos)
{
	char val[2];

	val[0] = (gtm_status() == GT_ON ? '1' : '0');
	val[1] = '\0';

	return simple_read_from_buffer(buf, len, ppos, val, 2);
}

static ssize_t enable_write(struct file *file, const char __user *buf,
			    size_t len, loff_t *ppos)
{
	ssize_t ret;
	char val[2];
	size_t val_size;

	val_size = min(len, (sizeof(val) - 1));
	if (copy_from_user(val, buf, val_size))
		return -EFAULT;

	val[1] = '\0';
	switch(val[0]) {
	case '0':
		ret = gtm_switch(GT_OFF);
		break;
	case '1':
		ret = gtm_switch(GT_ON);
		break;
	default:
		printk(GT_PREFIX "Invalid state!\n");
		return -EINVAL;
	}

	return ret == 0 ? len : ret;
}

static const struct file_operations enable_fops = {
	.owner = THIS_MODULE,
	.write = enable_write,
	.read = enable_read,
};


/* ===========================================================================
 * =                              LINKER                                     =
 * ===========================================================================
 */

static ssize_t linker_path_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_set_linker_path);
}

static ssize_t fixup_off_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	return get_ul_and_call(buf, len, gtm_set_fixup_off);
}

static ssize_t reloc_off_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	return get_ul_and_call(buf, len, gtm_set_reloc_off);
}

static const struct file_operations linker_path_fops = {
	.owner = THIS_MODULE,
	.write = linker_path_write,
};

static const struct file_operations fixup_off_fops = {
	.owner = THIS_MODULE,
	.write = fixup_off_write,
};

static const struct file_operations reloc_off_fops = {
	.owner = THIS_MODULE,
	.write = reloc_off_write,
};



/* ===========================================================================
 * =                             PTHREAD                                     =
 * ===========================================================================
 */

static ssize_t pthread_path_write(struct file *file, const char __user *buf,
				  size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, gtm_set_pthread_path);
}

static ssize_t init_off_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	return get_ul_and_call(buf, len, gtm_set_init_off);
}

static const struct file_operations pthread_path_fops = {
	.owner = THIS_MODULE,
	.write = pthread_path_write,
};

static const struct file_operations pthread_init_off_fops = {
	.owner = THIS_MODULE,
	.write = init_off_write,
};




int gtd_init(void)
{
	struct dentry *swap_dentry, *root, *linker, *by_path, *by_pid,
		      *by_id, *handler, *pthread, *dentry;
	int ret;

	ret = -ENODEV;
	if (!debugfs_initialized())
		goto fail;

	ret = -ENOENT;
	swap_dentry = swap_debugfs_getdir();
	if (!swap_dentry)
		goto fail;

	ret = -ENOMEM;
	root = swap_debugfs_create_dir(GT_FOLDER, swap_dentry);
	if (IS_ERR_OR_NULL(root))
		goto fail;

	gt_root = root;

	linker = swap_debugfs_create_dir(GT_LINKER, root);
	if (IS_ERR_OR_NULL(linker)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_PATH, GT_DEFAULT_PERMS, linker,
					  NULL, &linker_path_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_FIXUP_ADDR, GT_DEFAULT_PERMS,
					  linker, NULL, &fixup_off_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_RELOC_ADDR, GT_DEFAULT_PERMS,
					  linker, NULL, &reloc_off_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	by_path = swap_debugfs_create_dir(GT_BY_PATH, root);
	if (IS_ERR_OR_NULL(by_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_ADD, GT_DEFAULT_PERMS, by_path,
					  NULL, &by_path_add_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_DEL, GT_DEFAULT_PERMS, by_path,
					  NULL, &by_path_del_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	by_pid = swap_debugfs_create_dir(GT_BY_PID, root);
	if (IS_ERR_OR_NULL(by_pid)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_ADD, GT_DEFAULT_PERMS, by_pid,
					  NULL, &by_pid_add_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_DEL, GT_DEFAULT_PERMS, by_pid,
					  NULL, &by_pid_del_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	by_id = swap_debugfs_create_dir(GT_BY_ID, root);
	if (IS_ERR_OR_NULL(by_id)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_ADD, GT_DEFAULT_PERMS, by_id,
					  NULL, &by_id_add_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_DEL, GT_DEFAULT_PERMS, by_id,
					  NULL, &by_id_del_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_DEL_ALL, GT_DEFAULT_PERMS, root,
					  NULL, &del_all_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_LIST_TARGETS, GT_DEFAULT_PERMS, root,
					  NULL, &target_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_ENABLE, GT_DEFAULT_PERMS, root,
					  NULL, &enable_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	handler = swap_debugfs_create_dir(GT_HANDLER, root);
	if (IS_ERR_OR_NULL(handler)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_PATH, GT_DEFAULT_PERMS, handler,
					  NULL, &handler_path_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_HANDLER_FIXUP_OFF,
					  GT_DEFAULT_PERMS, handler,
					  NULL, &handler_fixup_off_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_HANDLER_RELOC_OFF,
					  GT_DEFAULT_PERMS, handler, NULL,
					  &handler_reloc_off_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	pthread = swap_debugfs_create_dir(GT_PTHREAD, root);
	if (IS_ERR_OR_NULL(pthread)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_PATH, GT_DEFAULT_PERMS, pthread,
					  NULL, &pthread_path_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(GT_MINIMAL_INIT, GT_DEFAULT_PERMS,
					  pthread, NULL,
					  &pthread_init_off_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	return 0;

remove:
	debugfs_remove_recursive(root);

fail:
	printk(GT_PREFIX "Debugfs initialization failure: %d\n", ret);

	return ret;
}

void gtd_exit(void)
{
	if (gt_root)
		debugfs_remove_recursive(gt_root);
	gt_root = NULL;
}
