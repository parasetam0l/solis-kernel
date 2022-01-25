/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2624.c
**
** Description:
**     DRV2624 chip driver
**
** =============================================================================
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/miscdevice.h>
#include <linux/drv2624.h>
#include <linux/input.h>

//#define	AUTOCALIBRATION_ENABLE

enum VIBRATOR_CONTROL {
	VIBRATOR_DISABLE = 0,
	VIBRATOR_ENABLE = 1,
};

#define MAX_LEVEL 0xffff
#define MAX_RTP_INPUT 127
#define POWER_SUPPY_VOLTAGE 3000000

static struct drv2624_data *g_DRV2624data = NULL;

static int drv2624_reg_read(struct drv2624_data *pDrv2624data, unsigned char reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(pDrv2624data->mpRegmap, reg, &val);
    
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0x%x error %d\n", __FUNCTION__, reg, ret);
		return ret;
	}
	else
		return val;
}

static int drv2624_reg_write(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned char val)
{
	int ret;

	ret = regmap_write(pDrv2624data->mpRegmap, reg, val);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0x%x, value=0%x error %d\n",
			__FUNCTION__, reg, val, ret);
	}

	return ret;
}

#if 0 /* not using bulk read/write */
static int drv2624_bulk_read(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned int count, u8 *buf)
{
	int ret;
	ret = regmap_bulk_read(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0%x, count=%d error %d\n",
			__FUNCTION__, reg, count, ret);
	}

	return ret;
}

static int drv2624_bulk_write(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned int count, const u8 *buf)
{
	int ret;
	ret = regmap_bulk_write(pDrv2624data->mpRegmap, reg, buf, count);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=0%x, count=%d error %d\n",
			__FUNCTION__, reg, count, ret);
	}

	return ret;
}
#endif
static int drv2624_set_bits(struct drv2624_data *pDrv2624data,
	unsigned char reg, unsigned char mask, unsigned char val)
{
	int ret;
	ret = regmap_update_bits(pDrv2624data->mpRegmap, reg, mask, val);
	if (ret < 0){
		dev_err(pDrv2624data->dev,
			"%s reg=%x, mask=0x%x, value=0x%x error %d\n",
			__FUNCTION__, reg, mask, val, ret);
	}

	return ret;
}

static int drv2624_set_go_bit(struct drv2624_data *pDrv2624data, unsigned char val)
{
	return drv2624_reg_write(pDrv2624data, DRV2624_REG_GO, (val&0x01));
}

static void drv2624_change_mode(struct drv2624_data *pDrv2624data, unsigned char work_mode)
{
	drv2624_set_bits(pDrv2624data, DRV2624_REG_MODE, DRV2624MODE_MASK , work_mode);
}

static int vib_run(struct drv2624_data *pDrv2624data, bool en)
{
	int ret = 0;
	unsigned char val;

	pr_info("[VIB] %s %s [level]%d\n", __func__, en ? "on" : "off", pDrv2624data->level);

	if (pDrv2624data == NULL) {
		pr_info("[VIB] the motor is not ready!!!");
		return -EINVAL;
	}

	if (en) {
		val = MAX_RTP_INPUT * pDrv2624data->level / MAX_LEVEL;
		ret = drv2624_reg_write(pDrv2624data, DRV2624_REG_RTP_INPUT, val);

		if (ret < 0) {
			pr_err("[VIB] %s RTP input fail %d", __func__, ret);
			return ret;
		}

		drv2624_set_go_bit(pDrv2624data, GO);

		if (pDrv2624data->running)
			return ret;
		pDrv2624data->running = true;
	} else {
		drv2624_set_go_bit(pDrv2624data, STOP);
		if (!pDrv2624data->running)
			return ret;
		pDrv2624data->running = false;
	}

	return ret;
}

static void vibrator_work_routine(struct work_struct *work)
{
	struct drv2624_data *pDrv2624data = container_of(work,
						       struct drv2624_data,
						       vibrator_work);

	if (pDrv2624data->level)
		vib_run(pDrv2624data, (bool)VIBRATOR_ENABLE);
	else
		vib_run(pDrv2624data, (bool)VIBRATOR_DISABLE);
}

static int drv2624_haptic_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct drv2624_data *pDrv2624data = input_get_drvdata(input);
	__u16 level = effect->u.rumble.strong_magnitude;

	pr_info("[VIB] %s [level]%d\n", __func__, level);

	if (level) {
		if(!(pDrv2624data->running && level == pDrv2624data->level)) {
			pDrv2624data->level = level;
			schedule_work(&pDrv2624data->vibrator_work);
		}
	}
	else {
		pDrv2624data->level = 0;
		schedule_work(&pDrv2624data->vibrator_work);
	}

	return 0;
}

