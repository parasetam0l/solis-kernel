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
#include <writer/swap_msg.h>
#include <kprobe/swap_ktd.h>
#include <uprobe/swap_uaccess.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/sspt/sspt_proc.h>
#include <us_manager/probes/probe_info_new.h>
#include "nsp.h"
#include "nsp_msg.h"
#include "nsp_print.h"
#include "nsp_debugfs.h"


/* ============================================================================
 * =                                 probes                                   =
 * ============================================================================
 */

/* dlopen@plt */
static int dlopen_eh(struct uretprobe_instance *ri, struct pt_regs *regs);
static int dlopen_rh(struct uretprobe_instance *ri, struct pt_regs *regs);
static struct probe_desc pin_dlopen = MAKE_URPROBE(dlopen_eh, dlopen_rh, 0);
static struct probe_new p_dlopen = {
	.desc = &pin_dlopen
};

/* dlsym@plt */
static int dlsym_eh(struct uretprobe_instance *ri, struct pt_regs *regs);
static int dlsym_rh(struct uretprobe_instance *ri, struct pt_regs *regs);
static struct probe_desc pin_dlsym = MAKE_URPROBE(dlsym_eh, dlsym_rh, 0);
static struct probe_new p_dlsym = {
	.desc = &pin_dlsym
};

/* main */
static int main_eh(struct uretprobe_instance *ri, struct pt_regs *regs);
static int main_rh(struct uretprobe_instance *ri, struct pt_regs *regs);
static struct probe_desc pin_main = MAKE_URPROBE(main_eh, main_rh, 0);

/* appcore_efl_main */
static int ac_efl_main_h(struct uprobe *p, struct pt_regs *regs);
static struct probe_desc pin_ac_efl_main = MAKE_UPROBE(ac_efl_main_h);
static struct probe_new p_ac_efl_main = {
	.desc = &pin_ac_efl_main
};

/* appcore_init@plt */
static int ac_init_rh(struct uretprobe_instance *ri, struct pt_regs *regs);
static struct probe_desc pin_ac_init = MAKE_URPROBE(NULL, ac_init_rh, 0);
static struct probe_new p_ac_init = {
	.desc = &pin_ac_init
};

/* elm_run@plt */
static int elm_run_h(struct uprobe *p, struct pt_regs *regs);
static struct probe_desc pin_elm_run = MAKE_UPROBE(elm_run_h);
static struct probe_new p_elm_run = {
	.desc = &pin_elm_run
};

/* __do_app */
static int do_app_eh(struct uretprobe_instance *ri, struct pt_regs *regs);
static int do_app_rh(struct uretprobe_instance *ri, struct pt_regs *regs);
static struct probe_desc pin_do_app = MAKE_URPROBE(do_app_eh, do_app_rh, 0);
static struct probe_new p_do_app = {
	.desc = &pin_do_app
};





/* ============================================================================
 * =                the variables are initialized by the user                 =
 * ============================================================================
 */
static const char *lpad_path;
static struct dentry *lpad_dentry;

static const char *libappcore_path;
static struct dentry *libappcore_dentry;
static const char *libcapi_path;
static struct dentry *libcapi_dentry;

static void uninit_variables(void)
{
	kfree(lpad_path);
	lpad_path = NULL;
	lpad_dentry = NULL;

	kfree(libappcore_path);
	libappcore_path = NULL;
	libappcore_dentry = NULL;

	kfree(libcapi_path);
	libcapi_path = NULL;
	libcapi_dentry = NULL;
}

static bool is_init(void)
{
	return lpad_dentry && libappcore_dentry && libcapi_dentry;
}

static int do_set_lpad_info(const char *path, unsigned long dlopen,
			    unsigned long dlsym)
{
	struct dentry *dentry;
	const char *new_path;

	dentry = dentry_by_path(path);
	if (dentry == NULL) {
		pr_err("dentry not found (path='%s')\n", path);
		return -EINVAL;
	}

	new_path = kstrdup(path, GFP_KERNEL);
	if (new_path == NULL) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}

	kfree(lpad_path);

	lpad_path = new_path;
	lpad_dentry = dentry;
	p_dlopen.offset = dlopen;
	p_dlsym.offset = dlsym;

	return 0;
}

