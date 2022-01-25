/*
 * @file fbiprobe/fbi_probe.h
 *
 * @author Aleksandr Aksenov <a.aksenov@samsung.com>
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014 Alexander Aksenov : FBI implement
 * 2014 Vitaliy Cherepanov: FBI portage
 *
 * @section DESCRIPTION
 *
 * Function body instrumetation
 *
 */

#ifndef __REGS_H__
#define __REGS_H__

#include <linux/ptrace.h>

#include "fbi_probe_module.h"
/* This function is used to compare register number and its name on x86 arch.
 * For ARM it is dumb.
 * List of registers and their nums on x86:
 * ax       0
 * bx       1
 * cx       2
 * dx       3
 * si       4
 * di       5
 * bp       6
 * sp       7
 */

static inline unsigned long *get_ptr_by_num(struct pt_regs *regs,
					    unsigned char reg_num)
{
	unsigned long *reg = NULL;
	/* FIXME: bad way to use "sizeof(long) " */
	if (reg_num < sizeof(struct pt_regs) / sizeof(long)) {
		reg = (unsigned long *)regs;
		reg =  &reg[reg_num];
	}

	return reg;
}

#endif /* __REGS_H__ */
