/**
 * @file writer/kernel_operations.h
 * @author Alexander Aksenov <a.aksenov@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Writer kernel operations.
 */

/* Kernel functions wrap */

#ifndef __KERNEL_OPERATIONS_H__
#define __KERNEL_OPERATIONS_H__

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>


/* ARCH-DEPENDED OPERATIONS */


/* Regs manipulations */
#if defined(CONFIG_ARM)

#define get_regs_ret_func(regs)     (regs->ARM_lr)    /**< Get lr reg. */
#define get_regs_ret_val(regs)      (regs->ARM_r0)    /**< Get ret val. */
#define get_regs_stack_ptr(regs)    (regs->ARM_sp)    /**< Get stack pointer. */

#elif defined(CONFIG_X86_32)

#define get_regs_ret_val(regs)      (regs->ax)        /**< Get ret val. */
#define get_regs_stack_ptr(regs)    (regs->sp)        /**< Get stack pointer. */

static inline u32 get_regs_ret_func(struct pt_regs *regs)
{
	u32 *sp, addr = 0;

	if (user_mode(regs)) {
		sp = (u32 *)regs->sp;
		if (get_user(addr, sp))
			pr_info("failed to dereference a pointer, sp=%p, "
				"pc=%lx\n", sp, regs->ip - 1);
	} else {
		sp = (u32 *)kernel_stack_pointer(regs);
		addr = *sp;
	}

	return addr;
}

#elif defined(CONFIG_ARM64)

static inline u64 get_regs_ret_func(struct pt_regs *regs)
{
	if (compat_user_mode(regs))
		return regs->compat_lr;
	else
		return regs->regs[30];
}

#endif /* CONFIG_arch */

#endif /* __KERNEL_OPERATIONS_H__ */
