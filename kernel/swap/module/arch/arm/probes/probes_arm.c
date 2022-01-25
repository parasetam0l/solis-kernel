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


#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ptrace.h>	/* nedded for printk.h */
#include <linux/linkage.h>	/* needed for printk.h */
#include <linux/printk.h>
#include "probes.h"
#include "tramps_arm.h"
#include "decode_arm_old.h"


#define sign_extend(x, signbit) ((x) | (0 - ((x) & (1 << (signbit)))))
#define branch_displacement(insn) sign_extend(((insn) & 0xffffff) << 2, 25)


static u32 get_addr_b(u32 insn, u32 addr)
{
	/* real position less then PC by 8 */
	return ((s32)addr + 8 + branch_displacement(insn));
}

static int prep_pc_dep_insn_execbuf(u32 *insns, u32 insn, int uregs)
{
	int i;

	if (uregs & 0x10) {
		int reg_mask = 0x1;
		/* search in reg list */
		for (i = 0; i < 13; i++, reg_mask <<= 1) {
			if (!(insn & reg_mask))
				break;
		}
	} else {
		for (i = 0; i < 13; i++) {
			if ((uregs & 0x1) && (ARM_INSN_REG_RN(insn) == i))
				continue;
			if ((uregs & 0x2) && (ARM_INSN_REG_RD(insn) == i))
				continue;
			if ((uregs & 0x4) && (ARM_INSN_REG_RS(insn) == i))
				continue;
			if ((uregs & 0x8) && (ARM_INSN_REG_RM(insn) == i))
				continue;
			break;
		}
	}

	if (i == 13) {
		pr_err("there are no free register %x in insn %x!",
		       uregs, insn);
		return -EINVAL;
	}

	/* set register to save */
	ARM_INSN_REG_SET_RD(insns[0], i);
	/* set register to load address to */
	ARM_INSN_REG_SET_RD(insns[1], i);
	/* set instruction to execute and patch it */
	if (uregs & 0x10) {
		ARM_INSN_REG_CLEAR_MR(insn, 15);
		ARM_INSN_REG_SET_MR(insn, i);
	} else {
		if ((uregs & 0x1) && (ARM_INSN_REG_RN(insn) == 15))
			ARM_INSN_REG_SET_RN(insn, i);
		if ((uregs & 0x2) && (ARM_INSN_REG_RD(insn) == 15))
			ARM_INSN_REG_SET_RD(insn, i);
		if ((uregs & 0x4) && (ARM_INSN_REG_RS(insn) == 15))
			ARM_INSN_REG_SET_RS(insn, i);
		if ((uregs & 0x8) && (ARM_INSN_REG_RM(insn) == 15))
			ARM_INSN_REG_SET_RM(insn, i);
	}

	insns[PROBES_TRAMP_INSN_IDX] = insn;
	/* set register to restore */
	ARM_INSN_REG_SET_RD(insns[3], i);

	return 0;
}

static int arch_check_insn_arm(u32 insn)
{
	/* check instructions that can change PC by nature */
	if (
	 /* ARM_INSN_MATCH(UNDEF, insn) || */
	    ARM_INSN_MATCH(AUNDEF, insn) ||
	    ARM_INSN_MATCH(SWI, insn) ||
	    ARM_INSN_MATCH(BREAK, insn) ||
	    ARM_INSN_MATCH(BXJ, insn)) {
		goto bad_insn;
#ifndef CONFIG_CPU_V7
	/* check instructions that can write result to PC */
	} else if ((ARM_INSN_MATCH(DPIS, insn) ||
		    ARM_INSN_MATCH(DPRS, insn) ||
		    ARM_INSN_MATCH(DPI, insn) ||
		    ARM_INSN_MATCH(LIO, insn) ||
		    ARM_INSN_MATCH(LRO, insn)) &&
		   (ARM_INSN_REG_RD(insn) == 15)) {
		goto bad_insn;
#endif /* CONFIG_CPU_V7 */
	/* check special instruction loads store multiple registers */
	} else if ((ARM_INSN_MATCH(LM, insn) || ARM_INSN_MATCH(SM, insn)) &&
			/* store PC or load to PC */
		   (ARM_INSN_REG_MR(insn, 15) ||
			 /* store/load with PC update */
		    ((ARM_INSN_REG_RN(insn) == 15) && (insn & 0x200000)))) {
		goto bad_insn;
	}

	return 0;

bad_insn:
	return -EFAULT;
}

