/**
 * driver/driver_to_buffer.c
 * @author Alexander Aksenov <a.aksenov@samsung.com>
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
 * Driver and buffer interaction interface implementation.
 */

#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/splice.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/mm.h>

#include <buffer/swap_buffer_module.h>
#include <buffer/swap_buffer_errors.h>
#include <buffer/buffer_description.h>

#include "driver_defs.h"
#include "swap_driver_errors.h"
#include "device_driver_to_driver_to_buffer.h"
#include "app_manage.h"

/** Maximum subbuffer size. Used for sanitization checks. */
#define MAXIMUM_SUBBUFFER_SIZE (64 * 1024)

/* Current busy buffer */
static struct swap_subbuffer *busy_buffer;

/* Buffers count ready to be read */
static int buffers_to_read;

/* Pages count in one subbuffer */
static int pages_per_buffer;

/* Used to sync changes of the buffers_to_read var */
static spinlock_t buf_to_read;


static inline void init_buffers_to_read(void)
{
	spin_lock_init(&buf_to_read);
	buffers_to_read = 0;
}

static inline void inc_buffers_to_read(void)
{
	unsigned long flags;

	spin_lock_irqsave(&buf_to_read, flags);
	buffers_to_read++;
	spin_unlock_irqrestore(&buf_to_read, flags);
}

static inline void dec_buffers_to_read(void)
{
	unsigned long flags;

	spin_lock_irqsave(&buf_to_read, flags);
	buffers_to_read--;
	spin_unlock_irqrestore(&buf_to_read, flags);
}

static inline void set_buffers_to_read(int count)
{
	unsigned long flags;

	spin_lock_irqsave(&buf_to_read, flags);
	buffers_to_read = count;
	spin_unlock_irqrestore(&buf_to_read, flags);
}

static inline int something_to_read(void)
{
	unsigned long flags;
	int result;

	spin_lock_irqsave(&buf_to_read, flags);
	result = buffers_to_read;
	spin_unlock_irqrestore(&buf_to_read, flags);

	return result;
}

/* TODO Get subbuffer for reading */
static size_t driver_to_buffer_get(void)
{
	int result;

	/* If there is no readable buffers, return error */
	result = swap_buffer_get(&busy_buffer);
	if (result == -E_SB_NO_READABLE_BUFFERS) {
		busy_buffer = NULL;
		return -E_SD_NO_DATA_TO_READ;
	} else if (result < 0) {
		print_err("swap_buffer_get unhandle error %d\n", result);
		return -E_SD_BUFFER_ERROR;
	}

	return busy_buffer->full_buffer_part;
}

/* TODO Release subbuffer */
static int driver_to_buffer_release(void)
{
	int result;

	if (!busy_buffer)
		return -E_SD_NO_BUSY_SUBBUFFER;

	result = swap_buffer_release(&busy_buffer);
	if (result == -E_SB_NO_SUBBUFFER_IN_BUSY) {
		return -E_SD_WRONG_SUBBUFFER_PTR;
	} else if (result < 0) {
		print_err("swap_buffer_release unhandle error %d\n", result);
		return -E_SD_BUFFER_ERROR;
	}

	busy_buffer = NULL;

	return E_SD_SUCCESS;
}

static int driver_to_buffer_callback(bool wakeup)
{
	/* Increment buffers_to_read counter */
	inc_buffers_to_read();
	if (wakeup)
		swap_device_wake_up_process();

	return E_SD_SUCCESS;
}

/**
 * @brief Copies data from subbuffer to userspace.
 *
 * @param[out] buf Pointer to userspace memory area whereto copy data from
 * subbuffer.
 * @param count Size of data to be read.
 * @return Read data size on success, negative error code on error.
 */
