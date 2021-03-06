/*
 *  p9220_charger.c
 *  Samsung p9220 Charger Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 * Yeongmi Ha <yeongmi86.ha@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/ctype.h>
#include <linux/firmware.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/alarmtimer.h>
#include <linux/battery/charger/p9220_charger.h>

#define ENABLE 1
#define DISABLE 0
#define CMD_CNT 3

#define P9220S_FW_SDCARD_BIN_PATH	"sdcard/p9220_otp.bin"
#define P9220S_OTP_FW_HEX_PATH		"idt/p9220_otp.bin"
#define P9220S_SRAM_FW_HEX_PATH		"idt/p9220_sram.bin"

bool sleep_mode = false;

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	POWER_SUPPLY_PROP_MANUFACTURER,
#endif
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_TEMP,
};

int p9220_otp_update = 0;
u8 adc_cal = 0;

extern struct class *power_supply_class;
extern unsigned int batt_booting_chk;

struct device *wpc_device;

extern int charger_muic_disable(int val);
extern unsigned int lpcharge;
extern void wireless_of_mst_hw_onoff(bool on);
int p9220_get_firmware_version(struct p9220_charger_data *charger, int firm_mode);

static int p9220_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];
	u8 rbuf[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rbuf;

	mutex_lock(&charger->io_lock);
	ret = i2c_transfer(client->adapter, msg, 2);
	mutex_unlock(&charger->io_lock);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s: i2c read error, reg: 0x%x, ret: %d\n",
			__func__, reg, ret);
		return -1;
	}
	*val = rbuf[0];

	if(!p9220_otp_update)
		pr_debug("%s reg = 0x%x, data = 0x%x\n", __func__, reg, *val);

	return ret;
}

static int p9220_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };

	mutex_lock(&charger->io_lock);
	ret = i2c_master_send(client, data, 3);
	mutex_unlock(&charger->io_lock);
	if (ret < 3) {
		dev_err(&client->dev, "%s: i2c write error, reg: 0x%x, ret: %d\n",
				__func__, reg, ret);
		return ret < 0 ? ret : -EIO;
	}

	pr_info("%s reg = 0x%x, data = 0x%x \n", __func__, reg, val);

	return 0;
}

static int p9220_reg_update(struct i2c_client *client, u16 reg, u8 val, u8 mask)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };
	u8 data2;
	int ret;

	ret = p9220_reg_read(client, reg, &data2);
	if (ret >= 0) {
		u8 old_val = data2 & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		data[2] = new_val;

		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, 3);
		mutex_unlock(&charger->io_lock);
		if (ret < 3) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x, ret: %d\n",
				__func__, reg, ret);
			return ret < 0 ? ret : -EIO;
		}
	}
	p9220_reg_read(client, reg, &data2);

	pr_info("%s reg = 0x%x, data = 0x%x, mask = 0x%x\n", __func__, reg, val, mask);

	return ret;
}

#if 0
static int p9220_reg_multi_read(struct i2c_client *client, u16 reg, u8 *val, int size)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];

//	pr_debug("%s: reg = 0x%x, size = 0x%x\n", __func__, reg, size);
	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = val;

	mutex_lock(&charger->io_lock);
	ret = i2c_transfer(client->adapter, msg, 2);
	mutex_unlock(&charger->io_lock);
	if (ret < 0)
	{
		pr_err("%s: i2c transfer fail", __func__);
		return -1;
	}

	return ret;
}

static int p9220_reg_multi_write(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	const int sendsz = 16;
	unsigned char data[sendsz+2];
	int cnt = 0;

	dev_err(&client->dev, "%s: size: 0x%x\n",
				__func__, size);
	while(size > sendsz) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, sendsz);
		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, sendsz+2);
		mutex_unlock(&charger->io_lock);
		if (ret < sendsz+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		cnt = cnt + sendsz;
		size = size - sendsz;
	}
	if (size > 0) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, size);
		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, size+2);
		mutex_unlock(&charger->io_lock);
		if (ret < size+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
	}

	return ret;
}

static int p9220_clear_sram(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	int ret;
	const int sendsz = 64;
	unsigned char data[sendsz+2];
	int cnt = 0;

	while(size > sendsz) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val, sendsz);
//		dev_dbg(&client->dev, "%s: addr: 0x%x, cnt: 0x%x\n", __func__, reg+cnt, cnt);
		ret = i2c_master_send(client, data, sendsz+2);
		if (ret < sendsz+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		cnt = cnt + sendsz;
		size = size - sendsz;
	}
	if (size > 0) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val, size);
//		dev_dbg(&client->dev, "%s: addr: 0x%x, cnt: 0x%x, size: 0x%x\n", __func__, reg+cnt, cnt, size);
		ret = i2c_master_send(client, data, size+2);
		if (ret < size+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n",
					__func__, reg);
			return ret < 0 ? ret : -EIO;
		}
	}

	return ret;
}
#endif

int p9220_get_adc(struct p9220_charger_data *charger, int adc_type)
{
	int ret = 0;
	u8 data[2] = {0,};

	switch (adc_type) {
		case P9220_ADC_VOUT:
			ret = p9220_reg_read(charger->client, P9220_ADC_VOUT_L_REG, &data[0]);
			ret = p9220_reg_read(charger->client, P9220_ADC_VOUT_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8))*12600/4095;
			} else
				ret = 0;
			break;
		case P9220_ADC_VRECT:
			ret = p9220_reg_read(charger->client, P9220_ADC_VRECT_L_REG, &data[0]);
			ret = p9220_reg_read(charger->client, P9220_ADC_VRECT_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8))*21000/4095;
			} else
				ret = 0;
			break;
		case P9220_ADC_TX_ISENSE:
			ret = p9220_reg_read(charger->client, P9220_ADC_TX_ISENSE_L_REG, &data[0]);
			ret = p9220_reg_read(charger->client, P9220_ADC_TX_ISENSE_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;
		case P9220_ADC_RX_IOUT:
			ret = p9220_reg_read(charger->client, P9220_ADC_RX_IOUT_L_REG, &data[0]);
			ret = p9220_reg_read(charger->client, P9220_ADC_RX_IOUT_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;
		case P9220_ADC_DIE_TEMP:
			ret = p9220_reg_read(charger->client, P9220_ADC_DIE_TEMP_L_REG, &data[0]);
			ret = p9220_reg_read(charger->client, P9220_ADC_DIE_TEMP_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;

		case P9220_ADC_ALLIGN_X:
			ret = p9220_reg_read(charger->client, P9220_ADC_ALLIGN_X_REG, &data[0]);
			if(ret >= 0 ) {
				ret = data[0]; // need to check
			} else
				ret = 0;
			break;

		case P9220_ADC_ALLIGN_Y:
			ret = p9220_reg_read(charger->client, P9220_ADC_ALLIGN_Y_REG, &data[0]);
			if(ret >= 0 ) {
				ret = data[0]; // need to check
			} else
				ret = 0;
			break;
		case P9220_ADC_OP_FRQ:
			ret = p9220_reg_read(charger->client, P9220_OP_FREQ_L_REG, &data[0]);
			if(ret < 0 ) {
				ret = 0;
				return ret;
			}
			ret = p9220_reg_read(charger->client, P9220_OP_FREQ_H_REG, &data[1]);
			if(ret >= 0 )
				ret = (data[0] | (data[1] << 8));
			else
				ret = 0;
			break;
		default:
			break;
	}

	return ret;
}

void p9220_set_vout(struct p9220_charger_data *charger, int vout)
{
	switch (vout) {
		case P9220_VOUT_5V:
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_5V_VAL);
			msleep(100);
			pr_info("%s vout read = %d mV \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			break;
		case P9220_VOUT_6V:
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_6V_VAL);
			msleep(100);
			pr_info("%s vout read = %d mV \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			break;
		case P9220_VOUT_9V:
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_9V_VAL);
			msleep(100);
			pr_info("%s vout read = %d mV \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			break;
		case P9220_VOUT_CV_CALL:
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG,
							(charger->pdata->wpc_cv_call_vout - 3500) / 100);
			msleep(100);
			pr_info("%s vout read = %d mV \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			break;
		case P9220_VOUT_CC_CALL:
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG,
							(charger->pdata->wpc_cc_call_vout - 3500) / 100);
			msleep(100);
			pr_info("%s vout read = %d mV \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			break;
		case P9220_VOUT_9V_STEP:
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_6V_VAL);
			msleep(1000);
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_7V_VAL);
			msleep(1000);
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_8V_VAL);
			msleep(1000);
			p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_9V_VAL);
			msleep(1000);		
			pr_info("%s vout read = %d mV \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			break;
		default:
			break;
	}
	charger->pdata->vout_status = vout;
}

void p9220_fod_set(struct p9220_charger_data *charger)
{
	int i = 0;

	pr_info("%s \n", __func__);
	if(charger->pdata->fod_data_check) {
		for(i=0; i< P9220_NUM_FOD_REG; i++)
			p9220_reg_write(charger->client, P9220_WPC_FOD_0A_REG+i, charger->pdata->fod_data[i]);
	}
}

void p9220_set_cmd_reg(struct p9220_charger_data *charger, u8 val, u8 mask)
{
	u8 temp = 0;
	int ret = 0, i = 0;

	do {
		pr_info("%s \n", __func__);
		ret = p9220_reg_update(charger->client, P9220_COMMAND_REG, val, mask); // command
		if(ret >= 0) {
			msleep(250);
			ret = p9220_reg_read(charger->client, P9220_COMMAND_REG, &temp); // check out set bit exists
			if(ret < 0 || i > 3 )
				break;
		}
		i++;
	}while(temp != 0);
}

void p9220_send_eop(struct p9220_charger_data *charger, int healt_mode)
{
	int i = 0;
	int ret = 0;

	switch(healt_mode) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_OVERHEATLIMIT:
		case POWER_SUPPLY_HEALTH_COLD:
		if(charger->pdata->cable_type == SEC_WIRELESS_PAD_PMA) {
			pr_info("%s pma mode \n", __func__);
			for(i = 0; i < CMD_CNT; i++) {
				ret = p9220_reg_write(charger->client, P9220_END_POWER_TRANSFER_REG, P9220_EPT_END_OF_CHG);
				if(ret >= 0) {
					p9220_set_cmd_reg(charger, P9220_CMD_SEND_EOP_MASK, P9220_CMD_SEND_EOP_MASK);
					msleep(250);
				} else
					break;
			}
		} else {
			pr_info("%s wpc mode \n", __func__);
			for(i = 0; i < CMD_CNT; i++) {
				ret = p9220_reg_write(charger->client, P9220_END_POWER_TRANSFER_REG, P9220_EPT_OVER_TEMP);
				if(ret >= 0) {
					p9220_set_cmd_reg(charger, P9220_CMD_SEND_EOP_MASK, P9220_CMD_SEND_EOP_MASK);
					msleep(250);
				} else
					break;
			}
		}
		break;
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
#if 0
			pr_info("%s ept-reconfigure \n", __func__);
			ret = p9220_reg_write(charger->client, P9220_END_POWER_TRANSFER_REG, P9220_EPT_RECONFIG);
			if(ret >= 0) {
				p9220_set_cmd_reg(charger, P9220_CMD_SEND_EOP_MASK, P9220_CMD_SEND_EOP_MASK);
				msleep(250);
			}
#endif
		break;
		default:
		break;
	}
}

int p9220_send_cs100(struct p9220_charger_data *charger)
{
	int i = 0;
	int ret = 0;

	for(i = 0; i < CMD_CNT; i++) {
		ret = p9220_reg_write(charger->client, P9220_CHG_STATUS_REG, 100);
		if(ret >= 0) {
			p9220_set_cmd_reg(charger, P9220_CMD_SEND_CHG_STS_MASK, P9220_CMD_SEND_CHG_STS_MASK);
			msleep(250);
			ret = 1;
		} else {
			ret = -1;
			break;
		}
	}
	return ret;
}

void p9220_send_packet(struct p9220_charger_data *charger, u8 header, u8 rx_data_com, u8 *data_val, int data_size)
{
	int ret;
	int i;
	ret = p9220_reg_write(charger->client, P9220_PACKET_HEADER, header);
	ret = p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, rx_data_com);

	for(i = 0; i< data_size; i++) {
		ret = p9220_reg_write(charger->client, P9220_RX_DATA_VALUE0 + i, data_val[i]);
	}
	p9220_set_cmd_reg(charger, P9220_CMD_SEND_RX_DATA_MASK, P9220_CMD_SEND_RX_DATA_MASK);
}

void p9220_send_command(struct p9220_charger_data *charger, int cmd_mode)
{
	//u8 data_val[4];
	//union power_supply_propval value;
	//int vrect = 0, i = 0;

	switch (cmd_mode) {
#if 0		
		case P9220_AFC_CONF_5V:
			pr_info("%s set 5V \n", __func__);
			if(!lpcharge) {
				value.intval = ENABLE;
				psy_do_property("max77833-charger", set, POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL, value);
			}
			data_val[0] = 0x05;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_AFC_SET, data_val, 1);
			msleep(120);

			p9220_set_vout(charger, P9220_VOUT_5V);
			pr_info("%s vout read = %d \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			pr_info("%s --- debug -- \n", __func__);
			p9220_reg_read(charger->client, P9220_RX_DATA_COMMAND, &data_val[0]);
			p9220_reg_read(charger->client, P9220_RX_DATA_VALUE0, &data_val[0]);
			p9220_reg_read(charger->client, P9220_COMMAND_REG, &data_val[0]);
			if(!lpcharge) {
				value.intval = DISABLE;
				psy_do_property("max77833-charger", set, POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL, value);
			}
			break;
		case P9220_AFC_CONF_9V:
			pr_info("%s set 9V \n", __func__);
			psy_do_property("battery", get, POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW, value);
			if (value.intval == POWER_SUPPLY_TYPE_USB){
				pr_info("%s wire_status = %d. do not set 9V \n", __func__,  value.intval);
				break;
			}

			data_val[0] = 0x2c;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_AFC_SET, data_val, 1);
			msleep(120);

			p9220_set_vout(charger, P9220_VOUT_9V);
			pr_info("%s vout read = %d \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));

#if 0
			do {
				vrect = p9220_get_adc(charger, P9220_ADC_VRECT);
				pr_info("%s vrect = %d mV \n", __func__, vrect);
				msleep(5);
				i++;
				if (i > 3)
					break;
			} while(vrect > 7000);

			if(vrect > 7000) {
				p9220_set_vout(charger, P9220_VOUT_9V);
				pr_info("%s vout read = %d \n", __func__,  p9220_get_adc(charger, P9220_ADC_VOUT));
			}
#endif			
			pr_info("%s --- debug -- \n", __func__);
			p9220_reg_read(charger->client, P9220_RX_DATA_COMMAND, &data_val[0]);
			p9220_reg_read(charger->client, P9220_RX_DATA_VALUE0, &data_val[0]);
			p9220_reg_read(charger->client, P9220_COMMAND_REG, &data_val[0]);
			break;
		case P9220_LED_CONTROL_ON:
			pr_info("%s led on \n", __func__);
			data_val[0] = 0x0;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_LED_CTRL, data_val, 1);
			break;
		case P9220_LED_CONTROL_OFF:
			pr_info("%s led off \n", __func__);
			data_val[0] = 0xff;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_LED_CTRL, data_val, 1);
			break;
		case P9220_FAN_CONTROL_ON:
			pr_info("%s fan on \n", __func__);
			data_val[0] = 0x0;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_FAN_CTRL, data_val, 1);
			break;
		case P9220_FAN_CONTROL_OFF:
			pr_info("%s fan off \n", __func__);
			data_val[0] = 0xff;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_FAN_CTRL, data_val, 1);
			break;
		case P9220_REQUEST_AFC_TX:
			pr_info("%s request afc tx \n", __func__);
			data_val[0] = 0x0;
			p9220_send_packet(charger, P9220_HEADER_AFC_CONF, P9220_RX_DATA_COM_REQ_AFC, data_val, 1);
			break;
#endif			
		default:
			break;
	}
}

#if defined(CONFIG_WPC_PH_MODE)
void p9220_send_php(struct p9220_charger_data *charger)
{
	int i = 0;

	for(i = 0; i < 5; i++) {
		p9220_reg_write(charger->client, P9220_PACKET_HEADER, 0x28);
		p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, 0x18);
		p9220_reg_write(charger->client, P9220_COMMAND_REG, 0x01);
	}
	wake_lock(&charger->wpc_wake_lock);
	charger->last_poll_time = ktime_get_boottime();
	alarm_start(&charger->polling_alarm,
		    ktime_add(charger->last_poll_time, ktime_set(70, 0)));
}
#endif

void p9220_led_control(struct p9220_charger_data *charger, bool on)
{
	int i = 0;

	if(on) {
		for(i=0; i< CMD_CNT; i++)
			p9220_send_command(charger, P9220_LED_CONTROL_ON);
	} else {
		for(i=0; i< CMD_CNT; i++)
			p9220_send_command(charger, P9220_LED_CONTROL_OFF);
	}
}

void p9220_fan_control(struct p9220_charger_data *charger, bool on)
{
	int i = 0;

	if(on) {
		for(i=0; i< CMD_CNT -1; i++)
			p9220_send_command(charger, P9220_FAN_CONTROL_ON);
	} else {
		for(i=0; i< CMD_CNT -1; i++)
			p9220_send_command(charger, P9220_FAN_CONTROL_OFF);
	}
}

void p9220_set_vrect_adjust(struct p9220_charger_data *charger, int set)
{
	int i = 0;
	pr_info("%s %d\n", __func__, set);

	switch (set) {
		case P9220_HEADROOM_0:
			for(i=0; i<6; i++) {
				p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 0x0);
				msleep(50);
			}
			break;
		case P9220_HEADROOM_1:
			for(i=0; i<6; i++) {
				p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 0x36);
				msleep(50);
			}
			break;
		case P9220_HEADROOM_2:
			for(i=0; i<6; i++) {
				p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 0x61);
				msleep(50);
			}
			break;
		case P9220_HEADROOM_3:
			for(i=0; i<6; i++) {
				p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 0x7f);
				msleep(50);
			}
			break;
		default:
			pr_info("%s no headroom mode\n", __func__);
			break;
	}
}

void p9220_mis_align(struct p9220_charger_data *charger)
{
	pr_info("%s: Reset M0\n",__func__);
	p9220_reg_write(charger->client, 0x3040, 0x80); /*restart M0 */
}

