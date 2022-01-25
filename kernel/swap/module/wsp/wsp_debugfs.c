/*
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
 *
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Web startup profiling
 */

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <master/swap_debugfs.h>
#include "wsp.h"
#include "wsp_debugfs.h"

static int do_write_cmd(const char *buf, size_t count)
{
	int n, ret = 0;
	char *name;
	unsigned long offset;

	name = kmalloc(count, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	n = sscanf(buf, "%lx %1024s", &offset, name);
	if (n != 2) {
		ret = -EINVAL;
		goto free_name;
	}

	ret = wsp_set_addr(name, offset);

free_name:
	kfree(name);
	return ret;
}

/* ============================================================================
 * ===                          DEBUGFS FOR CMD                             ===
 * ============================================================================
 */
static ssize_t write_cmd(struct file *file, const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	enum { max_count = 1024 };
	int ret;
	char *buf;

	if (count > max_count)
		return -ENOMEM;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto free_buf;
	}

	buf[count] = '\0';
	ret = do_write_cmd(buf, count);

free_buf:
	kfree(buf);
	return ret ? ret : count;
}

static const struct file_operations fops_cmd = {
	.write =	write_cmd,
	.llseek =	default_llseek,
};

/* ============================================================================
 * ===                         DEBUGFS FOR ENABLE                           ===
 * ============================================================================
 */
static ssize_t read_enabled(struct file *file, char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	char buf[2];

	buf[0] = wsp_get_mode() == WSP_OFF ? '0' : '1';
	buf[1] = '\n';

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t write_enabled(struct file *file, const char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	int ret = 0;
	char buf[32];
	size_t buf_size;

	buf_size = min(count, (sizeof(buf) - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size] = '\0';
	switch (buf[0]) {
	case '1':
		ret = wsp_set_mode(WSP_ON);
		break;
	case '0':
		ret = wsp_set_mode(WSP_OFF);
		break;
	default:
		return -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_enabled = {
	.read =		read_enabled,
	.write =	write_enabled,
	.llseek =	default_llseek,
};

/* ============================================================================
 * ===                       DEBUGFS FOR WEBAPP_PATH                        ===
 * ============================================================================
 */
static ssize_t write_webapp_path(struct file *file,
				 const char __user *user_buf,
				 size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (!path) {
		ret = -ENOMEM;
		goto write_webapp_path_failed;
	}

	if (copy_from_user(path, user_buf, len)) {
		ret = -EINVAL;
		goto write_webapp_path_failed;
	}

	path[len - 1] = '\0';
	wsp_set_webapp_path(path, len);

	ret = len;

write_webapp_path_failed:
	kfree(path);

	return ret;
}

static const struct file_operations fops_webapp_path = {
	.write = write_webapp_path
};

/* ============================================================================
 * ===                      DEBUGFS FOR EWEBKIT_PATH                        ===
 * ============================================================================
 */
static ssize_t write_ewebkit_path(struct file *file,
				  const char __user *user_buf,
				  size_t len, loff_t *ppos)
{
	ssize_t ret;
	char *path;

	path = kmalloc(len, GFP_KERNEL);
	if (!path) {
		ret = -ENOMEM;
		goto write_ewebkit_path_failed;
	}

	if (copy_from_user(path, user_buf, len)) {
		ret = -EINVAL;
		goto write_ewebkit_path_failed;
	}

	path[len - 1] = '\0';

	wsp_set_chromium_path(path, len);

	ret = len;

write_ewebkit_path_failed:
	kfree(path);

	return ret;
}

static const struct file_operations fops_ewebkit_path = {
	.write = write_ewebkit_path
};

static struct dentry *wsp_dir;

void wsp_debugfs_exit(void)
{
	debugfs_remove_recursive(wsp_dir);
	wsp_dir = NULL;
}

int wsp_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = swap_debugfs_getdir();
	if (!dentry)
		return -ENOENT;

	wsp_dir = swap_debugfs_create_dir("wsp", dentry);
	if (!wsp_dir)
		return -ENOMEM;

	dentry = swap_debugfs_create_file("enabled", 0600, wsp_dir, NULL,
					  &fops_enabled);
	if (!dentry)
		goto fail;

	dentry = swap_debugfs_create_file("cmd", 0600, wsp_dir, NULL,
					  &fops_cmd);
	if (!dentry)
		goto fail;

	dentry = swap_debugfs_create_file("webapp_path", 0600, wsp_dir, NULL,
					  &fops_webapp_path);
	if (!dentry)
		goto fail;

	dentry = swap_debugfs_create_file("ewebkit_path", 0600, wsp_dir, NULL,
					  &fops_ewebkit_path);
	if (!dentry)
		goto fail;

	return 0;

fail:
	wsp_debugfs_exit();
	return -ENOMEM;
}
