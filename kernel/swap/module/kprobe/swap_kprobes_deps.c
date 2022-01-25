/**
 * kprobe/swap_kprobes_deps.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
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
 * @section DESCRIPTION
 *
 * SWAP kprobe kernel-dependent dependencies.
 */

#include <linux/module.h>
#include <linux/sched.h>

#include <asm/pgtable.h>

#include "swap_kprobes_deps.h"
#include "swap_kdebug.h"


#include <linux/slab.h>
#include <linux/mm.h>

/* kernel define 'pgd_offset_k' redefinition */
#undef pgd_offset_k
#define pgd_offset_k(addr)	pgd_offset(init_task.active_mm, addr)

#ifndef is_zero_pfn

static unsigned long swap_zero_pfn ;

#endif /* is_zero_pfn */

static inline void *swap_kmap_atomic(struct page *page)
{
	return kmap_atomic(page);
}
static inline void swap_kunmap_atomic(void *kvaddr)
{
	kunmap_atomic(kvaddr);
}

DECLARE_MOD_FUNC_DEP(do_mmap_pgoff, unsigned long, struct file *file,
		     unsigned long addr, unsigned long len, unsigned long prot,
		     unsigned long flags, unsigned long pgoff,
		     unsigned long *populate);
DECLARE_MOD_DEP_WRAPPER(swap_do_mmap_pgoff,
			unsigned long,
			struct file *file, unsigned long addr,
			unsigned long len, unsigned long prot,
			unsigned long flags, unsigned long pgoff,
			unsigned long *populate)
IMP_MOD_DEP_WRAPPER(do_mmap_pgoff, file, addr, len,
		    prot, flags, pgoff, populate)

EXPORT_SYMBOL_GPL(swap_do_mmap_pgoff);

/* copy_to_user_page */
#ifndef copy_to_user_page
static DECLARE_MOD_FUNC_DEP(copy_to_user_page, void, struct vm_area_struct *vma,
			    struct page *page, unsigned long uaddr, void *dst,
			    const void *src, unsigned long len);
DECLARE_MOD_DEP_WRAPPER(swap_copy_to_user_page,
			void,
			struct vm_area_struct *vma, struct page *page,
			unsigned long uaddr, void *dst, const void *src,
			unsigned long len)
IMP_MOD_DEP_WRAPPER(copy_to_user_page, vma, page, uaddr, dst, src, len)
#else /* copy_to_user_page */
#define swap_copy_to_user_page copy_to_user_page
#endif /* copy_to_user_page */


static DECLARE_MOD_FUNC_DEP(find_extend_vma, struct vm_area_struct *,
			    struct mm_struct *mm, unsigned long addr);

static DECLARE_MOD_FUNC_DEP(handle_mm_fault, int, struct mm_struct *mm,
			    struct vm_area_struct *vma, unsigned long address,
			    unsigned int flags);

#ifdef __HAVE_ARCH_GATE_AREA
static DECLARE_MOD_FUNC_DEP(get_gate_vma, struct vm_area_struct *,
			    struct mm_struct *mm);

DECLARE_MOD_FUNC_DEP(in_gate_area, int, struct mm_struct *mm,
		     unsigned long addr);

static DECLARE_MOD_FUNC_DEP(in_gate_area_no_mm, int, unsigned long addr);
#endif /* __HAVE_ARCH_GATE_AREA */

static DECLARE_MOD_FUNC_DEP(follow_page_mask, \
		struct page *, struct vm_area_struct *vma, \
		unsigned long address, unsigned int foll_flags, \
		unsigned int *page_mask);
DECLARE_MOD_DEP_WRAPPER(swap_follow_page_mask,
			struct page *,
			struct vm_area_struct *vma, unsigned long address,
			unsigned int foll_flags, unsigned int *page_mask)
IMP_MOD_DEP_WRAPPER(follow_page_mask, vma, address, foll_flags, page_mask)

