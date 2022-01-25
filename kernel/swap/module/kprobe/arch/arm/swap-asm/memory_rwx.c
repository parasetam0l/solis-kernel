/*
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com> new memory allocator for slots
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
 * Copyright (C) Samsung Electronics, 2014
 */


#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/mmu_writeable.h>


#include <ksyms/ksyms.h>


static struct mm_struct *swap_init_mm;
static int (*swap_set_memory_ro)(unsigned long addr, int numpages);
static int (*swap_set_memory_rw)(unsigned long addr, int numpages);


static int get_pte_cb(pte_t *ptep, pgtable_t token,
		      unsigned long addr, void *data)
{
	*(pte_t *)data = *ptep;

	return 1;
}

static pte_t get_pte(unsigned long page_addr)
{
	pte_t pte = 0;

	apply_to_page_range(swap_init_mm, page_addr,
			    PAGE_SIZE, get_pte_cb, &pte);

	return pte;
}

static void write_to_module(unsigned long addr, unsigned long val)
{
	unsigned long *maddr = (unsigned long *)addr;
	unsigned long page_addr = addr & PAGE_MASK;
	pte_t pte;

	pte = get_pte(page_addr);
	if (pte_write(pte) == 0) {
		unsigned long flags;
		DEFINE_SPINLOCK(mem_lock);

		spin_lock_irqsave(&mem_lock, flags);
		if (swap_set_memory_rw(page_addr, 1) == 0) {
			*maddr = val;
			swap_set_memory_ro(page_addr, 1);
		} else {
			printk(KERN_INFO "RWX: failed to write memory %08lx (%08lx)\n",
			       addr, val);
		}
		spin_unlock_irqrestore(&mem_lock, flags);
	} else {
		*maddr = val;
	}

	flush_icache_range(addr, addr + sizeof(long));
}

void mem_rwx_write_u32(unsigned long addr, unsigned long val)
{
	if (addr < MODULES_VADDR || addr >= MODULES_END) {
		/*
		 * if addr doesn't belongs kernel space,
		 * segmentation fault will occur
		 */
		mem_text_write_kernel_word((long *)addr, val);
	} else {
		write_to_module(addr, val);
	}
}

int mem_rwx_once(void)
{
	const char *sym;

	sym = "set_memory_ro";
	swap_set_memory_ro = (void *)swap_ksyms(sym);
	if (swap_set_memory_ro == NULL)
		goto not_found;

	sym = "set_memory_rw";
	swap_set_memory_rw = (void *)swap_ksyms(sym);
	if (swap_set_memory_rw == NULL)
		goto not_found;

	sym = "init_mm";
	swap_init_mm = (void *)swap_ksyms(sym);
	if (swap_init_mm == NULL)
		goto not_found;

	return 0;

not_found:
	printk(KERN_INFO "ERROR: symbol '%s' not found\n", sym);
	return -ESRCH;
}
