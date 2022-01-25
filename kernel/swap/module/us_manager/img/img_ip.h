/**
 * @file us_manager/img/img_ip.h
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
 *
 */


#ifndef _IMG_IP_H
#define _IMG_IP_H

#include <linux/types.h>
#include <linux/kref.h>
#include <linux/mutex.h>


struct sspt_ip;
struct img_file;
struct probe_desc;

/**
 * @struct img_ip
 * @breaf Image of instrumentation pointer
 */
struct img_ip {
	/* img_file */
	struct list_head list;		/**< List for img_file */
	struct img_file *file;		/**< Pointer on the file (parent) */

	struct kref ref;

	/* sspt_ip */
	struct {
		struct mutex mtx;
		struct list_head head;
	} sspt;

	unsigned long addr;		/**< Function address */
	struct probe_desc *desc;	/**< Probe info */
};

struct img_ip *img_ip_create(unsigned long addr, struct probe_desc *info,
			     struct img_file *file);
void img_ip_clean(struct img_ip *ip);
void img_ip_get(struct img_ip *ip);
void img_ip_put(struct img_ip *ip);

void img_ip_add_ip(struct img_ip *ip, struct sspt_ip *sspt_ip);
void img_ip_lock(struct img_ip *ip);
void img_ip_unlock(struct img_ip *ip);

bool img_ip_is_unloadable(void);

/* debug */
void img_ip_print(struct img_ip *ip);
/* debug */

#endif /* _IMG_IP_H */
