#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/limits.h>
#include <asm/uaccess.h>
#include <master/swap_debugfs.h>
#include <master/swap_initializer.h>
#include "preload.h"
#include "preload_module.h"
#include "preload_debugfs.h"
#include "preload_control.h"
#include "preload_process.h"

static const char PRELOAD_FOLDER[] = "preload";
static const char PRELOAD_TARGET[] = "target_binaries";
static const char PRELOAD_IGNORED[] = "ignored_binaries";
static const char PRELOAD_BINARIES_LIST[] = "bins_list";
static const char PRELOAD_BINARIES_ADD[] = "bins_add";
static const char PRELOAD_BINARIES_REMOVE[] = "bins_remove";
static const char PRELOAD_ENABLE[] = "enable";
static const char PRELOAD_BY_PATH[] = "by_path";
static const char PRELOAD_BY_PID[] = "by_pid";
static const char PRELOAD_BY_ID[] = "by_id";
static const char PRELOAD_ADD[] = "add";
static const char PRELOAD_DEL[] = "del";
static const char PRELOAD_DEL_ALL[] = "del_all";
static const char PRELOAD_PTHREAD[] = "pthread";
static const char PRELOAD_PATH[] = "path";
static const char PRELOAD_MINIMAL_INIT[] = "minimal_init_off";

static struct dentry *preload_root;
static struct dentry *target_list = NULL;
static struct dentry *target_add = NULL;
static struct dentry *target_remove = NULL;
static struct dentry *ignored_list = NULL;
static struct dentry *ignored_add = NULL;
static struct dentry *ignored_remove = NULL;


/* Type for functions that add process by path and by id */
typedef int (*sh_t)(const char *);

/* Type for functions that add process by pid */
typedef int (*ph_t)(pid_t);

/* Type for function that handles unsigned long grabbed from userspace */
typedef int (*ulh_t)(unsigned long);


/* remove end-line symbols */
static void rm_endline_symbols(char *buf, size_t len)
{
	char *p, *buf_end;

	buf_end = buf + len;
	for (p = buf; p != buf_end; ++p)
		if (*p == '\n' || *p == '\r')
			*p = '\0';
}

static ssize_t get_string(const char __user *buf, size_t len, char **kbuf)
{
	char *string;
	ssize_t ret;

	string = kmalloc(len + 1, GFP_KERNEL);
	if (!string) {
		pr_warn(PRELOAD_PREFIX "No mem for user string!\n");
		return -ENOMEM;
	}

	if (copy_from_user(string, buf, len)) {
		pr_warn(PRELOAD_PREFIX "Failed to copy data from user!\n");
		ret = -EINVAL;
		goto get_string_fail;
	}

	string[len] = '\0';
	rm_endline_symbols(string, len);
	*kbuf = string;

	return len;

get_string_fail:
	kfree(string);

	return ret;
}


static ssize_t get_ul_and_call(const char __user *buf, size_t len, ulh_t cb)
{
	ssize_t ret;
	char *ulstring;
	unsigned long ul;

	ret = get_string(buf, len, &ulstring);
	if (ret != len)
		return ret;

	ret = kstrtoul(ulstring, 16, &ul);
	if (ret)
		goto get_ul_write_out;

	ret = cb(ul);

get_ul_write_out:
	kfree(ulstring);

	return ret == 0 ? len : ret;
}

static ssize_t get_string_and_call(const char __user *buf, size_t len, sh_t cb)
{
	char *string;
	ssize_t ret;

	ret = get_string(buf, len, &string);
	if (ret != len)
		return ret;

	ret = cb(string);
	if (ret)
		pr_warn(PRELOAD_PREFIX "Error adding process by <%s>\n",
			string);

	kfree(string);

	return ret == 0 ? len : ret;
}

static ssize_t get_pid_and_call(const char __user *buf, size_t len, ph_t cb)
{
	char *string;
	pid_t pid;
	ssize_t ret;

	ret = get_string(buf, len, &string);
	if (ret != len)
		return ret;

	ret = kstrtoul(string, 10, (unsigned long *)&pid);
	if (ret) {
		pr_warn(PRELOAD_PREFIX "Invalid PID!\n");
		goto get_pid_out;
	}

	ret = cb(pid);
	if (ret)
		pr_warn(PRELOAD_PREFIX "Error adding process by <%s>\n",
			string);

get_pid_out:
	kfree(string);

	return ret == 0 ? len : ret;
}

