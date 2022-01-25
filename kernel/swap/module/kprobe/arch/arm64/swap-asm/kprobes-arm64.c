/*
 * Copyright (C) Samsung Electronics, 2014
 *
 * Copied from: arch/arm64/kernel/kprobes-arm64.c
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
#include <kprobe/swap_kprobes.h>

#include "probes-decode.h"
#include "kprobes-arm64.h"
#include "simulate-insn.h"
#include "condn-helpers.h"


/*
 * condition check functions for kp_cores simulation
 */
static unsigned long __check_pstate(struct kp_core *p, struct pt_regs *regs)
{
	struct arch_specific_insn *asi = &p->ainsn;
	unsigned long pstate = regs->pstate & 0xffffffff;

	return asi->pstate_cc(pstate);
}

static unsigned long __check_cbz(struct kp_core *p, struct pt_regs *regs)
{
	return check_cbz((u32)p->opcode, regs);
}

static unsigned long __check_cbnz(struct kp_core *p, struct pt_regs *regs)
{
	return check_cbnz((u32)p->opcode, regs);
}

static unsigned long __check_tbz(struct kp_core *p, struct pt_regs *regs)
{
	return check_tbz((u32)p->opcode, regs);
}

static unsigned long __check_tbnz(struct kp_core *p, struct pt_regs *regs)
{
	return check_tbnz((u32)p->opcode, regs);
}

/*
 * prepare functions for instruction simulation
 */
static void prepare_none(struct kp_core *p, struct arch_specific_insn *asi)
{
}

static void prepare_bcond(struct kp_core *p, struct arch_specific_insn *asi)
{
	kprobe_opcode_t insn = p->opcode;

	asi->check_condn = __check_pstate;
	asi->pstate_cc = probe_condition_checks[insn & 0xf];
}

static void prepare_cbz_cbnz(struct kp_core *p, struct arch_specific_insn *asi)
{
	kprobe_opcode_t insn = p->opcode;

	asi->check_condn = (insn & (1 << 24)) ? __check_cbnz : __check_cbz;
}

static void prepare_tbz_tbnz(struct kp_core *p, struct arch_specific_insn *asi)
{
	kprobe_opcode_t insn = p->opcode;

	asi->check_condn = (insn & (1 << 24)) ? __check_tbnz : __check_tbz;
}


/* Load literal (PC-relative) instructions
 * Encoding:  xx01 1x00 xxxx xxxx xxxx xxxx xxxx xxxx
 *
 * opcode[26]: V=0, Load GP registers, simulate them.
 * Encoding: xx01 1000 xxxx xxxx xxxx xxxx xxxx xxxx
 *	opcode[31:30]: op = 00, 01 - LDR literal
 *	opcode[31:30]: op = 10,    - LDRSW literal
 *
 * 1.   V=1 -Load FP/AdvSIMD registers
 *	Encoding: xx01 1100 xxxx xxxx xxxx xxxx xxxx xxxx
 * 2.   V=0,opc=11 -PRFM(Prefetch literal)
 *	Encoding: 1101 1000 xxxx xxxx xxxx xxxx xxxx xxxx
 *	Reject FP/AdvSIMD literal load & PRFM literal.
 */
static const struct aarch64_decode_item load_literal_subtable[] = {
	DECODE_REJECT(0x1C000000, 0x3F000000),
	DECODE_REJECT(0xD8000000, 0xFF000000),
	DECODE_LITERAL(0x18000000, 0xBF000000, prepare_none,
		       simulate_ldr_literal),
	DECODE_LITERAL(0x98000000, 0xFF000000, prepare_none,
		       simulate_ldrsw_literal),
	DECODE_END,
};

/* AArch64 instruction decode table for kp_cores:
 * The instruction will fall into one of the 3 groups:
 *  1. Single stepped out-of-the-line slot.
 *     -Most instructions fall in this group, those does not
 *      depend on PC address.
 *
 *  2. Should be simulated because of PC-relative/literal access.
 *     -All branching and PC-relative insrtcutions are simulated
 *      in C code, making use of saved pt_regs
 *      Catch: SIMD/NEON register context are not saved while
 *      entering debug exception, so are rejected for now.
 *
 *  3. Cannot be probed(not safe) so are rejected.
 *     - Exception generation and exception return instructions
 *     - Exclusive monitor(LDREX/STREX family)
 *
 */
