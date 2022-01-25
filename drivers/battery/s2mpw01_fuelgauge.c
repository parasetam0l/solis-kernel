/*
 *  s2mpw01_fuelgauge.c
 *  Samsung S2MPW01 Fuel Gauge Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 *  Developed by Nguyen Tien Dat (tiendat.nt@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG

#define SINGLE_BYTE	1

#include <linux/battery/fuelgauge/s2mpw01_fuelgauge.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/of_gpio.h>
#include <linux/mfd/samsung/s2mpw01.h>

#define FG_CFG_FILE_PATH "/opt/etc/.fg_scaled_capacity_max"

static enum power_supply_property s2mpw01_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_UPDATE_BATTERY_DATA,
};

static int s2mpw01_fg_write_reg_byte(struct i2c_client *client, int reg, u8 data)
{
	int ret, i = 0;

	ret = s2mpw01_write_reg(client, reg,  data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = s2mpw01_write_reg(client, reg,  data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			pr_err("%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

static int s2mpw01_fg_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;
	s2mpw01_fg_write_reg_byte(client, reg, buf[0]);
	s2mpw01_fg_write_reg_byte(client, reg+1, buf[1]);
#else
	int ret, i = 0;

	ret = s2mpw01_bulk_write(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = s2mpw01_bulk_write(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			pr_err("%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static int s2mpw01_fg_read_reg_byte(struct i2c_client *client, int reg, void *data)
{
	int ret;
	u8 temp = 0;

	ret = s2mpw01_read_reg(client, reg, &temp);
	if (ret < 0)
		return ret;
	*(u8 *)data = (u8)temp;

	return ret;
}

static int s2mpw01_fg_read_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;
	u8 data1 = 0, data2 = 0;
	s2mpw01_fg_read_reg_byte(client, reg, &data1);
	s2mpw01_fg_read_reg_byte(client, reg + 1, &data2);
	buf[0] = data1;
	buf[1] = data2;
#else
	int ret = 0, i = 0;

	ret = s2mpw01_bulk_read(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = s2mpw01_bulk_read(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			pr_err("%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static void s2mpw01_fg_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x04; i <= 0x05; i++) {
		 s2mpw01_fg_read_reg_byte(i2c, i, &data);
		 sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x16; i <= 0x17; i++) {
		 s2mpw01_fg_read_reg_byte(i2c, i, &data);
		 sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	for (i = 0x1E; i <= 0x21; i++) {
		 s2mpw01_fg_read_reg_byte(i2c, i, &data);
		 sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
	for (i = 0x26; i <= 0x2D; i++) {
		 s2mpw01_fg_read_reg_byte(i2c, i, &data);
		 sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}
	for (i = 0x2E; i <= 0x30; i++) {
		 s2mpw01_fg_read_reg_byte(i2c, i, &data);
		 sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	/* adc direct value */
	s2mpw01_fg_read_reg_byte(i2c, 0x25, &data);
	data &= 0xF8;
	s2mpw01_fg_write_reg_byte(i2c, 0x25, data);

	s2mpw01_fg_read_reg_byte(i2c, 0x22, &data);
	sprintf(str+strlen(str), "ADCMX_0x22 :0x%02x, ", data);

	s2mpw01_fg_read_reg_byte(i2c, 0x23, &data);
	sprintf(str+strlen(str), "ADCMX_0x23 :0x%02x, ", data);

	pr_err("[DEBUG]%s: %s\n", __func__, str);
}

static void s2mpw01_restart_gauging(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 temp;

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_START, &temp);
	temp |= 0x33;
	s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_START, temp);

	pr_info("%s: dump done\n", __func__);

	msleep(500);
}

#if 0
static int s2mpw01_init_regs(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	int ret = 0;
	pr_err("%s: s2mpw01 fuelgauge initialize\n", __func__);
	return ret;
}
#endif