ssize_t driver_to_buffer_read(char __user *buf, size_t count)
{
	size_t bytes_to_copy;
	size_t bytes_to_read = 0;
	int page_counter = 0;

	/* Reading from swap_device means reading only current busy_buffer.
	 * So, if there is no busy_buffer, we don't get next to read, we just
	 * read nothing. In this case, or if there is nothing to read from
	 * busy_buffer - return -E_SD_NO_DATA_TO_READ. It should be correctly
	 * handled in device_driver */
	if (!busy_buffer || !busy_buffer->full_buffer_part)
		return -E_SD_NO_DATA_TO_READ;

	/* Bytes count that we're going to copy to user buffer is equal to user
	 * buffer size or to subbuffer readable size whichever is less */
	bytes_to_copy = (count > busy_buffer->full_buffer_part) ?
		    busy_buffer->full_buffer_part : count;

	/* Copy data from each page to buffer */
	while (bytes_to_copy > 0) {
		/* Get size that should be copied from current page */
		size_t read_from_this_page =
			(bytes_to_copy > PAGE_SIZE) ? PAGE_SIZE
			: bytes_to_copy;

		/* Copy and add size to copied bytes count */

		/* TODO Check with more than one page */
		bytes_to_read += read_from_this_page -
			 copy_to_user(
				 buf, page_address(busy_buffer->data_buffer) +
				 (sizeof(struct page *) *
				  page_counter),
				 read_from_this_page);
		bytes_to_copy -= read_from_this_page;
		page_counter++;
	}

	return bytes_to_read;
}

/**
 * @brief Flushes SWAP buffer.
 *
 * @return 0.
 */
int driver_to_buffer_flush(void)
{
	unsigned int flushed;

	flushed = swap_buffer_flush();
	set_buffers_to_read(flushed);
	swap_device_wake_up_process();

	return E_SD_SUCCESS;
}

/**
 * @brief Fills spd structure.
 *
 * @param[out] spd Pointer to the splice_pipe_desc struct that should be filled.
 * @return 0 on success, negative error code on error.
 */
int driver_to_buffer_fill_spd(struct splice_pipe_desc *spd)
{
	size_t data_to_splice = busy_buffer->full_buffer_part;
	struct page **pages = spd->pages;
	struct partial_page *partial = spd->partial;

	while (data_to_splice) {
		size_t read_from_current_page = min(data_to_splice,
						    (size_t)PAGE_SIZE);

		pages[spd->nr_pages] = alloc_page(GFP_KERNEL);
		if (!pages[spd->nr_pages]) {
			print_err("Cannot alloc page for splice\n");
			return -ENOMEM;
		}

		/* FIXME: maybe there is more efficient way */
		memcpy(page_address(pages[spd->nr_pages]),
	       page_address(&busy_buffer->data_buffer[spd->nr_pages]),
	       read_from_current_page);

		/* Always beginning of the page */
		partial[spd->nr_pages].offset = 0;
		partial[spd->nr_pages].len = read_from_current_page;

		/* Private is not used */
		partial[spd->nr_pages].private = 0;

		spd->nr_pages++;
		data_to_splice -= read_from_current_page;

		/* TODO: add check for pipe->buffers exceeding */
		/* if (spd->nr_pages == pipe->buffers) { */
		/*	break; */
		/* } */
	}
	return 0;
}

/**
 * @brief Check for subbuffer ready to be read.
 *
 * @return 1 if there is subbuffer to be read, 0 - if there isn't.
 */
int driver_to_buffer_buffer_to_read(void)
{
	return busy_buffer ? 1 : 0;
}

static size_t subbuf_size;
static unsigned int subbuf_count;
static bool buffer_enabled;
static DEFINE_MUTEX(buffer_mtx);

bool driver_to_buffer_enabled(void)
{
	return buffer_enabled;
}

enum _swap_driver_errors driver_to_buffer_set_size(size_t size)
{
	enum _swap_driver_errors ret = E_SD_SUCCESS;

	if (!size || size > MAXIMUM_SUBBUFFER_SIZE)
		return -E_SD_WRONG_ARGS;

	mutex_lock(&buffer_mtx);
	if (buffer_enabled) {
		ret = -E_SD_BUFFER_ENABLED;
		goto unlock;
	}

	subbuf_size = size;
unlock:
	mutex_unlock(&buffer_mtx);
	return ret;
}

size_t driver_to_buffer_get_size(void)
{
	return subbuf_size;
}

enum _swap_driver_errors driver_to_buffer_set_count(unsigned int count)
{
	enum _swap_driver_errors ret = E_SD_SUCCESS;

