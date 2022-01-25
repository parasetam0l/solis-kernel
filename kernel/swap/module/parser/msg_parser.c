/**
 * parser/msg_parser.c
 *
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov <v.cherepanov@samsung.com>
 *
 * @sectionLICENSE
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
 * Message parsing implementation.
 */


#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <us_manager/probes/probes.h>
#include "msg_parser.h"
#include "msg_buf.h"
#include "parser_defs.h"

/* ============================================================================
 * ==                               APP_INFO                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills pr_app_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param ai Pointer to the target app_inst_data.
 * @return 0 on success, error code on error.
 */
struct pr_app_info *pr_app_info_create(struct msg_buf *mb)
{
	int ret;
	struct pr_app_info *app_info;
	u32 app_type;
	char *str_id, *exec_path;

	app_info = kmalloc(sizeof(*app_info), GFP_KERNEL);
	if (!app_info)
		return ERR_PTR(-ENOMEM);

	print_parse_debug("app_info:\n");
	print_parse_debug("type:");
	ret = get_u32(mb, &app_type);
	if (ret) {
		print_err("failed to read target application type\n");
		goto free_app_info;
	}

	print_parse_debug("id:");
	ret = get_string(mb, &str_id);
	if (ret) {
		print_err("failed to read target application ID\n");
		goto free_app_info;
	}

	print_parse_debug("exec path:");
	ret = get_string(mb, &exec_path);
	if (ret) {
		print_err("failed to read executable path\n");
		goto free_id;
	}

	switch (app_type) {
	case AT_TIZEN_NATIVE_APP:
	case AT_TIZEN_WEB_APP:
	case AT_COMMON_EXEC:
		app_info->tgid = 0;
		break;
	case AT_PID: {
		u32 tgid = 0;

		if (*str_id != '\0') {
			ret = kstrtou32(str_id, 10, &tgid);
			if (ret) {
				print_err("converting string to PID, "
					  "str='%s'\n", str_id);
				goto free_exec_path;
			}
		}

		app_info->tgid = tgid;
		break;
	}
	default:
		print_err("wrong application type(%u)\n", app_type);
		ret = -EINVAL;
		goto free_exec_path;
	}

	app_info->type = (enum APP_TYPE)app_type;
	app_info->id = str_id;
	app_info->path = exec_path;

	return app_info;

free_exec_path:
	put_string(exec_path);
free_id:
	put_string(str_id);
free_app_info:
	kfree(app_info);
	return ERR_PTR(ret);
}

void pr_app_info_free(struct pr_app_info *app_info)
{
	put_string(app_info->path);
	put_string(app_info->id);
	kfree(app_info);
}

int pr_app_info_cmp(struct pr_app_info *app0, struct pr_app_info *app1)
{
	print_parse_debug("app0: %d, %d, %s, %s\n",
			  app0->type, app0->tgid, app0->id, app0->path);

	print_parse_debug("app1: %d, %d, %s, %s\n",
			  app1->type, app1->tgid, app1->id, app1->path);

	if ((app0->type == app1->type) &&
	    (app0->tgid == app1->tgid) &&
	    !strcmp(app0->id, app1->id) &&
	    !strcmp(app0->path, app1->path)) {
		return 0;
	}

	return 1;
}



/* ============================================================================
 * ==                                CONFIG                                  ==
 * ============================================================================
 */

/**
 * @brief Creates and fills conf_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled conf_data struct on success;\n
 * NULL on error.
 */
struct conf_data *create_conf_data(struct msg_buf *mb)
{
	struct conf_data *conf;
	u64 use_features0, use_features1;
	u32 stp, dmp;

	print_parse_debug("conf_data:\n");

	print_parse_debug("features:");
	if (get_u64(mb, &use_features0)) {
		print_err("failed to read use_features\n");
		return NULL;
	}

	if (get_u64(mb, &use_features1)) {
		print_err("failed to read use_features\n");
		return NULL;
	}

	print_parse_debug("sys trace period:");
	if (get_u32(mb, &stp)) {
		print_err("failed to read sys trace period\n");
		return NULL;
	}

	print_parse_debug("data msg period:");
	if (get_u32(mb, &dmp)) {
		print_err("failed to read data message period\n");
		return NULL;
	}

	conf = kmalloc(sizeof(*conf), GFP_KERNEL);
	if (conf == NULL) {
		print_err("out of memory\n");
		return NULL;
	}

