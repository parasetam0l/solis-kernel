/*
** =============================================================================
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
**     zh915.c
**
** Description:
**     ZH915 chip driver
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
#include <linux/of_gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/zh915.h>
#include <linux/input.h>
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/suspend.h>
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
static struct device *sec_motor;
#endif

static struct zh915_data *g_ZH915data = NULL;
extern unsigned int system_rev;
static bool is_suspend = false;
static int max_rtp_input;

static int zh915_reg_read(struct zh915_data *pZh915data, unsigned char reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(pZh915data->mpRegmap, reg, &val);
    
	if (ret < 0){
		dev_err(pZh915data->dev,
			"[VIB] %s reg=0x%x error %d\n", __FUNCTION__, reg, ret);
		return ret;
	}
	else
		return val;
}

static int zh915_reg_write(struct zh915_data *pZh915data,
	unsigned char reg, unsigned char val)
{
	int ret;

	ret = regmap_write(pZh915data->mpRegmap, reg, val);
	if (ret < 0){
		dev_err(pZh915data->dev,
			"[VIB] %s reg=0x%x, value=0%x error %d\n",
			__FUNCTION__, reg, val, ret);
	}

	return ret;
}

static int zh915_set_bits(struct zh915_data *pZh915data,
	unsigned char reg, unsigned char mask, unsigned char val)
{
	int ret;
	ret = regmap_update_bits(pZh915data->mpRegmap, reg, mask, val);
	if (ret < 0){
		dev_err(pZh915data->dev,
			"[VIB] %s reg=%x, mask=0x%x, value=0x%x error %d\n",
			__FUNCTION__, reg, mask, val, ret);
	}

	return ret;
}

static void vibrator_work_routine(struct work_struct *work)
{
	int err;
	unsigned char val;
	struct zh915_data *pZh915data = container_of(work,
						       struct zh915_data,
						       vibrator_work);

	if (pZh915data == NULL) {
		pr_info("[VIB] %s zh915_data NULL error\n", __func__);
		return;
	}

	mutex_lock(&pZh915data->lock);
	if (is_suspend) goto out;
	if (pZh915data->level) {
		/* TODO : removed W/A before PVR */
		if (system_rev == 0x09)
			val = 69;
		else
			val = max_rtp_input * pZh915data->level / MAX_LEVEL;
		err = zh915_reg_write(pZh915data, ZH915_REG_STRENGTH_WRITE, val);
		if (err < 0) {
			dev_err(pZh915data->dev, "[VIB] %s STRENGTH REG write %d fail %d\n", __func__, val, err);
			goto out;
		}
		if (!pZh915data->running) {
			if (pZh915data->gpio_en > 0)
				gpio_set_value(pZh915data->gpio_en, 1);

			err = zh915_reg_write(pZh915data, ZH915_REG_MODE, MODE_I2C);
			if (err < 0) {
				dev_err(pZh915data->dev, "[VIB] %s MODE_I2C REG write fail %d\n", __func__, err);
				goto out;
			}
			pZh915data->running = true;
		}
		dev_info(pZh915data->dev, "[VIB] %s Run [Strength] %d\n", __func__, val);
	}
	else if (pZh915data->running) {
		err = zh915_reg_write(pZh915data, ZH915_REG_MODE, MODE_STOP);
		if (err < 0) {
			dev_err(pZh915data->dev, "[VIB] %s MODE_STOP write fail %d\n", __func__, err);
			if (pZh915data->gpio_en > 0) {
				gpio_set_value(pZh915data->gpio_en, 0);
				pZh915data->running = false;
				dev_err(pZh915data->dev, "[VIB] %s emergency stop\n", __func__);
			}
			goto out;
		}
		if (pZh915data->gpio_en > 0) {
			if (pZh915data->msPlatData.break_mode)
				queue_work(system_long_wq, &pZh915data->delay_en_off);
			else
				gpio_set_value(pZh915data->gpio_en, 0);
		}
		pZh915data->running = false;
		pZh915data->last_motor_off = CURRENT_TIME;
		dev_info(pZh915data->dev, "[VIB] %s Stop\n", __func__);
	}

