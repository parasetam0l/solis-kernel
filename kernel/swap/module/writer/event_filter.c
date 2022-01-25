/**
 * writer/event_filter.c
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Events filter.
 */


#include <linux/module.h>
#include <linux/list.h>
#include "event_filter.h"


static LIST_HEAD(filter_list);

static int func_none(struct task_struct *task)
{
	return 1;
}

static struct ev_filter filter_none = {
	.name = "all",
	.filter = func_none
};

static struct ev_filter *filter_current = &filter_none;

int check_event(struct task_struct *task)
{
	return filter_current->filter(task);
}
EXPORT_SYMBOL_GPL(check_event);

static struct ev_filter *event_filter_find(const char *name)
{
	struct ev_filter *f, *tmp;

	list_for_each_entry_safe(f, tmp, &filter_list, list) {
		if (strcmp(f->name, name) == 0)
			return f;
	}

	return NULL;
}

/**
 * @brief Registers event filter.
 *
 * @param f Pointer to the event filter.
 * @return 0 on success, error code on error.
 */
int event_filter_register(struct ev_filter *f)
{
	if (event_filter_find(f->name))
		return -EINVAL;

	INIT_LIST_HEAD(&f->list);
	list_add(&f->list, &filter_list);

	return 0;
}
EXPORT_SYMBOL_GPL(event_filter_register);

/**
 * @brief Unregisters event filter.
 *
 * @param f Pointer to the event filter.
 * @return Void.
 */
void event_filter_unregister(struct ev_filter *f)
{
	struct ev_filter *filter, *tmp;

	if (filter_current == f)
		filter_current = &filter_none;

	list_for_each_entry_safe(filter, tmp, &filter_list, list) {
		if (filter == f) {
			list_del(&filter->list);
			break;
		}
	}
}
EXPORT_SYMBOL_GPL(event_filter_unregister);

/**
 * @brief Sets event filter by its name.
 *
 * @param name Filter name.
 * @return 0 on success, error code on error.
 */
int event_filter_set(const char *name)
{
	struct ev_filter *f;

	f = event_filter_find(name);
	if (f == NULL)
		return -EINVAL;

	filter_current = f;

	return 0;
}
EXPORT_SYMBOL_GPL(event_filter_set);

/**
 * @brief Gets filter name.
 *
 * @return Pointer to the filter name string.
 */
const char *event_filter_get(void)
{
	return filter_current->name;
}

/**
 * @brief Runs specified callback for each filter in list.
 *
 * @param func Specified callback.
 * @param data Pointer to the data passed to the callback.
 * @return Void.
 */
void event_filter_on_each(void (*func)(struct ev_filter *, void *),
			  void *data)
{
	struct ev_filter *f, *tmp;

	list_for_each_entry_safe(f, tmp, &filter_list, list)
		func(f, data);
}

/**
 * @brief Initializes event filter.
 *
 * @return Initialization result.
 */
int event_filter_init(void)
{
	return event_filter_register(&filter_none);
}

/**
 * @brief Uninitializes event filter.
 *
 * @return Void.
 */
void event_filter_exit(void)
{
	event_filter_unregister(&filter_none);
}
