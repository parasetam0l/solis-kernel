/**
 * @file driver/swap_driver_errors.h
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
 * SWAP driver error codes.
 */

#ifndef __SWAP_DRIVER_ERRORS_H__
#define __SWAP_DRIVER_ERRORS_H__


/**
 * @enum _swap_driver_errors
 * @brief SWAP driver errors enumeration.
 */
enum _swap_driver_errors {
	/**
	 * @brief Success.
	 */
	E_SD_SUCCESS = 0,
	/**
	 * @brief Alloc_chrdev_region failed.
	 */
	E_SD_ALLOC_CHRDEV_FAIL = 1,
	/**
	 * @brief cdev_alloc failed.
	 */
	E_SD_CDEV_ALLOC_FAIL = 2,
	/**
	 * @brief cdev_add failed.
	 */
	E_SD_CDEV_ADD_FAIL = 3,
	/**
	 * @brief class_create failed.
	 */
	E_SD_CLASS_CREATE_FAIL = 4,
	/**
	 * @brief device_create failed.
	 */
	E_SD_DEVICE_CREATE_FAIL = 5,
	/**
	 * @brief splice_* funcs not found.
	 */
	E_SD_NO_SPLICE_FUNCS = 6,
	/**
	 * @brief swap_buffer_get tells us that there is no readable subbuffers.
	 */
	E_SD_NO_DATA_TO_READ = 7,
	/**
	 * @brief No busy subbuffer.
	 */
	E_SD_NO_BUSY_SUBBUFFER = 8,
	/**
	 * @brief Wrong subbuffer pointer passed to swap_buffer module.
	 */
	E_SD_WRONG_SUBBUFFER_PTR = 9,
	/**
	 * @brief Unhandled swap_buffer error.
	 */
	E_SD_BUFFER_ERROR = 10,
	/**
	 * @brief Write to subbuffer error.
	 */
	E_SD_WRITE_ERROR = 11,
	/**
	 * @brief Arguments, been passed to the func, doesn't pass sanity check.
	 */
	E_SD_WRONG_ARGS = 12,
	/**
	 * @brief No memory to allocate.
	 */
	E_SD_NO_MEMORY = 13,
	/**
	 * @brief swap_buffer uninitialization error.
	 */
	E_SD_UNINIT_ERROR = 14,
	/**
	 * @brief Netlink init error.
	 */
	E_SD_NL_INIT_ERR = 15,
	/**
	 * @brief Netlink message send error.
	 */
	E_SD_NL_MSG_ERR = 16,
	/**
	 * @brief No daemon pid in us_interaction.
	 */
	E_SD_NO_DAEMON_PID = 17,
	/**
	 * @brief Buffer already enabled
	 */
	E_SD_BUFFER_ENABLED = 18,
	/**
	 * @brief Buffer already disabled
	 */
	E_SD_BUFFER_DISABLED = 19,
};

#endif /* __SWAP_DRIVER_ERRORS_H__ */
