/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include "swap_initializer.h"
#include "swap_debugfs.h"


static int change_permission(struct dentry *dentry)
{
	/* set UNIX permissions */
	dentry->d_inode->i_uid.val = 202; /* system_fw */
	dentry->d_inode->i_gid.val = 202; /* system_fw */

	return 0;
}

struct dentry *swap_debugfs_create_file(const char *name, umode_t mode,
					struct dentry *parent, void *data,
					const struct file_operations *fops)
{
	struct dentry *dentry;

	dentry = debugfs_create_file(name, mode, parent, data, fops);
	if (dentry) {
		if (change_permission(dentry)) {
			debugfs_remove(dentry);
			return NULL;
		}
	}

	return dentry;
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_file);

struct dentry *swap_debugfs_create_dir(const char *name, struct dentry *parent)
{
	struct dentry *dentry;

	dentry = debugfs_create_dir(name, parent);
	if (dentry) {
		if (change_permission(dentry)) {
			debugfs_remove(dentry);
			return NULL;
		}
	}

	return dentry;
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_dir);

struct dentry *swap_debugfs_create_x32(const char *name, umode_t mode,
				       struct dentry *parent, u32 *value)
{
	struct dentry *dentry;

	dentry = debugfs_create_x32(name, mode, parent, value);
	if (dentry) {
		if (change_permission(dentry)) {
			debugfs_remove(dentry);
			return NULL;
		}
	}

	return dentry;
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_x32);

struct dentry *swap_debugfs_create_x64(const char *name, umode_t mode,
				       struct dentry *parent, u64 *value)
{
	struct dentry *dentry;

	dentry = debugfs_create_x64(name, mode, parent, value);
	if (dentry) {
		if (change_permission(dentry)) {
			debugfs_remove(dentry);
			return NULL;
		}
	}

	return dentry;
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_x64);

struct dentry *swap_debugfs_create_u64(const char *name, umode_t mode,
				       struct dentry *parent, u64 *value)
{
	struct dentry *dentry;

	dentry = debugfs_create_u64(name, mode, parent, value);
	if (dentry) {
		if (change_permission(dentry)) {
			debugfs_remove(dentry);
			return NULL;
		}
	}

	return dentry;
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_u64);


/* based on define DEFINE_SIMPLE_ATTRIBUTE */
#define SWAP_DEFINE_SIMPLE_ATTRIBUTE(__fops, __get, __set, __fmt)	\
static int __fops ## _open(struct inode *inode, struct file *file)	\
{									\
	int ret;							\
									\
	ret = swap_init_simple_open(inode, file);			\
	if (ret)							\
		return ret;						\
									\
	__simple_attr_check_format(__fmt, 0ull);			\
	ret = simple_attr_open(inode, file, __get, __set, __fmt);	\
	if (ret)							\
		swap_init_simple_release(inode, file);			\
									\
	return ret;							\
}									\
static int __fops ## _release(struct inode *inode, struct file *file)	\
{									\
	simple_attr_release(inode, file);				\
	swap_init_simple_release(inode, file);				\
									\
	return 0;							\
}									\
static const struct file_operations __fops = {				\
	.owner   = THIS_MODULE,						\
	.open    = __fops ## _open,					\
	.release = __fops ## _release,					\
	.read    = simple_attr_read,					\
	.write   = simple_attr_write,					\
	.llseek  = generic_file_llseek,					\
}


static int fset_u64(void *data, u64 val)
{
	struct dfs_setget_64 *setget = data;

	return setget->set(val);
}

static int fget_u64(void *data, u64 *val)
{
	struct dfs_setget_64 *setget = data;

	*val = setget->get();
	return 0;
}

SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_setget_u64, fget_u64, fset_u64, "%llu\n");
SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_setget_u64_ro, fget_u64, NULL, "%llu\n");
SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_setget_u64_wo, NULL, fset_u64, "%llu\n");

SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_setget_x64, fget_u64, fset_u64, "0x%08llx\n");
SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_setget_x64_ro, fget_u64, NULL, "0x%08llx\n");
SWAP_DEFINE_SIMPLE_ATTRIBUTE(fops_setget_x64_wo, NULL, fset_u64, "0x%08llx\n");