int p9220_get_firmware_version(struct p9220_charger_data *charger, int firm_mode)
{
	int version = -1;
	int ret;
	u8 otp_fw_major[2] = {0,};
	u8 otp_fw_minor[2] = {0,};
	u8 tx_fw_major[2] = {0,};
	u8 tx_fw_minor[2] = {0,};

	switch (firm_mode) {
		case P9220_RX_FIRMWARE:
			ret = p9220_reg_read(charger->client, P9220_OTP_FW_MAJOR_REV_L_REG, &otp_fw_major[0]);
			ret = p9220_reg_read(charger->client, P9220_OTP_FW_MAJOR_REV_H_REG, &otp_fw_major[1]);
			if (ret >= 0) {
				version =  otp_fw_major[0] | (otp_fw_major[1] << 8);
			}
			pr_debug("%s rx major firmware version 0x%x \n", __func__, version);

			ret = p9220_reg_read(charger->client, P9220_OTP_FW_MINOR_REV_L_REG, &otp_fw_minor[0]);
			ret = p9220_reg_read(charger->client, P9220_OTP_FW_MINOR_REV_H_REG, &otp_fw_minor[1]);
			if (ret >= 0) {
				version =  otp_fw_minor[0] | (otp_fw_minor[1] << 8);
			}
			pr_debug("%s rx minor firmware version 0x%x \n", __func__, version);
			break;
		case P9220_TX_FIRMWARE:
			ret = p9220_reg_read(charger->client, P9220_SRAM_FW_MAJOR_REV_L_REG, &tx_fw_major[0]);
			ret = p9220_reg_read(charger->client, P9220_SRAM_FW_MAJOR_REV_H_REG, &tx_fw_major[1]);
			if (ret >= 0) {
				version =  tx_fw_major[0] | (tx_fw_major[1] << 8);
			}
			pr_debug("%s tx major firmware version 0x%x \n", __func__, version);

			ret = p9220_reg_read(charger->client, P9220_SRAM_FW_MINOR_REV_L_REG, &tx_fw_minor[0]);
			ret = p9220_reg_read(charger->client, P9220_SRAM_FW_MINOR_REV_H_REG, &tx_fw_minor[1]);
			if (ret >= 0) {
				version =  tx_fw_minor[0] | (tx_fw_minor[1] << 8);
			}
			pr_debug("%s tx minor firmware version 0x%x \n", __func__, version);
			break;
		default:
			pr_err("%s Wrong firmware mode \n", __func__);
			version = -1;
			break;
	}

	return version;
}

int p9220_get_ic_grade(struct p9220_charger_data *charger, int read_mode)
{
	u8 grade;
	int ret;

	switch (read_mode) {
		case P9220_IC_GRADE:
			ret = p9220_reg_read(charger->client, P9220_CHIP_REVISION_REG, &grade);

			if(ret >= 0) {
				grade &= P9220_CHIP_GRADE_MASK;
				pr_info("%s ic grade = %d \n", __func__, grade);
				ret =  grade;
			}
			else
				ret = -1;
			break;
		case P9220_IC_VERSION:
			ret = p9220_reg_read(charger->client, P9220_CHIP_REVISION_REG, &grade);

			if(ret >= 0) {
				grade &= P9220_CHIP_REVISION_MASK;
				pr_info("%s ic version = %d \n", __func__, grade);
				ret =  grade;
			}
			else
				ret = -1;
			break;
		case P9220_IC_VENDOR:
			ret = -1;
			break;
		default :
			ret = -1;
			break;
	}
	return ret;
}

void p9220_wireless_chg_init(struct p9220_charger_data *charger)
{
	pr_info("%s \n", __func__);

	p9220_set_vout(charger, P9220_VOUT_5V);
}

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
static int datacmp(const char *cs, const char *ct, int count)
{
	unsigned char c1, c2;

	while (count) {
		c1 = *cs++;
		c2 = *ct++;
		if (c1 != c2) {
			pr_err("%s, cnt %d\n", __func__, count);
			return c1 < c2 ? -1 : 1;
		}
		/*debug
		else {
			pr_err("%s, c1:%x c2:%x\n", __func__, c1, c2);
		}
		debug*/
		count--;
	}
	return 0;
}

static int LoadOTPLoaderInRAM(struct p9220_charger_data *charger, u16 addr)
{
	int i, size;
	u8 data[1016];
	if (p9220_reg_multi_write(charger->client, addr, OTPBootloader, sizeof(OTPBootloader)) < 0) {
		pr_err("%s,fail", __func__);
	}
	size = sizeof(OTPBootloader);
	i = 0;
	while(size > 0) {
		if (p9220_reg_multi_read(charger->client, addr+i, data+i, 16) < 0) {
			pr_err("%s, read failed(%d)", __func__, addr+i);
			return 0;
		}
		i += 16;
		size -= 16;
	}
	size = sizeof(OTPBootloader);
	
	i = 0;
	for (i = 0 ; i < size; i++) {
		if (i%16 == 0)
			pr_err("addr = 0x%x : \n", addr+i);
		pr_err(" 0x%x\n", data[i]);
	}

	if (datacmp(data, OTPBootloader, size)) {
		pr_err("%s, data is not matched\n", __func__);
		return 0;
	}
	return 1;
}

static int p9220_firmware_verify(struct p9220_charger_data *charger)
{
	int ret = 0;
	const u16 sendsz = 16;
	int i = 0;
	int block_len = 0;
	int block_addr = 0;
	u8 rdata[sendsz+2];

/* I2C WR to prepare boot-loader write */

	if (p9220_reg_write(charger->client, 0x3C00, 0x80) < 0) {
		pr_err("%s: reset FDEM error\n", __func__);
		return 0;
	}

	if (p9220_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: key error\n", __func__);
		return 0;
	}

	if (p9220_reg_write(charger->client, 0x3040, 0x11) < 0) {
		pr_err("%s: halt M0, OTP_I2C_EN set error\n", __func__);
		return 0;
	}

	if (p9220_reg_write(charger->client, 0x3C04, 0x04) < 0) {
		pr_err("%s: OTP_VRR 2.98V error\n", __func__);
		return 0;
	}

	if (p9220_reg_write(charger->client, 0x5C00, 0x11) < 0) {
		pr_err("%s: OTP_CTRL VPP_EN set error\n", __func__);
		return 0;
	}

	dev_err(&charger->client->dev, "%s, request_firmware\n", __func__);
	ret = request_firmware(&charger->firm_data_bin, P9220S_OTP_FW_HEX_PATH,
		&charger->client->dev);
	if ( ret < 0) {
		dev_err(&charger->client->dev, "%s: failed to request firmware %s (%d) \n",
				__func__, P9220S_OTP_FW_HEX_PATH, ret);
		return 0;
	}
	ret = 1;
	wake_lock(&charger->wpc_update_lock);
	for (i = 0; i < charger->firm_data_bin->size; i += sendsz) {
		block_len = (i + sendsz) > charger->firm_data_bin->size ? charger->firm_data_bin->size - i : sendsz;
		block_addr = 0x8000 + i;

		if (p9220_reg_multi_read(charger->client, block_addr, rdata, block_len) < 0) {
			pr_err("%s, read failed\n", __func__);
			ret = 0;
			break;
		}
		if (datacmp(charger->firm_data_bin->data + i, rdata, block_len)) {
			pr_err("%s, verify data is not matched.\n", __func__);
			ret = -1;
			break;
		}
	}
	release_firmware(charger->firm_data_bin);

	wake_unlock(&charger->wpc_update_lock);
	return ret;
}