/* ===========================================================================
 * =                           TARGET PROCESSES                              =
 * ===========================================================================
 */

static ssize_t by_path_add_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, pp_add_by_path);
}

static ssize_t by_path_del_write(struct file *file, const char __user *buf,
				 size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, pp_del_by_path);
}

static ssize_t by_pid_add_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	return get_pid_and_call(buf, len, pp_add_by_pid);
}

static ssize_t by_pid_del_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	return get_pid_and_call(buf, len, pp_del_by_pid);
}

static ssize_t by_id_add_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, pp_add_by_id);
}

static ssize_t by_id_del_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, pp_del_by_id);
}

static ssize_t del_all_write(struct file *file, const char __user *buf,
				size_t len, loff_t *ppos)
{
	pp_del_all();

	WARN(pc_clean_instrumented_bins(), PRELOAD_PREFIX
	     "Error while cleaning target bins\n");
	WARN(pc_clean_ignored_bins(), PRELOAD_PREFIX
	     "Error while cleaning ignored bins\n");

	return len;
}

static const struct file_operations by_path_add_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = by_path_add_write,
};

static const struct file_operations by_path_del_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = by_path_del_write,
};

static const struct file_operations by_pid_add_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = by_pid_add_write,
};

static const struct file_operations by_pid_del_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = by_pid_del_write,
};

static const struct file_operations by_id_add_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = by_id_add_write,
};

static const struct file_operations by_id_del_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = by_id_del_write,
};

static const struct file_operations del_all_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = del_all_write,
};

/* ===========================================================================
 * =                              ENABLE                                     =
 * ===========================================================================
 */

static ssize_t enable_read(struct file *file, char __user *buf,
			   size_t len, loff_t *ppos)
{
	char val[2];

	val[0] = (pm_status() == PRELOAD_ON ? '1' : '0');
	val[1] = '\0';

	return simple_read_from_buffer(buf, len, ppos, val, 2);
}

static ssize_t enable_write(struct file *file, const char __user *buf,
			    size_t len, loff_t *ppos)
{
	ssize_t ret = 0;
	char val[2];
	size_t val_size;

	val_size = min(len, (sizeof(val) - 1));
	if (copy_from_user(val, buf, val_size))
		return -EFAULT;

	val[1] = '\0';
	switch (val[0]) {
	case '0':
		ret = pm_switch(PRELOAD_OFF);
		break;
	case '1':
		ret = pm_switch(PRELOAD_ON);
		break;
	default:
		printk(PRELOAD_PREFIX "Invalid state!\n");
		return -EINVAL;
	}

	return ret == 0 ? len : ret;
}

static const struct file_operations enable_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = enable_write,
	.read = enable_read,
};


/* ===========================================================================
 * =                                BIN PATH                                 =
 * ===========================================================================
 */

static ssize_t bin_add_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto bin_add_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto bin_add_write_out;
	}

	path[len - 1] = '\0';

	if (file->f_path.dentry == target_add)
		ret = pc_add_instrumented_binary(path);
	else if (file->f_path.dentry == ignored_add)
		ret = pc_add_ignored_binary(path);
	else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = -EINVAL;
		goto bin_add_write_out;
	}


	if (ret != 0) {
		printk(PRELOAD_PREFIX "Cannot add binary %s\n", path);
		ret = -EINVAL;
		goto bin_add_write_out;
	}

	ret = len;

bin_add_write_out:
	kfree(path);

	return ret;
}

static ssize_t bin_remove_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	ssize_t ret;

	if (file->f_path.dentry == target_remove)
		ret = pc_clean_instrumented_bins();
	else if (file->f_path.dentry == ignored_remove)
		ret = pc_clean_ignored_bins();
	else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = -EINVAL;
		goto bin_remove_write_out;
	}

	if (ret != 0) {
		printk(PRELOAD_PREFIX "Error during clean!\n");
		ret = -EINVAL;
		goto bin_remove_write_out;
	}

	ret = len;

