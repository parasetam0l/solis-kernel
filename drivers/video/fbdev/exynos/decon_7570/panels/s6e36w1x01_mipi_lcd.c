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
#include "s6e36w1x01_dimming.h"
#include "lcd_ctrl.h"
#include "decon_lcd.h"
#include "s6e36w1x01_param.h"
#include "s6e36w1x01_mipi_lcd.h"

#define	BACKLIGHT_DEV_NAME	"s6e36w1x01-bl"
#define	LCD_DEV_NAME		"s6e36w1x01"

#ifdef CONFIG_EXYNOS_DECON_LCD_S6E36W2X01
extern int get_panel_id(void);
#else
static int panel_id;
int get_panel_id(void)
{
	return panel_id;
}
EXPORT_SYMBOL(get_panel_id);

static int __init panel_id_cmdline(char *mode)
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
__setup("lcdtype=", panel_id_cmdline);
#endif

static ssize_t s6e36w1x01_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "SDC_%06x\n", get_panel_id());
	strcat(buf, temp);

	return strlen(buf);
}
DEVICE_ATTR(s6e36w1x01_lcd_type, 0444, s6e36w1x01_lcd_type_show, NULL);

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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

	return sprintf(buf, "%s\n", lcd->hlpm_on ? "on" : "off");
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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

static struct device_attribute ld_dev_attrs[] = {
	__ATTR(mcd_test, S_IRUGO | S_IWUSR, s6e36w1x01_mcd_show, s6e36w1x01_mcd_store),
	__ATTR(alpm, S_IRUGO | S_IWUSR, s6e36w1x01_alpm_show, s6e36w1x01_alpm_store),
	__ATTR(hlpm, S_IRUGO | S_IWUSR, s6e36w1x01_hlpm_show, s6e36w1x01_hlpm_store),
	__ATTR(acl, S_IRUGO | S_IWUSR, s6e36w1x01_acl_show, s6e36w1x01_acl_store),
	__ATTR(scm, S_IRUGO | S_IWUSR, s6e36w1x01_scm_show, s6e36w1x01_scm_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, s6e36w1x01_hbm_show, s6e36w1x01_hbm_store),
	__ATTR(elvss, S_IRUGO | S_IWUSR, s6e36w1x01_elvss_show, s6e36w1x01_elvss_store),
	__ATTR(chip_id, S_IRUGO, s6e36w1x01_octa_chip_id_show, NULL),
	__ATTR(refresh, S_IRUGO | S_IWUSR,
			s6e36w1x01_refresh_show, s6e36w1x01_refresh_store),
};

static int s6e36w1x01_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

extern int s6e36w1x01_gamma_ctrl(u32 level);
static int update_brightness(int brightness)
{
	return s6e36w1x01_gamma_ctrl(brightness);
}

static int s6e36w1x01_set_brightness(struct backlight_device *bd)
{
	struct s6e36w1x01 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness;

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		printk(KERN_ALERT "%s:Brightness should be in the range of %d ~ %d\n",
			__func__, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	if (lcd->power == FB_BLANK_POWERDOWN) {
		printk(KERN_ERR "%s: panel power off.\n", __func__);
		return -EINVAL;
	}

	if (!lcd) {
		pr_info("%s: LCD is NULL\n", __func__);
		return 1;
	}

	pr_info("%s: brightness:[%d], level:[%d]\n", __func__, brightness, lcd->br_map[brightness]);

	return update_brightness(lcd->br_map[brightness]);
}

static int s6e36w1x01_get_power(struct lcd_device *ld)
{
	struct s6e36w1x01 *lcd = lcd_get_data(ld);

	pr_info("%s[%d]\n", __func__, lcd->power);

	return lcd->power;
}

static int s6e36w1x01_set_power(struct lcd_device *ld, int power)
{
	struct s6e36w1x01 *lcd = lcd_get_data(ld);

	lcd->power = power;

	pr_info("%s[%d]\n", __func__, lcd->power);

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

extern void s6e36w1x01_mdnie_set(enum mdnie_scenario scenario);
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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
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

	dev_info(dsim->dev, "%s(%d) +\n", __func__, on);

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

	dev_info(dsim->dev, "%s(%d) -\n", __func__, on);

	return 0;
}

int s6e36w1x01_reset_on(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	dev_info(dsim->dev, "%s +\n", __func__);

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

	usleep_range(10000, 11000);

	dev_info(dsim->dev, "%s -\n", __func__);
	return 0;
}

int s6e36w1x01_reset_off(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	int ret;

	dev_info(dsim->dev, "%s +\n", __func__);

	ret = gpio_request_one(res->lcd_reset, GPIOF_OUT_INIT_LOW, "lcd_reset");
	if (ret < 0) {
		dev_err(dsim->dev, "failed to get LCD reset GPIO\n");
		return -EINVAL;
	}

	gpio_free(res->lcd_reset);

	dev_info(dsim->dev, "%s -\n", __func__);
	return 0;
}

static int s6e36w1x01_reset_ctrl(struct dsim_device *dsim, int on)
{
	struct dsim_resources *res = &dsim->res;
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);

	dev_info(dsim->dev, "%s(%d) +\n", __func__, on);

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
	dev_info(dsim->dev, "%s(%d) -\n", __func__, on);

	return 0;
}

static int s6e36w1x01_power_on(struct dsim_device *dsim, int on)
{
	struct lcd_device *panel = dsim->lcd;
	struct s6e36w1x01 *lcd = dev_get_drvdata(&panel->dev);

	dev_info(dsim->dev, "%s(%d) +\n", __func__, on);

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
	dev_info(dsim->dev, "%s(%d) -\n", __func__, on);

	return 0;
}

static int s6e36w1x01_probe(struct dsim_device *dsim)
{
	struct s6e36w1x01 *lcd;
	int start = 0, end, i, offset = 0;
	int ret;

	pr_err("%s\n", __func__);

#if 0
	if (!get_panel_id()) {
		pr_err("No lcd attached!\n");
		return -ENODEV;
	}
#endif
	lcd = devm_kzalloc(dsim->dev,
				sizeof(struct s6e36w1x01), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate s6e36w1x01 structure.\n");
		return -ENOMEM;
	}

	lcd->dev = dsim->dev;

//	lcd->pd = (struct lcd_platform_data *)dsim_dev->platform_data;
//	lcd->boot_power_on = lcd->pd->lcd_enabled;

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

	ret = device_create_file(&lcd->ld->dev, &dev_attr_s6e36w1x01_lcd_type);
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

	s6e36w1x01_mdnie_lite_init(lcd);
	s6e36w1x01_set_panel_power(dsim, true);

	return 0;

err_remove_lcd_type_file:
	device_remove_file(&lcd->ld->dev, &dev_attr_s6e36w1x01_lcd_type);
err_unregister_lcd:
	lcd_device_unregister(lcd->ld);
err_unregister_bd:
	backlight_device_unregister(lcd->bd);
err_free_br_map:
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
	dev_info(dsim->dev, "%s\n", __func__);

	s6e36w1x01_disable();

	return 1;
}

struct mipi_dsim_lcd_driver s6e36w1x01_mipi_lcd_driver = {
	.probe		= s6e36w1x01_probe,
	.displayon	= s6e36w1x01_displayon,
	.suspend		= s6e36w1x01_suspend,
	.power_on	= s6e36w1x01_power_on,
	.reset_on		= s6e36w1x01_reset_ctrl,
};