static int p9220_reg_multi_write_verify(struct i2c_client *client, u16 reg, const u8 * val, int size)
{
	int ret = 0;
	const int sendsz = 16;
	int cnt = 0;
	int retry_cnt = 0;
	unsigned char data[sendsz+2];
	u8 rdata[sendsz+2];

//	dev_dbg(&client->dev, "%s: size: 0x%x\n", __func__, size);
	while(size > sendsz) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, sendsz);
//		dev_dbg(&client->dev, "%s: addr: 0x%x, cnt: 0x%x\n", __func__, reg+cnt, cnt);
		ret = i2c_master_send(client, data, sendsz+2);
		if (ret < sendsz+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n", __func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		if (p9220_reg_multi_read(client, reg+cnt, rdata, sendsz) < 0) {
			pr_err("%s, read failed(%d)\n", __func__, reg+cnt);
			return -1;
		}
		if (datacmp(val+cnt, rdata, sendsz)) {
			pr_err("%s, data is not matched. retry(%d)\n", __func__, retry_cnt);
			retry_cnt++;
			if(retry_cnt > 4) {
				pr_err("%s, data is not matched. write failed\n", __func__);
				retry_cnt = 0;
				return -1;
			}
			continue;
		}
//		pr_debug("%s, data is matched!\n", __func__);
		cnt += sendsz;
		size -= sendsz;
		retry_cnt = 0;
	}
	while (size > 0) {
		data[0] = (reg+cnt) >>8;
		data[1] = (reg+cnt) & 0xff;
		memcpy(data+2, val+cnt, size);
//		dev_dbg(&client->dev, "%s: addr: 0x%x, cnt: 0x%x, size: 0x%x\n", __func__, reg+cnt, cnt, size);
		ret = i2c_master_send(client, data, size+2);
		if (ret < size+2) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x\n", __func__, reg);
			return ret < 0 ? ret : -EIO;
		}
		if (p9220_reg_multi_read(client, reg+cnt, rdata, size) < 0) {
			pr_err("%s, read failed(%d)\n", __func__, reg+cnt);
			return -1;
		}
		if (datacmp(val+cnt, rdata, size)) {
			pr_err("%s, data is not matched. retry(%d)\n", __func__, retry_cnt);
			retry_cnt++;
			if(retry_cnt > 4) {
				pr_err("%s, data is not matched. write failed\n", __func__);
				retry_cnt = 0;
				return -1;
			}
			continue;
		}
		size = 0;
		pr_err("%s, data is matched!\n", __func__);
	}
	return ret;
}

static int p9220_TxFW_SRAM(struct p9220_charger_data *charger, const u8 * srcData, int fw_size)
{
	const u8 clear_data[] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

	u16 addr;
	u8 temp;
	int ret;

	pr_info("%s TX FW update Start \n",__func__);

	if (p9220_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error\n", __func__);
		return false;
	}
	if (p9220_reg_write(charger->client, 0x3040, 0x10) < 0) {
		pr_err("%s: halt M0 error\n", __func__);
		return false;
	}

	/* clear 0x0000 to 0x1FFF */
	if (p9220_clear_sram(charger->client, 0x0000, clear_data, 0x2000) < 0) {
		pr_err("%s: clear memory error\n", __func__);
		return false;
	}
	addr = 0x0800;
	if (p9220_reg_multi_write_verify(charger->client, addr, srcData, fw_size) < 0) {
		pr_err("%s: write fail", __func__);
		return false;
	}

	if (p9220_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error..\n", __func__);
		return false;
	}

	if (p9220_reg_write(charger->client, 0x3048, 0xD0) < 0) {
		pr_err("%s: remapping error..\n", __func__);
		return false;
	}

	p9220_reg_write(charger->client, 0x3040, 0x80);

	//p9220_reg_write(charger->client, 0x684, 0x21);
	//p9220_reg_write(charger->client, 0x685, 0x2);

	pr_err("TX firmware update finished.\n");
	ret = p9220_reg_read(charger->client, P9220_SYS_OP_MODE_REG, &temp);
	pr_info("%s: SYS_OP_MODE 0x%x, ret: %d\n", __func__, temp, ret);
	return true;
}

static int PgmOTPwRAM(struct p9220_charger_data *charger, unsigned short OtpAddr,
					  const u8 * srcData, int srcOffs, int size)
{
	int i, j, cnt;
	int block_len = 0;
	u8 fw_major[2] = {0,};
	u8 fw_minor[2] = {0,};

	p9220_reg_read(charger->client, P9220_OTP_FW_MAJOR_REV_L_REG, &fw_major[0]);
	p9220_reg_read(charger->client, P9220_OTP_FW_MAJOR_REV_H_REG, &fw_major[1]);
	p9220_reg_read(charger->client, P9220_OTP_FW_MINOR_REV_L_REG, &fw_minor[0]);
	p9220_reg_read(charger->client, P9220_OTP_FW_MINOR_REV_H_REG, &fw_minor[1]);

	if (p9220_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error\n", __func__);
		return false;		// write key
	}
	if (p9220_reg_write(charger->client, 0x3040, 0x10) < 0) {
		pr_err("%s: halt M0 error\n", __func__);
		return false;		// halt M0
	}
	if (!LoadOTPLoaderInRAM(charger, 0x1c00)){
		pr_err("%s: LoadOTPLoaderInRAM error\n", __func__);
		return false;		// make sure load address and 1KB size are OK
	}

	if (p9220_reg_write(charger->client, 0x3048, 0x80) < 0) {
		pr_err("%s: map RAM to OTP error\n", __func__);
		return false;		// map RAM to OTP
	}
	p9220_reg_write(charger->client, 0x3040, 0x80);
	msleep(100);

	for (i = 0; i < size; i += 128)		// program pages of 128 bytes
	{
		u8 sBuf[136] = {0,};
		u16 StartAddr = (u16)i;
		u16 CheckSum = StartAddr;
		u16 CodeLength = 128;

		block_len = (i + 128) > size ? size - i : 128;
		if (block_len == 128) {
			memcpy(sBuf + 8, srcData + i + srcOffs, 128);
		} else {
			memset(sBuf, 0, 136);
			memcpy(sBuf + 8, srcData + i + srcOffs, block_len);
		}

		for (j = 127; j >= 0; j--)
		{
			if (sBuf[j + 8] != 0)
				break;
			else
				CodeLength--;
		}
		if (CodeLength == 0)
			continue;				// skip programming if nothing to program
		for (; j >= 0; j--)
			CheckSum += sBuf[j + 8];	// add the non zero values
		CheckSum += CodeLength;			// finish calculation of the check sum

		memcpy(sBuf+2, &StartAddr,2);
		memcpy(sBuf+4, &CodeLength,2);
		memcpy(sBuf+6, &CheckSum,2);

		if (p9220_reg_multi_write_verify(charger->client, 0x400, sBuf, CodeLength + 8) < 0)
		{
			pr_err("ERROR: on writing to OTP buffer");
			return false;
		}
		sBuf[0] = 1;
		if (p9220_reg_write(charger->client, 0x400, sBuf[0]) < 0)
		{
			pr_err("ERROR: on OTP buffer validation");
			return false;
		}

		cnt = 0;
		do
		{
			msleep(20);
			if (p9220_reg_read(charger->client, 0x400, sBuf) < 0)
			{
				pr_err("ERROR: on readign OTP buffer status(%d)", cnt);
				return false;
			}
			 if (cnt > 1000) {
				 pr_err("ERROR: time out on buffer program to OTP");
				 break;
			 }
			 cnt++;
		} while (sBuf[0] == 1);

		if (sBuf[0] != 2)		// not OK
		{
			pr_err("ERROR: buffer write to OTP returned status %d ",sBuf[0]);
			return false;
		}
	}

	if (p9220_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: write key error..\n", __func__);
		return false;		// write key
	}
	if (p9220_reg_write(charger->client, 0x3048, 0x00) < 0) {
		pr_err("%s: remove code remapping error..\n", __func__);
		return false;		// remove code remapping
	}

	pr_err("OTP Programming finished in");
	pr_info("%s------------------------------------------------- \n", __func__);
	return true;
}

static int p9220_runtime_sram_change(struct p9220_charger_data *charger)
{
	int i = 0, ret = 0;
	u8 reg;

	pr_info("%s \n", __func__);

	do {
		ret = p9220_reg_write(charger->client, 0x5834, adc_cal);
		ret = p9220_reg_read(charger->client, 0x5834, &reg);
		pr_info("%s [%d] otp : 0x%x, sram : 0x%x \n", __func__, i, adc_cal, reg);
		if(i > 10 || ret < 0)
			return false;
		msleep(10);
		i++;
	} while(reg != adc_cal);

	return true;
}

int p9220_runtime_sram_preprocess(struct p9220_charger_data *charger)
{
	u8 reg;
	u8 pad_mode;

	pr_info("%s \n", __func__);

	if(gpio_get_value(charger->pdata->wpc_det)) {
		pr_info("%s it is wilreless lpm \n", __func__);
		p9220_reg_read(charger->client, P9220_SYS_OP_MODE_REG, &pad_mode);
		pr_info("%s pad_mode = %d \n", __func__, pad_mode);

		if(pad_mode == P9220_SYS_MODE_PMA)
			return true;
	}

	if (p9220_reg_write(charger->client, 0x3000, 0x5a) < 0) {
		pr_err("%s: failed unlock register\n", __func__);
	}

	if (p9220_reg_write(charger->client, 0x3040, 0x11) < 0) {
		pr_err("%s: failed stop process\n", __func__);
	}

	//write 1 at bit0 of 0xbfbe
	if(p9220_reg_read(charger->client, 0xbfbe, &reg) < 0)
		adc_cal = 0;
	else {
		adc_cal = reg = reg | 0x01;
		pr_info("%s 0xbfbe = 0x%x \n", __func__, reg);
	}

	return true;
}

