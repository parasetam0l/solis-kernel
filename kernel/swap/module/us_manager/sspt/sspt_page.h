#ifndef __SSPT_PAGE__
#define __SSPT_PAGE__

/**
 * @file us_manager/sspt/sspt_page.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * Copyright (C) Samsung Electronics, 2013
 */

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/kref.h>

struct sspt_ip;
struct sspt_file;
struct task_struct;
enum US_FLAGS;

/**
 * @struct sspt_page
 * @breaf Image of page for specified process
 */
struct sspt_page {
	/* sspt_file */
	struct hlist_node hlist;		/**< For sspt_file */
	struct sspt_file *file;			/**< Ptr to the file (parent) */

	/* sspt_ip */
	struct {
		struct mutex mtx;		/**< Lock page */
		struct list_head inst;		/**< For installed ip */
		struct list_head not_inst;	/**< For don'tinstalled ip */
	} ip_list;

	unsigned long offset;			/**< File offset */

	struct kref ref;
};

struct sspt_page *sspt_page_create(unsigned long offset);
void sspt_page_clean(struct sspt_page *page);
void sspt_page_get(struct sspt_page *page);
void sspt_page_put(struct sspt_page *page);

bool sspt_page_is_installed_ip(struct sspt_page *page, struct sspt_ip *ip);
void sspt_page_add_ip(struct sspt_page *page, struct sspt_ip *ip);
void sspt_page_lock(struct sspt_page *page);
void sspt_page_unlock(struct sspt_page *page);

bool sspt_page_is_installed(struct sspt_page *page);

int sspt_register_page(struct sspt_page *page, struct sspt_file *file);

int sspt_unregister_page(struct sspt_page *page,
			 enum US_FLAGS flag,
			 struct task_struct *task);

void sspt_page_on_each_ip(struct sspt_page *page,
			  void (*func)(struct sspt_ip *, void *), void *data);

#endif /* __SSPT_PAGE__ */
