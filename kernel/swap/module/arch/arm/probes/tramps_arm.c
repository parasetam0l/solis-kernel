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


__asm(
".text\n"
".arm\n"
".global gen_insn_execbuf\n"
"gen_insn_execbuf:\n"
"	nop\n"
"	nop\n"
"	nop\n"			/* original instruction */
"	nop\n"
"	ldr	pc, [pc, #4]\n"	/* ssbreak */
"	nop\n"			/* retbreak */
"	nop\n"
"	nop\n"			/* stored PC-4(next insn addr) */

".global pc_dep_insn_execbuf\n"
"pc_dep_insn_execbuf:\n"
"	str	r0, [sp, #-4]\n"
"	ldr	r0, [pc, #12]\n"
"	nop\n"			/* instruction with replaced PC */
"	ldr	r0, [sp, #-4]\n"
"	ldr	pc, [pc, #4]\n"	/* ssbreak */
"	nop\n"			/* retbreak */
"	nop\n"			/* stored PC */
"	nop\n"			/* stored PC-4 (next insn addr) */

".global b_r_insn_execbuf\n"
"b_r_insn_execbuf:\n"
"	nop\n"			/* bx, blx (Rm) */
"	ldr	pc, np1\n"
"	nop\n"
"	nop\n"
"	nop\n"
"	nop\n"			/* retbreak */
"	nop\n"
"np1:\n"
"	nop\n"			/* stored PC-4 (next insn addr) */

".global b_cond_insn_execbuf\n"
"b_cond_insn_execbuf:\n"
"	beq	condway\n"
"	ldr	pc, np2\n"
"condway:\n"
"	ldr	pc, bd2\n"
"	nop\n"
"	nop\n"
"	nop\n"			/* retbreak */
"bd2:\n"
"	nop\n"			/* branch displacement */
"np2:\n"
"	nop\n"			/* stored PC-4 (next insn addr) */

".global blx_off_insn_execbuf\n"
"blx_off_insn_execbuf:\n"
"	ldreq	lr, bd3\n"
"	blxeq	lr\n"
"	ldr	pc, np3\n"
"	nop\n"
"	nop\n"
"	nop\n"			/* retbreak */
"bd3:\n"
"	nop\n"			/* branch displacement */
"np3:\n"
"	nop\n"			/* stored PC-4 (next insn addr) */
);
