/**
 * parser/us_inst.c
 * @author Vyacheslav Cherkashin
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * User-space instrumentation controls.
 */


#include <linux/slab.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <us_manager/pf/pf_group.h>
#include <us_manager/probes/probes.h>

#include "msg_parser.h"
#include "us_inst.h"
#include "usm_msg.h"


static struct pfg_msg_cb msg_cb = {
	.msg_info = usm_msg_info,
	.msg_status_info = usm_msg_status_info,
	.msg_term = usm_msg_term,
	.msg_map = usm_msg_map,
	.msg_unmap = usm_msg_unmap
};


static int probe_inst_reg(struct pr_probe_desc *probe, struct pf_group *pfg,
			  struct dentry *dentry)
{
	probe->ip = pf_register_probe(pfg, dentry, probe->addr, &probe->p_desc);
	if (IS_ERR(probe->ip))
		return PTR_ERR(probe->ip);

	return 0;
}

static void probe_inst_unreg(struct pr_probe_desc *probe, struct pf_group *pfg)
{
	pf_unregister_probe(pfg, probe->ip);
}

static void do_bin_inst_unreg(struct list_head *head, struct pf_group *pfg)
{
	struct pr_probe_desc *probe;

	list_for_each_entry(probe, head, list) {
		probe_inst_unreg(probe, pfg);
	}
}

static void bin_inst_unreg(struct pr_bin_desc *bin, struct pf_group *pfg)
{
	do_bin_inst_unreg(&bin->probe_head, pfg);
}

static int bin_inst_reg(struct pr_bin_desc *bin, struct pf_group *pfg)
{
	struct pr_probe_desc *probe, *n;
	struct dentry *dentry;
	LIST_HEAD(reg_head);

	dentry = dentry_by_path(bin->info->path);
	if (dentry == NULL) {
		pr_warn("Cannot get dentry by path %s\n", bin->info->path);
		return -EINVAL;
	}

	list_for_each_entry_safe(probe, n, &bin->probe_head, list) {
		int ret;

		ret = probe_inst_reg(probe, pfg, dentry);
		if (!ret) {
			list_move(&probe->list, &reg_head);
		} else {
			do_bin_inst_unreg(&reg_head, pfg);
			return ret;
		}
	}

	list_splice(&reg_head, &bin->probe_head);
	return 0;
}

static struct pf_group *get_pfg_by_app_info(struct pr_app_info *app)
{
	struct pf_group *pfg = ERR_PTR(-EINVAL);
	struct dentry *dentry;

	dentry = dentry_by_path(app->path);
	if (dentry == NULL)
		return pfg;

	switch (app->type) {
	case AT_PID:
		if (app->tgid == 0) {
			if (app->path[0] == '\0')
				pfg = get_pf_group_dumb(dentry);
			else
				goto pf_dentry;
		} else {
			pfg = get_pf_group_by_tgid(app->tgid, dentry);
		}
		break;
	case AT_TIZEN_WEB_APP:
		pfg = get_pf_group_by_comm(app->id, dentry);
		break;
	case AT_TIZEN_NATIVE_APP:
	case AT_COMMON_EXEC:
 pf_dentry:
		pfg = get_pf_group_by_dentry(dentry, dentry);
		break;
	default:
		pr_info("ERROR: app_type=0x%x\n", app->type);
		break;
	}

	if (!pfg)
		pfg = ERR_PTR(-ENOMEM);

	if (!IS_ERR(pfg)) {
		/* TODO: move to other location and chack return value */
		pfg_msg_cb_set(pfg, &msg_cb);
	}

	return pfg;
}

static void do_us_app_inst_unreg(struct pr_app_desc *app,
				 struct list_head *head)
{
	struct pr_bin_desc *bin;

	list_for_each_entry(bin, head, list) {
		bin_inst_unreg(bin, app->pfg);
	}
	put_pf_group(app->pfg);
	app->pfg = NULL;
}

static void us_app_inst_unreg(struct pr_app_desc *app)
{
	do_us_app_inst_unreg(app, &app->bin_head);
}

static int us_app_inst_reg(struct pr_app_desc *app)
{
	struct pf_group *pfg;
	struct pr_bin_desc *bin, *n;
	LIST_HEAD(reg_head);

	pfg = get_pfg_by_app_info(app->info);
	if (IS_ERR(pfg))
		return PTR_ERR(pfg);

	app->pfg = pfg;
	list_for_each_entry_safe(bin, n, &app->bin_head, list) {
		int ret;

		ret = bin_inst_reg(bin, app->pfg);
		if (!ret) {
			list_move(&bin->list, &reg_head);
		} else {
			do_us_app_inst_unreg(app, &reg_head);
			return ret;
		}
	}

	list_splice(&reg_head, &app->bin_head);
	return 0;
}


static struct pr_probe_desc *find_probe(struct list_head *head,
					struct pr_probe_desc *probe)
{
	struct pr_probe_desc *p;

	list_for_each_entry(p, head, list) {
		if (!probe_inst_info_cmp(probe, p))
			return p;
	}

	return NULL;
}

static struct pr_bin_desc *find_bin(struct list_head *head,
				    struct pr_bin_info *info)
{
	struct pr_bin_desc *bin;

	list_for_each_entry(bin, head, list) {
		if (!pr_bin_info_cmp(bin->info, info))
			return bin;
	}