static DECLARE_MOD_FUNC_DEP(__flush_anon_page, \
		void, struct vm_area_struct *vma, struct page *page, \
		unsigned long vmaddr);
static DECLARE_MOD_FUNC_DEP(vm_normal_page, \
		struct page *, struct vm_area_struct *vma, \
		unsigned long addr, pte_t pte);


static DECLARE_MOD_FUNC_DEP(put_task_struct, \
		void, struct task_struct *tsk);

DECLARE_MOD_DEP_WRAPPER(swap_find_extend_vma,
			struct vm_area_struct *,
			struct mm_struct *mm, unsigned long addr)
IMP_MOD_DEP_WRAPPER(find_extend_vma, mm, addr)

DECLARE_MOD_DEP_WRAPPER(swap_handle_mm_fault,
			int,
			struct mm_struct *mm, struct vm_area_struct *vma,
			unsigned long address, unsigned int flags)
{
	if (in_atomic())
		return VM_FAULT_ERROR | VM_FAULT_OOM;

	IMP_MOD_DEP_WRAPPER(handle_mm_fault, mm, vma, address, flags)
}

struct vm_area_struct *swap_get_gate_vma(struct mm_struct *mm)
{
#ifdef __HAVE_ARCH_GATE_AREA
IMP_MOD_DEP_WRAPPER(get_gate_vma, mm)
#else /* __HAVE_ARCH_GATE_AREA */
	return get_gate_vma(mm);
#endif /* __HAVE_ARCH_GATE_AREA */
}

#ifdef CONFIG_HUGETLB_PAGE

DECLARE_MOD_FUNC_DEP(follow_hugetlb_page,				\
		     long,						\
		     struct mm_struct *mm, struct vm_area_struct *vma,	\
		     struct page **pages, struct vm_area_struct **vmas,	\
		     unsigned long *position, unsigned long *nr_pages,	\
		     long i, unsigned int flags);
DECLARE_MOD_DEP_WRAPPER(swap_follow_hugetlb_page,
			long,
			struct mm_struct *mm, struct vm_area_struct *vma,
			struct page **pages, struct vm_area_struct **vmas,
			unsigned long *position, unsigned long *nr_pages,
			long i, unsigned int flags)
IMP_MOD_DEP_WRAPPER(follow_hugetlb_page,				\
		    mm, vma, pages, vmas, position, nr_pages, i, flags)

#else /* CONFIG_HUGETLB_PAGE */
#define swap_follow_hugetlb_page follow_hugetlb_page
#endif /* CONFIG_HUGETLB_PAGE */

static inline int swap_in_gate_area(struct task_struct *task,
				    unsigned long addr)
{
#ifdef __HAVE_ARCH_GATE_AREA
	struct mm_struct *mm;

	if (task == NULL)
		return 0;

	mm = task->mm;
	IMP_MOD_DEP_WRAPPER(in_gate_area, mm, addr)
#else /*__HAVE_ARCH_GATE_AREA */
	struct mm_struct *mm;

	if (task == NULL)
		return 0;

	mm = task->mm;
	return in_gate_area(mm, addr);
#endif/*__HAVE_ARCH_GATE_AREA */
}


#ifdef __HAVE_ARCH_GATE_AREA
DECLARE_MOD_DEP_WRAPPER(swap_in_gate_area_no_mm, int, unsigned long addr)
IMP_MOD_DEP_WRAPPER(in_gate_area_no_mm, addr)
#endif /* __HAVE_ARCH_GATE_AREA */

static inline int swap_in_gate_area_no_xxx(unsigned long addr)
{
#ifdef __HAVE_ARCH_GATE_AREA
	return swap_in_gate_area_no_mm(addr);
#else /* __HAVE_ARCH_GATE_AREA */
	return in_gate_area_no_mm(addr);
#endif /* __HAVE_ARCH_GATE_AREA */
}

DECLARE_MOD_DEP_WRAPPER(swap__flush_anon_page,
			void,
			struct vm_area_struct *vma, struct page *page,
			unsigned long vmaddr)
