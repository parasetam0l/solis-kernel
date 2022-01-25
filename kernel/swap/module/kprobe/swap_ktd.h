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


#ifndef _SWAP_TD_H
#define _SWAP_TD_H


#include <linux/list.h>
#include "swap_td_raw.h"

struct task_struct;

struct ktask_data {
	int nr_bit;
	struct td_raw td_raw;

	/* init() and exit() may be called in atomic context */
	void (*init)(struct task_struct *, void *);
	void (*exit)(struct task_struct *, void *);
	unsigned long size;
};


int swap_ktd_reg(struct ktask_data *ktd);
void swap_ktd_unreg(struct ktask_data *ktd);

void *swap_ktd(struct ktask_data *ktd, struct task_struct *task);

int swap_ktd_init(void);
void swap_ktd_uninit_top(void);
void swap_ktd_uninit_bottom(void);
void swap_ktd_put_task(struct task_struct *task);


#endif /* _SWAP_TD_H */
