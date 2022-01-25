/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_page.c
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
#include "sspt_page.h"
#include "sspt_file.h"
#include "sspt_ip.h"
#include <us_manager/probes/use_probes.h>
#include <linux/slab.h>
#include <linux/list.h>

/**
 * @brief Create sspt_page struct
 *
 * @param offset File ofset
 * @return Pointer to the created sspt_page struct
 */
struct sspt_page *sspt_page_create(unsigned long offset)
{
	struct sspt_page *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
	if (obj) {
		INIT_HLIST_NODE(&obj->hlist);
		mutex_init(&obj->ip_list.mtx);
		INIT_LIST_HEAD(&obj->ip_list.inst);
		INIT_LIST_HEAD(&obj->ip_list.not_inst);
		obj->offset = offset;
		obj->file = NULL;
		kref_init(&obj->ref);
	}

	return obj;
}

void sspt_page_clean(struct sspt_page *page)
{
	struct sspt_ip *ip, *n;
	LIST_HEAD(head);

	sspt_page_lock(page);
	WARN_ON(!list_empty(&page->ip_list.inst));
	list_for_each_entry_safe(ip, n, &page->ip_list.inst, list) {
		sspt_ip_get(ip);
		list_move(&ip->list, &head);
	}

	list_for_each_entry_safe(ip, n, &page->ip_list.not_inst, list) {
		sspt_ip_get(ip);
		list_move(&ip->list, &head);
	}

	while (!list_empty(&head)) {
		ip = list_first_entry(&head, struct sspt_ip ,list);
		sspt_page_unlock(page);

		sspt_ip_clean(ip);

		sspt_page_lock(page);
		sspt_ip_put(ip);
	}
	sspt_page_unlock(page);
}

static void sspt_page_release(struct kref *ref)
{
	struct sspt_page *page = container_of(ref, struct sspt_page, ref);

	WARN_ON(!list_empty(&page->ip_list.inst) ||
		!list_empty(&page->ip_list.not_inst));

	kfree(page);
}

void sspt_page_get(struct sspt_page *page)
{
	kref_get(&page->ref);
}

void sspt_page_put(struct sspt_page *page)
{
	kref_put(&page->ref, sspt_page_release);
}

/**
 * @brief Add instruction pointer to sspt_page
 *
 * @param page Pointer to the sspt_page struct
 * @param ip Pointer to the us_ip struct
 * @return Void
 */
void sspt_page_add_ip(struct sspt_page *page, struct sspt_ip *ip)
{
	ip->page = page;
	list_add(&ip->list, &page->ip_list.not_inst);
}

void sspt_page_lock(struct sspt_page *page)
{
	mutex_lock(&page->ip_list.mtx);
}

void sspt_page_unlock(struct sspt_page *page)
{
	mutex_unlock(&page->ip_list.mtx);
}

/**
 * @brief Check if probes are set on the page
 *
 * @param page Pointer to the sspt_page struct
 * @return Boolean
 */
bool sspt_page_is_installed(struct sspt_page *page)
{
	return !list_empty(&page->ip_list.inst);
}

bool sspt_page_is_installed_ip(struct sspt_page *page, struct sspt_ip *ip)
{
	bool result = false; /* not installed by default */
	struct sspt_ip *p;

	sspt_page_lock(page);
	list_for_each_entry(p, &page->ip_list.inst, list) {
		if (p == ip) {
			result = true;
			goto unlock;
		}
	}

unlock:
	sspt_page_unlock(page);

	return result;
}

/**
 * @brief Install probes on the page
 *
 * @param page Pointer to the sspt_page struct
 * @param file Pointer to the sspt_file struct
 * @return Error code
 */
int sspt_register_page(struct sspt_page *page, struct sspt_file *file)
{
	int err = 0;
	struct sspt_ip *ip, *n;
	LIST_HEAD(not_inst_head);

	mutex_lock(&page->ip_list.mtx);
	if (list_empty(&page->ip_list.not_inst))
		goto unlock;

	list_for_each_entry_safe(ip, n, &page->ip_list.not_inst, list) {
		/* set virtual address */
		ip->orig_addr = file->vm_start + page->offset + ip->offset;

		err = sspt_register_usprobe(ip);
		if (err) {
			sspt_ip_get(ip);
			list_move(&ip->list, &not_inst_head);
			continue;
		}
	}

	list_splice_init(&page->ip_list.not_inst, &page->ip_list.inst);

unlock:
	mutex_unlock(&page->ip_list.mtx);

	list_for_each_entry_safe(ip, n, &not_inst_head, list) {
		sspt_ip_clean(ip);
		sspt_ip_put(ip);
	}

	return 0;
}

/**
 * @brief Uninstall probes on the page
 *
 * @param page Pointer to the sspt_page struct
 * @param flag Action for probes
 * @param task Pointer to the task_struct struct
 * @return Error code
 */
int sspt_unregister_page(struct sspt_page *page,
			 enum US_FLAGS flag,
			 struct task_struct *task)
{
	int err = 0;
	struct sspt_ip *ip;

	mutex_lock(&page->ip_list.mtx);
	if (list_empty(&page->ip_list.inst))
		goto unlock;

	list_for_each_entry(ip, &page->ip_list.inst, list) {
		err = sspt_unregister_usprobe(task, ip, flag);
		if (err != 0) {
			WARN_ON(1);
			break;
		}
	}

	if (flag != US_DISARM)
		list_splice_init(&page->ip_list.inst, &page->ip_list.not_inst);

unlock:
	mutex_unlock(&page->ip_list.mtx);
	return err;
}

void sspt_page_on_each_ip(struct sspt_page *page,
			  void (*func)(struct sspt_ip *, void *), void *data)
{
	struct sspt_ip *ip;

	mutex_lock(&page->ip_list.mtx);
	list_for_each_entry(ip, &page->ip_list.inst, list)
		func(ip, data);
	mutex_unlock(&page->ip_list.mtx);
}