bin_remove_write_out:
	return ret;
}

static ssize_t bin_list_read(struct file *file, char __user *usr_buf,
				 size_t count, loff_t *ppos)
{
	unsigned int i;
	unsigned int files_cnt = 0;
	ssize_t len = 0, tmp, ret = 0;
	char **filenames = NULL;
	char *buf = NULL;
	char *ptr = NULL;

	if (file->f_path.dentry == target_list) {
		files_cnt = pc_get_target_names(&filenames);
	} else if (file->f_path.dentry == ignored_list) {
		files_cnt = pc_get_ignored_names(&filenames);
	} else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = 0;
		goto bin_list_read_out;
	}

	if (files_cnt == 0) {
		printk(PRELOAD_PREFIX "Cannot read binaries names!\n");
		ret = 0;
		goto bin_list_read_fail;
	}

	for (i = 0; i < files_cnt; i++)
		len += strlen(filenames[i]);

	buf = kmalloc(len + files_cnt, GFP_KERNEL);
	if (buf == NULL) {
		ret = 0;
		goto bin_list_read_fail;
	}

	ptr = buf;

	for (i = 0; i < files_cnt; i++) {
		tmp = strlen(filenames[i]);
		memcpy(ptr, filenames[i], tmp);
		ptr += tmp;
		*ptr = '\n';
		ptr += 1;
	}

	ret = simple_read_from_buffer(usr_buf, count, ppos, buf, len);

	kfree(buf);

bin_list_read_fail:
	if (file->f_path.dentry == target_list) {
		pc_release_target_names(&filenames);
	} else if (file->f_path.dentry == ignored_list)  {
		pc_release_ignored_names(&filenames);
	} else {
		/* Should never occur */
		printk(PRELOAD_PREFIX "%s() called for invalid file %s!\n", __func__,
		       file->f_path.dentry->d_name.name);
		ret = 0;
	}

bin_list_read_out:
	return ret;
}

static const struct file_operations bin_list_file_ops = {
	.owner = THIS_MODULE,
	.read = bin_list_read
};

static const struct file_operations bin_add_file_ops = {
	.owner = THIS_MODULE,
	.write = bin_add_write,
};

static const struct file_operations bin_remove_file_ops = {
	.owner = THIS_MODULE,
	.write = bin_remove_write,
};



/* ===========================================================================
 * =                             PTHREAD                                     =
 * ===========================================================================
 */

static ssize_t pthread_path_write(struct file *file, const char __user *buf,
				  size_t len, loff_t *ppos)
{
	return get_string_and_call(buf, len, pp_set_pthread_path);
}

static ssize_t init_off_write(struct file *file, const char __user *buf,
			      size_t len, loff_t *ppos)
{
	return get_ul_and_call(buf, len, pp_set_init_offset);
}

static const struct file_operations pthread_path_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = pthread_path_write,
};

static const struct file_operations pthread_init_off_fops = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write = init_off_write,
};