static int do_set_appcore_info(struct nsp_info_data *info)
{
	struct dentry *dentry;
	const char *new_path;
	int ret = 0;

	new_path = kstrdup(info->appcore_path, GFP_KERNEL);
	if (!new_path)
		return -ENOMEM;
	kfree(libappcore_path);
	libappcore_path = new_path;

	new_path = kstrdup(info->capi_path, GFP_KERNEL);
	if (!new_path) {
		ret = -ENOMEM;
		goto fail_alloc;
	}
	kfree(libcapi_path);
	libcapi_path = new_path;

	dentry = dentry_by_path(info->appcore_path);
	if (!dentry) {
		pr_err("dentry not found (path='%s')\n", info->appcore_path);
		ret = -EINVAL;
		goto fail;
	}
	libappcore_dentry = dentry;

	dentry = dentry_by_path(info->capi_path);
	if (!dentry) {
		pr_err("dentry not found (path='%s')\n", info->capi_path);
		ret = -EINVAL;
		goto fail;
	}
	libcapi_dentry = dentry;

	p_ac_efl_main.offset = info->ac_efl_init;
	p_do_app.offset = info->do_app;
	p_ac_init.offset = info->ac_init;
	p_elm_run.offset = info->elm_run;

	return 0;

fail:
	kfree(libcapi_path);
	libcapi_path = NULL;
fail_alloc:
	kfree(libappcore_path);
	libappcore_path = NULL;

	return ret;
}





/* ============================================================================
 * =                                nsp_data                                  =
 * ============================================================================
 */
struct nsp_data {
	struct list_head list;

	const char *app_path;
	struct dentry *app_dentry;
	struct probe_new p_main;

	struct pf_group *pfg;
};

static LIST_HEAD(nsp_data_list);

static struct nsp_data *nsp_data_create(const char *app_path,
					unsigned long main_addr)
{
	struct dentry *dentry;
	struct nsp_data *data;

	dentry = dentry_by_path(app_path);
	if (dentry == NULL)
		return ERR_PTR(-ENOENT);

	data = kmalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL)
		return ERR_PTR(-ENOMEM);

	data->app_path = kstrdup(app_path, GFP_KERNEL);
	if (data->app_path == NULL) {
		kfree(data);
		return ERR_PTR(-ENOMEM);
	}

	data->app_dentry = dentry;
	data->p_main.desc = &pin_main;
	data->p_main.offset = main_addr;
	data->pfg = NULL;

	return data;
}

static void nsp_data_destroy(struct nsp_data *data)
{
	kfree(data->app_path);
	kfree(data);
}

static struct nsp_data *nsp_data_find(const struct dentry *dentry)
{
	struct nsp_data *data;

	list_for_each_entry(data, &nsp_data_list, list) {
		if (data->app_dentry == dentry)
			return data;
	}

	return NULL;
}

static struct nsp_data *nsp_data_find_by_path(const char *path)
{
	struct nsp_data *data;

	list_for_each_entry(data, &nsp_data_list, list) {
		if (strcmp(data->app_path, path) == 0)
			return data;
	}

	return NULL;
}

static void nsp_data_add(struct nsp_data *data)
{
	list_add(&data->list, &nsp_data_list);
}

static void nsp_data_rm(struct nsp_data *data)
{
	list_del(&data->list);
}

static int nsp_data_inst(struct nsp_data *data)
{
	int ret;
	struct pf_group *pfg;

	pfg = get_pf_group_by_dentry(lpad_dentry, (void *)data->app_dentry);
	if (pfg == NULL)
		return -ENOMEM;

	ret = pin_register(&p_dlsym, pfg, lpad_dentry);
	if (ret)
		goto put_g;

	ret = pin_register(&p_dlopen, pfg, lpad_dentry);
	if (ret)
		goto ur_dlsym;

	ret = pin_register(&data->p_main, pfg, data->app_dentry);
	if (ret)
		goto ur_dlopen;

	ret = pin_register(&p_ac_efl_main, pfg, libappcore_dentry);
	if (ret)
		goto ur_main;

	ret = pin_register(&p_ac_init, pfg, libcapi_dentry);
	if (ret)
		goto ur_ac_efl_main;

	ret = pin_register(&p_elm_run, pfg, libcapi_dentry);
	if (ret)
		goto ur_ac_init;

	ret = pin_register(&p_do_app, pfg, libappcore_dentry);
	if (ret)
		goto ur_elm_run;

	data->pfg = pfg;

	return 0;

ur_elm_run:
	pin_unregister(&p_elm_run, pfg);
ur_ac_init:
	pin_unregister(&p_ac_init, pfg);
ur_ac_efl_main:
	pin_unregister(&p_ac_efl_main, pfg);
ur_main:
	pin_unregister(&data->p_main, pfg);
ur_dlopen:
	pin_unregister(&p_dlopen, pfg);
ur_dlsym:
	pin_unregister(&p_dlsym, pfg);
put_g:
	put_pf_group(pfg);
	return ret;
}

