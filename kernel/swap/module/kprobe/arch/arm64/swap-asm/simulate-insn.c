/*
 * Copyright (C) Samsung Electronics, 2014
 *
 * Copied from: arch/arm64/kernel/simulate-insn.c
 *
 * Copyright (C) 2013 Linaro Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/ptrace.h>

#include "simulate-insn.h"

#define sign_extend(x, signbit)		\
	((x) | (0 - ((x) & ((long)1 << (signbit)))))

#define bbl_displacement(insn)		\
	sign_extend(((insn) & 0x3ffffff) << 2, 27)

#define bcond_displacement(insn)	\
	sign_extend(((insn >> 5) & 0xfffff) << 2, 21)

#define cbz_displacement(insn)	\
	sign_extend(((insn >> 5) & 0xfffff) << 2, 21)

#define tbz_displacement(insn)	\
	sign_extend(((insn >> 5) & 0x3fff) << 2, 15)

#define ldr_displacement(insn)	\
	sign_extend(((insn >> 5) & 0xfffff) << 2, 21)


unsigned long check_cbz(u32 opcode, struct pt_regs *regs)
{
	int xn = opcode & 0x1f;

	return (opcode & (1 << 31)) ?
	    !(regs->regs[xn]) : !(regs->regs[xn] & 0xffffffff);
}
EXPORT_SYMBOL_GPL(check_cbz);

unsigned long check_cbnz(u32 opcode, struct pt_regs *regs)
{
	int xn = opcode & 0x1f;

	return (opcode & (1 << 31)) ?
	    (regs->regs[xn]) : (regs->regs[xn] & 0xffffffff);
}
EXPORT_SYMBOL_GPL(check_cbnz);

unsigned long check_tbz(u32 opcode, struct pt_regs *regs)
{
	int xn = opcode & 0x1f;
	int bit_pos = ((opcode & (1 << 31)) >> 26) | ((opcode >> 19) & 0x1f);

	return ~((regs->regs[xn] >> bit_pos) & 0x1);
}
EXPORT_SYMBOL_GPL(check_tbz);

unsigned long check_tbnz(u32 opcode, struct pt_regs *regs)
{
	int xn = opcode & 0x1f;
	int bit_pos = ((opcode & (1 << 31)) >> 26) | ((opcode >> 19) & 0x1f);

	return (regs->regs[xn] >> bit_pos) & 0x1;
}
EXPORT_SYMBOL_GPL(check_tbnz);

/*
 * instruction simulate functions
 */
void simulate_none(u32 opcode, long addr, struct pt_regs *regs)
{
}

void simulate_adr_adrp(u32 opcode, long addr, struct pt_regs *regs)
{
	long res, imm, xn, shift;

	xn = opcode & 0x1f;
	shift = (opcode >> 31) ? 12 : 0;	/* check insn ADRP/ADR */
	imm = ((opcode >> 3) & 0xffffc) | ((opcode >> 29) & 0x3);
	res = addr + 8 + (sign_extend(imm, 20) << shift);

	regs->regs[xn] = opcode & 0x80000000 ? res & 0xfffffffffffff000 : res;
	regs->pc += 4;
}
EXPORT_SYMBOL_GPL(simulate_adr_adrp);

void simulate_b_bl(u32 opcode, long addr, struct pt_regs *regs)
{
	int disp = bbl_displacement(opcode);

	/* Link register is x30 */
	if (opcode & (1 << 31))
		regs->regs[30] = addr + 4;

	regs->pc = addr + disp;
}
EXPORT_SYMBOL_GPL(simulate_b_bl);

void simulate_b_cond(u32 opcode, long addr, struct pt_regs *regs)
{
	int disp = bcond_displacement(opcode);

	regs->pc = addr + disp;
}
EXPORT_SYMBOL_GPL(simulate_b_cond);

void simulate_br_blr_ret(u32 opcode, long addr, struct pt_regs *regs)
{
	int xn = (opcode >> 5) & 0x1f;

	/* Link register is x30 */
	if (((opcode >> 21) & 0x3) == 1)
		regs->regs[30] = addr + 4;

	regs->pc = regs->regs[xn];
}
EXPORT_SYMBOL_GPL(simulate_br_blr_ret);

void simulate_cbz_cbnz(u32 opcode, long addr, struct pt_regs *regs)
{
	int disp = cbz_displacement(opcode);

	regs->pc = addr + disp;
}
EXPORT_SYMBOL_GPL(simulate_cbz_cbnz);

void simulate_tbz_tbnz(u32 opcode, long addr, struct pt_regs *regs)
{
	int disp = tbz_displacement(opcode);

	regs->pc = addr + disp;
}
EXPORT_SYMBOL_GPL(simulate_tbz_tbnz);

void simulate_ldr_literal(u32 opcode, long addr, struct pt_regs *regs)
{
	u64 *load_addr;
	int xn = opcode & 0x1f;
	int disp = ldr_displacement(opcode);

	load_addr = (u64 *) (addr + disp);

	if (opcode & (1 << 30))	/* x0-x31 */
		regs->regs[xn] = *load_addr;
	else			/* w0-w31 */
		*(u32 *) (&regs->regs[xn]) = (*(u32 *) (load_addr));
}
EXPORT_SYMBOL_GPL(simulate_ldr_literal);

void simulate_ldrsw_literal(u32 opcode, long addr, struct pt_regs *regs)
{
	u64 *load_addr;
	long data;
	int xn = opcode & 0x1f;
	int disp = ldr_displacement(opcode);

	load_addr = (u64 *) (addr + disp);
	data = *load_addr;

	regs->regs[xn] = sign_extend(data, 63);
}
EXPORT_SYMBOL_GPL(simulate_ldrsw_literal);
