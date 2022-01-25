/*
 * @file fbiprobe/fbi_probe.c
 *
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
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
 *
 * 2014 Alexander Aksenov : FBI implement
 * 2014 Vitaliy Cherepanov: FBI implement, portage
 *
 * @section DESCRIPTION
 *
 * Function body instrumetation
 *
 */

#include "fbiprobe.h"
#include "fbi_probe_module.h"
#include "fbi_msg.h"
#include "regs.h"

#include <us_manager/us_manager.h>
#include <us_manager/probes/probes.h>
#include <us_manager/probes/register_probes.h>

#include <uprobe/swap_uprobes.h>
#include <us_manager/sspt/sspt_ip.h>

#include <kprobe/swap_kprobes_deps.h>
#include <linux/module.h>

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/dcache.h>
#include <linux/mm_types.h>

#include <us_manager/sspt/sspt_page.h>
#include <us_manager/sspt/sspt_file.h>

#define DIRECT_ADDR (0xFF)
#define MAX_STRING_LEN (512)

/* on fails. return NULL, set size to 0 */
/* you shoud free allocated data buffer */
static char *fbi_probe_alloc_and_read_from_addr(const struct fbi_var_data *fbid,
						unsigned long addr,
						uint32_t *size)
{
	uint8_t i, j;
	char *buf = NULL;
	struct fbi_step *step;

	*size = 0;

	/* get final variable address */
	step = fbid->steps;
	for (i = 0; i != fbid->steps_count; i++) {
		/* dereference */
		for (j = 0; j != step->ptr_order; j++) {
			unsigned long new_addr;
			/* equel to: addr = *addr */
			if (!read_proc_vm_atomic(current, addr, &new_addr,
						 sizeof(new_addr))) {
				print_warn("p = 0x%lx step #%d ptr_order #%d\n",
					   addr, i + 1, j + 1);
				goto exit_fail;
			}
			addr = new_addr;
			print_debug("dereference addr = 0x%lx;\n", addr);
		}

		/* offset */
		addr += step->data_offset;
		print_debug("addr + offset = 0x%lx;\n", addr);
		step++;
	}

	/* calculate data size */
	if (fbid->data_size == 0) {
		/*
		 * that mean variable is string and
		 * we need to calculate string length
		 */

		*size = strnlen_user((const char __user *)addr, MAX_STRING_LEN);
		if (*size == 0) {
			print_warn("Cannot get string from 0x%lx\n", addr);
			goto exit_fail;
		}
	} else {
		/* else use size from fbi struct */
		*size = fbid->data_size;
	}

	buf = kmalloc(*size, GFP_KERNEL);
	if (buf == NULL) {
		print_warn("Not enough memory\n");
		goto exit_fail_size_0;
	}

	if (!read_proc_vm_atomic(current, addr, buf, *size)) {
		print_warn("Error reading data at 0x%lx, task %d\n",
			   addr, current->pid);
		goto exit_fail_free_buf;
	}

	if (fbid->data_size == 0) {
		/*
		 * that mean variable is string and
		 * we need to add terminate '\0'
		 */
		buf[*size - 1] = '\0';
	}

	return buf;

exit_fail_free_buf:
	kfree(buf);
	buf = NULL;
exit_fail_size_0:
	*size = 0;
exit_fail:
	return NULL;

}

static int fbi_probe_get_data_from_reg(const struct fbi_var_data *fbi_i,
				       struct pt_regs *regs)
{
	unsigned long *reg_ptr;

	reg_ptr = get_ptr_by_num(regs, fbi_i->reg_n);
	if (reg_ptr == NULL) {
		print_err("fbi_probe_get_data_from_reg: Wrong register number!\n");
		return 0;
	}

	fbi_msg(fbi_i->var_id, fbi_i->data_size, (char *)reg_ptr);

	return 0;
}

static int fbi_probe_get_data_from_ptrs(const struct fbi_var_data *fbi_i,
					struct pt_regs *regs)
{
	unsigned long *reg_ptr;
	unsigned long addr;
	uint32_t size = 0;
	void *buf = NULL;

