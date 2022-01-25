/*
 *  SWAP uprobe manager
 *  modules/us_manager/img/img_ip.c
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
 * 2013	 Vyacheslav Cherkashin: SWAP us_manager implement
 *
 */


#include "img_ip.h"
#include "img_file.h"
#include <us_manager/probes/use_probes.h>
#include <us_manager/sspt/sspt.h>
#include <us_manager/sspt/sspt_ip.h>
#include <linux/slab.h>
#include <linux/atomic.h>


static atomic_t ip_counter = ATOMIC_INIT(0);

/**
 * @brief Create img_ip struct
 *
 * @param addr Function address
 * @param probe_i Pointer to the probe info data.
 * @return Pointer to the created img_ip struct
 */
struct img_ip *img_ip_create(unsigned long addr, struct probe_desc *pd,
			     struct img_file *file)
{
	struct img_ip *ip;

	ip = kmalloc(sizeof(*ip), GFP_KERNEL);
	if (!ip)
		return ERR_PTR(-ENOMEM);
	atomic_inc(&ip_counter);

	INIT_LIST_HEAD(&ip->list);
	kref_init(&ip->ref);
	mutex_init(&ip->sspt.mtx);
	INIT_LIST_HEAD(&ip->sspt.head);
	ip->addr = addr;
	ip->desc = pd;
	ip->file = file;

	return ip;
}

static void img_ip_release(struct kref *ref)
{
	struct img_ip *ip = container_of(ref, struct img_ip, ref);

	WARN_ON(!list_empty(&ip->sspt.head));

	atomic_dec(&ip_counter);
	kfree(ip);
}

void img_ip_clean(struct img_ip *ip)
{
	struct sspt_ip *p;

	img_ip_lock(ip);
	while(!list_empty(&ip->sspt.head)) {
		p = list_first_entry(&ip->sspt.head, struct sspt_ip ,img_list);
		sspt_ip_get(p);
		img_ip_unlock(ip);

		if (sspt_page_is_installed_ip(p->page, p))
			sspt_unregister_usprobe(NULL, p, US_UNREGS_PROBE);

		sspt_ip_clean(p);

		img_ip_lock(ip);
		sspt_ip_put(p);
	}
	img_ip_unlock(ip);
}

void img_ip_get(struct img_ip *ip)
{
	kref_get(&ip->ref);
}

void img_ip_put(struct img_ip *ip)
{
	kref_put(&ip->ref, img_ip_release);
}

void img_ip_add_ip(struct img_ip *ip, struct sspt_ip *sspt_ip)
{
	sspt_ip->img_ip = ip;
	list_add(&sspt_ip->img_list, &ip->sspt.head);
}

void img_ip_lock(struct img_ip *ip)
{
	mutex_lock(&ip->sspt.mtx);
}

void img_ip_unlock(struct img_ip *ip)
{
	mutex_unlock(&ip->sspt.mtx);
}

bool img_ip_is_unloadable(void)
{
	return !atomic_read(&ip_counter);
}

/**
 * @brief For debug
 *
 * @param ip Pointer to the img_ip struct
 * @return Void
 */

/* debug */
void img_ip_print(struct img_ip *ip)
{
	if (ip->desc->type == SWAP_RETPROBE)
		printk(KERN_INFO "###            addr=8%lx, args=%s\n",
		       ip->addr, ip->desc->info.rp_i.args);
}
/* debug */
