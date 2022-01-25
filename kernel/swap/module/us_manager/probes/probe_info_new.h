#ifndef _PROBE_INFO_NEW_H
#define _PROBE_INFO_NEW_H

/*
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <uprobe/swap_uprobes.h>
#include "probes.h"


struct dentry;
struct pf_group;


struct probe_info_new {
	enum probe_t type;
	union {
		struct {
			uprobe_pre_handler_t handler;
		} p;

		struct {
			uretprobe_handler_t entry_handler;
			uretprobe_handler_t ret_handler;
			/*
			 * FIXME: make dynamic size,
			 *        currently data_size = sizeof(void *)
			 */
			size_t data_size;
		} rp;
	} u;

	/* private */
	struct probe_info info;
};

struct probe_new {
	/* reg data */
	unsigned long offset;
	struct probe_desc *desc;

	/* unreg data */
	void *priv;
};


#define MAKE_UPROBE(_handler)				\
	{						\
		.type = SWAP_NEW_UP,			\
		.u.p.handler = _handler			\
	}

#define MAKE_URPROBE(_entry, _ret, _size)		\
	{						\
		.type = SWAP_NEW_URP,			\
		.u.rp.entry_handler = _entry,		\
		.u.rp.ret_handler = _ret,		\
		.u.rp.data_size = _size			\
	}

struct probe_info_otg {
	struct probe_info info;
	struct probe_info_new *data;	/* field 'data[0]' in probe_info struct */
};

int pin_register(struct probe_new *probe, struct pf_group *pfg,
		 struct dentry *dentry);
void pin_unregister(struct probe_new *probe, struct pf_group *pfg);


int pin_init(void);
void pin_exit(void);


#endif /* _PROBE_INFO_NEW_H */
