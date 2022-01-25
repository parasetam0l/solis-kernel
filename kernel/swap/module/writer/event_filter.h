/**
 * @file writer/event_filter.h
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
 * Event filter interface declaration.
 */


#ifndef _EVENT_FILTER_H
#define _EVENT_FILTER_H


#include <linux/list.h>

struct task_struct;

/**
 * @struct ev_filter
 * @bref Event filter structure.
 */
struct ev_filter {
	struct list_head list;                  /**< Filter list head. */
	char *name;                             /**< Filter name. */
	int (*filter)(struct task_struct *);    /**< Filter function. */
};


int check_event(struct task_struct *task);

int event_filter_register(struct ev_filter *f);
void event_filter_unregister(struct ev_filter *f);
int event_filter_set(const char *name);
const char *event_filter_get(void);

void event_filter_on_each(void (*func)(struct ev_filter *, void *),
			  void *data);

int event_filter_init(void);
void event_filter_exit(void);

#endif /* _EVENT_FILTER_H */
