/*
 * Copyright (C) Samsung Electronics, 2014
 *
 * Copied from: arch/arm64/kernel/kprobes-arm64.h
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

#ifndef _ARM_UPROBES_ARM64_H
#define _ARM_UPROBES_ARM64_H


enum uprobe_insn {
	INSN_REJECTED,
	INSN_GOOD_NO_SLOT,
	INSN_GOOD,
};


enum uprobe_insn arm64_uprobe_decode_insn(u32 insn, struct arch_insn *asi);


#endif /* _ARM_UPROBES_ARM64_H */
