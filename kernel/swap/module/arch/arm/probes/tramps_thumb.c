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
".thumb\n"

".global gen_insn_execbuf_thumb\n"
"gen_insn_execbuf_thumb:\n"
	"nop\n"
	"nop\n"
	"nop\n"			/* original instruction */
	"nop\n"			/* original instruction */
	"nop\n"
	"nop\n"
	"nop\n"
	"sub	sp, sp, #8\n"
	"str	r0, [sp, #0]\n"
	"ldr	r0, [pc, #12]\n"
	"str	r0, [sp, #4]\n"
	"nop\n"
	"pop	{r0, pc}\n"	/* ssbreak */
	"nop\n"			/* retbreak */
	"nop\n"
	"nop\n"
	"nop\n"			/* stored PC-4(next insn addr) hi */
	"nop\n"			/* stored PC-4(next insn addr) lo */

	"nop\n"

".global pc_dep_insn_execbuf_thumb\n"
".align 4\n"
"pc_dep_insn_execbuf_thumb:\n"
	"push	{r6, r7}\n"
	"ldr	r6, i1\n"
	"mov	r7, sp\n"
	"mov	sp, r6\n"
	"nop\n"			/* PC -> SP */
	"nop\n"			/* PC -> SP */
	"mov	sp, r7\n"
	"pop	{r6, r7}\n"
	"push	{r0, r1}\n"
	"ldr	r0, i2\n"
	"nop\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0, pc}\n"	/* ssbreak */
	"nop\n"			/* retbreak */
"i1:\n"
	"nop\n"			/* stored PC hi */
	"nop\n"			/* stored PC lo */
"i2:\n"
	"nop\n"			/* stored PC-4(next insn addr) hi */
	"nop\n"			/* stored PC-4(next insn addr) lo */

".global b_r_insn_execbuf_thumb\n"
".align 4\n"
"b_r_insn_execbuf_thumb:\n"
	"nop\n"
	"nop\n"
	"nop\n"
	"nop\n"
	"nop\n"			/* bx,blx (Rm) */
	"nop\n"
	"push	{r0,r1}\n"
	"ldr	r0, np\n"
	"nop\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0,pc}\n"
	"nop\n"
	"nop\n"			/* ssbreak */
	"nop\n"			/* retbreak */
	"nop\n"
	"nop\n"
"np:\n"
	"nop\n"			/* stored PC-4(next insn addr) hi */
	"nop\n"			/* stored PC-4(next insn addr) lo */

".global b_off_insn_execbuf_thumb\n"
".align 4\n"
"b_off_insn_execbuf_thumb:\n"
	"push	{r0,r1}\n"
	"ldr	r0, bd\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0, pc}\n"
	"nop\n"
	"nop\n"
	"push	{r0,r1}\n"
	"ldr	r0, np2\n"
	"nop\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0,pc}\n"
	"nop\n"
	"nop\n"			/* ssbreak */
	"nop\n"			/* retbreak */
"bd:\n"
	"nop\n"			/* branch displacement hi */
	"nop\n"			/* branch displacement lo */
"np2:\n"
	"nop\n"			/* stored PC-4(next insn addr) hi */
	"nop\n"			/* stored PC-4(next insn addr) lo */

".global b_cond_insn_execbuf_thumb\n"
".align 4\n"
"b_cond_insn_execbuf_thumb:\n"
	"beq	condway\n"
	"push	{r0,r1}\n"
	"ldr	r0, np4\n"
	"nop\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0,pc}\n"
"condway:\n"
	"push	{r0,r1}\n"
	"ldr	r0, bd4\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0,pc}\n"
	"nop\n"
	"nop\n"
	"nop\n"			/* ssbreak */
	"nop\n"			/* retbreak */
"bd4:\n"
	"nop\n"			/* branch displacement hi */
	"nop\n"			/* branch displacement lo */
"np4:\n"
	"nop\n"			/* stored PC-4(next insn addr) hi */
	"nop\n"			/* stored PC-4(next insn addr) lo */

".global cbz_insn_execbuf_thumb\n"
".align 4\n"
"cbz_insn_execbuf_thumb:\n"
	"nop\n"			/* cbz */
	"push	{r0,r1}\n"
	"ldr	r0, np5\n"
	"nop\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0,pc}\n"
	"push	{r0,r1}\n"
	"ldr	r0, bd5\n"
	"str	r0, [sp, #4]\n"
	"pop	{r0,pc}\n"
	"nop\n"
	"nop\n"
	"nop\n"			/* ssbreak */
	"nop\n"			/* retbreak */
"bd5:\n"
	"nop\n"			/* branch displacement hi */
	"nop\n"			/* branch displacement lo */
"np5:\n"
	"nop\n"			/* stored PC-4(next insn addr) hi */
	"nop\n"			/* stored PC-4(next insn addr) lo */
);
