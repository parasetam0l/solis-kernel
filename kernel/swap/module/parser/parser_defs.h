/**
 * @file modules/parser/parser_defs.h
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vitaliy Cherepanov:
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
 * Parser defenitions.
 */


#ifndef __SWAP_DRIVER_DEVICE_DEFS_H__
#define __SWAP_DRIVER_DEVICE_DEFS_H__

#include <linux/kernel.h>

/* #define PARSE_DEBUG */

/** Prints debug message. */
#define print_debug(msg, args...) \
	printk(KERN_DEBUG "SWAP_PARSER DEBUG : " msg, ##args)
/** Prints info message. */
#define print_msg(msg, args...)   \
	printk(KERN_INFO "SWAP_PARSER : " msg, ##args)
/** Prints warning message. */
#define print_warn(msg, args...)  \
	printk(KERN_WARNING "SWAP_PARSER WARNING : " msg, ##args)
/** Prints error message. */
#define print_err(msg, args...)   \
	printk(KERN_ERR "SWAP_PARSER ERROR : " msg, ##args)
/** Prints critical error message. */
#define print_crit(msg, args...)  \
	printk(KERN_CRIT "SWAP_PARSER CRITICAL : " msg, ##args)

/* debug parse */
#ifdef PARSE_DEBUG
#define print_parse_debug(msg, args...) \
	printk(KERN_DEBUG "SWAP_PARSER DEBUG : " msg, ##args)
#else
#define print_parse_debug(msg, args...) \
	do {} while (0)
#endif /* PARSE_DEBUG */

#endif /* __SWAP_DRIVER_DEVICE_DEFS_H__ */