static void drv2624_haptic_close(struct input_dev *input)
{
	struct drv2624_data *pDrv2624data = input_get_drvdata(input);

	if (pDrv2624data != NULL) {
		cancel_work_sync(&pDrv2624data->vibrator_work);
		if (pDrv2624data->running) {
			pDrv2624data->level = 0;
			schedule_work(&pDrv2624data->vibrator_work);
		}
	}
}
 
static int Haptics_init(struct drv2624_data *pDrv2624data)
{
    int ret = 0;

	struct input_dev *input_dev;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(pDrv2624data->dev, "[VIB] unable to allocate input device \n");
		return -ENODEV;
	}
	input_dev->name = "drv2624_haptic";
	input_dev->dev.parent = pDrv2624data->dev;
	input_dev->close = drv2624_haptic_close;
	input_set_capability(input_dev, EV_FF, FF_RUMBLE);
	ret = input_ff_create_memless(input_dev, NULL,
		drv2624_haptic_play);
	if (ret < 0) {
		dev_err(pDrv2624data->dev, "[VIB] input_ff_create_memless() failed: %d\n", ret);
		goto err_free_input;
	}
	ret = input_register_device(input_dev);
	if (ret < 0) {
		dev_err(pDrv2624data->dev,
			"[VIB] couldn't register input device: %d\n",
			ret);
		goto err_destroy_ff;
	}
	input_set_drvdata(input_dev, pDrv2624data);

    INIT_WORK(&pDrv2624data->vibrator_work, vibrator_work_routine);
    wake_lock_init(&pDrv2624data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
    mutex_init(&pDrv2624data->lock);

    return 0;
err_destroy_ff:
	input_ff_destroy(input_dev);
err_free_input:
	input_free_device(input_dev);

	return ret;
}

static int dev_man_calibrate(struct drv2624_data *pDrv2624data)
{
	/* close loop */
	drv2624_reg_write(pDrv2624data, 0x21, 17);
	drv2624_reg_write(pDrv2624data, 0x22, 163);
	drv2624_reg_write(pDrv2624data, 0x23, 55);
	drv2624_reg_write(pDrv2624data, 0x24, 100);
	drv2624_reg_write(pDrv2624data, 0x25, 128);
	drv2624_reg_write(pDrv2624data, 0x26, 0);
	drv2624_reg_write(pDrv2624data, 0x27, 151);
	drv2624_reg_write(pDrv2624data, 0x28, 17);
	drv2624_reg_write(pDrv2624data, 0x29, 12);

	return 0;
}

static void dev_init_platform_data(struct drv2624_data *pDrv2624data)
{
	struct drv2624_platform_data *pDrv2624Platdata = &pDrv2624data->msPlatData;
	struct actuator_data actuator = pDrv2624Platdata->msActuator;
	unsigned char value_temp = 0;
	unsigned char mask_temp = 0;

	drv2624_set_bits(pDrv2624data,
		DRV2624_REG_INT_ENABLE, INT_MASK_ALL, INT_ENABLE_ALL);

	drv2624_set_bits(pDrv2624data,
		DRV2624_REG_MODE, PINFUNC_MASK, (PINFUNC_INT<<PINFUNC_SHIFT));

	if((actuator.meActuatorType == ERM)||
		(actuator.meActuatorType == LRA)){
		mask_temp |= ACTUATOR_MASK;
		value_temp |= (actuator.meActuatorType << ACTUATOR_SHIFT);
	}

	if((pDrv2624Platdata->meLoop == CLOSE_LOOP)||
		(pDrv2624Platdata->meLoop == OPEN_LOOP)){
		mask_temp |= LOOP_MASK;
		value_temp |= (pDrv2624Platdata->meLoop << LOOP_SHIFT);
	}

	if(value_temp != 0){
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_CONTROL1,
			mask_temp|AUTOBRK_OK_MASK, value_temp|AUTOBRK_OK_ENABLE);
	}

	if(actuator.mnRatedVoltage != 0){
		drv2624_reg_write(pDrv2624data,
			DRV2624_REG_RATED_VOLTAGE, actuator.mnRatedVoltage);
	}else{
		dev_err(pDrv2624data->dev,
			"%s, ERROR Rated ZERO\n", __FUNCTION__);
	}

	if(actuator.mnOverDriveClampVoltage != 0){
		drv2624_reg_write(pDrv2624data,
			DRV2624_REG_OVERDRIVE_CLAMP, actuator.mnOverDriveClampVoltage);
	}else{
		dev_err(pDrv2624data->dev,
			"%s, ERROR OverDriveVol ZERO\n", __FUNCTION__);
	}

	if(actuator.meActuatorType == LRA){
		unsigned char DriveTime = 5*(1000 - actuator.mnLRAFreq)/actuator.mnLRAFreq;
		unsigned short openLoopPeriod =
			(unsigned short)((unsigned int)1000000000 / (24619 * actuator.mnLRAFreq));

		if(actuator.mnLRAFreq < 125)
			DriveTime |= (MINFREQ_SEL_45HZ << MINFREQ_SEL_SHIFT);
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_DRIVE_TIME,
			DRIVE_TIME_MASK | MINFREQ_SEL_MASK, DriveTime);
		drv2624_set_bits(pDrv2624data,
			DRV2624_REG_OL_PERIOD_H, 0x03, (openLoopPeriod&0x0300)>>8);
		drv2624_reg_write(pDrv2624data,
			DRV2624_REG_OL_PERIOD_L, (openLoopPeriod&0x00ff));

		dev_info(pDrv2624data->dev,
			"%s, LRA = %d, DriveTime=0x%x\n",
			__FUNCTION__, actuator.mnLRAFreq, DriveTime);
	}

	/* only use RTP mode */
	drv2624_change_mode(pDrv2624data, MODE_RTP);
	dev_man_calibrate(pDrv2624data);
}

