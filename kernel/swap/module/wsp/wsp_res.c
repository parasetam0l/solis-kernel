/*
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Web startup profiling
 */

#include <linux/slab.h>
#include <linux/atomic.h>
#include <kprobe/swap_kprobes_deps.h>
#include "wsp_res.h"

static atomic_t __resource_id = ATOMIC_INIT(0);

static inline int __wsp_resource_id(void)
{
	return atomic_inc_return(&__resource_id);
}

struct wsp_resource_data {
	struct list_head list;
	int id;
	unsigned long addr;
	char *path;
};

static LIST_HEAD(__resources_list);
static DEFINE_MUTEX(__resources_mutex);

static struct wsp_resource_data *wsp_resource_data_alloc(void)
{
	struct wsp_resource_data *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	INIT_LIST_HEAD(&p->list);

	return p;
}

static void wsp_resource_data_free(struct wsp_resource_data *p)
{
	if (!p)
		return;

	kfree(p->path);
	kfree(p);
}

static struct wsp_resource_data *wsp_resource_data_find(unsigned long addr)
{
	struct wsp_resource_data *p;

	list_for_each_entry(p, &__resources_list, list)
		if (p->addr == addr)
			return p;

	return NULL;
}

int wsp_resource_data_id(unsigned long addr)
{
	int ret = -1;
	struct wsp_resource_data *p;

	mutex_lock(&__resources_mutex);
	p = wsp_resource_data_find(addr);
	if (p)
		ret = p->id;
	mutex_unlock(&__resources_mutex);

	return ret;
}

int wsp_resource_data_add(unsigned long addr, char *path)
{
	int ret = -1;
	struct wsp_resource_data *p;

	mutex_lock(&__resources_mutex);
	p = wsp_resource_data_find(addr);
	if (p) {
		ret = p->id;
		goto out;
	}
	p = wsp_resource_data_alloc();
	if (p) {
		p->id = __wsp_resource_id();
		p->addr = addr;
		p->path = path;
		list_add_tail(&p->list, &__resources_list);
		ret = p->id;
	}

out:
	mutex_unlock(&__resources_mutex);

	return ret;
}

void wsp_resource_data_del(unsigned long addr)
{
	struct wsp_resource_data *p;

	mutex_lock(&__resources_mutex);
	p = wsp_resource_data_find(addr);
	if (p) {
		list_del(&p->list);
		wsp_resource_data_free(p);
	}

	mutex_unlock(&__resources_mutex);
}

/* ============================================================================
 * =                                init/exit()                               =
 * ============================================================================
 */
int wsp_res_init(void)
{
	return 0;
}

void wsp_res_exit(void)
{
	struct wsp_resource_data *p, *tmp;

	mutex_lock(&__resources_mutex);
	list_for_each_entry_safe(p, tmp, &__resources_list, list) {
		list_del(&p->list);
		wsp_resource_data_free(p);
	}
	mutex_unlock(&__resources_mutex);
}
