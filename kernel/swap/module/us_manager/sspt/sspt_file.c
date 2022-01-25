/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_file.c
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */

#include "sspt.h"
#include "sspt_file.h"
#include "sspt_page.h"
#include "sspt_proc.h"
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/sched.h>
#include <kprobe/swap_kprobes_deps.h>
#include <us_manager/probes/probes.h>
#include <us_manager/img/img_ip.h>

static int calculation_hash_bits(int cnt)
{
	int bits;
	for (bits = 1; cnt >>= 1; ++bits)
		;

	return bits;
}

static unsigned long htable_size(const struct sspt_file *file)
{
	return 1 << file->htable.bits;
}

static struct hlist_head *htable_head_by_idx(const struct sspt_file *file,
					     unsigned long idx)
{
	return &file->htable.heads[idx];
}

static struct hlist_head *htable_head_by_key(const struct sspt_file *file,
					     unsigned long offset)
{
	unsigned long idx = hash_ptr((void *)offset, file->htable.bits);

	return htable_head_by_idx(file, idx);
}

/**
 * @brief Create sspt_file struct
 *
 * @param dentry Dentry of file
 * @param page_cnt Size of hash-table
 * @return Pointer to the created sspt_file struct
 */
struct sspt_file *sspt_file_create(struct dentry *dentry, int page_cnt)
{
	int i, table_size;
	struct hlist_head *heads;
	struct sspt_file *obj = kmalloc(sizeof(*obj), GFP_ATOMIC);

	if (obj == NULL)
		return NULL;

	INIT_LIST_HEAD(&obj->list);
	obj->proc = NULL;
	obj->dentry = dentry;
	obj->loaded = 0;
	obj->vm_start = 0;
	obj->vm_end = 0;

	obj->htable.bits = calculation_hash_bits(page_cnt);
	table_size = htable_size(obj);

	heads = kmalloc(sizeof(*obj->htable.heads) * table_size, GFP_ATOMIC);
	if (heads == NULL)
		goto err;

	for (i = 0; i < table_size; ++i)
		INIT_HLIST_HEAD(&heads[i]);

	obj->htable.heads = heads;
	init_rwsem(&obj->htable.sem);

	return obj;

err:
	kfree(obj);
	return NULL;
}

/**
 * @brief Remove sspt_file struct
 *
 * @param file remove object
 * @return Void
 */
void sspt_file_free(struct sspt_file *file)
{
	struct hlist_head *head;
	struct sspt_page *page;
	int i, table_size = htable_size(file);
	struct hlist_node *n;
	DECLARE_NODE_PTR_FOR_HLIST(p);

	down_write(&file->htable.sem);
	for (i = 0; i < table_size; ++i) {
		head = htable_head_by_idx(file, i);
		swap_hlist_for_each_entry_safe(page, p, n, head, hlist) {
			hlist_del(&page->hlist);
			sspt_page_clean(page);
			sspt_page_put(page);
		}
	}
	up_write(&file->htable.sem);

	kfree(file->htable.heads);
	kfree(file);
}

static void sspt_add_page(struct sspt_file *file, struct sspt_page *page)
{
	page->file = file;
	hlist_add_head(&page->hlist, htable_head_by_key(file, page->offset));
}

static struct sspt_page *sspt_find_page(struct sspt_file *file,
					unsigned long offset)
{
	struct hlist_head *head = htable_head_by_key(file, offset);
	struct sspt_page *page;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	swap_hlist_for_each_entry(page, node, head, hlist) {
		if (page->offset == offset)
			return page;
	}

	return NULL;
}

static struct sspt_page *sspt_find_page_or_new(struct sspt_file *file,
					       unsigned long offset)
{
	struct sspt_page *page;

	down_write(&file->htable.sem);
	page = sspt_find_page(file, offset);
	if (page == NULL) {
		page = sspt_page_create(offset);
		if (page)
			sspt_add_page(file, page);
	}
	up_write(&file->htable.sem);

	return page;
}

/**
 * @brief Get sspt_page from sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param page Page address
 * @return Pointer to the sspt_page struct
 */
struct sspt_page *sspt_find_page_mapped(struct sspt_file *file,
					unsigned long page)
{
	unsigned long offset;
	struct sspt_page *p;

	if (file->vm_start > page || file->vm_end < page) {
		/* TODO: or panic?! */
		printk(KERN_INFO "ERROR: file_p[vm_start..vm_end] <> page: "
		       "file_p[vm_start=%lx, vm_end=%lx, "
		       "d_iname=%s] page=%lx\n",
		       file->vm_start, file->vm_end,
		       file->dentry->d_iname, page);
		return NULL;
	}

	offset = page - file->vm_start;

	down_read(&file->htable.sem);
	p = sspt_find_page(file, offset);
	up_read(&file->htable.sem);

	return p;
}

