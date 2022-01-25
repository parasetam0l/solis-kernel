/**
 * swap_sc_patch.c
 * @author Dmitry Kovalenko <d.kovalenko@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Patching of sys_call_table
 */

#include <ksyms/ksyms.h>
#include "swap_sc_patch.h"

static unsigned long original_syscall;
static int patched_syscall = -1;

/* disable write protection */
#define swap_disable_wprot()		\
	asm("pushl %eax \n"		\
	    "movl %cr0, %eax \n"	\
	    "andl $0xfffeffff, %eax \n"	\
	    "movl %eax, %cr0 \n"	\
	    "popl %eax");

/* enable write protection */
#define swap_enable_wprot()	       \
	asm("push %eax \n"	       \
	    "movl %cr0, %eax \n"       \
	    "orl $0x00010000, %eax \n" \
	    "movl %eax, %cr0 \n"       \
	    "popl %eax");

void patch_syscall(int syscall_n, unsigned long new_syscall_addr)
{
	unsigned long tmp;
	unsigned long *sc_table;

	/*
	 * Search for sys_call_table (4 bytes before sysenter_after_call)
	 * sysenter_do_call function which locates before sysenter_after_call
	 * has sys_call_table address in call instruction (latest instruction)
	 */
	tmp = swap_ksyms("sysenter_after_call");
	sc_table = *(unsigned long **)(tmp - 4);

	swap_disable_wprot();
	original_syscall = sc_table[syscall_n];
	sc_table[syscall_n] = new_syscall_addr;
	patched_syscall = syscall_n;
	swap_enable_wprot();
}

void swap_depatch_syscall(void)
{
	if (patched_syscall == -1) {
		printk(KERN_WARNING
		       "SWAP SC_PATCH: there is no patched syscalls");
		return;
	}

	patch_syscall(patched_syscall, original_syscall);
	patched_syscall = -1;
}

asmlinkage long sys_swap_func(void)
{
	/* Your code here */

	return -ENOSYS;
}

#define NI_SYSCALL4SWAP 31
void swap_patch_syscall(void)
{
	patch_syscall(NI_SYSCALL4SWAP, (unsigned long)&sys_swap_func);
}