static int make_branch_tarmpoline(u32 addr, u32 insn, u32 *tramp)
{
	int ok = 0;

	/* B */
	if (ARM_INSN_MATCH(B, insn) &&
	    !ARM_INSN_MATCH(BLX1, insn)) {
		/* B check can be false positive on BLX1 instruction */
		memcpy(tramp, b_cond_insn_execbuf, PROBES_TRAMP_LEN);
		tramp[PROBES_TRAMP_RET_BREAK_IDX] = RET_BREAK_ARM;
		tramp[0] |= insn & 0xf0000000;
		tramp[6] = get_addr_b(insn, addr);
		tramp[7] = addr + 4;
		ok = 1;
	/* BX, BLX (Rm) */
	} else if (ARM_INSN_MATCH(BX, insn) ||
		   ARM_INSN_MATCH(BLX2, insn)) {
		memcpy(tramp, b_r_insn_execbuf, PROBES_TRAMP_LEN);
		tramp[0] = insn;
		tramp[PROBES_TRAMP_RET_BREAK_IDX] = RET_BREAK_ARM;
		tramp[7] = addr + 4;
		ok = 1;
	/* BL, BLX (Off) */
	} else if (ARM_INSN_MATCH(BLX1, insn)) {
		memcpy(tramp, blx_off_insn_execbuf, PROBES_TRAMP_LEN);
		tramp[0] |= 0xe0000000;
		tramp[1] |= 0xe0000000;
		tramp[PROBES_TRAMP_RET_BREAK_IDX] = RET_BREAK_ARM;
		tramp[6] = get_addr_b(insn, addr) +
			   2 * (insn & 01000000) + 1; /* jump to thumb */
		tramp[7] = addr + 4;
		ok = 1;
	/* BL */
	} else if (ARM_INSN_MATCH(BL, insn)) {
		memcpy(tramp, blx_off_insn_execbuf, PROBES_TRAMP_LEN);
		tramp[0] |= insn & 0xf0000000;
		tramp[1] |= insn & 0xf0000000;
		tramp[PROBES_TRAMP_RET_BREAK_IDX] = RET_BREAK_ARM;
		tramp[6] = get_addr_b(insn, addr);
		tramp[7] = addr + 4;
		ok = 1;
	}

	return ok;
}

/**
 * @brief Creates ARM trampoline.
 *
 * @param addr Probe address.
 * @param insn Instuction at this address.
 * @param tramp Pointer to memory for trampoline.
 * @return 0 on success, error code on error.
 */
int make_trampoline_arm(u32 addr, u32 insn, u32 *tramp)
{
	int ret, uregs, pc_dep;

	if (addr & 0x03) {
		pr_err("Error in %s at %d: attempt to register probe "
		       "at an unaligned address\n", __FILE__, __LINE__);
		return -EINVAL;
	}

	ret = arch_check_insn_arm(insn);
	if (ret)
		return ret;

	if (make_branch_tarmpoline(addr, insn, tramp))
		return 0;

	uregs = pc_dep = 0;
	/* Rm */
	if (ARM_INSN_MATCH(CLZ, insn)) {
		uregs = 0xa;
		if (ARM_INSN_REG_RM(insn) == 15)
			pc_dep = 1;
	/* Rn, Rm ,Rd */
	} else if (ARM_INSN_MATCH(DPIS, insn) || ARM_INSN_MATCH(LRO, insn) ||
	    ARM_INSN_MATCH(SRO, insn)) {
		uregs = 0xb;
		if ((ARM_INSN_REG_RN(insn) == 15) ||
		    (ARM_INSN_REG_RM(insn) == 15) ||
		    (ARM_INSN_MATCH(SRO, insn) &&
		     (ARM_INSN_REG_RD(insn) == 15))) {
			pc_dep = 1;
		}
	/* Rn ,Rd */
	} else if (ARM_INSN_MATCH(DPI, insn) || ARM_INSN_MATCH(LIO, insn) ||
		   ARM_INSN_MATCH(SIO, insn)) {
		uregs = 0x3;
		if ((ARM_INSN_REG_RN(insn) == 15) ||
		    (ARM_INSN_MATCH(SIO, insn) &&
		    (ARM_INSN_REG_RD(insn) == 15))) {
			pc_dep = 1;
		}
	/* Rn, Rm, Rs */
	} else if (ARM_INSN_MATCH(DPRS, insn)) {
		uregs = 0xd;
		if ((ARM_INSN_REG_RN(insn) == 15) ||
		    (ARM_INSN_REG_RM(insn) == 15) ||
		    (ARM_INSN_REG_RS(insn) == 15)) {
			pc_dep = 1;
		}
	/* register list */
	} else if (ARM_INSN_MATCH(SM, insn)) {
		uregs = 0x10;
		if (ARM_INSN_REG_MR(insn, 15))
			pc_dep = 1;
	}

	/* check instructions that can write result to SP and uses PC */
	if (pc_dep && (ARM_INSN_REG_RD(insn) == 13)) {
		pr_err("Error in %s at %d: instruction check failed (arm)\n",
		       __FILE__, __LINE__);
		return -EFAULT;
	}

	if (unlikely(uregs && pc_dep)) {
		memcpy(tramp, pc_dep_insn_execbuf, PROBES_TRAMP_LEN);
		if (prep_pc_dep_insn_execbuf(tramp, insn, uregs) != 0) {
			pr_err("Error in %s at %d: failed "
			       "to prepare exec buffer for insn %x!",
			       __FILE__, __LINE__, insn);
			return -EINVAL;
		}

		tramp[6] = addr + 8;
	} else {
		memcpy(tramp, gen_insn_execbuf, PROBES_TRAMP_LEN);
		tramp[PROBES_TRAMP_INSN_IDX] = insn;
	}

	/* TODO: remove for probe */
	tramp[PROBES_TRAMP_RET_BREAK_IDX] = RET_BREAK_ARM;
	tramp[7] = addr + 4;

	return 0;
}

int noret_arm(u32 opcode)
{
	return !!(ARM_INSN_MATCH(BL, opcode) ||
		  ARM_INSN_MATCH(BLX1, opcode) ||
		  ARM_INSN_MATCH(BLX2, opcode));
}
