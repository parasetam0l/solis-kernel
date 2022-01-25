/* s6e36w1x01_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * SeungBeom, Park <sb1.parki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "../dsim.h"
#include "s6e36w1x01_dimming_pop.h"
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "s6e36w1x01_param_pop.h"
#include "s6e36w1x01_mipi_lcd.h"

#define	BACKLIGHT_DEV_NAME	"s6e36w1x01-bl"
#define	LCD_DEV_NAME		"s6e36w1x01"

static const char *power_state_str[] = {
	"UNBLANK",
	"NORMAL",
	"VSYNC_SUSPEND",
	"HSYNC_SUSPEND",
	"POWERDOWN",
	"UNKNOWN",
};

static int panel_id;
int get_panel_id(void)
{
	return panel_id;
}
EXPORT_SYMBOL(get_panel_id);

static int __init s6e36w1x01_panel_id_cmdline(char *mode)
{
	char *pt;

	panel_id = 0;
	if (mode == NULL)
		return 1;

	for (pt = mode; *pt != 0; pt++) {
		panel_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			panel_id += *pt - '0';
		break;
		case 'a' ... 'f':
			panel_id += 10 + *pt - 'a';
		break;
		case 'A' ... 'F':
			panel_id += 10 + *pt - 'A';
		break;
		}
	}

	pr_info("%s: panel_id = 0x%x", __func__, panel_id);

	return 0;
}
__setup("lcdtype=", s6e36w1x01_panel_id_cmdline);

static void s6e36w1x01_esd_detect_work(struct work_struct *work)
{
	struct s6e36w1x01 *lcd = container_of(work,
						struct s6e36w1x01, det_work);
	char *event_string = "LCD_ESD=ON";
	char *envp[] = {event_string, NULL};

	if (!POWER_IS_OFF(lcd->power)) {
		kobject_uevent_env(&lcd->esd_dev->kobj,
			KOBJ_CHANGE, envp);
		dev_info(lcd->dev, "%s:Send uevent. ESD DETECTED\n", __func__);
	}
}

irqreturn_t s6e36w1x01_esd_interrupt(int irq, void *dev_id)
{
	struct s6e36w1x01 *lcd = dev_id;

	if (!work_busy(&lcd->det_work)) {
		schedule_work(&lcd->det_work);
		dev_info(lcd->dev, "%s: add esd schedule_work by irq[%d]]\n",
			__func__, irq);
	}

	return IRQ_HANDLED;
}

static ssize_t s6e36w1x01_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "SDC_%06x\n", get_panel_id());
	strcat(buf, temp);

	return strlen(buf);
}
DEVICE_ATTR(lcd_type, 0444, s6e36w1x01_lcd_type_show, NULL);

static ssize_t s6e36w1x01_mcd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->mcd_on ? "on" : "off");
}

extern void s6e36w1x01_mcd_test_on(void);
static ssize_t s6e36w1x01_mcd_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->mcd_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->mcd_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	s6e36w1x01_mcd_test_on();

	return size;
}

static ssize_t s6e36w1x01_hlpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->hlpm_on);
}

extern void s6e36w1x01_hlpm_on(void);
static ssize_t s6e36w1x01_hlpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hlpm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->hlpm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	s6e36w1x01_hlpm_on();

	pr_info("%s: val[%d]\n", __func__, lcd->hlpm_on);

	return size;
}

static ssize_t s6e36w1x01_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	int len = 0;

	pr_debug("%s:val[%d]\n", __func__, lcd->ao_mode);

	switch (lcd->ao_mode) {
	case AO_NODE_OFF:
		len = sprintf(buf, "%s\n", "off");
		break;
	case AO_NODE_ALPM:
		len = sprintf(buf, "%s\n", "on");
		break;
	case AO_NODE_SELF:
		len = sprintf(buf, "%s\n", "self");
		break;
	default:
		dev_warn(dev, "invalid status.\n");
		break;
	}

	return len;
}

static ssize_t s6e36w1x01_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if (lcd->hlpm_on) {
		pr_info("%s:hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->ao_mode = AO_NODE_ALPM;
	else if (!strncmp(buf, "off", 3))
		lcd->ao_mode = AO_NODE_OFF;
	else if (!strncmp(buf, "self", 4))
		lcd->ao_mode = AO_NODE_SELF;
	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

	return size;
}

static ssize_t s6e36w1x01_scm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->scm_on ? "on" : "off");
}

static ssize_t s6e36w1x01_scm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		lcd->scm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->scm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s: val[%d]\n", __func__, lcd->scm_on);

	return size;
}

static void s6e36w1x01_acl_update(struct s6e36w1x01 *lcd, unsigned int value)
{
	/* TODO: implement acl update function */
}