static void s2mpw01_alert_init(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* VBAT Threshold setting: 3.55V */
	data[0] = 0x00 & 0x0f;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << 4);

	data[1] = 0x00;
	s2mpw01_fg_write_reg(fuelgauge->i2c, S2MPW01_FG_REG_IRQ_LVL, data);
}

static bool s2mpw01_check_status(struct i2c_client *client)
{
	u8 data[2];
	bool ret = false;

	/* check if Smn was generated */
	if (s2mpw01_fg_read_reg(client, S2MPW01_FG_REG_STATUS, data) < 0)
		return ret;

	dev_dbg(&client->dev, "%s: status to (%02x%02x)\n",
		__func__, data[1], data[0]);

	if (data[1] & (0x1 << 1))
		return true;
	else
		return false;
}

static int s2mpw01_set_temperature(struct s2mpw01_fuelgauge_data *fuelgauge,
			int temperature)
{
	char val;
	u8 temp1, temp2, temp3;
	int temperature_level;

	val = temperature / 10;

	if (val < 0)
		temperature_level = TEMP_LEVEL_VERY_LOW;
	else if (val < 15)
		temperature_level = TEMP_LEVEL_LOW;
	else if (val > 35)
		temperature_level = TEMP_LEVEL_HIGH;
	else
		temperature_level = TEMP_LEVEL_MID;

	if (fuelgauge->before_temp_level == temperature_level)
		return temperature;

	fuelgauge->before_temp_level = temperature_level;

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, 0x19, &temp1);
	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, 0x28, &temp2);
	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, 0x2B, &temp3);
	temp1 &= 0x0F;
	temp2 &= 0x0F;
	temp3 &= 0x0F;

	if (val < 15) {
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0B,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[0]);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0E,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[1]);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0F,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[2]);
		temp1 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[3] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x19, temp1);
		temp2 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[4] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x28, temp2);
		temp3 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[5] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x2B, temp3);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RTEMP, 10);
	} else if (val >= 15 && val <= 35) {
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0B,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[0]);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0E,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[1]);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0F,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[2]);
		temp1 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[3] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x19, temp1);
		temp2 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[4] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x28, temp2);
		temp3 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[5] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x2B, temp3);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RTEMP, 25);
	} else if (val > 35) {
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0B,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[0]);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0E,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[1]);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x0F,
			fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[2]);
		temp1 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[3] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x19, temp1);
		temp2 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[4] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x28, temp2);
		temp3 |= fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[5] << 4;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, 0x2B, temp3);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RTEMP, 40);
	}

	pr_info("%s: temperature to (%d)\n", __func__, temperature);

	return temperature;
}

static int s2mpw01_get_temperature(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	s32 temperature = 0;

	/*
	 *  use monitor regiser.
	 *  monitor register default setting is temperature
	 */
	if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RTEMP, data) < 0)
		return -ERANGE;

	/* data[] store 2's compliment format number */
	if (data[0] & (0x1 << 7)) {
		/* Negative */
		temperature = ((~(data[0])) & 0xFF) + 1;
		temperature *= -10;
	} else {
		temperature = data[0] & 0x7F;
		temperature *= 10;
	}

	dev_dbg(&fuelgauge->i2c->dev, "%s: temperature (%d)\n",
		__func__, temperature);

	return temperature;
}

extern int yu_battery_capacity;

/* soc should be 0.01% unit */
static int s2mpw01_get_soc(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	for (i = 0; i < 50; i++) {
		if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RSOC, check_data) < 0)
			return -EINVAL;
	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);

	if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;
	}

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: raw capacity (0x%x:%d)\n", __func__,
		compliment, rsoc);

	return min(rsoc, 10000) / 10;
}

static int s2mpw01_get_rawsoc(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc, i;

	for (i = 0; i < 50; i++) {
		if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RSOC, data) < 0)
			return -EINVAL;
		if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RSOC, check_data) < 0)
			return -EINVAL;
		if ((data[0] == check_data[0]) && (data[1] == check_data[1]))
			break;
	}

	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 12);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 12));
	}

	dev_dbg(&fuelgauge->i2c->dev, "[DEBUG]%s: raw capacity (0x%x:%d)\n", __func__,
			compliment, rsoc);
	return min(rsoc, 10000);
}

