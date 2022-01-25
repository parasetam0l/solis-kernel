/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/ip.c
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

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include "sspt_ip.h"
#include "sspt_page.h"
#include "sspt_file.h"
#include <us_manager/probes/use_probes.h>
#include <us_manager/img/img_ip.h>

/**
 * @brief Create us_ip struct
 *
 * @param page User page
 * @param offset Function offset from the beginning of the page
 * @param probe_i Pointer to the probe data.
 * @param page Pointer to the parent sspt_page struct
 * @return Pointer to the created us_ip struct
 */
struct sspt_ip *sspt_ip_create(struct img_ip *img_ip, struct sspt_page *page)
{
	struct sspt_ip *ip;

	ip = kmalloc(sizeof(*ip), GFP_ATOMIC);
	if (!ip)
		return NULL;

	memset(ip, 0, sizeof(*ip));
	INIT_LIST_HEAD(&ip->list);
	INIT_LIST_HEAD(&ip->img_list);
	ip->offset = img_ip->addr & ~PAGE_MASK;
	ip->desc = img_ip->desc;
	atomic_set(&ip->usage, 2);	/* for 'img_ip' and 'page' */

	/* add to img_ip list */
	img_ip_get(img_ip);
	img_ip_lock(img_ip);
	img_ip_add_ip(img_ip, ip);
	img_ip_unlock(img_ip);

	/* add to page list */
	sspt_page_get(page);
	sspt_page_lock(page);
	sspt_page_add_ip(page, ip);
	sspt_page_unlock(page);

	return ip;
}

static void sspt_ip_free(struct sspt_ip *ip)
{
	WARN_ON(!list_empty(&ip->list) || !list_empty(&ip->img_list));

	kfree(ip);
}

void sspt_ip_get(struct sspt_ip *ip)
{
	atomic_inc(&ip->usage);
}

void sspt_ip_put(struct sspt_ip *ip)
{
	if (atomic_dec_and_test(&ip->usage))
		sspt_ip_free(ip);
}

void sspt_ip_clean(struct sspt_ip *ip)
{
	bool put_page = false;
	bool put_ip = false;

	/* remove from page */
	sspt_page_lock(ip->page);
	if (!list_empty(&ip->list)) {
		list_del_init(&ip->list);
		put_page = true;
	}
	sspt_page_unlock(ip->page);
	if (put_page) {
		sspt_page_put(ip->page);
		sspt_ip_put(ip);
	}

	/* remove from img_ip */
	img_ip_lock(ip->img_ip);
	if (!list_empty(&ip->img_list)) {
		list_del_init(&ip->img_list);
		put_ip = true;
	}
	img_ip_unlock(ip->img_ip);
	if (put_ip) {
		img_ip_put(ip->img_ip);
		sspt_ip_put(ip);
	}
}