	if (!count)
		return -E_SD_WRONG_ARGS;

	mutex_lock(&buffer_mtx);
	if (buffer_enabled) {
		ret = -E_SD_BUFFER_ENABLED;
		goto unlock;
	}
	subbuf_count = count;

unlock:
	mutex_unlock(&buffer_mtx);
	return ret;
}

unsigned int driver_to_buffer_get_count(void)
{
	return subbuf_count;
}

/**
 * @brief Initializes SWAP buffer.
 *
 * @param size Size of one subbuffer.
 * @param count Count of subbuffers.
 * @return 0 on success, negative error code on error.
 */
int driver_to_buffer_initialize(void)
{
	enum _swap_driver_errors result;
	struct buffer_init_t buf_init = {
		.subbuffer_size = subbuf_size,
		.nr_subbuffers = subbuf_count,
		.subbuffer_full_cb = driver_to_buffer_callback,
		.lower_threshold = 20,
		.low_mem_cb = app_manage_pause_apps,
		.top_threshold = 80,
		.enough_mem_cb = app_manage_cont_apps,
	};

	mutex_lock(&buffer_mtx);
	if (buffer_enabled) {
		result = -E_SD_BUFFER_ENABLED;
		goto unlock;
	}

	result = swap_buffer_init(&buf_init);
	if (result == -E_SB_NO_MEM_QUEUE_BUSY
		|| result == -E_SB_NO_MEM_BUFFER_STRUCT) {
		result = -E_SD_NO_MEMORY;
		goto unlock;
	}

	/* TODO Race condition: buffer can be used in other thread till */
	/* we're in this func */
	/* Initialize driver_to_buffer variables */
	pages_per_buffer = result;
	busy_buffer = NULL;
	init_buffers_to_read();
	result = E_SD_SUCCESS;
	buffer_enabled = true;

unlock:
	mutex_unlock(&buffer_mtx);
	return result;
}

/**
 * @brief Uninitializes buffer.
 *
 * @return 0 on success, negative error code on error.
 */
int driver_to_buffer_uninitialize(void)
{
	int result;

	mutex_lock(&buffer_mtx);
	if (!buffer_enabled) {
		result = -E_SD_BUFFER_DISABLED;
		goto unlock;
	}

	/* Release occupied buffer */
	if (busy_buffer) {
		result = driver_to_buffer_release();
		/* TODO Maybe release anyway */
		if (result < 0)
			goto unlock;
		busy_buffer = NULL;
	}

	result = swap_buffer_uninit();
	if (result == -E_SB_UNRELEASED_BUFFERS) {
		print_err("Can't uninit buffer! There are busy subbuffers!\n");
		result = -E_SD_BUFFER_ERROR;
	} else if (result < 0) {
		print_err("swap_buffer_uninit error %d\n", result);
		result = -E_SD_BUFFER_ERROR;
	} else {
		result = E_SD_SUCCESS;
		buffer_enabled = false;
	}

	/* Reinit driver_to_buffer vars */
	init_buffers_to_read();
	pages_per_buffer = 0;

unlock:
	mutex_unlock(&buffer_mtx);
	return result;
}

/**
 * @brief Get next buffer to read.
 *
 * @return 0 on success, negative error code on error, E_SD_NO_DATA_TO_READ if
 * there is nothing to be read.
 */
int driver_to_buffer_next_buffer_to_read(void)
{
	int result;

	/* If there is busy_buffer first release it */
	if (busy_buffer) {
		result = driver_to_buffer_release();
		if (result)
			return result;
	}

	/* If there is no buffers to read, return E_SD_NO_DATA_TO_READ.
	 * SHOULD BE POSITIVE, cause there is no real error. */
	if (!something_to_read())
		return E_SD_NO_DATA_TO_READ;

	/* Get next buffer to read */
	result = driver_to_buffer_get();
	if (result < 0) {
		print_err("buffer_to_reads > 0, but there are no buffers to read\n");
		return result;
	}

	/* Decrement buffers_to_read counter */
	dec_buffers_to_read();

	return E_SD_SUCCESS;
}
