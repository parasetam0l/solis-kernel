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
#include "tramps_thumb.h"
#include "decode_thumb.h"
#include "decode_thumb_old.h"


static inline s32 branch_t16_dest(u32 insn, unsigned int insn_addr)
{
	s32 offset = insn & 0x3ff;
	offset -= insn & 0x400;
	return insn_addr + 4 + offset * 2;
}

static inline s32 branch_cond_t16_dest(u32 insn, unsigned int insn_addr)
{
	s32 offset = insn & 0x7f;
	offset -= insn & 0x80;
	return insn_addr + 4 + offset * 2;
}

static inline s32 branch_t32_dest(u32 insn, unsigned int insn_addr)
{
	unsigned int poff = insn & 0x3ff;
	unsigned int offset = (insn & 0x07fe0000) >> 17;

	poff -= (insn & 0x400);

	if (insn & (1 << 12))
		return insn_addr + 4 + (poff << 12) + offset * 4;

	return (insn_addr + 4 + (poff << 12) + offset * 4) & ~3;
}

static inline s32 cbz_t16_dest(u32 insn, unsigned int insn_addr)
{
	unsigned int i = (insn & 0x200) >> 3;
	unsigned int offset = (insn & 0xf8) >> 2;
	return insn_addr + 4 + i + offset;
}

/* is instruction Thumb2 and NOT a branch, etc... */
static int is_thumb2(u32 insn)
{
	return ((insn & 0xf800) == 0xe800 ||
		(insn & 0xf800) == 0xf000 ||
		(insn & 0xf800) == 0xf800);
}