static const struct aarch64_decode_item aarch64_decode_table[] = {
	/*
	 * Data processing - PC relative(literal) addressing:
	 * Encoding: xxx1 0000 xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_LITERAL(0x10000000, 0x1F000000, prepare_none,
			simulate_adr_adrp),

	/*
	 * Data processing - Add/Substract Immediate:
	 * Encoding: xxx1 0001 xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_SINGLESTEP(0x11000000, 0x1F000000),

	/*
	 * Data processing
	 * Encoding:
	 *      xxx1 0010 0xxx xxxx xxxx xxxx xxxx xxxx (Logical)
	 *      xxx1 0010 1xxx xxxx xxxx xxxx xxxx xxxx (Move wide)
	 *      xxx1 0011 0xxx xxxx xxxx xxxx xxxx xxxx (Bitfield)
	 *      xxx1 0011 1xxx xxxx xxxx xxxx xxxx xxxx (Extract)
	 */
	DECODE_SINGLESTEP(0x12000000, 0x1E000000),

	/*
	 * Data processing - SIMD/FP/AdvSIMD/Crypto-AES/SHA
	 * Encoding: xxx0 111x xxxx xxxx xxxx xxxx xxxx xxxx
	 * Encoding: xxx1 111x xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_SINGLESTEP(0x0E000000, 0x0E000000),

	/*
	 * Data processing - Register
	 * Encoding: xxxx 101x xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_SINGLESTEP(0x0A000000, 0x0E000000),

	/* Branching Instructions
	 *
	 * Encoding:
	 *  x001 01xx xxxx xxxx xxxx xxxx xxxx xxxx (uncondtional Branch)
	 *  x011 010x xxxx xxxx xxxx xxxx xxxx xxxx (compare & branch)
	 *  x011 011x xxxx xxxx xxxx xxxx xxxx xxxx (Test & Branch)
	 *  0101 010x xxxx xxxx xxxx xxxx xxxx xxxx (Conditional, immediate)
	 *  1101 011x xxxx xxxx xxxx xxxx xxxx xxxx (Unconditional,register)
	 */
	DECODE_BRANCH(0x14000000, 0x7C000000, prepare_none,
			simulate_b_bl),
	DECODE_BRANCH(0x34000000, 0x7E000000, prepare_cbz_cbnz,
		      simulate_cbz_cbnz),
	DECODE_BRANCH(0x36000000, 0x7E000000, prepare_tbz_tbnz,
		      simulate_tbz_tbnz),
	DECODE_BRANCH(0x54000000, 0xFE000000, prepare_bcond,
			simulate_b_cond),
	DECODE_BRANCH(0xD6000000, 0xFE000000, prepare_none,
		      simulate_br_blr_ret),

	/* System insn:
	 * Encoding: 1101 0101 00xx xxxx xxxx xxxx xxxx xxxx
	 *
	 * Note: MSR immediate (update PSTATE daif) is not safe handling
	 * within kp_cores, rejected.
	 *
	 * Don't re-arrange these decode table entries.
	 */
	DECODE_REJECT(0xD500401F, 0xFFF8F01F),
	DECODE_SINGLESTEP(0xD5000000, 0xFFC00000),

	/* Exception Generation:
	 * Encoding:  1101 0100 xxxx xxxx xxxx xxxx xxxx xxxx
	 * Instructions: SVC, HVC, SMC, BRK, HLT, DCPS1, DCPS2, DCPS3
	 */
	DECODE_REJECT(0xD4000000, 0xFF000000),

	/*
	 * Load/Store - Exclusive monitor
	 * Encoding: xx00 1000 xxxx xxxx xxxx xxxx xxxx xxxx
	 *
	 * Reject exlusive monitor'ed instructions
	 */
	DECODE_REJECT(0x08000000, 0x3F000000),

	/*
	 * Load/Store - PC relative(literal):
	 * Encoding:  xx01 1x00 xxxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_TABLE(0x18000000, 0x3B000000, load_literal_subtable),

	/*
	 * Load/Store - Register Pair
	 * Encoding:
	 *      xx10 1x00 0xxx xxxx xxxx xxxx xxxx xxxx
	 *      xx10 1x00 1xxx xxxx xxxx xxxx xxxx xxxx
	 *      xx10 1x01 0xxx xxxx xxxx xxxx xxxx xxxx
	 *      xx10 1x01 1xxx xxxx xxxx xxxx xxxx xxxx
	 */
	DECODE_SINGLESTEP(0x28000000, 0x3A000000),

	/*
	 * Load/Store - Register
	 * Encoding:
	 *      xx11 1x00 xx0x xxxx xxxx 00xx xxxx xxxx (unscaled imm)
	 *      xx11 1x00 xx0x xxxx xxxx 01xx xxxx xxxx (imm post-indexed)
	 *      xx11 1x00 xx0x xxxx xxxx 10xx xxxx xxxx (unpriviledged)
	 *      xx11 1x00 xx0x xxxx xxxx 11xx xxxx xxxx (imm pre-indexed)
	 *
	 *      xx11 1x00 xx10 xxxx xxxx xx10 xxxx xxxx (register offset)
	 *
	 *      xx11 1x01 xxxx xxxx xxxx xxxx xxxx xxxx (unsigned imm)
	 */
	DECODE_SINGLESTEP(0x38000000, 0x3B200000),
	DECODE_SINGLESTEP(0x38200200, 0x38300300),
	DECODE_SINGLESTEP(0x39000000, 0x3B000000),

	/*
	 * Load/Store - AdvSIMD
	 * Encoding:
	 *  0x00 1100 0x00 0000 xxxx xxxx xxxx xxxx (Multiple-structure)
	 *  0x00 1100 1x0x xxxx xxxx xxxx xxxx xxxx (Multi-struct post-indexed)
	 *  0x00 1101 0xx0 0000 xxxx xxxx xxxx xxxx (Single-structure))
	 *  0x00 1101 1xxx xxxx xxxx xxxx xxxx xxxx (Single-struct post-index)
	 */
	DECODE_SINGLESTEP(0x0C000000, 0xBFBF0000),
	DECODE_SINGLESTEP(0x0C800000, 0xBFA00000),
	DECODE_SINGLESTEP(0x0D000000, 0xBF9F0000),
	DECODE_SINGLESTEP(0x0D800000, 0xBF800000),

	/* Unallocated:         xxx0 0xxx xxxx xxxx xxxx xxxx xxxx xxxx */
	DECODE_REJECT(0x00000000, 0x18000000),
	DECODE_END,
};