out:
	mutex_unlock(&pZh915data->lock);
}

#define AUTO_BRAKE_DELAY_MTIME	150
static void zh915_delay_en_off(struct work_struct *work)
{
	struct zh915_data *pZh915data = container_of(work,
						       struct zh915_data,
						       delay_en_off);
	struct timespec tm, elapsed;

	msleep(AUTO_BRAKE_DELAY_MTIME);
	mutex_lock(&pZh915data->lock);
	if (is_suspend) goto out;
	if (!pZh915data->running) {
		tm = CURRENT_TIME;
		elapsed = timespec_sub(tm, pZh915data->last_motor_off);
		if (elapsed.tv_nsec >= AUTO_BRAKE_DELAY_MTIME * 1000000)
			gpio_set_value(pZh915data->gpio_en, 0);
	}
out:
	mutex_unlock(&pZh915data->lock);
}

static int zh915_haptic_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct zh915_data *pZh915data = input_get_drvdata(input);
	__u16 level = effect->u.rumble.strong_magnitude;

	if (level) {
		pZh915data->level = level;
		queue_work(system_highpri_wq, &pZh915data->vibrator_work);
	}
	else {
		pZh915data->level = 0;
		queue_work(system_highpri_wq, &pZh915data->vibrator_work);
	}

	return 0;
}

static int zh915_pm_notifier(struct notifier_block *notifier,
                                       unsigned long pm_event, void *v)
{
	int err;
	unsigned char val;
	struct zh915_data *pZh915data = container_of(notifier,
						       struct zh915_data,
						       zh915_pm_nb);
    switch (pm_event) {
    case PM_SUSPEND_PREPARE:
		if (pZh915data != NULL) {
			mutex_lock(&pZh915data->lock);
			err = zh915_reg_write(pZh915data, ZH915_REG_MODE, MODE_STOP);
			if (err < 0) {
				dev_err(pZh915data->dev, "[VIB] %s MODE_STOP write fail %d\n", __func__, err);
				goto out_err;
			}
			if (pZh915data->gpio_en > 0 && gpio_get_value(pZh915data->gpio_en)) {
				if (pZh915data->msPlatData.break_mode) {
					msleep(AUTO_BRAKE_DELAY_MTIME);
					gpio_set_value(pZh915data->gpio_en, 0);
				} else
					gpio_set_value(pZh915data->gpio_en, 0);
			}
			pZh915data->running = false;
			dev_info(pZh915data->dev, "[VIB] %s Stop\n", __func__);
			is_suspend = true;
			mutex_unlock(&pZh915data->lock);
		}
		break;
   case PM_POST_SUSPEND:
		if (pZh915data != NULL) {
			mutex_lock(&pZh915data->lock);
			is_suspend = false;
			if (pZh915data->level) {
				/* TODO : removed W/A before PVR */
				if (system_rev == 0x09)
					val = 69;
				else
					val = max_rtp_input * pZh915data->level / MAX_LEVEL;
				err = zh915_reg_write(pZh915data, ZH915_REG_STRENGTH_WRITE, val);
				if (err < 0) {
					dev_err(pZh915data->dev, "[VIB] %s STRENGTH REG write %d fail %d\n", __func__, val, err);
					goto out_err;
				}

				if (!pZh915data->running) {
					if (pZh915data->gpio_en > 0)
						gpio_set_value(pZh915data->gpio_en, 1);

					err = zh915_reg_write(pZh915data, ZH915_REG_MODE, MODE_I2C);
					if (err < 0) {
						dev_err(pZh915data->dev, "[VIB] %s MODE_I2C REG write fail %d\n", __func__, err);
						goto out_err;
					}
					pZh915data->running = true;
				}
				dev_info(pZh915data->dev, "[VIB] %s Run [Strength]%d\n", __func__, val);
			}
			mutex_unlock(&pZh915data->lock);
		}
        break;
   }

        return NOTIFY_OK;
out_err:
		mutex_unlock(&pZh915data->lock);
        return NOTIFY_BAD;
}

