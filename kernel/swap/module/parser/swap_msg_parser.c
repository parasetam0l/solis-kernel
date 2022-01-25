/**
 * parser/swap_msg_parser.c
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov
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
 * Parser module interface implementation.
 */


#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/cpumask.h>
#include <linux/uaccess.h>

#include "parser_defs.h"
#include "us_inst.h"
#include "msg_buf.h"
#include "msg_cmd.h"
#include "usm_msg.h"
#include "cpu_ctrl.h"
#include "features.h"

#include <driver/driver_to_msg.h>
#include <driver/swap_ioctl.h>
#include <master/swap_initializer.h>

/**
 * @enum MSG_ID
 * @brief Message IDs.
 */
enum MSG_ID {
	MSG_KEEP_ALIVE		= 0x0001,	/**< Keep alive message. */
	MSG_START		= 0x0002,	/**< Start message. */
	MSG_STOP		= 0x0003,	/**< Stop message. */
	MSG_CONFIG		= 0x0004,	/**< Config message. */
	MSG_SWAP_INST_ADD	= 0x0008,	/**< Swap inst add message. */
	MSG_SWAP_INST_REMOVE	= 0x0009,	/**< Swap inst remove message. */
	MSG_WRT_LAUNCHER_PORT	= 0x8001	/**< WRT launcher port. */
};

/**
 * @struct basic_msg_fmt
 * @brief Basic part of each message.
 */
struct basic_msg_fmt {
	u32 msg_id;                         /**< Message ID. */
	u32 len;                            /**< Message length. */
} __packed;

static int msg_handler(void __user *msg)
{
	int ret;
	u32 size;
	enum MSG_ID msg_id;
	struct msg_buf mb;
	void __user *payload;
	struct basic_msg_fmt bmf;
	enum { size_max = 128 * 1024 * 1024 };

	ret = copy_from_user(&bmf, (void *)msg, sizeof(bmf));
	if (ret)
		return ret;

	size = bmf.len;
	if (size >= size_max) {
		printk(KERN_INFO "%s: too large message, size=%u\n",
		       __func__, size);
		return -ENOMEM;
	}

	ret = init_mb(&mb, size);
	if (ret)
		return ret;

	payload = msg + sizeof(bmf);
	if (size) {
		ret = copy_from_user(mb.begin, (void *)payload, size);
		if (ret)
			goto uninit;
	}

	msg_id = bmf.msg_id;
	switch (msg_id) {
	case MSG_KEEP_ALIVE:
		print_parse_debug("MSG_KEEP_ALIVE. size=%d\n", size);
		ret = msg_keep_alive(&mb);
		break;
	case MSG_START:
		print_parse_debug("MSG_START. size=%d\n", size);
		ret = msg_start(&mb);
		break;
	case MSG_STOP:
		print_parse_debug("MSG_STOP. size=%d\n", size);
		ret = msg_stop(&mb);
		break;
	case MSG_CONFIG:
		print_parse_debug("MSG_CONFIG. size=%d\n", size);
		ret = msg_config(&mb);
		break;
	case MSG_SWAP_INST_ADD:
		print_parse_debug("MSG_SWAP_INST_ADD. size=%d\n", size);
		ret = msg_swap_inst_add(&mb);
		break;
	case MSG_SWAP_INST_REMOVE:
		print_parse_debug("MSG_SWAP_INST_REMOVE. size=%d\n", size);
		ret = msg_swap_inst_remove(&mb);
		break;
	case MSG_WRT_LAUNCHER_PORT: {
		/* TODO: discuss wrt-launcher port transfer */
		int port;
		print_parse_debug("MSG_WRT_LAUNCHER_PORT. size=%d\n", size);
		port = get_wrt_launcher_port();
		if (copy_to_user(payload, &port, sizeof(port))) {
			ret = -EIO;
			break;
		}
		ret = port ? 0 : -EINVAL;
		break;
	}
	default:
		print_err("incorrect message ID [%u]. size=%d\n", msg_id, size);
		ret = -EINVAL;
		break;
	}

uninit:
	uninit_mb(&mb);
	return ret;
}

static struct driver_msg_handler dmsg_handler = {
	.mod = THIS_MODULE,
	.handler = msg_handler,
};

static int reg_msg_handler(void)
{
	driver_msg_handler_set(&dmsg_handler);
	return 0;
}

static void unreg_msg_handler(void)
{
	driver_msg_handler_set(NULL);
	app_list_unreg_all();

	disable_all_features();
}

static int once(void)
{
	int ret;

	ret = once_cmd();
	if (ret)
		return ret;

	ret = init_cpu_deps();

	return ret;
}

SWAP_LIGHT_INIT_MODULE(once, reg_msg_handler, unreg_msg_handler, NULL, NULL);

MODULE_LICENSE("GPL");