static ssize_t s6e36w1x01_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->acl);
}

static ssize_t s6e36w1x01_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	unsigned long value;
	int rc;

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	s6e36w1x01_acl_update(lcd, value);

	lcd->acl = value;

	dev_info(lcd->dev, "acl control[%d]\n", lcd->acl);

	return size;
}

static ssize_t s6e36w1x01_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

extern void s6e36w1x01_hbm_on(void);
static ssize_t s6e36w1x01_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

#if 0
	if (lcd->power > FB_BLANK_NORMAL) {
		/*
		 *  let the fimd know the smies status
		 *  before DPMS ON
		 */
		ops->set_smies_active(master, lcd->hbm_on);
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}
#endif

	s6e36w1x01_hbm_on();

	dev_info(lcd->dev, "HBM %s.\n", lcd->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t s6e36w1x01_elvss_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", lcd->temp_stage);
}

extern int s6e36w1x01_temp_offset_comp(unsigned int stage);
static ssize_t s6e36w1x01_elvss_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	unsigned long value;
	int rc;

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	lcd->temp_stage = value;

	s6e36w1x01_temp_offset_comp(lcd->temp_stage);

	dev_info(lcd->dev, "ELVSS temp stage[%d].\n", lcd->temp_stage);

	return size;
}

static ssize_t s6e36w1x01_octa_chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	char temp[20];

	sprintf(temp, "%02x%02x%02x%02x%02x\n",
				lcd->chip[0], lcd->chip[1], lcd->chip[2],
				lcd->chip[3], lcd->chip[4]);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t s6e36w1x01_refresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->refresh);
}

static ssize_t s6e36w1x01_refresh_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (strncmp(buf, "30", 2) && strncmp(buf, "60", 2)) {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	/* TODO: implement frame freq set */
	if (!strncmp(buf, "30", 2)) {
		lcd->refresh = 30;
	} else {
		lcd->refresh = 60;
	}

	return size;
}

#define 	MAX_MTP_READ_SIZE	0xff
static unsigned char mtp_read_data[MAX_MTP_READ_SIZE] = {0, };
static int rdata_length = 0;
static ssize_t s6e36w1x01_read_mtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char buffer[LDI_MTP4_LEN*8] = {0, };
	int i, ret = 0;

	for ( i = 0; i < rdata_length; i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", mtp_read_data[i]);
		strcat(buf, buffer);

		if ( i < (rdata_length-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");

	pr_info("%s: length=%d\n", __func__, rdata_length);
	pr_info("%s\n", buf);

	return ret;
}

extern void s6e36w1x01_read_mtp_reg(u32 addr, char* buffer, u32 size);
static ssize_t s6e36w1x01_read_mtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	u32 buff[3] = {0, }; //register, size, offset
	int i;

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_err("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_err("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	sscanf(buf, "0x%x, 0x%x, 0x%x", &buff[0], &buff[1], &buff[2]);
	pr_info("%s: 0x%x, 0x%x, 0x%x\n", __func__, buff[0], buff[1], buff[2]);

	for (i = 0; i < 3; i++) {
		if (buff[i] > MAX_MTP_READ_SIZE)
			return -EINVAL;
	}

	rdata_length = buff[1];

	s6e36w1x01_read_mtp_reg(buff[0]+buff[2],
			&mtp_read_data[0], rdata_length);

	return size;
}

static ssize_t s6e36w1x01_cell_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	char temp[LDI_MDATE_MAX_PARA];
	char result_buff[CELL_ID_LEN+1];

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	memset(&temp[0], 0x00, LDI_MDATE_MAX_PARA);
	s6e36w1x01_read_mtp_reg(LDI_MDATE, &temp[0], LDI_MDATE_MAX_PARA);
	sprintf(result_buff, "%02x%02x%02x%02x%02x%02x%02x",
				temp[7], temp[8], temp[9], temp[10],
				0x00, 0x00, 0x00);
	strcat(buf, result_buff);

	memset(&temp[0], 0x00, WHITE_COLOR_LEN);
	s6e36w1x01_read_mtp_reg(LDI_WHITE_COLOR, &temp[0], WHITE_COLOR_LEN);
	sprintf(result_buff, "%02x%02x%02x%02x\n",
				temp[0], temp[1], temp[2], temp[3]);
	strcat(buf, result_buff);

	pr_info("%s:[%s]", __func__, buf);

	return strlen(buf);
}

static int dimming_table_number = 0;
#define MAX_DIMMING_TABLE	2
#define DIMMING_TABLE_PRINT_SIZE	20
static ssize_t s6e36w1x01_dimming_table_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0) {
		dev_err(lcd->dev,
			"%s: failed to read parameter value\n",
			__func__);
		return -EIO;
	}

	if ((value > MAX_DIMMING_TABLE) ||(value < 0)){
		dev_err(lcd->dev, 	"%s: Invalid value. [%d]\n",
			__func__, (int)value);
		return -EIO;
	}

	dimming_table_number = value;

	dev_info(lcd->dev, "%s: dimming_table[%d]\n",
			__func__, dimming_table_number);

	return size;
}

