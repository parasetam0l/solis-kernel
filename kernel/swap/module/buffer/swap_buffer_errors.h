/**
 * @file buffer/swap_buffer_errors.h
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
 * SWAP Buffer error codes enumeration.
 */

#ifndef __SWAP_BUFFER_ERRORS_H__
#define __SWAP_BUFFER_ERRORS_H__

/**
 * @enum _swap_buffer_errors
 * @brief SWAP buffer errors enumeration.
 */
enum _swap_buffer_errors {
	/**
	 * @brief Success.
	 */
	E_SB_SUCCESS = 0,
	/**
	 * @brief There are some unreleased buffers.
	 * Mainly returned by swap_buffer_uninit.
	 */
	E_SB_UNRELEASED_BUFFERS = 1,
	/**
	 * @brief No buffers for writing.
	 */
	E_SB_NO_WRITABLE_BUFFERS = 2,
	/**
	 * @brief Wrong data size: size == 0 or size > subbuffer size.
	 */
	E_SB_WRONG_DATA_SIZE = 3,
	/**
	 * @brief Trying to write data after SWAP buffer has been stopped.
	 */
	E_SB_IS_STOPPED = 4,
	/**
	 * @brief Memory areas of data to be written and subbuffer itself
	 * are overlap.
	 */
	E_SB_OVERLAP = 5,
	/**
	 * @brief No buffers for reading.
	 */
	E_SB_NO_READABLE_BUFFERS = 6,
	/**
	 * @brief Callback function ptr == NULL.
	 */
	E_SB_NO_CALLBACK = 7,
	/**
	 * @brief Memory for queue_busy wasn't allocated.
	 */
	E_SB_NO_MEM_QUEUE_BUSY = 8,
	/**
	 * @brief Memory for one of struct swap_buffer wasn't allocated.
	 */
	E_SB_NO_MEM_BUFFER_STRUCT = 9,
	/**
	 * @brief Memort for data buffer itself wasn't allocated.
	 */
	E_SB_NO_MEM_DATA_BUFFER = 10,
	/**
	 * @brief No such subbuffer in busy_list.
	 */
	E_SB_NO_SUBBUFFER_IN_BUSY = 11,
	/**
	 * @brief Subbuffers aren't allocated.
	 */
	E_SB_NOT_ALLOC = 12,
	/**
	 * @brief Thresholds > 100, top < lower.
	 */
	E_SB_WRONG_THRESHOLD = 13
};

#endif /* __SWAP_BUFFER_ERRORS_H__ */
