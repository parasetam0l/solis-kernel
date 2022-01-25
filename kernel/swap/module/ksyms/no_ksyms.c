/**
 * @file ksyms/no_ksyms.c
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
 * SWAP symbols searching implementation.
 */

#include "ksyms.h"
#include "ksyms_init.h"
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/fcntl.h>

/**
 * @def KSYMS_ERR
 * Error message define.
 */
#define KSYMS_ERR(format, args...) \
	do { \
		char *f = __FILE__; \
		char *n = strrchr(f, '/'); \
		printk(KERN_INFO "%s:%u \'%s\' ERROR: " format "\n" ,  \
		       (n) ? n+1 : f, __LINE__, __func__, ##args); \
	} while (0)

/**
 * @struct sys_map_item
 * @brief System map list item info.
 * @var sys_map_item::list
 * List pointer.
 * @var sys_map_item::addr
 * Symbol address.
 * @var sys_map_item::name
 * Symbol name.
 */
struct sys_map_item {
	struct list_head list;

	unsigned long addr;
	char *name;
};

static char *sm_path;
module_param(sm_path, charp, 0);

/**
 * @var smi_list
 * List of sys_map_item.
 */
LIST_HEAD(smi_list);
static struct file *file;

static int cnt_init_sm;

/**
 * @var cnt_init_sm_lock
 * System map items list lock.
 */
DEFINE_SEMAPHORE(cnt_init_sm_lock);

static int file_open(void)
{
	struct file *f = filp_open(sm_path, O_RDONLY, 0);

	if (IS_ERR(f)) {
		KSYMS_ERR("cannot open file \'%s\'", sm_path);
		return PTR_ERR(f);
	}

	file = f;

	return 0;
}

static void file_close(void)
{
	if (file) {
		int ret = filp_close(file, NULL);
		file = NULL;

		if (ret) {
			KSYMS_ERR("while closing file \'%s\' err=%d",
				  sm_path, ret);
		}
	}
}

static int file_check(void)
{
	int ret = file_open();
	if (ret == 0)
		file_close();

	return ret;
}

static long file_size(struct file *file)
{
	struct kstat st;
	if (vfs_getattr(file->f_path.mnt, file->f_path.dentry, &st))
		return -1;

	if (!S_ISREG(st.mode))
		return -1;

	if (st.size != (long)st.size)
		return -1;

	return st.size;
}

static struct sys_map_item *create_smi(unsigned long addr, const char *name)
{
	struct sys_map_item *smi = kmalloc(sizeof(*smi), GFP_KERNEL);

	if (smi == NULL) {
		KSYMS_ERR("not enough memory");
		return NULL;
	}

	smi->name = kmalloc(strlen(name) + 1, GFP_KERNEL);
	if (smi->name == NULL) {
		kfree(smi);
		KSYMS_ERR("not enough memory");
		return NULL;
	}

	INIT_LIST_HEAD(&smi->list);
	smi->addr = addr;
	strcpy(smi->name, name);

	return smi;
}

static void free_smi(struct sys_map_item *smi)
{
	kfree(smi->name);
	kfree(smi);
}

static void add_smi(struct sys_map_item *smi)
{
	list_add_tail(&smi->list, &smi_list);
}

static int is_endline(char c)
{
	return c == '\n' || c == '\r' || c == '\0';
}

static int is_symbol_attr(char c)
{
	return c == 't' || c == 'T';
}

static struct sys_map_item *get_sys_map_item(char *begin, char *end)
{
	struct sys_map_item *smi = NULL;
	int n, len = end - begin;
	unsigned long addr;
	char attr, name[128], *line;

	line = kmalloc(len + 1, GFP_KERNEL);
	memcpy(line, begin, len);
	line[len] = '\0';

	n = sscanf(line, "%lx %c %127s", &addr, &attr, name);
	name[127] = '\0';

	if (n != 3) {
		KSYMS_ERR("parsing line: \"%s\"", line);
		attr = '\0';
	}

	kfree(line);

	if (is_symbol_attr(attr))
		smi = create_smi(addr, name);

	return smi;
}


static void parsing(char *buf, int size)
{
	struct sys_map_item *smi;
	char *start, *end, *c;

	start = buf;
	end = buf + size;

	for (c = start; c < end; ++c) {
		if (is_endline(*c)) {
			smi = get_sys_map_item(start, c);
			if (smi)
				add_smi(smi);

			for (start = c; c < end; ++c) {
				if (!is_endline(*c)) {
					start = c;
					break;
				}
			}
		}
	}
}

static int create_sys_map(void)
{
	char *data;
	long size;
	int ret = file_open();

	if (ret)
		return ret;

	size = file_size(file);
	if (size < 0) {
		KSYMS_ERR("cannot get file size");
		ret = size;
		goto close;
	}

	data = vmalloc(size);
	if (data == NULL) {
		KSYMS_ERR("not enough memory");
		ret = -1;
		goto close;
	}

	if (kernel_read(file, 0, data, size) != size) {
		KSYMS_ERR("reading file %s", sm_path);
		ret = -1;
		goto free;
	}

	parsing(data, size);

free:
	vfree(data);

close:
	file_close();

	return 0;
}

static void free_sys_map(void)
{
	struct sys_map_item *smi, *n;
	list_for_each_entry_safe(smi, n, &smi_list, list) {
		list_del(&smi->list);
		free_smi(smi);
	}
}

/**
 * @brief Generates symbols list.
 *
 * @return 0 on success.
 */
int swap_get_ksyms(void)
{
	int ret = 0;

	down(&cnt_init_sm_lock);
	if (cnt_init_sm == 0)
		ret = create_sys_map();

	++cnt_init_sm;
	up(&cnt_init_sm_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_get_ksyms);

/**
 * @brief Frees symbols list.
 *
 * @return Void.
 */
void swap_put_ksyms(void)
{
	down(&cnt_init_sm_lock);
	--cnt_init_sm;
	if (cnt_init_sm == 0)
		free_sys_map();

	if (cnt_init_sm < 0) {
		KSYMS_ERR("cnt_init_sm=%d", cnt_init_sm);
		cnt_init_sm = 0;
	}

	up(&cnt_init_sm_lock);
}
EXPORT_SYMBOL_GPL(swap_put_ksyms);

/**
 * @brief Searches for symbol by its exact name.
 *
 * @param name Pointer the name string.
 * @return Symbol's address.
 */
unsigned long swap_ksyms(const char *name)
{
	struct sys_map_item *smi;

	list_for_each_entry(smi, &smi_list, list) {
		if (strcmp(name, smi->name) == 0)
			return smi->addr;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(swap_ksyms);

/**
 * @brief Searches for symbol by substring of its name.
 *
 * @param name Pointer to the name substring.
 * @return Symbol's address.
 */
unsigned long swap_ksyms_substr(const char *name)
{
	struct sys_map_item *smi;
	size_t len = strlen(name);

	list_for_each_entry(smi, &smi_list, list) {
		if (strncmp(name, smi->name, len) == 0)
			return smi->addr;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(swap_ksyms_substr);

/**
 * @brief SWAP ksyms module initialization.
 *
 * @return 0 on success, negative error code on error.
 */
int ksyms_init(void)
{
	int ret = 0;

	if (sm_path == NULL) {
		KSYMS_ERR("sm_path=NULL");
		return -EINVAL;
	}

	ret = file_check();
	if (ret)
		return -EINVAL;

	/* TODO: calling func 'swap_get_ksyms' in
	 * module used func 'swap_ksyms' */
	swap_get_ksyms();

	return 0;
}

/**
 * @brief SWAP ksyms module deinitialization.
 *
 * @return Void.
 */
void ksyms_exit(void)
{
	down(&cnt_init_sm_lock);

	if (cnt_init_sm > 0)
		free_sys_map();

	up(&cnt_init_sm_lock);
}
