/* drivers/battery/s2mpw01_charger.c
 * S2MPW01 Charger Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>
#include <linux/battery/charger/s2mpw01_charger.h>
#include <linux/version.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>

#define ENABLE_MIVR 1
#define MINVAL(a, b) ((a <= b) ? a : b)

#define EOC_DEBOUNCE_CNT 2
#define HEALTH_DEBOUNCE_CNT 3
#define DEFAULT_CHARGING_CURRENT 500

#define EOC_SLEEP 200
#define EOC_TIMEOUT (EOC_SLEEP * 6)
#ifndef EN_TEST_READ
#define EN_TEST_READ 1
#endif

struct s2mpw01_charger_data {
	struct s2mpw01_dev	*iodev;
	struct i2c_client       *client;
	struct device *dev;
	struct s2mpw01_platform_data *s2mpw01_pdata;
	struct delayed_work	charger_work;
	struct delayed_work init_work;
	struct delayed_work usb_work;
	struct delayed_work rid_work;
	struct delayed_work ta_work;
	struct delayed_work tx_pad_work;
	struct delayed_work ac_ok_work;

	struct wake_lock ta_work_lock;
	struct wake_lock ta_pad_lock;
	struct wake_lock ac_ok_lock;

	struct workqueue_struct *charger_wqueue;
	struct power_supply	psy_chg;
	s2mpw01_charger_platform_data_t *pdata;
	int dev_id;
	int charging_current;
	int siop_level;
	int cable_type;
	bool is_charging;
	bool is_usb_ready;
	struct mutex io_lock;

	/* register programming */
	int reg_addr;
	int reg_data;

	bool full_charged;
	bool ovp;
	bool factory_mode;

	int unhealth_cnt;
	int status;
	int onoff;

	int tx_type;
	int retail_mode;
	int extreme_mode;

	/* charger enable, disable data */
	u8 chg_en_data;

	/* s2mpw01 */
	int irq_det_bat;
	int irq_chg;
	int irq_tmrout;

	int irq_uart_off;
	int irq_uart_on;
	int irq_usb_off;
	int irq_usb_on;
	int irq_uart_cable;
	int irq_fact_leakage;
	int irq_jigon;
	int irq_acokf;
	int irq_acokr;
	int irq_rid_attach;
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_attached_dev_t	muic_dev;
#endif
};

static enum power_supply_property s2mpw01_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_USB_OTG,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
};

extern unsigned int batt_booting_chk;
extern unsigned int system_rev;

static int s2mpw01_get_charging_health(struct s2mpw01_charger_data *charger);
static void s2mpw01_check_acok(struct s2mpw01_charger_data *charger);
static void s2mpw01_muic_detect_handler(struct s2mpw01_charger_data *charger, bool is_on);

/* Add for charger debugging */
int s2mpw01_charger_en;
int s2mpw01_charger_current;
int s2mpw01_charger_float;
int s2mpw01_charger_topoff;

static void s2mpw01_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x0; i <= 0x17; i++) {
		s2mpw01_read_reg(i2c, i, &data);

		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	pr_err("[DEBUG]%s: %s\n", __func__, str);
}

static void s2mpw01_enable_charger_switch(struct s2mpw01_charger_data *charger,
		int onoff)
{
	unsigned int data = 0;
	u8 acok_stat = 0;

	data = charger->chg_en_data;

	/* ACOK status */
	if(charger->dev_id < EVT_3) {
		s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &acok_stat);
		pr_err("[DEBUG]%s: onoff[%d], chg_en_data[0x%x], acok[0x%x]\n",
			__func__, onoff, charger->chg_en_data, acok_stat);
	}

	if (onoff > 0) {
		pr_err("[DEBUG]%s: turn on charger\n", __func__);
		s2mpw01_charger_en = 1;
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
	} else {
		charger->full_charged = false;
		pr_err("[DEBUG] %s: turn off charger\n", __func__);

		s2mpw01_charger_en = 0;

		if(charger->dev_id < EVT_3) {
			if (!charger->factory_mode) {
				data |= 0x40;
				s2mpw01_write_reg(charger->client, 0x2E, data);
				s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, 0, EN_CHG_MASK);
			}
		} else {
			s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, 0, EN_CHG_MASK);
		}

		/* ACOK status : high > keep charger off, low > charger on */
		if (!(acok_stat & ACOK_STATUS_MASK) && (charger->dev_id < EVT_3)) {
			data = charger->chg_en_data;
			s2mpw01_write_reg(charger->client, 0x2E, data);
			s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
			pr_err("[DEBUG] %s: turn on charger for RID detection\n", __func__);
		}
	}
}

static void s2mpw01_topoff_interrupt_onoff(struct s2mpw01_charger_data *charger, int onoff)
{

	if (onoff > 0) {
		/* Use top-off interrupt. Masking off */
		pr_err("[DEBUG]%s: Use top-off interrupt : 0x%x, 0x%x\n", __func__,
			charger->iodev->irq_masks_cur[3], charger->iodev->irq_masks_cache[3]);
		charger->iodev->irq_masks_cur[3] &= ~0x04;
		charger->iodev->irq_masks_cache[3] &= ~0x04;
		charger->iodev->topoff_mask_status = 1;
		s2mpw01_enable_charger_switch(charger, 0);
		msleep(100);
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_INT1M, 0x00, 0x04);
		s2mpw01_enable_charger_switch(charger, 1);
	} else {
		/* Not use top-off interrupt. Masking */
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_INT1M, 0x04, 0x04);
		pr_err("[DEBUG]%s: Top-off interrupt Masking: 0x%x, 0x%x\n", __func__,
			charger->iodev->irq_masks_cur[3], charger->iodev->irq_masks_cache[3]);
		charger->iodev->irq_masks_cur[3] |= 0x04;
		charger->iodev->irq_masks_cache[3] |= 0x04;
		charger->iodev->topoff_mask_status = 0;
	}

}