	conf->use_features0 = use_features0;
	conf->use_features1 = use_features1;
	conf->sys_trace_period = stp;
	conf->data_msg_period = dmp;

	return conf;
}

/**
 * @brief conf_data cleanup.
 *
 * @param conf Pointer to the target conf_data.
 * @return Void.
 */
void destroy_conf_data(struct conf_data *conf)
{
	kfree(conf);
}

static struct conf_data config;

/**
 * @brief Saves config to static config variable.
 *
 * @param conf Variable to save.
 * @return Void.
 */
void save_config(const struct conf_data *conf)
{
	memcpy(&config, conf, sizeof(config));
}

/**
 * @brief Restores config from static config variable.
 *
 * @param conf Variable to restore.
 * @return Void.
 */
void restore_config(struct conf_data *conf)
{
	memcpy(conf, &config, sizeof(*conf));
}



/* ============================================================================
 * ==                             PROBES PARSING                             ==
 * ============================================================================
 */

/**
 * @brief Gets retprobe data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_retprobe(struct msg_buf *mb, struct probe_desc *pd)
{
	char *args;
	char ret_type;

	print_parse_debug("funct args:");
	if (get_string(mb, &args)) {
		print_err("failed to read data function arguments\n");
		return -EINVAL;
	}

	print_parse_debug("funct ret type:");
	if (get_u8(mb, (u8 *)&ret_type)) {
		print_err("failed to read data function arguments\n");
		goto free_args;
	}

	pd->type = SWAP_RETPROBE;
	pd->info.rp_i.args = args;
	pd->info.rp_i.ret_type = ret_type;

	return 0;

free_args:
	put_string(args);
	return -EINVAL;
}

/**
 * @brief Retprobe data cleanup.
 *
 * @param pi Pointer to the probe_info comprising retprobe.
 * @return Void.
 */
void put_retprobe(struct probe_info *pi)
{
	put_string(pi->rp_i.args);
}

static int cmp_retprobe(struct probe_info *p0, struct probe_info *p1)
{
	if (p0->rp_i.ret_type == p1->rp_i.ret_type &&
	    !strcmp(p0->rp_i.args, p1->rp_i.args)) {
		return 0;
	}

	return 1;
}

/**
 * @brief Gets webprobe data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_webprobe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_WEBPROBE;

	return 0;
}

static int cmp_webprobe(struct probe_info *p0, struct probe_info *p1)
{
	return 0;
}

/**
 * @brief Gets preload data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_preload_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	u64 handler;
	u8 flags;
	char *path;

	print_parse_debug("funct handler:");
	if (get_u64(mb, &handler)) {
		print_err("failed to read function handler\n");
		return -EINVAL;
	}

	print_parse_debug("collect events flag:");
	if (get_u8(mb, &flags)) {
		print_err("failed to read collect events type\n");
		return -EINVAL;
	}

	print_parse_debug("handler library:");
	if (get_string(mb, &path)) {
		print_err("failed to read handler library path\n");
		return -EINVAL;
	}

	pd->type = SWAP_PRELOAD_PROBE;
	pd->info.pl_i.handler = handler;
	pd->info.pl_i.flags = flags;
	pd->info.pl_i.path = path;

	return 0;
}

/**
 * @brief Preload probe data cleanup.
 *
 * @param pi Pointer to the probe_info struct.
 * @return Void.
 */
void put_preload_probe(struct probe_info *pi)
{
	put_string((char *)pi->pl_i.path);
}

static int cmp_preload_probe(struct probe_info *p0, struct probe_info *p1)
{
	if (p0->pl_i.handler == p1->pl_i.handler &&
	    p0->pl_i.flags == p1->pl_i.flags) {
		return 0;
	}

	return 1;
}

/**
 * @brief Gets preload get_caller and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */

int get_get_caller_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_GET_CALLER;

	return 0;
}

/**
 * @brief Preload get_caller probe data cleanup.
 *
 * @param pi Pointer to the probe_info struct.
 * @return Void.
 */
void put_get_caller_probe(struct probe_info *pi)
{
}

static int cmp_get_caller_probe(struct probe_info *p0, struct probe_info *p1)
{
	return 0;
}

/**
 * @brief Gets preload get_call_type and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pd Pointer to the probe_desc struct.
 * @return 0 on success, error code on error.
 */
int get_get_call_type_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_GET_CALL_TYPE;

	return 0;
}

/**
 * @brief Preload get_call type probe data cleanup.
 *
 * @param pi Pointer to the probe_info struct.
 * @return Void.
 */