int pd_init(void)
{
	struct dentry *swap_dentry, *root, *target_path, *ignored_path,
		      *by_path, *by_pid, *by_id, *pthread, *dentry;
	int ret;

	ret = -ENODEV;
	if (!debugfs_initialized())
		goto fail;

	ret = -ENOENT;
	swap_dentry = swap_debugfs_getdir();
	if (!swap_dentry)
		goto fail;

	ret = -ENOMEM;
	root = swap_debugfs_create_dir(PRELOAD_FOLDER, swap_dentry);
	if (IS_ERR_OR_NULL(root))
		goto fail;

	preload_root = root;

	target_path = swap_debugfs_create_dir(PRELOAD_TARGET, root);
	if (IS_ERR_OR_NULL(target_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	target_list = swap_debugfs_create_file(PRELOAD_BINARIES_LIST,
					       PRELOAD_DEFAULT_PERMS,
					       target_path,
					       NULL, &bin_list_file_ops);
	if (IS_ERR_OR_NULL(target_list)) {
		ret = -ENOMEM;
		goto remove;
	}

	target_add = swap_debugfs_create_file(PRELOAD_BINARIES_ADD,
					      PRELOAD_DEFAULT_PERMS,
					      target_path,
					      NULL, &bin_add_file_ops);
	if (IS_ERR_OR_NULL(target_add)) {
		ret = -ENOMEM;
		goto remove;
	}

	target_remove = swap_debugfs_create_file(PRELOAD_BINARIES_REMOVE,
						 PRELOAD_DEFAULT_PERMS,
						 target_path,
						 NULL, &bin_remove_file_ops);
	if (IS_ERR_OR_NULL(target_remove)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_path = swap_debugfs_create_dir(PRELOAD_IGNORED, root);
	if (IS_ERR_OR_NULL(ignored_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_list = swap_debugfs_create_file(PRELOAD_BINARIES_LIST,
						PRELOAD_DEFAULT_PERMS,
						ignored_path,
						NULL, &bin_list_file_ops);
	if (IS_ERR_OR_NULL(ignored_list)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_add = swap_debugfs_create_file(PRELOAD_BINARIES_ADD,
					       PRELOAD_DEFAULT_PERMS,
					       ignored_path,
					       NULL, &bin_add_file_ops);
	if (IS_ERR_OR_NULL(ignored_add)) {
		ret = -ENOMEM;
		goto remove;
	}

	ignored_remove = swap_debugfs_create_file(PRELOAD_BINARIES_REMOVE,
						  PRELOAD_DEFAULT_PERMS,
						  ignored_path, NULL,
						  &bin_remove_file_ops);
	if (IS_ERR_OR_NULL(ignored_remove)) {
		ret = -ENOMEM;
		goto remove;
	}

	by_path = swap_debugfs_create_dir(PRELOAD_BY_PATH, root);
	if (IS_ERR_OR_NULL(by_path)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_ADD, PRELOAD_DEFAULT_PERMS,
					  by_path, NULL, &by_path_add_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_DEL, PRELOAD_DEFAULT_PERMS,
					  by_path, NULL, &by_path_del_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	by_pid = swap_debugfs_create_dir(PRELOAD_BY_PID, root);
	if (IS_ERR_OR_NULL(by_pid)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_ADD, PRELOAD_DEFAULT_PERMS,
					  by_pid, NULL, &by_pid_add_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_DEL, PRELOAD_DEFAULT_PERMS,
					  by_pid, NULL, &by_pid_del_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	by_id = swap_debugfs_create_dir(PRELOAD_BY_ID, root);
	if (IS_ERR_OR_NULL(by_id)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_ADD, PRELOAD_DEFAULT_PERMS,
					  by_id, NULL, &by_id_add_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_DEL, PRELOAD_DEFAULT_PERMS,
					  by_id, NULL, &by_id_del_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_DEL_ALL,
					  PRELOAD_DEFAULT_PERMS,
					  root, NULL, &del_all_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_ENABLE, PRELOAD_DEFAULT_PERMS,
					  root, NULL, &enable_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	pthread = swap_debugfs_create_dir(PRELOAD_PTHREAD, root);
	if (IS_ERR_OR_NULL(pthread)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_PATH, PRELOAD_DEFAULT_PERMS,
					  pthread, NULL, &pthread_path_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	dentry = swap_debugfs_create_file(PRELOAD_MINIMAL_INIT,
					  PRELOAD_DEFAULT_PERMS, pthread, NULL,
					  &pthread_init_off_fops);
	if (IS_ERR_OR_NULL(dentry)) {
		ret = -ENOMEM;
		goto remove;
	}

	return 0;

remove:

	debugfs_remove_recursive(root);

fail:
	printk(PRELOAD_PREFIX "Debugfs initialization failure: %d\n", ret);

	return ret;
}

void pd_exit(void)
{
	if (preload_root)
		debugfs_remove_recursive(preload_root);
	target_list = NULL;
	target_add = NULL;
	target_remove = NULL;
	ignored_list = NULL;
	ignored_add = NULL;
	ignored_remove = NULL;
	preload_root = NULL;
}
