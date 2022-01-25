#ifndef __DEBUGFS_US_MANAGER_H__
#define __DEBUGFS_US_MANAGER_H__

/**
 * @file us_manager/debugfs_us_manager.h
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 *
 * @section LICENSE
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
 * Copyright (C) Samsung Electronics, 2014
 */

/**
 * @def US_MANAGER_DFS_DIR @hideinitializer
 * Name in debugfs
 */
#define US_MANAGER_DFS_DIR "us_manager"

/**
 * @def US_MANAGER_DFS_DIR @hideinitializer
 * Name in debugfs
 */
#define US_MANAGER_TASKS   "tasks"

int init_debugfs_us_manager(void);
void exit_debugfs_us_manager(void);

#endif /* __DEBUGFS_US_MANAGER_H__ */