void put_get_call_type_probe(struct probe_info *pi)
{
}

static int cmp_get_caller_type_probe(struct probe_info *p0,
				     struct probe_info *p1)
{
	return 0;
}

/**
 * @brief Gets preload write_msg and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pi Pointer to the probe_info struct.
 * @return 0 on success, error code on error.
 */
int get_write_msg_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	pd->type = SWAP_WRITE_MSG;

	return 0;
}

/**
 * @brief Preload write_msg type probe data cleanup.
 *
 * @param pi Pointer to the probe_info comprising retprobe.
 * @return Void.
 */
void put_write_msg_probe(struct probe_info *pi)
{
}

static int cmp_write_msg_probe(struct probe_info *p0, struct probe_info *p1)
{
	return 0;
}

/**
 * @brief Gets FBI probe data and puts it to the probe_info struct.
 *
 * @param mb Pointer to the message buffer.
 * @param pi Pointer to the probe_info struct.
 * @return 0 on success, error code on error.
 */
int get_fbi_data(struct msg_buf *mb, struct fbi_var_data *vd)
{
	u64 var_id;
	u64 reg_offset;
	u8 reg_n;
	u32 data_size;
	u8 steps_count, i;
	struct fbi_step *steps = NULL;

	print_parse_debug("var ID:");
	if (get_u64(mb, &var_id)) {
		print_err("failed to read var ID\n");
		return -EINVAL;
	}

	print_parse_debug("register offset:");
	if (get_u64(mb, &reg_offset)) {
		print_err("failed to read register offset\n");
		return -EINVAL;
	}

	print_parse_debug("register number:");
	if (get_u8(mb, &reg_n)) {
		print_err("failed to read number of the register\n");
		return -EINVAL;
	}

	print_parse_debug("data size:");
	if (get_u32(mb, &data_size)) {
		print_err("failed to read data size\n");
		return -EINVAL;
	}

	print_parse_debug("steps count:");
	if (get_u8(mb, &steps_count)) {
		print_err("failed to read steps count\n");
		return -EINVAL;
	}

	if (steps_count > 0) {
		steps = kmalloc(steps_count * sizeof(*vd->steps),
				GFP_KERNEL);
		if (steps == NULL) {
			print_err("MALLOC FAIL\n");
			return -ENOMEM;
		}

		for (i = 0; i != steps_count; i++) {
			print_parse_debug("steps #%d ptr_order:", i);
			if (get_u8(mb, &(steps[i].ptr_order))) {
				print_err("failed to read pointer order(step #%d)\n",
					  i);
				goto free_steps;
			}
			print_parse_debug("steps #%d data_offset:", i);
			if (get_u64(mb, &(steps[i].data_offset))){
				print_err("failed to read offset (steps #%d)\n",
					  i);
				goto free_steps;
			}
		}
	}

	vd->reg_n = reg_n;
	vd->reg_offset = reg_offset;
	vd->data_size = data_size;
	vd->var_id = var_id;
	vd->steps_count = steps_count;
	vd->steps = steps;

	return 0;

free_steps:
	kfree(steps);
	return -EINVAL;
}

int get_fbi_probe(struct msg_buf *mb, struct probe_desc *pd)
{
	uint8_t var_count, i;
	struct fbi_var_data *vars;

	print_parse_debug("var count:");
	if (get_u8(mb, &var_count)) {
		print_err("failed to read var ID\n");
		return -EINVAL;
	}

	vars = kmalloc(var_count * sizeof(*vars), GFP_KERNEL);
	if (vars == NULL) {
		print_err("alloc vars error\n");
		goto err;
	}

	for (i = 0; i != var_count; i++) {
		if (get_fbi_data(mb, &vars[i]) != 0)
			goto free_vars;
	}

	pd->type = SWAP_FBIPROBE;
	pd->info.fbi_i.var_count = var_count;
	pd->info.fbi_i.vars = vars;
	return 0;

free_vars:
	kfree(vars);

err:
	return -EINVAL;

}

/**
 * @brief FBI probe data cleanup.
 *
 * @param pi Pointer to the probe_info comprising FBI probe.
 * @return Void.
 */
void put_fbi_probe(struct probe_info *pi)
{
	/* FIXME: memory leak (vars) */
	return;
}

static int cmp_fbi_probe(struct probe_info *p0, struct probe_info *p1)
{
	/* TODO: to implement */
	return 0;
}