static int s2mpw01_get_current(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2], temp;
	u16 compliment;
	int curr = 0;

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_MONOUT_CFG, &temp);
	temp &= 0xF0;
	temp |= 0x05;
	s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_MONOUT_CFG, temp);

	if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_CURR, data) < 0)
		return -EINVAL;

	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(fuelgauge->dev, "%s: rCUR_CC(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}

	pr_info("%s: rCUR_CC(0x%4x), current (%d)mA\n", __func__, compliment, curr);

	return curr;
}

static int s2mpw01_get_ocv(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 rocv = 0;

	if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_ROCV, data) < 0)
		return -EINVAL;

	rocv = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_dbg(&fuelgauge->i2c->dev, "%s: rocv (%d)\n", __func__, rocv);


	return rocv;
}

static int s2mpw01_get_vbat(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2], val;
	u32 vbat = 0;
	int ret;

	if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RVBAT, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	s2mpw01_fg_test_read(fuelgauge->i2c);

	ret = s2mpw01_read_reg(fuelgauge->pmic, 0x46, &val);
	if (ret < 0) {
		pr_err("%s: 0x46 read error\n", __func__);
		return ret;
	}
	pr_info("%s: data0 (%d) data1 (%d) vbat (%d) 0x46: (0x%x)\n", __func__, data[0], data[1], vbat, val);

	return vbat;
}

static int s2mpw01_get_avgvbat(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 new_vbat, old_vbat = 0;
	int cnt;

	for (cnt = 0; cnt < 5; cnt++) {
		if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_RVBAT, data) < 0)
			return -EINVAL;

		new_vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

		if (cnt == 0)
			old_vbat = new_vbat;
		else
			old_vbat = new_vbat / 2 + old_vbat / 2;
	}

	pr_info("%s: avgvbat (%d)\n", __func__, old_vbat);

	return old_vbat;
}

#if defined(CONFIG_PREVENT_SOC_JUMP)
static int s2mpw01_fg_set_scaled_capacity_max(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	struct file *fp;
	mm_segment_t old_fs;
	char buf[5] = {0, };
	int error = 0;
	int ret = -1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(FG_CFG_FILE_PATH, O_RDWR | O_CREAT, 0600);
	if (IS_ERR(fp)) {
		pr_err("failed to open file. %s\n",	FG_CFG_FILE_PATH);
		error = -ENOENT;
		goto open_err;
	}

	pr_err("fuelgauge->capacity_max[%d]\n", fuelgauge->capacity_max);
	snprintf(buf, sizeof(buf), "%4d\n", fuelgauge->capacity_max);

	if (fp->f_mode & FMODE_WRITE) {
		ret = fp->f_op->write(fp, (const char *)buf, sizeof(buf), &fp->f_pos);
		if (ret < 0)
			pr_err("failed to write scaled capacity_max to file.\n");
	}
	set_fs(old_fs);

	filp_close(fp, current->files);

open_err:
	set_fs(old_fs);
	return error;
}
#endif

static void s2mpw01_fg_get_scaled_capacity_max(struct work_struct *work)
{
	struct s2mpw01_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mpw01_fuelgauge_data, scaled_work.work);

	struct file *fp;
	mm_segment_t old_fs;
	int fw_size, nread;
	int ret = -1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(FG_CFG_FILE_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		pr_err("failed to open file. %s\n", FG_CFG_FILE_PATH);
		fuelgauge->scaled_capacity_max = SCALED_VAL_NO_EXIST;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (fw_size > 0) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,
			fw_size, &fp->f_pos);

		if (nread != fw_size) {
			pr_err("failed to read capacity_max file, nread %u Bytes\n", nread);
		} else {
			sscanf(fw_data, "%d", &ret);
			fuelgauge->capacity_max = ret;
			fuelgauge->scaled_capacity_max = SCALED_VAL_EXIST;
			pr_err("%s:scaled capacity_max[%d]\n", __func__, ret);
		}

		kfree(fw_data);
	}

	fuelgauge->initial_update_of_soc = true;

	filp_close(fp, current->files);

