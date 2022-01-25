/**
 * @file driver/driver_to_msg.h
 * @author Vyacheslav Cherkashin
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
 * Driver and parser interaction interface declaration.
 */

#ifndef __SWAP_DRIVER_DRIVER_TO_MSG__
#define __SWAP_DRIVER_DRIVER_TO_MSG__

#include <linux/compiler.h>

struct module;

struct driver_msg_handler {
	struct module *mod;
	int (*handler)(void __user *data);
};

/* Set the message handler */
void driver_msg_handler_set(struct driver_msg_handler *msg_handler);

#endif /* __SWAP_DRIVER_DRIVER_TO_MSG__ */