static void s2mpw01_set_regulation_voltage(struct s2mpw01_charger_data *charger,
		int float_voltage)
{
	unsigned int data;

	pr_err("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);

	s2mpw01_charger_float = float_voltage;

	if (float_voltage <= 4200)
		data = 0;
	else if (float_voltage > 4200 && float_voltage <= 4550)
		data = (float_voltage - 4200) / 50;
	else
		data = 0x7;

	s2mpw01_update_reg(charger->client,
			S2MPW01_CHG_REG_CTRL5, data << SET_VF_VBAT_SHIFT, SET_VF_VBAT_MASK);
}

static void s2mpw01_set_additional(struct s2mpw01_charger_data *charger, int n, int onoff)
{
	int val = 0xff;

	pr_err("[DEBUG]%s: n (%d), onoff (%d)\n", __func__, n, onoff);

	if (onoff == 1) {

		switch (n) {
		case 1:
			val = 0x1;
			break;
		case 2:
			val = 0x3;
			break;
		case 3:
			val = 0x7;
			break;
		case 4:
			val = 0xf;
			break;
		case 5:
			val = 0x1f;
			break;
		case 6:
			val = 0x3f;
			break;
		case 7:
			val = 0x7f;
			break;
		case 8:
			val = 0xff;
			break;
		}
		/* Apply additional charging current */
		s2mpw01_update_reg(charger->client,
		S2MPW01_CHG_REG_CTRL7, val << SET_ADD_PATH_SHIFT, SET_ADD_PATH_MASK);

		/* Additional charging path On */
		s2mpw01_update_reg(charger->client,
		S2MPW01_CHG_REG_CTRL5, 1 << SET_ADD_ON_SHIFT, SET_ADD_ON_MASK);
	} else if (onoff == 0) {
		/* Additional charging path Off */
		s2mpw01_update_reg(charger->client,
		S2MPW01_CHG_REG_CTRL5, 0 << SET_ADD_ON_SHIFT, SET_ADD_ON_MASK);

		/* Restore addition charging current */
		s2mpw01_update_reg(charger->client,
		S2MPW01_CHG_REG_CTRL7, val << SET_ADD_PATH_SHIFT, SET_ADD_PATH_MASK);
	}
}

static int s2mpw01_get_additional(struct i2c_client *i2c)
{
	int ret, n = 0;
	u8 on_off, val;

	ret = s2mpw01_read_reg(i2c, S2MPW01_CHG_REG_CTRL5, &on_off);
	if (ret < 0)
		return n;

	on_off = (on_off & SET_ADD_ON_MASK) >> SET_ADD_ON_SHIFT;

	ret = s2mpw01_read_reg(i2c, S2MPW01_CHG_REG_CTRL7, &val);
	if (ret < 0)
		return n;

	pr_err("[DEBUG]%s: on_off (%x), val (0x%x)", __func__, on_off, val);

	if (on_off) {
		switch (val) {
		case 0x1:
			n = 1;
			break;
		case 0x3:
			n = 2;
			break;
		case 0x7:
			n = 3;
			break;
		case 0xf:
			n = 4;
			break;
		case 0x1f:
			n = 5;
			break;
		case 0x3f:
			n = 6;
			break;
		case 0x7f:
			n = 7;
			break;
		case 0xff:
			n = 8;
			break;
		}
	}
	return n;
}

static void s2mpw01_set_fast_charging_current(struct s2mpw01_charger_data *charger,
		int charging_current)
{
	int data;

	pr_err("[DEBUG]%s: fast charge current  %d\n", __func__, charging_current);

	s2mpw01_charger_current = charging_current;

	/* Disable additional charging current */
	s2mpw01_set_additional(charger, 0, 0);

	if (charging_current <= 75)
		data = 0x6;
	else if (charging_current <= 120) {
		data = 0x6;
		s2mpw01_set_additional(charger, 5, 1);
	}
	else if (charging_current <= 150)
		data = 0;
	else if (charging_current <= 175)
		data = 0x7;
	else if (charging_current > 175 && charging_current <= 400)
		data = (charging_current - 150) / 50;
	else
		data = 0x5;

	s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL2, data << FAST_CHARGING_CURRENT_SHIFT,
			FAST_CHARGING_CURRENT_MASK);
	s2mpw01_test_read(charger->client);
}

static int s2mpw01_get_fast_charging_current(struct i2c_client *i2c)
{
	int ret, n;
	u8 data;

	ret = s2mpw01_read_reg(i2c, S2MPW01_CHG_REG_CTRL2, &data);
	if (ret < 0)
		return ret;

	data = (data & FAST_CHARGING_CURRENT_MASK) >> FAST_CHARGING_CURRENT_SHIFT;

	if (data <= 0x5)
		data = data * 50 + 150;
	else if (data == 0x06)
		data = 75;
	else if (data == 0x07)
		data = 175;

	n = s2mpw01_get_additional(i2c);
	data = data + data * n/8;

	return data;
}

int eoc_current[16] =
{ 5,10,12,15,20,17,25,30,35,40,50,60,70,80,90,100,};

static int s2mpw01_get_current_eoc_setting(struct s2mpw01_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_CTRL4, &data);
	if (ret < 0)
		return ret;

	data = (data & FIRST_TOPOFF_CURRENT_MASK) >> 4;

	if (data > 0x0f)
		data = 0x0f;

	pr_err("[DEBUG]%s: data(0x%x), top-off current	%d\n", __func__, data, eoc_current[data]);

	return eoc_current[data];
}

static void s2mpw01_set_topoff_current(struct i2c_client *i2c, int current_limit)
{
	int data;

	if (current_limit <= 5)
		data = 0;
	else if (current_limit > 5 && current_limit <= 10)
		data = (current_limit - 5) / 5;
	else if (current_limit > 10 && current_limit < 18)
		data = (current_limit - 10) / 5 * 2 + 1;
	else if (current_limit >= 18 && current_limit < 20)
		data = 5;	  /* 17.5 mA */
	else if (current_limit >= 20 && current_limit < 25)
		data = 4;
	else if (current_limit >= 25 && current_limit <= 40)
		data = (current_limit - 25) / 5 + 6;
	else if (current_limit > 40 && current_limit <= 100)
		data = (current_limit - 40) / 10 + 9;
	else
		data = 0x0F;

	pr_err("[DEBUG]%s: top-off current	%d, data=0x%x\n", __func__, current_limit, data);
	s2mpw01_charger_topoff = current_limit;

	s2mpw01_update_reg(i2c, S2MPW01_CHG_REG_CTRL4, data << FIRST_TOPOFF_CURRENT_SHIFT,
			FIRST_TOPOFF_CURRENT_MASK);
}

/* eoc reset */
static void s2mpw01_set_charging_current(struct s2mpw01_charger_data *charger)
{
	int adj_current = 0;
	union power_supply_propval value;

	pr_err("[DEBUG]%s: charger->siop_level  %d\n", __func__, charger->siop_level);
	adj_current = charger->charging_current * charger->siop_level / 100;

	if (charger->retail_mode) {
		pr_err("[DEBUG]%s: retail_mode %d\n", __func__, charger->retail_mode);
		if (batt_booting_chk == 1) {
			adj_current = 75;
		}
	}

	/* When SOC is higher than 96%, then Current must be set to 75 mA */
	value.intval = 0;
	psy_do_property("sec-fuelgauge", get, POWER_SUPPLY_PROP_CAPACITY, value);
	if ((charger->cable_type == POWER_SUPPLY_TYPE_WPC) &&
		(value.intval >= 96)) {
		pr_err("[DEBUG]%s: SOC(%d) is Higher 96 set to 75 mA\n", __func__,
			value.intval);
		adj_current = 75;
	}

#if !defined(CONFIG_TIZEN_SEC_KERNEL_ENG)
	if ((charger->cable_type == POWER_SUPPLY_TYPE_WPC) &&
			(charger->tx_type == 0) && (batt_booting_chk == 1)) {
		pr_err("[DEBUG]%s: Right TX pad\n", __func__);
		adj_current = 75;
	}
#endif

	if (charger->extreme_mode) {
		pr_err("[DEBUG]%s: extreme_mode %d\n", __func__, charger->extreme_mode);
		adj_current = 250;
	}

	if (batt_booting_chk == 0) {
		adj_current = 300;
	}

	s2mpw01_set_fast_charging_current(charger, adj_current);

	/*
	* For retail mode, when we turn on charger with 85 mA
	* So we must turn on additional path at attaching
	* condition
	*/
	if (charger->retail_mode) {
		if (batt_booting_chk == 1) {
			s2mpw01_set_additional(charger, 2, 1);
		}
	}
}