static int Haptics_init(struct zh915_data *pZh915data)
{
    int ret = 0;

	struct input_dev *input_dev;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(pZh915data->dev, "[VIB] unable to allocate input device \n");
		return -ENODEV;
	}
	input_dev->name = "zh915_haptic";
	input_dev->dev.parent = pZh915data->dev;
	input_set_capability(input_dev, EV_FF, FF_RUMBLE);
	ret = input_ff_create_memless(input_dev, NULL,
		zh915_haptic_play);
	if (ret < 0) {
		dev_err(pZh915data->dev, "[VIB] input_ff_create_memless() failed: %d\n", ret);
		goto err_free_input;
	}
	ret = input_register_device(input_dev);
	if (ret < 0) {
		dev_err(pZh915data->dev,
			"[VIB] couldn't register input device: %d\n",
			ret);
		goto err_destroy_ff;
	}
	input_set_drvdata(input_dev, pZh915data);

    INIT_WORK(&pZh915data->vibrator_work, vibrator_work_routine);
	INIT_WORK(&pZh915data->delay_en_off, zh915_delay_en_off);
    wake_lock_init(&pZh915data->wklock, WAKE_LOCK_SUSPEND, "vibrator");
    mutex_init(&pZh915data->lock);

	pZh915data->zh915_pm_nb.notifier_call = zh915_pm_notifier;
	register_pm_notifier(&pZh915data->zh915_pm_nb);

    return 0;
err_destroy_ff:
	input_ff_destroy(input_dev);
err_free_input:
	input_free_device(input_dev);

	return ret;
}

static int dev_init_platform_data(struct zh915_data *pZh915data)
{
	int ret = 0, i;
	struct zh915_platform_data *pZh915Platdata = &pZh915data->msPlatData;
	unsigned char value_tmp = 0;

	value_tmp = ((25000000 / (2 * pZh915Platdata->mdata.resonance_freq)) - 50000) / 512;
	ret = zh915_reg_write(pZh915data,
			ZH915_REG_RESONANCE_FREQ, value_tmp);
	if (ret < 0) {
		dev_err(pZh915data->dev, "[VIB] %s RESONANCE FREQ REG write %d fail %d\n", __func__, value_tmp, ret);
		ret = 0;
	}

	value_tmp = 0;
	value_tmp |= (pZh915Platdata->meLoop << LOOP_SHIFT );
	value_tmp |= (pZh915Platdata->mdata.motor_type << MOTOR_TYPE_SHIFT );
	value_tmp |= ((pZh915Platdata->break_mode ? 0x01 : 0x00) << BREAK_SHIFT);
 
	ret = zh915_set_bits(pZh915data,
		ZH915_REG_CONTROL, ZH915_REG_CONTROL_MASK, value_tmp);
	if (ret < 0) {
		dev_err(pZh915data->dev, "[VIB] %s CONTROL REG write %d fail %d\n", __func__, value_tmp, ret);
		return ret;
	}

	if (pZh915Platdata->count_init_regs > 0) {
		for (i = 0; i < pZh915Platdata->count_init_regs; i++)
			zh915_reg_write(pZh915data, pZh915Platdata->init_regs[i].addr, pZh915Platdata->init_regs[i].data);
	}

	return ret;
}