open_err:
	set_fs(old_fs);
}

/* capacity is  0.1% unit */
static void s2mpw01_fg_get_scaled_capacity(
		struct s2mpw01_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_err("%s: scaled capacity (%d.%d)\n",
			__func__, val->intval/10, val->intval%10);
}

/* capacity is integer */
static void s2mpw01_fg_get_atomic_capacity(
		struct s2mpw01_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
		pr_err("%s: ATOMIC capacity (old %d : new %d)\n",
				__func__, fuelgauge->capacity_old, val->intval);
		if (fuelgauge->capacity_old < val->intval) {
			val->intval = fuelgauge->capacity_old + 1;
		} else if (fuelgauge->capacity_old > val->intval) {
			val->intval = fuelgauge->capacity_old - 1;
		}
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
			SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
				fuelgauge->capacity_old < val->intval) {
			pr_err("%s: ABNORMAL capacity (old %d : new %d)\n",
					__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int s2mpw01_fg_check_capacity_max(
		struct s2mpw01_fuelgauge_data *fuelgauge, int capacity_max)
{
	int new_capacity_max = capacity_max;

	if (new_capacity_max < (fuelgauge->pdata->capacity_max -
				fuelgauge->pdata->capacity_max_margin - 10)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max -
			 fuelgauge->pdata->capacity_max_margin);

		pr_info("%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	} else if (new_capacity_max > (fuelgauge->pdata->capacity_max +
				fuelgauge->pdata->capacity_max_margin)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin);

		pr_info("%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	}

	return new_capacity_max;
}

static int s2mpw01_fg_calculate_dynamic_scale(
		struct s2mpw01_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = s2mpw01_get_rawsoc(fuelgauge) / 10;

	pr_info("%s: raw_soc_val.intval(%d), capacity(%d)\n",
			__func__, raw_soc_val.intval, capacity);
	pr_info("capacity_max(%d), capacity_max_margin(%d), fuelgauge->capacity_max (%d)\n",
			fuelgauge->pdata->capacity_max, fuelgauge->pdata->capacity_max_margin, fuelgauge->capacity_max);

	if (raw_soc_val.intval <
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		dev_dbg(fuelgauge->dev, "%s: capacity_max (%d)",
				__func__, fuelgauge->capacity_max);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval >
			 fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin) :
			raw_soc_val.intval;
		dev_dbg(fuelgauge->dev, "%s: raw soc (%d)",
				__func__, fuelgauge->capacity_max);
	}

	if (capacity != 100) {
		fuelgauge->capacity_max = s2mpw01_fg_check_capacity_max(
			fuelgauge, (fuelgauge->capacity_max * 100 / capacity));
	} else  {
		fuelgauge->capacity_max =
			(fuelgauge->capacity_max * 99 / 100);
	}

	/* update capacity_old for sec_fg_get_atomic_capacity algorithm */
	fuelgauge->capacity_old = capacity;

	pr_info("%s: %d is used for capacity_max\n", __func__, fuelgauge->capacity_max);

	return fuelgauge->capacity_max;
}

bool s2mpw01_fuelgauge_fuelalert_init(struct s2mpw01_fuelgauge_data *fuelgauge , int soc)
{
	u8 data[2];

	/* 1. Set s2mpw01 alert configuration. */
	s2mpw01_alert_init(fuelgauge);

	if (s2mpw01_fg_read_reg(fuelgauge->i2c, S2MPW01_FG_REG_IRQ, data) < 0)
		return -1;

	/*Enable VBAT, SOC */
	data[1] &= 0xfc;

	/*Disable IDLE_ST, INIT_ST */
	data[1] |= 0x0c;

	s2mpw01_fg_write_reg(fuelgauge->i2c, S2MPW01_FG_REG_IRQ, data);

	dev_dbg(&fuelgauge->i2c->dev, "%s: irq_reg(%02x%02x) irq(%d)\n",
			__func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	return true;
}

bool s2mpw01_fuelgauge_is_fuelalerted(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	return s2mpw01_check_status(fuelgauge->i2c);
}

bool s2mpw01_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	struct s2mpw01_fuelgauge_data *fuelgauge = irq_data;
	int ret;

	ret = s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_IRQ, 0x00);
	if (ret < 0)
		pr_err("%s: Error(%d)\n", __func__, ret);

	return ret;
}