static ssize_t s6e36w1x01_dimming_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	char buffer[32];
	int ret = 0, carry = 0;
	int i, j;
	int max_cnt;

	if (dimming_table_number == 0) {
		i = 0;
		max_cnt = DIMMING_TABLE_PRINT_SIZE;
	} else if (dimming_table_number == 1) {
		i = DIMMING_TABLE_PRINT_SIZE;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 2;
	} else {
		i = DIMMING_TABLE_PRINT_SIZE * 2;
		max_cnt = MAX_GAMMA_CNT;
	}

	for (; i < max_cnt; i++) {
		ret += sprintf(buffer, "[Gtbl][%2d]", i);
		strcat(buf, buffer);

		for (j = 1; j < GAMMA_CMD_CNT ; j++) {
			if (j == 1 || j == 3 || j == 5) {
				if (lcd->gamma_tbl[i][j])
					carry = lcd->gamma_tbl[i][j]*0x100;
			} else {
				ret += sprintf(buffer, " %3d,",
					lcd->gamma_tbl[i][j]+carry);
				strcat(buf, buffer);
				if (carry)
					carry = 0;
			}
		}
		strcat(buf, "\n");
		ret += strlen("\n");
	}

	return ret;
}

static struct device_attribute ld_dev_attrs[] = {
	__ATTR(mcd_test, S_IRUGO | S_IWUSR, s6e36w1x01_mcd_show, s6e36w1x01_mcd_store),
	__ATTR(alpm, S_IRUGO | S_IWUSR, s6e36w1x01_alpm_show, s6e36w1x01_alpm_store),
	__ATTR(hlpm, S_IRUGO | S_IWUSR, s6e36w1x01_hlpm_show, s6e36w1x01_hlpm_store),
	__ATTR(acl, S_IRUGO | S_IWUSR, s6e36w1x01_acl_show, s6e36w1x01_acl_store),
	__ATTR(scm, S_IRUGO | S_IWUSR, s6e36w1x01_scm_show, s6e36w1x01_scm_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, s6e36w1x01_hbm_show, s6e36w1x01_hbm_store),
	__ATTR(elvss, S_IRUGO | S_IWUSR, s6e36w1x01_elvss_show, s6e36w1x01_elvss_store),
	__ATTR(chip_id, 0444, s6e36w1x01_octa_chip_id_show, NULL),
	__ATTR(cell_id, 0444, s6e36w1x01_cell_id_show, NULL),
	__ATTR(refresh, S_IRUGO | S_IWUSR,
			s6e36w1x01_refresh_show, s6e36w1x01_refresh_store),
	__ATTR(read_mtp, S_IRUGO | S_IWUSR,
			s6e36w1x01_read_mtp_show, s6e36w1x01_read_mtp_store),
	__ATTR(dim_table, S_IRUGO | S_IWUSR,
			s6e36w1x01_dimming_table_show, s6e36w1x01_dimming_table_store),
};