IMP_MOD_DEP_WRAPPER(__flush_anon_page, vma, page, vmaddr)

static inline void swap_flush_anon_page(struct vm_area_struct *vma,
					struct page *page,
					unsigned long vmaddr)
{
#if defined(ARCH_HAS_FLUSH_ANON_PAGE) && defined(CONFIG_ARM)
	if (PageAnon(page))
		swap__flush_anon_page(vma, page, vmaddr);
#else /* defined(ARCH_HAS_FLUSH_ANON_PAGE) && defined(CONFIG_ARM) */
	flush_anon_page(vma, page, vmaddr);
#endif /* defined(ARCH_HAS_FLUSH_ANON_PAGE) && defined(CONFIG_ARM) */
}

DECLARE_MOD_DEP_WRAPPER(swap_vm_normal_page,
			struct page *,
			struct vm_area_struct *vma, unsigned long addr,
			pte_t pte)
IMP_MOD_DEP_WRAPPER(vm_normal_page, vma, addr, pte)



/**
 * @brief Initializes module dependencies.
 *
 * @return 0.
 */
int init_module_dependencies(void)
{

	INIT_MOD_DEP_VAR(handle_mm_fault, handle_mm_fault);

#ifndef copy_to_user_page
	INIT_MOD_DEP_VAR(copy_to_user_page, copy_to_user_page);
#endif /* copy_to_user_page */

	INIT_MOD_DEP_VAR(find_extend_vma, find_extend_vma);

#ifdef CONFIG_HUGETLB_PAGE
	INIT_MOD_DEP_VAR(follow_hugetlb_page, follow_hugetlb_page);
#endif

#ifdef	__HAVE_ARCH_GATE_AREA
	INIT_MOD_DEP_VAR(in_gate_area, in_gate_area);
	INIT_MOD_DEP_VAR(get_gate_vma, get_gate_vma);

	INIT_MOD_DEP_VAR(in_gate_area_no_mm, in_gate_area_no_mm);
#endif

	INIT_MOD_DEP_VAR(follow_page_mask, follow_page_mask);


#ifndef is_zero_pfn
	swap_zero_pfn = page_to_pfn(ZERO_PAGE(0));
#endif /* is_zero_pfn */

#if defined(ARCH_HAS_FLUSH_ANON_PAGE) && defined(CONFIG_ARM)
	INIT_MOD_DEP_VAR(__flush_anon_page, __flush_anon_page);
#endif /* defined(ARCH_HAS_FLUSH_ANON_PAGE) && defined(CONFIG_ARM) */

	INIT_MOD_DEP_VAR(vm_normal_page, vm_normal_page);

	INIT_MOD_DEP_VAR(put_task_struct, __put_task_struct);
	INIT_MOD_DEP_VAR(do_mmap_pgoff, do_mmap_pgoff);

	return 0;
}


#ifdef CONFIG_ARM64

static int do_access_process_vm(struct task_struct *tsk, struct mm_struct *mm,
				unsigned long addr, void *buf, int len,
				int write)
{
	struct vm_area_struct *vma;
	void *old_buf = buf;

	while (len) {
		int bytes, ret, offset;
		void *maddr;
		struct page *page = NULL;

		ret = get_user_pages(tsk, mm, addr, 1, write, 1, &page, &vma);
		if (ret <= 0) {
#ifndef CONFIG_HAVE_IOREMAP_PROT
			break;
#else
			/*
			 * Check if this is a VM_IO | VM_PFNMAP VMA, which
			 * we can access using slightly different code.
			 */
			vma = find_vma(mm, addr);
			if (!vma || vma->vm_start > addr)
				break;
			if (vma->vm_ops && vma->vm_ops->access)
				ret = vma->vm_ops->access(vma, addr, buf, len,
							  write);
			if (ret <= 0)
				break;
			bytes = ret;
#endif
		} else {
			bytes = len;
			offset = addr & (PAGE_SIZE-1);
			if (bytes > PAGE_SIZE-offset)
				bytes = PAGE_SIZE-offset;

			maddr = kmap(page);
			if (write) {
				swap_copy_to_user_page(vma, page, addr,
						       maddr + offset,
						       buf, bytes);
				set_page_dirty_lock(page);
			} else {
				copy_from_user_page(vma, page, addr,
						    buf, maddr + offset, bytes);
			}
			kunmap(page);
			page_cache_release(page);
		}
		len -= bytes;
		buf += bytes;
		addr += bytes;
	}

	return buf - old_buf;
}

