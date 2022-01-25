/**
 * @file driver/us_interaction_msg.h
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
 * Netlink messages declaration.
 */

#ifndef __US_INTERACTION_MSG_H__
#define __US_INTERACTION_MSG_H__

#define CN_SWAP_IDX     0x22    /**< Should be unique throughout the system */
#define CN_SWAP_VAL     0x1     /**< Just the same in kernel and user */
#define CN_DAEMON_GROUP 0x1     /**< Listener group. Connector works a bit
				 * faster when using one */

/**
 * @enum us_interaction_k2u_msg_t
 * @brief Kernel-to-user netlink messages headers.
 */
enum us_interaction_k2u_msg_t {
	/**
	 * @brief Make daemon pause apps.
	 */
	US_INT_PAUSE_APPS = 1,
	/**
	 * @brief Make daemon continue apps.
	 */
	US_INT_CONT_APPS = 2
};

#endif /* __US_INTERACTION_MSG_H__ */