/* ============================================================================
 * ==                                 PROBE                                  ==
 * ============================================================================
 */

/**
 * @brief Creates and fills func_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled func_inst_data struct on success;\n
 * 0 on error.
 */
struct pr_probe_desc *pr_probe_desc_create(struct msg_buf *mb)
{
	int ret = -EINVAL;
	struct pr_probe_desc *probe;
	int (*get_probe)(struct msg_buf *mb, struct probe_desc *pd);
	u64 addr;
	u8 type;

	print_parse_debug("func addr:");
	if (get_u64(mb, &addr)) {
		print_err("failed to read data function address\n");
		return ERR_PTR(-EINVAL);
	}

	print_parse_debug("probe type:");
	if (get_u8(mb, &type)) {
		print_err("failed to read data probe type\n");
		return ERR_PTR(-EINVAL);
	}

	probe = kmalloc(sizeof(*probe), GFP_KERNEL);
	if (!probe) {
		print_err("out of memory\n");
		return ERR_PTR(-ENOMEM);
	}
	INIT_LIST_HEAD(&probe->list);

	switch (type) {
	case SWAP_RETPROBE:
		get_probe = get_retprobe;
		break;
	case SWAP_WEBPROBE:
		get_probe = get_webprobe;
		break;
	case SWAP_PRELOAD_PROBE:
		get_probe = get_preload_probe;
		break;
	case SWAP_GET_CALLER:
		get_probe = get_get_caller_probe;
		break;
	case SWAP_GET_CALL_TYPE:
		get_probe = get_get_call_type_probe;
		break;
	case SWAP_FBIPROBE:
		get_probe = get_fbi_probe;
		break;
	case SWAP_WRITE_MSG:
		get_probe = get_write_msg_probe;
		break;
	default:
		printk(KERN_WARNING "SWAP PARSER: Wrong probe type %d!\n",
		       type);
		ret = -EINVAL;
		goto err;
	}

	ret = get_probe(mb, &probe->p_desc);
	if (ret)
		goto err;

	probe->addr = addr;
	return probe;

err:
	kfree(probe);
	return ERR_PTR(ret);
}


/**
 * @brief func_inst_data cleanup.
 *
 * @param fi Pointer to the target func_inst_data.
 * @return Void.
 */
void pr_probe_desc_free(struct pr_probe_desc *probe)
{
	switch (probe->p_desc.type) {
	case SWAP_RETPROBE:
		put_retprobe(&(probe->p_desc.info));
		break;
	case SWAP_WEBPROBE:
		break;
	case SWAP_PRELOAD_PROBE:
		put_preload_probe(&(probe->p_desc.info));
		break;
	case SWAP_GET_CALLER:
		put_get_caller_probe(&(probe->p_desc.info));
		break;
	case SWAP_GET_CALL_TYPE:
		put_get_call_type_probe(&(probe->p_desc.info));
		break;
	case SWAP_FBIPROBE:
		put_fbi_probe(&(probe->p_desc.info));
		break;
	case SWAP_WRITE_MSG:
		put_write_msg_probe(&(probe->p_desc.info));
		break;
	default:
		pr_err("SWAP PARSER: Wrong probe type %d!\n",
		       probe->p_desc.type);
	}

	kfree(probe);
}

int probe_inst_info_cmp(struct pr_probe_desc *p0, struct pr_probe_desc *p1)
{
	enum probe_t type;
	int (*cmp_probe)(struct probe_info *p0, struct probe_info *p1);

	if (p0->addr != p1->addr &&
	    p0->p_desc.type != p1->p_desc.type)
		return 1;

	type = p0->p_desc.type;
	switch (type) {
	case SWAP_RETPROBE:
		cmp_probe = cmp_retprobe;
		break;
	case SWAP_WEBPROBE:
		cmp_probe = cmp_webprobe;
		break;
	case SWAP_PRELOAD_PROBE:
		cmp_probe = cmp_preload_probe;
		break;
	case SWAP_GET_CALLER:
		cmp_probe = cmp_get_caller_probe;
		break;
	case SWAP_GET_CALL_TYPE:
		cmp_probe = cmp_get_caller_type_probe;
		break;
	case SWAP_FBIPROBE:
		cmp_probe = cmp_fbi_probe;
		break;
	case SWAP_WRITE_MSG:
		cmp_probe = cmp_write_msg_probe;
		break;
	default:
		pr_err("SWAP PARSER: Wrong probe type %d!\n", type);
		return 1;
	}

	return cmp_probe(&p0->p_desc.info, &p1->p_desc.info);
}

