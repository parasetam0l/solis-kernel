/**
 *  webprobe/webprobe_debugfs.c
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015		 Anastasia Lyupa <a.lyupa@samsung.com>
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <master/swap_debugfs.h>
#include <master/swap_initializer.h>

#include "webprobe_debugfs.h"
#include "webprobe.h"

static const char ENABLED_FILE[] =		"enabled";
static const char APP_INFO_FILE[] =		"app_info";
static const char INSPSERVER_START_FILE[] =	"inspector_server_start";
static const char TICK_PROBE_FILE[] =		"tick_probe";

enum { max_count = 256 };
static char app_info[max_count];

/* ============================================================================
 * ===              DEBUGFS FOR WEBPROBE INSTRUMENTATION                    ===
 * ============================================================================
 */

static ssize_t read_enabled(struct file *file, char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	char buf[2];

	buf[0] = web_prof_enabled() ? '1' : '0';
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
		ret = web_prof_enable();
		break;
	case '0':
		ret = web_prof_disable();
		break;
	default:
		return -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_enabled = {
	.write =	write_enabled,
	.read =		read_enabled,
	.open =		swap_init_simple_open,
	.release =	swap_init_simple_release,
};

static ssize_t write_app_info(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	int ret = 0;
	char *buf, *path, *id;
	int n;

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

	path = kmalloc(count, GFP_KERNEL);
	if (!path) {
		ret = -ENOMEM;
		goto free_buf;
	}

	id = kmalloc(count, GFP_KERNEL);
	if (!id) {
		ret = -ENOMEM;
		goto free_path;
	}

	n = sscanf(buf, "%s %s", path, id);

	if (n != 2) {
		ret = -EINVAL;
		goto free_app_info;
	}

	web_prof_data_set(path, id);
	snprintf(app_info, sizeof(app_info), "%s\n", buf);

free_app_info:
	kfree(id);
free_path:
	kfree(path);
free_buf:
	kfree(buf);

	return ret ? ret : count;
}

static ssize_t read_app_info(struct file *file, char __user *userbuf,
			     size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(userbuf, count, ppos, app_info,
				       sizeof(app_info) - 1);
}

static const struct file_operations fops_app_info = {
	.write =	write_app_info,
	.read =		read_app_info,
	.open =		swap_init_simple_open,
	.release =	swap_init_simple_release,
};

/* ============================================================================
 * ===                             INIT/EXIT                                ===
 * ============================================================================
 */

static struct dentry *webprobe_dir;

void webprobe_debugfs_exit(void)
{
	debugfs_remove_recursive(webprobe_dir);
	webprobe_dir = NULL;
}

int webprobe_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = swap_debugfs_getdir();
	if (!dentry)
		return -ENOENT;

	webprobe_dir = swap_debugfs_create_dir("webprobe", dentry);
	if (!webprobe_dir)
		return -ENOMEM;

	dentry = swap_debugfs_create_file(ENABLED_FILE, 0600, webprobe_dir,
					  NULL, &fops_enabled);

	dentry = swap_debugfs_create_file(APP_INFO_FILE, 0600, webprobe_dir,
					  NULL, &fops_app_info);
	if (!dentry)
		goto fail;

	dentry = swap_debugfs_create_x64(INSPSERVER_START_FILE, 0600,
					 webprobe_dir,
					 web_prof_addr_ptr(INSPSERVER_START));
	if (!dentry)
		goto fail;

	dentry = swap_debugfs_create_x64(TICK_PROBE_FILE, 0600, webprobe_dir,
					 web_prof_addr_ptr(TICK_PROBE));
	if (!dentry)
		goto fail;

	return 0;

fail:
	webprobe_debugfs_exit();
	return -ENOMEM;
}