#if 0 /* dont use irq */
static irqreturn_t drv2624_irq_handler(int irq, void *dev_id)
{
	struct drv2624_data *pDrv2624data = (struct drv2624_data *)dev_id;

	pDrv2624data->mnIntStatus =
		drv2624_reg_read(pDrv2624data,DRV2624_REG_STATUS);
	if(pDrv2624data->mnIntStatus & INT_MASK){
		pDrv2624data->mnWorkMode |= WORK_IRQ;
		schedule_work(&pDrv2624data->vibrator_work);
	}
	return IRQ_HANDLED;
}
#endif

#ifdef AUTOCALIBRATION_ENABLE
static int dev_auto_calibrate(struct drv2624_data *pDrv2624data)
{
	wake_lock(&pDrv2624data->wklock);
	pDrv2624data->mnVibratorPlaying = YES;
	drv2624_change_mode(pDrv2624data, MODE_CALIBRATION);
	drv2624_set_go_bit(pDrv2624data, GO);

	return 0;
}
#endif

#if defined(CONFIG_OF)
static int of_drv2624_dt(struct drv2624_platform_data *pdata)
{
//	struct device_node *np_haptic;
//	const char *temp_str;
	int ret = 0;

	/* close loop  */
	pdata->mnGpioNRST = 0;
	pdata->mnGpioINT = 0;
	pdata->meLoop = CLOSE_LOOP;
	pdata->msActuator.meActuatorType = LRA;
	pdata->msActuator.mnRatedVoltage = 127;
	pdata->msActuator.mnOverDriveClampVoltage = 155;
	pdata->msActuator.mnLRAFreq = 178;

/*
	np_haptic = of_find_node_by_path("/i2c@13880000/drv2624@25");
	if (np_haptic == NULL) {
		pr_err("[VIB] error to get dt node\n");
		return -EINVAL;
	}

	of_property_read_u32(np_haptic, "drv2624,mngpionrst", &pdata->mnGpioNRST);
	of_property_read_u32(np_haptic, "drv2624,mngpioint", &pdata->mnGpioINT);
	of_property_read_u16(np_haptic, "drv2624,meloop", &pdata->meLoop);
	of_property_read_u16(np_haptic, "drv2624,meactuator-type", &pdata->msActuator.meActuatorType);
	of_property_read_u16(np_haptic, "drv2624,mnratedvoltage", &pdata->msActuator.mnRatedVoltage);
	of_property_read_u16(np_haptic, "drv2624,mnoverdriveclampvoltage", &pdata->msActuator.mnOverDriveClampVoltage);
	of_property_read_u16(np_haptic, "drv2624,mnlrafreq", &pdata->msActuator.mnLRAFreq);
*/
	pr_info("[VIB] mnGpioNRST = %d\n", pdata->mnGpioNRST);
	pr_info("[VIB] mnGpioINT = %d\n", pdata->mnGpioINT);
	pr_info("[VIB] meLoop = %d\n", pdata->meLoop);
	pr_info("[VIB] msActuator.meActuatorType = %d\n", pdata->msActuator.meActuatorType);
	pr_info("[VIB] msActuator.mnRatedVoltage = %d\n", pdata->msActuator.mnRatedVoltage);
	pr_info("[VIB] msActuator.mnOverDriveClampVoltage = %d\n", pdata->msActuator.mnOverDriveClampVoltage);
	pr_info("[VIB] msActuator.mnLRAFreq = %d\n", pdata->msActuator.mnLRAFreq);

	return ret;
}
#endif /* CONFIG_OF */