enum {
	S2MPW01_MIVR_4200MV = 0,
	S2MPW01_MIVR_4300MV,
	S2MPW01_MIVR_4400MV,
	S2MPW01_MIVR_4500MV,
	S2MPW01_MIVR_4600MV,
	S2MPW01_MIVR_4700MV,
	S2MPW01_MIVR_4800MV,
	S2MPW01_MIVR_4900MV,
};

#if ENABLE_MIVR
/* charger input regulation voltage setting */
static void s2mpw01_set_mivr_level(struct s2mpw01_charger_data *charger)
{
	int mivr = S2MPW01_MIVR_4600MV;

	s2mpw01_update_reg(charger->client,
			S2MPW01_CHG_REG_CTRL4, mivr << SET_VIN_DROP_SHIFT, SET_VIN_DROP_MASK);
}
#endif /*ENABLE_MIVR*/

static void s2mpw01_configure_charger(struct s2mpw01_charger_data *charger)
{
	int eoc = 0;
	union power_supply_propval chg_mode;

	pr_err("%s() set configure charger \n", __func__);

	if (charger->charging_current < 0) {
		pr_info("%s() OTG is activated. Ignore command!\n",	__func__);
		return;
	}

	if (!charger->pdata->charging_current_table) {
		pr_err("%s() table is not exist\n", __func__);
		return;
	}

#if ENABLE_MIVR
	s2mpw01_set_mivr_level(charger);
#endif /*DISABLE_MIVR*/

	/* msleep(200); */

	s2mpw01_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
		[charger->cable_type].fast_charging_current;

	pr_err("%s() fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	s2mpw01_set_charging_current(charger);

	if (charger->pdata->full_check_type == SEC_BATTERY_FULLCHARGED_CHGPSY) {
		if (charger->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_CHGPSY) {
				psy_do_property("battery", get,
						POWER_SUPPLY_PROP_CHARGE_NOW,
						chg_mode);

				if (chg_mode.intval == SEC_BATTERY_CHARGING_2ND) {
					/* s2mpw01_enable_charger_switch(charger, 0); */
					charger->full_charged = false;
					eoc = charger->pdata->charging_current_table
						[charger->cable_type].full_check_current_2nd;
				} else {
					eoc = charger->pdata->charging_current_table
						[charger->cable_type].full_check_current_1st;
				}
			} else {
				eoc = charger->pdata->charging_current_table
					[charger->cable_type].full_check_current_1st;
			}
		s2mpw01_set_topoff_current(charger->client, eoc);

		wake_lock_timeout(&charger->ta_work_lock, HZ);

		/* use TOP-OFF interrupt */
		schedule_delayed_work(&charger->ta_work, msecs_to_jiffies(200));
	}
	s2mpw01_enable_charger_switch(charger, 1);
}

/* here is set init charger data */
static bool s2mpw01_chg_init(struct s2mpw01_charger_data *charger)
{
	pr_info("%s : DEV ID : 0x%x\n", __func__, charger->dev_id);
	/* Buck switching mode frequency setting */

	/* Disable Timer function (Charging timeout fault) */
	/* to be */

	/* change Top-off detection debounce time (0x56 to 0x76) */
	s2mpw01_write_reg(charger->client, 0x2C, 0x76);

#if !(ENABLE_MIVR)
	/* voltage regulatio disable does not exist mu005 */
#endif

	if (!charger->pdata->topoff_timer_enable) {
		pr_info("%s: Top-off timer disable\n", __func__);
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL8,
				NO_TIMEOUT_30M_MASK, NO_TIMEOUT_30M_MASK);
	} else {
		pr_info("%s: Top-off timer enable\n", __func__);
	}

	/* Factory_mode initialization */
	charger->factory_mode = false;

	return true;
}

static int s2mpw01_get_charging_status(struct s2mpw01_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	u8 chg_sts;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &chg_sts);
	if (ret < 0)
		return status;
	pr_info("%s : charger status : 0x%x\n", __func__, chg_sts);

	if (charger->full_charged) {
			pr_info("%s : POWER_SUPPLY_STATUS_FULL : 0x%x\n", __func__, chg_sts);
			return POWER_SUPPLY_STATUS_FULL;
	}

	switch (chg_sts & 0x12) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x10:	/*charge state */
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x12:	/* Input is invalid */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	default:
		break;
	}

	s2mpw01_test_read(charger->client);
	return status;
}

static int s2mpw01_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(iic, S2MPW01_CHG_REG_STATUS1, &data);
	if (ret < 0) {
		pr_err("%s fail\n", __func__);
		return ret;
	}

	switch (data & (1 << CHG_STATUS1_CHG_STS)) {
	case 0x10:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		/* 005 does not need to do this */
		/* pre-charge mode */
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static bool s2mpw01_get_batt_present(struct i2c_client *iic)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(iic, S2MPW01_CHG_REG_STATUS2, &data);
	if (ret < 0)
		return false;

	return (data & DET_BAT_STATUS_MASK) ? true : false;
}

static int s2mpw01_get_charging_health(struct s2mpw01_charger_data *charger)
{
	int ret;
	u8 data, data1;

	pr_info("[%s] \n " , __func__);
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &data);
	s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &data1);

	pr_info("[%s] chg_status1: 0x%x, pm_status1: 0x%x\n" , __func__, data, data1);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (data & (1 << CHG_STATUS1_CHGVIN)) {
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		pr_info("[%s] POWER_SUPPLY_HEALTH_GOOD\n" , __func__);
		return POWER_SUPPLY_HEALTH_GOOD;
	}

	if((data1 & ACOK_STATUS_MASK) && (data & CHGVINOVP_STATUS_MASK) &&
		(data & CIN2BAT_STATUS_MASK)) {
		pr_info("[%s] POWER_SUPPLY_HEALTH_OVERVOLTAGE, unhealth_cnt %d\n" ,
			__func__, charger->unhealth_cnt);
		if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
			return POWER_SUPPLY_HEALTH_GOOD;
		charger->unhealth_cnt++;
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int s2mpw01_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	/* int chg_curr, aicr; */
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_chg);
	unsigned char status;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpw01_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpw01_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 2000;
#if defined(CONFIG_P9220_VOUT_CONTROL)
		if (charger->status == POWER_SUPPLY_STATUS_FULL) {
			union power_supply_propval value;
			psy_do_property("wpc", get,
				POWER_SUPPLY_PROP_STATUS, value);
			pr_info("%s: wpc status val:%d\n", __func__, value.intval);
			if (value.intval == 0) {
				if (charger->onoff == 1) {
					pr_info("%s: call s2mpw01_check_acok\n", __func__);
					wake_lock_timeout(&charger->ac_ok_lock, HZ * 2);
					schedule_delayed_work(&charger->ac_ok_work, msecs_to_jiffies(700));
				}
			}
		} else if (charger->retail_mode &&
				charger->status == POWER_SUPPLY_STATUS_CHARGING &&
				charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
			union power_supply_propval value;
			psy_do_property("wpc", get,
				POWER_SUPPLY_PROP_STATUS, value);
			pr_info("%s: wpc status val:%d\n", __func__, value.intval);
			if (value.intval == 0) {
				if (charger->onoff == 1) {
					pr_info("%s: call s2mpw01_check_acok\n", __func__);
					wake_lock_timeout(&charger->ac_ok_lock, HZ * 2);
					schedule_delayed_work(&charger->ac_ok_work, msecs_to_jiffies(700));
				}
			}
		} else
