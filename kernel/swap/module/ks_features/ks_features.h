/**
 * @file ks_features/ks_features.h
 * @author Vyacheslav Cherkashin: SWAP ks_features implement
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
 * SWAP kernel features interface declaration.
 */


#ifndef _KS_FEATURES_H
#define _KS_FEATURES_H

/**
 * @enum feature_id
 * Features ids
 */
enum feature_id {
	FID_FILE = 1,			/**< File probes */
	FID_IPC = 2,			/**< Hz probes */
	FID_PROCESS = 3,		/**< Process probes */
	FID_SIGNAL = 4,			/**< Signal probes */
	FID_NET = 5,			/**< Network probes */
	FID_DESC = 6,			/**< Description probes */
	FID_SWITCH = 7,			/**< Switch context probes */
	FID_SYSFILE_ACTIVITY = 8	/**< System file activity */
};

int set_feature(enum feature_id id);
int unset_feature(enum feature_id id);

/* debug */
void print_features(void);
void print_all_syscall(void);
/* debug */

#endif /*  _KS_FEATURES_H */