static struct regmap_config drv2624_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static int drv2624_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct drv2624_data *pDrv2624data;
	struct drv2624_platform_data *pDrv2624Platdata;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		dev_err(&client->dev, "%s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	/* platform_data init */
	if(client->dev.of_node) {
		pDrv2624Platdata = devm_kzalloc(&client->dev, sizeof(struct drv2624_platform_data), GFP_KERNEL);
		if (!pDrv2624Platdata) {
			dev_err(&client->dev, "[VIB] unable to allocate pdata memory\n");
			return -ENOMEM;
		}
		err = of_drv2624_dt(pDrv2624Platdata);
		if (err < 0) {
			dev_err(&client->dev, "[VIB] Failed to read vibrator DT %d\n", err);
			return -ENODEV;
		}
	} else
		pDrv2624Platdata = dev_get_platdata(&client->dev);

	pDrv2624data = devm_kzalloc(&client->dev, sizeof(struct drv2624_data), GFP_KERNEL);
	if (pDrv2624data == NULL){
		dev_err(&client->dev, "%s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pDrv2624data->dev = &client->dev;
	pDrv2624data->mpRegmap = devm_regmap_init_i2c(client, &drv2624_i2c_regmap);
	if (IS_ERR(pDrv2624data->mpRegmap)) {
		err = PTR_ERR(pDrv2624data->mpRegmap);
		dev_err(pDrv2624data->dev,
			"%s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	pDrv2624data->regulator
			= devm_regulator_get(pDrv2624data->dev, "vdd_ldo18");
	if (IS_ERR(pDrv2624data->regulator)) {
		pr_info("[VIB] Failed to get moter power supply.\n");
		return -EFAULT;
	}
	regulator_set_voltage(pDrv2624data->regulator, POWER_SUPPY_VOLTAGE, POWER_SUPPY_VOLTAGE);


	memcpy(&pDrv2624data->msPlatData, pDrv2624Platdata, sizeof(struct drv2624_platform_data));
	i2c_set_clientdata(client,pDrv2624data);

#if 0 /* NRST is high */
	if(pDrv2624data->msPlatData.mnGpioNRST){
		err = gpio_request(pDrv2624data->msPlatData.mnGpioNRST,HAPTICS_DEVICE_NAME"NRST");
		if(err < 0){
			dev_err(pDrv2624data->dev,
				"%s: GPIO %d request NRST error\n",
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioNRST);
			return err;
		}

		gpio_direction_output(pDrv2624data->msPlatData.mnGpioNRST, 0);
		udelay(1000);
		gpio_direction_output(pDrv2624data->msPlatData.mnGpioNRST, 1);
		udelay(500);
	}
#endif

	err = drv2624_reg_read(pDrv2624data, DRV2624_REG_ID);
	if(err < 0){
		dev_err(pDrv2624data->dev,
			"%s, i2c bus fail (%d)\n", __FUNCTION__, err);
		goto exit_gpio_request_failed;
	}else{
		dev_info(pDrv2624data->dev,
			"%s, ID status (0x%x)\n", __FUNCTION__, err);
		pDrv2624data->mnDeviceID = err;
	}

	/* skip ID check */
#if 0
	if(pDrv2624data->mnDeviceID != DRV2624_ID){
		dev_err(pDrv2624data->dev,
			"%s, device_id(%d) fail\n",
			__FUNCTION__, pDrv2624data->mnDeviceID);
		goto exit_gpio_request_failed;
	}
#endif

	dev_init_platform_data(pDrv2624data);

#if 0 /* INT is ground */
	if(pDrv2624data->msPlatData.mnGpioINT){
		err = gpio_request(pDrv2624data->msPlatData.mnGpioINT,HAPTICS_DEVICE_NAME"INT");
		if(err < 0){
			dev_err(pDrv2624data->dev,
				"%s: GPIO %d request INT error\n",
				__FUNCTION__, pDrv2624data->msPlatData.mnGpioINT);
			goto exit_gpio_request_failed;
		}

		gpio_direction_input(pDrv2624data->msPlatData.mnGpioINT);

		err = request_threaded_irq(client->irq, drv2624_irq_handler,
				NULL, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name, pDrv2624data);

		if (err < 0) {
			dev_err(pDrv2624data->dev,
				"%s: request_irq failed\n", __FUNCTION__);
			goto exit_gpio_request_failed;
		}
	}
#endif

	g_DRV2624data = pDrv2624data;

    Haptics_init(pDrv2624data);

#if 0 /* not using ram sequence(dont need firmware) */
	err = request_firmware_nowait(THIS_MODULE,
		FW_ACTION_HOTPLUG,	"drv2624.bin",	&(client->dev),
		GFP_KERNEL, pDrv2624data, HapticsFirmwareLoad);
#endif

#ifdef AUTOCALIBRATION_ENABLE
	err = dev_auto_calibrate(pDrv2624data);
	if(err < 0){
		dev_err(pDrv2624data->dev,
			"%s, ERROR, calibration fail\n",
			__FUNCTION__);
	}
#endif

    dev_info(pDrv2624data->dev,
		"drv2624 probe succeeded\n");

    return 0;

exit_gpio_request_failed:
	if(pDrv2624data->msPlatData.mnGpioNRST){
		gpio_free(pDrv2624data->msPlatData.mnGpioNRST);
	}

	if(pDrv2624data->msPlatData.mnGpioINT){
		gpio_free(pDrv2624data->msPlatData.mnGpioINT);
	}

    dev_err(pDrv2624data->dev,
		"%s failed, err=%d\n",
		__FUNCTION__, err);
	return err;
}

static int drv2624_remove(struct i2c_client* client)
{
	struct drv2624_data *pDrv2624data = i2c_get_clientdata(client);

	if (pDrv2624data != NULL) {
		if(pDrv2624data->msPlatData.mnGpioNRST)
			gpio_free(pDrv2624data->msPlatData.mnGpioNRST);

		if(pDrv2624data->msPlatData.mnGpioINT)
			gpio_free(pDrv2624data->msPlatData.mnGpioINT);
	}

    return 0;
}

static int drv2624_suspend(struct i2c_client* client,
			pm_message_t state)
{
	struct drv2624_data *pDrv2624data = i2c_get_clientdata(client);

	pr_info("[VIB] %s\n", __func__);

	if (pDrv2624data != NULL) {
		/* TODO : i2c power supply power control */
		cancel_work_sync(&pDrv2624data->vibrator_work);
		if (pDrv2624data->running) {
			pDrv2624data->level = 0;
			schedule_work(&pDrv2624data->vibrator_work);
		}
	}

	return 0;
}

static int drv2624_resume(struct i2c_client* client)
{
	struct drv2624_data *pDrv2624data = i2c_get_clientdata(client);

	pr_info("[VIB] %s\n", __func__);

	if (pDrv2624data != NULL) {
		/* TODO : i2c power supply power control */
		if (pDrv2624data->running) {
			pDrv2624data->level = 0;
			schedule_work(&pDrv2624data->vibrator_work);
		}
	}

	return 0;
}

static void drv2624_shutdown(struct i2c_client* client)
{
	pr_info("[VIB] %s\n", __func__);

	/* TODO : i2c power supply power control */

	return;
}

#if defined(CONFIG_OF)
static struct of_device_id haptic_dt_ids[] = {
	{ .compatible = "drv2624" },
	{ },
};
MODULE_DEVICE_TABLE(of, haptic_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_device_id drv2624_id_table[] =
{
    { HAPTICS_DEVICE_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, drv2624_id_table);

static struct i2c_driver drv2624_driver =
{
    .driver = {
        .name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = haptic_dt_ids,
#endif /* CONFIG_OF */
    },
    .id_table = drv2624_id_table,
    .probe = drv2624_probe,
    .remove = drv2624_remove,
    .suspend	= drv2624_suspend,
    .resume		= drv2624_resume,
    .shutdown	= drv2624_shutdown,
};

static int __init drv2624_init(void)
{
	pr_info("[VIB] %s\n", __func__);
	return i2c_add_driver(&drv2624_driver);
}

static void __exit drv2624_exit(void)
{
	i2c_del_driver(&drv2624_driver);
}

module_init(drv2624_init);
module_exit(drv2624_exit);

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
