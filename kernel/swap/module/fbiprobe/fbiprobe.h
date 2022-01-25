/*
 * @file fbi_probe/fbi_probe.h
 *
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014 Alexander Aksenov : FBI implement
 * 2014 Vitaliy Cherepanov: FBI implement, portage
 *
 * @section DESCRIPTION
 *
 * Function body instrumentation.
 *
 */

#ifndef __FBI_PROBE_H__
#define __FBI_PROBE_H__

#include <linux/types.h>

/* FBI step */
struct fbi_step {
	uint8_t ptr_order;         /* Specifies what is located on the address:
				    * ptr_order = 0  -  variable
				    * ptr_order = 1  -  pointer to variable
				    * ptr_order = 2  -  pointer to pointer
				    * etc. */

	uint64_t data_offset;
} __packed;

/* FBI var data */
struct fbi_var_data {
	/* Variable position is evaluated by the following rule:
	 * var_position = *(pointer_to_register) - reg_offset
	 * It is expected that the offset is not null only when we're taking
	 * var value from stack.
	 */
	uint64_t var_id;           /* Variable identifier
				    * Used to specify var */
	uint64_t reg_offset;       /* Offset relative to the registers value
				    * address, specified with reg_n */
	uint8_t reg_n;             /* Register number. Hope times of cpu
				    * with more than 2 million ones are very
				    * far from us */
	uint32_t data_size;        /* Data size to be read */

	uint8_t steps_count;	   /* Count of steps to extract variable
				    * value */
	struct fbi_step *steps;    /* extract steps */
};

/* FBI info */
struct fbi_info {
	uint8_t var_count;
	struct fbi_var_data *vars;
};

#endif /* __FBI_PROBE_H__ */
