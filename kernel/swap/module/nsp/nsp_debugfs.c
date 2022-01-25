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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/slab.h>
#include <linux/limits.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <master/swap_debugfs.h>
#include "nsp.h"


/* remove end-line symbols */
static void rm_endline_symbols(char *buf, size_t len)
{
	char *p, *buf_end;

	buf_end = buf + len;
	for (p = buf; p != buf_end; ++p)
		if (*p == '\n' || *p == '\r')
			*p = '\0';
}

/*
 * format:
 *	main:app_path
 *
 * sample:
 *	0x00000d60:/bin/app_sample
 */
static int do_add(const char *buf, size_t len)
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

	ret = nsp_add(app_path, main_addr);

free_app_path:
	kfree(app_path);
	return ret;
}

/*
 * format:
 *	path
 *
 * sample:
 *	/tmp/sample
 */
static int do_rm(const char *buf, size_t len)
{
	return nsp_rm(buf);
}

static int do_rm_all(const char *buf, size_t len)
{
	return nsp_rm_all();
}

/*
 * format:
 *	dlopen_addr@plt:dlsym_addr@plt:launchpad_path
 *
 * sample:
 *	0x000234:0x000342:/usr/bin/launchpad-loader
 */
static int do_set_lpad_info(const char *data, size_t len)
{
	int n, ret;
	unsigned long dlopen_addr;
	unsigned long dlsym_addr;
	char *lpad_path;
	const char fmt[] = "%%lx:%%lx:/%%%ds";
	char fmt_buf[64];

	n = snprintf(fmt_buf, sizeof(fmt_buf), fmt, PATH_MAX - 2);
	if (n <= 0)
		return -EINVAL;

	lpad_path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (lpad_path == NULL)
		return -ENOMEM;

	n = sscanf(data, fmt_buf, &dlopen_addr, &dlsym_addr, lpad_path + 1);
	if (n != 3) {
		ret = -EINVAL;
		goto free_lpad_path;
	}
	lpad_path[0] = '/';

	ret = nsp_set_lpad_info(lpad_path, dlopen_addr, dlsym_addr);

free_lpad_path:
	kfree(lpad_path);
	return ret;
}

/*
 * format:
 *	appcore_efl_init:__do_app:libappcore-efl ui_app_init:elm_run@plt:libcapi-appfw-application
 *
 * sample:
 *	0x2000:0x2ed0:/usr/lib/libappcore-efl.so.1 0x2b20:0x11f0:/usr/lib/libcapi-appfw-application.so.0
 */
static int do_set_appcore_info(const char *data, size_t len)
{
	int n, ret;
	struct nsp_info_data info;
	const char fmt[] = "%%lx:%%lx:/%%%ds %%lx:%%lx:/%%%ds";
	char fmt_buf[64];
	char *appcore_path, *capi_path;

	n = snprintf(fmt_buf, sizeof(fmt_buf), fmt, PATH_MAX - 2, PATH_MAX - 2);
	if (n <= 0)
		return -EINVAL;

	appcore_path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!appcore_path)
		return -ENOMEM;

	capi_path = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!capi_path) {
		ret = -ENOMEM;
		goto fail_alloc;
	}

	n = sscanf(data, fmt_buf,
		   &info.ac_efl_init, &info.do_app, appcore_path + 1,
		   &info.ac_init, &info.elm_run, capi_path + 1);
	if (n != 6) {
		ret = -EINVAL;
		goto fail;
	}
	appcore_path[0] = '/';
	capi_path[0] = '/';

	info.appcore_path = appcore_path;
	info.capi_path = capi_path;
	ret = nsp_set_appcore_info(&info);

fail:
	kfree(capi_path);
fail_alloc:
	kfree(appcore_path);
	return ret;
}

/*
 * format:
 *	0 byte - type
 *	1 byte - ' '
 *	2.. bytes - data
 */
static int do_cmd(const char *data, size_t len)
{
	char type;
	size_t len_data;
	const char *cmd_data;

	if (len) {
		if (data[0] == 'c')
			return do_rm_all(data + 1, len - 1);
	}
	/*
	 * 0 byte - type
	 * 1 byte - ' '
	 */
	if (len < 2 || data[1] != ' ')
		return -EINVAL;

	len_data = len - 2;
	cmd_data = data + 2;
	type = data[0];
	switch (type) {
	case 'a':
		return do_add(cmd_data, len_data);
	case 'b':
		return do_set_lpad_info(cmd_data, len_data);
	case 'l':
		return do_set_appcore_info(cmd_data, len_data);
	case 'r':
		return do_rm(cmd_data, len_data);
	default:
		return -EINVAL;
	}

	return 0;
}




/* ============================================================================
 * ===                          DEBUGFS FOR CMD                             ===
 * ============================================================================
 */
static ssize_t write_cmd(struct file *file, const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	char *buf;
	ssize_t ret = count;

	buf = kmalloc(count + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto free_buf;
	}

	buf[count] = '\0';
	rm_endline_symbols(buf, count);

	if (do_cmd(buf, count))
		ret = -EINVAL;

free_buf:
	kfree(buf);

	return ret;
}

static ssize_t read_cmd(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	const char help[] =
			"use:\n"
			"\ta $app_path - add\n"
			"\tr $app_path - remove\n"
			"\tc - remove all\n"
			"\tb dlopen_addr@plt:dlsym_addr@plt:launchpad_path\n"
			"\tl appcore_efl_init:__do_app:libappcore-efl_path ui_app_init:elm_run@plt:libcapi-appfw-application_path\n";
	ssize_t ret;

	ret = simple_read_from_buffer(user_buf, count, ppos,
				      help, sizeof(help));

	return ret;
}

static const struct file_operations fops_cmd = {
	.read =		read_cmd,
	.write =	write_cmd,
	.llseek =	default_llseek
};




/* ============================================================================
 * ===                         DEBUGFS FOR ENABLE                           ===
 * ============================================================================
 */
static ssize_t read_enabled(struct file *file, char /*__user*/ *user_buf,
			    size_t count, loff_t *ppos)
{
	char buf[2];

	buf[0] = nsp_get_stat() == NS_OFF ? '0' : '1';
	buf[1] = '\n';

	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t write_enabled(struct file *file, const char /*__user*/ *user_buf,
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
		ret = nsp_set_stat(NS_ON);
		break;
	case '0':
		ret = nsp_set_stat(NS_OFF);
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




static struct dentry *nsp_dir = NULL;

void nsp_debugfs_exit(void)
{
	if (nsp_dir)
		debugfs_remove_recursive(nsp_dir);

	nsp_dir = NULL;
}

int nsp_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = swap_debugfs_getdir();
	if (dentry == NULL)
		return -ENOENT;

	nsp_dir = swap_debugfs_create_dir("nsp", dentry);
	if (nsp_dir == NULL)
		return -ENOMEM;

	dentry = swap_debugfs_create_file("cmd", 0600, nsp_dir, NULL,
					  &fops_cmd);
	if (dentry == NULL)
		goto fail;

	dentry = swap_debugfs_create_file("enabled", 0600, nsp_dir, NULL,
					  &fops_enabled);
	if (dentry == NULL)
		goto fail;

	return 0;

fail:
	nsp_debugfs_exit();
	return -ENOMEM;
}
