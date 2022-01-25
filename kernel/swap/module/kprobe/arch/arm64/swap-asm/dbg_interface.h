#ifndef _ASM_DBG_INTERFACE_H
#define _ASM_DBG_INTERFACE_H

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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/list.h>
#include <linux/types.h>


#define ESR_ELx_IL       (1 << 25)
#define ESR_ELx_EC_BRK   0xf0000000
#define ESR_ELx_EC_MASK  0xfc000000
#define BRK_COMM_MASK    0x0000ffff

#define DBG_BRK_ESR_MASK (ESR_ELx_EC_MASK | ESR_ELx_IL | BRK_COMM_MASK)
#define DBG_BRK_ESR(x)   (ESR_ELx_EC_BRK | ESR_ELx_IL | (BRK_COMM_MASK & (x)))

#define MAKE_BRK(v)      ((((v) & 0xffff) << 5) | 0xd4200000)


enum dbg_code {
	DBG_HANDLED,
	DBG_ERROR,
};


struct brk_hook {
	struct list_head list;
	u32 spsr_mask;
	u32 spsr_val;
	u32 esr_mask;
	u32 esr_val;
	enum dbg_code (*fn)(struct pt_regs *regs, unsigned int esr);
};


void dbg_brk_hook_reg(struct brk_hook *hook);
void dbg_brk_hook_unreg(struct brk_hook *hook);

int dbg_iface_init(void);
void dbg_iface_uninit(void);


#endif /* _ASM_DBG_INTERFACE_H */
