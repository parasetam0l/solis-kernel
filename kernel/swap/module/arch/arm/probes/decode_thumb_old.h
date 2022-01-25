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


#ifndef _SWAP_ASM_DECODE_THUMB_OLD_H
#define _SWAP_ASM_DECODE_THUMB_OLD_H


/* == THUMB == */
#define THUMB_INSN_MATCH(name, insn) \
	(((insn & 0x0000FFFF) & MASK_THUMB_INSN_##name) == \
	 PTRN_THUMB_INSN_##name)


/* Undefined */
#define MASK_THUMB_INSN_UNDEF	0xFE00
#define PTRN_THUMB_INSN_UNDEF	0xDE00

/* Branches */
#define MASK_THUMB_INSN_B1	0xF000
#define PTRN_THUMB_INSN_B1	0xD000		/* b<cond> label */

#define MASK_THUMB_INSN_B2	0xF800
#define PTRN_THUMB_INSN_B2	0xE000		/* b label */

#define MASK_THUMB_INSN_CBZ	0xF500
#define PTRN_THUMB_INSN_CBZ	0xB100		/* CBZ/CBNZ */

#define MASK_THUMB_INSN_BLX2	0xFF80		/* blx reg */
#define PTRN_THUMB_INSN_BLX2	0x4780

#define MASK_THUMB_INSN_BX	0xFF80
#define PTRN_THUMB_INSN_BX	0x4700

/* Software interrupts */
#define MASK_THUMB_INSN_SWI	0xFF00
#define PTRN_THUMB_INSN_SWI	0xDF00

/* Break */
#define MASK_THUMB_INSN_BREAK	0xFF00
#define PTRN_THUMB_INSN_BREAK	0xBE00

/* Data processing immediate */
#define MASK_THUMB_INSN_DP	0xFC00
#define PTRN_THUMB_INSN_DP	0x4000

#define MASK_THUMB_INSN_APC	0xF800
#define PTRN_THUMB_INSN_APC	0xA000		/* ADD Rd, [PC, #<imm8> * 4] */

#define MASK_THUMB_INSN_MOV3	0xFF00
#define PTRN_THUMB_INSN_MOV3	0x4600		/* MOV Rd, PC */

/* Load immediate offset */
#define MASK_THUMB_INSN_LIO1	0xF800
#define PTRN_THUMB_INSN_LIO1	0x6800		/* LDR */

#define MASK_THUMB_INSN_LIO2	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_LIO2	0x7800		/* LDRB */

#define MASK_THUMB_INSN_LIO3	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_LIO3	0x8800		/* LDRH */

#define MASK_THUMB_INSN_LIO4	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_LIO4	0x9800		/* LDR SP relative */

/* Store immediate offset */
#define MASK_THUMB_INSN_SIO1	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_SIO1	0x6000		/* STR */

#define MASK_THUMB_INSN_SIO2	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_SIO2	0x7000		/* STRB */

#define MASK_THUMB_INSN_SIO3	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_SIO3	0x8000		/* STRH */

#define MASK_THUMB_INSN_SIO4	MASK_THUMB_INSN_LIO1
#define PTRN_THUMB_INSN_SIO4	0x9000		/* STR SP relative */

/* Load register offset */
#define MASK_THUMB_INSN_LRO1	0xFE00
#define PTRN_THUMB_INSN_LRO1	0x5600		/* LDRSB */

#define MASK_THUMB_INSN_LRO2	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_LRO2	0x5800		/* LDR */

#define MASK_THUMB_INSN_LRO3	0xf800
#define PTRN_THUMB_INSN_LRO3	0x4800		/* LDR Rd, [PC, #<imm8> * 4] */

#define MASK_THUMB_INSN_LRO4	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_LRO4	0x5A00		/* LDRH */

#define MASK_THUMB_INSN_LRO5	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_LRO5	0x5C00		/* LDRB */

#define MASK_THUMB_INSN_LRO6	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_LRO6	0x5E00		/* LDRSH */

/* Store register offset */
#define MASK_THUMB_INSN_SRO1	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_SRO1	0x5000		/* STR */

#define MASK_THUMB_INSN_SRO2	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_SRO2	0x5200		/* STRH */

#define MASK_THUMB_INSN_SRO3	MASK_THUMB_INSN_LRO1
#define PTRN_THUMB_INSN_SRO3	0x5400		/* STRB */


/* == THUMB2 == */
#define THUMB2_INSN_MATCH(name, insn) \
	((insn & MASK_THUMB2_INSN_##name) == PTRN_THUMB2_INSN_##name)

#define THUMB2_INSN_REG_RT(insn)	((insn & 0xf0000000) >> 28)
#define THUMB2_INSN_REG_RT2(insn)	((insn & 0x0f000000) >> 24)
#define THUMB2_INSN_REG_RN(insn)	(insn & 0x0000000f)
#define THUMB2_INSN_REG_RD(insn)	((insn & 0x0f000000) >> 24)
#define THUMB2_INSN_REG_RM(insn)	((insn & 0x000f0000) >> 16)


/* Branches */
#define MASK_THUMB2_INSN_B1	0xD000F800
#define PTRN_THUMB2_INSN_B1	0x8000F000

#define MASK_THUMB2_INSN_B2	0xD000F800
#define PTRN_THUMB2_INSN_B2	0x9000F000

#define MASK_THUMB2_INSN_BL	0xD000F800
#define PTRN_THUMB2_INSN_BL	0xD000F000	/* bl imm  swapped */

#define MASK_THUMB2_INSN_BLX1	0xD001F800
#define PTRN_THUMB2_INSN_BLX1	0xC000F000

#define MASK_THUMB2_INSN_BXJ	0xD000FFF0
#define PTRN_THUMB2_INSN_BXJ	0x8000F3C0

/* Data processing register shift */
#define MASK_THUMB2_INSN_DPRS	0xFFE00000
#define PTRN_THUMB2_INSN_DPRS	0xEA000000

/* Data processing immediate */
#define MASK_THUMB2_INSN_DPI	0xFBE08000
#define PTRN_THUMB2_INSN_DPI	0xF2000000

#define MASK_THUMB2_INSN_RSBW	0x8000fbe0
#define PTRN_THUMB2_INSN_RSBW	0x0000f1c0	/* RSB{S}.W Rd,Rn,#<const> */

#define MASK_THUMB2_INSN_RORW	0xf0f0ffe0
#define PTRN_THUMB2_INSN_RORW	0xf000fa60	/* ROR{S}.W Rd, Rn, Rm */

#define MASK_THUMB2_INSN_ROR	0x0030ffef
#define PTRN_THUMB2_INSN_ROR	0x0030ea4f	/* ROR{S} Rd, Rm, #<imm> */

#define MASK_THUMB2_INSN_LSLW1	0xf0f0ffe0
#define PTRN_THUMB2_INSN_LSLW1	0xf000fa00	/* LSL{S}.W Rd, Rn, Rm */

#define MASK_THUMB2_INSN_LSLW2	0x0030ffef
#define PTRN_THUMB2_INSN_LSLW2	0x0000ea4f	/* LSL{S}.W Rd, Rm, #<imm5>*/

#define MASK_THUMB2_INSN_LSRW1	0xf0f0ffe0
#define PTRN_THUMB2_INSN_LSRW1	0xf000fa20	/* LSR{S}.W Rd, Rn, Rm */

#define MASK_THUMB2_INSN_LSRW2	0x0030ffef
#define PTRN_THUMB2_INSN_LSRW2	0x0010ea4f	/* LSR{S}.W Rd, Rm, #<imm5> */

#define MASK_THUMB2_INSN_TEQ1	0x8f00fbf0
#define PTRN_THUMB2_INSN_TEQ1	0x0f00f090	/* TEQ Rn, #<const> */

#define MASK_THUMB2_INSN_TEQ2	0x0f00fff0
#define PTRN_THUMB2_INSN_TEQ2	0x0f00ea90	/* TEQ Rn, Rm{,<shift>} */

#define MASK_THUMB2_INSN_TST1	0x8f00fbf0
#define PTRN_THUMB2_INSN_TST1	0x0f00f010	/* TST Rn, #<const> */

#define MASK_THUMB2_INSN_TST2	0x0f00fff0
#define PTRN_THUMB2_INSN_TST2	0x0f00ea10	/* TST Rn, Rm{,<shift>} */

/* Load immediate offset */
#define MASK_THUMB2_INSN_LDRW	0x0000fff0
#define PTRN_THUMB2_INSN_LDRW	0x0000f850	/* LDR.W Rt, [Rn, #-<imm12>] */

#define MASK_THUMB2_INSN_LDRW1	MASK_THUMB2_INSN_LDRW
#define PTRN_THUMB2_INSN_LDRW1	0x0000f8d0	/* LDR.W Rt, [Rn, #<imm12>] */

#define MASK_THUMB2_INSN_LDRBW	MASK_THUMB2_INSN_LDRW
#define PTRN_THUMB2_INSN_LDRBW	0x0000f810	/* LDRB.W Rt, [Rn, #-<imm8>] */

#define MASK_THUMB2_INSN_LDRBW1	MASK_THUMB2_INSN_LDRW
#define PTRN_THUMB2_INSN_LDRBW1	0x0000f890	/* LDRB.W Rt, [Rn, #<imm12>] */

#define MASK_THUMB2_INSN_LDRHW	MASK_THUMB2_INSN_LDRW
#define PTRN_THUMB2_INSN_LDRHW	0x0000f830	/* LDRH.W Rt, [Rn, #-<imm8>] */

#define MASK_THUMB2_INSN_LDRHW1	MASK_THUMB2_INSN_LDRW
#define PTRN_THUMB2_INSN_LDRHW1	0x0000f8b0	/* LDRH.W Rt, [Rn, #<imm12>] */

#define MASK_THUMB2_INSN_LDRD	0x0000fed0
#define PTRN_THUMB2_INSN_LDRD	0x0000e850	/* LDRD Rt, Rt2, [Rn, #-<imm8>] */

#define MASK_THUMB2_INSN_LDRD1	MASK_THUMB2_INSN_LDRD
#define PTRN_THUMB2_INSN_LDRD1	0x0000e8d0	/* LDRD Rt, Rt2, [Rn, #<imm8>] */

#define MASK_THUMB2_INSN_LDRWL	0x0fc0fff0
#define PTRN_THUMB2_INSN_LDRWL	0x0000f850	/* LDR.W Rt, [Rn,Rm,LSL #<imm2>] */

#define MASK_THUMB2_INSN_LDREX	0x0f00ffff
#define PTRN_THUMB2_INSN_LDREX	0x0f00e85f	/* LDREX Rt, [PC, #<imm8>] */

#define MASK_THUMB2_INSN_MUL	0xf0f0fff0
#define PTRN_THUMB2_INSN_MUL	0xf000fb00	/* MUL Rd, Rn, Rm */

#define MASK_THUMB2_INSN_DP	0x0000ff00
#define PTRN_THUMB2_INSN_DP	0x0000eb00	/* ADD/SUB/SBC/...Rd,Rn,Rm{,<shift>} */

/* Store immediate offset */
#define MASK_THUMB2_INSN_STRW	0x0fc0fff0
#define PTRN_THUMB2_INSN_STRW	0x0000f840	/* STR.W Rt,[Rn,Rm,{LSL #<imm2>}] */

#define MASK_THUMB2_INSN_STRW1	0x0000fff0
#define PTRN_THUMB2_INSN_STRW1	0x0000f8c0	/* STR.W Rt, [Rn, #imm12]
						 * STR.W Rt, [PC, #imm12] shall be
						 * skipped, because it hangs
						 * on Tegra. WTF */

#define MASK_THUMB2_INSN_STRHW	MASK_THUMB2_INSN_STRW
#define PTRN_THUMB2_INSN_STRHW	0x0000f820	/* STRH.W Rt,[Rn,Rm,{LSL #<imm2>}] */

#define MASK_THUMB2_INSN_STRHW1	0x0000fff0
#define PTRN_THUMB2_INSN_STRHW1	0x0000f8a0	/* STRH.W Rt, [Rn, #<imm12>] */

#define MASK_THUMB2_INSN_STRHT	0x0f00fff0	/* strht r1, [pc, #imm] illegal
						 * instruction on Tegra. WTF */
#define PTRN_THUMB2_INSN_STRHT	0x0e00f820	/* STRHT Rt, [Rn, #<imm8>] */

#define MASK_THUMB2_INSN_STRT	0x0f00fff0
#define PTRN_THUMB2_INSN_STRT	0x0e00f840	/* STRT Rt, [Rn, #<imm8>] */

#define MASK_THUMB2_INSN_STRBW	MASK_THUMB2_INSN_STRW
#define PTRN_THUMB2_INSN_STRBW	0x0000f800	/* STRB.W Rt,[Rn,Rm,{LSL #<imm2>}] */

#define MASK_THUMB2_INSN_STRBW1	0x0000fff0
#define PTRN_THUMB2_INSN_STRBW1	0x0000f880	/* STRB.W Rt, [Rn, #<imm12>]
						 * STRB.W Rt, [PC, #imm12] shall be
						 * skipped, because it hangs
						 * on Tegra. WTF */

#define MASK_THUMB2_INSN_STRBT	0x0f00fff0
#define PTRN_THUMB2_INSN_STRBT	0x0e00f800	/* STRBT Rt, [Rn, #<imm8>}] */

#define MASK_THUMB2_INSN_STRD	0x0000fe50
#define PTRN_THUMB2_INSN_STRD	0x0000e840	/* STR{D,EX,EXB,EXH,EXD} Rt, Rt2, [Rn, #<imm8>] */

/* Load register offset */
#define MASK_THUMB2_INSN_ADR	0x8000fa1f
#define PTRN_THUMB2_INSN_ADR	0x0000f20f

/* Load multiple */
#define MASK_THUMB2_INSN_LDMIA	0x8000ffd0
#define PTRN_THUMB2_INSN_LDMIA	0x8000e890	/* LDMIA(.W) Rn(!),{Rx-PC} */

#define MASK_THUMB2_INSN_LDMDB	0x8000ffd0
#define PTRN_THUMB2_INSN_LDMDB	0x8000e910	/* LDMDB(.W) Rn(!), {Rx-PC} */


#endif /* _SWAP_ASM_DECODE_THUMB_OLD_H */