#endif
		{
			/* Add W/A to solve duplicate irq Attach/Detach */
			if (charger->onoff == 1) {
				pr_info("%s: call s2mpw01_check_acok\n", __func__);
				wake_lock_timeout(&charger->ac_ok_lock, HZ * 2);
				schedule_delayed_work(&charger->ac_ok_work, msecs_to_jiffies(700));
			}
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			val->intval = s2mpw01_get_fast_charging_current(charger->client);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mpw01_get_charge_type(charger->client);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = charger->pdata->chg_float_voltage;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mpw01_get_batt_present(charger->client);
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &status);
		pr_info("%s: pm status : 0x%x\n", __func__, status);
		if (status & ACOK_STATUS_MASK)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = charger->tx_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		return -ENODATA;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpw01_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_chg);
	int eoc;
/*	int previous_cable_type = charger->cable_type; */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			pr_info("%s() [BATT] Type Battery\n", __func__);
			if (!charger->pdata->charging_current_table)
				return -EINVAL;

			charger->charging_current = charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].fast_charging_current;

			s2mpw01_set_charging_current(charger);
			s2mpw01_set_topoff_current(charger->client,
					charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].full_check_current_1st);
			charger->is_charging = false;
			charger->full_charged = false;
			s2mpw01_topoff_interrupt_onoff(charger, 0);
			s2mpw01_enable_charger_switch(charger, 0);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			pr_info("%s() OTG mode not supported\n", __func__);
		} else {
			pr_info("%s()  Set charging, Cable type = %d\n",
				 __func__, charger->cable_type);
			charger->is_charging = true;
			/* Enable charger */
			s2mpw01_configure_charger(charger);
		}
#if EN_TEST_READ
		msleep(100);
		s2mpw01_test_read(charger->client);
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pr_info("%s() is_charging %d\n", __func__, charger->is_charging);
		/* set charging current */
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			charger->siop_level = val->intval;
			pr_info("%s() SIOP level = %d, chg current = %d\n", __func__,
					val->intval, charger->charging_current);
			eoc = s2mpw01_get_current_eoc_setting(charger);
			s2mpw01_set_charging_current(charger);
		}
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		pr_info("%s() set current[%d]\n", __func__, val->intval);
		charger->charging_current = val->intval;
		s2mpw01_set_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s() float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mpw01_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		eoc = s2mpw01_get_current_eoc_setting(charger);
		pr_info("%s() Set Power Now -> chg current = %d mA, eoc = %d mA\n",
				__func__, val->intval, eoc);
		s2mpw01_set_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_USB_OTG:
		pr_err("%s() OTG mode not supported\n", __func__);
		/* s2mpw01_charger_otg_control(charger, val->intval); */
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		pr_info("%s() CHARGING_ENABLE\n", __func__);
		/* charger->is_charging = val->intval; */
		s2mpw01_enable_charger_switch(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pr_info("%s() CHARGE_CONTROL_LIMIT_MAX[%d]\n", __func__, val->intval);
		charger->tx_type = val->intval;
		s2mpw01_set_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_AVG:
		pr_info("%s() POWER_SUPPLY_PROP_CHARGE_AVG[%d]\n", __func__, val->intval);
		charger->retail_mode = val->intval;
		break;
	case POWER_SUPPLY_PROP_POWER_AVG:
		pr_info("%s() POWER_SUPPLY_PROP_POWER_AVG[%d]\n", __func__, val->intval);
		charger->extreme_mode = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#if 0
/* s2mpw01 interrupt service routine */
static irqreturn_t s2mpw01_det_bat_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &val);
	if ((val & DET_BAT_STATUS_MASK) == 0) {
		s2mpw01_enable_charger_switch(charger, 0);
		pr_err("charger-off if battery removed\n");
	}
	return IRQ_HANDLED;
}
#endif

static void s2mpw01_check_acok(struct s2mpw01_charger_data *charger)
{
	int i;
	unsigned char status;

	for (i = 0; i < 5; i++) {
		s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &status);
		pr_info("%s: pm status : 0x%x\n", __func__, status);
		if (status & ACOK_STATUS_MASK) {
			pr_err("[DEBUG]%s: skip_irq : acok[0x%x] count %d\n", __func__, status, i);
			goto skip_check;
		}
		msleep(100);
	}
	s2mpw01_muic_detect_handler(charger, false);

skip_check:
	return;
}

static void s2mpw01_factory_mode_setting(struct s2mpw01_charger_data *charger)
{
	unsigned int stat_val;

	/* ACOK status */
	if(charger->dev_id < EVT_3) {
		stat_val = charger->chg_en_data;
		s2mpw01_write_reg(charger->client, 0x2E, stat_val);
		s2mpw01_enable_charger_switch(charger, 1);
	}

	charger->factory_mode = true;
	pr_err("%s, factory mode\n", __func__);
}

static irqreturn_t s2mpw01_chg_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val, valm;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &val);
	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_INT1M, &valm);
	pr_info("%s : %02x, CHG_INT1 ---> 0x%x\n", __func__, val, valm);

	if ((val & TOP_OFFSTATUS_MASK) && (val & CHGSTS_STATUS_MASK)) {
		pr_info("%s : top_off status~!\n", __func__);
		charger->full_charged = true;
		/* TOP-OFF interrupt masking */
		s2mpw01_topoff_interrupt_onoff(charger, 0);
	}

	return IRQ_HANDLED;
}

static irqreturn_t s2mpw01_tmrout_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &val);
	if (val & 0x10) {
		/* Timer out status */
		pr_err("%s, fast-charging timeout, timer clear\n", __func__);
		s2mpw01_enable_charger_switch(charger, 0);
		msleep(100);
		s2mpw01_enable_charger_switch(charger, 1);
	}
	return IRQ_HANDLED;
}

#if defined(CONFIG_S2MPW01_RID_DETECT)
static void s2mpw01_muic_init_detect(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
		container_of(work, struct s2mpw01_charger_data, init_work.work);

	int ret;
	unsigned char status, chg_sts2, chg_sts3;

	/* check when booting after USB connected */
	ret = s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &status);
	if (ret < 0) {
		pr_err("{DEBUG] %s : pm status read fail\n", __func__);
	}
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
	if (ret < 0) {
		pr_err("{DEBUG] %s : chg status2 read fail\n", __func__);
	}
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
	if (ret < 0) {
		pr_err("{DEBUG] %s : chg status3 read fail\n", __func__);
	}

	pr_info("%s: pm status, chg status3: 0x%x,0x%x,0x%x\n", __func__, status, chg_sts2, chg_sts3);
	if(status & ACOK_STATUS_MASK) {
		charger->onoff = 1;
		if (chg_sts3 & UART_CABLE_STATUS_MASK) {
#if defined(CONFIG_MUIC_NOTIFIER)
			/* if (charger->is_usb_ready) */
			{
				charger->muic_dev = ATTACHED_DEV_USB_MUIC;
				muic_notifier_attach_attached_dev(charger->muic_dev);
			}
#endif
			pr_info("%s: USB connected\n", __func__);
		} else {
			if (!(chg_sts2 & JIGON_STATUS_MASK)) {
#if defined(CONFIG_MUIC_NOTIFIER)
				if (chg_sts2 & CHGIN_INPUT_STATUS_MASK) {
					pr_info("%s: Wired TA connected\n", __func__);
					charger->muic_dev = ATTACHED_DEV_TA_MUIC;
				} else {
					pr_info("%s: Wireless TA connected\n", __func__);
					charger->muic_dev = ATTACHED_DEV_WIRELESS_TA_MUIC;
				}
				muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
			} else {
				if (chg_sts2 & UART_BOOT_OFF_STATUS_MASK) {
#if defined(CONFIG_MUIC_NOTIFIER)
					charger->muic_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
					muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
					pr_info("%s: JIG_ID UART OFF ( 523K ) connected VBUS ON\n", __func__);
				} else if (chg_sts3 & UART_BOOT_ON_STATUS_MASK) {
#if defined(CONFIG_MUIC_NOTIFIER)
					charger->muic_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
					muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
					pr_info("%s: JIG_ID UART ON ( 619K ) connected\n", __func__);
				}
			}
		}
	} else {
		if (chg_sts2 & UART_BOOT_OFF_STATUS_MASK) {
#if defined(CONFIG_MUIC_NOTIFIER)
			charger->muic_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
			pr_info("%s: JIG_ID UART OFF ( 523K ) connected VBUS OFF\n", __func__);
		}
	}
}

