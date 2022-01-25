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


#include <linux/kconfig.h>

#ifdef CONFIG_SWAP_KERNEL_IMMUTABLE
# error "Kernel is immutable"
#endif /* CONFIG_SWAP_KERNEL_IMMUTABLE */


#include <linux/module.h>
#include <linux/rwlock.h>
#include <asm/debug-monitors.h>
#include <ksyms/ksyms.h>
#include "dbg_interface.h"




/* ============================================================================
 * =                               BRK IMPLEMENT                              =
 * ============================================================================
 */
static LIST_HEAD(brk_list);
static DEFINE_RWLOCK(brk_list_lock);

void dbg_brk_hook_reg(struct brk_hook *hook)
{
	write_lock(&brk_list_lock);
	list_add(&hook->list, &brk_list);
	write_unlock(&brk_list_lock);
}
EXPORT_SYMBOL_GPL(dbg_brk_hook_reg);

void dbg_brk_hook_unreg(struct brk_hook *hook)
{
	write_lock(&brk_list_lock);
	list_del(&hook->list);
	write_unlock(&brk_list_lock);
}
EXPORT_SYMBOL_GPL(dbg_brk_hook_unreg);

static enum dbg_code call_brk_hook(struct pt_regs *regs, unsigned int esr)
{
	struct brk_hook *hook;
	enum dbg_code (*fn)(struct pt_regs *regs, unsigned int esr) = NULL;

	read_lock(&brk_list_lock);
	list_for_each_entry(hook, &brk_list, list)
		if (((esr & hook->esr_mask) == hook->esr_val) &&
		    ((regs->pstate & hook->spsr_mask) == hook->spsr_val))
			fn = hook->fn;
	read_unlock(&brk_list_lock);

	return fn ? fn(regs, esr) : DBG_ERROR;
}


typedef int (*dbg_fn_t)(unsigned long addr, unsigned int esr,
			struct pt_regs *regs);

static dbg_fn_t *brk_handler_ptr;
static dbg_fn_t orig_brk_handler;

static int brk_handler(unsigned long addr, unsigned int esr,
		       struct pt_regs *regs)
{
	/* call the registered breakpoint handler */
	if (call_brk_hook(regs, esr) == DBG_HANDLED)
		return 0;

	return orig_brk_handler(addr, esr, regs);
}

static void init_brk(dbg_fn_t *fn)
{
	brk_handler_ptr = fn;
	orig_brk_handler = *brk_handler_ptr;
	*brk_handler_ptr = brk_handler;
}

static void uninit_brk(void)
{
	*brk_handler_ptr = orig_brk_handler;
}






/* ============================================================================
 * =                                INIT / EXIT                               =
 * ============================================================================
 */
int dbg_iface_init(void)
{
	struct fault_info {
		int (*fn)(unsigned long addr, unsigned int esr,
			  struct pt_regs *regs);
		int sig;
		int code;
		const char *name;
	};

	struct fault_info *debug_finfo;
	struct fault_info *finfo_brk;

	debug_finfo = (struct fault_info *)swap_ksyms("debug_fault_info");
	if (debug_finfo == NULL) {
		pr_err("cannot found 'debug_fault_info'\n");
		return -EINVAL;
	}

	finfo_brk = &debug_finfo[DBG_ESR_EVT_BRK];

	init_brk(&finfo_brk->fn);

	return 0;
}

void dbg_iface_uninit(void)
{
	uninit_brk();
}
