/*
 *  SWAP uprobe manager
 *  modules/retprobe/retprobe.c
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014	 Alexander Aksenov: Probes interface implement
 *
 */

#include "retprobe.h"
#include <us_manager/us_manager.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/probes/register_probes.h>
#include <uprobe/swap_uprobes.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "rp_msg.h"


static int retprobe_copy(struct probe_info *dest,
			 const struct probe_info *source)
{
	size_t len;

	memcpy(dest, source, sizeof(*source));

	len = strlen(source->rp_i.args) + 1;
	dest->rp_i.args = kmalloc(len, GFP_ATOMIC);
	if (dest->rp_i.args == NULL)
		return -ENOMEM;
	memcpy(dest->rp_i.args, source->rp_i.args, len);

	return 0;
}


static void retprobe_cleanup(struct probe_info *probe_i)
{
	kfree(probe_i->rp_i.args);
}



static struct uprobe *retprobe_get_uprobe(struct sspt_ip *ip)
{
	return &ip->retprobe.up;
}

static int retprobe_register_probe(struct sspt_ip *ip)
{
	return swap_register_uretprobe(&ip->retprobe);
}

static void retprobe_unregister_probe(struct sspt_ip *ip, int disarm)
{
	__swap_unregister_uretprobe(&ip->retprobe, disarm);
}


static int retprobe_entry_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp && get_quiet() == QT_OFF) {
		struct sspt_ip *ip = container_of(rp, struct sspt_ip, retprobe);
		const char *fmt = ip->desc->info.rp_i.args;
		const unsigned long func_addr = (unsigned long)ip->orig_addr;

		rp_msg_entry(regs, func_addr, fmt);
	}

	return 0;
}

static int retprobe_ret_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp && get_quiet() == QT_OFF) {
		struct sspt_ip *ip = container_of(rp, struct sspt_ip, retprobe);
		const unsigned long func_addr = (unsigned long)ip->orig_addr;
		const unsigned long ret_addr = (unsigned long)ri->ret_addr;
		const char ret_type = ip->desc->info.rp_i.ret_type;

		rp_msg_exit(regs, func_addr, ret_type, ret_addr);
	}

	return 0;
}

static void retprobe_init(struct sspt_ip *ip)
{
	ip->retprobe.entry_handler = retprobe_entry_handler;
	ip->retprobe.handler = retprobe_ret_handler;
	ip->retprobe.maxactive = 0;
}

static void retprobe_uninit(struct sspt_ip *ip)
{
	retprobe_cleanup(&ip->desc->info);
}


static struct probe_iface retprobe_iface = {
	.init = retprobe_init,
	.uninit = retprobe_uninit,
	.reg = retprobe_register_probe,
	.unreg = retprobe_unregister_probe,
	.get_uprobe = retprobe_get_uprobe,
	.copy = retprobe_copy,
	.cleanup = retprobe_cleanup
};

static int __init retprobe_module_init(void)
{
	return swap_register_probe_type(SWAP_RETPROBE, &retprobe_iface);
}

static void __exit retprobe_module_exit(void)
{
	swap_unregister_probe_type(SWAP_RETPROBE);
}

module_init(retprobe_module_init);
module_exit(retprobe_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP retprobe");
MODULE_AUTHOR("Alexander Aksenov <a.aksenov@samsung.com>");
