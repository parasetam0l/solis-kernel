/**
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: initial implementation for ARM/MIPS
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space
 * Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>:
 * redesign module for separating core and arch parts
 * @author Alexander Shirshikov <a.shirshikov@samsung.com>:
 * initial implementation for Thumb
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
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 */

#ifndef _SWAP_ASM_TRAMPS_ARM_H
#define _SWAP_ASM_TRAMPS_ARM_H


#include <linux/types.h>


extern u32 gen_insn_execbuf[];
extern u32 pc_dep_insn_execbuf[];
extern u32 b_r_insn_execbuf[];
extern u32 b_cond_insn_execbuf[];
extern u32 blx_off_insn_execbuf[];


#endif /* _SWAP_ASM_TRAMPS_ARM_H */