int swap_access_process_vm(struct task_struct *tsk, unsigned long addr,
			   void *buf, int len, int write)
{
	int ret;
	struct mm_struct *mm;

	mm = get_task_mm(tsk);
	if (!mm)
		return 0;

	ret = do_access_process_vm(tsk, mm, addr, buf, len, write);
	mmput(mm);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_access_process_vm);

#else /* CONFIG_ARM64 */


static inline int use_zero_page(struct vm_area_struct *vma)
{
	/*
	 * We don't want to optimize FOLL_ANON for make_pages_present()
	 * when it tries to page in a VM_LOCKED region. As to VM_SHARED,
	 * we want to get the page from the page tables to make sure
	 * that we serialize and update with any other user of that
	 * mapping.
	 */
	if (vma->vm_flags & (VM_LOCKED | VM_SHARED))
		return 0;
	/*
	 * And if we have a fault routine, it's not an anonymous region.
	 */
	return !vma->vm_ops || !vma->vm_ops->fault;
}



#ifdef __HAVE_COLOR_ZERO_PAGE

static inline int swap_is_zero_pfn(unsigned long pfn)
{
	unsigned long offset_from_zero_pfn = pfn - swap_zero_pfn;
	return offset_from_zero_pfn <= (zero_page_mask >> PAGE_SHIFT);
}

#else /* __HAVE_COLOR_ZERO_PAGE */

static inline int swap_is_zero_pfn(unsigned long pfn)
{
	return pfn == swap_zero_pfn;
}
#endif /* __HAVE_COLOR_ZERO_PAGE */


static inline int stack_guard_page(struct vm_area_struct *vma,
				   unsigned long addr)
{
	return stack_guard_page_start(vma, addr) ||
			stack_guard_page_end(vma, addr+PAGE_SIZE);
}


/**
 * @brief Gets user pages uprobe.
 *
 * @param tsk Pointer to the task_struct.
 * @param mm Pointer to the mm_struct.
 * @param start Starting address.
 * @param nr_pages Pages number.
 * @param gup_flags Flags.
 * @param pages Pointer to the array of pointers to the target page structs.
 * @param vmas Pointer to the array of pointers to the target vm_area_struct.
 * @param nonblocking Pointer to int.
 * @return negative error code on error, positive result otherwise.
 */
long __get_user_pages_uprobe(struct task_struct *tsk, struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages,
		unsigned int gup_flags, struct page **pages,
		struct vm_area_struct **vmas, int *nonblocking)
{
	long i;
	unsigned long vm_flags;
	unsigned int page_mask;

	if (!nr_pages)
		return 0;

	VM_BUG_ON(!!pages != !!(gup_flags & FOLL_GET));

	/*
	 * Require read or write permissions.
	 * If FOLL_FORCE is set, we only require the "MAY" flags.
	 */
	vm_flags  = (gup_flags & FOLL_WRITE) ?
			(VM_WRITE | VM_MAYWRITE) : (VM_READ | VM_MAYREAD);
	vm_flags &= (gup_flags & FOLL_FORCE) ?
			(VM_MAYREAD | VM_MAYWRITE) : (VM_READ | VM_WRITE);

	/*
	 * If FOLL_FORCE and FOLL_NUMA are both set, handle_mm_fault
	 * would be called on PROT_NONE ranges. We must never invoke
	 * handle_mm_fault on PROT_NONE ranges or the NUMA hinting
	 * page faults would unprotect the PROT_NONE ranges if
	 * _PAGE_NUMA and _PAGE_PROTNONE are sharing the same pte/pmd
	 * bitflag. So to avoid that, don't set FOLL_NUMA if
	 * FOLL_FORCE is set.
	 */
	if (!(gup_flags & FOLL_FORCE))
		gup_flags |= FOLL_NUMA;

