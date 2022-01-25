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


#ifndef _SWAP_MSG_H
#define _SWAP_MSG_H

#include <linux/version.h>
#include <linux/types.h>



#include <linux/ktime.h>       /* Needed by timekeeping.h */
#include <linux/timekeeping.h> /* Now getnstimeofday() is here */


enum swap_msg_id {
	MSG_PROC_INFO			= 0x0001,
	MSG_TERMINATE			= 0x0002,
	MSG_ERROR			= 0x0003,
	MSG_SAMPLE			= 0x0004,
	MSG_FUNCTION_ENTRY		= 0x0008,
	MSG_FUNCTION_EXIT		= 0x0009,
	MSG_SYSCALL_ENTRY		= 0x000a,
	MSG_SYSCALL_EXIT		= 0x000b,
	MSG_FILE_FUNCTION_ENTRY		= 0x000c,
	MSG_FILE_FUNCTION_EXIT		= 0x000d,
	MSG_PROCESS_STATUS_INFO		= 0x000e,
	MSG_CONTEXT_SWITCH_ENTRY	= 0x0010,
	MSG_CONTEXT_SWITCH_EXIT		= 0x0011,
	MSG_PROC_MAP			= 0x0012,
	MSG_PROC_UNMAP			= 0x0013,
	MSG_PROC_COMM			= 0x0014,
	MSG_WEB_PROFILING		= 0x0015,
	MSG_NSP				= 0x0019,
	MSG_WSP				= 0x001a,
	MSG_FBI				= 0x0020
};

enum {
	SWAP_MSG_PRIV_DATA = 20,
	SWAP_MSG_BUF_SIZE = 32 * 1024,
	SWAP_MSG_PAYLOAD_SIZE = SWAP_MSG_BUF_SIZE - SWAP_MSG_PRIV_DATA
};


struct swap_msg;


static inline u64 swap_msg_spec2time(struct timespec *ts)
{
	return ((u64)ts->tv_nsec) << 32 | ts->tv_sec;
}

static inline u64 swap_msg_current_time(void)
{
	struct timespec ts;
	getnstimeofday(&ts);
	return swap_msg_spec2time(&ts);
}

struct swap_msg *swap_msg_get(enum swap_msg_id id);
int swap_msg_flush(struct swap_msg *m, size_t size);
int swap_msg_flush_wakeupoff(struct swap_msg *m, size_t size);
void swap_msg_put(struct swap_msg *m);

static inline void *swap_msg_payload(struct swap_msg *m)
{
	return (void *)m + SWAP_MSG_PRIV_DATA;
}

static inline size_t swap_msg_size(struct swap_msg *m)
{
	return (size_t)SWAP_MSG_PAYLOAD_SIZE;
}


int swap_msg_pack_args(char *buf, int len,
		       const char *fmt, struct pt_regs *regs);
int swap_msg_pack_ret_val(char *buf, int len,
			  char ret_type, struct pt_regs *regs);


int swap_msg_raw(void *buf, size_t size);
void swap_msg_error(const char *fmt, ...);

void swap_msg_seq_num_reset(void);
void swap_msg_discard_reset(void);
int swap_msg_discard_get(void);

int swap_msg_init(void);
void swap_msg_exit(void);


#endif /* _SWAP_MSG_H */
