/**
 * @file ksyms/ksyms_module.c
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * @section DESCRIPTION
 *
 * SWAP symbols searching module initialization implementation.
 */

#include "ksyms_init.h"

#include <linux/module.h>

/**
 * @brief Init ksyms module.
 *
 * @return 0 on success.
 */
int __init swap_ksyms_init(void)
{
	int ret = ksyms_init();

	printk(KERN_INFO "SWAP_KSYMS: Module initialized\n");

	return ret;
}

/**
 * @brief Exit ksyms module.
 *
 * @return Void.
 */
void __exit swap_ksyms_exit(void)
{
	ksyms_exit();

	printk(KERN_INFO "SWAP_KSYMS: Module uninitialized\n");
}

module_init(swap_ksyms_init);
module_exit(swap_ksyms_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP ksyms module");
MODULE_AUTHOR("Vyacheslav Cherkashin <v.cherkashin@samaung.com>");
