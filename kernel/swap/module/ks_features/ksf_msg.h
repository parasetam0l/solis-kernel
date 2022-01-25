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


#ifndef _KSF_MSG_H
#define _KSF_MSG_H


enum probe_t {
	PT_KS_NONE	= 0x00,
	PT_KS_FILE	= 0x01,
	PT_KS_IPC	= 0x02,
	PT_KS_PROCESS	= 0x04,
	PT_KS_SIGNAL	= 0x08,
	PT_KS_NETWORK	= 0x10,
	PT_KS_DESC	= 0x20
};


enum file_api_t {
	FOPS_OPEN		= 0,
	FOPS_CLOSE		= 1,
	FOPS_READ_BEGIN		= 2,
	FOPS_READ_END		= 3,
	FOPS_READ		= FOPS_READ_BEGIN,
	FOPS_WRITE_BEGIN	= 4,
	FOPS_WRITE_END		= 5,
	FOPS_WRITE		= FOPS_WRITE_BEGIN,
	FOPS_DIRECTORY		= 6,
	FOPS_PERMS		= 7,
	FOPS_OTHER		= 8,
	FOPS_SEND		= 9,
	FOPS_RECV		= 10,
	FOPS_OPTION		= 11,
	FOPS_MANAGE		= 12,
	FOPS_LOCK_START		= 14, /* 13 */
	FOPS_LOCK_END		= 15,
	FOPS_LOCK_RELEASE	= 16
};


struct pt_regs;


void ksf_msg_entry(struct pt_regs *regs, unsigned long func_addr,
		   enum probe_t type, const char *fmt);
void ksf_msg_exit(struct pt_regs *regs, unsigned long func_addr,
		  unsigned long ret_addr, enum probe_t type, char ret_type);

void ksf_msg_file_entry(int fd, enum file_api_t api, const char *path);
void ksf_msg_file_entry_open(int fd, enum file_api_t api, const char *path,
			     const char __user *ofile);
void ksf_msg_file_entry_lock(int fd, enum file_api_t api, const char *path,
			     int type, int whence, s64 start, s64 len);
void ksf_msg_file_exit(struct pt_regs *regs, char ret_type);

void ksf_switch_entry(struct task_struct *task);
void ksf_switch_exit(struct task_struct *task);


#endif /* _KSF_MSG_H */