	i = 0;

	do {
		struct vm_area_struct *vma;

		vma = swap_find_extend_vma(mm, start);
		if (!vma && swap_in_gate_area(tsk, start)) {
			unsigned long pg = start & PAGE_MASK;
			pgd_t *pgd;
			pud_t *pud;
			pmd_t *pmd;
			pte_t *pte;

			/* user gate pages are read-only */
			if (gup_flags & FOLL_WRITE)
				return i ? : -EFAULT;
			if (pg > TASK_SIZE)
				pgd = pgd_offset_k(pg);
			else
				pgd = pgd_offset_gate(mm, pg);
			BUG_ON(pgd_none(*pgd));
			pud = pud_offset(pgd, pg);
			BUG_ON(pud_none(*pud));
			pmd = pmd_offset(pud, pg);
			if (pmd_none(*pmd))
				return i ? : -EFAULT;
			VM_BUG_ON(pmd_trans_huge(*pmd));
			pte = pte_offset_map(pmd, pg);
			if (pte_none(*pte)) {
				pte_unmap(pte);
				return i ? : -EFAULT;
			}
			vma = swap_get_gate_vma(mm);
			if (pages) {
				struct page *page;

				page = swap_vm_normal_page(vma, start, *pte);
				if (!page) {
					if (!(gup_flags & FOLL_DUMP) &&
					     swap_is_zero_pfn(pte_pfn(*pte)))
						page = pte_page(*pte);
					else {
						pte_unmap(pte);
						return i ? : -EFAULT;
					}
				}
				pages[i] = page;
				get_page(page);
			}
			pte_unmap(pte);
			page_mask = 0;
			goto next_page;
		}

		if (!vma ||
		    (vma->vm_flags & (VM_IO | VM_PFNMAP)) ||
		    !(vm_flags & vma->vm_flags))
			return i ? : -EFAULT;

		if (is_vm_hugetlb_page(vma)) {
			i = swap_follow_hugetlb_page(mm, vma, pages, vmas,
					&start, &nr_pages, i, gup_flags);
			continue;
		}

		do {
			struct page *page;
			unsigned int foll_flags = gup_flags;
			unsigned int page_increm;

			/*
			 * If we have a pending SIGKILL, don't keep faulting
			 * pages and potentially allocating memory.
			 */
			if (unlikely(fatal_signal_pending(current)))
				return i ? i : -ERESTARTSYS;

			/* cond_resched(); */
			while (!(page = swap_follow_page_mask(vma, start,
						foll_flags, &page_mask))) {
				int ret;
				unsigned int fault_flags = 0;

				/* For mlock, just skip the stack guard page. */
				if (foll_flags & FOLL_MLOCK) {
					if (stack_guard_page(vma, start))
						goto next_page;
				}
				if (foll_flags & FOLL_WRITE)
					fault_flags |= FAULT_FLAG_WRITE;
				if (nonblocking)
					fault_flags |= FAULT_FLAG_ALLOW_RETRY;
				if (foll_flags & FOLL_NOWAIT)
					fault_flags |=
						(FAULT_FLAG_ALLOW_RETRY |
						 FAULT_FLAG_RETRY_NOWAIT);

				ret = swap_handle_mm_fault(mm, vma, start,
							fault_flags);

				if (ret & VM_FAULT_ERROR) {
					if (ret & VM_FAULT_OOM)
						return i ? i : -ENOMEM;
					if (ret & (VM_FAULT_HWPOISON |
						   VM_FAULT_HWPOISON_LARGE)) {
						if (i)
							return i;
						else if (gup_flags &
							 FOLL_HWPOISON)
							return -EHWPOISON;
						else
							return -EFAULT;
					}
					if (ret & VM_FAULT_SIGBUS)
						return i ? i : -EFAULT;
					BUG();
				}

				if (tsk) {
					if (ret & VM_FAULT_MAJOR)
						tsk->maj_flt++;
					else
						tsk->min_flt++;
				}

				if (ret & VM_FAULT_RETRY) {
					if (nonblocking)
						*nonblocking = 0;
					return i;
				}

				/*
				 * The VM_FAULT_WRITE bit tells us that
				 * do_wp_page has broken COW when necessary,
				 * even if maybe_mkwrite decided not to set
				 * pte_write. We can thus safely do subsequent
				 * page lookups as if they were reads. But only
				 * do so when looping for pte_write is futile:
				 * in some cases userspace may also be wanting
				 * to write to the gotten user page, which a
				 * read fault here might prevent (a readonly
				 * page might get reCOWed by userspace write).
				 */
				if ((ret & VM_FAULT_WRITE) &&
				    !(vma->vm_flags & VM_WRITE))
					foll_flags &= ~FOLL_WRITE;

				/* cond_resched(); */
			}
			if (IS_ERR(page))
				return i ? i : PTR_ERR(page);
			if (pages) {
				pages[i] = page;

				swap_flush_anon_page(vma, page, start);
				flush_dcache_page(page);
				page_mask = 0;
			}
next_page:
			if (vmas) {
				vmas[i] = vma;
				page_mask = 0;
			}
			page_increm = 1 + (~(start >> PAGE_SHIFT) & page_mask);
			if (page_increm > nr_pages)
				page_increm = nr_pages;
			i += page_increm;
			start += page_increm * PAGE_SIZE;
			nr_pages -= page_increm;
		} while (nr_pages && start < vma->vm_end);
	} while (nr_pages);
	return i;
}