static struct pr_bin_info *pr_bin_info_create(const char *path)
{
	struct pr_bin_info *info;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	info->path = kstrdup(path, GFP_KERNEL);
	if (!info->path) {
		kfree(info);
		return ERR_PTR(-ENOMEM);
	}

	return info;
}

static void pr_bin_info_free(struct pr_bin_info *info)
{
	kfree(info->path);
	kfree(info);
}


struct pr_bin_desc *pr_bin_desc_create(const char *path,
				       struct list_head *probe_list)
{
	struct pr_bin_desc *bin;

	bin = kmalloc(sizeof(*bin), GFP_KERNEL);
	if (!bin)
		return ERR_PTR(-ENOMEM);

	bin->info = pr_bin_info_create(path);
	if (IS_ERR(bin->info)) {
		long err = PTR_ERR(bin->info);

		kfree(bin);
		return ERR_PTR(err);
	}

	INIT_LIST_HEAD(&bin->list);
	INIT_LIST_HEAD(&bin->probe_head);
	list_splice_init(probe_list, &bin->probe_head);

	return bin;
}

void pr_bin_desc_free(struct pr_bin_desc *bin)
{
	struct pr_probe_desc *p, *n;

	list_for_each_entry_safe(p, n, &bin->probe_head, list) {
		list_del(&p->list);
		pr_probe_desc_free(p);
	}

	pr_bin_info_free(bin->info);
	kfree(bin);
}

int pr_bin_info_cmp(struct pr_bin_info *b0, struct pr_bin_info *b1)
{
	return strcmp(b0->path, b1->path);
}


static void pr_probe_desc_list_free(struct list_head *head)
{
	struct pr_probe_desc *probe, *n;

	list_for_each_entry_safe(probe, n, head, list) {
		list_del(&probe->list);
		pr_probe_desc_free(probe);
	}
}

static int pr_probe_desc_list_create(struct msg_buf *mb, struct list_head *head)
{
	u32 i, cnt;

	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of functions\n");
		return -EINVAL;
	}

	print_parse_debug("probe count:%d", cnt);
	if (remained_mb(mb) / MIN_SIZE_FUNC_INST < cnt) {
		print_err("to match count of probes(%u)\n", cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; ++i) {
		struct pr_probe_desc *probe;

		print_parse_debug("probe #%d:\n", i + 1);
		probe = pr_probe_desc_create(mb);
		if (IS_ERR(probe)) {
			pr_probe_desc_list_free(head);
			return PTR_ERR(probe);
		}

		list_add(&probe->list, head);
	}

	return cnt;
}


struct pr_bin_desc *bin_info_by_lib(struct msg_buf *mb)
{
	u32 cnt;
	char *path;
	struct pr_bin_desc *bin = ERR_PTR(-EINVAL);

	print_parse_debug("bin path:");
	if (get_string(mb, &path)) {
		print_err("failed to read path of binary\n");
		return bin;
	}

	print_parse_debug("func count:");
	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of functions\n");
		goto free_path;
	}

	if (remained_mb(mb) / MIN_SIZE_FUNC_INST < cnt) {
		print_err("to match count of functions(%u)\n", cnt);
		goto free_path;
	}

	if (cnt) {
		u32 i;
		LIST_HEAD(probe_head);

		for (i = 0; i < cnt; ++i) {
			struct pr_probe_desc *probe;
			print_parse_debug("func #%d:\n", i + 1);
			probe = pr_probe_desc_create(mb);
			if (IS_ERR(probe)) {
				/* set error to 'bin' */
				bin = ERR_PTR(PTR_ERR(probe));
				pr_probe_desc_list_free(&probe_head);
				goto free_path;
			}

			list_add(&probe->list, &probe_head);
		}

		bin = pr_bin_desc_create(path, &probe_head);
		if (IS_ERR(bin))
			pr_probe_desc_list_free(&probe_head);
	}

free_path:
	put_string(path);
	return bin;
}




void bin_info_list_free(struct list_head *head)
{
	struct pr_bin_desc *bin, *n;

	list_for_each_entry_safe(bin, n, head, list) {
		list_del(&bin->list);
		pr_bin_desc_free(bin);
	}
}