static int prep_pc_dep_insn_execbuf_thumb(u32 *insns, u32 insn, int uregs)
{
	unsigned char mreg = 0;
	unsigned char reg = 0;

	if (THUMB_INSN_MATCH(APC, insn) ||
	    THUMB_INSN_MATCH(LRO3, insn)) {
		reg = ((insn & 0xffff) & uregs) >> 8;
	} else if (THUMB_INSN_MATCH(MOV3, insn)) {
		if (((((unsigned char)insn) & 0xff) >> 3) == 15)
			reg = (insn & 0xffff) & uregs;
		else
			return 0;
	} else if (THUMB2_INSN_MATCH(ADR, insn)) {
		reg = ((insn >> 16) & uregs) >> 8;
		if (reg == 15)
			return 0;
	} else if (THUMB2_INSN_MATCH(LDRW, insn) ||
		   THUMB2_INSN_MATCH(LDRW1, insn) ||
		   THUMB2_INSN_MATCH(LDRHW, insn) ||
		   THUMB2_INSN_MATCH(LDRHW1, insn) ||
		   THUMB2_INSN_MATCH(LDRWL, insn)) {
		reg = ((insn >> 16) & uregs) >> 12;
		if (reg == 15)
			return 0;
	/*
	 * LDRB.W PC, [PC, #immed] => PLD [PC, #immed], so Rt == PC is skipped
	 */
	} else if (THUMB2_INSN_MATCH(LDRBW, insn) ||
		   THUMB2_INSN_MATCH(LDRBW1, insn) ||
		   THUMB2_INSN_MATCH(LDREX, insn)) {
		reg = ((insn >> 16) & uregs) >> 12;
	} else if (THUMB2_INSN_MATCH(DP, insn)) {
		reg = ((insn >> 16) & uregs) >> 12;
		if (reg == 15)
			return 0;
	} else if (THUMB2_INSN_MATCH(RSBW, insn)) {
		reg = ((insn >> 12) & uregs) >> 8;
		if (reg == 15)
			return 0;
	} else if (THUMB2_INSN_MATCH(RORW, insn)) {
		reg = ((insn >> 12) & uregs) >> 8;
		if (reg == 15)
			return 0;
	} else if (THUMB2_INSN_MATCH(ROR, insn) ||
		   THUMB2_INSN_MATCH(LSLW1, insn) ||
		   THUMB2_INSN_MATCH(LSLW2, insn) ||
		   THUMB2_INSN_MATCH(LSRW1, insn) ||
		   THUMB2_INSN_MATCH(LSRW2, insn)) {
		reg = ((insn >> 12) & uregs) >> 8;
		if (reg == 15)
			return 0;
	} else if (THUMB2_INSN_MATCH(TEQ1, insn) ||
		   THUMB2_INSN_MATCH(TST1, insn)) {
		reg = 15;
	} else if (THUMB2_INSN_MATCH(TEQ2, insn) ||
		   THUMB2_INSN_MATCH(TST2, insn)) {
		reg = THUMB2_INSN_REG_RM(insn);
	}

	if ((THUMB2_INSN_MATCH(STRW, insn) ||
	     THUMB2_INSN_MATCH(STRBW, insn) ||
	     THUMB2_INSN_MATCH(STRD, insn) ||
	     THUMB2_INSN_MATCH(STRHT, insn) ||
	     THUMB2_INSN_MATCH(STRT, insn) ||
	     THUMB2_INSN_MATCH(STRHW1, insn) ||
	     THUMB2_INSN_MATCH(STRHW, insn)) &&
	    THUMB2_INSN_REG_RT(insn) == 15) {
		reg = THUMB2_INSN_REG_RT(insn);
	}

	if (reg == 6 || reg == 7) {
		*((u16 *)insns + 0) =
			(*((u16 *)insns + 0) & 0x00ff) |
			((1 << mreg) | (1 << (mreg + 1)));
		*((u16 *)insns + 1) =
			(*((u16 *)insns + 1) & 0xf8ff) | (mreg << 8);
		*((u16 *)insns + 2) =
			(*((u16 *)insns + 2) & 0xfff8) | (mreg + 1);
		*((u16 *)insns + 3) =
			(*((u16 *)insns + 3) & 0xffc7) | (mreg << 3);
		*((u16 *)insns + 7) =
			(*((u16 *)insns + 7) & 0xf8ff) | (mreg << 8);
		*((u16 *)insns + 8) =
			(*((u16 *)insns + 8) & 0xffc7) | (mreg << 3);
		*((u16 *)insns + 9) =
			(*((u16 *)insns + 9) & 0xffc7) | ((mreg + 1) << 3);
		*((u16 *)insns + 10) =
			(*((u16 *)insns + 10) & 0x00ff) |
			((1 << mreg) | (1 << (mreg + 1)));
	}

	if (THUMB_INSN_MATCH(APC, insn)) {
		/* ADD Rd, PC, #immed_8*4 -> ADD Rd, SP, #immed_8*4 */
		*((u16 *)insns + 4) = ((insn & 0xffff) | 0x800);
	} else if (THUMB_INSN_MATCH(LRO3, insn)) {
		/* LDR Rd, [PC, #immed_8*4] ->
		 * LDR Rd, [SP, #immed_8*4] */
		*((u16 *)insns + 4) = ((insn & 0xffff) + 0x5000);
	} else if (THUMB_INSN_MATCH(MOV3, insn)) {
		/* MOV Rd, PC -> MOV Rd, SP */
		*((u16 *)insns + 4) = ((insn & 0xffff) ^ 0x10);
	} else if (THUMB2_INSN_MATCH(ADR, insn)) {
		/* ADDW Rd,PC,#imm -> ADDW Rd,SP,#imm */
		insns[2] = (insn & 0xfffffff0) | 0x0d;
	} else if (THUMB2_INSN_MATCH(LDRW, insn) ||
		   THUMB2_INSN_MATCH(LDRBW, insn) ||
		   THUMB2_INSN_MATCH(LDRHW, insn)) {
		/* LDR.W Rt, [PC, #-<imm_12>] ->
		 * LDR.W Rt, [SP, #-<imm_8>]
		 * !!!!!!!!!!!!!!!!!!!!!!!!
		 * !!! imm_12 vs. imm_8 !!!
		 * !!!!!!!!!!!!!!!!!!!!!!!! */
		insns[2] = (insn & 0xf0fffff0) | 0x0c00000d;
	} else if (THUMB2_INSN_MATCH(LDRW1, insn) ||
		   THUMB2_INSN_MATCH(LDRBW1, insn) ||
		   THUMB2_INSN_MATCH(LDRHW1, insn) ||
		   THUMB2_INSN_MATCH(LDRD, insn) ||
		   THUMB2_INSN_MATCH(LDRD1, insn) ||
		   THUMB2_INSN_MATCH(LDREX, insn)) {
		/* LDRx.W Rt, [PC, #+<imm_12>] ->
		 * LDRx.W Rt, [SP, #+<imm_12>]
		 (+/-imm_8 for LDRD Rt, Rt2, [PC, #<imm_8>] */
		insns[2] = (insn & 0xfffffff0) | 0xd;
	} else if (THUMB2_INSN_MATCH(MUL, insn)) {
		/* MUL Rd, Rn, SP */
		insns[2] = (insn & 0xfff0ffff) | 0x000d0000;
	} else if (THUMB2_INSN_MATCH(DP, insn)) {
		if (THUMB2_INSN_REG_RM(insn) == 15)
			/* DP Rd, Rn, PC */
			insns[2] = (insn & 0xfff0ffff) | 0x000d0000;
		else if (THUMB2_INSN_REG_RN(insn) == 15)
			/* DP Rd, PC, Rm */
			insns[2] = (insn & 0xfffffff0) | 0xd;
	} else if (THUMB2_INSN_MATCH(LDRWL, insn)) {
		/* LDRx.W Rt, [PC, #<imm_12>] ->
		 * LDRx.W Rt, [SP, #+<imm_12>]
		 * (+/-imm_8 for LDRD Rt, Rt2, [PC, #<imm_8>] */
		insns[2] = (insn & 0xfffffff0) | 0xd;
	} else if (THUMB2_INSN_MATCH(RSBW, insn)) {
		/*  RSB{S}.W Rd, PC, #<const> -> RSB{S}.W Rd, SP, #<const> */
		insns[2] = (insn & 0xfffffff0) | 0xd;
	} else if (THUMB2_INSN_MATCH(RORW, insn) ||
		   THUMB2_INSN_MATCH(LSLW1, insn) ||
		   THUMB2_INSN_MATCH(LSRW1, insn)) {
		if ((THUMB2_INSN_REG_RM(insn) == 15) &&
		    (THUMB2_INSN_REG_RN(insn) == 15))
			/*  ROR.W Rd, PC, PC */
			insns[2] = (insn & 0xfffdfffd);
		else if (THUMB2_INSN_REG_RM(insn) == 15)
			/*  ROR.W Rd, Rn, PC */
			insns[2] = (insn & 0xfff0ffff) | 0xd0000;
		else if (THUMB2_INSN_REG_RN(insn) == 15)
			/*  ROR.W Rd, PC, Rm */
			insns[2] = (insn & 0xfffffff0) | 0xd;
	} else if (THUMB2_INSN_MATCH(ROR, insn) ||
		   THUMB2_INSN_MATCH(LSLW2, insn) ||
		   THUMB2_INSN_MATCH(LSRW2, insn)) {
		/*  ROR{S} Rd, PC, #<const> -> ROR{S} Rd, SP, #<const> */
		insns[2] = (insn & 0xfff0ffff) | 0xd0000;
	}

	if (THUMB2_INSN_MATCH(STRW, insn) ||
	    THUMB2_INSN_MATCH(STRBW, insn)) {
		/*  STRx.W Rt, [Rn, SP] */
		insns[2] = (insn & 0xfff0ffff) | 0x000d0000;
	} else if (THUMB2_INSN_MATCH(STRD, insn) ||
		   THUMB2_INSN_MATCH(STRHT, insn) ||
		   THUMB2_INSN_MATCH(STRT, insn) ||
		   THUMB2_INSN_MATCH(STRHW1, insn)) {
		if (THUMB2_INSN_REG_RN(insn) == 15)
			/*  STRD/T/HT{.W} Rt, [SP, ...] */
			insns[2] = (insn & 0xfffffff0) | 0xd;
		else
			insns[2] = insn;
	} else if (THUMB2_INSN_MATCH(STRHW, insn) &&
		   (THUMB2_INSN_REG_RN(insn) == 15)) {
		if (THUMB2_INSN_REG_RN(insn) == 15)
			/*  STRH.W Rt, [SP, #-<imm_8>] */
			insns[2] = (insn & 0xf0fffff0) | 0x0c00000d;
		else
			insns[2] = insn;
	}

	/*  STRx PC, xxx */
	if ((reg == 15) && (THUMB2_INSN_MATCH(STRW, insn)   ||
			    THUMB2_INSN_MATCH(STRBW, insn)  ||
			    THUMB2_INSN_MATCH(STRD, insn)   ||
			    THUMB2_INSN_MATCH(STRHT, insn)  ||
			    THUMB2_INSN_MATCH(STRT, insn)   ||
			    THUMB2_INSN_MATCH(STRHW1, insn) ||
			    THUMB2_INSN_MATCH(STRHW, insn))) {
		insns[2] = (insns[2] & 0x0fffffff) | 0xd0000000;
	}

	if (THUMB2_INSN_MATCH(TEQ1, insn) ||
	    THUMB2_INSN_MATCH(TST1, insn)) {
		/*  TEQ SP, #<const> */
		insns[2] = (insn & 0xfffffff0) | 0xd;
	} else if (THUMB2_INSN_MATCH(TEQ2, insn) ||
		   THUMB2_INSN_MATCH(TST2, insn)) {
		if ((THUMB2_INSN_REG_RN(insn) == 15) &&
		    (THUMB2_INSN_REG_RM(insn) == 15))
			/*  TEQ/TST PC, PC */
			insns[2] = (insn & 0xfffdfffd);
		else if (THUMB2_INSN_REG_RM(insn) == 15)
			/*  TEQ/TST Rn, PC */
			insns[2] = (insn & 0xfff0ffff) | 0xd0000;
		else if (THUMB2_INSN_REG_RN(insn) == 15)
			/*  TEQ/TST PC, Rm */
			insns[2] = (insn & 0xfffffff0) | 0xd;
	}

	return 0;
}