static void s2mpw01_ta_detect(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
		container_of(work, struct s2mpw01_charger_data, ta_work.work);

	if (charger->is_charging) {
		if (batt_booting_chk) {
			s2mpw01_topoff_interrupt_onoff(charger, 1);
		} else {
			pr_err("[DEBUG]%s: batt_booting_chk:0, Use top-off interrupt: 0x%x, 0x%x\n", __func__,
				charger->iodev->irq_masks_cur[3], charger->iodev->irq_masks_cache[3]);
		}
	}

	wake_unlock(&charger->ta_work_lock);
}

static void s2mpw01_tx_pad_detect(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
		container_of(work, struct s2mpw01_charger_data, tx_pad_work.work);

	pr_err("{DEBUG] %s\n", __func__);
	s2mpw01_set_charging_current(charger);

	wake_unlock(&charger->ta_pad_lock);
}

static void s2mpw01_muic_usb_detect(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
		container_of(work, struct s2mpw01_charger_data, usb_work.work);

	int ret;
	unsigned char status, chg_sts2, chg_sts3;

	charger->is_usb_ready = true;
	/* check when booting after USB connected */
	ret = s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &status);
	if (ret < 0) {
		pr_err("{DEBUG] %s : pm status read fail\n", __func__);
	}
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
	if (ret < 0) {
		pr_err("{DEBUG] %s : chg status2 read fail\n", __func__);
	}
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
	if (ret < 0) {
		pr_err("{DEBUG] %s : chg status3 read fail\n", __func__);
	}

	pr_info("%s: pm status, chg status2 status3: 0x%x,0x%x,0x%x\n", __func__, status, chg_sts2, chg_sts3);
	if(status & ACOK_STATUS_MASK) {
		charger->onoff = 1;
		if(chg_sts3 & UART_CABLE_STATUS_MASK) {
			if (charger->is_usb_ready) {
#if defined(CONFIG_MUIC_NOTIFIER)
				charger->muic_dev = ATTACHED_DEV_USB_MUIC;
				muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
			}
			pr_info("%s: USB connected\n", __func__);
		} else if (chg_sts2 & USB_BOOT_ON_STATUS_MASK) {
			if (charger->is_usb_ready) {
#if defined(CONFIG_MUIC_NOTIFIER)
				charger->muic_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
				muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
			}
			pr_info("%s: JIG_ID USB ON ( 301K ) connected\n", __func__);
		} else if (chg_sts2 & USB_BOOT_OFF_STATUS_MASK) {
			if (charger->is_usb_ready) {
#if defined(CONFIG_MUIC_NOTIFIER)
				charger->muic_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
				muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
			}
			pr_info("%s: JIG_ID USB OFF ( 255K ) connected\n", __func__);
		}
	}
}

static void s2mpw01_muic_rid_check(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
		container_of(work, struct s2mpw01_charger_data, rid_work.work);
	unsigned char chg_sts2, chg_sts3;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
	pr_info("%s: acokr irq stat2: 0x%x stat3: 0x%x\n", __func__, chg_sts2, chg_sts3);

	if (!((chg_sts2 & USB_BOOT_ON_STATUS_MASK) || (chg_sts2 & USB_BOOT_OFF_STATUS_MASK) ||
		(chg_sts3 & UART_BOOT_ON_STATUS_MASK) || (chg_sts2 & UART_BOOT_OFF_STATUS_MASK) ||
		(chg_sts3 & UART_CABLE_STATUS_MASK))) {
		charger->factory_mode = false;
		pr_err("%s: factory mode[%d]\n", __func__, charger->factory_mode);
	}
}