/**
 * @brief Gets user pages uprobe.
 *
 * @param tsk Pointer to the task_struct.
 * @param mm Pointer to the mm_struct.
 * @param start Starting address.
 * @param len Length.
 * @param write Write flag.
 * @param force Force flag.
 * @param pages Pointer to the array of pointers to the target page structs.
 * @param vmas Pointer to the array of pointers to the target vm_area_struct.
 * @return negative error code on error, positive result otherwise.
 */
int get_user_pages_uprobe(struct task_struct *tsk, struct mm_struct *mm,
		unsigned long start, int len, int write, int force,
		struct page **pages, struct vm_area_struct **vmas)
{
	int flags = FOLL_TOUCH;

	if (pages)
		flags |= FOLL_GET;
	if (write)
		flags |= FOLL_WRITE;
	if (force)
		flags |= FOLL_FORCE;

	return __get_user_pages_uprobe(tsk, mm,
				start, len, flags,
						pages, vmas, NULL);
}

#define ACCESS_PROCESS_OPTIMIZATION 0

#if ACCESS_PROCESS_OPTIMIZATION

#define GET_STEP_X(LEN, STEP) (((LEN) >= (STEP)) ? (STEP) : (LEN) % (STEP))
#define GET_STEP_4(LEN) GET_STEP_X((LEN), 4)

static void read_data_current(unsigned long addr, void *buf, int len)
{
	int step;
	int pos = 0;

	for (step = GET_STEP_4(len); len; len -= step) {
		switch (GET_STEP_4(len)) {
		case 1:
			get_user(*(u8 *)(buf + pos),
				 (unsigned long *)(addr + pos));
			step = 1;
			break;

		case 2:
		case 3:
			get_user(*(u16 *)(buf + pos),
				 (unsigned long *)(addr + pos));
			step = 2;
			break;

		case 4:
			get_user(*(u32 *)(buf + pos),
				 (unsigned long *)(addr + pos));
			step = 4;
			break;
		}

		pos += step;
	}
}