int p9220_firmware_update(struct p9220_charger_data *charger, int cmd)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;	
    const u8 *fw_img;

	pr_info("%s firmware update mode is = %d \n", __func__, cmd);

	switch(cmd) {
		case SEC_WIRELESS_RX_SDCARD_MODE:
		charger->pdata->otp_firmware_result = P9220_FW_RESULT_DOWNLOADING;
		msleep(200);
		//charger_muic_disable(ENABLE);
		disable_irq(charger->pdata->irq_wpc_int);
		disable_irq(charger->pdata->irq_wpc_det);
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		fp = filp_open(P9220S_FW_SDCARD_BIN_PATH, O_RDONLY, S_IRUSR);

		if (IS_ERR(fp)) {
			pr_err("%s: failed to open %s\n", __func__, P9220S_FW_SDCARD_BIN_PATH);
			ret = -ENOENT;
			set_fs(old_fs);
			return ret;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		pr_err("%s: start, file path %s, size %ld Bytes\n",
			__func__, P9220S_FW_SDCARD_BIN_PATH, fsize);

		fw_img = kmalloc(fsize, GFP_KERNEL);

		if (fw_img == NULL) {
			pr_err("%s, kmalloc failed\n", __func__);
			ret = -EFAULT;
			goto malloc_error;
		}

		nread = vfs_read(fp, (char __user *)fw_img,
					fsize, &fp->f_pos);
		pr_err("nread %ld Bytes\n", nread);
		if (nread != fsize) {
			pr_err("failed to read firmware file, nread %ld Bytes\n", nread);
			ret = -EIO;
			goto read_err;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		p9220_otp_update = 1;
		PgmOTPwRAM(charger, 0 ,fw_img, 0, fsize);
		p9220_otp_update = 0;

		kfree(fw_img);
		enable_irq(charger->pdata->irq_wpc_int);
		enable_irq(charger->pdata->irq_wpc_det);
		//charger_muic_disable(DISABLE);
		break;
	case SEC_WIRELESS_RX_BUILT_IN_MODE:
		charger->pdata->otp_firmware_result = P9220_FW_RESULT_DOWNLOADING;
		msleep(200);
		//charger_muic_disable(ENABLE);
		disable_irq(charger->pdata->irq_wpc_int);
		disable_irq(charger->pdata->irq_wpc_det);
		dev_err(&charger->client->dev, "%s, request_firmware\n", __func__);
		ret = request_firmware(&charger->firm_data_bin, P9220S_OTP_FW_HEX_PATH,
			&charger->client->dev);
		if ( ret < 0) {
			dev_err(&charger->client->dev, "%s: failed to request firmware %s (%d) \n", __func__, P9220S_OTP_FW_HEX_PATH, ret);
			charger->pdata->otp_firmware_result = P9220_FW_RESULT_FAIL;
			return -EINVAL;
		}
		wake_lock(&charger->wpc_update_lock);
		//pr_info("%s data size = %ld \n", __func__, charger->firm_data_bin->size);
		p9220_otp_update = 1;
		ret = PgmOTPwRAM(charger, 0 ,charger->firm_data_bin->data, 0, charger->firm_data_bin->size);
		p9220_otp_update = 0;
		release_firmware(charger->firm_data_bin);

		charger->pdata->otp_firmware_ver = p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);
		charger->pdata->wc_ic_grade = p9220_get_ic_grade(charger, P9220_IC_GRADE);
		charger->pdata->wc_ic_rev = p9220_get_ic_grade(charger, P9220_IC_VERSION);

		if(ret)
			charger->pdata->otp_firmware_result = P9220_FW_RESULT_PASS;
		else
			charger->pdata->otp_firmware_result = P9220_FW_RESULT_FAIL;
		enable_irq(charger->pdata->irq_wpc_int);
		enable_irq(charger->pdata->irq_wpc_det);
		//charger_muic_disable(DISABLE);
		wake_unlock(&charger->wpc_update_lock);
		break;
	case SEC_WIRELESS_TX_ON_MODE:
		charger->pdata->tx_firmware_result = P9220_FW_RESULT_DOWNLOADING;
		msleep(200);
		//charger_muic_disable(ENABLE);
		dev_err(&charger->client->dev, "%s, built in sram mode on \n", __func__);
		ret = request_firmware(&charger->firm_data_bin, P9220S_SRAM_FW_HEX_PATH, &charger->client->dev);

		if ( ret < 0) {
			dev_err(&charger->client->dev, "%s: failed to request firmware %s (%d) \n", __func__,
								P9220S_SRAM_FW_HEX_PATH, ret);
			charger->pdata->tx_firmware_result = P9220_FW_RESULT_FAIL;
			return -EINVAL;
		}

		wake_lock(&charger->wpc_update_lock);
		//pr_info("%s data size = %ld \n", __func__, charger->firm_data_bin->size);
		ret = p9220_TxFW_SRAM(charger, charger->firm_data_bin->data, charger->firm_data_bin->size);
		release_firmware(charger->firm_data_bin);

		charger->pdata->tx_firmware_ver = p9220_get_firmware_version(charger, P9220_TX_FIRMWARE);

		if(ret) {
			charger->pdata->tx_firmware_result = P9220_FW_RESULT_PASS;
			charger->pdata->tx_status = SEC_TX_STANDBY;
		} else {
			charger->pdata->tx_firmware_result = P9220_FW_RESULT_FAIL;
			charger->pdata->tx_status = SEC_TX_ERROR;
		}
		//charger_muic_disable(DISABLE);

		charger->pdata->cable_type = P9220_PAD_MODE_TX;
		wake_unlock(&charger->wpc_update_lock);
		break;
	case SEC_WIRELESS_TX_OFF_MODE:
		charger->pdata->tx_firmware_result = P9220_FW_RESULT_DOWNLOADING;
		charger->pdata->cable_type = P9220_PAD_MODE_NONE;
		dev_err(&charger->client->dev, "%s, built in sram mode off \n", __func__);
		charger->pdata->tx_status = SEC_TX_OFF;
		break;
	case SEC_WIRELESS_RX_INIT:
		p9220_runtime_sram_preprocess(charger); /* get 0xBFBE value */
		break;
	default:
		return -1;
		break;
	}

	pr_info("%s --------------------------------------------------------------- \n", __func__);

	return 0;

read_err:
	kfree(fw_img);
malloc_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	//charger_muic_disable(DISABLE);
	return ret;
}
#endif


/*
void p9220_wireless_chg_init(struct p9220_charger_data *charger)
{
		union power_supply_propval value;
		u8 fw_major[2] = {0,};
		u8 fw_minor[2] = {0,};
	
		pr_info("%s \n", __func__);
	
		value.intval = true;
		psy_do_property("max77833-charger", set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
	
		p9220_reg_read(charger->client, P9220_OTP_FW_MAJOR_REV_L_REG, &fw_major[0]); 
		p9220_reg_read(charger->client, P9220_OTP_FW_MAJOR_REV_H_REG, &fw_major[1]); 
		p9220_reg_read(charger->client, P9220_OTP_FW_MINOR_REV_L_REG, &fw_minor[0]); 
		p9220_reg_read(charger->client, P9220_OTP_FW_MINOR_REV_H_REG, &fw_minor[1]); 
	
		pr_info("%s firm major ver = 0x%x, firm minor ver = 0x%x\n", __func__,	fw_major[0] | (fw_major[1] << 8), fw_minor[0] | (fw_minor[1] << 8));

		p9220_set_vout(charger, P9220_VOUT_5V);
	
		value.intval = false;
		psy_do_property("max77833-charger", set,
			POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
}
*/
/*
static void p9220_detect_work(
		struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_work.work);

	pr_info("%s\n", __func__);

	p9220_wireless_chg_init(charger);
}
*/

static void p9220_sec_mode_change(struct p9220_charger_data *charger)
{
	int vrect, vout;

	if (charger->pdata->can_vbat_monitoring) {
		if (!(charger->pdata->charging_mode == SEC_SWELLING_MODE ||
				charger->pdata->charging_mode == SEC_SWELLING_LDU_MODE ||
				charger->pdata->charging_mode == SEC_VBAT_MONITORING_DISABLE_MODE ||
				charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE ||
				charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_FULL_MODE)) {
			msleep(200);
			p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 9);
			msleep(200);
			p9220_reg_update(charger->client, P9220_WPC_FLAG_REG, P9220_VBAT_MONITORING_MODE, P9220_VBAT_MONITORING_MODE_MASK);
			msleep(200);
			/* 0x5053 firmware bug W/A - set Vout before Vbattery Monitoring enable */
			if (charger->pdata->otp_firmware_ver == 0x5053) {
				p9220_reg_write(charger->client, P9220_IDT_BATTERY_MODE, P9220_IDT_BATTERY_INITIAL_MODE);
				msleep(200);
			}
			pr_err("%s: vbat monitoring mode\n", __func__);
			return;
		}
	}

	if (charger->pdata->num_sec_mode <= charger->pdata->charging_mode) {
		pr_err("%s: no sec_mode configure data\n", __func__);
		return;
	}

	vout = charger->pdata->sec_mode_config_data[charger->pdata->charging_mode].vout;
	vrect = charger->pdata->sec_mode_config_data[charger->pdata->charging_mode].vrect;

	pr_info("%s: mode[%d] vrect(%d) vout(%d)\n", __func__, charger->pdata->charging_mode, vrect, vout);

	if (!vout)
		return;

	p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_TO_VAL(vout));
	msleep(200);
	p9220_reg_write(charger->client, P9220_VRECT_SET_REG, vrect);
}

static void p9220_wpc_mode_change(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_mode_work.work);

	wake_lock(&charger->wpc_wake_lock);
	p9220_sec_mode_change(charger);
	wake_unlock(&charger->wpc_wake_lock);
}

static void p9220_wpc_init(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_init_work.work);

	union power_supply_propval value1, value2;

	wake_lock(&charger->wpc_wake_lock);

	psy_do_property("wpc", get,
		POWER_SUPPLY_PROP_STATUS, value1);
	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_POWER_NOW, value2);
	pr_info("%s: wpc status val:%d acok:%d\n", __func__, value1.intval, value2.intval);
	if ((value1.intval == 1) && (value2.intval ==0)) {
		if (charger->pdata->can_vbat_monitoring) {
			/* VRECT TARGET VOLTAGE = 5449mV(0x0427) */
			p9220_reg_write(charger->client, P9220_VRECT_TARGET_VOL_L, 0X27);
			p9220_reg_write(charger->client, P9220_VRECT_TARGET_VOL_H, 0X04);
		} else {
			p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 126);
			p9220_reg_update(charger->client, 0x81, 0x80, 0x80); //set 7th bit in 0x81 to power on
		}
		p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_TOGGLE_LDO_MASK, P9220_CMD_TOGGLE_LDO_MASK);
	}

	wake_unlock(&charger->wpc_wake_lock);
}

static int p9220_temperature_check(struct p9220_charger_data *charger)
{
	if (charger->temperature >= charger->pdata->tx_off_high_temp) {
		/* send TX watchdog command in high temperature */
		pr_err("%s:HIGH TX OFF TEMP:%d\n", __func__, charger->temperature);
		p9220_reg_write(charger->client, P9220_PACKET_HEADER, 0x18);
		p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, 0xe7);
		p9220_reg_write(charger->client, P9220_COMMAND_REG, 0x01);
		p9220_reg_write(charger->client, P9220_PACKET_HEADER, 0x18);
		p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, 0xe7);
		p9220_reg_write(charger->client, P9220_COMMAND_REG, 0x01);
	}

	return 0;
}

static int p9220_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct p9220_charger_data *charger =
		container_of(psy, struct p9220_charger_data, psy_chg);
	int ret;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);
			pr_info("%s rx major firmware version 0x%x \n", __func__, ret);

			if (ret >= 0)
				val->intval = 1;
			else
				val->intval = 0;

			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
		case POWER_SUPPLY_PROP_HEALTH:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
			return -ENODATA;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
				break;
		case POWER_SUPPLY_PROP_ONLINE:
		case POWER_SUPPLY_PROP_PRESENT:
			pr_debug("%s cable_type =%d \n ", __func__, charger->pdata->cable_type);
			val->intval = charger->pdata->cable_type;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = charger->temperature;
			break;
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			val->intval = charger->pdata->charging_mode;
			break;	
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
		case POWER_SUPPLY_PROP_MANUFACTURER:
			pr_info("%s POWER_SUPPLY_PROP_MANUFACTURER =%d \n ", __func__, val->intval);
			val->intval = 0;
			break;
			if (val->intval == SEC_WIRELESS_OTP_FIRM_RESULT) {
				pr_info("%s otp firmware result = %d,\n",__func__, charger->pdata->otp_firmware_result);
				val->intval = charger->pdata->otp_firmware_result;
			} else if(val->intval == SEC_WIRELESS_IC_GRADE) {
				val->intval = charger->pdata->wc_ic_grade;
			} else if(val->intval == SEC_WIRELESS_IC_REVISION) {
				val->intval = charger->pdata->wc_ic_rev;
			} else if(val->intval == SEC_WIRELESS_OTP_FIRM_VER_BIN) {
				val->intval = P9220_OTP_FIRM_VERSION;
			} else if(val->intval == SEC_WIRELESS_OTP_FIRM_VER) {
				if (charger->pdata->ic_on_mode || charger->pdata->cable_type != SEC_WIRELESS_PAD_NONE)
					val->intval = p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);
				else
					val->intval = -1;
			} else if(val->intval == SEC_WIRELESS_TX_FIRM_RESULT) {
				val->intval = charger->pdata->tx_firmware_result;
			} else if (val->intval == SEC_WIRELESS_TX_FIRM_VER) {
				//if(charger->pdata->ic_on_mode)
					val->intval = charger->pdata->tx_firmware_ver;
				//else
				//	val->intval = -1;
			} else if(val->intval == SEC_TX_FIRMWARE) {
				val->intval = charger->pdata->tx_status;
			} else if(val->intval == SEC_WIRELESS_OTP_FIRM_VERIFY) {
				val->intval = p9220_firmware_verify(charger);
			} else{
				val->intval = -1;
				pr_err("%s wrong mode \n", __func__);
			}
			break;
#endif
		case POWER_SUPPLY_PROP_ENERGY_NOW: /* vout */
			pr_info("%s POWER_SUPPLY_PROP_ENERGY_NOW =%d\n",
				__func__, val->intval);
			val->intval = 0;
			break;
			if(charger->pdata->ic_on_mode || charger->pdata->is_charging) {
				val->intval = p9220_get_adc(charger, P9220_ADC_VOUT);
			} else
				val->intval = 0;
			break;

		case POWER_SUPPLY_PROP_ENERGY_AVG: /* vrect */
			pr_info("%s POWER_SUPPLY_PROP_ENERGY_AVG =%d\n",
				__func__, val->intval);
			val->intval = 0;
			break;
			if(charger->pdata->ic_on_mode || charger->pdata->is_charging) {
				val->intval = p9220_get_adc(charger, P9220_ADC_VRECT);
			} else
				val->intval = 0;
			break;
		default:
			pr_debug("%s default================== =%d\n",
				__func__, val->intval);
			return -EINVAL;
	}
	return 0;
}

