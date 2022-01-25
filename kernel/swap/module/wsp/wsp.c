/*
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
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
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 *
 * Web startup profiling
 */

#include <linux/string.h>
#include <us_manager/sspt/sspt.h>
#include <us_manager/probes/probe_info_new.h>
#include "wsp.h"
#include "wsp_res.h"
#include "wsp_msg.h"

struct wsp_probe {
	const char *name;
	struct probe_new probe;
};

struct wsp_bin {
	const char *name;
	unsigned long cnt;
	struct wsp_probe *probe_array;
};

static char *webapp_path;
static char *chromium_path;

#define WSP_PROBE_MAKE(_name_, _offset_,  _desc_)	\
{							\
	.name = (_name_),				\
	.probe.offset = (_offset_),			\
	.probe.desc = (_desc_)				\
}

/*
 * res_request
 */
/* blink::ResourceLoader.m_request.m_url */
#define URL_OFFSET		84
/* base::String.m_impl.m_ptr */
#define URL_LEN_OFFSET		4
#define URL_DATA_OFFSET		12

static char *path_get_from_object(unsigned long ptr)
{
	char *path;
	unsigned long url, len, ret;

	get_user(url, (unsigned long __user *)(ptr + URL_OFFSET));
	get_user(len, (unsigned long __user *)(url + URL_LEN_OFFSET));
	path = kzalloc(len + 1, GFP_KERNEL);
	if (!path)
		return NULL;

	ret = copy_from_user(path,
			     (const void __user *)(url + URL_DATA_OFFSET),
			     len);
	if (ret) {
		kfree(path);
		path = NULL;
	} else {
		path[len] = '\0';
	}

	return path;
}

static int res_request_handle(struct uprobe *p, struct pt_regs *regs)
{
	unsigned long ptr;
	char *path;

	ptr = (unsigned long)swap_get_uarg(regs, 0);
	path = path_get_from_object(ptr);
	if (path) {
		int id = wsp_resource_data_add(ptr, path);
		if (id >= 0)
			wsp_msg(WSP_RES_LOAD_BEGIN, id, path);
	}

	return 0;
}

static struct probe_desc res_request = MAKE_UPROBE(res_request_handle);

/*
 * res_finish
 */
static int res_finish_ehandle(struct uretprobe_instance *ri,
			      struct pt_regs *regs)
{
	int id;
	unsigned long ptr = (unsigned long)swap_get_uarg(regs, 0);

	id = wsp_resource_data_id(ptr);
	if (id >= 0) {
		*(unsigned long *)ri->data = ptr;
		wsp_msg(WSP_RES_PROC_BEGIN, id, NULL);
	}

	return 0;
}

static int res_finish_rhandle(struct uretprobe_instance *ri,
			      struct pt_regs *regs)
{
	int id;
	unsigned long ptr;

	ptr = *(unsigned long *)ri->data;
	id = wsp_resource_data_id(ptr);
	if (id >= 0) {
		wsp_msg(WSP_RES_PROC_END, id, NULL);
		wsp_msg(WSP_RES_LOAD_END, id, NULL);
		wsp_resource_data_del(ptr);
	}

	return 0;
}

static struct probe_desc res_finish =
		MAKE_URPROBE(res_finish_ehandle, res_finish_rhandle,
			     sizeof(unsigned long));

/*
 * redraw
 */
static int redraw_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	wsp_msg(WSP_DRAW_BEGIN, 0, NULL);

	return 0;
}

static int redraw_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	wsp_msg(WSP_DRAW_END, 0, NULL);

	return 0;
}

static struct probe_desc redraw = MAKE_URPROBE(redraw_eh, redraw_rh, 0);

/* blink::ResourceLoader::start() */
#define RES_REQ "_ZN5blink14ResourceLoader5startEv"
/* blink::ResourceLoader::didFinishLoading(WebURLLoader*, double , int64_t) */
#define RES_FINISH "_ZN5blink14ResourceLoader16didFinishLoadingEPNS_12WebURLLoaderEdx"

/* content::RenderWidget::DidCommitAndDrawCompositorFrame */
#define REDRAW "_ZN7content23CompositorOutputSurface11SwapBuffersEPN2cc15CompositorFrameE"

static struct wsp_probe __probe_array[] = {
	/* res */
	WSP_PROBE_MAKE(RES_REQ, 0, &res_request),
	WSP_PROBE_MAKE(RES_FINISH, 0, &res_finish),

	/* redraw */
	WSP_PROBE_MAKE(REDRAW, 0, &redraw),
};

static struct wsp_bin chromium_bin = {
	.name = NULL,
	.probe_array = __probe_array,
	.cnt = ARRAY_SIZE(__probe_array)
};

