#ifndef __SSPT_DEBUG__
#define __SSPT_DEBUG__

/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_debug.h
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */

#include <kprobe/swap_kprobes_deps.h>
#include <us_manager/probes/probes.h>


static inline void print_retprobe(struct uretprobe *rp)
{
	printk(KERN_INFO "###         RP: handler=%lx\n",
			(unsigned long)rp->handler);
}

static inline void print_ip(struct sspt_ip *ip, int i)
{
	if (ip->desc->type == SWAP_RETPROBE) {
		struct uretprobe *rp = &ip->retprobe;

		printk(KERN_INFO "###       addr[%2d]=%lx, R_addr=%lx\n",
		       i, (unsigned long)ip->offset,
		       (unsigned long)rp->up.addr);
		print_retprobe(rp);
	}
}

static inline void print_page_probes(const struct sspt_page *page)
{
	int i = 0;
	struct sspt_ip *ip;

	printk(KERN_INFO "###     offset=%lx\n", page->offset);
	printk(KERN_INFO "###     no install:\n");
	list_for_each_entry(ip, &page->ip_list.not_inst, list) {
		print_ip(ip, i);
		++i;
	}

	printk(KERN_INFO "###     install:\n");
	list_for_each_entry(ip, &page->ip_list.inst, list) {
		print_ip(ip, i);
		++i;
	}
}

static inline void print_file_probes(struct sspt_file *file)
{
	int i;
	unsigned long table_size;
	struct sspt_page *page = NULL;
	struct hlist_head *head = NULL;
	static unsigned char *NA = "N/A";
	unsigned char *name;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	if (file == NULL) {
		printk(KERN_INFO "### file_p == NULL\n");
		return;
	}

	table_size = (1 << file->htable.bits);
	name = (file->dentry) ? file->dentry->d_iname : NA;

	printk(KERN_INFO "### print_file_probes: path=%s, d_iname=%s, "
	       "table_size=%lu, vm_start=%lx\n",
	       file->dentry->d_iname, name, table_size, file->vm_start);

	down_read(&file->htable.sem);
	for (i = 0; i < table_size; ++i) {
		head = &file->htable.heads[i];
		swap_hlist_for_each_entry_rcu(page, node, head, hlist) {
			print_page_probes(page);
		}
	}
	up_read(&file->htable.sem);
}

static inline void print_proc_probes(struct sspt_proc *proc)
{
	struct sspt_file *file;

	printk(KERN_INFO "### print_proc_probes\n");
	down_read(&proc->files.sem);
	list_for_each_entry(file, &proc->files.head, list) {
		print_file_probes(file);
	}
	up_read(&proc->files.sem);
	printk(KERN_INFO "### print_proc_probes\n");
}


#endif /* __SSPT_DEBUG__ */
