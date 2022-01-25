/**
 * @file buffer/data_types.h
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
 * Declares data types for SWAP buffer.
 */

#ifndef __DATA_TYPES_H__
#define __DATA_TYPES_H__


#include <linux/spinlock.h>


struct page;

/**
 * @struct sync_t
 * @brief Using spinlocks as sync primitives.
 * @var sync_t::spinlock
 * Spinlock.
 * @var sync_t::flags
 * Flags for spinlock.
 */
struct sync_t {
	spinlock_t spinlock;
	unsigned long flags;
};

/**
 * @brief swap_subbuffer_ptr points to the first memory page of the subbuffer.
 */
typedef struct page *swap_subbuffer_ptr;

#endif /* __DATA_TYPES_H__ */
