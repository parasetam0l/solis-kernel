/*
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Web startup profiling
 */

#include <master/swap_initializer.h>
#include "wsp.h"
#include "wsp_debugfs.h"

SWAP_LIGHT_INIT_MODULE(NULL, wsp_init, wsp_exit,
		       wsp_debugfs_init, wsp_debugfs_exit);

MODULE_LICENSE("GPL");