#if defined(CONFIG_OF)
static int of_zh915_dt(struct i2c_client* client, struct zh915_platform_data *pdata)
{
	int err;
	int ret = 0;
	const char *motor_name;
	int i, val = 0;

	err = of_property_read_string(client->dev.of_node,"zh915,motor-name", &motor_name);
	if (err < 0) {
		dev_err(&client->dev, "[VIB] %s No motor name\n",__func__);
		return -ENODEV;
	}

	for (i = 0; i < ARRAY_SIZE(init_mdata); i++) {
		if (!strcmp(init_mdata[i].motor_name, motor_name)) {
			memcpy(&pdata->mdata, &init_mdata[i], sizeof(struct motor_data));
			break;
		}
	}

	if (i == ARRAY_SIZE(init_mdata)) {
		dev_err(&client->dev, "[VIB] %s motor name match fail\n",__func__);
		return -ENODEV;
	}

	of_property_read_u32(client->dev.of_node,"zh915,loop-type", &val);
	if (val)
		pdata->meLoop = OPEN_LOOP;
	else
		pdata->meLoop = CLOSE_LOOP;

	pdata->break_mode = of_property_read_bool(client->dev.of_node,"zh915,break-on");

	pdata->count_init_regs = of_property_count_u32_elems(client->dev.of_node,"zh915,regs-init");
	if (pdata->count_init_regs > 0) {
		pdata->init_regs = devm_kzalloc(&client->dev, sizeof(u32) * pdata->count_init_regs, GFP_KERNEL);
		err = of_property_read_u32_array(client->dev.of_node,"zh915,regs-init",
								(u32 *)pdata->init_regs, pdata->count_init_regs);
		if (err < 0) {
			dev_err(&client->dev, "[VIB] %s regs-init fail %d\n", __func__, err);
			return -ENODEV;
		}
		pdata->count_init_regs /= 2;
	}

	pdata->gpio_en = of_get_named_gpio(client->dev.of_node, "zh915,motor_en", 0);

	of_property_read_string(client->dev.of_node,"zh915,regulator-name", &pdata->regulator_name);

	err = of_property_read_u32(client->dev.of_node,"zh915,max-rtp-input", &max_rtp_input);
	if (err < 0)
		max_rtp_input = MAX_RTP_INPUT;
	dev_err(&client->dev, "[VIB] %s :RTP_INPUT %d :%d\n", __func__, err, max_rtp_input);

	return ret;
}
#endif /* CONFIG_OF */

static struct regmap_config zh915_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static unsigned char zh915_debugfs_addr;

