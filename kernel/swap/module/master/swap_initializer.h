/**
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * SWAP event notification interface.
 */

#ifndef _SWAP_INITIALIZER_H
#define _SWAP_INITIALIZER_H


#include <linux/list.h>
#include <linux/types.h>
#include <linux/module.h>


struct file;
struct inode;


typedef int (*swap_init_t)(void);
typedef void (*swap_uninit_t)(void);


struct swap_init_struct {
	swap_init_t once;	/* to call only on the first initialization */

	swap_init_t core_init;
	swap_uninit_t core_uninit;

	swap_init_t fs_init;
	swap_uninit_t fs_uninit;

	/* private fields */
	struct list_head list;
	unsigned once_flag:1;
	unsigned core_flag:1;
	unsigned fs_flag:1;
};


int swap_init_simple_open(struct inode *inode, struct file *file);
int swap_init_simple_release(struct inode *inode, struct file *file);

int swap_init_init(void);
int swap_init_uninit(void);

int swap_init_stat_get(void);
void swap_init_stat_put(void);

int swap_init_register(struct swap_init_struct *init);
void swap_init_unregister(struct swap_init_struct *init);


#define SWAP_LIGHT_INIT_MODULE(_once, _init, _uninit, _fs_init, _fs_uninit) \
	static struct swap_init_struct __init_struct = {		\
		.once = _once,						\
		.core_init = _init,					\
		.core_uninit = _uninit,					\
		.fs_init = _fs_init,					\
		.fs_uninit = _fs_uninit,				\
		.list = LIST_HEAD_INIT(__init_struct.list),		\
		.once_flag = false,					\
		.core_flag = false,					\
		.fs_flag = false					\
	};								\
	static int __init __init_mod(void)				\
	{								\
		return swap_init_register(&__init_struct);		\
	}								\
	static void __exit __exit_mod(void)				\
	{								\
		swap_init_unregister(&__init_struct);			\
	}								\
	module_init(__init_mod);					\
	module_exit(__exit_mod)


#endif /* _SWAP_INITIALIZER_H */
