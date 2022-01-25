/*
 * @file fbiprobe/fbi_probe.h
 *
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014 Alexander Aksenov : FBI implement
 * 2014 Vitaliy Cherepanov: FBI implement, portage
 *
 * @section DESCRIPTION
 *
 * Function body instrumentation.
 *
 */

#ifndef __FBI_PROBE_MODULE_H__
#define __FBI_PROBE_MODULE_H__

#include <linux/kernel.h>

/* MESSAGES */

#define MODULE_NAME "SWAP_FBI_PROBE"

/* FBI_DEBUG_ON:
 * val | DEBUG | MSG | WARN | ERR | CRITICAL|
 * ----+-------+-----+------+-----+---------|
 * 0   | OFF   | OFF | OFF  | OFF | OFF     |
 * 1   | OFF   | OFF | OFF  | OFF | ON      |
 * 2   | OFF   | OFF | OFF  | ON  | ON      |
 * 3   | OFF   | OFF | ON   | ON  | ON      |
 * 4   | OFF   | ON  | ON   | ON  | ON      |
 * 5   | ON    | ON  | ON   | ON  | ON      |
 */

#define FBI_DEBUG_LEVEL 3

/** Prints debug message.*/
#if (FBI_DEBUG_LEVEL >= 5)
#define print_debug(msg, args...) \
	printk(KERN_DEBUG MODULE_NAME " DEBUG : " msg, ##args)
#else
#define print_debug(msg, args...)
#endif

/** Prints info message.*/
#if (FBI_DEBUG_LEVEL >= 4)
#define print_msg(msg, args...)   \
	printk(KERN_INFO MODULE_NAME " : " msg, ##args)
#else
#define print_msg(msg, args...)
#endif

/** Prints warning message.*/
#if (FBI_DEBUG_LEVEL >= 3)
#define print_warn(msg, args...)  \
	printk(KERN_WARNING MODULE_NAME " WARNING : " msg, ##args)
#else
#define print_warn(msg, args...)
#endif

/** Prints error message.*/
#if (FBI_DEBUG_LEVEL >= 2)
#define print_err(msg, args...)   \
	printk(KERN_ERR MODULE_NAME " ERROR : " msg, ##args)
#else
#define print_err(msg, args...)
#endif

/** Prints critical error message.*/
#if (FBI_DEBUG_LEVEL >= 1)
#define print_crit(msg, args...)  \
	printk(KERN_CRIT MODULE_NAME " CRITICAL : " msg, ##args)
#else
#define print_crit(msg, args...)
#endif

#endif /* __FBI_PROBE_MODULE_H__ */
