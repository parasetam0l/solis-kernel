/**
 * writer/debugfs_writer.c
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Writer debugfs implementation.
 */


#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <master/swap_debugfs.h>
#include <master/swap_initializer.h>
#include "swap_msg.h"
#include "event_filter.h"


/* ============================================================================
 * ===                               BUFFER                                 ===
 * ============================================================================
 */
static char *common_buf;
enum { subbuf_size = 8*1024 };
enum { common_buf_size = subbuf_size * NR_CPUS };

static int init_buffer(void)
{
	common_buf = vmalloc(common_buf_size);

	return common_buf ? 0 : -ENOMEM;
}

static void exit_buffer(void)
{
	vfree(common_buf);
	common_buf = NULL;
}

static void *get_current_buf(void)
{
	return common_buf + subbuf_size * get_cpu();
}

static void put_current_buf(void)
{
	put_cpu();
}





/* ============================================================================
 * ===                             FOPS_RAW                                 ===
 * ============================================================================
 */
static ssize_t write_raw(struct file *file, const char __user *user_buf,
			 size_t count, loff_t *ppos)
{
	int ret;
	void *buf;

	if (count > subbuf_size)
		return -EINVAL;

	buf = get_current_buf();
	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto put_buf;
	}

	ret = swap_msg_raw(buf, count);

put_buf:
	put_current_buf();
	return ret;
}

static const struct file_operations fops_raw = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.write =	write_raw,
	.llseek =	default_llseek
};





/* ============================================================================
 * ===                        FOPS_AVAILABLE_FILTERS                        ===
 * ============================================================================
 */
struct read_buf {
	char *begin;
	char *ptr;
	char *end;
};

static void func_for_read(struct ev_filter *f, void *data)
{
	struct read_buf *rbuf = (struct read_buf *)data;
	int len = strlen(f->name);

	if (rbuf->end - rbuf->ptr < len + 2)
		return;

	if (rbuf->ptr != rbuf->begin) {
		*rbuf->ptr = ' ';
		++rbuf->ptr;
	}

	memcpy(rbuf->ptr, f->name, len);
	rbuf->ptr += len;
}

static ssize_t read_af(struct file *file, char __user *user_buf,
		       size_t count, loff_t *ppos)
{
	char buf[512];
	struct read_buf rbuf = {
		.begin = buf,
		.ptr = buf,
		.end = buf + sizeof(buf)
	};

	event_filter_on_each(func_for_read, (void *)&rbuf);

	*rbuf.ptr = '\n';
	++rbuf.ptr;

	return simple_read_from_buffer(user_buf, count, ppos,
				       rbuf.begin, rbuf.ptr - rbuf.begin);
}

static const struct file_operations fops_available_filters = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.read =		read_af,
	.llseek =	default_llseek
};





/* ============================================================================
 * ===                              FOPS_FILTER                             ===
 * ============================================================================
 */
static ssize_t read_filter(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	const char *name = event_filter_get();
	int len = strlen(name);
	char *buf;
	ssize_t ret;

	buf = kmalloc(len + 2, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	memcpy(buf, name, len);
	buf[len] = '\0';
	buf[len + 1] = '\n';

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len + 2);
	kfree(buf);

	return ret;
}

static ssize_t write_filter(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	enum { len = 32 };
	char buf[len], name[len];
	size_t buf_size;
	ssize_t ret;

	buf_size = min(count, (size_t)(len - 1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[len - 1] = '\0';
	ret = sscanf(buf, "%31s", name);
	if (ret != 1)
		return -EINVAL;

	ret = event_filter_set(name);
	if (ret)
		return -EINVAL;

	return count;
}

static const struct file_operations fops_filter = {
	.owner = THIS_MODULE,
	.open = swap_init_simple_open,
	.release = swap_init_simple_release,
	.read =		read_filter,
	.write =	write_filter,
	.llseek =	default_llseek
};





/* ============================================================================
 * ===                              INIT/EXIT                               ===
 * ============================================================================
 */
static struct dentry *writer_dir;

/**
 * @brief Removes writer debugfs.
 *
 * @return Void.
 */
void exit_debugfs_writer(void)
{
	if (writer_dir)
		debugfs_remove_recursive(writer_dir);

	writer_dir = NULL;

	exit_buffer();
}

/**
 * @brief Initializes writer debugfs.
 *
 * @return 0 on success, error code on error.
 */
int init_debugfs_writer(void)
{
	int ret;
	struct dentry *swap_dir, *dentry;

	ret = init_buffer();
	if (ret)
		return ret;

	swap_dir = swap_debugfs_getdir();
	if (swap_dir == NULL)
		return -ENOENT;

	writer_dir = swap_debugfs_create_dir("writer", swap_dir);
	if (writer_dir == NULL)
		return -ENOMEM;

	dentry = swap_debugfs_create_file("raw", 0600, writer_dir,
					  NULL, &fops_raw);
	if (dentry == NULL)
		goto fail;

	dentry = swap_debugfs_create_file("available_filters", 0600, writer_dir,
					  NULL, &fops_available_filters);
	if (dentry == NULL)
		goto fail;

	dentry = swap_debugfs_create_file("filter", 0600, writer_dir,
					  NULL, &fops_filter);
	if (dentry == NULL)
		goto fail;

	return 0;

fail:
	exit_debugfs_writer();
	return -ENOMEM;
}