extern void s6e36w1x01_mdnie_set(enum mdnie_scenario scenario);
static void s6e36w1x01_mdnie_restore(struct s6e36w1x01 *lcd, bool aod_state)
{
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	if ((mdnie->scenario == SCENARIO_UI) ||
	(mdnie->scenario == SCENARIO_GRAY))
		return;

	if (aod_state) {
		switch (mdnie->scenario) {
		case SCENARIO_GRAY:
		case SCENARIO_GRAY_NEGATIVE:
			s6e36w1x01_mdnie_set(SCENARIO_GRAY);
			break;
		case SCENARIO_UI:
		case SCENARIO_GALLERY:
		case SCENARIO_VIDEO:
		case SCENARIO_VTCALL:
		case SCENARIO_CAMERA:
		case SCENARIO_BROWSER:
		case SCENARIO_NEGATIVE:
		case SCENARIO_EMAIL:
		case SCENARIO_EBOOK:
		case SCENARIO_CURTAIN:
		default:
			s6e36w1x01_mdnie_set(SCENARIO_UI);
			break;
		}
		usleep_range(40000, 41000);
	} else {
		usleep_range(40000, 41000);
		s6e36w1x01_mdnie_set(mdnie->scenario);
	}
}

static int s6e36w1x01_aod_ctrl(struct dsim_device *dsim, int enable)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:enable[%d]hlpm_on[%d]\n", "aod_ctrl", enable, lcd->hlpm_on);

	lcd->hlpm_on = enable;

	if (lcd->hlpm_on) {
		s6e36w1x01_mdnie_restore(lcd, lcd->hlpm_on);
		s6e36w1x01_hlpm_on();
	} else {
		s6e36w1x01_hlpm_on();
		s6e36w1x01_mdnie_restore(lcd, lcd->hlpm_on);
	}

	return 0;
}

static void s6e36w1x01_get_gamma_tbl(struct s6e36w1x01 *lcd,
						const unsigned char *data)
{
	int i;

	s6e36w1x01_read_gamma(lcd->dimming, data);
	s6e36w1x01_generate_volt_tbl(lcd->dimming);

	for (i = 0; i < MAX_GAMMA_CNT - 1; i++) {
		lcd->gamma_tbl[i][0] = LDI_GAMMA;
		s6e36w1x01_get_gamma(lcd->dimming, i, &lcd->gamma_tbl[i][1]);
	}

	memcpy(lcd->gamma_tbl[MAX_GAMMA_CNT - 1], GAMMA_360, sizeof(GAMMA_360));

	return;
}

extern void s6e36w1x01_get_mtp_data(u8 *read_buf);
extern void s6e36w1x01_get_chip_code(u8 *read_buf);
static int s6e36w1x01_check_mtp(struct dsim_device *dsim)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);
	unsigned char mtp_data[LDI_MTP4_LEN] = {0, };

	s6e36w1x01_get_mtp_data(mtp_data);
	s6e36w1x01_get_chip_code(lcd->chip);

	s6e36w1x01_get_gamma_tbl(lcd, mtp_data);

	return 0;
}

static int s6e36w1x01_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

extern int s6e36w1x01_gamma_ctrl(u32 level);
static int s6e36w1x01_set_brightness(struct backlight_device *bd)
{
	struct s6e36w1x01 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "%s:Brightness should be in the range of %d ~ %d\n",
			__func__, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	if (!lcd) {
		pr_info("%s: LCD is NULL\n", __func__);
		return -EINVAL;
	}

	if (lcd->power == FB_BLANK_POWERDOWN) {
		printk(KERN_ERR "%s: panel power off.[%d]\n", __func__, brightness);
		return -EINVAL;
	}

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	pr_info("%s: brightness:[%d], level:[%d]\n",
		__func__, brightness, lcd->br_map[brightness]);

	return s6e36w1x01_gamma_ctrl(lcd->br_map[brightness]);
}

static int s6e36w1x01_get_power(struct lcd_device *ld)
{
	struct s6e36w1x01 *lcd = lcd_get_data(ld);

	if (FB_BLANK_POWERDOWN < lcd->power)
		pr_debug("%s: %d\n", __func__, lcd->power);
	else
		pr_debug("%s: %s\n", __func__, power_state_str[lcd->power]);

	return lcd->power;
}

static int s6e36w1x01_set_power(struct lcd_device *ld, int power)
{
	struct s6e36w1x01 *lcd = lcd_get_data(ld);

	lcd->power = power;

	if (FB_BLANK_POWERDOWN < lcd->power)
		pr_info("%s: %d\n", __func__, lcd->power);
	else
		pr_info("%s: %s\n", __func__, power_state_str[lcd->power]);

	return 0;
}

static struct lcd_ops s6e36w1x01_lcd_ops = {
	.get_power = s6e36w1x01_get_power,
	.set_power = s6e36w1x01_set_power,
};

