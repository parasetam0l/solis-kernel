/**
 * @file sampler/kernel_operations.h
 * @author Alexander Aksenov <a.aksenov@samsung.com>
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
 * Parser definitions.
 */

#ifndef __KERNEL_OPERATIONS_H__
#define __KERNEL_OPERATIONS_H__

#include <linux/kernel.h>

/** Prints debug message.*/
#define print_debug(msg, args...) \
	printk(KERN_DEBUG "SWAP_SAMPLER DEBUG : " msg, ##args)
/** Prints info message.*/
#define print_msg(msg, args...)   \
	printk(KERN_INFO "SWAP_SAMPLER : " msg, ##args)
/** Prints warning message.*/
#define print_warn(msg, args...)  \
	printk(KERN_WARNING "SWAP_SAMPLER WARNING : " msg, ##args)
/** Prints error message.*/
#define print_err(msg, args...)   \
	printk(KERN_ERR "SWAP_SAMPLER ERROR : " msg, ##args)
/** Prints critical error message.*/
#define print_crit(msg, args...)  \
	printk(KERN_CRIT "SWAP_SAMPLER CRITICAL : " msg, ##args)

#endif /* __KERNEL_OPERATIONS_H__ */
