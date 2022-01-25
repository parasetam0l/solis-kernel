/**
 * @file us_manager/img/img_proc.h
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
 */


#ifndef _IMG_PROC_H
#define _IMG_PROC_H

#include <linux/types.h>

struct dentry;
struct sspt_proc;
struct probe_desc;


struct img_proc *img_proc_create(void);
void img_proc_free(struct img_proc *proc);

struct img_ip *img_proc_add_ip(struct img_proc *proc, struct dentry *dentry,
			       unsigned long addr, struct probe_desc *pd);
void img_proc_del_ip(struct img_proc *proc, struct img_ip *ip);

void img_proc_copy_to_sspt(struct img_proc *i_proc, struct sspt_proc *proc);
bool img_proc_is_unloadable(void);

/* debug */
void img_proc_print(struct img_proc *proc);
/* debug */

#endif /* _IMG_PROC_H */
