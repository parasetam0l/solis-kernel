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


#ifndef _SWAP_ASM_COMPAT_ARM64_H
#define _SWAP_ASM_COMPAT_ARM64_H


#ifdef CONFIG_ARM64

# define PSR_T_BIT		COMPAT_PSR_T_BIT

# define ARM_r0			compat_usr(0)
# define ARM_r1			compat_usr(1)
# define ARM_r2			compat_usr(2)
# define ARM_r3			compat_usr(3)
# define ARM_r4			compat_usr(4)
# define ARM_r5			compat_usr(5)
# define ARM_r6			compat_usr(6)
# define ARM_r7			compat_usr(7)
# define ARM_r8			compat_usr(8)
# define ARM_r9			compat_usr(9)
# define ARM_r10		compat_usr(10)
# define ARM_fp			compat_fp
# define ARM_ip			compat_usr(12)
# define ARM_sp			compat_sp
# define ARM_lr			compat_lr
# define ARM_pc			pc
# define ARM_cpsr		pstate

# define thumb_mode(regs)	compat_thumb_mode(regs)

#endif /* CONFIG_ARM64 */


#endif /* _SWAP_ASM_COMPAT_ARM64_H */






