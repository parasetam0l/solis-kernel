/*
 * cyttsp5_samsung_factory.c
 * Cypress TrueTouch(TM) Standard Product V5 Device Access module.
 * Configuration and Test command/status user interface.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <linux/slab.h>
#include <linux/err.h>

#include "cyttsp5_regs.h"
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif

/************************************************************************
 * Macros, Structures
 ************************************************************************/

enum {
	BUILT_IN = 0,
	UMS,
};

#define MAX_NODE_NUM 900 /* 30 * 30 */
#define MAX_INPUT_HEADER_SIZE 12
#define MAX_GIDAC_NODES 32
#define MAX_LIDAC_NODES (MAX_GIDAC_NODES * 30)

enum {
	FACTORYCMD_WAITING,
	FACTORYCMD_RUNNING,
	FACTORYCMD_OK,
	FACTORYCMD_FAIL,
	FACTORYCMD_NOT_APPLICABLE
};

enum {
	IDAC_GLOBAL,
	IDAC_LOCAL,
};
#define FACTORY_CMD(name, func) .cmd_name = name, .cmd_func = func

struct factory_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func)(void *device_data);
};

/************************************************************************
 * function def
 ************************************************************************/
static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void get_mutual_cap(void *device_data);
static void get_raw_count(void *device_data);
static void get_difference(void *device_data);
static void get_local_idac(void *device_data);
static void get_global_idac(void *device_data);
static void run_raw_count_read(void *device_data);
static void run_difference_read(void *device_data);
static void run_local_idac_read(void *device_data);
static void run_global_idac_read(void *device_data);
static void run_mutual_cap_read(void *device_data);
static void report_rate(void *device_data);
static void run_connect_test(void *device_data);
static void get_chip_pwrstate(void *device_data);
static void run_calibration(void *device_data);
static void get_checksum_data(void *device_data);
static void get_fw_verify_result(void *device_data);
static void not_support_cmd(void *device_data);

/************************************************************************
 * cmd table
 ************************************************************************/
struct factory_cmd factory_cmds[] = {
	{FACTORY_CMD("fw_update", fw_update),},
	{FACTORY_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{FACTORY_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{FACTORY_CMD("get_config_ver", get_config_ver),},
	{FACTORY_CMD("get_threshold", get_threshold),},
	{FACTORY_CMD("module_off_master", module_off_master),},
	{FACTORY_CMD("module_on_master", module_on_master),},
	{FACTORY_CMD("module_off_slave", not_support_cmd),},
	{FACTORY_CMD("module_on_slave", not_support_cmd),},
	{FACTORY_CMD("get_chip_vendor", get_chip_vendor),},
	{FACTORY_CMD("get_chip_name", get_chip_name),},
	{FACTORY_CMD("get_x_num", get_x_num),},
	{FACTORY_CMD("get_y_num", get_y_num),},
	{FACTORY_CMD("get_mutual_cap", get_mutual_cap),},
	{FACTORY_CMD("get_raw_count", get_raw_count),},
	{FACTORY_CMD("get_difference", get_difference),},
	{FACTORY_CMD("get_local_idac", get_local_idac),},
	{FACTORY_CMD("get_global_idac", get_global_idac),},
	{FACTORY_CMD("run_raw_count_read", run_raw_count_read),},
	{FACTORY_CMD("run_difference_read", run_difference_read),},
	{FACTORY_CMD("run_local_idac_read", run_local_idac_read),},
	{FACTORY_CMD("run_global_idac_read", run_global_idac_read),},
	{FACTORY_CMD("run_mutual_cap_read", run_mutual_cap_read),},
	{FACTORY_CMD("report_rate", report_rate),},
	{FACTORY_CMD("tsp_connect_test", run_connect_test),},
	{FACTORY_CMD("clear_cover_mode", not_support_cmd),},
	{FACTORY_CMD("get_chip_pwrstate", get_chip_pwrstate),},
	{FACTORY_CMD("calibration", run_calibration),},
	{FACTORY_CMD("get_checksum_data", get_checksum_data),},
	{FACTORY_CMD("run_fw_verify", get_fw_verify_result),},
	{FACTORY_CMD("not_support_cmd", not_support_cmd),},
};

/************************************************************************
 * helpers
 ************************************************************************/
static void set_cmd_result(struct cyttsp5_samsung_factory_data *sfd,
		char *strbuff, int len)
{
	strlcat(sfd->factory_cmd_result, strbuff, FACTORY_CMD_RESULT_STR_LEN);
}

static void set_default_result(struct cyttsp5_samsung_factory_data *sfd)
{
	char *delim = ":";

	memset(sfd->factory_cmd_result, 0x00, FACTORY_CMD_RESULT_STR_LEN);
	memcpy(sfd->factory_cmd_result, sfd->factory_cmd,
		strlen(sfd->factory_cmd));
	strlcat(sfd->factory_cmd_result, delim, FACTORY_CMD_RESULT_STR_LEN);
}

/************************************************************************
 * commands
 ************************************************************************/
static int fw_update_ums(struct cyttsp5_samsung_factory_data *sfd)
{
	struct file *fp;
	mm_segment_t old_fs;
	int fw_size, nread;
	int error = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(CY_FW_FILE_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(sfd->dev, "%s: failed to open firmware. %s\n",
			__func__, CY_FW_FILE_PATH);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size > 0) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,
			fw_size, &fp->f_pos);

		dev_info(sfd->dev, "%s: start, file path %s, size %u Bytes\n",
			__func__, CY_FW_FILE_PATH, fw_size);

		if (nread != fw_size) {
			dev_err(sfd->dev, "%s: failed to read firmware file, nread %u Bytes\n",
				__func__, nread);
			error = -EIO;
		} else {
#ifdef CYTTSP5_BINARY_FW_UPGRADE
			error = upgrade_firmware_from_sdcard(sfd->dev,
				(const u8 *)fw_data, fw_size);
#endif
		}

		if (error < 0)
			dev_err(sfd->dev,
				"%s: failed update firmware\n", __func__);

		kfree(fw_data);
	}

	filp_close(fp, current->files);

open_err:
	set_fs(old_fs);
	return error;
}

