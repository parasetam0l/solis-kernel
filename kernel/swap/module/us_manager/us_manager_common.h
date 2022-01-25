/*
 *  SWAP uprobe manager
 *  modules/us_manager/us_slot_manager.c
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013	 Alexander Aksenov: SWAP us_manager implement
 *
 */


#include <linux/mm.h>
#include <linux/version.h>

/*
 * TODO: move declaration and definition swap_do_mmap_pgoff()
 *       from swap_kprobe.ko to swap_us_manager.ko
 */
#include <kprobe/swap_kprobes_deps.h>


static inline unsigned long __swap_do_mmap(struct file *filp,
					   unsigned long addr,
					   unsigned long len,
					   unsigned long prot,
					   unsigned long flag,
					   unsigned long offset)
{
	unsigned long populate;

	return swap_do_mmap_pgoff(filp, addr, len, prot,
				  flag, offset, &populate);
}