static int p9220_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct p9220_charger_data *charger =
		container_of(psy, struct p9220_charger_data, psy_chg);
	union power_supply_propval value1, value2;

	pr_info("%s: psp=%d val=%d\n", __func__, psp, val->intval);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			if(val->intval == POWER_SUPPLY_STATUS_FULL) {
				pr_info("%s set green led \n", __func__);
				charger->pdata->cs100_status = p9220_send_cs100(charger);
				charger->pdata->cs100_status = p9220_send_cs100(charger);
				/*p9220_reg_write(charger->client, P9220_END_POWER_TRANSFER_REG, P9220_EPT_END_OF_CHG);
				p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_SEND_EOP_MASK, P9220_CMD_SEND_EOP_MASK);*/
			} else if(val->intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
				p9220_mis_align(charger);
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			if(gpio_get_value(charger->pdata->wpc_det)) {
				msleep(250);
				pr_info("%s: Charger interrupt occured during lpm \n", __func__);
				queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);

				/* this code is for restart on wirless pad */
				p9220_send_command(charger, P9220_REQUEST_AFC_TX);
			}
			queue_delayed_work(charger->wqueue, &charger->wpc_lpm_work, msecs_to_jiffies(2000));
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			if(val->intval == POWER_SUPPLY_HEALTH_OVERHEAT ||
				val->intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT ||
				val->intval == POWER_SUPPLY_HEALTH_COLD) {
				pr_info("%s ept-ot \n", __func__);
				p9220_send_eop(charger, val->intval);
			}
			break;
		case POWER_SUPPLY_PROP_TEMP:
			charger->temperature = val->intval;
			p9220_temperature_check(charger);
			break;
		case POWER_SUPPLY_PROP_ONLINE:
#if 0
			if(val->intval == POWER_SUPPLY_TYPE_WIRELESS ||
				val->intval == POWER_SUPPLY_TYPE_HV_WIRELESS ||
				val->intval == POWER_SUPPLY_TYPE_PMA_WIRELESS ) {
				charger->pdata->ic_on_mode = true;
			} else {
				charger->pdata->ic_on_mode = false;
			}
#endif
			if (val->intval == POWER_SUPPLY_TYPE_WPC && !charger->pdata->can_vbat_monitoring) {
				p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 126);
				p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_TO_VAL(4900));
				schedule_delayed_work(&charger->wpc_mode_work, msecs_to_jiffies(5000));
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			charger->pdata->siop_level = val->intval;
			psy_do_property("battery", get,
				POWER_SUPPLY_PROP_STATUS, value1);
			psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CAPACITY, value2);

			if(charger->pdata->siop_level == 100 &&
				(value1.intval == POWER_SUPPLY_STATUS_FULL ||
				value2.intval >= 99) ) {
				pr_info("%s vrect headroom set ROOM 2, siop = %d \n", __func__, charger->pdata->siop_level);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_2);
			} else if(charger->pdata->siop_level < 100){
				pr_info("%s vrect headroom set ROOM 0, siop = %d \n", __func__, charger->pdata->siop_level);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_0);
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			if(val->intval) {
				charger->pdata->ic_on_mode = true;
			} else {
				charger->pdata->ic_on_mode = false;
			}
			break;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
		case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
			p9220_firmware_update(charger, val->intval);
			pr_info("%s rx result = %d, tx result = %d \n",__func__,
					charger->pdata->otp_firmware_result,charger->pdata->tx_firmware_result);
			break;
#endif
#if defined(CONFIG_WIRELESS_CHARGER_HIGH_VOLTAGE)
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			if (val->intval == WIRELESS_VOUT_NORMAL_VOLTAGE) {
				pr_info("%s: Wireless Vout forced set to 5V. + PAD CMD\n",__func__);
				for(int i = 0; i < CMD_CNT; i++) {
					p9220_send_command(charger, P9220_AFC_CONF_5V);
					msleep(250);
				}
			} else if (val->intval == WIRELESS_VOUT_HIGH_VOLTAGE) {
				pr_info("%s: Wireless Vout forced set to 9V. + PAD CMD\n",__func__);
				for(int i = 0; i < CMD_CNT; i++) {
					p9220_send_command(charger, P9220_AFC_CONF_9V);
					msleep(250);
				}
			} else if (val->intval == WIRELESS_VOUT_CV_CALL) {
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_3);
				msleep(500);
				p9220_set_vout(charger, P9220_VOUT_CV_CALL);
				msleep(500);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_0);
				pr_info("%s: Wireless Vout forced set to %dmV\n",
								__func__, charger->pdata->wpc_cv_call_vout);
			} else if (val->intval == WIRELESS_VOUT_CC_CALL) {
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_3);
				msleep(500);
				p9220_set_vout(charger, P9220_VOUT_CC_CALL);
				msleep(500);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_0);
				pr_info("%s: Wireless Vout forced set to %dmV\n",
								__func__, charger->pdata->wpc_cc_call_vout);
			} else if (val->intval == WIRELESS_VOUT_5V) {
				p9220_set_vout(charger, P9220_VOUT_5V);
				pr_info("%s: Wireless Vout forced set to 5V\n", __func__);
			} else if (val->intval == WIRELESS_VOUT_9V) {
				int prev_current;
				union power_supply_propval value;
				psy_do_property("max77833-charger", get,
						POWER_SUPPLY_PROP_CURRENT_AVG, value);
				prev_current = value.intval;
				value.intval = 200;
				psy_do_property("max77833-charger", set,
						POWER_SUPPLY_PROP_CURRENT_MAX, value);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_1);
				msleep(1000);
				p9220_set_vout(charger, P9220_VOUT_9V);
				msleep(1000);
				value.intval = prev_current;
				psy_do_property("max77833-charger", set,
						POWER_SUPPLY_PROP_CURRENT_MAX, value);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_0);
				pr_info("%s: Wireless Vout forced set to 9V\n", __func__);
			} else if (val->intval == WIRELESS_PAD_FAN_OFF) {
				pr_info("%s: fan off \n",__func__);
				p9220_fan_control(charger, 0);
			} else if (val->intval == WIRELESS_PAD_FAN_ON) {
				pr_info("%s: fan on \n",__func__);
				p9220_fan_control(charger, 1);
			} else if (val->intval == WIRELESS_PAD_LED_OFF) {
				pr_info("%s: led off \n",__func__);
				p9220_led_control(charger, 0);
			} else if (val->intval == WIRELESS_PAD_LED_ON) {
				pr_info("%s: led on \n",__func__);
				p9220_led_control(charger, 1);
			} else if(val->intval == WIRELESS_VRECT_ADJ_ON) {
				pr_info("%s: vrect adjust to have big headroom(defualt value) \n",__func__);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_1);
			} else if(val->intval == WIRELESS_VRECT_ADJ_OFF) {
				pr_info("%s: vrect adjust to have small headroom \n",__func__);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_0);
			}  else if(val->intval == WIRELESS_VRECT_ADJ_ROOM_0) {
				pr_info("%s: vrect adjust to have headroom 0 \n",__func__);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_0);
			}  else if(val->intval == WIRELESS_VRECT_ADJ_ROOM_1) {
				pr_info("%s: vrect adjust to have headroom 1 \n",__func__);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_1);
			}  else if(val->intval == WIRELESS_VRECT_ADJ_ROOM_2) {
				pr_info("%s: vrect adjust to have headroom 2 \n",__func__);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_2);
			}  else if(val->intval == WIRELESS_VRECT_ADJ_ROOM_3) {
				pr_info("%s: vrect adjust to have headroom 3 \n",__func__);
				p9220_set_vrect_adjust(charger, P9220_HEADROOM_3);
			} else {
				pr_info("%s: Unknown Command(%d)\n",__func__, val->intval);
			}
			break;
#else
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			/* adjust vrect headroom */
			if (batt_booting_chk) {
#if 0
				if (val->intval == charger->pdata->charging_mode) {
					pr_info("%s: mode is not changed\n", __func__);
				} else
#endif
				{
					unsigned int prev_charging_mode = charger->pdata->charging_mode;

					pr_info("%s: new mode=%d, prev mode=%d\n", __func__, val->intval,
						charger->pdata->charging_mode);
					switch (val->intval) {
						case SEC_INITIAL_MODE:
							charger->pdata->charging_mode = SEC_INITIAL_MODE;
							break;
						case SEC_SWELLING_MODE:
							charger->pdata->charging_mode = SEC_SWELLING_MODE;
							break;
						case SEC_LOW_BATT_MODE:
							if (charger->pdata->charging_mode != SEC_MID_BATT_MODE) {
								charger->pdata->charging_mode = SEC_LOW_BATT_MODE;
							}
							break;
						case SEC_MID_BATT_MODE:
							if (charger->pdata->charging_mode != SEC_HIGH_BATT_MODE) {
								charger->pdata->charging_mode = SEC_MID_BATT_MODE;
							}
							break;
						case SEC_HIGH_BATT_MODE:
							if (charger->pdata->charging_mode != SEC_VERY_HIGH_BATT_MODE) {
								charger->pdata->charging_mode = SEC_HIGH_BATT_MODE;
							}
							break;
						case SEC_VERY_HIGH_BATT_MODE:
							charger->pdata->charging_mode = SEC_VERY_HIGH_BATT_MODE;
							break;
						case SEC_FULL_NONE_MODE:
							charger->pdata->charging_mode = SEC_FULL_NONE_MODE;
							break;
						case SEC_FULL_NORMAL_MODE:
							charger->pdata->charging_mode = SEC_FULL_NORMAL_MODE;
							break;
						case SEC_SWELLING_LDU_MODE:
							charger->pdata->charging_mode = SEC_SWELLING_LDU_MODE;
							break;
						case SEC_LOW_BATT_LDU_MODE:
							charger->pdata->charging_mode = SEC_LOW_BATT_LDU_MODE;
							break;
						case SEC_FULL_NONE_LDU_MODE:
							charger->pdata->charging_mode = SEC_FULL_NONE_LDU_MODE;
							break;
						case SEC_VBAT_MONITORING_DISABLE_MODE:
							charger->pdata->charging_mode = SEC_VBAT_MONITORING_DISABLE_MODE;
							break;
						case SEC_FULL_NORMAL_LDU_MODE:
							charger->pdata->charging_mode = SEC_FULL_NORMAL_LDU_MODE;
							break;
						case SEC_INCOMPATIBLE_TX_MODE:
							charger->pdata->charging_mode = SEC_INCOMPATIBLE_TX_MODE;
							break;
						case SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE:
							charger->pdata->charging_mode = SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE;
							break;
						case SEC_INCOMPATIBLE_TX_FULL_MODE:
							charger->pdata->charging_mode = SEC_INCOMPATIBLE_TX_FULL_MODE;
							break;
						default:
							pr_info("%s: invalid parameter(%d)\n", __func__, val->intval);
							break;
					}
					if (charger->pdata->charging_mode != prev_charging_mode) {
						if (charger->pdata->can_vbat_monitoring) {
							if (charger->pdata->charging_mode == SEC_FULL_NONE_LDU_MODE ||
									charger->pdata->charging_mode == SEC_LOW_BATT_LDU_MODE) {
								p9220_reg_write(charger->client, P9220_VOUT_LOWER_THRESHOLD, VOUT_LOWER_4_2V);
								msleep(200);
								/* Margin between Vbattery - Vout = 400mV(0x0190) */
								p9220_reg_write(charger->client, P9220_BATT_VOLTAGE_HEADROOM_L, 0x90);
								p9220_reg_write(charger->client, P9220_BATT_VOLTAGE_HEADROOM_H, 0x01);
								msleep(200);
							} else if (charger->pdata->charging_mode == SEC_FULL_NORMAL_LDU_MODE) {
								p9220_reg_write(charger->client, P9220_VOUT_LOWER_THRESHOLD, VOUT_LOWER_4_2V);
								msleep(200);
								/* Margin between Vbattery - Vout = 500mV(0x01f4) */
								p9220_reg_write(charger->client, P9220_BATT_VOLTAGE_HEADROOM_L, 0xf4);
								p9220_reg_write(charger->client, P9220_BATT_VOLTAGE_HEADROOM_H, 0x01);
								msleep(200);
							} else if (charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_MODE ||
											charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE ||
											charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_FULL_MODE) {
								p9220_reg_write(charger->client, P9220_VOUT_LOWER_THRESHOLD, VOUT_LOWER_4_7V);
								msleep(200);
							} else {
								p9220_reg_write(charger->client, P9220_VOUT_LOWER_THRESHOLD, VOUT_LOWER_4_6V);
								msleep(200);
							}
							if (charger->pdata->charging_mode == SEC_SWELLING_MODE ||
									charger->pdata->charging_mode == SEC_SWELLING_LDU_MODE ||
									charger->pdata->charging_mode == SEC_VBAT_MONITORING_DISABLE_MODE ||
									charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE ||
									charger->pdata->charging_mode == SEC_INCOMPATIBLE_TX_FULL_MODE) {
								p9220_reg_update(charger->client, P9220_WPC_FLAG_REG, 0x00, P9220_VBAT_MONITORING_MODE_MASK);
								msleep(200);
								cancel_delayed_work(&charger->wpc_mode_work);
								p9220_sec_mode_change(charger);
								msleep(2000);
							}
							else if (prev_charging_mode == SEC_SWELLING_MODE ||
										prev_charging_mode == SEC_SWELLING_LDU_MODE ||
										prev_charging_mode == SEC_VBAT_MONITORING_DISABLE_MODE ||
										prev_charging_mode == SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE ||
										prev_charging_mode == SEC_INCOMPATIBLE_TX_FULL_MODE) {
								wake_lock_timeout(&charger->wpc_vbat_monitoring_enable_lock, HZ * 6);
								schedule_delayed_work(&charger->wpc_mode_work, msecs_to_jiffies(5000));
							} else {
								p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 9);
								p9220_reg_update(charger->client, P9220_WPC_FLAG_REG, P9220_VBAT_MONITORING_MODE, P9220_VBAT_MONITORING_MODE_MASK);
							}
						} else {
							cancel_delayed_work(&charger->wpc_mode_work);
							p9220_sec_mode_change(charger);
						}
					}
				}
			} else {
				if (!charger->pdata->can_vbat_monitoring)
					p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 126);
			}
			if (!lpcharge)
				p9220_reg_write(charger->client, 0x78, 0x0);
			break;
