/**
 * webprobe/webprobe.c
 * @author Ruslan Soloviev
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2014
 *
 * @section DESCRIPTION
 *
 * Web application profiling
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <us_manager/us_manager.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/probes/probe_info_new.h>
#include <uprobe/swap_uprobes.h>
#include <parser/msg_cmd.h>
#include <master/swap_initializer.h>

#include "webprobe.h"
#include "webprobe_debugfs.h"
#include "web_msg.h"

struct web_prof_data {
	struct dentry *app_dentry;
	struct dentry *lib_dentry;
	struct pf_group *pfg;
	u64 inspserver_addr;
	u64 tick_addr;

	struct probe_new inspserver_probe;
	struct probe_new tick_probe;

	bool enable;
};

static DEFINE_MUTEX(web_mutex);
static const char *CHROMIUM_EWK = "/usr/lib/libchromium-ewk.so";
static struct web_prof_data *web_data;

/* function tick handler */
static int tick_handler(struct uprobe *p, struct pt_regs *regs);
static struct probe_desc pin_tick_handler = MAKE_UPROBE(tick_handler);

/* function inspector port */
static int insport_rhandler(struct uretprobe_instance *ri,
			    struct pt_regs *regs);
static struct probe_desc pin_insport_rhandler =
				MAKE_URPROBE(NULL, insport_rhandler, 0);

static int insport_rhandler(struct uretprobe_instance *ri,
			    struct pt_regs *regs)
{
	set_wrt_launcher_port((int)regs_return_value(regs));

	return 0;
}

static int tick_handler(struct uprobe *p, struct pt_regs *regs)
{
	web_sample_msg(regs);

	return 0;
}

u64 *web_prof_addr_ptr(enum web_prof_addr_t type)
{
	u64 *addr_ptr;

	mutex_lock(&web_mutex);
	switch (type) {
	case INSPSERVER_START:
		addr_ptr = &web_data->inspserver_addr;
		break;
	case TICK_PROBE:
		addr_ptr = &web_data->tick_addr;
		break;
	default:
		pr_err("ERROR: WEB_PROF_ADDR_PTR_TYPE=0x%x\n", type);
		addr_ptr = NULL;
	}
	mutex_unlock(&web_mutex);

	return addr_ptr;
}

int web_prof_data_set(char *app_path, char *app_id)
{
	int ret = 0;

	mutex_lock(&web_mutex);
	web_data->app_dentry = dentry_by_path(app_path);
	if (!web_data->app_dentry) {
		ret = -EFAULT;
		goto out;
	}

	web_data->lib_dentry = dentry_by_path(CHROMIUM_EWK);
	if (!web_data->lib_dentry) {
		ret = -EFAULT;
		goto out;
	}

	if (web_data->pfg) {
		put_pf_group(web_data->pfg);
		web_data->pfg = NULL;
	}

	web_data->pfg = get_pf_group_by_comm(app_id, web_data->app_dentry);
	if (!web_data->pfg) {
		ret = -EFAULT;
		goto out;
	}

out:
	mutex_unlock(&web_mutex);

	return 0;
}

bool web_prof_enabled(void)
{
	bool ret;

	mutex_lock(&web_mutex);
	ret = web_data->enable;
	mutex_unlock(&web_mutex);

	return ret;
}

static void __web_prof_disable(struct web_prof_data *data)
{
	pin_unregister(&data->tick_probe, data->pfg);
	pin_unregister(&data->inspserver_probe, data->pfg);
}

static int __web_prof_enable(struct web_prof_data *data)
{
	int ret;

	data->tick_probe.offset = (unsigned long)data->tick_addr;
	data->tick_probe.desc = &pin_tick_handler;
	ret = pin_register(&data->tick_probe, data->pfg, data->lib_dentry);
	if (ret)
		goto fail0;

	data->inspserver_probe.offset = (unsigned long)data->inspserver_addr;
	data->inspserver_probe.desc = &pin_insport_rhandler;
	ret = pin_register(&data->inspserver_probe, data->pfg,
			   data->lib_dentry);
	if (ret)
		goto fail1;

	return 0;

fail1:
	pin_unregister(&data->tick_probe, data->pfg);
fail0:
	return ret;
}

int web_prof_enable(void)
{
	int ret = 0;

	mutex_lock(&web_mutex);
	if (web_data->enable) {
		pr_err("ERROR: Web profiling is already enabled\n");
		ret = -EBUSY;
		goto out;
	}

	if (!web_data->inspserver_addr) {
		pr_err("bad inspserver addr 0x%llx\n",
		       web_data->inspserver_addr);
		goto out;
	}

	if (!web_data->tick_addr) {
		pr_err("bad tick addr 0x%llx\n", web_data->tick_addr);
		goto out;
	}

	ret = __web_prof_enable(web_data);
	if (ret) {
		pr_err("failed to enable Web profiling\n");
		goto out;
	}

	web_data->enable = true;

out:
	mutex_unlock(&web_mutex);

	return ret;
}

int web_prof_disable(void)
{
	int ret = 0;

	mutex_lock(&web_mutex);
	if (!web_data->enable) {
		pr_err("ERROR: Web profiling is already disabled\n");
		ret = -EBUSY;
		goto out;
	}

	__web_prof_disable(web_data);
	if (web_data->pfg) {
		put_pf_group(web_data->pfg);
		web_data->pfg = NULL;
	}
	web_data->enable = false;

out:
	mutex_unlock(&web_mutex);
	return ret;
}

static int webprobe_module_init(void)
{
	mutex_lock(&web_mutex);
	web_data = kzalloc(sizeof(*web_data), GFP_KERNEL);
	if (!web_data)
		return -ENOMEM;

	web_data->enable = false;
	mutex_unlock(&web_mutex);

	return 0;
}

static void webprobe_module_exit(void)
{
	mutex_lock(&web_mutex);
	if (web_data->enable)
		__web_prof_disable(web_data);

	if (web_data->pfg) {
		put_pf_group(web_data->pfg);
		web_data->pfg = NULL;
	}

	kfree(web_data);
	web_data = NULL;
	mutex_unlock(&web_mutex);
}

SWAP_LIGHT_INIT_MODULE(NULL, webprobe_module_init, webprobe_module_exit,
		       webprobe_debugfs_init, webprobe_debugfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP webprobe");
MODULE_AUTHOR("Ruslan Soloviev <r.soloviev@samsung.com>"
	      "Anastasia Lyupa <a.lyupa@samsung.com>");