static ssize_t read_zh915_dump_regs(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	static char buf[PAGE_SIZE];

	unsigned int val;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		val = zh915_reg_read(g_ZH915data, 0x00);
		ret += snprintf(buf + ret, sizeof(buf), "0x00 = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x01);
		ret += snprintf(buf + ret, sizeof(buf), "0x01 = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x03);
		ret += snprintf(buf + ret, sizeof(buf), "0x03 = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x04);
		ret += snprintf(buf + ret, sizeof(buf), "0x04 = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x0C);
		ret += snprintf(buf + ret, sizeof(buf), "0x0C = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x2A);
		ret += snprintf(buf + ret, sizeof(buf), "0x2A = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x2B);
		ret += snprintf(buf + ret, sizeof(buf), "0x2B = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x2E);
		ret += snprintf(buf + ret, sizeof(buf), "0x2E = 0x%x\n", val);
		val = zh915_reg_read(g_ZH915data, 0x2F);
		ret += snprintf(buf + ret, sizeof(buf), "0x2F = 0x%x\n", val);
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			return -EFAULT;
		}
		*ppos += ret;
	}
	return ret;
}

static const struct file_operations zh915_dump_regs_fops = {
	.read = read_zh915_dump_regs,
};

static int zh915_debugfs_data_fops_get(void *data, u64 * val)
{
	*val = zh915_reg_read(g_ZH915data, zh915_debugfs_addr);

	return 0;
}

static int zh915_debugfs_data_fops_set(void *data, u64 val)
{
	zh915_reg_write(g_ZH915data, zh915_debugfs_addr, (unsigned char)val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(zh915_debugfs_data_fops,
			zh915_debugfs_data_fops_get, zh915_debugfs_data_fops_set, "%llx\n");

static int zh915_debugfs_enable_get(void *data, u64 * val)
{
	*val = gpio_get_value(g_ZH915data->gpio_en);

	return 0;
}

static int zh915_debugfs_enable_set(void *data, u64 val)
{
	if (val)
		gpio_set_value(g_ZH915data->gpio_en, 1);
	else
		gpio_set_value(g_ZH915data->gpio_en, 0);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(zh915_debugfs_enable,
			zh915_debugfs_enable_get, zh915_debugfs_enable_set, "%llx\n");

int zh915_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_dir("zh915", NULL);

	debugfs_create_file("dump_regs", S_IRUSR | S_IWUSR, d, NULL, &zh915_dump_regs_fops);
	debugfs_create_u8("addr", S_IRUSR | S_IWUSR, d, &zh915_debugfs_addr);
	debugfs_create_file("data", S_IRUSR | S_IWUSR, d, NULL, &zh915_debugfs_data_fops);
	if (g_ZH915data->gpio_en > 0)
		debugfs_create_file("enable", S_IRUSR | S_IWUSR, d, NULL, &zh915_debugfs_enable);

	return 0;
}

#ifdef CONFIG_SEC_SYSFS
static ssize_t zh915_check_i2c(struct device *dev, struct device_attribute *attr,
		char *buf) {
	int err;

	err = zh915_reg_read(g_ZH915data, ZH915_REG_ID);
	if(err < 0)
		return snprintf(buf, 20, "NG");

	return snprintf(buf, 20, "OK");
}

static DEVICE_ATTR(check_i2c, S_IRUGO, zh915_check_i2c, NULL);

static struct attribute *sec_motor_attrs[] = {
	&dev_attr_check_i2c.attr,
	NULL
};
static const struct attribute_group sec_motor_attr_group = {
	.attrs = sec_motor_attrs,
};

static int sec_motor_init(void) {
	int err;

	sec_motor = sec_device_create(NULL, "motor");
	if (IS_ERR(sec_motor)) {
		pr_err("[VIB] failed to create sec_motor\n");
		return -ENODEV;
	}

	err = sysfs_create_group(&sec_motor->kobj, &sec_motor_attr_group);
	if (err < 0) {
		pr_err("[VIB] failed to create sec_motor_attr\n");
		return err;
	}

	return 0;
}
#endif

static int zh915_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	struct zh915_data *pZh915data;
	struct zh915_platform_data *pZh915Platdata;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		dev_err(&client->dev, "[VIB] %s:I2C check failed\n", __FUNCTION__);
		return -ENODEV;
	}

	/* platform_data init */
	if(client->dev.of_node) {
		pZh915Platdata = devm_kzalloc(&client->dev, 
				sizeof(struct zh915_platform_data), GFP_KERNEL);
		if (!pZh915Platdata) {
			dev_err(&client->dev, "[VIB] unable to allocate pdata memory\n");
			return -ENOMEM;
		}
		err = of_zh915_dt(client, pZh915Platdata);
		if (err < 0) {
			dev_err(&client->dev, "[VIB] fail to read DT %d\n", err);
			return -ENODEV;
		}
	} else
		pZh915Platdata = dev_get_platdata(&client->dev);

	pZh915data = devm_kzalloc(&client->dev, sizeof(struct zh915_data), GFP_KERNEL);
	if (pZh915data == NULL){
		dev_err(&client->dev, "[VIB] %s:no memory\n", __FUNCTION__);
		return -ENOMEM;
	}

	pZh915data->dev = &client->dev;
	pZh915data->mpRegmap = devm_regmap_init_i2c(client, &zh915_i2c_regmap);
	if (IS_ERR(pZh915data->mpRegmap)) {
		err = PTR_ERR(pZh915data->mpRegmap);
		dev_err(pZh915data->dev,
			"[VIB] %s:Failed to allocate register map: %d\n",__FUNCTION__,err);
		return err;
	}

	if (pZh915Platdata->regulator_name) {
		pZh915data->regulator
				= devm_regulator_get(pZh915data->dev, pZh915Platdata->regulator_name);
		if (IS_ERR(pZh915data->regulator)) {
			dev_err(pZh915data->dev, "[VIB] Failed to get moter power supply.\n");
			return -EFAULT;
		}
		err = regulator_set_voltage(pZh915data->regulator, POWER_SUPPLY_VOLTAGE, POWER_SUPPLY_VOLTAGE);
		if (err < 0)
			dev_err(pZh915data->dev, "[VIB] Failed to set moter power %duV. %d\n", POWER_SUPPLY_VOLTAGE, err);
		err = regulator_enable(pZh915data->regulator);
		if (err < 0) {
			dev_err(pZh915data->dev, "[VIB] %s regulator enable fail\n", pZh915Platdata->regulator_name);
			return -EFAULT;
		}
	}

	if (pZh915Platdata->gpio_en > 0) {
		err = devm_gpio_request_one(pZh915data->dev, pZh915Platdata->gpio_en,
												GPIOF_DIR_OUT | GPIOF_INIT_LOW, "motor");
		if (err < 0) {
			dev_err(pZh915data->dev ,"[VIB] gpio_en request fail %d\n", err);
			return -EFAULT;
		}
	}
	pZh915data->gpio_en = pZh915Platdata->gpio_en;

	memcpy(&pZh915data->msPlatData, pZh915Platdata, sizeof(struct zh915_platform_data));

	err = zh915_reg_read(pZh915data, ZH915_REG_ID);
	if(err < 0){
		dev_err(pZh915data->dev,
			"[VIB] %s, i2c bus fail (%d)\n", __FUNCTION__, err);
		return -EFAULT;
	}else{
		dev_info(pZh915data->dev,
			"[VIB] %s, ID status (0x%x)\n", __FUNCTION__, err);
		pZh915data->mnDeviceID = err;
	}

	err = dev_init_platform_data(pZh915data);
	if (err < 0){
		dev_err(pZh915data->dev, "[VIB] dev_init_platform failed. %d\n", err);
		return -EFAULT;
	}

    err = Haptics_init(pZh915data);
	if (err < 0){
		dev_err(pZh915data->dev, "[VIB] Haptics_init failed. %d\n", err);
		return -EFAULT;
	}

	g_ZH915data = pZh915data;
	i2c_set_clientdata(client,pZh915data);

	zh915_debug_init();
#ifdef CONFIG_SEC_SYSFS
	sec_motor_init();
#endif

    dev_info(pZh915data->dev,
		"[VIB] zh915 probe succeeded\n");

    return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id haptic_dt_ids[] = {
	{ .compatible = "zh915" },
	{ },
};
MODULE_DEVICE_TABLE(of, haptic_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_device_id zh915_id_table[] =
{
    { HAPTICS_DEVICE_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, zh915_id_table);

static struct i2c_driver zh915_driver =
{
    .driver = {
        .name = HAPTICS_DEVICE_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = haptic_dt_ids,
#endif /* CONFIG_OF */
    },
    .id_table = zh915_id_table,
    .probe = zh915_probe,
};

static int __init zh915_init(void)
{
	pr_info("[VIB] %s\n", __func__);
	return i2c_add_driver(&zh915_driver);
}

static void __exit zh915_exit(void)
{
	i2c_del_driver(&zh915_driver);
}

module_init(zh915_init);
module_exit(zh915_exit);

MODULE_AUTHOR("samsung");
MODULE_DESCRIPTION("Driver for "HAPTICS_DEVICE_NAME);