static void s2mpw01_muic_detect_handler(struct s2mpw01_charger_data *charger, bool is_on)
{
	unsigned char chg_sts2, chg_sts3;
	unsigned char stat_val;
	union power_supply_propval value;

	if(is_on) {
		charger->onoff = 1;

		/*
		 * W/A in case of the chip revision is under EVT 3
		 * enables charging before reading the rid.
		 */
		if ((!charger->factory_mode) && (charger->dev_id < EVT_3)) {
			stat_val = charger->chg_en_data;
			stat_val |= 0x40;
			s2mpw01_write_reg(charger->client, 0x2E, stat_val);
		}

		s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
		s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
		pr_info("%s: rid irq stat2: 0x%x stat3: 0x%x\n", __func__, chg_sts2, chg_sts3);

		if(chg_sts3 & UART_CABLE_STATUS_MASK) {
#if defined(CONFIG_MUIC_NOTIFIER)
			if (charger->is_usb_ready) {
				charger->muic_dev = ATTACHED_DEV_USB_MUIC;
				muic_notifier_attach_attached_dev(charger->muic_dev);
			}
#endif
			pr_info("%s: USB connected. status3: 0x%x\n", __func__, chg_sts3);
		} else {
			if (!(chg_sts2 & JIGON_STATUS_MASK) && !(chg_sts2 & USB_BOOT_ON_STATUS_MASK) &&
				!(chg_sts2 & USB_BOOT_OFF_STATUS_MASK) && !(chg_sts3 & UART_BOOT_ON_STATUS_MASK)
				&& !(chg_sts2 & UART_BOOT_OFF_STATUS_MASK)) {
#if defined(CONFIG_MUIC_NOTIFIER)
				if (chg_sts2 & CHGIN_INPUT_STATUS_MASK) {
					pr_info("%s: Wired TA connected\n", __func__);
					charger->muic_dev = ATTACHED_DEV_TA_MUIC;
				} else {
					pr_info("%s: Wireless TA connected\n", __func__);
					charger->muic_dev = ATTACHED_DEV_WIRELESS_TA_MUIC;
					wake_lock(&charger->ta_pad_lock);
					schedule_delayed_work(&charger->tx_pad_work, msecs_to_jiffies(3000));
				}
				muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
			} else {
				if ((chg_sts2 & USB_BOOT_ON_STATUS_MASK) || (chg_sts2 & USB_BOOT_OFF_STATUS_MASK) ||
					(chg_sts3 & UART_BOOT_ON_STATUS_MASK) || (chg_sts2 & UART_BOOT_OFF_STATUS_MASK)) {
#if defined(CONFIG_MUIC_NOTIFIER)
					charger->muic_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
					muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
					pr_info(" USB_BOOT_ON_STATUS_MASK USB_BOOT_OFF_STATUS_MASK.\n");
					pr_info(" UART_BOOT_ON_STATUS_MASK UART_BOOT_OFF_STATUS_MASK.\n");
					pr_info("%s: JIG_ID UART OFF ( 523K ) connected \n", __func__);
				}
				if ((chg_sts2 & USB_BOOT_ON_STATUS_MASK) ||
					(chg_sts2 & USB_BOOT_OFF_STATUS_MASK)) {
#if defined(CONFIG_MUIC_NOTIFIER)
					if (charger->is_usb_ready)
						charger->muic_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
						muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
					pr_info(" USB_BOOT_ON_STATUS_MASK USB_BOOT_OFF_STATUS_MASK.\n");
					pr_info("%s: JIG_ID USB ON ( 301K ) connected \n", __func__);
				}
				pr_info("%s: JIG_ID connected.\n", __func__);
			}
		}
	} else {
		charger->onoff = 0;

		/*
		 * For retail mode and non S3 pad, when we turn on charger with 85 mA
		 * So we must turn off additional path at detaching
		 * condition
		 */
		s2mpw01_set_additional(charger, 0, 0);

		/*
		 * RID is detached.
		 * W/A when the chip revision is under EVT 3.
		 */
		if(charger->dev_id < EVT_3) {
			stat_val= charger->chg_en_data;
			s2mpw01_write_reg(charger->client, 0x2E, stat_val);
		}

		s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
		s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
		pr_info("%s: acokf irq:sts2[0x%x],sts3[0x%x]\n", __func__, chg_sts2, chg_sts3);

#if defined(CONFIG_MUIC_NOTIFIER)
		charger->muic_dev = ATTACHED_DEV_USB_MUIC;
		muic_notifier_detach_attached_dev(charger->muic_dev);
#endif

		/* TOP-OFF interrupt masking */
		s2mpw01_topoff_interrupt_onoff(charger, 0);

#if defined(CONFIG_MUIC_NOTIFIER)
		charger->muic_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		muic_notifier_detach_attached_dev(charger->muic_dev);
#endif
		psy_do_property("wpc", get, POWER_SUPPLY_PROP_STATUS, value);
		if (value.intval == 0)
			charger->tx_type = 0;
	}
}

static irqreturn_t s2mpw01_muic_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	int i;
	u8 acok_stat = 0;

	pr_info("%s: irq:%d\n", __func__, irq);

	if (irq == charger->irq_acokr) {
		s2mpw01_muic_detect_handler(charger, true);
	} else if (irq == charger->irq_acokf) {
#if defined(CONFIG_P9220_VOUT_CONTROL)
		if (charger->status == POWER_SUPPLY_STATUS_FULL) {
			union power_supply_propval value;

			psy_do_property("battery", get, POWER_SUPPLY_PROP_CHARGE_NOW, value);
			if (value.intval == SEC_BATTERY_CHARGING_NONE) {
				psy_do_property("wpc", get, POWER_SUPPLY_PROP_STATUS, value);
				pr_info("%s: wpc status val:%d\n", __func__, value.intval);
				if (value.intval == 1)
					goto skip_irq;
			}
		} else if (charger->retail_mode &&
				charger->status == POWER_SUPPLY_STATUS_CHARGING &&
				charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
			union power_supply_propval value;

			psy_do_property("wpc", get, POWER_SUPPLY_PROP_STATUS, value);
			pr_info("%s: wpc status val:%d\n", __func__, value.intval);
			if (value.intval == 1)
				goto skip_irq;
		}
#endif
		/* W/A : Preventing V drop issue when Charger on time */
		for (i = 0; i < 5; i++) {
			s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &acok_stat);
			pr_err("[DEBUG]%s: acok[0x%x]\n", __func__, acok_stat);
			if(acok_stat & ACOK_STATUS_MASK) {
				pr_err("[DEBUG]%s: skip_irq : acok[0x%x] count %d\n", __func__, acok_stat, i);
				goto skip_irq;
			}
			msleep(100);
		}
		/* Add this W/A for fixing vdrop issue */
		//batt_booting_chk = 1;

		s2mpw01_muic_detect_handler(charger, false);
	} else if (irq == charger->irq_usb_on) {
		/* usb boot on */
		pr_info("%s: usb boot on irq\n", __func__);
		if (charger->onoff == 1) {
			pr_info("%s: usb boot on notify done\n", __func__);

#if defined(CONFIG_MUIC_NOTIFIER)
			charger->muic_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
			muic_notifier_attach_attached_dev(charger->muic_dev);

			if (charger->is_usb_ready) {
				charger->muic_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
				pr_info("%s: [attach] muic_dev [0x%x]\n", __func__, charger->muic_dev);
				muic_notifier_attach_attached_dev(charger->muic_dev);
			}
		}
#endif
		s2mpw01_factory_mode_setting(charger);
	} else if (irq == charger->irq_uart_off) {
		/* uart boot off */
		pr_info("%s: uart boot off irq:%d\n", __func__, irq);
		if (charger->onoff == 1) {
			pr_info("%s: uart boot off notify done\n", __func__);
			s2mpw01_factory_mode_setting(charger);
#if defined(CONFIG_MUIC_NOTIFIER)
			charger->muic_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
		}
	} else if (irq == charger->irq_uart_on) {
		/* uart boot on */
		pr_info("%s: uart boot on irq:%d\n", __func__, irq);
		pr_info("%s: JIG_ID UART ON ( 619K ) connected \n", __func__);
		s2mpw01_factory_mode_setting(charger);
	} else if (irq == charger->irq_usb_off) {
		/* usb boot off */
		pr_info("%s: usb boot off irq:%d\n", __func__, irq);
		if (charger->onoff == 1) {
			pr_info("%s: usb boot off notify done\n", __func__);
			pr_info("%s: JIG_ID USB OFF ( 255K ) connected \n", __func__);
			s2mpw01_factory_mode_setting(charger);
#if defined(CONFIG_MUIC_NOTIFIER)
			charger->muic_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
			muic_notifier_attach_attached_dev(charger->muic_dev);
#endif
		}
	} else if (irq == charger->irq_jigon) {
		schedule_delayed_work(&charger->rid_work, msecs_to_jiffies(200));

		/* jigon */
		pr_info("%s: jigon irq:%d\n", __func__, irq);
	}
#if 0
	else if(irq == charger->irq_fact_leakage) {

		/* fact leakage */
		pr_info("%s: fact leakage irq:%d\n", __func__, irq);
	} else if(irq == charger->irq_uart_cable) {

		/* uart cable */
		pr_info("%s: uart cable irq:%d\n", __func__, irq);
	}
#endif
skip_irq:

	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, s2mpw01_muic_isr,	\
				0, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s: Failed to request s2mpw01 muic IRQ #%d: %d\n",		\
				__func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