static const struct backlight_ops s6e36w1x01_backlight_ops = {
	.get_brightness = s6e36w1x01_get_brightness,
	.update_status = s6e36w1x01_set_brightness,
};

extern void s6e36w1x01_mdnie_outdoor_set(enum mdnie_outdoor on);
static ssize_t s6e36w1x01_scenario_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->scenario);
}

static ssize_t s6e36w1x01_scenario_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	sscanf(buf, "%d", &value);

	dev_info(lcd->dev, "[mDNIe]cur[%d]new[%d]\n",
			mdnie->scenario, value);

	if (mdnie->scenario == value)
		return size;

	mdnie->scenario = value;

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	s6e36w1x01_mdnie_set(mdnie->scenario);

	return size;
}

static ssize_t s6e36w1x01_outdoor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->outdoor);
}

static ssize_t s6e36w1x01_outdoor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	if ((lcd->alpm_on) || (lcd->hlpm_on)) {
		pr_info("%s:alpm or hlpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	sscanf(buf, "%d", &value);

	if (value >= OUTDOOR_MAX) {
		dev_warn(lcd->dev, "invalid outdoor mode set\n");
		return -EINVAL;
	}
	mdnie->outdoor = value;

	s6e36w1x01_mdnie_outdoor_set(value);

	mdnie->outdoor = value;

	return size;
}

static struct device_attribute mdnie_attrs[] = {
	__ATTR(scenario, 0664, s6e36w1x01_scenario_show, s6e36w1x01_scenario_store),
	__ATTR(outdoor, 0664, s6e36w1x01_outdoor_show, s6e36w1x01_outdoor_store),
};

void s6e36w1x01_mdnie_lite_init(struct s6e36w1x01 *lcd)
{
	static struct class *mdnie_class;
	struct mdnie_lite_device *mdnie;
	int i;

	mdnie = kzalloc(sizeof(struct mdnie_lite_device), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie object.\n");
		return;
	}

	mdnie_class = class_create(THIS_MODULE, "extension");
	if (IS_ERR(mdnie_class)) {
		pr_err("Failed to create class(mdnie)!\n");
		goto err_free_mdnie;
	}

	mdnie->dev = device_create(mdnie_class, NULL, 0, NULL, "mdnie");
	if (IS_ERR(&mdnie->dev)) {
		pr_err("Failed to create device(mdnie)!\n");
		goto err_free_mdnie;
	}

	for (i = 0; i < ARRAY_SIZE(mdnie_attrs); i++) {
		if (device_create_file(mdnie->dev, &mdnie_attrs[i]) < 0)
			pr_err("Failed to create device file(%s)!\n",
				mdnie_attrs[i].attr.name);
	}

	mdnie->scenario = SCENARIO_UI;
	lcd->mdnie = mdnie;

	dev_set_drvdata(mdnie->dev, lcd);

	return;

err_free_mdnie:
	kfree(mdnie);
}

extern void s6e36w1x01_display_init(struct decon_lcd * lcd);
static int s6e36w1x01_displayon(struct dsim_device *dsim)
{
	struct lcd_device *lcd = dsim->lcd;
	struct s6e36w1x01 *panel = dev_get_drvdata(&lcd->dev);
	struct mdnie_lite_device *mdnie = panel->mdnie;

	s6e36w1x01_display_init(&dsim->lcd_info);

	if (mdnie->outdoor == OUTDOOR_ON)
		s6e36w1x01_mdnie_outdoor_set(mdnie->outdoor);
	else
		s6e36w1x01_mdnie_set(mdnie->scenario);

	panel->power = FB_BLANK_UNBLANK;

	return 1;
}

static int s6e36w1x01_set_panel_power(struct dsim_device *dsim, bool on)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	pr_debug("%s(%d) +\n", __func__, on);

	if (on) {
		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0], GPIOF_OUT_INIT_HIGH, "lcd_power0");
			if (ret < 0) {
				dev_err(dsim->dev, "failed LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(10000, 11000);
		} else if (res->lcd_regulator[0]) {
			ret = regulator_enable(res->lcd_regulator[0]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_0 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to enable regulator_0\n");

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1], GPIOF_OUT_INIT_HIGH, "lcd_power1");
			if (ret < 0) {
				dev_err(dsim->dev, "failed 2nd LCD power on\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(10000, 11000);
		} else if (res->lcd_regulator[1]){
			ret = regulator_enable(res->lcd_regulator[1]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_1 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to enable regulator_1\n");
	} else {
		if (res->lcd_power[0] > 0) {
			ret = gpio_request_one(res->lcd_power[0], GPIOF_OUT_INIT_LOW, "lcd_power0");
			if (ret < 0) {
				dev_err(dsim->dev, "failed LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[0]);
			usleep_range(5000, 6000);
		} else if (res->lcd_regulator[0]){
			ret = regulator_disable(res->lcd_regulator[0]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_0 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to disable regulator_0\n");

		if (res->lcd_power[1] > 0) {
			ret = gpio_request_one(res->lcd_power[1], GPIOF_OUT_INIT_LOW, "lcd_power1");
			if (ret < 0) {
				dev_err(dsim->dev, "failed 2nd LCD power off\n");
				return -EINVAL;
			}
			gpio_free(res->lcd_power[1]);
			usleep_range(5000, 6000);
		} else if (res->lcd_regulator[1]){
			ret = regulator_disable(res->lcd_regulator[1]);
			if (ret) {
				dev_err(dsim->dev, "failed to enable lcd_regulator_1 regulator.\n");
				return -EINVAL;
			}
		} else
			dev_err(dsim->dev, "Failed to disable regulator_1\n");

	}

	pr_debug("%s(%d) -\n", __func__, on);

	return 0;
}

int s6e36w1x01_reset_on(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	pr_debug("%s +\n", __func__);

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_HIGH, "lcd_reset");
	if (ret < 0) {
		dev_err(dsim->dev, "failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 0);
	usleep_range(5000, 6000);
	gpio_set_value(res->lcd_reset, 1);

	gpio_free(res->lcd_reset);

	/* 5ms delay */
	usleep_range(5000, 5010);

	pr_debug("%s -\n", __func__);
	return 0;
}

int s6e36w1x01_reset_off(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	pr_debug("%s +\n", __func__);

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_LOW, "lcd_reset");
	if (ret < 0) {
		dev_err(dsim->dev, "failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	gpio_free(res->lcd_reset);

	pr_debug("%s -\n", __func__);
	return 0;
}

static int s6e36w1x01_reset_ctrl(struct dsim_device *dsim, int on)
{
	struct dsim_resources *res = &dsim->res;
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_debug("%s: %s\n", __func__, on ? "ON":"OFF");

	if (on) {
		if (lcd->lp_mode)
			goto out;

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on) {
			lcd->boot_power_on = false;
			goto out;
		}
		s6e36w1x01_reset_on(dsim);

		if (res->oled_det > 0)
			enable_irq(lcd->esd_irq);
	} else {
		lcd->power = FB_BLANK_POWERDOWN;
		if (res->oled_det > 0)
			disable_irq(lcd->esd_irq);
		s6e36w1x01_reset_off(dsim);
	}

out:
	return 0;
}

static int s6e36w1x01_power_on(struct dsim_device *dsim, int on)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s: %s\n", __func__, on ? "ON":"OFF");

	if (on) {
		if (lcd->lp_mode)
			goto out;

		s6e36w1x01_set_panel_power(dsim, on);

		/* 10ms delay */
		usleep_range(10000, 10010);

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on)
			goto out;
	} else {
		lcd->power = FB_BLANK_POWERDOWN;
		s6e36w1x01_set_panel_power(dsim, on);
	}

out:
	return 0;
}

extern int s6e36w1x01_update_hlpm_gamma(void);
static int s6e36w1x01_probe(struct dsim_device *dsim)
{
	struct s6e36w1x01 *lcd;
	struct dsim_resources *res = &dsim->res;
	int start = 0, end, i, offset = 0;
	int ret;

	pr_err("%s\n", __func__);

	if (!get_panel_id()) {
		pr_err("No lcd attached!\n");
		return -ENODEV;
	}

	lcd = devm_kzalloc(dsim->dev,
				sizeof(struct s6e36w1x01), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate s6e36w1x01 structure.\n");
		return -ENOMEM;
	}

	lcd->dev = dsim->dev;

	lcd->dimming = devm_kzalloc(dsim->dev,
				sizeof(*lcd->dimming), GFP_KERNEL);
	if (!lcd->dimming) {
		pr_err("failed to allocate dimming.\n");
		ret = -ENOMEM;
		goto err_free_lcd;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i] = (unsigned char *)
			kzalloc(sizeof(unsigned char) * GAMMA_CMD_CNT,
			GFP_KERNEL);
		if (!lcd->gamma_tbl[i]) {
			pr_err("failed to allocate gamma_tbl\n");
			ret = -ENOMEM;
			goto err_free_dimming;
		}
	}

	lcd->br_map = devm_kzalloc(dsim->dev,
		sizeof(unsigned char) * (MAX_BRIGHTNESS + 1), GFP_KERNEL);
	if (!lcd->br_map) {
		pr_err("failed to allocate br_map\n");
		ret = -ENOMEM;
		goto err_free_gamma_tbl;
	}

	for (i = 0; i < DIMMING_COUNT; i++) {
		end = br_convert[offset++];
		memset(&lcd->br_map[start], i, end - start + 1);
		start = end + 1;
	}

	mutex_init(&lcd->lock);

	lcd->bd = backlight_device_register(BACKLIGHT_DEV_NAME, lcd->dev, lcd,
		&s6e36w1x01_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		printk(KERN_ALERT "failed to register backlight device!\n");
		goto err_free_br_map;
	}

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;

	dsim->lcd = lcd_device_register(LCD_DEV_NAME, lcd->dev, lcd,
						&s6e36w1x01_lcd_ops);
	if (IS_ERR(dsim->lcd)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(dsim->lcd);
		goto err_unregister_bd;
	}
	lcd->ld = dsim->lcd;

	lcd->esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(lcd->esd_class)) {
		dev_err(lcd->dev, "Failed to create class(lcd_event)!\n");
		ret = PTR_ERR(lcd->esd_class);
		goto err_unregister_lcd;
	}

	lcd->esd_dev = device_create(lcd->esd_class, NULL, 0, NULL, "esd");

	INIT_WORK(&lcd->det_work, s6e36w1x01_esd_detect_work);

	if (res->oled_det > 0) {
		lcd->esd_irq = gpio_to_irq(res->oled_det);
		dev_info(lcd->dev, "esd_irq_num [%d]\n", lcd->esd_irq);
		ret = devm_request_irq(lcd->dev, lcd->esd_irq,
					s6e36w1x01_esd_interrupt,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "oled_det",
					lcd);
		if (ret < 0) {
			dev_err(lcd->dev, "failed to request det irq.\n");
			goto err_unregister_lcd;
		}
	}

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, " failed to create lcd_type sysfs.\n");
		goto err_unregister_lcd;
	}

	for (i = 0; i < ARRAY_SIZE(ld_dev_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev,
				&ld_dev_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add ld dev sysfs entries\n");
			for (i--; i >= 0; i--)
				device_remove_file(lcd->dev,
					&ld_dev_attrs[i]);
			goto err_remove_lcd_type_file;
		}
	}

	s6e36w1x01_check_mtp(dsim);
	s6e36w1x01_update_hlpm_gamma();
	s6e36w1x01_mdnie_lite_init(lcd);
	s6e36w1x01_set_panel_power(dsim, true);

	return 0;

err_remove_lcd_type_file:
	device_remove_file(&lcd->ld->dev, &dev_attr_lcd_type);
err_unregister_lcd:
	lcd_device_unregister(lcd->ld);
err_unregister_bd:
	backlight_device_unregister(lcd->bd);
err_free_br_map:
	mutex_destroy(&lcd->lock);
	devm_kfree(dsim->dev, lcd->br_map);
err_free_gamma_tbl:
	for (i = 0; i < MAX_GAMMA_CNT; i++)
		if (lcd->gamma_tbl[i])
			devm_kfree(dsim->dev, lcd->gamma_tbl[i]);
err_free_dimming:
	devm_kfree(dsim->dev, lcd->dimming);
err_free_lcd:
	devm_kfree(dsim->dev, lcd);

	return ret;
}

extern void s6e36w1x01_disable(void);
static int s6e36w1x01_suspend(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_debug("%s\n", __func__);

	if (res->oled_det > 0)
		disable_irq(lcd->esd_irq);

	s6e36w1x01_disable();

	return 1;
}

struct mipi_dsim_lcd_driver s6e36w1x01_mipi_lcd_driver = {
	.probe		= s6e36w1x01_probe,
	.displayon	= s6e36w1x01_displayon,
	.suspend		= s6e36w1x01_suspend,
	.power_on	= s6e36w1x01_power_on,
	.reset_on		= s6e36w1x01_reset_ctrl,
	.aod_ctrl		= s6e36w1x01_aod_ctrl,
};
