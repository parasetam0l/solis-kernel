/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/us_manager/sspt/sspt_feature.c
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

#include "sspt_feature.h"
#include "sspt_proc.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>


struct sspt_feature {
	struct list_head feature_list;
};

struct sspt_feature_img {
	struct list_head list;

	void *(*alloc)(void);
	void (*free)(void *data);
};

struct sspt_feature_data {
	struct list_head list;

	struct sspt_feature_img *img;
	void *data;
};

static DEFINE_SPINLOCK(feature_img_lock);
static LIST_HEAD(feature_img_list);

static struct sspt_feature_data *create_feature_data(
	struct sspt_feature_img *img)
{
	struct sspt_feature_data *fd;

	fd = kmalloc(sizeof(*fd), GFP_ATOMIC);
	if (fd) {
		INIT_LIST_HEAD(&fd->list);
		fd->img = img;
		fd->data = img->alloc();
	}

	return fd;
}

static void destroy_feature_data(struct sspt_feature_data *fd)
{
	fd->img->free(fd->data);
	kfree(fd);
}

/**
 * @brief Create sspt_feature struct
 *
 * @return Pointer to the created sspt_feature struct
 */
struct sspt_feature *sspt_create_feature(void)
{
	struct sspt_feature *f;

	f = kmalloc(sizeof(*f), GFP_ATOMIC);
	if (f) {
		struct sspt_feature_data *fd;
		struct sspt_feature_img *fi;
		unsigned long flags;

		INIT_LIST_HEAD(&f->feature_list);

		spin_lock_irqsave(&feature_img_lock, flags);
		list_for_each_entry(fi, &feature_img_list, list) {
			fd = create_feature_data(fi);
                        if (fd) /* add to list */
				list_add(&fd->list, &f->feature_list);
		}
		spin_unlock_irqrestore(&feature_img_lock, flags);
	}

	return f;
}

/**
 * @brief Destroy sspt_feature struct
 *
 * @param f remove object
 * @return Void
 */
void sspt_destroy_feature(struct sspt_feature *f)
{
	struct sspt_feature_data *fd, *n;

	list_for_each_entry_safe(fd, n, &f->feature_list, list) {
		/* delete from list */
		list_del(&fd->list);
		destroy_feature_data(fd);
	}

	kfree(f);
}

static void add_feature_img_to_list(struct sspt_feature_img *fi)
{
	unsigned long flags;

	spin_lock_irqsave(&feature_img_lock, flags);
	list_add(&fi->list, &feature_img_list);
	spin_unlock_irqrestore(&feature_img_lock, flags);
}

static void del_feature_img_from_list(struct sspt_feature_img *fi)
{
	unsigned long flags;

	spin_lock_irqsave(&feature_img_lock, flags);
	list_del(&fi->list);
	spin_unlock_irqrestore(&feature_img_lock, flags);
}

static struct sspt_feature_img *create_feature_img(void *(*alloc)(void),
						   void (*free)(void *data))
{
	struct sspt_feature_img *fi;

	fi = kmalloc(sizeof(*fi), GFP_ATOMIC);
	if (fi) {
		INIT_LIST_HEAD(&fi->list);
		fi->alloc = alloc;
		fi->free = free;

		add_feature_img_to_list(fi);
	}

	return fi;
}

static void destroy_feature_img(struct sspt_feature_img *fi)
{
	del_feature_img_from_list(fi);

	kfree(fi);
}

static void del_feature_by_img(struct sspt_feature *f,
			       struct sspt_feature_img *img)
{
	struct sspt_feature_data *fd;

	list_for_each_entry(fd, &f->feature_list, list) {
		if (img == fd->img) {
			/* delete from list */
			list_del(&fd->list);
			destroy_feature_data(fd);
			break;
		}
	}
}

static void del_feature_from_proc(struct sspt_proc *proc, void *data)
{
	del_feature_by_img(proc->feature, (struct sspt_feature_img *)data);
}

/**
 * @brief Get data for feature
 *
 * @param f Pointer to the sspt_feature struct
 * @param id Feature ID
 * @return Pointer to the data
 */
void *sspt_get_feature_data(struct sspt_feature *f, sspt_feature_id_t id)
{
	struct sspt_feature_img *img = (struct sspt_feature_img *)id;
	struct sspt_feature_data *fd;

	list_for_each_entry(fd, &f->feature_list, list) {
		if (img == fd->img)
			return fd->data;
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(sspt_get_feature_data);

/**
 * @brief Register sspt feature
 *
 * @param alloc Callback for allocating data
 * @param free Callback to release data
 * @return Feature ID
 */
sspt_feature_id_t sspt_register_feature(void *(*alloc)(void),
					void (*free)(void *data))
{
	struct sspt_feature_img *fi;

	fi = create_feature_img(alloc, free);

	/* TODO: add to already instrumentation process */

	return (sspt_feature_id_t)fi;
}
EXPORT_SYMBOL_GPL(sspt_register_feature);

/**
 * @brief Unregister sspt feature
 *
 * @param id Feature ID
 * @return Void
 */
void sspt_unregister_feature(sspt_feature_id_t id)
{
	struct sspt_feature_img *fi = (struct sspt_feature_img *)id;

	on_each_proc(del_feature_from_proc, (void *)fi);
	destroy_feature_img(fi);
}
EXPORT_SYMBOL_GPL(sspt_unregister_feature);
