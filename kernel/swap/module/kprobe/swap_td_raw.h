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
 * 2014         Vasiliy Ulyanov <v.ulyanov@samsung.com>
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#ifndef _SWAP_TD_RAW_H
#define _SWAP_TD_RAW_H


#include <linux/list.h>


struct td_raw {
	struct list_head list;
	unsigned long offset;
};


int swap_td_raw_reg(struct td_raw *raw, unsigned long size);
void swap_td_raw_unreg(struct td_raw *raw);

void *swap_td_raw(struct td_raw *raw, struct task_struct *task);

int swap_td_raw_init(void);
void swap_td_raw_uninit(void);


#endif /* _SWAP_TD_RAW_H */
