#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <master/swap_debugfs.h>
#include "uihv.h"
#include "uihv_module.h"

static const char UIHV_FOLDER[] = "uihv";
static const char UIHV_PATH[] = "path";
static const char UIHV_APP_INFO[] = "app_info";
static const char UIHV_ENABLE[] = "enable";

static struct dentry *uihv_root;



/* ===========================================================================
 * =                           UI VIEWER PATH                                =
 * ===========================================================================
 */


static ssize_t uihv_path_write(struct file *file, const char __user *buf,
			       size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (path == NULL) {
		ret = -ENOMEM;
		goto uihv_path_write_out;
	}

	if (copy_from_user(path, buf, len)) {
		ret = -EINVAL;
		goto uihv_path_write_out;
	}

	path[len - 1] = '\0';

	if (uihv_set_handler(path) != 0) {
		printk(UIHV_PREFIX "Cannot set ui viewer path %s\n", path);
		ret = -EINVAL;
		goto uihv_path_write_out;
	}

	ret = len;

	printk(UIHV_PREFIX "Set ui viewer path %s\n", path);

uihv_path_write_out:
	kfree(path);

	return ret;
}

static const struct file_operations uihv_path_file_ops = {
	.owner = THIS_MODULE,
	.write = uihv_path_write,
};


/*
 * format:
 *	main:app_path
 *
 * sample:
 *	0x00000d60:/bin/app_sample
 */
static int uihv_add_app_info(const char *buf, size_t len)
{
	int n, ret;
	char *app_path;
	unsigned long main_addr;
	const char fmt[] = "%%lx:/%%%ds";
	char fmt_buf[64];

	n = snprintf(fmt_buf, sizeof(fmt_buf), fmt, PATH_MAX - 2);
	if (n <= 0)
		return -EINVAL;

	app_path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (app_path == NULL)
		return -ENOMEM;

	n = sscanf(buf, fmt_buf, &main_addr, app_path + 1);
	if (n != 2) {
		ret = -EINVAL;
		goto free_app_path;
	}
	app_path[0] = '/';

	printk(UIHV_PREFIX "Set ui viewer app path %s, main offset 0x%lx\n", app_path, main_addr);

	ret = uihv_data_set(app_path, main_addr);

free_app_path:
	kfree(app_path);
	return ret;
}

static ssize_t write_uihv_app_info(struct file *file,
					const char __user *user_buf,
					size_t len, loff_t *ppos)
{
	ssize_t ret = len;
	char *buf;

	buf = kmalloc(len, GFP_KERNEL);
	if (buf == NULL) {
		ret = -ENOMEM;
		goto free_buf;
	}

	if (copy_from_user(buf, user_buf, len)) {
		ret = -EINVAL;
		goto free_buf;
	}

	buf[len - 1] = '\0';

	if (uihv_add_app_info(buf, len))
		ret = -EINVAL;

free_buf:
	kfree(buf);

	return ret;
}

static const struct file_operations uihv_app_info_file_ops = {
	.owner = THIS_MODULE,
	.write = write_uihv_app_info,
};

static ssize_t write_uihv_enable(struct file *file,
				 const char __user *user_buf,
				 size_t len, loff_t *ppos)
{
	ssize_t ret = len;
	char *buf;

	buf = kmalloc(len, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(buf, user_buf, len)) {
		ret = -EINVAL;
		goto free_buf;
	}

	buf[len - 1] = '\0';

	if (buf[0] == '0')
		ret = uihv_disable();
	else
		ret = uihv_enable();

free_buf:
	kfree(buf);

out:
	return ret;
}

static const struct file_operations uihv_enable_file_ops = {
	.owner = THIS_MODULE,
	.write = write_uihv_enable,
};

int uihv_dfs_init(void)
{
	struct dentry *swap_dentry, *root, *path, *app_info, *uihv_enable;
	int ret;

	ret = -ENODEV;
	if (!debugfs_initialized())
		goto fail;

	ret = -ENOENT;
	swap_dentry = swap_debugfs_getdir();
	if (!swap_dentry)
		goto fail;

	ret = -ENOMEM;
	root = swap_debugfs_create_dir(UIHV_FOLDER, swap_dentry);
	if (IS_ERR_OR_NULL(root))
		goto fail;

	uihv_root = root;

	path = swap_debugfs_create_file(UIHV_PATH, UIHV_DEFAULT_PERMS, root,
					NULL, &uihv_path_file_ops);
	if (IS_ERR_OR_NULL(path)) {
		ret = -ENOMEM;
		goto remove;
	}

	app_info = swap_debugfs_create_file(UIHV_APP_INFO,
					    UIHV_DEFAULT_PERMS, root, NULL,
					    &uihv_app_info_file_ops);
	if (IS_ERR_OR_NULL(app_info)) {
		ret = -ENOMEM;
		goto remove;
	}

	uihv_enable = swap_debugfs_create_file(UIHV_ENABLE,
					       UIHV_DEFAULT_PERMS, root, NULL,
					       &uihv_enable_file_ops);
	if (IS_ERR_OR_NULL(uihv_enable)) {
		ret = -ENOMEM;
		goto remove;
	}

	return 0;

remove:
	debugfs_remove_recursive(root);

fail:
	printk(UIHV_PREFIX "Debugfs initialization failure: %d\n", ret);

	return ret;
}

void uihv_dfs_exit(void)
{
	if (uihv_root)
		debugfs_remove_recursive(uihv_root);

	uihv_root = NULL;
}
