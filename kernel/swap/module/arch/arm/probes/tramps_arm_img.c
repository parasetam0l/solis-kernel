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


#include "tramps_arm.h"


/*
 * These arrays generated from tramps_arm.c
 * using 32 bit compiler:
 *   $ gcc tramps_arm.c -c -o tramps_arm.o
 *   $ objdump -d tramps_arm.o
 */

u32 gen_insn_execbuf[] = {
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe59ff004,	//   ldr	pc, [pc, #4]
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
};

u32 pc_dep_insn_execbuf[] = {
	0xe50d0004,	//   str	r0, [sp, #-4]
	0xe59f000c,	//   ldr	r0, [pc, #12]
	0xe320f000,	//   nop
	0xe51d0004,	//   ldr	r0, [sp, #-4]
	0xe59ff004,	//   ldr	pc, [pc, #4]
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
};

u32 b_r_insn_execbuf[] = {
	0xe320f000,	//   nop
	0xe59ff010,	//   ldr	pc, [pc, #16]
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
};

u32 b_cond_insn_execbuf[] = {
	0x0a000000,	//   beq	68 <condway>
	0xe59ff010,	//   ldr	pc, [pc, #16]
	0xe59ff008,	//   ldr	pc, [pc, #8]
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
};

u32 blx_off_insn_execbuf[] = {
	0x059fe010,	//   ldreq	lr, [pc, #16]
	0x012fff3e,	//   blxeq	lr
	0xe59ff00c,	//   ldr	pc, [pc, #12]
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
	0xe320f000,	//   nop
};
