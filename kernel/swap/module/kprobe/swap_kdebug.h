/**
 * @file kprobe/swap_kdebug.h
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
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
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * Header for debug purposes.
 */


#ifndef _SWAP_KPROBE_DEBUG_H
#define _SWAP_KPROBE_DEBUG_H

/* #define _DEBUG */

#ifdef _DEBUG
#define DBPRINTF(format, args...) do { \
		if (1) { \
			char *f = __FILE__; \
			char *n = strrchr(f, '/'); \
			printk(KERN_INFO "%s : %u : %s : " format "\n" , \
			       (n) ? n+1 : f, __LINE__, __func__, ##args); \
		} \
	} while (0)
#else
#define DBPRINTF(format, args...)
#endif


#endif /* _SWAP_KPROBE_DEBUG_H */
