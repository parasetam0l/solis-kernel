/*
 *  SWAP uprobe manager
 *  modules/us_manager/img/img_file.c
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


#include "img_file.h"
#include "img_ip.h"
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/dcache.h>
#include <linux/atomic.h>


static atomic_t file_counter = ATOMIC_INIT(0);


static void img_del_ip_by_list(struct img_ip *ip);

/**
 * @brief Create img_file struct
 *
 * @param dentry Dentry of file
 * @return Pointer to the created img_file struct
 */
struct img_file *img_file_create(struct dentry *dentry)
{
	struct img_file *file;

	file = kmalloc(sizeof(*file), GFP_ATOMIC);
	if (file == NULL) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	atomic_inc(&file_counter);

	file->dentry = dentry;
	INIT_LIST_HEAD(&file->list);
	INIT_LIST_HEAD(&file->ips.head);
	mutex_init(&file->ips.mtx);
	atomic_set(&file->use, 1);

	return file;
}

/**
 * @brief Remove img_file struct
 *
 * @param file remove object
 * @return Void
 */
static void img_file_free(struct img_file *file)
{
	struct img_ip *ip, *tmp;

	list_for_each_entry_safe(ip, tmp, &file->ips.head, list) {
		img_del_ip_by_list(ip);
		img_ip_clean(ip);
		img_ip_put(ip);
	}

	atomic_dec(&file_counter);
	kfree(file);
}

/* called with mutex_[lock/unlock](&file->ips.mtx) */
static void img_add_ip_by_list(struct img_file *file, struct img_ip *ip)
{
	list_add(&ip->list, &file->ips.head);
}

/* called with mutex_[lock/unlock](&file->ips.mtx) */
static void img_del_ip_by_list(struct img_ip *ip)
{
	list_del(&ip->list);
}

void img_file_get(struct img_file *file)
{
	WARN_ON(!atomic_read(&file->use));
	atomic_inc(&file->use);
}

void img_file_put(struct img_file *file)
{
	if (atomic_dec_and_test(&file->use))
		img_file_free(file);
}


/**
 * @brief Add instrumentation pointer
 *
 * @param file Pointer to the img_file struct
 * @param addr Function address
 * @param probe_Pointer to a probe_info structure with an information about
 * the probe.
 * @return Error code
 */
struct img_ip *img_file_add_ip(struct img_file *file, unsigned long addr,
			       struct probe_desc *pd)
{
	struct img_ip *ip;

	ip = img_ip_create(addr, pd, file);
	if (IS_ERR(ip))
		return ip;

	mutex_lock(&file->ips.mtx);
	img_add_ip_by_list(file, ip);
	mutex_unlock(&file->ips.mtx);

	return ip;
}

/**
 * @brief Delete img_ip struct from img_file struct
 *
 * @param file Pointer to the img_file struct
 * @param addr Function address
 * @return Error code
 */
void img_file_del_ip(struct img_file *file, struct img_ip *ip)
{
	mutex_lock(&file->ips.mtx);
	img_del_ip_by_list(ip);
	mutex_unlock(&file->ips.mtx);

	img_ip_clean(ip);
	img_ip_put(ip);
}

/**
 * @brief Check on absence img_ip structs in img_file struct
 *
 * @param file Pointer to the img_file struct
 * @return
 *       - 0 - not empty
 *       - 1 - empty
 */
int img_file_empty(struct img_file *file)
{
	return list_empty(&file->ips.head);
}

bool img_file_is_unloadable(void)
{
	return !(atomic_read(&file_counter) + !img_ip_is_unloadable());
}

/**
 * @brief For debug
 *
 * @param file Pointer to the img_file struct
 * @return Void
 */

/* debug */
void img_file_print(struct img_file *file)
{
	struct img_ip *ip;

	printk(KERN_INFO "###      d_iname=%s\n", file->dentry->d_iname);

	mutex_lock(&file->ips.mtx);
	list_for_each_entry(ip, &file->ips.head, list) {
		img_ip_print(ip);
	}
	mutex_unlock(&file->ips.mtx);
}
/* debug */