	reg_ptr = get_ptr_by_num(regs, fbi_i->reg_n);
	if (reg_ptr == NULL) {
		print_err("fbi_probe_get_data_from_ptrs: Wrong register number!\n");
		goto send_msg;
	}

	addr = *reg_ptr + fbi_i->reg_offset;
	print_warn("reg = %p; off = 0x%llx; addr = 0x%lx!\n", reg_ptr,
		   fbi_i->reg_offset, addr);

	buf = fbi_probe_alloc_and_read_from_addr(fbi_i, addr, &size);

send_msg:
	/* If buf is NULL size will be 0.
	 * That mean we cannot get data for this probe.
	 * But we should send probe message with packed data size 0
	 * as error message.
	 */
	fbi_msg(fbi_i->var_id, size, buf);

	if (buf != NULL)
		kfree(buf);
	else
		print_err("cannot get data from ptrs\n");

	return 0;
}

static struct vm_area_struct *find_vma_exe_by_dentry(struct mm_struct *mm,
						     struct dentry *dentry)
{
	struct vm_area_struct *vma;

	/* FIXME: down_write(&mm->mmap_sem); up_write(&mm->mmap_sem); */
	/* TODO FILTER vma */
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (vma->vm_file &&
		   (vma->vm_file->f_path.dentry == dentry))
			/* found */
			goto exit;
	}

	/* not found */
	vma = NULL;
exit:
	return vma;
}

static int fbi_probe_get_data_from_direct_addr(const struct fbi_var_data *fbi_i,
					       struct sspt_ip *ip,
					       struct pt_regs *regs)
{
	struct vm_area_struct *vma;
	unsigned long addr;
	uint32_t size = 0;
	char *buf;

	/* register offset is global address */
	vma = find_vma_exe_by_dentry(current->mm, ip->page->file->dentry);
	if (vma == NULL) {
		print_warn("cannot locate dentry\n");
		goto exit;
	}

	addr = vma->vm_start + fbi_i->reg_offset;

	print_debug("DIRECT_ADDR reg_offset = %llx\n", fbi_i->reg_offset);
	print_debug("DIRECT_ADDR vm_start   = %lx\n", vma->vm_start);
	print_debug("DIRECT_ADDR res_addr   = %lx\n", addr);

	buf = fbi_probe_alloc_and_read_from_addr(fbi_i, addr, &size);
	/* If buf is NULL size will be 0.
	 * That mean we cannot get data for this probe.
	 * But we should send probe message with packed data size 0
	 * as error message.
	 */
	fbi_msg(fbi_i->var_id, size, buf);

	if (buf != NULL) {
		kfree(buf);
	} else {
		print_warn("get data by direct addr failed (0x%lx :0x%llx)\n",
			   addr, fbi_i->reg_offset);
	}
exit:
	return 0;
}

static int fbi_probe_handler(struct uprobe *p, struct pt_regs *regs)
{
	struct sspt_ip *ip = container_of(p, struct sspt_ip, uprobe);
	struct fbi_info *fbi_i = &ip->desc->info.fbi_i;
	struct fbi_var_data *fbi_d = NULL;
	uint8_t i;

	if (ip->desc->type != SWAP_FBIPROBE) {
		/* How this can occure? Doesn't matter, just print and go */
		print_err("Not FBI probe in FBI handler!\n");
		return 0;
	}

	for (i = 0; i != fbi_i->var_count; i++) {
		fbi_d = &fbi_i->vars[i];
		if (fbi_d->reg_n == DIRECT_ADDR) {
			if (0 != fbi_probe_get_data_from_direct_addr(fbi_d, ip,
								     regs))
				print_err("fbi_probe_get_data_from_direct_addr error\n");
		} else if (fbi_d->steps_count == 0) {
			if (0 != fbi_probe_get_data_from_reg(fbi_d, regs))
				print_err("fbi_probe_get_data_from_reg error\n");
		} else {
			if (0 != fbi_probe_get_data_from_ptrs(fbi_d, regs))
				print_err("fbi_probe_get_data_from_ptrs error\n");
		}
	}

	return 0;
}