static void nsp_data_uninst(struct nsp_data *data)
{
	struct pf_group *pfg = data->pfg;

	pin_unregister(&p_do_app, pfg);
	pin_unregister(&p_elm_run, pfg);
	pin_unregister(&p_ac_init, pfg);
	pin_unregister(&p_ac_efl_main, pfg);
	pin_unregister(&data->p_main, pfg);
	pin_unregister(&p_dlopen, pfg);
	pin_unregister(&p_dlsym, pfg);
	put_pf_group(pfg);

	data->pfg = NULL;
}

static int __nsp_add(const char *app_path, unsigned long main_addr)
{
	struct nsp_data *data;

	if (nsp_data_find_by_path(app_path))
		return -EEXIST;

	data = nsp_data_create(app_path, main_addr);
	if (IS_ERR(data))
		return PTR_ERR(data);

	nsp_data_add(data);

	return 0;
}

static int __nsp_rm(const char *path)
{
	struct dentry *dentry;
	struct nsp_data *data;

	dentry = dentry_by_path(path);
	if (dentry == NULL)
		return -ENOENT;

	data = nsp_data_find(dentry);
	if (data == NULL)
		return -ESRCH;

	nsp_data_rm(data);
	nsp_data_destroy(data);

	return 0;
}

static int __nsp_rm_all(void)
{
	struct nsp_data *data, *n;

	list_for_each_entry_safe(data, n, &nsp_data_list, list) {
		nsp_data_rm(data);
		nsp_data_destroy(data);
	}

	return 0;
}

static void __nsp_disabel(void)
{
	struct nsp_data *data;

	list_for_each_entry(data, &nsp_data_list, list) {
		if (data->pfg)
			nsp_data_uninst(data);
	}
}

static int __nsp_enable(void)
{
	int ret;
	struct nsp_data *data;

	list_for_each_entry(data, &nsp_data_list, list) {
		ret = nsp_data_inst(data);
		if (ret)
			goto fail;
	}

	return 0;

fail:
	__nsp_disabel();
	return ret;
}







/* ============================================================================
 * =                             set parameters                               =
 * ============================================================================
 */
#define F_ARG1(m, t, a)		m(t, a)
#define F_ARG2(m, t, a, ...)	m(t, a), F_ARG1(m, __VA_ARGS__)
#define F_ARG3(m, t, a, ...)	m(t, a), F_ARG2(m, __VA_ARGS__)
#define F_ARG(n, m, ...)	F_ARG##n(m, __VA_ARGS__)

#define M_TYPE_AND_ARG(t, a)	t a
#define M_ARG(t, a)		a

#define DECLARE_SAFE_FUNC(n, func_name, do_func, ...)	\
int func_name(F_ARG(n, M_TYPE_AND_ARG,  __VA_ARGS__))	\
{							\
	int ret;					\
	mutex_lock(&stat_mutex);			\
	if (stat == NS_ON) {				\
		ret = -EBUSY;				\
		goto unlock;				\
	}						\
	ret = do_func(F_ARG(n, M_ARG,  __VA_ARGS__));	\
unlock:							\
	mutex_unlock(&stat_mutex);			\
	return ret;					\
}

#define DECLARE_SAFE_FUNC0(name, _do)		DECLARE_SAFE_FUNC(1, name, _do, void, /* */);
#define DECLARE_SAFE_FUNC1(name, _do, ...)	DECLARE_SAFE_FUNC(1, name, _do, __VA_ARGS__);
#define DECLARE_SAFE_FUNC2(name, _do, ...)	DECLARE_SAFE_FUNC(2, name, _do, __VA_ARGS__);
#define DECLARE_SAFE_FUNC3(name, _do, ...)	DECLARE_SAFE_FUNC(3, name, _do, __VA_ARGS__);