bool s2mpw01_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

static int s2mpw01_fg_update_bat_param(struct s2mpw01_fuelgauge_data *fuelgauge, int step)
{
	int i, ret = 0;
	u8 param0, param1, param2;
	u8 batcap0, batcap1;
	u8 por_state = 0;

	mutex_lock(&fuelgauge->fg_lock);

	if (!fuelgauge->fg_num_age_step)
		goto exit_fg_update_bat_param;

	fuelgauge->fg_age_step = step;

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RBATCAP,  &batcap0);
	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RBATCAP + 1,  &batcap1);

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_PARAM1,  &param0);
	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_PARAM1 + 1,  &param1);
	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_PARAM1 + 2,  &param2);

	if ((param0 != fuelgauge->age_data_info[fuelgauge->fg_age_step].model_param1[0]) || \
	       (param1 != fuelgauge->age_data_info[fuelgauge->fg_age_step].model_param1[1]) || \
	       (param2 != fuelgauge->age_data_info[fuelgauge->fg_age_step].model_param1[2]) || \
	       (batcap0 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]) || \
	       (batcap1 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1])) {

		/*Set 0x17[4] to restore batt param at the bootloader when abnormal power off*/
		s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RESET, &por_state);
		por_state |= 0x10;
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RESET, por_state);

		for (i = 0; i < 2; i++) {
			s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RBATCAP + i, \
						fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[i]);
		}

		for (i = 0; i < S2MPW01_FG_PARAM1_NUM; i++) {
			s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_PARAM1 + i, \
								fuelgauge->age_data_info[fuelgauge->fg_age_step].model_param1[i]);
		}

		/* Successfully finished, clear 0x17[4] */
		s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RESET, &por_state);
		por_state &= ~(0x10);
		s2mpw01_fg_write_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_RESET, por_state);

		ret = 1;
	}

exit_fg_update_bat_param:
	mutex_unlock(&fuelgauge->fg_lock);
	return ret;
}