static int s2mpw01_muic_irq_init(struct s2mpw01_charger_data *charger)
{
	int ret = 0;

	if (charger->iodev && (charger->iodev->irq_base > 0)) {
		int irq_base = charger->iodev->irq_base;

		/* request MUIC IRQ */
#if 0
		charger->irq_fact_leakage = irq_base + S2MPW01_CHG_IRQ_FACT_LEAKAGE_INT3;
		REQUEST_IRQ(charger->irq_fact_leakage, charger, "muic-fact_leakage");

		charger->irq_uart_cable = irq_base + S2MPW01_CHG_IRQ_UART_CABLE_INT3;
		REQUEST_IRQ(charger->irq_uart_cable, charger, "muic-uart_cable");
#endif
		charger->irq_jigon = irq_base + S2MPW01_CHG_IRQ_JIGON_INT2;
		REQUEST_IRQ(charger->irq_jigon, charger, "muic-jigon");

		charger->irq_usb_off = irq_base + S2MPW01_CHG_IRQ_USB_BOOT_OFF_INT2;
		REQUEST_IRQ(charger->irq_usb_off, charger, "muic-usb_off");

		charger->irq_usb_on = irq_base + S2MPW01_CHG_IRQ_USB_BOOT_ON_INT2;
		REQUEST_IRQ(charger->irq_usb_on, charger, "muic-usb_on");

		charger->irq_uart_off = irq_base + S2MPW01_CHG_IRQ_UART_BOOT_OFF_INT2;
		REQUEST_IRQ(charger->irq_uart_off, charger, "muic-uart_off");

		charger->irq_uart_on = irq_base + S2MPW01_CHG_IRQ_UART_BOOT_ON_INT3;
		REQUEST_IRQ(charger->irq_uart_on, charger, "muic-uart_on");

		charger->irq_acokf = irq_base + S2MPW01_PMIC_IRQ_ACOKBF_INT1;
		REQUEST_IRQ(charger->irq_acokf, charger, "muic-acokf");

		charger->irq_acokr = irq_base + S2MPW01_PMIC_IRQ_ACOKBR_INT1;
		REQUEST_IRQ(charger->irq_acokr, charger, "muic-acokr");
	}

	pr_err("%s:usb_off(%d), usb_on(%d), uart_off(%d), uart_on(%d), jig_on(%d), muic-acokf(%d), muic-acokr(%d)\n",
		__func__, charger->irq_usb_off, charger->irq_usb_on, charger->irq_uart_off, charger->irq_uart_on,
		charger->irq_jigon, charger->irq_acokf, charger->irq_acokr);

	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s: IRQ(%d):%s free done\n",	\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void s2mpw01_muic_free_irqs(struct s2mpw01_charger_data *charger)
{
	pr_info("%s\n", __func__);

	/* free MUIC IRQ */
	FREE_IRQ(charger->irq_uart_off, charger, "muic-uart_off");
	FREE_IRQ(charger->irq_uart_on, charger, "muic-uart_on");
	FREE_IRQ(charger->irq_usb_off, charger, "muic-usb_off");
	FREE_IRQ(charger->irq_usb_on, charger, "muic-usb_on");
	FREE_IRQ(charger->irq_uart_cable, charger, "muic-uart_cable");
	FREE_IRQ(charger->irq_fact_leakage, charger, "muic-fact_leakage");
	FREE_IRQ(charger->irq_jigon, charger, "muic-jigon");
}
#endif

static void s2mpw01_ac_ok_detect(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
		container_of(work, struct s2mpw01_charger_data, ac_ok_work.work);

	pr_err("{DEBUG] %s onoff(%d)\n", __func__, charger->onoff);

	if (charger->onoff == 1) {
		s2mpw01_check_acok(charger);
	}
	wake_unlock(&charger->ac_ok_lock);
}

#ifdef CONFIG_OF
static int s2mpw01_charger_parse_dt(struct device *dev,
		struct s2mpw01_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpw01-charger");
	const u32 *p;
	int ret, i , len;

	/* SC_CTRL8 , SET_VF_VBAT , Battery regulation voltage setting */
	ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);

	ret = of_property_read_u32(np, "battery,topoff_timer_enable",
				&pdata->topoff_timer_enable);
	if (ret)
		pdata->topoff_timer_enable = 0;

	np = of_find_node_by_name(NULL, "sec-battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
		"battery,charger_name", (char const **)&pdata->charger_name);

		ret = of_property_read_u32(np, "battery,full_check_type",
				&pdata->full_check_type);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
				&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n",
						__func__);

		pdata->chg_eoc_dualpath = of_property_read_bool(np,
				"battery,chg_eoc_dualpath");

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current_table =
				kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"battery,input_current_limit", i,
				&pdata->charging_current_table[i].input_current_limit);
			ret = of_property_read_u32_index(np,
				"battery,fast_charging_current", i,
				&pdata->charging_current_table[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
				"battery,full_check_current_1st", i,
				&pdata->charging_current_table[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
				"battery,full_check_current_2nd", i,
				&pdata->charging_current_table[i].full_check_current_2nd);
		}
	}
	pr_info("s2mpw01 charger parse dt retval = %d\n", ret);
	return ret;
}
#else
static int s2mpw01_charger_parse_dt(struct device *dev,
		struct s2mpw01_charger_platform_data *pdata)
{
	return -ENOSYS;
}
#endif
/* if need to set s2mpw01 pdata */
static struct of_device_id s2mpw01_charger_match_table[] = {
	{ .compatible = "samsung,s2mpw01-charger",},
	{},
};