static DEFINE_MUTEX(stat_mutex);
static enum nsp_stat stat = NS_OFF;

DECLARE_SAFE_FUNC2(nsp_add, __nsp_add, const char *, app_path,
		   unsigned long, main_addr);
DECLARE_SAFE_FUNC1(nsp_rm, __nsp_rm, const char *, app_path);
DECLARE_SAFE_FUNC0(nsp_rm_all, __nsp_rm_all);
DECLARE_SAFE_FUNC3(nsp_set_lpad_info, do_set_lpad_info,
		   const char *, path, unsigned long, dlopen,
		   unsigned long, dlsym);
DECLARE_SAFE_FUNC1(nsp_set_appcore_info, do_set_appcore_info,
		   struct nsp_info_data *, info);





/* ============================================================================
 * =                               set stat                                   =
 * ============================================================================
 */
static int set_stat_off(void)
{
	if (stat == NS_OFF)
		return -EINVAL;

	__nsp_disabel();

	stat = NS_OFF;

	return 0;
}

static int set_stat_on(void)
{
	if (is_init() == false)
		return -EPERM;

	if (stat == NS_ON)
		return -EINVAL;

	__nsp_enable();

	stat = NS_ON;

	return 0;
}

int nsp_set_stat(enum nsp_stat st)
{
	int ret = -EINVAL;

	mutex_lock(&stat_mutex);
	switch (st) {
	case NS_OFF:
		ret = set_stat_off();
		break;
	case NS_ON:
		ret = set_stat_on();
		break;
	}
	mutex_unlock(&stat_mutex);

	return ret;
}

enum nsp_stat nsp_get_stat(void)
{
	return stat;
}





/* ============================================================================
 * =                                 tdata                                    =
 * ============================================================================
 */
enum nsp_proc_stat {
	NPS_OPEN_E,		/* mapping begin */
	NPS_OPEN_R,
	NPS_SYM_E,
	NPS_SYM_R,		/* mapping end   */
	NPS_MAIN_E,		/* main begin    */
	NPS_AC_EFL_MAIN_E,	/* main end      */
	NPS_AC_INIT_R,		/* create begin  */
	NPS_ELM_RUN_E,		/* create end    */
	NPS_DO_APP_E,		/* reset begin   */
	NPS_DO_APP_R		/* reset end     */
};

struct tdata {
	enum nsp_proc_stat stat;
	struct nsp_data *nsp_data;
	u64 time;
	void __user *handle;
	struct probe_new p_main;
};


static void ktd_init(struct task_struct *task, void *data)
{
	struct tdata *tdata = (struct tdata *)data;

	tdata->nsp_data = NULL;
}

static struct ktask_data ktd = {
	.init = ktd_init,
	.size = sizeof(struct tdata),
};

static struct tdata *tdata_get(struct task_struct *task)
{
	return (struct tdata *)swap_ktd(&ktd, task);
}





/* ============================================================================
 * =                                handlers                                  =
 * ============================================================================
 */
static int dlopen_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	const char __user *user_s = (const char __user *)swap_get_uarg(regs, 0);
	const char *path;
	struct nsp_data *nsp_data;

	path = strdup_from_user(user_s, GFP_ATOMIC);
	if (path == NULL)
		return 0;

	nsp_data = nsp_data_find_by_path(path);
	if (nsp_data) {
		struct tdata *tdata = tdata_get(current);

		/* init tdata */
		if (tdata->nsp_data == NULL) {
			tdata->stat = NPS_OPEN_E;
			tdata->time = swap_msg_current_time();
			tdata->nsp_data = nsp_data;
		}
	}

	kfree(path);
	return 0;
}

static int dlopen_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct tdata *tdata = tdata_get(current);
	void *handle = (void *)regs_return_value(regs);

	if ((tdata->stat == NPS_OPEN_E) && handle) {
		tdata->stat = NPS_OPEN_R;
		tdata->handle = handle;
	} else {
		tdata->handle = NULL;
	}

	return 0;
}

