#ifndef _ENERGY_H
#define _ENERGY_H

/**
 * @file energy/energy.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENCE
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
 *
 * @section DESCRIPTION
 *
 */


#include <linux/types.h>


/** Description of parameters */
enum parameter_energy {
	PE_TIME_IDLE,		/**< IDLE working time */
	PE_TIME_SYSTEM,		/**< system working time */
	PE_TIME_APPS,		/**< apps working time */
	PE_READ_SYSTEM,		/**< number of bytes are read by system */
	PE_WRITE_SYSTEM,	/**< number of bytes are write by system */
	PE_READ_APPS,		/**< number of bytes are read by apps */
	PE_WRITE_APPS,		/**< number of bytes are write by apps */
	PE_WF_RECV_SYSTEM,	/**< number of bytes are receive by system through wifi */
	PE_WF_SEND_SYSTEM,	/**< number of bytes are send by system through wifi */
	PE_WF_RECV_APPS,	/**< number of bytes are receive by apps through wifi */
	PE_WF_SEND_APPS,	/**< number of bytes are send by apps through wifi */
	PE_L2CAP_RECV_SYSTEM,	/**< number of bytes(ACL packets) are recv by system through bluetooth */
	PE_L2CAP_RECV_APPS,	/**< number of bytes(ACL packets) are recv by apps through bluetooth */
	PE_SCO_RECV_SYSTEM,	/**< number of bytes(SCO packets) are recv by system through bluetooth */
	PE_SCO_RECV_APPS,	/**< number of bytes(SCO packets) are recv by apps through bluetooth */
	PT_SEND_ACL_SYSTEM,	/**< number of bytes(ACL packets) are send by system through bluetooth */
	PT_SEND_ACL_APPS,	/**< number of bytes(ACL packets) are send by apps through bluetooth */
	PT_SEND_SCO_SYSTEM,	/**< number of bytes(SCO packets) are send by system through bluetooth */
	PT_SEND_SCO_APPS,	/**< number of bytes(SCO packets) are send by apps through bluetooth */
};


int energy_once(void);
int energy_init(void);
void energy_uninit(void);

int set_energy(void);
int unset_energy(void);

int get_parameter_energy(enum parameter_energy pe, void *buf, size_t sz);

#endif /* _ENERGY_H */