#endif
		case POWER_SUPPLY_PROP_PRESENT:
			switch(val->intval) {
				case P9220_WATCHDOG:
					if (charger->pdata->watchdog_test == false) {
						p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_SS_WATCHDOG_MASK, P9220_CMD_SS_WATCHDOG_MASK);
						p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_SS_WATCHDOG_MASK, P9220_CMD_SS_WATCHDOG_MASK);
						p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_SS_WATCHDOG_MASK, P9220_CMD_SS_WATCHDOG_MASK);
						//p9220_wpc_init(charger);
					}
					break;
				case P9220_EOP_HIGH_TEMP:
					p9220_reg_write(charger->client, P9220_PACKET_HEADER, 0x18);
					p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, 0xe7);
					p9220_reg_write(charger->client, P9220_COMMAND_REG, 0x01);
					p9220_reg_write(charger->client, P9220_PACKET_HEADER, 0x18);
					p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, 0xe7);
					p9220_reg_write(charger->client, P9220_COMMAND_REG, 0x01);
					break;
				case P9220_VOUT_OFF:
					psy_do_property("sec-charger", get,
						POWER_SUPPLY_PROP_POWER_NOW, value1);
					if (value1.intval == 1) {
						msleep(2000);
						p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_TOGGLE_LDO_MASK, P9220_CMD_TOGGLE_LDO_MASK);
						if (charger->pdata->can_vbat_monitoring) {
							/* VRECT TARGET VOLTAGE = 4839mV(0x03B0) */
							p9220_reg_write(charger->client, P9220_VRECT_TARGET_VOL_L, 0XB0);
							p9220_reg_write(charger->client, P9220_VRECT_TARGET_VOL_H, 0X03);
						} else {
							msleep(2000);
							p9220_reg_update(charger->client, 0x81, 0x0, 0x80); //clear 7th bit in 0x81 to reduce power
							msleep(2000);
							p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 8); //default vrect headroom
						}
					}
					break;
				case P9220_VOUT_ON:
					psy_do_property("sec-charger", get,
						POWER_SUPPLY_PROP_POWER_NOW, value1);
					if (value1.intval == 0) {
						if (charger->pdata->can_vbat_monitoring) {
							/* VRECT TARGET VOLTAGE = 5449mV(0x0427) */
							p9220_reg_write(charger->client, P9220_VRECT_TARGET_VOL_L, 0X27);
							p9220_reg_write(charger->client, P9220_VRECT_TARGET_VOL_H, 0X04);
						} else {
							p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 126);
							msleep(2000);
							p9220_reg_update(charger->client, 0x81, 0x80, 0x80); //set 7th bit in 0x81 to power on
						}
						msleep(3000);
						p9220_reg_update(charger->client, P9220_COMMAND_REG, P9220_CMD_TOGGLE_LDO_MASK, P9220_CMD_TOGGLE_LDO_MASK);
						msleep(2000);
						charger->pdata->charging_mode = SEC_INITIAL_MODE;
					}
					break;
				default:
					break;
 			}
			break;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
		case POWER_SUPPLY_PROP_MANUFACTURER:
			charger->pdata->otp_firmware_result = val->intval;
			pr_info("%s otp_firmware result initialize (%d)\n",__func__,
					charger->pdata->otp_firmware_result);
			break;
#endif
		default:
			return -EINVAL;
	}

	return 0;
}


#define FREQ_OFFSET	384000 /* 64*6000 */

static void p9220_wpc_opfq_work(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_opfq_work.work);

	u16 op_fq;
	u8 pad_mode;
	union power_supply_propval value;

	p9220_reg_read(charger->client, P9220_SYS_OP_MODE_REG, &pad_mode);
	if (pad_mode == P9220_PAD_MODE_WPC) {
			op_fq = FREQ_OFFSET / p9220_get_adc(charger, P9220_ADC_OP_FRQ);
			pr_info("%s: Operating FQ %dkHz(0x%x)\n", __func__, op_fq, op_fq);
			if (op_fq > 230) { /* wpc threshold 230kHz */
				pr_info("%s: Reset M0\n",__func__);
				p9220_reg_write(charger->client, 0x3040, 0x80); /*restart M0 */

				charger->pdata->opfq_cnt++;
				if (charger->pdata->opfq_cnt <= CMD_CNT) {
					queue_delayed_work(charger->wqueue, &charger->wpc_opfq_work, msecs_to_jiffies(10000));
					return;
				}
			}
	} else if (pad_mode == P9220_PAD_MODE_PMA) {
			charger->pdata->cable_type = P9220_PAD_MODE_PMA;
			value.intval = SEC_WIRELESS_PAD_PMA;
			psy_do_property("wireless", set, POWER_SUPPLY_PROP_ONLINE, value);
	}
	charger->pdata->opfq_cnt = 0;
	wake_unlock(&charger->wpc_opfq_lock);

}

static void p9220_wpc_det_work(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_det_work.work);
	int wc_w_state;
	union power_supply_propval value;
	u8 pad_mode;
	u8 vrect;

	wake_lock(&charger->wpc_wake_lock);
	pr_info("%s\n",__func__);
	wc_w_state = gpio_get_value(charger->pdata->wpc_det);

	if ((charger->wc_w_state == 0) && (wc_w_state == 1)) {
		/* this code is only for Zero2, There is zinitix leakage . should turn on before charging on */
		//if(charger->pdata->on_mst_wa)
		//	wireless_of_mst_hw_onoff(1);

		charger->pdata->vout_status = P9220_VOUT_5V;

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
		/* read firmware version */
		if(p9220_get_firmware_version(charger, P9220_RX_FIRMWARE) == P9220_OTP_FIRM_VERSION && adc_cal > 0)
			p9220_runtime_sram_change(charger);/* change sram */
#endif
		/* set fod value */
		if(charger->pdata->fod_data_check)
			p9220_fod_set(charger);

		/* enable Mode Change INT */
		p9220_reg_update(charger->client, P9220_INT_ENABLE_L_REG,
						P9220_STAT_MODE_CHANGE_MASK, P9220_STAT_MODE_CHANGE_MASK);

		if(!lpcharge) {
			value.intval = DISABLE;
			psy_do_property("max77833-charger", set, POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL, value);
		}
		value.intval = false;
		psy_do_property("max77833-charger", set,
				POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);

		value.intval = 1;
		psy_do_property("max77833-charger", set, POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW, value);

		/* read vrect adjust */
		p9220_reg_read(charger->client, P9220_VRECT_SET_REG, &vrect);

		pr_info("%s: wpc activated, set V_INT as PN\n",__func__);

		/* read pad mode */
		p9220_reg_read(charger->client, P9220_SYS_OP_MODE_REG, &pad_mode);
		if(pad_mode == P9220_SYS_MODE_PMA) {
			charger->pdata->cable_type = P9220_PAD_MODE_PMA;
			value.intval = SEC_WIRELESS_PAD_PMA;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		} else {
			charger->pdata->cable_type = P9220_PAD_MODE_WPC;
			value.intval = SEC_WIRELESS_PAD_WPC;
			psy_do_property("wireless", set,
					POWER_SUPPLY_PROP_ONLINE, value);
			wake_lock(&charger->wpc_opfq_lock);
			queue_delayed_work(charger->wqueue, &charger->wpc_opfq_work, msecs_to_jiffies(10000));
		}

		/* set request afc_tx */
		p9220_send_command(charger, P9220_REQUEST_AFC_TX);

		charger->pdata->is_charging = 1;
	} else if ((charger->wc_w_state == 1) && (wc_w_state == 0)) {

		charger->pdata->cable_type = P9220_PAD_MODE_NONE;
		charger->pdata->is_charging = 0;
		charger->pdata->vout_status = P9220_VOUT_0V;
		charger->pdata->opfq_cnt = 0;

		value.intval = SEC_WIRELESS_PAD_NONE;
		psy_do_property("wireless", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc deactivated, set V_INT as PD\n",__func__);

		msleep(1000);

		/* if vout <= 3000mV, restart M0 for misalign protect */ 
		if (p9220_get_adc(charger, P9220_ADC_VOUT) <= 3000) {
			pr_err("%s Restart M0, wireless detached! \n", __func__);
			p9220_reg_write(charger->client, 0x3040, 0x80); /*restart M0 */
		}

		if(!lpcharge) {
			value.intval = ENABLE;
			psy_do_property("max77833-charger", set, POWER_SUPPLY_PROP_CHARGE_AICL_CONTROL, value);
		}

		value.intval = 0;
		psy_do_property("max77833-charger", set, POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW, value);
		cancel_delayed_work(&charger->wpc_isr_work);

		if(delayed_work_pending(&charger->wpc_opfq_work)) {
			wake_unlock(&charger->wpc_opfq_lock);
			cancel_delayed_work(&charger->wpc_opfq_work);
		}

		/* this code is only for Zero2, There is zinitix leakage . should turn on before charging on */
		//if(charger->pdata->on_mst_wa)
		//	wireless_of_mst_hw_onoff(0);
	}

	pr_info("%s: w(%d to %d)\n", __func__,
		charger->wc_w_state, wc_w_state);

	charger->wc_w_state = wc_w_state;
	wake_unlock(&charger->wpc_wake_lock);
}

static enum alarmtimer_restart p9220_wpc_ph_mode_alarm(struct alarm *alarm, ktime_t now)
{
	struct p9220_charger_data *charger = container_of(alarm,
				struct p9220_charger_data, polling_alarm);

	union power_supply_propval value;

	value.intval = false;
	psy_do_property("sec-charger", set,
		POWER_SUPPLY_PROP_PRESENT, value);
	pr_info("%s: wpc ph mode ends.\n", __func__);
	wake_unlock(&charger->wpc_wake_lock);

	return ALARMTIMER_NORESTART;
}

static void p9220_wpc_init_detect(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_init_detect_work.work);
	int ret;

	wake_lock(&charger->wpc_wake_lock);

	/* check again firmare version support vbat monitoring */
	if (!charger->pdata->otp_firmware_ver) {
		ret = p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);
		if (ret > 0) {
			pr_info("%s rx major firmware version 0x%x\n", __func__, ret);
			charger->pdata->otp_firmware_ver = ret;
			if (charger->pdata->otp_firmware_ver == 0x5053)
				charger->pdata->can_vbat_monitoring = true;
		}
	}

	schedule_delayed_work(&charger->wpc_isr_work, 0);

	ret = enable_irq_wake(charger->client->irq);
	if (ret < 0)
		pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
			__func__, ret);

	wake_unlock(&charger->wpc_wake_lock);
}

