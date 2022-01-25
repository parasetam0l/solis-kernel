/**
 * driver/us_interaction.c
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
 * Kernel-to-user interface implementation.
 */


#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/connector.h>
#include <linux/slab.h>

#include "us_interaction.h"
#include "us_interaction_msg.h"
#include "swap_driver_errors.h"
#include "driver_defs.h"


/* Connector id struct */
static struct cb_id cn_swap_id = {CN_SWAP_IDX, CN_SWAP_VAL};

/* Swap connector name */
static const char cn_swap_name[] = "cn_swap";

/* Send messages counter */
static u32 msg_counter;

/**
 * @brief Sends message to userspace via netlink.
 *
 * @param data Pointer to the data to be send.
 * @param size Size of the data to be send.
 * @return 0 on success, error code on error.
 */
int us_interaction_send_msg(const void *data, size_t size)
{
	struct cn_msg *msg;
	int ret;

	msg = kzalloc(sizeof(*msg) + size, GFP_ATOMIC);
	if (msg == NULL)
		return -E_SD_NO_MEMORY;

	memcpy(&msg->id, &cn_swap_id, sizeof(msg->id));
	msg->seq = msg_counter;
	msg->len = size;
	memcpy(msg->data, data, msg->len);

	ret = cn_netlink_send(msg, 0, CN_DAEMON_GROUP, GFP_ATOMIC);
	if (ret < 0)
		goto fail_send;
	kfree(msg);

	msg_counter++;

	return E_SD_SUCCESS;

fail_send:
	kfree(msg);

	return ret;
}

static void us_interaction_recv_msg(struct cn_msg *msg,
				    struct netlink_skb_parms *nsp)
{
}

/**
 * @brief Creates netlink connection.
 *
 * @return 0 on success, error code on error.
 */
int us_interaction_create(void)
{
	int res;

	res = cn_add_callback(&cn_swap_id,
			      cn_swap_name,
			      us_interaction_recv_msg);
	if (res)
		return -E_SD_NL_INIT_ERR;

	return E_SD_SUCCESS;
}

/**
 * @brief Destroy netlink connection.
 *
 * @return Void.
 */
void us_interaction_destroy(void)
{
	cn_del_callback(&cn_swap_id);
}
