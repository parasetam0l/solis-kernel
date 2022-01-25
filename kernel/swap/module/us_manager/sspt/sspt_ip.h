#ifndef _SSPT_IP
#define _SSPT_IP

/**
 * @file us_manager/sspt/ip.h
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

#include <linux/list.h>
#include <linux/atomic.h>
#include <uprobe/swap_uprobes.h>
#include <us_manager/probes/probes.h>

struct sspt_page;

/**
 * @struct sspt_ip
 * @breaf Image of instrumentation pointer for specified process
 */
struct sspt_ip {
	/* sspt_page */
	struct list_head list;      /**< For sspt_page */
	struct sspt_page *page;     /**< Pointer on the page (parent) */

	/* img_ip */
	struct img_ip *img_ip;      /**< Pointer on the img_ip (parent) */
	struct list_head img_list;  /**< For img_ip */

	atomic_t usage;

	unsigned long orig_addr;    /**< Function address */
	unsigned long offset;       /**< Page offset */

	struct probe_desc *desc;    /**< Probe's data */

	union {
		struct uretprobe retprobe;
		struct uprobe uprobe;
	};
};


struct sspt_ip *sspt_ip_create(struct img_ip *img_ip, struct sspt_page *page);
void sspt_ip_clean(struct sspt_ip *ip);
void sspt_ip_get(struct sspt_ip *ip);
void sspt_ip_put(struct sspt_ip *ip);


#endif /* _SSPT_IP */
