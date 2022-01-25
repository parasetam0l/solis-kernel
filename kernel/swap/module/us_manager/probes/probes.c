/*
 *  SWAP uprobe manager
 *  modules/us_manager/probes/probes.c
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

#include "probes.h"
#include "register_probes.h"
#include "use_probes.h"

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>

static struct probe_iface *probes_methods[SWAP_PROBE_MAX_VAL] = { NULL };

/* 1 - correct probe type
   0 - wrong probe type
*/
static inline int correct_probe_type(enum probe_t probe_type)
{
	if (probe_type >= SWAP_PROBE_MAX_VAL)
		return 0;

	return 1;
}

static inline int methods_exist(enum probe_t probe_type)
{
	if (!correct_probe_type(probe_type) ||
	    (probes_methods[probe_type] == NULL)) {
		printk(KERN_WARNING "SWAP US_MANAGER: Wrong probe type!\n");
		return 0;
	}

	return 1;
}

/**
 * @brief Calls specified probe type init method.
 *
 * @param pi Pointer to the probe_info.
 * @param ip Pointer to the probe us_ip struct.
 * @return Void.
 */
void probe_info_init(enum probe_t type, struct sspt_ip *ip)
{
	if (!methods_exist(type)) {
		return;
	}

	probes_methods[type]->init(ip);
}

/**
 * @brief Calls specified probe type uninit method.
 *
 * @param pi Pointer to the probe_info.
 * @param ip Pointer to the probe us_ip struct.
 * @return Void.
 */
void probe_info_uninit(enum probe_t type, struct sspt_ip *ip)
{
	if (!methods_exist(type)) {
		return;
	}

	probes_methods[type]->uninit(ip);
}

/**
 * @brief Calls specified probe type register method.
 *
 * @param pi Pointer to the probe_info.
 * @param ip Pointer to the probe us_ip struct.
 * @return -EINVAL on wrong probe type, method result otherwise.
 */
int probe_info_register(enum probe_t type, struct sspt_ip *ip)
{
	if (!methods_exist(type)) {
		return -EINVAL;
	}

	return probes_methods[type]->reg(ip);
}

/**
 * @brief Calls specified probe type unregister method.
 *
 * @param pi Pointer to the probe_info.
 * @param ip Pointer to the probe us_ip struct.
 * @param disarm Disarm flag.
 * @return Void.
 */
void probe_info_unregister(enum probe_t type, struct sspt_ip *ip, int disarm)
{
	if (!methods_exist(type)) {
		return;
	}

	probes_methods[type]->unreg(ip, disarm);
}

/**
 * @brief Calls specified probe type get underlying uprobe method.
 *
 * @param pi Pointer to the probe_info.
 * @param ip Pointer to the probe us_ip struct.
 * @return Pointer to the uprobe struct, NULL on error.
 */
struct uprobe *probe_info_get_uprobe(enum probe_t type, struct sspt_ip *ip)
{
	if (!methods_exist(type)) {
		return NULL;
	}

	return probes_methods[type]->get_uprobe(ip);
}

/**
 * @brief Registers probe type.
 *
 * @param probe_type Number, associated with this probe type.
 * @param pi Pointer to the probe interface structure
 * @return 0 on succes, error code on error.
 */
int swap_register_probe_type(enum probe_t probe_type, struct probe_iface *pi)
{
	if (!correct_probe_type(probe_type)) {
		printk(KERN_ERR "SWAP US_MANAGER: incorrect probe type!\n");
		return -EINVAL;
	}

	if (probes_methods[probe_type] != NULL)
		printk(KERN_WARNING "SWAP US_MANAGER: Re-registering probe %d\n",
		   probe_type);

	probes_methods[probe_type] = pi;

	return 0;
}
EXPORT_SYMBOL_GPL(swap_register_probe_type);

/**
 * @brief Unregisters probe type.
 *
 * @param probe_type Probe type that should be unregistered.
 * @return Void.
 */
void swap_unregister_probe_type(enum probe_t probe_type)
{
	if (!correct_probe_type(probe_type)) {
		printk(KERN_ERR "SWAP US_MANAGER: incorrect probe type!\n");
		return;
	}

	probes_methods[probe_type] = NULL;
}
EXPORT_SYMBOL_GPL(swap_unregister_probe_type);
