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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/module.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/sspt/sspt_proc.h>
#include "probes.h"
#include "probe_info_new.h"
#include "register_probes.h"


/*
 * handlers
 */
static int urp_entry_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp) {
		struct sspt_ip *ip = container_of(rp, struct sspt_ip, retprobe);
		struct probe_desc *pd = NULL;

		pd = ip->desc;
		if (pd && pd->u.rp.entry_handler)
			return pd->u.rp.entry_handler(ri, regs);

	}

	return 0;
}

static int urp_ret_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp) {
		struct sspt_ip *ip = container_of(rp, struct sspt_ip, retprobe);
		struct probe_desc *pd = NULL;

		pd = ip->desc;
		if (pd && pd->u.rp.ret_handler)
			return pd->u.rp.ret_handler(ri, regs);
	}

	return 0;
}

static int uprobe_handler(struct uprobe *p, struct pt_regs *regs)
{
	struct sspt_ip *ip = container_of(p, struct sspt_ip, uprobe);
	struct probe_desc *pd = NULL;

	pd = ip->desc;
	if (pd && pd->u.p.handler)
		return pd->u.p.handler(p, regs);

	return 0;
}

/*
 * register/unregister interface
 */
int pin_register(struct probe_new *probe, struct pf_group *pfg,
		 struct dentry *dentry)
{
	struct img_ip *ip;

	ip = pf_register_probe(pfg, dentry, probe->offset, probe->desc);
	if (IS_ERR(ip)) {
		pr_err("%s: register probe failed\n", __func__);
		return PTR_ERR(ip);
	}

	probe->priv = ip;
	return 0;
}
EXPORT_SYMBOL_GPL(pin_register);

void pin_unregister(struct probe_new *probe, struct pf_group *pfg)
{
	struct img_ip *ip = probe->priv;

	pf_unregister_probe(pfg, ip);
}
EXPORT_SYMBOL_GPL(pin_unregister);





/*
 * SWAP_NEW_UP
 */
static int up_copy(struct probe_info *dst, const struct probe_info *src)
{
	return 0;
}

static void up_cleanup(struct probe_info *probe_i)
{
}

static struct uprobe *up_get_uprobe(struct sspt_ip *ip)
{
	return &ip->uprobe;
}

static int up_register_probe(struct sspt_ip *ip)
{
	return swap_register_uprobe(&ip->uprobe);
}

static void up_unregister_probe(struct sspt_ip *ip, int disarm)
{
	__swap_unregister_uprobe(&ip->uprobe, disarm);
}

static void up_init(struct sspt_ip *ip)
{
	ip->uprobe.pre_handler = uprobe_handler;
}

static void up_uninit(struct sspt_ip *ip)
{
}

static struct probe_iface up_iface = {
	.init = up_init,
	.uninit = up_uninit,
	.reg = up_register_probe,
	.unreg = up_unregister_probe,
	.get_uprobe = up_get_uprobe,
	.copy = up_copy,
	.cleanup = up_cleanup
};





/*
 * SWAP_NEW_URP
 */
static int urp_copy(struct probe_info *dst, const struct probe_info *src)
{
	return 0;
}

static void urp_cleanup(struct probe_info *probe_i)
{
}

static struct uprobe *urp_get_uprobe(struct sspt_ip *ip)
{
	return &ip->retprobe.up;
}

static int urp_register_probe(struct sspt_ip *ip)
{
	return swap_register_uretprobe(&ip->retprobe);
}

static void urp_unregister_probe(struct sspt_ip *ip, int disarm)
{
	__swap_unregister_uretprobe(&ip->retprobe, disarm);
}

static void urp_init(struct sspt_ip *ip)
{
	ip->retprobe.entry_handler = urp_entry_handler;
	ip->retprobe.handler = urp_ret_handler;
	ip->retprobe.maxactive = 0;
	/* FIXME: make dynamic size field 'data_size' */
#ifdef CONFIG_ARM64
	/*
	 * Loader module use field uretprobe_instance.data for storing
	 * 'struct us_priv'. For ARM64 it requires much more space.
	 */
	ip->retprobe.data_size = 512 - sizeof(struct uretprobe_instance);
#else /* CONFIG_ARM64 */
	ip->retprobe.data_size = 128;
#endif /* CONFIG_ARM64 */
}

static void urp_uninit(struct sspt_ip *ip)
{
}

static struct probe_iface urp_iface = {
	.init = urp_init,
	.uninit = urp_uninit,
	.reg = urp_register_probe,
	.unreg = urp_unregister_probe,
	.get_uprobe = urp_get_uprobe,
	.copy = urp_copy,
	.cleanup = urp_cleanup
};




/*
 * init/exit()
 */
int pin_init(void)
{
	int ret;

	ret = swap_register_probe_type(SWAP_NEW_UP, &up_iface);
	if (ret)
		return ret;

	ret = swap_register_probe_type(SWAP_NEW_URP, &urp_iface);
	if (ret)
		swap_unregister_probe_type(SWAP_NEW_UP);

	return ret;
}

void pin_exit(void)
{
	swap_unregister_probe_type(SWAP_NEW_URP);
	swap_unregister_probe_type(SWAP_NEW_UP);
}