static int arch_check_insn_thumb(u32 insn)
{
	int ret = 0;

	/* check instructions that can change PC */
	if (THUMB_INSN_MATCH(UNDEF, insn) ||
	    THUMB2_INSN_MATCH(BLX1, insn) ||
	    THUMB2_INSN_MATCH(BL, insn) ||
	    THUMB_INSN_MATCH(SWI, insn) ||
	    THUMB_INSN_MATCH(BREAK, insn) ||
	    THUMB2_INSN_MATCH(B1, insn) ||
	    THUMB2_INSN_MATCH(B2, insn) ||
	    THUMB2_INSN_MATCH(BXJ, insn) ||
	    (THUMB2_INSN_MATCH(ADR, insn) &&
	     THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRW, insn) && THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRW1, insn) &&
	     THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRHW, insn) &&
	     THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRHW1, insn) &&
	     THUMB2_INSN_REG_RT(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRWL, insn) &&
	     THUMB2_INSN_REG_RT(insn) == 15) ||
	    THUMB2_INSN_MATCH(LDMIA, insn) ||
	    THUMB2_INSN_MATCH(LDMDB, insn) ||
	    (THUMB2_INSN_MATCH(DP, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(RSBW, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(RORW, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(ROR, insn) && THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSLW1, insn) &&
	     THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSLW2, insn) &&
	     THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSRW1, insn) &&
	     THUMB2_INSN_REG_RD(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LSRW2, insn) &&
	     THUMB2_INSN_REG_RD(insn) == 15) ||
	    /* skip PC, #-imm12 -> SP, #-imm8 and Tegra-hanging instructions */
	    (THUMB2_INSN_MATCH(STRW1, insn) &&
	     THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRBW1, insn) &&
	     THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRHW1, insn) &&
	     THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(STRHW, insn) &&
	     THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRW, insn) && THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRBW, insn) &&
	     THUMB2_INSN_REG_RN(insn) == 15) ||
	    (THUMB2_INSN_MATCH(LDRHW, insn) &&
	     THUMB2_INSN_REG_RN(insn) == 15) ||
	    /* skip STRDx/LDRDx Rt, Rt2, [Rd, ...] */
	    (THUMB2_INSN_MATCH(LDRD, insn) || THUMB2_INSN_MATCH(LDRD1, insn) ||
	     THUMB2_INSN_MATCH(STRD, insn))) {
		ret = -EFAULT;
	}

	return ret;
}