static int s2mpw01_charger_probe(struct platform_device *pdev)
{
	struct s2mpw01_dev *s2mpw01 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw01_platform_data *pdata = dev_get_platdata(s2mpw01->dev);
	struct s2mpw01_charger_data *charger;
	int ret = 0;
	u8 acok_stat = 0, data = 0;
	u8 chg_sts2, chg_sts3;

	pr_info("%s:[BATT] S2MPW01 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->iodev = s2mpw01;
	charger->dev = &pdev->dev;
	charger->client = s2mpw01->charger;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		pr_err("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpw01_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "sec-charger";

	charger->psy_chg.name           = charger->pdata->charger_name;
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = s2mpw01_chg_get_property;
	charger->psy_chg.set_property   = s2mpw01_chg_set_property;
	charger->psy_chg.properties     = s2mpw01_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(s2mpw01_charger_props);

	charger->dev_id = s2mpw01->pmic_rev;

#if defined(CONFIG_MUIC_NOTIFIER)
	charger->muic_dev = ATTACHED_DEV_NONE_MUIC;
#endif
	charger->onoff = 0;

	/* need to check siop level */
	charger->siop_level = 100;
	charger->tx_type = 0;
	charger->retail_mode = 0;
	charger->extreme_mode = 0;

	s2mpw01_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}

	wake_lock_init(&charger->ta_work_lock, WAKE_LOCK_SUSPEND,
		"sec-chg-ta-work");
	wake_lock_init(&charger->ta_pad_lock, WAKE_LOCK_SUSPEND,
		"sec-chg-ta-pad");
	wake_lock_init(&charger->ac_ok_lock, WAKE_LOCK_SUSPEND,
		"sec-chg-ac-ok");

#if 0
	/*
	 * irq request
	 * if you need to add irq , please refer below code.
	 */
	charger->irq_det_bat = pdata->irq_base + S2MPW01_CHG_IRQ_BATDET_INT2;
	ret = request_threaded_irq(charger->irq_det_bat, NULL,
			s2mpw01_det_bat_isr, 0 , "det-bat-in-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw01->dev, "%s: Fail to request det bat in IRQ: %d: %d\n",
					__func__, charger->irq_det_bat, ret);
		goto err_reg_irq;
	}
#endif
	charger->irq_chg = pdata->irq_base + S2MPW01_CHG_IRQ_TOPOFF_INT1;
	ret = request_threaded_irq(charger->irq_chg, NULL,
			s2mpw01_chg_isr, 0 , "chg-irq", charger);
	if (ret < 0) {
		pr_err("%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_chg, ret);
		goto err_power_supply_register;
	}

	charger->irq_tmrout = charger->iodev->irq_base + S2MPW01_CHG_IRQ_TMROUT_INT3;
	ret = request_threaded_irq(charger->irq_tmrout, NULL,
			s2mpw01_tmrout_isr, 0 , "tmrout-irq", charger);
	if (ret < 0) {
		pr_err("%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_tmrout, ret);
		goto err_power_supply_register;
	}

	s2mpw01_test_read(charger->client);

	INIT_DELAYED_WORK(&charger->tx_pad_work, s2mpw01_tx_pad_detect);
	INIT_DELAYED_WORK(&charger->ac_ok_work, s2mpw01_ac_ok_detect);

#if defined(CONFIG_S2MPW01_RID_DETECT)
	INIT_DELAYED_WORK(&charger->rid_work, s2mpw01_muic_rid_check);
	/* charger topoff on/off work */
	INIT_DELAYED_WORK(&charger->ta_work, s2mpw01_ta_detect);

	ret = s2mpw01_muic_irq_init(charger);
	if (ret) {
		pr_err( "[muic] %s: failed to init muic irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	INIT_DELAYED_WORK(&charger->init_work, s2mpw01_muic_init_detect);
	schedule_delayed_work(&charger->init_work, msecs_to_jiffies(3000));

	charger->is_usb_ready = false;
	INIT_DELAYED_WORK(&charger->usb_work, s2mpw01_muic_usb_detect);
	schedule_delayed_work(&charger->usb_work, msecs_to_jiffies(13000));
#endif
	/* initially TOP-OFF interrupt masking */
	s2mpw01_topoff_interrupt_onoff(charger, 0);

	ret = s2mpw01_read_reg(charger->iodev->pmic, 0x41, &charger->chg_en_data);
	if (ret < 0) {
		pr_err("%s: failed to read PM addr 0x41(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	/* factory_mode setting */
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &chg_sts2);
	if (ret < 0) {
		pr_err("{DEBUG] %s : chg status2 read fail\n", __func__);
	}
	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS3, &chg_sts3);
	if (ret < 0) {
		pr_err("{DEBUG] %s : chg status3 read fail\n", __func__);
	}

	if ((chg_sts2 & USB_BOOT_ON_STATUS_MASK) || (chg_sts2 & USB_BOOT_OFF_STATUS_MASK) ||
		(chg_sts3 & UART_BOOT_ON_STATUS_MASK) || (chg_sts2 & UART_BOOT_OFF_STATUS_MASK))
		s2mpw01_factory_mode_setting(charger);

	/* make bit[6] to 0 (if 0x41 is 0, old rev.) */
	if (charger->dev_id < EVT_3) {
		if (charger->chg_en_data == 0)
			charger->chg_en_data = 0x21;
		else
			charger->chg_en_data &= 0xBF;
	}

	ret = s2mpw01_read_reg(charger->iodev->pmic, S2MPW01_PMIC_REG_STATUS1, &acok_stat);
	if (ret < 0) {
		pr_err("%s: failed to read S2MPW01_PMIC_REG_STATUS1(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	data = charger->chg_en_data;
	/* if acok is high, set 1 to bit[6]. if acok is low, set 0 to bit[6] */
	if ((!charger->factory_mode) && (charger->dev_id < EVT_3)) {
		if (acok_stat & ACOK_STATUS_MASK)
			data |= 0x40;
		s2mpw01_write_reg(charger->client, 0x2E, data);
	}
	pr_info("%s:[BATT] S2MPW01 charger driver loaded OK\n", __func__);

	return 0;

fail_init_irq:
#if defined(CONFIG_S2MPW01_RID_DETECT)
	s2mpw01_muic_free_irqs(charger);
#endif
err_power_supply_register:
	wake_lock_destroy(&charger->ac_ok_lock);
	wake_lock_destroy(&charger->ta_pad_lock);
	wake_lock_destroy(&charger->ta_work_lock);
	destroy_workqueue(charger->charger_wqueue);
	power_supply_unregister(&charger->psy_chg);
//	power_supply_unregister(&charger->psy_battery);
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int s2mpw01_charger_remove(struct platform_device *pdev)
{
	struct s2mpw01_charger_data *charger =
		platform_get_drvdata(pdev);

	wake_lock_destroy(&charger->ac_ok_lock);
	wake_lock_destroy(&charger->ta_pad_lock);
	wake_lock_destroy(&charger->ta_work_lock);

	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mpw01_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mpw01_charger_resume(struct device *dev)
{
	struct s2mpw01_charger_data *charger = dev_get_drvdata(dev);
	u8 val;
	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_INT1M, &val);
	pr_info("%s : CHG_INT1 ---> 0x%x \n", __func__, val);
#if 0
	if(charger->iodev->topoff_mask_status > 0)
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_INT1M, 0x00, 0x04);
	else
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_INT1M, 0x04, 0x04);
#endif
	pr_err("[DEBUG]%s: Top-off interrupt Masking : 0x%x, topoff status %d\n", __func__,
		charger->iodev->irq_masks_cur[3], charger->iodev->topoff_mask_status);

	return 0;
}
#else
#define s2mpw01_charger_suspend NULL
#define s2mpw01_charger_resume NULL
#endif

static void s2mpw01_charger_shutdown(struct device *dev)
{
	struct s2mpw01_charger_data *charger = dev_get_drvdata(dev);
	unsigned int stat_val = 0;

	/* ACOK status */
	if (charger->dev_id < EVT_3) {
		stat_val= charger->chg_en_data;
		s2mpw01_write_reg(charger->client, 0x2E, stat_val);
	}
	s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
	s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL2, 0x1 << FAST_CHARGING_CURRENT_SHIFT,
			FAST_CHARGING_CURRENT_MASK);

	s2mpw01_set_additional(charger, 0, 0);

	pr_info("%s: S2MPW01 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mpw01_charger_pm_ops, s2mpw01_charger_suspend,
		s2mpw01_charger_resume);

static struct platform_driver s2mpw01_charger_driver = {
	.driver         = {
		.name   = "s2mpw01-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mpw01_charger_match_table,
		.pm     = &s2mpw01_charger_pm_ops,
		.shutdown = s2mpw01_charger_shutdown,
	},
	.probe          = s2mpw01_charger_probe,
	.remove		= s2mpw01_charger_remove,
};

static int __init s2mpw01_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mpw01_charger_driver);

	return ret;
}
device_initcall(s2mpw01_charger_init);

static void __exit s2mpw01_charger_exit(void)
{
	platform_driver_unregister(&s2mpw01_charger_driver);
}
module_exit(s2mpw01_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MPW01");
