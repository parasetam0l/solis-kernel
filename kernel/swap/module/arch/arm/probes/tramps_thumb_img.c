/**
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial
 * implementation; Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for
 * separating core and arch parts
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
 * Copyright (C) Samsung Electronics, 2006-2016
 *
 */


#include "tramps_thumb.h"


/*
 * These arrays generated from tramps_thumb.c
 * using 32 bit compiler:
 *   $ gcc tramps_thumb.c -c -o tramps_thumb.o
 *   $ objdump -d tramps_thumb.o
 */

u16 gen_insn_execbuf_thumb[] = {
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xb082,		//   sub	sp, #8
	0x9000,		//   str	r0, [sp, #0]
	0x4803,		//   ldr	r0, [pc, #12]
	0x9001,		//   str	r0, [sp, #4]
	0xbf00,		//   nop
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
};

u16 pc_dep_insn_execbuf_thumb[] = {
	0xb4c0,		//   push	{r6, r7}
	0x4e06,		//   ldr	r6, [pc, #24]
	0x466f,		//   mov	r7, sp
	0x46b5,		//   mov	sp, r6
	0xbf00,		//   nop
	0xbf00,		//   nop
	0x46bd,		//   mov	sp, r7
	0xbcc0,		//   pop	{r6, r7}
	0xb403,		//   push	{r0, r1}
	0x4803,		//   ldr	r0, [pc, #12]
	0xbf00,		//   nop
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
};

u16 b_r_insn_execbuf_thumb[] = {
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xb403,		//   push	{r0, r1}
	0x4804,		//   ldr	r0, [pc, #16]
	0xbf00,		//   nop
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
};

u16 b_off_insn_execbuf_thumb[] = {
	0xb403,		//   push	{r0, r1}
	0x4806,		//   ldr	r0, [pc, #24]
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xb403,		//   push	{r0, r1}
	0x4804,		//   ldr	r0, [pc, #16]
	0xbf00,		//   nop
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
};

u16 b_cond_insn_execbuf_thumb[] = {
	0xf000, 0x8005, //   beq.w	ce <condway>
	0xb403,		//   push	{r0, r1}
	0x4807,		//   ldr	r0, [pc, #28]
	0xbf00,		//   nop
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xb403,		//   push	{r0, r1}
	0xf8df, 0x000c, //   ldr.w	r0, [pc, #12]
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
};

u16 cbz_insn_execbuf_thumb[] = {
	0xbf00,		//   nop
	0xb403,		//   push	{r0, r1}
	0x4806,		//   ldr	r0, [pc, #24]
	0xbf00,		//   nop
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xb403,		//   push	{r0, r1}
	0x4803,		//   ldr	r0, [pc, #12]
	0x9001,		//   str	r0, [sp, #4]
	0xbd01,		//   pop	{r0, pc}
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
	0xbf00,		//   nop
};