static int do_make_trampoline_thumb(u32 vaddr, u32 insn,
				    u32 *tramp, size_t tramp_len)
{
	int ret;
	int uregs = 0;
	int pc_dep = 0;
	unsigned int addr;

	ret = arch_check_insn_thumb(insn);
	if (ret)
		return ret;

	if (THUMB_INSN_MATCH(APC, insn) || THUMB_INSN_MATCH(LRO3, insn)) {
		uregs = 0x0700;		/* 8-10 */
		pc_dep = 1;
	} else if (THUMB_INSN_MATCH(MOV3, insn) &&
		   (((((unsigned char)insn) & 0xff) >> 3) == 15)) {
		/* MOV Rd, PC */
		uregs = 0x07;
		pc_dep = 1;
	} else if THUMB2_INSN_MATCH(ADR, insn) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if (((THUMB2_INSN_MATCH(LDRW, insn) ||
		     THUMB2_INSN_MATCH(LDRW1, insn) ||
		     THUMB2_INSN_MATCH(LDRBW, insn) ||
		     THUMB2_INSN_MATCH(LDRBW1, insn) ||
		     THUMB2_INSN_MATCH(LDRHW, insn) ||
		     THUMB2_INSN_MATCH(LDRHW1, insn) ||
		     THUMB2_INSN_MATCH(LDRWL, insn)) &&
		    THUMB2_INSN_REG_RN(insn) == 15) ||
		     THUMB2_INSN_MATCH(LDREX, insn) ||
		     ((THUMB2_INSN_MATCH(STRW, insn) ||
		       THUMB2_INSN_MATCH(STRBW, insn) ||
		       THUMB2_INSN_MATCH(STRHW, insn) ||
		       THUMB2_INSN_MATCH(STRHW1, insn)) &&
		      (THUMB2_INSN_REG_RN(insn) == 15 ||
		       THUMB2_INSN_REG_RT(insn) == 15)) ||
		     ((THUMB2_INSN_MATCH(STRT, insn) ||
		       THUMB2_INSN_MATCH(STRHT, insn)) &&
		       (THUMB2_INSN_REG_RN(insn) == 15 ||
			THUMB2_INSN_REG_RT(insn) == 15))) {
		uregs = 0xf000;		/* Rt 12-15 */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(LDRD, insn) ||
		    THUMB2_INSN_MATCH(LDRD1, insn)) &&
		   (THUMB2_INSN_REG_RN(insn) == 15)) {
		uregs = 0xff00;		/* Rt 12-15, Rt2 8-11 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(MUL, insn) &&
		   THUMB2_INSN_REG_RM(insn) == 15) {
		uregs = 0xf;
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(DP, insn) &&
		   (THUMB2_INSN_REG_RN(insn) == 15 ||
		    THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0xf000;		/* Rd 12-15 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(STRD, insn) &&
		   ((THUMB2_INSN_REG_RN(insn) == 15) ||
		    (THUMB2_INSN_REG_RT(insn) == 15) ||
		    THUMB2_INSN_REG_RT2(insn) == 15)) {
		uregs = 0xff00;		/* Rt 12-15, Rt2 8-11 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(RSBW, insn) &&
		   THUMB2_INSN_REG_RN(insn) == 15) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if (THUMB2_INSN_MATCH(RORW, insn) &&
		   (THUMB2_INSN_REG_RN(insn) == 15 ||
		    THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0x0f00;
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(ROR, insn) ||
		    THUMB2_INSN_MATCH(LSLW2, insn) ||
		    THUMB2_INSN_MATCH(LSRW2, insn)) &&
		   THUMB2_INSN_REG_RM(insn) == 15) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(LSLW1, insn) ||
		    THUMB2_INSN_MATCH(LSRW1, insn)) &&
		   (THUMB2_INSN_REG_RN(insn) == 15 ||
		    THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0x0f00;		/* Rd 8-11 */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(TEQ1, insn) ||
		    THUMB2_INSN_MATCH(TST1, insn)) &&
		   THUMB2_INSN_REG_RN(insn) == 15) {
		uregs = 0xf0000;	/* Rn 0-3 (16-19) */
		pc_dep = 1;
	} else if ((THUMB2_INSN_MATCH(TEQ2, insn) ||
		    THUMB2_INSN_MATCH(TST2, insn)) &&
		   (THUMB2_INSN_REG_RN(insn) == 15 ||
		    THUMB2_INSN_REG_RM(insn) == 15)) {
		uregs = 0xf0000;	/* Rn 0-3 (16-19) */
		pc_dep = 1;
	}

	if (unlikely(uregs && pc_dep)) {
		memcpy(tramp, pc_dep_insn_execbuf_thumb, tramp_len);
		prep_pc_dep_insn_execbuf_thumb(tramp, insn, uregs);

		addr = vaddr + 4;
		*((u16 *)tramp + 13) = RET_BREAK_THUMB;
		*((u16 *)tramp + 14) = addr & 0x0000ffff;
		*((u16 *)tramp + 15) = addr >> 16;
		if (!is_thumb2(insn)) {
			addr = vaddr + 2;
			*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((u16 *)tramp + 17) = addr >> 16;
		} else {
			addr = vaddr + 4;
			*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((u16 *)tramp + 17) = addr >> 16;
		}
	} else {
		memcpy(tramp, gen_insn_execbuf_thumb, tramp_len);
		*((u16 *)tramp + 13) = RET_BREAK_THUMB;
		if (!is_thumb2(insn)) {
			addr = vaddr + 2;
			*((u16 *)tramp + 2) = insn;
			*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((u16 *)tramp + 17) = addr >> 16;
		} else {
			addr = vaddr + 4;
			tramp[1] = insn;
			*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
			*((u16 *)tramp + 17) = addr >> 16;
		}
	}

	if (THUMB_INSN_MATCH(B2, insn)) {
		memcpy(tramp, b_off_insn_execbuf_thumb, tramp_len);
		*((u16 *)tramp + 13) = RET_BREAK_THUMB;
		addr = branch_t16_dest(insn, vaddr);
		*((u16 *)tramp + 14) = (addr & 0x0000ffff) | 0x1;
		*((u16 *)tramp + 15) = addr >> 16;
		*((u16 *)tramp + 16) = 0;
		*((u16 *)tramp + 17) = 0;

	} else if (THUMB_INSN_MATCH(B1, insn)) {
		memcpy(tramp, b_cond_insn_execbuf_thumb, tramp_len);
		*((u16 *)tramp + 13) = RET_BREAK_THUMB;
		*((u16 *)tramp + 0) |= (insn & 0xf00);
		addr = branch_cond_t16_dest(insn, vaddr);
		*((u16 *)tramp + 14) = (addr & 0x0000ffff) | 0x1;
		*((u16 *)tramp + 15) = addr >> 16;
		addr = vaddr + 2;
		*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((u16 *)tramp + 17) = addr >> 16;

	} else if (THUMB_INSN_MATCH(BLX2, insn) ||
		   THUMB_INSN_MATCH(BX, insn)) {
		memcpy(tramp, b_r_insn_execbuf_thumb, tramp_len);
		*((u16 *)tramp + 13) = RET_BREAK_THUMB;
		*((u16 *)tramp + 4) = insn;
		addr = vaddr + 2;
		*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((u16 *)tramp + 17) = addr >> 16;

	} else if (THUMB_INSN_MATCH(CBZ, insn)) {
		memcpy(tramp, cbz_insn_execbuf_thumb, tramp_len);
		*((u16 *)tramp + 13) = RET_BREAK_THUMB;
		/* zero out original branch displacement (imm5 = 0; i = 0) */
		*((u16 *)tramp + 0) = insn & (~0x2f8);
		/* replace it with 8 bytes offset in execbuf (imm5 = 0b00010) */
		*((u16 *)tramp + 0) |= 0x20;
		addr = cbz_t16_dest(insn, vaddr);
		*((u16 *)tramp + 14) = (addr & 0x0000ffff) | 0x1;
		*((u16 *)tramp + 15) = addr >> 16;
		addr = vaddr + 2;
		*((u16 *)tramp + 16) = (addr & 0x0000ffff) | 0x1;
		*((u16 *)tramp + 17) = addr >> 16;
	}

	return 0;
}

int make_trampoline_thumb(struct arch_insn_arm *ainsn,
			  u32 vaddr, u32 insn, u32 *tramp, size_t tramp_len)
{
	int ret;

	ret = do_make_trampoline_thumb(vaddr, insn, tramp, tramp_len);
	if (ret) {
		struct decode_info info = {
			.vaddr = vaddr,
			.tramp = tramp,
			.handeler = NULL,
		};

		ret = decode_thumb(insn, &info);
		if (info.handeler) {
			u16 *tr = (u16 *)tramp;
			tr[13] = RET_BREAK_THUMB; /* bp for uretprobe */
			ainsn->handler = info.handeler;
		}
	}

	return ret;
}

int noret_thumb(u32 opcode)
{
	return !!(THUMB2_INSN_MATCH(BL, opcode) ||
		  THUMB2_INSN_MATCH(BLX1, opcode) ||
		  THUMB_INSN_MATCH(BLX2, opcode));
}