static int s2mpw01_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mpw01_fuelgauge_data *fuelgauge =
		container_of(psy, struct s2mpw01_fuelgauge_data, psy_fg);

	pr_debug("%s %d psp=%d\n", __func__, __LINE__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		return -ENODATA;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mpw01_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mpw01_get_avgvbat(fuelgauge);
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = s2mpw01_get_ocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = s2mpw01_get_current(fuelgauge);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mpw01_get_rawsoc(fuelgauge);
		} else {
			val->intval = s2mpw01_get_soc(fuelgauge);

			if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
				s2mpw01_fg_get_scaled_capacity(fuelgauge, val);

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
					fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				s2mpw01_fuelgauge_fuelalert_init(fuelgauge,
						fuelgauge->pdata->fuel_alert_soc);
			}

			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			/* send error before get scaled_capacity_max value */
			if (fuelgauge->scaled_capacity_max == SCALED_VAL_UNKNOWN) {
				val->intval = -EAGAIN;
			} else {
				if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
						 SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
					s2mpw01_fg_get_atomic_capacity(fuelgauge, val);
			}
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mpw01_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_UPDATE_BATTERY_DATA:
		return -ENODATA;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpw01_fg_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct s2mpw01_fuelgauge_data *fuelgauge =
		container_of(psy, struct s2mpw01_fuelgauge_data, psy_fg);

	pr_info("%s:psp[%d]\n", __func__, psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		pr_info("capacity_calculation_type[%d]\n",
			fuelgauge->pdata->capacity_calculation_type);
		if (fuelgauge->pdata->capacity_calculation_type &
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
#if defined(CONFIG_PREVENT_SOC_JUMP)
			s2mpw01_fg_calculate_dynamic_scale(fuelgauge, val->intval);
			s2mpw01_fg_set_scaled_capacity_max(fuelgauge);
#else
			s2mpw01_fg_calculate_dynamic_scale(fuelgauge, 100);
#endif
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			fuelgauge->is_charging = false;
		else
			fuelgauge->is_charging = true;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET)
			fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		s2mpw01_set_temperature(fuelgauge, val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		pr_info("%s: capacity_max changed, %d -> %d\n",
			__func__, fuelgauge->capacity_max, val->intval);
		fuelgauge->capacity_max = s2mpw01_fg_check_capacity_max(fuelgauge, val->intval);
		fuelgauge->initial_update_of_soc = true;
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		pr_info("%s: POWER_SUPPLY_PROP_CALIBRATE \n", __func__);
		s2mpw01_restart_gauging(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_UPDATE_BATTERY_DATA:
		s2mpw01_fg_update_bat_param(fuelgauge, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mpw01_fg_isr_work(struct work_struct *work)
{
	struct s2mpw01_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mpw01_fuelgauge_data, isr_work.work);
	u8 fg_alert_status = 0;

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_STATUS+1, &fg_alert_status);
	pr_info("%s : fg_alert_status(0x%x)\n",
		__func__, fg_alert_status);

	fg_alert_status &= 0x03;
	if (fg_alert_status & 0x01)
		pr_info("%s : Battery Voltage is very Low!\n", __func__);

	if (fg_alert_status & 0x02)
		pr_info("%s : Battery Level is Very Low!\n", __func__);

	if (!fg_alert_status)
		pr_info("%s : SOC or Volage is Good!\n", __func__);
	wake_unlock(&fuelgauge->fuel_alert_wake_lock);
}

static irqreturn_t s2mpw01_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mpw01_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mpw01_fg_read_reg_byte(fuelgauge->i2c, S2MPW01_FG_REG_IRQ, &fg_irq);
	pr_info("%s: fg_irq(0x%x)\n", __func__, fg_irq);
	wake_lock(&fuelgauge->fuel_alert_wake_lock);
	schedule_delayed_work(&fuelgauge->isr_work, 0);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int s2mpw01_fuelgauge_parse_dt(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpw01-fuelgauge");
	int ret;
	int i, len;
	const u32 *p;

	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		fuelgauge->pdata->fg_irq = of_get_named_gpio(np, "fuelgauge,fuel_int", 0);
		if (fuelgauge->pdata->fg_irq < 0)
			pr_err("%s error reading fg_irq = %d\n",
				__func__, fuelgauge->pdata->fg_irq);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);
		fuelgauge->pdata->repeated_fuelalert = of_property_read_bool(np,
				"fuelgauge,repeated_fuelalert");

		p = of_get_property(np, "fuelgauge,low_temp_compensate", &len);
		if (!p)
			return -EINVAL;

		len = len / sizeof(u32);
		if (len > S2MPW01_FG_TEMP_DATA)
			return -EINVAL;

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"fuelgauge,low_temp_compensate", i,
				&fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_LOW].temp[i]);
			ret = of_property_read_u32_index(np,
				"fuelgauge,mid_temp_compensate", i,
				&fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_MID].temp[i]);
			ret = of_property_read_u32_index(np,
				"fuelgauge,high_temp_compensate", i,
				&fuelgauge->pdata->temp_fg_data[TEMP_LEVEL_HIGH].temp[i]);
		}

		/* get battery_params node */
		p = of_get_property(np, "fuelgauge,battery_data", &len);
		if (!p)
			pr_err("%s battery_params node NULL\n", __func__);
		else {
			fuelgauge->fg_num_age_step = len / sizeof(fg_age_data_info_t);
			fuelgauge->age_data_info = kzalloc(len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "fuelgauge,battery_data",
					(int *)fuelgauge->age_data_info, len/sizeof(int));
			pr_err("%s: [Long life] fuelgauge->fg_num_age_step %d \n", __func__, fuelgauge->fg_num_age_step);

			for (i = 0; i < fuelgauge->fg_num_age_step; i++)
				pr_err("%s: [Long life] age_step = %d, batcap[0] = %02x, model_param1[0] = %02x\n",
					__func__, i, fuelgauge->age_data_info[i].batcap[0], fuelgauge->age_data_info[i].model_param1[0]);
		}


		np = of_find_node_by_name(NULL, "sec-battery");
		if (!np) {
			pr_err("%s np NULL\n", __func__);
		} else {
			ret = of_property_read_string(np,
				"battery,fuelgauge_name",
				(char const **)&fuelgauge->pdata->fuelgauge_name);
			p = of_get_property(np,
					"battery,input_current_limit", &len);
			if (!p)
				return 1;

			len = len / sizeof(u32);
			fuelgauge->pdata->charging_current =
					kzalloc(sizeof(struct sec_charging_current) * len,
					GFP_KERNEL);

			for (i = 0; i < len; i++) {
				ret = of_property_read_u32_index(np,
					"battery,input_current_limit", i,
					&fuelgauge->pdata->charging_current[i].input_current_limit);
				ret = of_property_read_u32_index(np,
					"battery,fast_charging_current", i,
					&fuelgauge->pdata->charging_current[i].fast_charging_current);
				ret = of_property_read_u32_index(np,
					"battery,full_check_current_1st", i,
					&fuelgauge->pdata->charging_current[i].full_check_current_1st);
				ret = of_property_read_u32_index(np,
					"battery,full_check_current_2nd", i,
					&fuelgauge->pdata->charging_current[i].full_check_current_2nd);
			}
		}
	}

	return 0;
}

