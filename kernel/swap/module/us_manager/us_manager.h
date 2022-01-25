/**
 * @file us_manager/us_manager.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2013
 */

#ifndef _US_MANAGER_H
#define _US_MANAGER_H


/** Quiet mode */
enum quiet_type {
	QT_ON,		/**< Quiet mode - on */
	QT_OFF		/**< Quiet mode - off */
};

/** Instrumentation status */
enum status_type {
	ST_OFF,		/**< Instrumentation status - off */
	ST_ON		/**< Instrumentation status - on */
};

void set_quiet(enum quiet_type q);
enum quiet_type get_quiet(void);

enum status_type usm_get_status(void);
void usm_put_status(enum status_type st);

int usm_start(void);
int usm_stop(void);

#endif /* _US_MANAGER_H */
