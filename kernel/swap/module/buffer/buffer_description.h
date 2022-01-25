/**
 * @file buffer/buffer_description.h
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
 * swap_subbuffer structure represents one buffers subbufer
 */

#ifndef __BUFFER_DESCRIPTION_H__
#define __BUFFER_DESCRIPTION_H__

#include "data_types.h"

/**
 * @struct swap_subbuffer
 * @brief This structures are combined in array which represents the SWAP buffer.
 * @var swap_subbuffer::next_in_queue
 * Pointer to the next swap_subbufer in queue
 * @var swap_subbuffer::full_buffer_part
 * Currently occupied subbuffers size
 * @var swap_subbuffer::data_buffer
 * Pointer to subbuffers data itself of type swap_subbuffer_ptr
 * @var swap_subbuffer::buffer_sync
 * Subbuffers sync primitive
 */
struct swap_subbuffer {
	/* Pointer to the next subbuffer in queue */
	struct swap_subbuffer *next_in_queue;
	/* Size of the filled part of a subbuffer */
	size_t full_buffer_part;
	/* Pointer to data buffer */
	swap_subbuffer_ptr data_buffer;
	/* Buffer rw sync */
	struct sync_t buffer_sync;
};

#endif /* __BUFFER_DESCRIPTION_H__ */
