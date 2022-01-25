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


#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <buffer/swap_buffer_module.h>
#include <swap-asm/swap_kprobes.h>
#include <swap-asm/swap_uprobes.h>
#include "swap_msg.h"


#define MSG_PREFIX	"[SWAP_MSG] "


/* simple buffer */
struct sb_struct {
	size_t subbuf_size;

	size_t count;
	void *data;
};

static int sb_init(struct sb_struct *sb, size_t count, size_t subbuf_size)
{
	sb->data = vmalloc(count * subbuf_size);
	if (!sb->data)
		return -ENOMEM;

	sb->count = count;
	sb->subbuf_size = subbuf_size;

	return 0;
}

static void sb_uninit(struct sb_struct *sb)
{
	vfree(sb->data);
}

static void *sb_data(struct sb_struct *sb, size_t idx)
{
	return sb->data + sb->subbuf_size * idx;
}

static size_t sb_idx(struct sb_struct *sb, void *data)
{
	return (data - sb->data) / sb->subbuf_size;
}

static bool sb_contains(struct sb_struct *sb, void *data)
{
	void *begin = sb->data;
	void *end = sb->data + sb->count * sb->subbuf_size;

	return data >= begin && data < end;
}

static size_t sb_count(struct sb_struct *sb)
{
	return sb->count;
}


/* pool buffer */
struct pb_struct {
	spinlock_t lock;
	size_t free_first;
	size_t free_count;

	struct sb_struct buf;
};

static void *pb_data(struct pb_struct *pb, size_t idx)
{
	return sb_data(&pb->buf, idx);
}

static size_t pb_idx(struct pb_struct *pb, void *data)
{
	return sb_idx(&pb->buf, data);
}

static void pb_val_set(struct pb_struct *pb, size_t idx, size_t val)
{
	*(size_t *)pb_data(pb, idx) = val;
}

static size_t pb_val_get(struct pb_struct *pb, size_t idx)
{
	return *(size_t *)pb_data(pb, idx);
}

static int pb_init(struct pb_struct *pb, size_t count, size_t subbuf_size)
{
	int ret;
	size_t idx;

	ret = sb_init(&pb->buf, count, subbuf_size);
	if (ret)
		return ret;

	spin_lock_init(&pb->lock);
	pb->free_first = 0;
	pb->free_count = count;

	for (idx = 0; idx < count; ++idx)
		pb_val_set(pb, idx, idx + 1);

	return 0;
}

static void pb_uninit(struct pb_struct *pb)
{
	WARN(sb_count(&pb->buf) != pb->free_count,
	     "count=%zu free_conut=%zu\n", sb_count(&pb->buf), pb->free_count);

	sb_uninit(&pb->buf);
}

static void *pb_buf_get(struct pb_struct *pb)
{
	void *data;
	unsigned long flags;

	if (!pb->free_count)
		return NULL;

	spin_lock_irqsave(&pb->lock, flags);
	data = pb_data(pb, pb->free_first);
	pb->free_first = pb_val_get(pb, pb->free_first);
	--pb->free_count;
	spin_unlock_irqrestore(&pb->lock, flags);

	return data;
}

static void pb_buf_put(struct pb_struct *pb, void *data)
{
	unsigned long flags;
	size_t idx = pb_idx(pb, data);

	spin_lock_irqsave(&pb->lock, flags);
	pb_val_set(pb, idx, pb->free_first);
	pb->free_first = idx;
	++pb->free_count;
	spin_unlock_irqrestore(&pb->lock, flags);
}


struct swap_msg {
	u32 msg_id;
	u32 seq_num;
	u64 time;
	u32 len;
	char payload[0];
} __packed;


static struct sb_struct cpu_buf;
static struct pb_struct pool_buffer;
static atomic_t seq_num = ATOMIC_INIT(-1);
static atomic_t discarded = ATOMIC_INIT(0);


int swap_msg_init(void)
{
	int ret;

	ret = sb_init(&cpu_buf, NR_CPUS, SWAP_MSG_BUF_SIZE);
	if (ret) {
		pr_err(MSG_PREFIX "Cannot init cpu_buf, ret=%d\n", ret);
		return ret;
	}

	ret = pb_init(&pool_buffer, NR_CPUS * 32, SWAP_MSG_BUF_SIZE);
	if (ret) {
		sb_uninit(&cpu_buf);
		pr_err(MSG_PREFIX "Cannot init ring_buffer, ret=%d\n", ret);
	}

	return ret;
}

void swap_msg_exit(void)
{
	pb_uninit(&pool_buffer);
	sb_uninit(&cpu_buf);
}