/* FBI probe interfaces */
void fbi_probe_cleanup(struct probe_info *probe_i)
{
	uint8_t i;
	struct fbi_info *fbi_i = &(probe_i->fbi_i);

	for (i = 0; i != fbi_i->var_count; i++) {
		if (fbi_i->vars[i].steps != NULL) {
			if (fbi_i->vars[i].steps != NULL)
				kfree(fbi_i->vars[i].steps);
			fbi_i->vars[i].steps = NULL;
			fbi_i->vars[i].steps_count = 0;
		}
	}

	kfree(fbi_i->vars);
	fbi_i->vars = NULL;
}

void fbi_probe_init(struct sspt_ip *ip)
{
	ip->uprobe.pre_handler = (uprobe_pre_handler_t)fbi_probe_handler;
}

void fbi_probe_uninit(struct sspt_ip *ip)
{
	if (ip != NULL)
		fbi_probe_cleanup(&ip->desc->info);
}

static int fbi_probe_register_probe(struct sspt_ip *ip)
{
	return swap_register_uprobe(&ip->uprobe);
}

static void fbi_probe_unregister_probe(struct sspt_ip *ip, int disarm)
{
	__swap_unregister_uprobe(&ip->uprobe, disarm);
}

static struct uprobe *fbi_probe_get_uprobe(struct sspt_ip *ip)
{
	return &ip->uprobe;
}

int fbi_probe_copy(struct probe_info *dest, const struct probe_info *source)
{
	uint8_t steps_count;
	size_t steps_size;
	size_t vars_size;
	struct fbi_var_data *vars;
	struct fbi_step *steps_source;
	struct fbi_step *steps_dest = NULL;
	uint8_t i, n;
	int ret = 0;

	memcpy(dest, source, sizeof(*source));

	vars_size = source->fbi_i.var_count * sizeof(*source->fbi_i.vars);
	vars = kmalloc(vars_size, GFP_KERNEL);
	if (vars == NULL)
		return -ENOMEM;

	memcpy(vars, source->fbi_i.vars, vars_size);

	for (i = 0; i != source->fbi_i.var_count; i++) {
		steps_dest = NULL;
		steps_count = vars[i].steps_count;
		steps_size = sizeof(*steps_source) * steps_count;
		steps_source = vars[i].steps;

		if (steps_size != 0 && steps_source != NULL) {
			steps_dest = kmalloc(steps_size, GFP_KERNEL);
			if (steps_dest == NULL) {
				print_err("can not alloc data\n");
				n = i;
				ret = -ENOMEM;
				goto err;
			}

			memcpy(steps_dest, steps_source, steps_size);
		}
		vars[i].steps = steps_dest;
	}

	dest->fbi_i.vars = vars;

	return ret;
err:
	for (i = 0; i < n; i++)
		kfree(vars[i].steps);
	kfree(vars);
	return ret;
}

/* Register */
static struct probe_iface fbi_probe_iface = {
	.init = fbi_probe_init,
	.uninit = fbi_probe_uninit,
	.reg = fbi_probe_register_probe,
	.unreg = fbi_probe_unregister_probe,
	.get_uprobe = fbi_probe_get_uprobe,
	.copy = fbi_probe_copy,
	.cleanup = fbi_probe_cleanup
};

static int __init fbiprobe_module_init(void)
{
	int ret = 0;
	ret = swap_register_probe_type(SWAP_FBIPROBE, &fbi_probe_iface);
	print_debug("Init done. Result=%d\n", ret);
	return ret;
}

static void __exit fbiprobe_module_exit(void)
{
	swap_unregister_probe_type(SWAP_FBIPROBE);
}

module_init(fbiprobe_module_init);
module_exit(fbiprobe_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP fbiprobe");
MODULE_AUTHOR("Alexander Aksenov <a.aksenov@samsung.com>; Vitaliy Cherepanov <v.cherepanov@samsung.com>");