static void get_fw_verify_result(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_core_commands *core_cmd = cyttsp5_get_commands();
	struct cyttsp5_core_nonhid_cmd *nonhid_cmd = core_cmd->cmd;
	char strbuff[16] = {0};
	int i, rc = 0;
	u16 calculated_crc, stored_crc;
	u8 result;

	set_default_result(sfd);

	/* verify data block crc */
	for (i = CY_TCH_PARM_EBID; i <= CY_DDATA_EBID; i++) {
		rc = nonhid_cmd->verify_config_block_crc(sfd->dev, 0,
			i, &result, &calculated_crc, &stored_crc);
		if (rc) {
			dev_err(sfd->dev, "%s: failed verify_config_block_crc. rc=%d, i=%d\n", __func__, rc, i);
			goto out;
		}
		if ((result) || (calculated_crc != stored_crc)) {
			dev_err(sfd->dev,
				"%s: failed verify_config_block_crc result.[%d][0x%x][0x%x][%d]\n",
				__func__, result, calculated_crc, stored_crc, i);
			goto out;
		}
	}

	/* verify application integrity */
	/* change to bootloader mode */
	rc = nonhid_cmd->start_bl(sfd->dev, 0);
	if (rc) {
		dev_err(sfd->dev, "%s: failed start_bl. rc=%d\n", __func__, rc);
		goto out;
	}

	rc = nonhid_cmd->verify_app_integrity(sfd->dev, 0, &result);
	if ((rc)) {
		dev_err(sfd->dev, "%s: failed verify_app_integrity. [%d]\n", __func__, rc);
		goto app_out;
	}
	if (!result) {
		dev_err(sfd->dev,
			"%s: failed verify_app_integrity result.[%d]\n", __func__, result);
		goto app_out;
	}

	snprintf(strbuff, sizeof(strbuff), "%s", "OK");
	sfd->factory_cmd_state = FACTORYCMD_OK;

app_out:
	/* change to application mode */
	rc = nonhid_cmd->launch_app(sfd->dev, 0);
	if (rc) {
		dev_err(sfd->dev, "%s: failed launch_app. rc=%d\n", __func__, rc);
		goto out;
	}
out:
	if (sfd->factory_cmd_state != FACTORYCMD_OK) {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
			strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void fw_update(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	cyttsp5_upgrade_firmware_from_builtin upgrade_firmware_from_builtin;
	char strbuff[16] = {0};
	int rc = 0;

	set_default_result(sfd);

	switch (sfd->factory_cmd_param[0]) {
	case BUILT_IN:
		upgrade_firmware_from_builtin =
			cyttsp5_request_upgrade_firmware_from_builtin(sfd->dev);
		if (upgrade_firmware_from_builtin != NULL)
			rc = upgrade_firmware_from_builtin(sfd->dev, true);
		else
			rc = -1;
		break;
	case UMS:
		rc = fw_update_ums(sfd);
		break;
	default:
		dev_err(sfd->dev, "%s: parameter %d is wrong\n",
			__func__, sfd->factory_cmd_param[0]);
		rc = -1;
		break;
	}

	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%s", "OK");
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		dev_err(sfd->dev, "%s: rc=%d\n", __func__, rc);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
			strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_fw_ver_bin(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_platform_data *pdata = dev_get_platdata(sfd->dev);
	struct cyttsp5_touch_firmware *fw = pdata->loader_pdata->fw;
	char strbuff[16] = {0};

	set_default_result(sfd);

	if (fw) {
		snprintf(strbuff, sizeof(strbuff), "CY%02x%04x",
			fw->hw_version, fw->fw_version);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "CY%02x%02x%02x",
		sti->hw_version, sti->fw_versionh, sti->fw_versionl);
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_config_ver(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);
	char strbuff[16] = {0};

	set_default_result(sfd);
#ifdef SAMSUNG_TSP_INFO
	snprintf(strbuff, sizeof(strbuff), "%u", sti->config_version);

	sfd->factory_cmd_state = FACTORYCMD_OK;
#else
	snprintf(strbuff, sizeof(strbuff), "%s", "NG");

	sfd->factory_cmd_state = FACTORYCMD_FAIL;
#endif
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_threshold(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);
	char strbuff[16] = {0};
	int rc = 0;

	set_default_result(sfd);

	snprintf(strbuff, sizeof(strbuff), "%u",
		get_unaligned_be16(&sti->thresholdh));
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	if (rc < 0)
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	else
		sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void module_off_master(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	char strbuff[3] = {0};

	set_default_result(sfd);

	cd->cpdata->power(cd->cpdata, 0, cd->dev, 0);

	snprintf(strbuff, sizeof(strbuff), "%s", "OK");
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
		sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void module_on_master(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	char strbuff[3] = {0};

	set_default_result(sfd);

	cd->cpdata->power(cd->cpdata, 1, cd->dev, 0);

	snprintf(strbuff, sizeof(strbuff), "%s", "OK");
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
		sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_chip_vendor(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "%s", "Cypress");
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_chip_name(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "%s", "CYTMA525");
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_x_num(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);

	snprintf(strbuff, sizeof(strbuff), "%u",
		sfd->si->sensing_conf_data.electrodes_x);
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_y_num(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);

	snprintf(strbuff, sizeof(strbuff), "%u",
		sfd->si->sensing_conf_data.electrodes_y);

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_OK;
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static inline s16 node_value_s16(u8 *buf, u16 node)
{
	return (s16)get_unaligned_le16(buf + node * 2);
}

static inline int node_offset(struct cyttsp5_samsung_factory_data *sfd,
	int x, int y)
{
	struct cyttsp5_samsung_tsp_info_dev* sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);

	return (sti->gidac_nodes == sfd->si->sensing_conf_data.electrodes_x) ? /*x is tx*/
			y + sfd->si->sensing_conf_data.electrodes_y * x
			:
			x + sfd->si->sensing_conf_data.electrodes_x * y;
}

static void get_raw_mutual_cap(struct cyttsp5_samsung_factory_data *sfd,
	struct cyttsp5_sfd_self_test_data *self_test_data)
{
	s16 value = 0;
	char strbuff[16] = {0};

	set_default_result(sfd);

	if (sfd->factory_cmd_param[0] < 0 ||
		sfd->factory_cmd_param[0] >= sfd->si->sensing_conf_data.electrodes_x ||
		sfd->factory_cmd_param[1] < 0 ||
		sfd->factory_cmd_param[1] >= sfd->si->sensing_conf_data.electrodes_y) {
		dev_err(sfd->dev,
			"%s: parameter wrong param[0]=%d param[1]=%d\n", __func__,
			sfd->factory_cmd_param[0], sfd->factory_cmd_param[1]);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		value = (s16)get_unaligned_le16(self_test_data->buf +
			node_offset(sfd, sfd->factory_cmd_param[0],
					sfd->factory_cmd_param[1]) * 2);

		snprintf(strbuff, sizeof(strbuff), "%d", value);
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_OK;

		dev_info(sfd->dev, "%s: node [%d,%d] = %d\n", __func__,
			sfd->factory_cmd_param[0], sfd->factory_cmd_param[1], value);
	}
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}


static void get_raw_diff(struct cyttsp5_samsung_factory_data *sfd,
	struct cyttsp5_sfd_panel_scan_data *panel_scan_data)
{
	s16 value = 0;
	char strbuff[16] = {0};

	set_default_result(sfd);

	if (sfd->factory_cmd_param[0] < 0 ||
		sfd->factory_cmd_param[0] >= sfd->si->sensing_conf_data.electrodes_x ||
		sfd->factory_cmd_param[1] < 0 ||
		sfd->factory_cmd_param[1] >= sfd->si->sensing_conf_data.electrodes_y) {
		dev_err(sfd->dev,
			"%s: parameter wrong param[0]=%d param[1]=%d\n", __func__,
			sfd->factory_cmd_param[0], sfd->factory_cmd_param[1]);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		u16 node = node_offset(sfd, sfd->factory_cmd_param[0],
			sfd->factory_cmd_param[1]);

		if (panel_scan_data->element_size == 2)
			value = node_value_s16(panel_scan_data->buf + CY_CMD_RET_PANEL_HDR,
				node);
		else
			value = panel_scan_data->buf[CY_CMD_RET_PANEL_HDR + node];

		snprintf(strbuff, sizeof(strbuff), "%d", value);
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_OK;

		dev_info(sfd->dev, "%s: node [%d,%d] = %d\n", __func__,
			sfd->factory_cmd_param[0], sfd->factory_cmd_param[1], value);
	}
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_raw_count(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_raw_diff(sfd, &sfd->raw);
}

static void get_difference(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_raw_diff(sfd, &sfd->diff);
}

static void get_mutual_cap(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	get_raw_mutual_cap(sfd, &sfd->mutual_cap);
}

#define X_NUM_8X8	8
typedef struct xy {u8 x; u8 y;} XY;
static const XY excluded_nodes_7x7[] = {
	{0, 0}, {0, 1}, {1, 0},
	{0, 5}, {0, 6}, {1, 6},
	{5, 0}, {6, 0}, {6, 1},
	{5, 6}, {6, 5}, {6, 6},
};

static const XY excluded_nodes_8x8[] = {
	{0, 0}, {0, 1}, {1, 0},
	{0, 6}, {0, 7}, {1, 7},
	{6, 0}, {7, 0}, {7, 1},
	{6, 7}, {7, 6}, {7, 7},
};

static bool node_excluded(struct cyttsp5_samsung_factory_data *sfd, u8 x, u8 y)
{
	int i;
	int max_x = sfd->si->sensing_conf_data.electrodes_x;

	if (max_x >= X_NUM_8X8) {
		for (i = 0; i < ARRAY_SIZE(excluded_nodes_8x8); i++) {
			if (x == excluded_nodes_8x8[i].x &&
				y == excluded_nodes_8x8[i].y)
				return true;
		}

	} else {
		for (i = 0; i < ARRAY_SIZE(excluded_nodes_7x7); i++) {
			if (x == excluded_nodes_7x7[i].x &&
				y == excluded_nodes_7x7[i].y)
				return true;
		}
	}

	return false;
}

#define NUM_Y (sfd->si->sensing_conf_data.electrodes_y)

static void find_max_min_s16(struct cyttsp5_samsung_factory_data *sfd,
	u8 *buf, int num_nodes, s16 *max_value,	s16 *min_value)
{
	int i;
	*max_value = 0x8000;
	*min_value = 0x7FFF;

	for (i = 0; i < num_nodes; i++) {
		if (node_excluded(sfd, i/NUM_Y, i%NUM_Y)) {
			dev_vdbg(sfd->dev, "%s: i%d, x%d, x%d v%d excluded\n",
				__func__, i, i/NUM_Y, i%NUM_Y, (s16)get_unaligned_le16(buf));
		} else {
		*max_value = max((s16)*max_value, (s16)get_unaligned_le16(buf));
		*min_value = min((s16)*min_value, (s16)get_unaligned_le16(buf));
		}
		buf += 2;
	}
}
static void find_max_min_s8(u8 *buf, int num_nodes, s16 *max_value,
					s16 *min_value)
{
	int i;
	*max_value = 0x8000;
	*min_value = 0x7FFF;

	for (i = 0; i < num_nodes; i++) {
		*max_value = max((s8)*max_value, (s8)(*buf));
		*min_value = min((s8)*min_value, (s8)(*buf));
		buf += 1;
	}
}

static int retrieve_panel_scan(struct cyttsp5_samsung_factory_data *sfd,
		u8 *buf, u8 data_id, int num_nodes, u8 *r_element_size)
{
	int rc = 0;
	int elem = num_nodes;
	int elem_offset = 0;
	u16 actual_read_len;
	u8 config;
	u16 length;
	u8 *buf_offset;
	u8 element_size = 0;

	/* fill buf with header and data */
	rc = sfd->corecmd->cmd->retrieve_panel_scan(sfd->dev, 0, elem_offset,
		elem, data_id, buf, &config, &actual_read_len, NULL);
	if (rc < 0)
		goto end;

	length = get_unaligned_le16(buf);
	buf_offset = buf + length;

	element_size = config & CY_CMD_RET_PANEL_ELMNT_SZ_MASK;
	*r_element_size = element_size;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		/* append data to the buf */
		rc = sfd->corecmd->cmd->retrieve_panel_scan(sfd->dev, 0,
				elem_offset, elem, data_id, NULL, &config,
				&actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len * element_size;
		buf_offset = buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}

static int panel_scan_and_retrieve(struct cyttsp5_samsung_factory_data *sfd,
		u8 data_id, struct cyttsp5_sfd_panel_scan_data *panel_scan_data)
{
	struct device *dev = sfd->dev;
	int rc = 0;

	dev_dbg(sfd->dev, "%s, line=%d\n", __func__, __LINE__);

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = sfd->corecmd->cmd->exec_panel_scan(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: exec panel scan failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = retrieve_panel_scan(sfd, panel_scan_data->buf, data_id,
		sfd->num_all_nodes, &panel_scan_data->element_size);
	if (rc < 0) {
		dev_err(dev, "%s: retrieve_panel_scan raw count failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	if (panel_scan_data->element_size == 2)
		find_max_min_s16(sfd, panel_scan_data->buf + CY_CMD_RET_PANEL_HDR,
			sfd->num_all_nodes, &panel_scan_data->max,
			&panel_scan_data->min);
	else
		find_max_min_s8(panel_scan_data->buf + CY_CMD_RET_PANEL_HDR,
			sfd->num_all_nodes, &panel_scan_data->max,
			&panel_scan_data->min);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
	}

release_exclusive:
	if (cyttsp5_release_exclusive(dev) < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

static void print_raw_diff_log(struct cyttsp5_samsung_factory_data *sfd,
					u8 *buf)
{
	int x, y;
	int x_num = sfd->si->sensing_conf_data.electrodes_x;
	int y_num = sfd->si->sensing_conf_data.electrodes_y;

	dev_info(sfd->dev, "%s: xnum=%d,ynum=%d\n", __func__, x_num, y_num);
	buf += CY_CMD_RET_PANEL_HDR;

	pr_info("%s:	 ", sfd->dev->driver->name);
	for (x = 0; x < x_num; x++)
		pr_cont("[%2d ] ", x);

	pr_cont("\n");

	for (y = 0; y < y_num; y++) {
		pr_info("%s: [%02d]", sfd->dev->driver->name, y);
		for (x = 0; x < x_num; x++)
			pr_cont( "%6d",
				(s16)get_unaligned_le16(buf + node_offset(sfd, x, y) * 2)
				);
		pr_cont("\n");
	}
}

static void run_raw_count_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = panel_scan_and_retrieve(sfd, CY_MUT_RAW, &sfd->raw);
	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%d,%d",
			sfd->raw.min, sfd->raw.max);
		print_raw_diff_log(sfd, sfd->raw.buf);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void run_difference_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = panel_scan_and_retrieve(sfd, CY_MUT_DIFF, &sfd->diff);
	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%d,%d",
			sfd->diff.min, sfd->diff.max);
		print_raw_diff_log(sfd, sfd->diff.buf);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

/************************************************************************
 * commands - IDAC
 ************************************************************************/
static u16 gidac_node_num(struct cyttsp5_samsung_factory_data *sfd)
{
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);

	return sti->gidac_nodes;
}
static u16 lidac_node_num(struct cyttsp5_samsung_factory_data *sfd)
{
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);

	return sti->gidac_nodes * sti->rx_nodes;
}

static void find_max_min_u8(u8 *buf, int num_nodes, u8 *max_value,
					u8 *min_value)
{
	int i;
	*max_value = 0x00;
	*min_value = 0xff;

	for (i = 0; i < num_nodes; i++) {
		*max_value = max((u8)*max_value, (u8)(*buf));
		*min_value = min((u8)*min_value, (u8)(*buf));
		buf += 1;
	}
}

static void find_max_min_lidac(struct cyttsp5_samsung_factory_data *sfd,
	u8 *buf, int num_nodes, u8 *max_value, u8 *min_value)
{
	int i;
	*max_value = 0x00;
	*min_value = 0xff;

	for (i = 0; i < num_nodes; i++) {
		if (node_excluded(sfd, i/NUM_Y, i%NUM_Y)) {
			dev_vdbg(sfd->dev, "%s: i%d, x%d, x%d v%d excluded\n",
				__func__, i, i/NUM_Y, i%NUM_Y, (u8)(*buf));
		} else {
			*max_value = max((u8)*max_value, (u8)(*buf));
			*min_value = min((u8)*min_value, (u8)(*buf));
		}
		buf += 1;
	}
}

static int retrieve_data_structure(struct cyttsp5_samsung_factory_data *sfd,
		u8 data_id, u8 *buf, int num_nodes)
{
	int rc = 0;
	int elem = num_nodes;
	int elem_offset = 0;
	u16 actual_read_len;
	u8 config;
	u16 length;
	u8 *buf_offset;

	/* fill buf with header and data */
	rc = sfd->corecmd->cmd->retrieve_data_structure(sfd->dev, 0,
		elem_offset, elem, data_id, buf, &config,
		&actual_read_len, NULL);
	if (rc < 0)
		goto end;

	length = get_unaligned_le16(buf);
	buf_offset = buf + length;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		/* append data to the buf */
		rc = sfd->corecmd->cmd->retrieve_data_structure(sfd->dev, 0,
				elem_offset, elem, data_id, NULL, &config,
				&actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len;
		buf_offset = buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}

static void get_global_idac(void *device_data)
{
	struct cyttsp5_samsung_factory_data* sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	u8 *buf = sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR;
	u8 value = 0;
	char strbuff[16] = {0};

	set_default_result(sfd);

	if (sfd->factory_cmd_param[0] < 0 ||
		sfd->factory_cmd_param[0] >= gidac_node_num(sfd)) {
		dev_err(sfd->dev, "%s: parameter %d is wrong\n",
				__func__, sfd->factory_cmd_param[0]);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		value = buf[(u8)sfd->factory_cmd_param[0]];

		snprintf(strbuff, sizeof(strbuff), "%d", value);
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_OK;

		dev_info(sfd->dev, "%s: node %d = %d\n",
				__func__, sfd->factory_cmd_param[0], value);
	}
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_local_idac(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	u8 *buf = sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR + gidac_node_num(sfd);
	u8 value = 0;

	char strbuff[16] = {0};

	set_default_result(sfd);

	if (sfd->factory_cmd_param[0] < 0 ||
		sfd->factory_cmd_param[0] >= sfd->si->sensing_conf_data.electrodes_x ||
		sfd->factory_cmd_param[1] < 0 ||
		sfd->factory_cmd_param[1] >= sfd->si->sensing_conf_data.electrodes_y) {
		dev_err(sfd->dev,
			"%s: parameter wrong param[0]=%d param[1]=%d\n", __func__,
			sfd->factory_cmd_param[0], sfd->factory_cmd_param[1]);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		u16 node = node_offset(sfd, sfd->factory_cmd_param[0],
			sfd->factory_cmd_param[1]);

		value = buf[node];

		snprintf(strbuff, sizeof(strbuff), "%d", value);
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

		sfd->factory_cmd_state = FACTORYCMD_OK;

		dev_info(sfd->dev, "%s: node [%d,%d] = %d\n", __func__,
			sfd->factory_cmd_param[0], sfd->factory_cmd_param[1], value);
	}
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static int retrieve_mutual_idac(struct cyttsp5_samsung_factory_data *sfd,
		u8 type)
{
	struct device *dev = sfd->dev;
	int rc = 0;

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = retrieve_data_structure(sfd, CY_PWC_MUT, sfd->mutual_idac.buf,
			gidac_node_num(sfd)+lidac_node_num(sfd));
	if (rc < 0) {
		dev_err(dev, "%s: retrieve_data_structure failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}
	if (type == IDAC_GLOBAL)
		find_max_min_u8(sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR,
			gidac_node_num(sfd), &sfd->mutual_idac.gidac_max,
			&sfd->mutual_idac.gidac_min);
	else
		find_max_min_lidac(sfd, sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR
			+ gidac_node_num(sfd), lidac_node_num(sfd),
			&sfd->mutual_idac.lidac_max,
			&sfd->mutual_idac.lidac_min);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

release_exclusive:
	rc = cyttsp5_release_exclusive(dev);
	if (rc < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

static void print_idac_log(struct cyttsp5_samsung_factory_data *sfd,
					u8 type, u8 *buf)
{
	struct cyttsp5_samsung_tsp_info_dev* sti =
			cyttsp5_get_samsung_tsp_info(sfd->dev);
	int tx, rx;
	int tx_num = sti->gidac_nodes;
	int rx_num = sti->rx_nodes;
	if (type == IDAC_GLOBAL)
		rx_num = 1;
	dev_info(sfd->dev, "%s: tx num%d rx_num%d\n", __func__,
			tx_num, rx_num);

	pr_info("%s:	   ", sfd->dev->driver->name);
	for (tx = 0; tx < tx_num; tx++)
		pr_cont("[%3d]", tx);

	for (rx = 0; rx < rx_num; rx++) {
		pr_info("%s: [%02d]", sfd->dev->driver->name, rx);
		for (tx = 0; tx < tx_num; tx++)
			pr_cont( "%5d", *(buf + rx + tx * rx_num) );
		pr_cont("\n");
	}
}

static void run_idac_read(struct cyttsp5_samsung_factory_data *sfd,
	u8 type)
{
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = retrieve_mutual_idac(sfd, type);
	if (rc == 0) {
		if (type == IDAC_GLOBAL) {
			snprintf(strbuff, sizeof(strbuff), "%d,%d",
				sfd->mutual_idac.gidac_min,
				sfd->mutual_idac.gidac_max);
			print_idac_log(sfd, IDAC_GLOBAL,
				sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR);
		} else {
			snprintf(strbuff, sizeof(strbuff), "%d,%d",
				sfd->mutual_idac.lidac_min,
				sfd->mutual_idac.lidac_max);
			print_idac_log(sfd, IDAC_LOCAL,
				sfd->mutual_idac.buf + CY_CMD_RET_PANEL_HDR	+ gidac_node_num(sfd));
		}
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void run_global_idac_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	run_idac_read(sfd, IDAC_GLOBAL);
}

static void run_local_idac_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;

	run_idac_read(sfd, IDAC_LOCAL);
}

static int get_self_test_result(struct cyttsp5_samsung_factory_data *sfd,
		u8 *buf, u8 test_id, int num_nodes)
{
	int rc = 0;
	int elem = num_nodes * 2;
	int elem_offset = 0;
	u16 actual_read_len;
	u16 length;
	u8 *buf_offset;

	/* fill buf with header and data */
	rc = sfd->corecmd->cmd->get_self_test_result(sfd->dev, 0,
			elem_offset, elem, test_id, &actual_read_len, buf);
	if (rc < 0)
		goto end;

	length = actual_read_len;
	buf_offset = buf + actual_read_len;

	elem -= actual_read_len;
	elem_offset = actual_read_len;
	while (elem > 0) {
		/* append data to the buf */
		rc = sfd->corecmd->cmd->get_self_test_result(sfd->dev, 0,
				elem_offset, elem, test_id, &actual_read_len, buf_offset);
		if (rc < 0)
			goto end;

		if (!actual_read_len)
			break;

		length += actual_read_len;
		buf_offset = buf + length;
		elem -= actual_read_len;
		elem_offset += actual_read_len;
	}
end:
	return rc;
}

static int self_test_and_get_result(struct cyttsp5_samsung_factory_data *sfd,
		u8 test_id, u8 write_idacs, struct cyttsp5_sfd_self_test_data *self_test_data)
{
	struct device *dev = sfd->dev;
	int rc = 0;
	u8 result = 0;
	u8 available = 0;

	dev_dbg(sfd->dev, "%s, line=%d\n", __func__, __LINE__);

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = sfd->corecmd->cmd->run_self_test(dev, 0, test_id, write_idacs, &result, &available);
	if (rc < 0) {
		dev_err(dev, "%s: run self test failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = get_self_test_result(sfd, self_test_data->buf, test_id, sfd->num_all_nodes);
	if (rc < 0) {
		dev_err(dev, "%s: get_self_test_result failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	find_max_min_s16(sfd, self_test_data->buf, sfd->num_all_nodes,
		&self_test_data->max, &self_test_data->min);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);
	}

release_exclusive:
	if (cyttsp5_release_exclusive(dev) < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

static void print_mutual_cap_log(struct cyttsp5_samsung_factory_data *sfd,
					u8 *buf)
{
	int x, y;
	int x_num = sfd->si->sensing_conf_data.electrodes_x;
	int y_num = sfd->si->sensing_conf_data.electrodes_y;

	dev_info(sfd->dev, "%s: xnum=%d,ynum=%d\n", __func__, x_num, y_num);

	pr_info("%s:	 ", sfd->dev->driver->name);
	for (x = 0; x < x_num; x++)
		pr_cont("[%2d ]", x);

	pr_cont("\n");

	for (y = 0; y < y_num; y++) {
		pr_info("%s: [%02d]", sfd->dev->driver->name, y);
		for (x = 0; x < x_num; x++)
			pr_cont( "%5d",
				(s16)get_unaligned_le16(buf + node_offset(sfd, x, y) * 2)
				);
		pr_cont("\n");
	}
}

static void run_mutual_cap_read(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = self_test_and_get_result(sfd, CY_ST_ID_CM_PANEL, 0, &sfd->mutual_cap);
	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%d,%d",
			sfd->mutual_cap.min, sfd->mutual_cap.max);
		print_mutual_cap_log(sfd, sfd->mutual_cap.buf);
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

	set_cmd_result(sfd, strbuff, strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

extern int cyttsp5_hid_output_get_cal_status(struct device *dev, u8* status);
static int get_cal_status(struct cyttsp5_samsung_factory_data *sfd,
	u8* status)
{
	struct device *dev = sfd->dev;

	int rc = -1;

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->suspend_scanning(dev, 0);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

	rc = cyttsp5_hid_output_get_cal_status(dev, status);
	if (rc < 0)
		dev_err(dev, "%s: error get_cal_status r=%d\n",
			__func__, rc);

	rc = sfd->corecmd->cmd->resume_scanning(dev, 0);
	if (rc < 0)
		dev_err(dev, "%s: resume_scanning failed r=%d\n",
			__func__, rc);

release_exclusive:
	if (cyttsp5_release_exclusive(dev) < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

/************************************************************************
 * report_rate
 ************************************************************************/
#define PARAM_ID_ACT_INTRVL0 0x4D

static int set_param(struct cyttsp5_samsung_factory_data *sfd,
	u8 param_id, u32 value)
{
	struct device *dev = sfd->dev;
	int rc = 0;

	dev_dbg(sfd->dev, "%s\n", __func__);

	rc = cyttsp5_request_exclusive(dev, CY_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		dev_err(dev, "%s: request exclusive failed(%d)\n",
			__func__, rc);
		return rc;
	}

	rc = sfd->corecmd->cmd->set_param(dev, 0, param_id, value);
	if (rc < 0) {
		dev_err(dev, "%s: suspend scanning failed r=%d\n",
			__func__, rc);
		goto release_exclusive;
	}

release_exclusive:
	rc = cyttsp5_release_exclusive(dev);
	if (rc < 0)
		dev_err(dev, "%s: release_exclusive failed r=%d\n",
			__func__, rc);

	return rc;
}

int set_report_rate(struct cyttsp5_core_data *cd, int rate)
{
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;
	struct device *dev = sfd->dev;
	int interval;
	int rc;

	interval = 1000 / rate;

	rc = set_param(sfd, PARAM_ID_ACT_INTRVL0, interval);

	if (!rc)
		dev_info(dev, "%s: report rate changing success. [%d]\n",
			__func__, interval);

	else
		dev_info(dev, "%s: report rate changing failed.[%d]\n",
			__func__, interval);

	return rc;
}

static void report_rate(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	u8 interval;
	int rc;

	set_default_result(sfd);

	if ((sfd->factory_cmd_param[0] < 0) ||
		(sfd->factory_cmd_param[0] > 2)) {
		dev_err(sfd->dev, "%s: parameter %d is wrong\n",
					__func__, sfd->factory_cmd_param[0]);

		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		goto exit;
	}

	switch (sfd->factory_cmd_param[0]) {
	case 1:
		interval = 17/*ms, 60Hz*/;
		break;
	case 2:
		interval = 33/*ms, 30Hz*/;
		break;
	default:
		interval = 10/*ms, 100Hz*/;
		break;
	}

	rc = set_param(sfd, PARAM_ID_ACT_INTRVL0, interval);
	if (rc == 0) {
		snprintf(strbuff, sizeof(strbuff), "%s", "OK");
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

exit:
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

/************************************************************************
 * glove_enable
 ************************************************************************/
int set_glove_enable(struct cyttsp5_core_data *cd, bool enable)
{
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;
	struct device *dev = sfd->dev;
	int rc;

	if (enable)
		rc = set_param(sfd,
			CY_RAM_ID_TOUCHMODE_ENABLED, TOUCHMODE_FINGER_GLOVE);
	else
		rc = set_param(sfd,
			CY_RAM_ID_TOUCHMODE_ENABLED, TOUCHMODE_FINGER_ONLY);

	if (!rc)
		dev_info(dev, "%s: glove %s success.\n",
			__func__, enable ? "enable" : "disable");
	else
		dev_info(dev, "%s: glove %s failed.\n",
			__func__, enable ? "enable" : "disable");

	return rc;
}

/************************************************************************
 * sysfs cmd
 ************************************************************************/
static void run_connect_test(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	u8 status;
	int rc;

	set_default_result(sfd);

	rc = get_cal_status(sfd, &status);
	if (rc) {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
		dev_err(sfd->dev, "%s: cal status get failed.(%d)\n",
			__func__, rc);
		goto out;
	}

	if (!status) {
		snprintf(strbuff, sizeof(strbuff), "%s", "OK");
		sfd->factory_cmd_state = FACTORYCMD_OK;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	}

out:
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_checksum_data(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);

	char strbuff[32] = {0};
	u32 checksum = 0;
	int ret;

	set_default_result(sfd);

	ret = cyttsp5_get_firmware_crc(cd, &checksum);
	if (ret < 0) {
		dev_err(sfd->dev, "%s: failed to get firmware crc info.(%d)\n",
			__func__, ret);
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
		checksum = 0;
	} else
		sfd->factory_cmd_state = FACTORYCMD_OK;

	snprintf(strbuff, sizeof(strbuff), "0x%x", checksum);
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));

	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void get_chip_pwrstate(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[32] = {0};
#if defined(CYTEST_GETPOWERMODE)
	const char* mode_str[PWRMODE_MAX+1] = {
		"PWRMODE_ACTIVE",
		"PWRMODE_LOOKFORTOUCH",
		"PWRMODE_LOWPOWER",
		"PWRMODE_UNKOWN",};
	int power_mode;
#endif

	set_default_result(sfd);

#if defined(CYTEST_GETPOWERMODE)
	power_mode = cyttsp5_test_get_power_mode(sfd->dev);
	if (power_mode > PWRMODE_MAX)
		power_mode = PWRMODE_MAX;

	snprintf(strbuff, sizeof(strbuff), mode_str[power_mode-PWRMODE_ACTIVE]);
	sfd->factory_cmd_state = FACTORYCMD_OK;
#else
	snprintf(strbuff, sizeof(strbuff), "not support commnad");
	sfd->factory_cmd_state = FACTORYCMD_FAIL;
#endif

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void run_calibration(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};
	int rc;

	set_default_result(sfd);

	rc = cyttsp5_fw_calibrate(sfd->dev);
	if (rc) {
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
	} else {
		snprintf(strbuff, sizeof(strbuff), "%s", "OK");
		sfd->factory_cmd_state = FACTORYCMD_OK;

	}

	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	dev_info(sfd->dev, "%s: %s(%d)\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
}

static void not_support_cmd(void *device_data)
{
	struct cyttsp5_samsung_factory_data *sfd =
		(struct cyttsp5_samsung_factory_data *) device_data;
	char strbuff[16] = {0};

	set_default_result(sfd);
	snprintf(strbuff, sizeof(strbuff), "%s", "NA");
	set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	sfd->factory_cmd_state = FACTORYCMD_NOT_APPLICABLE;
	dev_info(sfd->dev, "%s: \"%s(%d)\"\n", __func__,
		strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
	return;
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	struct factory_cmd *factory_cmd_ptr = NULL;
	int param_cnt = 0;
	int ret, len, i;
	char *cur, *start, *end;
	char strbuff[FACTORY_CMD_STR_LEN] = {0};
	char delim = ',';
	bool cmd_found = false;

	if (sfd->factory_cmd_is_running == true) {
		dev_err(sfd->dev, "factory_cmd: other cmd is running.\n");
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = true;
	mutex_unlock(&sfd->factory_cmd_lock);

	sfd->factory_cmd_state = FACTORYCMD_RUNNING;

	for (i = 0; i < ARRAY_SIZE(sfd->factory_cmd_param); i++)
		sfd->factory_cmd_param[i] = 0;

	if (count >= FACTORY_CMD_STR_LEN)
		count = (FACTORY_CMD_STR_LEN -1);

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(sfd->factory_cmd, 0x00, ARRAY_SIZE(sfd->factory_cmd));
	memcpy(sfd->factory_cmd, buf, len);

	cur = strnchr(buf, count, (int)delim);
	if (cur)
		memcpy(strbuff, buf, cur - buf);
	else
		memcpy(strbuff, buf, len);

	/* find command */
	list_for_each_entry(factory_cmd_ptr,
			&sfd->factory_cmd_list_head, list) {
		if (!strcmp(strbuff, factory_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(factory_cmd_ptr,
				&sfd->factory_cmd_list_head, list) {
			if (!strcmp("not_support_cmd",
				factory_cmd_ptr->cmd_name))
				break;
		}
	}

	if (!cd->device_enabled) {
		set_default_result(sfd);
		sfd->factory_cmd_state = FACTORYCMD_FAIL;
		snprintf(strbuff, sizeof(strbuff), "%s", "NG");
		set_cmd_result(sfd, strbuff, (int)strnlen(strbuff, sizeof(strbuff)));
		dev_err(sfd->dev, "%s: TSP is not enabled.\n", __func__);
		goto err_out;
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(strbuff, 0x00, ARRAY_SIZE(strbuff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(strbuff, start, end - start);
				*(strbuff + strlen(strbuff)) = '\0';
				ret = kstrtoint(strbuff, 10,
					sfd->factory_cmd_param + param_cnt);
				start = cur + 1;
				memset(strbuff, 0x00, ARRAY_SIZE(strbuff));
				if ((FACTORY_CMD_PARAM_NUM-1) > param_cnt)
					param_cnt++;

			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(sfd->dev, "cmd = %s\n", factory_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(sfd->dev, "cmd param %d= %d\n", i,
			sfd->factory_cmd_param[i]);

	factory_cmd_ptr->cmd_func(sfd);

err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	char strbuff[16] = {0};

	dev_info(sfd->dev, "tsp cmd: status:%d, PAGE_SIZE=%ld\n",
			sfd->factory_cmd_state, PAGE_SIZE);

	if (sfd->factory_cmd_state == FACTORYCMD_WAITING)
		snprintf(strbuff, sizeof(strbuff), "WAITING");

	else if (sfd->factory_cmd_state == FACTORYCMD_RUNNING)
		snprintf(strbuff, sizeof(strbuff), "RUNNING");

	else if (sfd->factory_cmd_state == FACTORYCMD_OK)
		snprintf(strbuff, sizeof(strbuff), "OK");

	else if (sfd->factory_cmd_state == FACTORYCMD_FAIL)
		snprintf(strbuff, sizeof(strbuff), "FAIL");

	else if (sfd->factory_cmd_state == FACTORYCMD_NOT_APPLICABLE)
		snprintf(strbuff, sizeof(strbuff), "NOT_APPLICABLE");

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuff);
}

static ssize_t show_get_fw_ver_ic(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_tsp_info_dev *sti =
		cyttsp5_get_samsung_tsp_info(sfd->dev);

	dev_info(sfd->dev, "%s: CY%02x%02x%02x", __func__,
		sti->hw_version, sti->fw_versionh, sti->fw_versionl);

	return snprintf(buf, PAGE_SIZE, "CY%02x%02x%02x\n",
		sti->hw_version, sti->fw_versionh, sti->fw_versionl);
}

static ssize_t show_get_fw_ver_bin(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_platform_data *pdata = dev_get_platdata(sfd->dev);
	struct cyttsp5_touch_firmware *fw = pdata->loader_pdata->fw;

	dev_info(sfd->dev, "%s: CY%02x%04x\n", __func__,
		fw->hw_version, fw->fw_version);

	return snprintf(buf, PAGE_SIZE, "CY%02x%04x\n",
			fw->hw_version, fw->fw_version);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);

	dev_info(sfd->dev, "tsp cmd: result: %s\n", sfd->factory_cmd_result);

	mutex_lock(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = false;
	mutex_unlock(&sfd->factory_cmd_lock);

	sfd->factory_cmd_state = FACTORYCMD_WAITING;

	return snprintf(buf, PAGE_SIZE, "%s\n", sfd->factory_cmd_result);
}

static ssize_t show_chip_log(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	struct cyttsp5_platform_data *pdata = dev_get_platdata(sfd->dev);
	struct cyttsp5_touch_firmware *fw = pdata->loader_pdata->fw;
	struct cyttsp5_samsung_tsp_info_dev *sti = &cd->samsung_tsp_info;
	int x_num = sfd->si->sensing_conf_data.electrodes_x;
	int y_num = sfd->si->sensing_conf_data.electrodes_y;
	int x, y, ret, length = 0;
	char *raw_buf, *diff_buf;

	/* get firmware version */
	length += sprintf(buf+length, "chip version: CY%02x%02x%02x\n",
			sti->hw_version, sti->fw_versionh, sti->fw_versionl);
	length += sprintf(buf+length, "bin version: CY%02x%04x\n",
			fw->hw_version, fw->fw_version);

	if (!cd->device_enabled) {
		length += sprintf(buf+length, "%s: device is disabled. [%d]\n",
				__func__, cd->device_enabled);
		return length;
	}

	/* get CY_MUT_RAW */
	length += sprintf(buf+length, "\n[CY_MUT_RAW] x=%d, y=%d\n", x_num, y_num);

	ret = panel_scan_and_retrieve(sfd, CY_MUT_RAW, &sfd->raw);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "%s: raw data get failed.\n", __func__);

	raw_buf = sfd->raw.buf + CY_CMD_RET_PANEL_HDR;

	length += sprintf(buf+length, "	 ");

	for (x = 0; x < x_num; x++) {
		length += sprintf(buf+length, "[%3d]", x);
	}
	length += sprintf(buf+length, "\n");

	for (y = 0; y < y_num; y++) {
		length += sprintf(buf+length, "[%3d]", y);

		for (x = 0; x < x_num; x++) {
			length += sprintf(buf+length, "%5d",
				(s16)get_unaligned_le16(raw_buf + node_offset(sfd, x, y) * 2)
				);
		}
		length += sprintf(buf+length, "\n");
	}

	/* get CY_MUT_DIFF */
	length += sprintf(buf+length, "\n[CY_MUT_DIFF] x=%d, y=%d\n", x_num, y_num);

	ret = panel_scan_and_retrieve(sfd, CY_MUT_DIFF, &sfd->diff);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "%s: raw data get failed.\n", __func__);

	diff_buf = sfd->diff.buf + CY_CMD_RET_PANEL_HDR;

	length += sprintf(buf+length, "	 ");
	for (x = 0; x < x_num; x++) {
		length += sprintf(buf+length, "[%3d]", x);
	}
	length += sprintf(buf+length, "\n");

	for (y = 0; y < y_num; y++) {
		length += sprintf(buf+length, "[%3d]", y);

		for (x = 0; x < x_num; x++) {
			length += sprintf(buf+length, "%5d",
				(s16)get_unaligned_le16(diff_buf + node_offset(sfd, x, y) * 2)
				);
		}
		length += sprintf(buf+length, "\n");
	}

	return length;
}

static ssize_t show_ambient(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);

	dev_info(sfd->dev, "%s: irq_wake=[%d]\n",
			__func__, cd->irq_wake);

	return snprintf(buf, PAGE_SIZE, "%d\n", cd->irq_wake);
}

extern int cyttsp5_core_start(struct device *dev);
static ssize_t store_ambient(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	int ambient_mode = -1;

	sscanf(buf, "%d", &ambient_mode);

	dev_info(sfd->dev, "%s: ambient_mode:[%s]\n",
			__func__, ambient_mode ? "ON":"OFF");

	if (!cd->device_enabled) {
		dev_err(sfd->dev, "%s: TSP is not enabled.\n", __func__);
		return -1;
	}

	if (cd->irq_wake == ambient_mode) {
		dev_info(sfd->dev, "%s: already set. [%d][%d]\n",
			__func__, ambient_mode, cd->irq_wake);
		return count;
	}

	if (ambient_mode) {
		cyttsp5_core_ambient_on(sfd->dev);
	} else {
		cd->irq_wake = false;
		cyttsp5_core_start(cd->dev);
	}

	return count;
}

static ssize_t store_report_rate(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	int report_rate;

	sscanf(buf, "%d", &report_rate);

	if (cd->report_rate != report_rate) {
		cd->report_rate = report_rate;
		if (cd->device_enabled)
			set_report_rate(cd, report_rate);
	}

	dev_info(sfd->dev, "%s: report_rate=[%d]\n",
			__func__, report_rate);
	return count;
}

static ssize_t show_report_rate(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);

	dev_info(sfd->dev, "%s: report_rate=[%d]\n",
			__func__, cd->report_rate);

	return snprintf(buf, PAGE_SIZE, "%d\n", cd->report_rate);
}

static ssize_t store_glove_enable(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);
	int glove_enable;

	sscanf(buf, "%d", &glove_enable);

	if (cd->glove_enable != glove_enable) {
		cd->glove_enable = glove_enable;
		if (cd->device_enabled)
			set_glove_enable(cd, glove_enable);
	}

	dev_info(sfd->dev, "%s: glove_enable=[%d]\n",
			__func__, glove_enable);
	return count;
}

static ssize_t show_glove_enable(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct cyttsp5_samsung_factory_data *sfd = dev_get_drvdata(dev);
	struct cyttsp5_core_data *cd = dev_get_drvdata(sfd->dev);

	dev_info(sfd->dev, "%s: glove_enable=[%d]\n",
			__func__, cd->glove_enable);

	return snprintf(buf, PAGE_SIZE, "%d\n", cd->glove_enable);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(fw_ver_ic, S_IRUGO, show_get_fw_ver_ic, NULL);
static DEVICE_ATTR(fw_ver_bin, S_IRUGO, show_get_fw_ver_bin, NULL);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);
static DEVICE_ATTR(chip_log, S_IRUGO, show_chip_log, NULL);
static DEVICE_ATTR(mode, S_IRUGO | S_IWUSR | S_IWGRP,\
			show_ambient, store_ambient);
static DEVICE_ATTR(report_rate, S_IRUGO | S_IWUSR | S_IWGRP,
			show_report_rate, store_report_rate);
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			show_glove_enable, store_glove_enable);

static struct attribute *sec_touch_factory_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_chip_log.attr,
	&dev_attr_mode.attr,
	&dev_attr_report_rate.attr,
	&dev_attr_glove_mode.attr,
	&dev_attr_fw_ver_ic.attr,
	&dev_attr_fw_ver_bin.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_factory_attributes,
};

/************************************************************************
 * init
 ************************************************************************/
#define SEC_DEV_TOUCH_MAJOR			0
#define SEC_DEV_TSP_MINOR			1
int cyttsp5_samsung_factory_probe(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;
	int rc = 0;
	int i;

	sfd->dev = dev;

	sfd->corecmd = cyttsp5_get_commands();
	if (!sfd->corecmd) {
		dev_err(dev, "%s: core cmd not available\n", __func__);
		rc = -EINVAL;
		goto error_return;
	}

	sfd->si = _cyttsp5_request_sysinfo(dev);
	if (!sfd->si) {
		dev_err(dev,
			"%s: Fail get sysinfo pointer from core\n", __func__);
		rc = -EINVAL;
		goto error_return;
	}

	dev_dbg(dev, "%s: electrodes_x=%d\n", __func__,
		sfd->si->sensing_conf_data.electrodes_x);
	dev_dbg(dev, "%s: electrodes_y=%d\n", __func__,
		sfd->si->sensing_conf_data.electrodes_y);

	sfd->num_all_nodes = sfd->si->sensing_conf_data.electrodes_x *
		sfd->si->sensing_conf_data.electrodes_y;
	if (sfd->num_all_nodes > MAX_NODE_NUM) {
		dev_err(dev,
			"%s: sensor node num(%d) exceeds limits\n", __func__,
			sfd->num_all_nodes);
		rc = -EINVAL;
		goto error_return;
	}

	sfd->raw.buf = kzalloc((MAX_INPUT_HEADER_SIZE +
		sfd->num_all_nodes * 2), GFP_KERNEL);
	if (sfd->raw.buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->raw.buf\n", __func__);
		rc = -ENOMEM;
		goto error_return;
	}

	sfd->diff.buf = kzalloc((MAX_INPUT_HEADER_SIZE +
		sfd->num_all_nodes * 2), GFP_KERNEL);
	if (sfd->diff.buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->diff.buf\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_difference_buf;
	}

	sfd->mutual_idac.buf = kzalloc((MAX_INPUT_HEADER_SIZE +
		gidac_node_num(sfd) + lidac_node_num(sfd)), GFP_KERNEL);
	if (sfd->mutual_idac.buf == NULL) {
		dev_err(dev,
			"%s: Error, kzalloc sfd->mutual_idac.buf\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_idac_buf;
	}

	sfd->mutual_cap.buf = kzalloc((sfd->num_all_nodes * 2), GFP_KERNEL);
	if (sfd->mutual_cap.buf == NULL) {
		dev_err(dev, "%s: Error, kzalloc sfd->mutual_cap.buf\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_mutual_cap_buf;
	}

	INIT_LIST_HEAD(&sfd->factory_cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(factory_cmds); i++)
		list_add_tail(&factory_cmds[i].list,
				&sfd->factory_cmd_list_head);

	mutex_init(&sfd->factory_cmd_lock);
	sfd->factory_cmd_is_running = false;

#ifdef CONFIG_SEC_SYSFS
	sfd->factory_dev = sec_device_create(sfd, "tsp");
	if (IS_ERR(sfd->factory_dev)) {
		dev_err(sfd->dev, "Failed to create sec_device_create\n");
		goto error_device_create;
	}
#else
	if (sec_class) {
		sfd->factory_dev = device_create(sec_class, NULL,
			MKDEV(SEC_DEV_TOUCH_MAJOR, SEC_DEV_TSP_MINOR), sfd, "tsp");
		if (IS_ERR(sfd->factory_dev)) {
			dev_err(sfd->dev, "Failed to create device for the sysfs\n");
			goto error_device_create;
		}
	} else {
		dev_err(sfd->dev, "%s: sec_class is NULL\n", __func__);
		goto error_device_create;
	}
#endif
	rc = sysfs_create_group(&sfd->factory_dev->kobj,
		&sec_touch_factory_attr_group);
	if (rc) {
		dev_err(sfd->dev, "Failed to create sysfs group\n");
		goto error_sysfs_create_group;
	}

	sfd->sysfs_nodes_created = true;
	dev_dbg(sfd->dev, "%s success. rc=%d\n", __func__, rc);
	return 0;

error_sysfs_create_group:
#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(sfd->factory_dev->devt);
#else
	device_destroy(sec_class, sfd->factory_dev->devt);
#endif
error_device_create:
	kfree(sfd->mutual_cap.buf);
error_alloc_mutual_cap_buf:
	kfree(sfd->mutual_idac.buf);
error_alloc_idac_buf:
	kfree(sfd->diff.buf);
error_alloc_difference_buf:
	kfree(sfd->raw.buf);
error_return:
	dev_err(dev, "%s failed. rc=%d\n", __func__, rc);
	return rc;
}

int cyttsp5_samsung_factory_release(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_samsung_factory_data *sfd = &cd->sfd;

	if (sfd->sysfs_nodes_created) {
		sysfs_remove_group(&sfd->factory_dev->kobj,
			&sec_touch_factory_attr_group);
#ifdef CONFIG_SEC_SYSFS
		sec_device_destroy(sfd->factory_dev->devt);
#else
		device_destroy(sec_class, sfd->factory_dev->devt);
#endif
		kfree(sfd->mutual_idac.buf);
		kfree(sfd->diff.buf);
		kfree(sfd->raw.buf);
		sfd->sysfs_nodes_created = false;
	}
	dev_dbg(dev, "%s\n", __func__);

	return 0;
}

