/**
 * parser/msg_cmd.c
 * @author Vyacheslav Cherkashin
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
 * Module's messages parsing implementation.
 */


#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "msg_parser.h"
#include "msg_buf.h"
#include "features.h"
#include "parser_defs.h"
#include "us_inst.h"
#include <writer/swap_msg.h>
#include <us_manager/us_manager.h>


static int wrt_launcher_port;

static int set_config(struct conf_data *conf)
{
	int ret;

	ret = set_features(conf);

	return ret;
}

/**
 * @brief Message "keep alive" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_keep_alive(struct msg_buf *mb)
{
	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%zu", remained_mb(mb));
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Message "start" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_start(struct msg_buf *mb)
{
	int ret = 0;
	struct conf_data conf;
	LIST_HEAD(app_head);

	swap_msg_seq_num_reset();
	swap_msg_discard_reset();

	if (!create_us_inst_data(mb, &app_head))
		return -EINVAL;

	if (!is_end_mb(mb)) {
		pr_info("Too long message, remained=%zu", remained_mb(mb));
		destroy_us_inst_data(&app_head);
		return -EINVAL;
	}

	ret = app_list_reg(&app_head);
	if (ret) {
		pr_info("Cannot mod us inst, ret = %d\n", ret);
		return ret;
	}

	ret = usm_start();
	if (ret) {
		app_list_unreg_all();
		return ret;
	}

	restore_config(&conf);
	set_config(&conf);

	return 0;
}

/**
 * @brief Message "stop" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_stop(struct msg_buf *mb)
{
	int ret = 0;
	int discarded;

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%zu", remained_mb(mb));
		return -EINVAL;
	}

	ret = usm_stop();
	if (ret)
		return ret;

	app_list_unreg_all();
	disable_all_features();

	discarded = swap_msg_discard_get();
	printk(KERN_INFO "discarded messages: %d\n", discarded);
	swap_msg_discard_reset();

	return ret;
}

/**
 * @brief Message "config" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_config(struct msg_buf *mb)
{
	int ret = 0;
	struct conf_data *conf;
	enum status_type st;

	conf = create_conf_data(mb);
	if (conf == NULL)
		return -EINVAL;

	if (!is_end_mb(mb)) {
		print_err("to long message, remained=%zu", remained_mb(mb));
		ret = -EINVAL;
		goto free_conf_data;
	}

	st = usm_get_status();
	if (st == ST_ON)
		set_config(conf);

	save_config(conf);
	usm_put_status(st);

free_conf_data:
	destroy_conf_data(conf);

	return ret;
}

/**
 * @brief Message "swap inst add" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_swap_inst_add(struct msg_buf *mb)
{
	LIST_HEAD(app_head);

	if (!create_us_inst_data(mb, &app_head))
		return -EINVAL;

	if (!is_end_mb(mb)) {
		pr_info("Too long message, remained=%zu", remained_mb(mb));
		destroy_us_inst_data(&app_head);
		return -EINVAL;
	}

	return app_list_reg(&app_head);
}

/**
 * @brief Message "swap inst remove" handling.
 *
 * @param mb Pointer to the message buffer.
 * @return 0 on success, negative error code on error.
 */
int msg_swap_inst_remove(struct msg_buf *mb)
{
	LIST_HEAD(app_head);

	INIT_LIST_HEAD(&app_head);
	if (!create_us_inst_data(mb, &app_head))
		return -EINVAL;

	if (!is_end_mb(mb)) {
		pr_info("Too long message, remained=%zu", remained_mb(mb));
		destroy_us_inst_data(&app_head);
		return -EINVAL;
	}

	return app_list_unreg(&app_head);
}

void set_wrt_launcher_port(int port)
{
	wrt_launcher_port = port;
}
EXPORT_SYMBOL_GPL(set_wrt_launcher_port);

#define GET_PORT_DELAY		100	/* msec */
#define GET_PORT_TIMEOUT	10000	/* msec */

int get_wrt_launcher_port(void)
{
	int port;
	int timeout = GET_PORT_TIMEOUT;

	do {
		port = wrt_launcher_port;
		timeout -= GET_PORT_DELAY;
		mdelay(GET_PORT_DELAY);
	} while (!port && timeout > 0);

	set_wrt_launcher_port(0);

	return port;
}

/**
 * @brief Initializes commands handling.
 *
 * @return Initialization results.
 */
int once_cmd(void)
{
	return once_features();
}