void swap_msg_seq_num_reset(void)
{
	atomic_set(&seq_num, -1);
}
EXPORT_SYMBOL_GPL(swap_msg_seq_num_reset);

void swap_msg_discard_reset(void)
{
	atomic_set(&discarded, 0);
}
EXPORT_SYMBOL_GPL(swap_msg_discard_reset);

int swap_msg_discard_get(void)
{
	return atomic_read(&discarded);
}
EXPORT_SYMBOL_GPL(swap_msg_discard_get);


u64 swap_msg_timespec2time(struct timespec *ts)
{
	return ((u64)ts->tv_nsec) << 32 | ts->tv_sec;
}





struct swap_msg *swap_msg_get(enum swap_msg_id id)
{
	struct swap_msg *m;

	m = pb_buf_get(&pool_buffer);
	if (!m)
		m = sb_data(&cpu_buf, get_cpu());

	m->msg_id = (u32)id;
	m->seq_num = atomic_inc_return(&seq_num);
	m->time = swap_msg_current_time();

	return m;
}
EXPORT_SYMBOL_GPL(swap_msg_get);

static int __swap_msg_flush(struct swap_msg *m, size_t size, bool wakeup)
{
	if (unlikely(size >= SWAP_MSG_PAYLOAD_SIZE))
		return -ENOMEM;

	m->len = size;

	if (swap_buffer_write(m, SWAP_MSG_PRIV_DATA + size, wakeup) !=
	    (SWAP_MSG_PRIV_DATA + size)) {
		atomic_inc(&discarded);
		return -EINVAL;
	}

	return 0;
}

int swap_msg_flush(struct swap_msg *m, size_t size)
{
	return __swap_msg_flush(m, size, true);
}
EXPORT_SYMBOL_GPL(swap_msg_flush);

int swap_msg_flush_wakeupoff(struct swap_msg *m, size_t size)
{
	return __swap_msg_flush(m, size, false);
}
EXPORT_SYMBOL_GPL(swap_msg_flush_wakeupoff);

void swap_msg_put(struct swap_msg *m)
{
	if (unlikely(sb_contains(&cpu_buf, m)))
		put_cpu();
	else
		pb_buf_put(&pool_buffer, m);
}
EXPORT_SYMBOL_GPL(swap_msg_put);






static unsigned long get_arg(struct pt_regs *regs, unsigned long n)
{
	return user_mode(regs) ?
			swap_get_uarg(regs, n) :	/* US argument */
			swap_get_sarg(regs, n);		/* sys_call argument */
}

int swap_msg_pack_args(char *buf, int len,
		       const char *fmt, struct pt_regs *regs)
{
	char *buf_old = buf;
	u32 *tmp_u32;
	u64 *tmp_u64;
	int i,		/* the index of the argument */
	    fmt_i,	/* format index */
	    fmt_len;	/* the number of parameters, in format */

	fmt_len = strlen(fmt);

	for (i = 0, fmt_i = 0; fmt_i < fmt_len; ++i, ++fmt_i) {
		if (len < 2)
			return -ENOMEM;

		*buf = fmt[fmt_i];
		buf += 1;
		len -= 1;

		switch (fmt[fmt_i]) {
		case 'b': /* 1 byte(bool) */
			*buf = (char)!!get_arg(regs, i);
			buf += 1;
			len -= 1;
			break;
		case 'c': /* 1 byte(char) */
			*buf = (char)get_arg(regs, i);
			buf += 1;
			len -= 1;
			break;
		case 'f': /* 4 byte(float) */
#ifdef CONFIG_ARM64
			if (len < 4)
				return -ENOMEM;

			tmp_u32 = (u32 *)buf;
			*tmp_u32 = swap_get_float(regs, i);
			buf += 4;
			len -= 4;
			break;
#endif /* CONFIG_ARM64 */
			/* For others case f == d */
		case 'd': /* 4 byte(int) */
			if (len < 4)
				return -ENOMEM;
			tmp_u32 = (u32 *)buf;
			*tmp_u32 = (u32)get_arg(regs, i);
			buf += 4;
			len -= 4;
			break;
		case 'x': /* 8 byte(long) */
		case 'p': /* 8 byte(pointer) */
			if (len < 8)
				return -ENOMEM;
			tmp_u64 = (u64 *)buf;
			*tmp_u64 = (u64)get_arg(regs, i);
			buf += 8;
			len -= 8;
			break;
		case 'w': /* 8 byte(double) */
			if (len < 8)
				return -ENOMEM;
			tmp_u64 = (u64 *)buf;
#ifdef CONFIG_ARM64
			*tmp_u64 = swap_get_double(regs, i);
#else /* CONFIG_ARM64 */
			*tmp_u64 = get_arg(regs, i);
			++i;
			*tmp_u64 |= (u64)get_arg(regs, i) << 32;
#endif /* CONFIG_ARM64 */
			buf += 8;
			len -= 8;
			break;
		case 's': /* string end with '\0' */
		{
			enum { max_str_len = 512 };
			const char __user *user_s;
			int len_s, ret;

			user_s = (const char __user *)get_arg(regs, i);
			len_s = strnlen_user(user_s, max_str_len);
			if (len < len_s)
				return -ENOMEM;

			ret = strncpy_from_user(buf, user_s, len_s);
			if (ret < 0)
				return -EFAULT;

			buf[ret] = '\0';

			buf += ret + 1;
			len -= ret + 1;
		}
			break;
		default:
			return -EINVAL;
		}
	}

	return buf - buf_old;
}
EXPORT_SYMBOL_GPL(swap_msg_pack_args);

