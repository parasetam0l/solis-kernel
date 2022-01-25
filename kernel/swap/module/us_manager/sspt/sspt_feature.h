#ifndef _SSPT_FEATUER_H
#define _SSPT_FEATUER_H

/**
 * @file us_manager/sspt/sspt_feature.h
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

struct sspt_feature;

typedef void *sspt_feature_id_t;	/**< @brief sspt feature ID type */
#define SSPT_FEATURE_ID_BAD	NULL	/**< @def SSPT_FEATURE_ID_BAD */

struct sspt_feature *sspt_create_feature(void);
void sspt_destroy_feature(struct sspt_feature *f);

void *sspt_get_feature_data(struct sspt_feature *f, sspt_feature_id_t id);
sspt_feature_id_t sspt_register_feature(void *(*alloc)(void),
					void (*free)(void *data));
void sspt_unregister_feature(sspt_feature_id_t id);

#endif /* _SSPT_FEATUER_H */