/* not working */
static void write_data_current(unsigned long addr, void *buf, int len)
{
	int step;
	int pos = 0;

	for (step = GET_STEP_4(len); len; len -= step) {
		switch (GET_STEP_4(len)) {
		case 1:
			put_user(*(u8 *)(buf + pos),
				 (unsigned long *)(addr + pos));
			step = 1;
			break;

		case 2:
		case 3:
			put_user(*(u16 *)(buf + pos),
				 (unsigned long *)(addr + pos));
			step = 2;
			break;

		case 4:
			put_user(*(u32 *)(buf + pos),
				 (unsigned long *)(addr + pos));
			step = 4;
			break;
		}

		pos += step;
	}
}
#endif

/**
 * @brief Read-write task memory.
 *
 * @param tsk Pointer to the target task task_struct.
 * @param addr Address to read-write.
 * @param buf Pointer to buffer where to put-get data.
 * @param len Buffer length.
 * @param write Write flag. If 0 - reading, if 1 - writing.
 * @return Read-write size, error code on error.
 */
int access_process_vm_atomic(struct task_struct *tsk, unsigned long addr,
			     void *buf, int len, int write)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	void *old_buf = buf;
	int atomic;

	if (len <= 0)
		return -1;

#if ACCESS_PROCESS_OPTIMIZATION
	if (write == 0 && tsk == current) {
		read_data_current(addr, buf, len);
		return len;
	}
#endif

	mm = tsk->mm; /* function 'get_task_mm' is to be called */
	if (!mm)
		return 0;

	/* FIXME: danger: write memory in atomic context */
	atomic = in_atomic();

	/* ignore errors, just check how much was successfully transferred */
	while (len) {
		int bytes, ret, offset;
		void *maddr;
		struct page *page = NULL;

		ret = get_user_pages_uprobe(tsk, mm, addr, 1,
						write, 1, &page, &vma);

		if (ret <= 0) {
			/*
			 * Check if this is a VM_IO | VM_PFNMAP VMA, which
			 * we can access using slightly different code.
			 */
#ifdef CONFIG_HAVE_IOREMAP_PROT
			vma = find_vma(mm, addr);
			if (!vma)
				break;
			if (vma->vm_ops && vma->vm_ops->access)
				ret = vma->vm_ops->access(vma, addr, buf,
							len, write);
			if (ret <= 0)
#endif
				break;
			bytes = ret;
		} else {
			bytes = len;
			offset = addr & (PAGE_SIZE-1);
			if (bytes > PAGE_SIZE-offset)
				bytes = PAGE_SIZE-offset;

			maddr = atomic ? swap_kmap_atomic(page) : kmap(page);

			if (write) {
				swap_copy_to_user_page(vma, page, addr,
							maddr + offset,
						       buf, bytes);
				set_page_dirty_lock(page);
			} else {
				copy_from_user_page(vma, page, addr,
						    buf, maddr + offset,
						    bytes);
			}

			atomic ? swap_kunmap_atomic(maddr) : kunmap(page);
			page_cache_release(page);
		}
		len -= bytes;
		buf += bytes;
		addr += bytes;
	}

	return buf - old_buf;
}
EXPORT_SYMBOL_GPL(access_process_vm_atomic);

#endif /* CONFIG_ARM64 */

/**
 * @brief Page present.
 *
 * @param mm Pointer to the target mm_struct.
 * @param address Address.
 */
int page_present(struct mm_struct *mm, unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	unsigned long pfn;

	pgd = pgd_offset(mm, address);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto out;

	pud = pud_offset(pgd, address);
	if (pud_none(*pud) || unlikely(pud_bad(*pud)))
		goto out;

	pmd = pmd_offset(pud, address);
	if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd)))
		goto out;

	ptep = pte_offset_map(pmd, address);
	if (pte_none(*ptep)) {
		pte_unmap(ptep);
		goto out;
	}

	pte = *ptep;
	pte_unmap(ptep);
	if (pte_present(pte)) {
		pfn = pte_pfn(pte);
		if (pfn_valid(pfn))
			return 1;
	}

out:
	return 0;
}
EXPORT_SYMBOL_GPL(page_present);