static int kp_core_decode_insn(kprobe_opcode_t insn,
			       struct arch_specific_insn *asi,
			       const struct aarch64_decode_item *tbl)
{
	unsigned int entry, ret = INSN_REJECTED;

	for (entry = 0; !decode_table_end(tbl[entry]); entry++) {
		if (decode_table_hit(tbl[entry], insn))
			break;
	}

	switch (decode_get_type(tbl[entry])) {
	case DECODE_TYPE_END:
	case DECODE_TYPE_REJECT:
	default:
		ret = INSN_REJECTED;
		break;

	case DECODE_TYPE_SINGLESTEP:
		ret = INSN_GOOD;
		break;

	case DECODE_TYPE_SIMULATE:
		asi->prepare = decode_prepare_fn(tbl[entry]);
		asi->handler = decode_handler_fn(tbl[entry]);
		ret = INSN_GOOD_NO_SLOT;
		break;

	case DECODE_TYPE_TABLE:
		/* recurse with next level decode table */
		ret = kp_core_decode_insn(insn, asi,
					  decode_sub_table(tbl[entry]));
	};

	return ret;
}

/* Return:
 *   INSN_REJECTED     If instruction is one not allowed to kprobe,
 *   INSN_GOOD         If instruction is supported and uses instruction slot,
 *   INSN_GOOD_NO_SLOT If instruction is supported but doesn't use its slot.
 */
enum kp_core_insn arm_kp_core_decode_insn(kprobe_opcode_t insn,
					  struct arch_specific_insn *asi)
{
	return kp_core_decode_insn(insn, asi, aarch64_decode_table);
}