static void p9220_wpc_isr_work(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_isr_work.work);

	u8 data;
	int ret;
	union power_supply_propval value;
	int i;

	wake_lock(&charger->wpc_wake_lock);
	pr_info("%s\n",__func__);

	ret = p9220_reg_read(charger->client, P9220_TX_DATA_COMMAND, &data);
	if(ret >= 0) {
#if defined(CONFIG_WPC_PH_MODE)
		if (data == 0x18) {
			alarm_cancel(&charger->polling_alarm);
			value.intval = true;
			psy_do_property("sec-charger", set,
				POWER_SUPPLY_PROP_PRESENT, value);
			if (charger->pdata->charging_mode == SEC_FULL_NONE_MODE) {
				p9220_send_php(charger);
			}
		} else {
			alarm_cancel(&charger->polling_alarm);
		}
#endif
		if (charger->pdata->d_tx_type == TX_TYPE_POP && data == P9220_TX_DATA_COM_TX_ID) {
			ret = p9220_reg_read(charger->client, P9220_TX_DATA_VALUE0, &data);
			if (ret > 0) {
				if (data == P9220_TX_ID_POP) {
					pr_info("%s data = 0x%x, this is Gear POP TX pad \n", __func__, data);
					value.intval = 1;
					psy_do_property("sec-charger", set,
						POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);
					psy_do_property("battery", set,
						POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);
				}
			}
			p9220_reg_read(charger->client, P9220_INT_L_REG, &data);
			if (data & P9220_STAT_TX_DATA_RECEIVED_MASK)
				p9220_reg_write(charger->client, P9220_INT_CLEAR_L_REG, P9220_INT_TX_DATA_RECEIVED);
			pr_info("%s requst TX-ID to TX\n", __func__);
			/* request "Request TX-ID" to TX */
			for (i = 0; i < 3; i++) {
				p9220_reg_write(charger->client, P9220_PACKET_HEADER, P9220_HEADER_AFC_CONF); // using 2 bytes packet property
				p9220_reg_write(charger->client, P9220_RX_DATA_COMMAND, P9220_RX_DATA_COM_REQ_TX_ID);
				p9220_reg_write(charger->client, P9220_RX_DATA_VALUE0, 0x0);
				p9220_reg_write(charger->client, P9220_COMMAND_REG, P9220_CMD_SEND_RX_DATA);
				msleep(300);
			}
		}
		if(charger->pdata->d_tx_type == TX_TYPE_S3 && data == 0x02) {
			ret = p9220_reg_read(charger->client, P9220_TX_DATA_VALUE0, &data);
			if(ret > 0) {
				if (data == 0x99) {
					pr_info("%s data = 0x%x, this is Gear S3 TX pad \n", __func__, data);
					value.intval = 1;
					psy_do_property("sec-charger", set,
						POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);
					psy_do_property("battery", set,
						POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);
#if defined(CONFIG_WPC_PH_MODE)
					if ((charger->pdata->charging_mode == SEC_FULL_NONE_MODE) ||
						(charger->pdata->charging_mode == SEC_FULL_NORMAL_MODE)) {
							value.intval = POWER_SUPPLY_STATUS_CHARGING;
							psy_do_property("battery", set,
								POWER_SUPPLY_PROP_STATUS, value);
					}
#endif
				}
			}
		}
	}
	wake_unlock(&charger->wpc_wake_lock);
}

static irqreturn_t p9220_wpc_det_irq_thread(int irq, void *irq_data)
{
	struct p9220_charger_data *charger = irq_data;

	pr_info("%s !\n",__func__);

	queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);

	return IRQ_HANDLED;
}

static irqreturn_t p9220_wpc_irq_thread(int irq, void *irq_data)
{
	struct p9220_charger_data *charger = irq_data;
	int ret;
	u8 irq_src[2];
	u8 reg_data;

	wake_lock(&charger->wpc_wake_lock);

	/* check again firmare version support vbat monitoring */
	if (!charger->pdata->otp_firmware_ver) {
		ret = p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);
		if (ret > 0) {
			pr_info("%s rx major firmware version 0x%x\n", __func__, ret);
			charger->pdata->otp_firmware_ver = ret;
			if (charger->pdata->otp_firmware_ver == 0x5053)
				charger->pdata->can_vbat_monitoring = true;
		}
	}

	if (charger->pdata->can_vbat_monitoring) {
		p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 9);
		p9220_reg_update(charger->client, P9220_WPC_FLAG_REG, P9220_VBAT_MONITORING_MODE, P9220_VBAT_MONITORING_MODE_MASK);
	}

	ret = p9220_reg_read(charger->client, P9220_INT_L_REG, &irq_src[0]);
	ret = p9220_reg_read(charger->client, P9220_INT_H_REG, &irq_src[1]);

	if (ret < 0) {
		pr_err("%s: Failed to read interrupt source: %d\n",
			__func__, ret);
		wake_unlock(&charger->wpc_wake_lock);
		return IRQ_NONE;
	}

	pr_info("%s: interrupt source(0x%x)\n", __func__, irq_src[1] << 8 | irq_src[0]);

	if(irq_src[0] & P9220_STAT_MODE_CHANGE_MASK) {
		pr_info("%s MODE CHANGE IRQ ! \n", __func__);
		ret = p9220_reg_read(charger->client, P9220_SYS_OP_MODE_REG, &reg_data);
	}

	if(irq_src[0] & P9220_STAT_TX_DATA_RECEIVED_MASK) {
		pr_info("%s TX RECIVED IRQ ! \n", __func__);
		if(!delayed_work_pending(&charger->wpc_isr_work) &&
			charger->pdata->cable_type != P9220_PAD_MODE_WPC_AFC )
			schedule_delayed_work(&charger->wpc_isr_work, msecs_to_jiffies(1000));
	}

	if(irq_src[1] & P9220_STAT_OVER_CURR_MASK) {
		pr_info("%s OVER CURRENT IRQ ! \n", __func__);
	}

	if(irq_src[1] & P9220_STAT_OVER_TEMP_MASK) {
		pr_info("%s OVER TEMP IRQ ! \n", __func__);
	}

	if(irq_src[1] & P9220_STAT_TX_CONNECT_MASK) {
		pr_info("%s TX CONNECT IRQ ! \n", __func__);
		charger->pdata->tx_status = SEC_TX_POWER_TRANSFER;
	}

	if ((irq_src[1] << 8 | irq_src[0]) != 0) {
		msleep(5);

		/* clear intterupt */
		p9220_reg_write(charger->client, P9220_INT_CLEAR_L_REG, irq_src[0]); // clear int
		p9220_reg_write(charger->client, P9220_INT_CLEAR_H_REG, irq_src[1]); // clear int
		p9220_set_cmd_reg(charger, 0x20, P9220_CMD_CLEAR_INT_MASK); // command
	}
	/* debug */
	wake_unlock(&charger->wpc_wake_lock);

	return IRQ_HANDLED;
}


static void p9220_wpc_lpm_work(struct work_struct *work)
{
	struct p9220_charger_data *charger =
		container_of(work, struct p9220_charger_data, wpc_lpm_work.work);
	int ret = 0;

	wake_lock(&charger->wpc_wake_lock);
	pr_info("%s\n",__func__);

	/* wpc_det */
	if (charger->pdata->irq_wpc_det) {
		ret = request_threaded_irq(charger->pdata->irq_wpc_det,
					NULL, p9220_wpc_det_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
					IRQF_ONESHOT,
					"wpd-det-irq", charger);
			if (ret)
				pr_err("%s: Failed to Reqeust IRQ\n", __func__);
	}

	/* wpc_irq */
	if (charger->pdata->irq_wpc_int) {
		msleep(100);
		ret = request_threaded_irq(charger->pdata->irq_wpc_int,
				NULL, p9220_wpc_irq_thread,
				IRQF_TRIGGER_LOW |
				IRQF_ONESHOT,
				"wpc-irq", charger);
		if (ret)
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
	}
	wake_unlock(&charger->wpc_wake_lock);

	pr_info("%s wc_w_state_irq = %d\n", __func__, gpio_get_value(charger->pdata->wpc_int));
}


#if 0
static ssize_t p9220_store_addr(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct p9220_charger_data *charger = dev_get_drvdata(dev);

	int x;
	if (sscanf(buf, "0x%x\n", &x) == 1) {
		charger->addr = x;
	}
	return count;
}

static ssize_t p9220_show_addr(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct p9220_charger_data *charger = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", charger->addr);
}

static ssize_t p9220_store_size(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct p9220_charger_data *charger = dev_get_drvdata(dev);

	int x;
	if (sscanf(buf, "%d\n", &x) == 1) {
		charger->size = x;
	}
	return count;
}

static ssize_t p9220_show_size(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct p9220_charger_data *charger = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", charger->size);
}
static ssize_t p9220_store_data(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct p9220_charger_data *charger = dev_get_drvdata(dev);

	int x;

	if (sscanf(buf, "0x%x", &x) == 1) {
		u8 data = x;
		if (p9220_reg_write(charger->client, charger->addr, data) < 0)
		{
			dev_info(charger->dev,
					"%s: addr: 0x%x write fail\n", __func__, charger->addr);
		}
	}
	return count;
}

static ssize_t p9220_show_data(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct p9220_charger_data *charger = dev_get_drvdata(dev);

	u8 data;
	int i, count = 0;;
	if (charger->size == 0)
		charger->size = 1;

	for (i = 0; i < charger->size; i++) {
		if (p9220_reg_read(charger->client, charger->addr+i, &data) < 0) {
			dev_info(charger->dev,
					"%s: read fail\n", __func__);
			count += sprintf(buf+count, "addr: 0x%x read fail\n", charger->addr+i);
			continue;
		}
		count += sprintf(buf+count, "addr: 0x%x, data: 0x%x\n", charger->addr+i,data);
	}
	return count;
}
#endif

enum {
	WPC_FW_VER = 0,
	WPC_IBUCK,
	WPC_WATCHDOG,
	WPC_ADDR,
	WPC_SIZE,
	WPC_DATA,
};


static struct device_attribute p9220_attributes[] = {
	SEC_WPC_ATTR(fw),
	SEC_WPC_ATTR(ibuck),
	SEC_WPC_ATTR(watchdog_test),
	SEC_WPC_ATTR(addr),
	SEC_WPC_ATTR(size),
	SEC_WPC_ATTR(data),
};

ssize_t sec_wpc_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct p9220_charger_data *charger =
		container_of(psy, struct p9220_charger_data, psy_chg);

	const ptrdiff_t offset = attr - p9220_attributes;
	u8 data;
	int i, count = 0;

	switch (offset) {
		case WPC_FW_VER:
			{
				int ret =0;
				int version =0;

				ret = p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);

				if (ret >= 0) {
					version =  ret;
				}
				pr_info("%s rx major firmware version 0x%x \n", __func__, version);

				count += sprintf(buf+count, "%x\n", version);
			}
			break;
		case WPC_IBUCK:
			{
				int ret =0;
				int ibuck =0;
				u8 raw_iout[2] = {0,};

				ret = p9220_reg_read(charger->client, P9220_RAW_IOUT_L_REG, &raw_iout[0]);
				ret = p9220_reg_read(charger->client, P9220_RAW_IOUT_H_REG, &raw_iout[1]);
				if (ret >= 0) {
					ibuck =  raw_iout[0] | (raw_iout[1] << 8);
				}
				pr_info("%s raw iout %d \n", __func__, ibuck);

				count += sprintf(buf+count, "%d\n", ibuck-120);
			}
			break;
		case WPC_WATCHDOG:
			{
				pr_info("%s: watchdog test [%d] \n", __func__, charger->pdata->watchdog_test);
				count += sprintf(buf+count, "%d\n", charger->pdata->watchdog_test);
			}
			break;
		case WPC_ADDR:
			count += sprintf(buf+count, "0x%x\n", charger->addr);
			break;
		case WPC_SIZE:
			count += sprintf(buf+count, "0x%x\n", charger->size);
			break;
		case WPC_DATA:
			if (charger->size == 0)
				charger->size = 1;

			for (i = 0; i < charger->size; i++) {
				if (p9220_reg_read(charger->client, charger->addr+i, &data) < 0) {
					dev_info(charger->dev,
							"%s: read fail\n", __func__);
					count += sprintf(buf+count, "addr: 0x%x read fail\n", charger->addr+i);
					continue;
				}
				count += sprintf(buf+count, "addr: 0x%x, data: 0x%x\n", charger->addr+i,data);
			}
			break;
		default:
			break;
		}
	return count;
}	

ssize_t sec_wpc_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct p9220_charger_data *charger =
		container_of(psy, struct p9220_charger_data, psy_chg);
	const ptrdiff_t offset = attr - p9220_attributes;
	int x, ret;

	switch (offset) {
		case WPC_WATCHDOG:
			sscanf(buf, "%d\n", &x);
			if (x == 1)
				charger->pdata->watchdog_test = true;
			else if (x == 0)
				charger->pdata->watchdog_test = false;
			else
				pr_debug("%s: non valid input\n", __func__);
			pr_info("%s: watchdog test is set to %d\n", __func__, charger->pdata->watchdog_test);
			ret = count;
			break;
		case WPC_ADDR:
			if (sscanf(buf, "0x%x\n", &x) == 1) {
				charger->addr = x;
			}
			ret = count;
			break;
		case WPC_SIZE:
			if (sscanf(buf, "%d\n", &x) == 1) {
				charger->size = x;
			}
			ret = count;
			break;
		case WPC_DATA:
			if (sscanf(buf, "0x%x", &x) == 1) {
				u8 data = x;
				if (p9220_reg_write(charger->client, charger->addr, data) < 0)
				{
					dev_info(charger->dev,
							"%s: addr: 0x%x write fail\n", __func__, charger->addr);
				}
			}
			ret = count;
			break;
		default:
			break;
	}
	return ret;
}