/**
 * @brief Add instruction pointer to sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param offset File offset
 * @param args Function arguments
 * @param ret_type Return type
 * @return Void
 */
void sspt_file_add_ip(struct sspt_file *file, struct img_ip *img_ip)
{
	unsigned long offset = 0;
	struct sspt_page *page = NULL;
	struct sspt_ip *ip = NULL;

	offset = img_ip->addr & PAGE_MASK;
	page = sspt_find_page_or_new(file, offset);
	if (!page)
		return;

	ip = sspt_ip_create(img_ip, page);
	if (!ip)
		return;

	probe_info_init(ip->desc->type, ip);
}

void sspt_file_on_each_ip(struct sspt_file *file,
			  void (*func)(struct sspt_ip *, void *), void *data)
{
	int i;
	const int table_size = htable_size(file);
	struct sspt_page *page;
	struct hlist_head *head;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	down_read(&file->htable.sem);
	for (i = 0; i < table_size; ++i) {
		head = htable_head_by_idx(file, i);
		swap_hlist_for_each_entry(page, node, head, hlist)
			sspt_page_on_each_ip(page, func, data);
	}
	up_read(&file->htable.sem);
}

/**
 * @brief Check install sspt_file (legacy code, it is need remove)
 *
 * @param file Pointer to the sspt_file struct
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int sspt_file_check_install_pages(struct sspt_file *file)
{
	int ret = 0;
	int i, table_size;
	struct sspt_page *page;
	struct hlist_head *head;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	table_size = htable_size(file);

	down_read(&file->htable.sem);
	for (i = 0; i < table_size; ++i) {
		head = htable_head_by_idx(file, i);
		swap_hlist_for_each_entry(page, node, head, hlist) {
			if (sspt_page_is_installed(page)) {
				ret = 1;
				goto unlock;
			}
		}
	}

unlock:
	up_read(&file->htable.sem);
	return ret;
}

/**
 * @brief Install sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @return Void
 */
void sspt_file_install(struct sspt_file *file)
{
	struct sspt_page *page = NULL;
	struct hlist_head *head = NULL;
	int i, table_size = htable_size(file);
	unsigned long page_addr;
	struct mm_struct *mm;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	down_read(&file->htable.sem);
	for (i = 0; i < table_size; ++i) {
		head = htable_head_by_idx(file, i);
		swap_hlist_for_each_entry(page, node, head, hlist) {
			unsigned long offset = page->offset;

#ifdef CONFIG_64BIT
			/* Reset most significant bit for only 64-bit */
			offset &= (~(1UL << 63));
#endif /* CONFIG_64BIT */

			page_addr = file->vm_start + offset;
			if (page_addr < file->vm_start ||
			    page_addr >= file->vm_end)
				continue;

			mm = page->file->proc->leader->mm;
			if (page_present(mm, page_addr))
				sspt_register_page(page, file);
		}
	}
	up_read(&file->htable.sem);
}

/**
 * @brief Uninstall sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param task Pointer to the task_stract struct
 * @param flag Action for probes
 * @return Void
 */
int sspt_file_uninstall(struct sspt_file *file,
			struct task_struct *task,
			enum US_FLAGS flag)
{
	int i, err = 0;
	int table_size = htable_size(file);
	struct sspt_page *page;
	struct hlist_head *head;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	down_read(&file->htable.sem);
	for (i = 0; i < table_size; ++i) {
		head = htable_head_by_idx(file, i);
		swap_hlist_for_each_entry(page, node, head, hlist) {
			err = sspt_unregister_page(page, flag, task);
			if (err != 0) {
				printk(KERN_INFO "ERROR sspt_file_uninstall: "
				       "err=%d\n", err);
				up_read(&file->htable.sem);
				return err;
			}
		}
	}
	up_read(&file->htable.sem);

	if (flag != US_DISARM) {
		file->loaded = 0;
		file->vm_start = 0;
		file->vm_end = 0;
	}

	return err;
}

/**
 * @brief Set mapping for sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param vma Pointer to the vm_area_struct struct
 * @return Void
 */
void sspt_file_set_mapping(struct sspt_file *file, struct vm_area_struct *vma)
{
	if (file->loaded == 0) {
		file->loaded = 1;
		file->vm_start = vma->vm_start;
		file->vm_end = vma->vm_end;
	}
}