static int dlsym_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct tdata *tdata = tdata_get(current);
	void __user *handle = (void __user *)swap_get_uarg(regs, 0);
	const char __user *str = (const char __user *)swap_get_uarg(regs, 1);

	if (handle == tdata->handle && tdata->stat == NPS_OPEN_R) {
		const char *name;

		name = strdup_from_user(str, GFP_ATOMIC);
		if (name && (strcmp(name, "main") == 0))
			tdata->stat = NPS_SYM_E;

		kfree(name);
	}

	return 0;
}

static int dlsym_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct tdata *tdata = tdata_get(current);

	if (tdata->handle && tdata->stat == NPS_SYM_E)
		tdata->stat = NPS_SYM_R;

	return 0;
}

static void stage_begin(enum nsp_proc_stat priv, enum nsp_proc_stat cur)
{
	struct tdata *tdata = tdata_get(current);

	if (tdata->handle && tdata->stat == priv) {
		tdata->stat = cur;
		tdata->time = swap_msg_current_time();
	}
}

static void stage_end(enum nsp_proc_stat priv, enum nsp_proc_stat cur,
		      enum nsp_msg_stage st)
{
	struct tdata *tdata = tdata_get(current);
	u64 time_start;
	u64 time_end;

	if (tdata->handle && tdata->stat == priv) {
		tdata->stat = cur;
		time_start = tdata->time;

		time_end = swap_msg_current_time();
		nsp_msg(st, time_start, time_end);
	}
}

static int main_h(struct uprobe *p, struct pt_regs *regs)
{
	struct tdata *tdata = tdata_get(current);
	u64 time_start;
	u64 time_end;

	if (tdata->handle && tdata->stat == NPS_SYM_R) {
		tdata->stat = NPS_MAIN_E;
		time_start = tdata->time;
		time_end = swap_msg_current_time();
		tdata->time = time_end;

		nsp_msg(NMS_MAPPING, time_start, time_end);
	}

	return 0;
}

/* FIXME: workaround for simultaneously nsp and main() function profiling */
#include <retprobe/rp_msg.h>
#include <us_manager/us_manager.h>

static int main_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp) {
		main_h(&rp->up, regs);

		if (get_quiet() == QT_OFF) {
			struct sspt_ip *ip;
			unsigned long func_addr;

			ip = container_of(rp, struct sspt_ip, retprobe);
			func_addr = (unsigned long)ip->orig_addr;
			rp_msg_entry(regs, func_addr, "p");
		}
	}

	return 0;
}

static int main_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp && get_quiet() == QT_OFF) {
		struct sspt_ip *ip;
		unsigned long func_addr;
		unsigned long ret_addr;

		ip = container_of(rp, struct sspt_ip, retprobe);
		func_addr = (unsigned long)ip->orig_addr;
		ret_addr = (unsigned long)ri->ret_addr;
		rp_msg_exit(regs, func_addr, 'n', ret_addr);
	}

	return 0;
}

static int ac_efl_main_h(struct uprobe *p, struct pt_regs *regs)
{
	stage_end(NPS_MAIN_E, NPS_AC_EFL_MAIN_E, NMS_MAIN);
	return 0;
}

static int ac_init_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	stage_begin(NPS_AC_EFL_MAIN_E, NPS_AC_INIT_R);
	return 0;
}

static int elm_run_h(struct uprobe *p, struct pt_regs *regs)
{
	stage_end(NPS_AC_INIT_R, NPS_ELM_RUN_E, NMS_CREATE);
	return 0;
}

static int do_app_eh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	int event = swap_get_uarg(regs, 0);
	enum { AE_RESET = 5 };	/* FIXME: hardcode */

	if (event == AE_RESET)
		stage_begin(NPS_ELM_RUN_E, NPS_DO_APP_E);

	return 0;
}

static int do_app_rh(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	stage_end(NPS_DO_APP_E, NPS_DO_APP_R, NMS_RESET);
	return 0;
}





int nsp_init(void)
{
	return swap_ktd_reg(&ktd);
}

void nsp_exit(void)
{
	if (stat == NS_ON)
		set_stat_off();

	uninit_variables();
	swap_ktd_unreg(&ktd);
}