int bin_info_list_create(struct msg_buf *mb, struct list_head *head)
{
	u32 i, cnt;

	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of binaries\n");
		return -EINVAL;
	}
	print_parse_debug("bin count:i%d", cnt);

	if (remained_mb(mb) / MIN_SIZE_LIB_INST < cnt) {
		print_err("to match count of binaries(%u)\n", cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; ++i) {
		struct pr_bin_desc *bin;

		print_parse_debug("bin #%d:\n", i_lib + 1);
		bin = bin_info_by_lib(mb);
		if (IS_ERR(bin)) {
			bin_info_list_free(head);
			return PTR_ERR(bin);
		} else if (bin) {
			list_add(&bin->list, head);
		}
	}

	return cnt;
}


/* ============================================================================
 * ==                                 APP                                    ==
 * ============================================================================
 */

/**
 * @brief Creates and fills pr_app_desc struct.
 *
 * @param mb Pointer to the message buffer.
 * @return Pointer to the filled app_inst_data struct on success;\n
 * 0 on error.
 */
struct pr_app_desc *pr_app_desc_create(struct msg_buf *mb)
{
	int cnt_probe, cnt_bin, ret = -EINVAL;
	struct pr_app_info *app_info;
	struct pr_app_desc *app;
	LIST_HEAD(probe_head);
	LIST_HEAD(bin_head);

	app = kmalloc(sizeof(*app), GFP_KERNEL);
	if (!app) {
		print_err("%s: Out of memory\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	app_info = pr_app_info_create(mb);
	if (IS_ERR(app_info)) {
		ret = PTR_ERR(app_info);
		goto free_app_inst;
	}

	app->info = app_info;
	app->pfg = NULL;
	INIT_LIST_HEAD(&app->list);
	INIT_LIST_HEAD(&app->bin_head);

	cnt_probe = pr_probe_desc_list_create(mb, &probe_head);
	if (cnt_probe < 0) {
		ret = cnt_probe;
		goto free_app_info;
	} else if (cnt_probe) {
		struct pr_bin_desc *bin;

		bin = pr_bin_desc_create(app_info->path, &probe_head);
		if (IS_ERR(bin)) {
			pr_probe_desc_list_free(&probe_head);
			ret = PTR_ERR(bin);
			goto free_app_info;
		}

		/* add pr_bin_desc */
		list_add(&bin->list, &app->bin_head);
	}

	cnt_bin = bin_info_list_create(mb, &app->bin_head);
	if (cnt_probe < 0) {
		ret = cnt_bin;
		goto free_bins;
	}

	return app;

free_bins:
	bin_info_list_free(&app->bin_head);
free_app_info:
	pr_app_info_free(app_info);
free_app_inst:
	kfree(app);
	return ERR_PTR(ret);
}

/**
 * @brief pr_app_desc cleanup.
 *
 * @param ai Pointer to the target app_inst_data.
 * @return Void.
 */
void pr_app_desc_free(struct pr_app_desc *app)
{
	bin_info_list_free(&app->bin_head);
	pr_app_info_free(app->info);
	kfree(app);
}


/* ============================================================================
 * ==                                US_INST                                 ==
 * ============================================================================
 */

/**
 * @brief Creates and fills us_inst_data struct.
 *
 * @param mb Pointer to the message buffer.
 * @param head Pointer to the list head.
 * @return u32 count of created elements.
 */
u32 create_us_inst_data(struct msg_buf *mb,
			struct list_head *head)
{
	u32 cnt, i;

	print_parse_debug("us_inst_data:\n");

	print_parse_debug("app count:");
	if (get_u32(mb, &cnt)) {
		print_err("failed to read count of applications\n");
		return 0;
	}

	if (remained_mb(mb) / MIN_SIZE_APP_INST < cnt) {
		print_err("to match count of applications(%u)\n", cnt);
		return 0;
	}

	for (i = 0; i < cnt; ++i) {
		struct pr_app_desc *app;

		print_parse_debug("app #%d:\n", i + 1);
		app = pr_app_desc_create(mb);
		if (IS_ERR(app))
			goto err;

		list_add_tail(&app->list, head);
	}

	return cnt;

err:
	destroy_us_inst_data(head);
	return 0;
}

/**
 * @brief us_inst_data cleanup.
 *
 * @param head Pointer to the list head.
 * @return Void.
 */
void destroy_us_inst_data(struct list_head *head)
{
	struct pr_app_desc *app, *n;

	list_for_each_entry_safe(app, n, head, list) {
		list_del(&app->list);
		pr_app_desc_free(app);
	}
}
