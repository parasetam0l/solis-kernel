/**
 * writer/swap_writer_module.c
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 * Packing and writing data.
 */


#include <linux/module.h>
#include <master/swap_initializer.h>
#include "swap_msg.h"
#include "event_filter.h"
#include "debugfs_writer.h"


static int core_init(void)
{
	int ret;

	ret = swap_msg_init();
	if (ret)
		return ret;

	ret = event_filter_init();
	if (ret)
		swap_msg_exit();

	return ret;
}

static void core_exit(void)
{
	event_filter_exit();
	swap_msg_exit();
}

SWAP_LIGHT_INIT_MODULE(NULL, core_init, core_exit,
		       init_debugfs_writer, exit_debugfs_writer);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP Writer module");
MODULE_AUTHOR("Cherkashin V., Aksenov A.S.");
