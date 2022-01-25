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
 * Copyright (C) Samsung Electronics, 2016
 *
 * 2016         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#ifndef _SWAP_ASM_ARM_PROBES_H
#define _SWAP_ASM_ARM_PROBES_H


#define BREAK_ARM			0xffffdeff
#define BREAK_THUMB			(BREAK_ARM & 0xffff)
#define RET_BREAK_ARM			BREAK_ARM
#define RET_BREAK_THUMB			BREAK_THUMB

#define PROBES_TRAMP_LEN		(9 * 4)
#define PROBES_TRAMP_INSN_IDX		2
#define PROBES_TRAMP_RET_BREAK_IDX	5


#endif /* _SWAP_ASM_ARM_PROBES_H */