	return NULL;
}

static struct pr_app_desc *find_app(struct list_head *head,
				    struct pr_app_info *app_info)
{
	struct pr_app_desc *app;

	list_for_each_entry(app, head, list) {
		if (!pr_app_info_cmp(app->info, app_info))
			return app;
	}

	return NULL;
}

static void us_probe_get_equal_elements(struct list_head *probe_head,
					struct list_head *test_probe_head,
					struct list_head *out_probe_head)
{
	struct pr_probe_desc *test_probe, *n;

	list_for_each_entry_safe(test_probe, n, test_probe_head, list) {
		struct pr_probe_desc *probe;

		probe = find_probe(probe_head, test_probe);
		if (probe) {
			list_move(&probe->list, out_probe_head);

			/* remove probe */
			list_del(&test_probe->list);
			pr_probe_desc_free(test_probe);
		} else {
			return;
		}
	}
}

static void us_bin_get_equal_elements(struct list_head *bin_head,
				      struct list_head *test_bin_head,
				      struct list_head *out_bin_head)
{
	struct pr_bin_desc *test_bin, *n;

	list_for_each_entry_safe(test_bin, n, test_bin_head, list) {
		struct pr_bin_desc *bin;
		LIST_HEAD(out_probe_head);

		bin = find_bin(bin_head, test_bin->info);
		if (!bin)
			return;

		us_probe_get_equal_elements(&bin->probe_head,
					    &test_bin->probe_head,
					    &out_probe_head);

		/* check all probes found */
		if (list_empty(&test_bin->probe_head)) {
			list_move(&test_bin->list, out_bin_head);
			list_splice(&out_probe_head, &test_bin->probe_head);
		} else {
			list_splice(&out_probe_head, &bin->probe_head);
		}
	}
}

static void us_app_get_equal_elements(struct list_head *app_head,
				      struct list_head *test_app_head,
				      struct list_head *out_app_head)
{
	struct pr_app_desc *test_app, *n;

	list_for_each_entry_safe(test_app, n, test_app_head, list) {
		struct pr_app_desc *app;
		LIST_HEAD(out_bin_head);

		app = find_app(app_head, test_app->info);
		if (!app)
			return;

		us_bin_get_equal_elements(&app->bin_head,
					  &test_app->bin_head,
					  &out_bin_head);

		/* check all bins found */
		if (list_empty(&test_app->bin_head)) {
			list_move(&test_app->list, out_app_head);
			list_splice(&out_bin_head, &test_app->bin_head);
		} else {
			list_splice(&out_bin_head, &app->bin_head);
		}
	}
}


static void bin_list_splice(struct list_head *list, struct list_head *head)
{
	struct pr_bin_desc *new_bin, *n;

	list_for_each_entry_safe(new_bin, n, list, list) {
		struct pr_bin_desc *bin;

		bin = find_bin(head, new_bin->info);
		if (bin) {
			list_splice_init(&new_bin->probe_head,
					 &bin->probe_head);

			list_del(&new_bin->list);
			pr_bin_desc_free(new_bin);
		} else {
			list_move(&new_bin->list, head);
		}
	}
}

static void app_list_splice(struct list_head *list, struct list_head *head)
{
	struct pr_app_desc *new_app, *n;

	list_for_each_entry_safe(new_app, n, list, list) {
		struct pr_app_desc *app;

		app = find_app(head, new_app->info);
		if (app) {
			bin_list_splice(&new_app->bin_head, &app->bin_head);

			list_del(&new_app->list);
			put_pf_group(app->pfg);
			pr_app_desc_free(new_app);
		} else {
			list_move(&new_app->list, head);
		}
	}
}

static void app_list_free(struct list_head *head)
{
	struct pr_app_desc *app, *n;

	list_for_each_entry_safe(app, n, head, list) {
		list_del(&app->list);
		pr_app_desc_free(app);
	}
}

static void do_app_list_unreg(struct list_head *head)
{
	struct pr_app_desc *app;

	list_for_each_entry(app, head, list) {
		us_app_inst_unreg(app);
	}
}



static LIST_HEAD(app_head);

/* After call the 'head' list is empty, do not free it. */
int app_list_unreg(struct list_head *head)
{
	LIST_HEAD(out_app_head);

	us_app_get_equal_elements(&app_head, head, &out_app_head);

	/* check all apps found */
	if (!list_empty(head)) {
		app_list_splice(&out_app_head, &app_head);
		return -EINVAL;
	}

	do_app_list_unreg(&out_app_head);
	app_list_free(&out_app_head);
	return 0;
}

/* After call the 'head' list is empty, do not free it. */
int app_list_reg(struct list_head *head)
{
	LIST_HEAD(reg_head);
	struct pr_app_desc *app, *n;

	list_for_each_entry_safe(app, n, head, list) {
		int ret;

		ret = us_app_inst_reg(app);
		if (!ret) {
			list_move(&app->list, &reg_head);
		} else {
			do_app_list_unreg(&reg_head);
			list_splice(&reg_head, head);
			app_list_free(head);
			return ret;
		}
	}

	app_list_splice(&reg_head, &app_head);
	return 0;
}

void app_list_unreg_all(void)
{
	do_app_list_unreg(&app_head);
	app_list_free(&app_head);
}