static struct of_device_id s2mpw01_fuelgauge_match_table[] = {
	{ .compatible = "samsung,s2mpw01-fuelgauge",},
	{},
};
#else
static int s2mpw01_fuelgauge_parse_dt(struct s2mpw01_fuelgauge_data *fuelgauge)
{
	return -ENOSYS;
}

#define s2mpw01_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static int s2mpw01_fuelgauge_probe(struct platform_device *pdev)
{
	struct s2mpw01_dev *s2mpw01 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw01_fuelgauge_data *fuelgauge;
	union power_supply_propval raw_soc_val;
	int ret = 0;

	pr_info("%s: S2MPW01 Fuelgauge Driver Loading\n", __func__);

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->dev = &pdev->dev;
	fuelgauge->i2c = s2mpw01->fuelgauge;
	fuelgauge->pmic = s2mpw01->pmic;

	fuelgauge->pdata = devm_kzalloc(&pdev->dev, sizeof(*(fuelgauge->pdata)),
			GFP_KERNEL);
	if (!fuelgauge->pdata) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpw01_fuelgauge_parse_dt(fuelgauge);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, fuelgauge);

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "sec-fuelgauge";

	fuelgauge->psy_fg.name          = fuelgauge->pdata->fuelgauge_name;
	fuelgauge->psy_fg.type          = POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property  = s2mpw01_fg_get_property;
	fuelgauge->psy_fg.set_property  = s2mpw01_fg_set_property;
	fuelgauge->psy_fg.properties    = s2mpw01_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
			ARRAY_SIZE(s2mpw01_fuelgauge_props);

	/* temperature level init */
	fuelgauge->before_temp_level = TEMP_LEVEL_MID;

	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	fuelgauge->scaled_capacity_max = SCALED_VAL_UNKNOWN;
	/* get the stored scaled capacity max value */
	INIT_DELAYED_WORK(&fuelgauge->scaled_work, s2mpw01_fg_get_scaled_capacity_max);
	schedule_delayed_work(&fuelgauge->scaled_work, msecs_to_jiffies(3000));
	raw_soc_val.intval = s2mpw01_get_rawsoc(fuelgauge);
