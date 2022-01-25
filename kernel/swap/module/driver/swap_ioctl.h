/**
 * @file driver/swap_ioctl.h
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
 * Provides ioctl commands and recources for SWAP driver.
 */

#ifndef __SWAP_IOCTL_H__
#define __SWAP_IOCTL_H__

#include <linux/ioctl.h>

/** SWAP device magic number. */
#define SWAP_DRIVER_IOC_MAGIC 0xAF

/**
 * @struct buffer_initialize
 * @brief SWAP buffer initialization struct.
 * @var buffer_initialize::size
 * Size of one subbuffer.
 * @var buffer_initialize::count
 * Count of subbuffers in the buffer.
 */
struct buffer_initialize {
	u32 size;
	u32 count;
} __packed;

/* SWAP Device ioctl commands */

/** Initialize buffer message. */
#define SWAP_DRIVER_BUFFER_INITIALIZE		_IOW(SWAP_DRIVER_IOC_MAGIC, 1, \
						     struct buffer_initialize *)
/** Uninitialize buffer message. */
#define SWAP_DRIVER_BUFFER_UNINITIALIZE		_IO(SWAP_DRIVER_IOC_MAGIC, 2)
/** Set next buffer to read. */
#define SWAP_DRIVER_NEXT_BUFFER_TO_READ		_IO(SWAP_DRIVER_IOC_MAGIC, 3)
/** Flush buffers. */
#define SWAP_DRIVER_FLUSH_BUFFER		_IO(SWAP_DRIVER_IOC_MAGIC, 4)
/** Custom message. */
#define SWAP_DRIVER_MSG				_IOW(SWAP_DRIVER_IOC_MAGIC, 5, \
						     void *)
/** Force wake up daemon. */
#define SWAP_DRIVER_WAKE_UP			_IO(SWAP_DRIVER_IOC_MAGIC, 6)

#endif /* __SWAP_IOCTL_H__ */