static struct dentry *do_create_dfs_file(const char *name, umode_t mode,
					 struct dentry *parent, void *data,
					 const struct file_operations *fops,
					 const struct file_operations *fops_ro,
					 const struct file_operations *fops_wo)
{
	/* if there are no write bits set, make read only */
	if (!(mode & S_IWUGO))
		return swap_debugfs_create_file(name, mode, parent,
						data, fops_ro);
	/* if there are no read bits set, make write only */
	if (!(mode & S_IRUGO))
		return swap_debugfs_create_file(name, mode, parent,
						data, fops_wo);

	return swap_debugfs_create_file(name, mode, parent, data, fops);
}

struct dentry *swap_debugfs_create_setget_u64(const char *name, umode_t mode,
					      struct dentry *parent,
					      struct dfs_setget_64 *setget)
{
	return do_create_dfs_file(name, mode, parent, setget,
				  &fops_setget_u64,
				  &fops_setget_u64_ro,
				  &fops_setget_u64_wo);
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_setget_u64);

struct dentry *swap_debugfs_create_setget_x64(const char *name, umode_t mode,
					      struct dentry *parent,
					      struct dfs_setget_64 *setget)
{
	return do_create_dfs_file(name, mode, parent, setget,
				  &fops_setget_x64,
				  &fops_setget_x64_ro,
				  &fops_setget_x64_wo);
}
EXPORT_SYMBOL_GPL(swap_debugfs_create_setget_x64);


static int set_enable(int enable)
{
	int ret = 0, change, enable_current;

	enable_current = swap_init_stat_get();

	change = ((!!enable_current) << 1) | (!!enable);
	switch (change) {
	case 0b01: /* init */
		ret = swap_init_init();
		break;
	case 0b10: /* uninit */
		ret = swap_init_uninit();
		break;
	default:
		ret = -EINVAL;
		break;
	}

	swap_init_stat_put();

	return ret;
}

static ssize_t read_enable(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	char buf[3];
	int enable;

	enable = swap_init_stat_get();
	swap_init_stat_put();

	if (enable)
		buf[0] = '1';
	else
		buf[0] = '0';
	buf[1] = '\n';
	buf[2] = '\0';

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static int do_write_enable(const char *buf, size_t size)
{
	if (size < 1)
		return -EINVAL;

	switch (buf[0]) {
	case '1':
		return set_enable(1);
	case '0':
		return set_enable(0);
	}

	return -EINVAL;
}

static ssize_t write_enable(struct file *file, const char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	int ret;
	char buf[32];
	size_t buf_size;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	ret = do_write_enable(buf, buf_size);

	return ret ? ret : count;
}

static const struct file_operations fops_enable = {
	.owner = THIS_MODULE,
	.read = read_enable,
	.write = write_enable,
	.llseek = default_llseek,
};


static struct dentry *swap_dir;

/**
 * @brief Get debugfs dir.
 *
 * @return Pointer to dentry stuct.
 */
struct dentry *swap_debugfs_getdir(void)
{
	return swap_dir;
}
EXPORT_SYMBOL_GPL(swap_debugfs_getdir);

static int debugfs_dir_init(void)
{
	swap_dir = swap_debugfs_create_dir("swap", NULL);
	if (swap_dir == NULL)
		return -ENOMEM;

	return 0;
}

static void debugfs_dir_exit(void)
{
	struct dentry *dir = swap_dir;

	swap_dir = NULL;
	debugfs_remove_recursive(dir);
}

/**
 * @brief Initializes SWAP debugfs.
 *
 * @return 0 on success, negative error code on error.
 */
int swap_debugfs_init(void)
{
	int ret;
	struct dentry *dentry;

	ret = debugfs_dir_init();
	if (ret)
		return ret;

	dentry = swap_debugfs_create_file("enable", 0600, swap_dir, NULL,
					  &fops_enable);
	if (dentry == NULL) {
		debugfs_dir_exit();
		return -ENOMEM;
	}

	return 0;
}

/**
 * @brief Deinitializes SWAP debugfs and recursively removes all its files.
 *
 * @return Void.
 */
void swap_debugfs_uninit(void)
{
	debugfs_dir_exit();
}