#if 0 /* Fix me */
	s2mpw01_init_regs(fuelgauge);
	if (raw_soc_val.intval == 0)
		raw_soc_val.intval = s2mpw01_get_rawsoc(fuelgauge);
#endif
	raw_soc_val.intval = raw_soc_val.intval / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		s2mpw01_fg_calculate_dynamic_scale(fuelgauge, 100);

	ret = power_supply_register(&pdev->dev, &fuelgauge->psy_fg);
	if (ret) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		goto err_data_free;
	}

	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mpw01_fuelgauge_fuelalert_init(fuelgauge,
					fuelgauge->pdata->fuel_alert_soc);
		wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
					WAKE_LOCK_SUSPEND, "fuel_alerted");
		if (fuelgauge->pdata->fg_irq > 0) {
			INIT_DELAYED_WORK(&fuelgauge->isr_work, s2mpw01_fg_isr_work);

			fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
			pr_info("%s : fg_irq = %d\n", __func__, fuelgauge->fg_irq);
			if (fuelgauge->fg_irq > 0) {
				ret = request_threaded_irq(fuelgauge->fg_irq,
					NULL, s2mpw01_fg_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					"fuelgauge-irq", fuelgauge);
				if (ret < 0) {
					pr_err("%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}
				ret = enable_irq_wake(fuelgauge->fg_irq);
				if (ret < 0)
					pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
			} else {
				pr_err("%s: Failed gpio_to_irq(%d)\n",
						__func__, fuelgauge->fg_irq);
				goto err_supply_unreg;
			}
		}
	}

	fuelgauge->initial_update_of_soc = true;

	pr_info("%s: S2MPW01 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(&fuelgauge->psy_fg);
err_data_free:
	if (pdev->dev.of_node)
		kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static const struct i2c_device_id s2mpw01_fuelgauge_id[] = {
	{"s2mpw01-fuelgauge", 0},
	{}
};

static void s2mpw01_fuelgauge_shutdown(struct platform_device *pdev)
{
}

static int s2mpw01_fuelgauge_remove(struct platform_device *pdev)
{
	struct s2mpw01_fuelgauge_data *fuelgauge = platform_get_drvdata(pdev);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

#if defined CONFIG_PM
static int s2mpw01_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mpw01_fuelgauge_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mpw01_fuelgauge_suspend NULL
#define s2mpw01_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mpw01_fuelgauge_pm_ops, s2mpw01_fuelgauge_suspend,
		s2mpw01_fuelgauge_resume);

static struct platform_driver s2mpw01_fuelgauge_driver = {
	.driver = {
		.name = "s2mpw01-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mpw01_fuelgauge_pm_ops,
		.of_match_table = s2mpw01_fuelgauge_match_table,
	},
	.probe  = s2mpw01_fuelgauge_probe,
	.remove = s2mpw01_fuelgauge_remove,
	.shutdown   = s2mpw01_fuelgauge_shutdown,
/*	.id_table   = s2mpw01_fuelgauge_id,	*/
};

static int __init s2mpw01_fuelgauge_init(void)
{
	int ret = 0;
	pr_info("%s: S2MPW01 Fuelgauge Init\n", __func__);
	ret = platform_driver_register(&s2mpw01_fuelgauge_driver);

	return ret;
}

static void __exit s2mpw01_fuelgauge_exit(void)
{
	platform_driver_unregister(&s2mpw01_fuelgauge_driver);
}
device_initcall(s2mpw01_fuelgauge_init);
module_exit(s2mpw01_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung S2MPW01 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