static int sec_wpc_create_attrs(struct device *dev)
{
	unsigned long i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(p9220_attributes); i++) {
		rc = device_create_file(dev, &p9220_attributes[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &p9220_attributes[i]);
create_attrs_succeed:
	return rc;
}

static int wpc_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	struct p9220_charger_data *charger =
		container_of(nb, struct p9220_charger_data,
			     wpc_nb);

	pr_info("%s action=%lu, attached_dev=%d\n",
		__func__, action, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			p9220_reg_update(charger->client, P9220_WPC_FLAG_REG, 0x0, P9220_WATCHDOG_DIS_MASK);
			p9220_get_firmware_version(charger, P9220_RX_FIRMWARE);
			p9220_get_firmware_version(charger, P9220_TX_FIRMWARE);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int p9220_parse_dt(struct device *dev, p9220_charger_platform_data_t *pdata)
{

	struct device_node *np = dev->of_node;
	const u32 *p;
	int len, ret;
	/*changes can be added later, when needed*/

	pdata->wpc_int = of_get_named_gpio_flags(np, "p9220,irq-gpio",
						0, &pdata->irq_gpio_flags);
	pr_debug("%s: irq-gpio: %u \n", __func__, pdata->wpc_int);

	pdata->can_vbat_monitoring = of_property_read_bool(np, "p9220,vbat-monitoring");

	p = of_get_property(np, "p9220,sec_mode_data", &len);
	if (p) {
		pdata->num_sec_mode = len / sizeof(struct p9220_sec_mode_config_data);
		pdata->sec_mode_config_data = kzalloc(len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "p9220,sec_mode_data",
				 (u32 *)pdata->sec_mode_config_data, len/sizeof(u32));
		if (ret) {
			pr_err("%s: failed to read p9220,sec_mode_data: %d\n", __func__, ret);
			kfree(pdata->sec_mode_config_data);
			pdata->sec_mode_config_data = NULL;
			pdata->num_sec_mode = 0;
		}
		pr_err("%s: num_sec_mode : %d\n", __func__, pdata->num_sec_mode);
		for (len = 0; len < pdata->num_sec_mode; ++len)
			pr_err("mode %d : vrect:%d, vout:%d\n",
				len, pdata->sec_mode_config_data[len].vrect, pdata->sec_mode_config_data[len].vout);
	} else
		pr_err("%s: there is no p9220,sec_mode_data\n", __func__);

	ret = of_property_read_u32(np, "p9220,tx-off-high-temp",
		&pdata->tx_off_high_temp);
	if (ret) {
		pr_info("%s : TX-OFF-TEMP is Empty\n", __func__);
		pdata->tx_off_high_temp = INT_MAX;
	}

	ret = of_property_read_u32(np, "p9220,dedicated-tx-type",
		&pdata->d_tx_type);
	if (ret)
		pr_info("%s : dedicated tx type is Empty\n", __func__);

	return 0;
}

static int p9220_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct p9220_charger_data *charger;
	p9220_charger_platform_data_t *pdata = client->dev.platform_data;
	int ret = 0;

	dev_info(&client->dev,
		"%s: p9220 Charger Driver Loading\n", __func__);

	pdata = devm_kzalloc(&client->dev, sizeof(p9220_charger_platform_data_t), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	ret = p9220_parse_dt(&client->dev, pdata);
	if (ret < 0)
		return ret;

	client->irq = gpio_to_irq(pdata->wpc_int);
	pdata->wireless_charger_name = "wpc";
	pdata->hw_rev_changed = 1;
	pdata->on_mst_wa=1;
	pdata->wpc_cv_call_vout = 5600;
	pdata->wpc_det = 2;
	pdata->wpc_int = 4;
		
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_wpc_nomem;
	}
	charger->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}
	charger->client = client;
	charger->pdata = pdata;

	i2c_set_clientdata(client, charger);
	
	charger->pdata->ic_on_mode = false;
	charger->pdata->cable_type = P9220_PAD_MODE_NONE;
	charger->pdata->is_charging = 0;

	charger->pdata->otp_firmware_result = P9220_FW_RESULT_DOWNLOADING;
	charger->pdata->tx_firmware_result = P9220_FW_RESULT_DOWNLOADING;
	charger->pdata->tx_status = 0;
	charger->pdata->cs100_status = 0;
	charger->pdata->vout_status = P9220_VOUT_0V;
	charger->pdata->opfq_cnt = 0;
	charger->pdata->watchdog_test = false;
	charger->pdata->charging_mode = SEC_INITIAL_MODE;

	charger->psy_chg.name		= pdata->wireless_charger_name;
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= p9220_chg_get_property;
	charger->psy_chg.set_property	= p9220_chg_set_property;
	charger->psy_chg.properties	= sec_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);

	mutex_init(&charger->io_lock);

	wake_lock_init(&charger->wpc_wake_lock, WAKE_LOCK_SUSPEND,
			"wpc_wakelock");
	wake_lock_init(&charger->wpc_vbat_monitoring_enable_lock, WAKE_LOCK_SUSPEND,
			"wpc_vbat_monitoring_enable_lock");

	charger->temperature = 250;

	/* wpc_det */
	if (charger->pdata->irq_wpc_det) {
		INIT_DELAYED_WORK(&charger->wpc_det_work, p9220_wpc_det_work);
		INIT_DELAYED_WORK(&charger->wpc_opfq_work, p9220_wpc_opfq_work);
#if 0

		ret = request_threaded_irq(charger->pdata->irq_wpc_det,
				NULL, p9220_wpc_det_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT,
				"wpd-det-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_supply_unreg;
		}

		ret = enable_irq_wake(charger->pdata->irq_wpc_det);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
#endif
	}

	/* wpc_irq */
	if (charger->client->irq) {
		INIT_DELAYED_WORK(&charger->wpc_isr_work, p9220_wpc_isr_work);
		INIT_DELAYED_WORK(&charger->wpc_lpm_work, p9220_wpc_lpm_work);
#if 1
		ret = gpio_request(charger->client->irq, "wpc-irq");
		if (ret) {
			pr_err("%s failed requesting gpio(%d)\n",
				__func__, charger->client->irq);
			goto err_supply_unreg;
		}
		ret = request_threaded_irq(charger->client->irq,
				NULL, p9220_wpc_irq_thread,
				IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT,
				"wpc-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_supply_unreg;
		}
#endif		
	}

	INIT_DELAYED_WORK(&charger->wpc_init_work, p9220_wpc_init);
	schedule_delayed_work(&charger->wpc_init_work, msecs_to_jiffies(1000));

	INIT_DELAYED_WORK(&charger->wpc_init_detect_work, p9220_wpc_init_detect);
	schedule_delayed_work(&charger->wpc_init_detect_work, msecs_to_jiffies(2000));

	INIT_DELAYED_WORK(&charger->wpc_mode_work, p9220_wpc_mode_change);

	if(&charger->psy_chg==NULL){
		//pr_info("%s ====================>>>>>>>>>>>>>>>>>>>222222222222",__func__);
		goto err_supply_unreg;
	}
	
	ret = power_supply_register(&client->dev, &charger->psy_chg);
	//ret =0;
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_supply_unreg;
	}

	ret = sec_wpc_create_attrs(charger->psy_chg.dev);
	//ret =0;
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_supply_unreg;
	}

	
#if 0
	charger->wqueue = create_singlethread_workqueue("p9220_workqueue");
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	wake_lock_init(&charger->wpc_wake_lock, WAKE_LOCK_SUSPEND,
			"wpc_wakelock");
	wake_lock_init(&charger->wpc_update_lock, WAKE_LOCK_SUSPEND,
			"wpc_update_lock");
	wake_lock_init(&charger->wpc_opfq_lock, WAKE_LOCK_SUSPEND,
			"wpc_opfq_lock");

	pr_info("%s ====================>>>>>>>>>>>>>>>>>>>",__func__);
#endif
	charger->last_poll_time = ktime_get_boottime();
	alarm_init(&charger->polling_alarm, ALARM_BOOTTIME,
		p9220_wpc_ph_mode_alarm);

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&charger->wpc_nb, wpc_handle_notification,
			       MUIC_NOTIFY_DEV_CHARGER);
#endif
	dev_info(&client->dev,
		"%s: p9220 Charger Driver Loaded\n", __func__);

	//device_init_wakeup(charger->dev, 1);
	return 0;
//err_pdata_free:
//	power_supply_unregister(&charger->psy_chg);
err_supply_unreg:
	wake_lock_destroy(&charger->wpc_wake_lock);
	wake_lock_destroy(&charger->wpc_vbat_monitoring_enable_lock);
	mutex_destroy(&charger->io_lock);
err_i2cfunc_not_support:
//irq_base_err:
	kfree(charger);
err_wpc_nomem:
	devm_kfree(&client->dev, pdata);
	return ret;
}
#if 0
static int p9220_charger_remove(struct i2c_client *client)
{
	return 0;
}
#endif


#if defined CONFIG_PM
static int p9220_charger_suspend(struct i2c_client *client,
				pm_message_t state)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);

	pr_info("%s: mode[%d]\n", __func__, charger->pdata->charging_mode);

	if (!charger->pdata->can_vbat_monitoring) {
		cancel_delayed_work(&charger->wpc_mode_work);
		p9220_sec_mode_change(charger);
	}

	if(!charger->pdata->hw_rev_changed) { /* this code is only temporary with non int_ext wpc_det */ 
		if (device_may_wakeup(charger->dev)){
			enable_irq_wake(charger->pdata->irq_wpc_int);
		}
		disable_irq(charger->pdata->irq_wpc_int);
	} else { /* this is for proper board */
		if (device_may_wakeup(charger->dev)){
			enable_irq_wake(charger->pdata->irq_wpc_int);
			enable_irq_wake(charger->pdata->irq_wpc_det);
		}
		disable_irq(charger->pdata->irq_wpc_int);
		disable_irq(charger->pdata->irq_wpc_det);
	}
	return 0;
}

static int p9220_charger_resume(struct i2c_client *client)
{
	int wc_w_state;
	struct p9220_charger_data *charger = i2c_get_clientdata(client);

	pr_info("%s: mode[%d]\n", __func__, charger->pdata->charging_mode);

	if (charger->pdata->charging_mode != SEC_INITIAL_MODE && !charger->pdata->can_vbat_monitoring) {
		p9220_reg_write(charger->client, P9220_VRECT_SET_REG, 126);
		msleep(200);
		p9220_reg_write(charger->client, P9220_VOUT_SET_REG, P9220_VOUT_TO_VAL(4900));
		schedule_delayed_work(&charger->wpc_mode_work, msecs_to_jiffies(5000));
	}

	if(!charger->pdata->hw_rev_changed) { /* this code is only temporary with non int_ext wpc_det */ 
		wc_w_state = gpio_get_value(charger->pdata->wpc_det);

		if(charger->wc_w_state != wc_w_state)
			queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);
		if (device_may_wakeup(charger->dev)) {
			disable_irq_wake(charger->pdata->irq_wpc_int);
		}
		enable_irq(charger->pdata->irq_wpc_int);
	} else { /* this is for proper board */
		if (device_may_wakeup(charger->dev)) {
			disable_irq_wake(charger->pdata->irq_wpc_int);
			disable_irq_wake(charger->pdata->irq_wpc_det);
		}
		enable_irq(charger->pdata->irq_wpc_int);
		enable_irq(charger->pdata->irq_wpc_det);
	}
	return 0;
}
#else
#define p9220_charger_suspend NULL
#define p9220_charger_resume NULL
#endif

static void p9220_charger_shutdown(struct i2c_client *client)
{
	struct p9220_charger_data *charger = i2c_get_clientdata(client);

	pr_debug("%s \n", __func__);
	if(charger->pdata->is_charging) {
		//p9220_set_vout(charger, P9220_VOUT_5V);
		pr_info("%s: Wireless Vout forced set to 5V\n", __func__);
		//p9220_set_vrect_adjust(charger, P9220_HEADROOM_1);

		/* this code is only for Zero2, There is zinitix leakage . should turn on before charging on */
		//if(charger->pdata->on_mst_wa)
			//wireless_of_mst_hw_onoff(0);
	}
}


static const struct i2c_device_id p9220_charger_id[] = {
	{"p9220-charger", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, p9220_charger_id);

static struct of_device_id p9220_i2c_match_table[] = {
	{ .compatible = "p9220,i2c"},
	{},
};

static struct i2c_driver p9220_charger_driver = {
	.driver = {
		.name	= "p9220-charger",
		.owner	= THIS_MODULE,
		.of_match_table = p9220_i2c_match_table,
	},
	.shutdown	= p9220_charger_shutdown,
	.suspend	= p9220_charger_suspend,
	.resume		= p9220_charger_resume,
	.probe	= p9220_charger_probe,
	//.remove	= p9220_charger_remove,
	.id_table	= p9220_charger_id,
};

static int __init p9220_charger_init(void)
{
	pr_debug("%s \n",__func__);
	return i2c_add_driver(&p9220_charger_driver);
}

static void __exit p9220_charger_exit(void)
{
	pr_debug("%s \n",__func__);
	i2c_del_driver(&p9220_charger_driver);
}

module_init(p9220_charger_init);
module_exit(p9220_charger_exit);

MODULE_DESCRIPTION("Samsung p9220 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
