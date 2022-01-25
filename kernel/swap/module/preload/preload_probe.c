/*
 *  SWAP uprobe manager
 *  modules/us_manager/probes/preload_probe.c
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
 * 2014	 Alexander Aksenov: Preload implement
 *
 */

#include <linux/module.h>
#include <us_manager/us_manager.h>
#include <us_manager/probes/register_probes.h>
#include <us_manager/sspt/sspt_page.h>
#include <uprobe/swap_uprobes.h>
#include <us_manager/sspt/sspt_ip.h>
#include "preload_probe.h"
#include "preload.h"
#include "preload_module.h"

static int preload_info_copy(struct probe_info *dest,
			      const struct probe_info *source)
{
	memcpy(dest, source, sizeof(*source));

	return 0;
}

static void preload_info_cleanup(struct probe_info *probe_i)
{
}

static struct uprobe *preload_get_uprobe(struct sspt_ip *ip)
{
	return &ip->retprobe.up;
}

/* Registers probe if preload is 'running' or 'ready'.
 */
static int preload_register_probe(struct sspt_ip *ip)
{
	return swap_register_uretprobe(&ip->retprobe);
}

static void preload_unregister_probe(struct sspt_ip *ip, int disarm)
{
	__swap_unregister_uretprobe(&ip->retprobe, disarm);
}

static void preload_init(struct sspt_ip *ip)
{
	pm_uprobe_init(ip);
}

static void preload_uninit(struct sspt_ip *ip)
{
	pm_uprobe_exit(ip);

	preload_info_cleanup(&ip->desc->info);
}

static struct probe_iface preload_iface = {
	.init = preload_init,
	.uninit = preload_uninit,
	.reg = preload_register_probe,
	.unreg = preload_unregister_probe,
	.get_uprobe = preload_get_uprobe,
	.copy = preload_info_copy,
	.cleanup = preload_info_cleanup
};

static int get_caller_info_copy(struct probe_info *dest,
				const struct probe_info *source)
{
	memcpy(dest, source, sizeof(*source));

	return 0;
}

static void get_caller_info_cleanup(struct probe_info *probe_i)
{
}

static struct uprobe *get_caller_get_uprobe(struct sspt_ip *ip)
{
	return &ip->uprobe;
}

static int get_caller_register_probe(struct sspt_ip *ip)
{
	return swap_register_uprobe(&ip->uprobe);
}

static void get_caller_unregister_probe(struct sspt_ip *ip, int disarm)
{
	__swap_unregister_uprobe(&ip->uprobe, disarm);
}

static void get_caller_init(struct sspt_ip *ip)
{
	pm_get_caller_init(ip);
}

static void get_caller_uninit(struct sspt_ip *ip)
{
	pm_get_caller_exit(ip);

	get_caller_info_cleanup(&ip->desc->info);
}

static struct probe_iface get_caller_iface = {
	.init = get_caller_init,
	.uninit = get_caller_uninit,
	.reg = get_caller_register_probe,
	.unreg = get_caller_unregister_probe,
	.get_uprobe = get_caller_get_uprobe,
	.copy = get_caller_info_copy,
	.cleanup = get_caller_info_cleanup
};

static void get_call_type_init(struct sspt_ip *ip)
{
	pm_get_call_type_init(ip);
}

static void get_call_type_uninit(struct sspt_ip *ip)
{
	pm_get_call_type_exit(ip);

	get_caller_info_cleanup(&ip->desc->info);
}

static struct probe_iface get_call_type_iface = {
	.init = get_call_type_init,
	.uninit = get_call_type_uninit,
	.reg = get_caller_register_probe,
	.unreg = get_caller_unregister_probe,
	.get_uprobe = get_caller_get_uprobe,
	.copy = get_caller_info_copy,
	.cleanup = get_caller_info_cleanup
};

static void write_msg_init(struct sspt_ip *ip)
{
	pm_write_msg_init(ip);
}

static int write_msg_reg(struct sspt_ip *ip)
{
	return get_caller_register_probe(ip);
}

static void write_msg_uninit(struct sspt_ip *ip)
{
	pm_write_msg_exit(ip);

	get_caller_info_cleanup(&ip->desc->info);
}

static struct probe_iface write_msg_iface = {
	.init = write_msg_init,
	.uninit = write_msg_uninit,
	.reg = write_msg_reg,
	.unreg = get_caller_unregister_probe,
	.get_uprobe = get_caller_get_uprobe,
	.copy = get_caller_info_copy,
	.cleanup = get_caller_info_cleanup
};

int register_preload_probes(void)
{
	int ret;

	ret = swap_register_probe_type(SWAP_PRELOAD_PROBE, &preload_iface);
	if (ret != 0)
		return ret;

	ret = swap_register_probe_type(SWAP_GET_CALLER, &get_caller_iface);
	if (ret != 0)
		return ret;

	ret = swap_register_probe_type(SWAP_GET_CALL_TYPE, &get_call_type_iface);
	if (ret != 0)
		return ret;

	ret = swap_register_probe_type(SWAP_WRITE_MSG, &write_msg_iface);

	return ret;
}

void unregister_preload_probes(void)
{
	swap_unregister_probe_type(SWAP_PRELOAD_PROBE);
	swap_unregister_probe_type(SWAP_GET_CALLER);
	swap_unregister_probe_type(SWAP_GET_CALL_TYPE);
	swap_unregister_probe_type(SWAP_WRITE_MSG);
}
