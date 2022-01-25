/*
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#ifndef _USM_MSG_H
#define _USM_MSG_H


struct dentry;
struct task_struct;
struct vm_area_struct;


void usm_msg_info(struct task_struct *task, struct dentry *dentry);
void usm_msg_term(struct task_struct *task);
void usm_msg_map(struct vm_area_struct *vma);
void usm_msg_unmap(unsigned long start, unsigned long end);
void usm_msg_comm(struct task_struct *task);
void usm_msg_status_info(struct task_struct *task);


#endif /* _USM_MSG_H */