int swap_msg_pack_ret_val(char *buf, int len,
			  char ret_type, struct pt_regs *regs)
{
	const char *buf_old = buf;
	u32 *tmp_u32;
	u64 *tmp_u64;

	*buf = ret_type;
	++buf;

	switch (ret_type) {
	case 'b': /* 1 byte(bool) */
		if (len < 1)
			return -ENOMEM;
		*buf = (char)!!regs_return_value(regs);
		++buf;
		break;
	case 'c': /* 1 byte(char) */
		if (len < 1)
			return -ENOMEM;
		*buf = (char)regs_return_value(regs);
		++buf;
		break;
	case 'd': /* 4 byte(int) */
		if (len < 4)
			return -ENOMEM;
		tmp_u32 = (u32 *)buf;
		*tmp_u32 = regs_return_value(regs);
		buf += 4;
		break;
	case 'x': /* 8 byte(long) */
	case 'p': /* 8 byte(pointer) */
		if (len < 8)
			return -ENOMEM;
		tmp_u64 = (u64 *)buf;
		*tmp_u64 = (u64)regs_return_value(regs);
		buf += 8;
		break;
	case 's': /* string end with '\0' */
	{
		enum { max_str_len = 512 };
		const char __user *user_s;
		int len_s, ret;

		user_s = (const char __user *)regs_return_value(regs);
		len_s = strnlen_user(user_s, max_str_len);
		if (len < len_s)
			return -ENOMEM;

		ret = strncpy_from_user(buf, user_s, len_s);
		if (ret < 0)
			return -EFAULT;

		buf[ret] = '\0';
		buf += ret + 1;
	}
		break;
	case 'n':
	case 'v':
		break;
	case 'f': /* 4 byte(float) */
		if (len < 4)
			return -ENOMEM;
		tmp_u32 = (u32 *)buf;
		*tmp_u32 = swap_get_urp_float(regs);
		buf += 4;
		break;
	case 'w': /* 8 byte(double) */
		if (len < 8)
			return -ENOMEM;
		tmp_u64 = (u64 *)buf;
		*tmp_u64 = swap_get_urp_double(regs);
		buf += 8;
		break;
	default:
		return -EINVAL;
	}

	return buf - buf_old;
}
EXPORT_SYMBOL_GPL(swap_msg_pack_ret_val);





int swap_msg_raw(void *data, size_t size)
{
	struct swap_msg *m = (struct swap_msg *)data;

	if (sizeof(*m) > size) {
		pr_err(MSG_PREFIX "ERROR: message RAW small size=%zu\n", size);
		return -EINVAL;
	}

	if (m->len + sizeof(*m) != size) {
		pr_err(MSG_PREFIX "ERROR: message RAW wrong format\n");
		return -EINVAL;
	}

	m->seq_num = atomic_inc_return(&seq_num);

	/* TODO: What should be returned?! When message was discarded. */
	if (swap_buffer_write(m, size, true) != size)
		atomic_inc(&discarded);

	return size;
}
EXPORT_SYMBOL_GPL(swap_msg_raw);

void swap_msg_error(const char *fmt, ...)
{
	int ret;
	struct swap_msg *m;
	void *p;
	size_t size;
	va_list args;

	m = swap_msg_get(MSG_ERROR);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	va_start(args, fmt);
	ret = vsnprintf(p, size, fmt, args);
	va_end(args);

	if (ret <= 0) {
		pr_err(MSG_PREFIX "ERROR: msg error packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, ret + 1);

put_msg:
	swap_msg_put(m);
}
EXPORT_SYMBOL_GPL(swap_msg_error);