/* check chromium_bin array on init address */
static bool wsp_is_addr_init(void)
{
	int i;

	for (i = 0; i < chromium_bin.cnt; ++i)
		if (chromium_bin.probe_array[i].probe.offset == 0)
			return false;

	return true;
}

static int wsp_bin_register(struct pf_group *pfg, struct wsp_bin *bin)
{
	int i, ret;
	struct dentry *dentry;

	dentry = dentry_by_path(bin->name);
	if (!dentry) {
		pr_err("dentry not found (path='%s'\n", bin->name);
		return -EINVAL;
	}

	for (i = 0; i < bin->cnt; ++i) {
		struct wsp_probe *p = &bin->probe_array[i];

		ret = pin_register(&p->probe, pfg, dentry);
		if (ret) {
			pr_err("failed to register WSP probe (%lx:%d)\n",
			       p->probe.offset, ret);
			return ret;
		}
	}

	return 0;
}

static void wsp_bin_unregister(struct pf_group *pfg, struct wsp_bin *bin)
{
	int i;
	struct dentry *dentry;

	dentry = dentry_by_path(bin->name);
	if (!dentry) {
		pr_err("dentry not found (path='%s'\n", bin->name);
		return;
	}

	for (i = 0; i < bin->cnt; ++i) {
		struct wsp_probe *p = &bin->probe_array[i];

		pin_unregister(&p->probe, pfg);
	}
}

static char *do_set_path(char *path, size_t len)
{
	char *p;

	p = kmalloc(len, GFP_KERNEL);
	if (!p)
		return NULL;

	strncpy(p, path, len);
	return p;
}

static void do_free_path(char **dest)
{
	kfree(*dest);
	*dest = NULL;
}

static struct pf_group *g_pfg;

static int wsp_app_register(void)
{
	struct dentry *dentry;

	if (!webapp_path || !chromium_path) {
		pr_err("WSP: some required paths are not set!\n");
		return -EINVAL;
	}

	chromium_bin.name = chromium_path;

	dentry = dentry_by_path(webapp_path);
	if (!dentry) {
		pr_err("dentry not found (path='%s'\n", webapp_path);
		return -EINVAL;
	}

	g_pfg = get_pf_group_by_dentry(dentry, (void *)dentry);
	if (!g_pfg) {
		pr_err("WSP: g_pfg is NULL (by dentry=%p)\n", dentry);
		return -ENOMEM;
	}

	return wsp_bin_register(g_pfg, &chromium_bin);
}

static void wsp_app_unregister(void)
{
	if (!chromium_bin.name) {
		pr_err("WSP: chromium path is not initialized\n");
		return;
	}

	wsp_bin_unregister(g_pfg, &chromium_bin);
	put_pf_group(g_pfg);
}

static int do_wsp_on(void)
{
	int ret;

	ret = wsp_res_init();
	if (ret)
		return ret;

	ret = wsp_app_register();
	if (ret)
		wsp_res_exit();

	return ret;
}

static int do_wsp_off(void)
{
	wsp_app_unregister();
	wsp_res_exit();

	return 0;
}

static enum wsp_mode g_mode = WSP_OFF;
static DEFINE_MUTEX(g_mode_mutex);

int wsp_set_addr(const char *name, unsigned long offset)
{
	int i, ret = 0;

	if (mutex_trylock(&g_mode_mutex) == 0)
		return -EBUSY;

	for (i = 0; i < chromium_bin.cnt; ++i) {
		if (!strcmp(name, chromium_bin.probe_array[i].name)) {
			chromium_bin.probe_array[i].probe.offset = offset;
			goto unlock;
		}
	}

	ret = -EINVAL;

unlock:
	mutex_unlock(&g_mode_mutex);
	return ret;
}

int wsp_set_mode(enum wsp_mode mode)
{
	int ret = -EINVAL;

	if (g_mode == mode)
		return -EBUSY;

	mutex_lock(&g_mode_mutex);
	switch (mode) {
	case WSP_ON:
		ret = wsp_is_addr_init() ? do_wsp_on() : -EPERM;
		break;
	case WSP_OFF:
		ret = do_wsp_off();
		break;
	}

	if (!ret)
		g_mode = mode;

	mutex_unlock(&g_mode_mutex);
	return ret;
}

enum wsp_mode wsp_get_mode(void)
{
	return g_mode;
}

void wsp_set_webapp_path(char *path, size_t len)
{
	do_free_path(&webapp_path);
	webapp_path = do_set_path(path, len);
}

void wsp_set_chromium_path(char *path, size_t len)
{
	do_free_path(&chromium_path);
	chromium_path = do_set_path(path, len);
}

int wsp_init(void)
{
	return 0;
}

void wsp_exit(void)
{
	wsp_set_mode(WSP_OFF);
	do_free_path(&webapp_path);
	do_free_path(&chromium_path);
}
