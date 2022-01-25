/* drivers/motor/isa1000a_vibrator.c
* Copyright (C) 2016 Samsung Electronics Co. Ltd. All Rights Reserved.
*
* Author: Junyoun Kim <junyouns.kim@samsung.com>\
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/isa1000a_vibrator.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/delay.h>

#define MAX_LEVEL 0xffff

enum ISA1000A_VIBRATOR_CONTROL {
	ISA1000A_VIBRATOR_DISABLE = 0,
	ISA1000A_VIBRATOR_ENABLE = 1,
};

struct isa1000a_vibrator_data {
	struct isa1000a_vibrator_platform_data *pdata;
	struct pwm_device *pwm;
	struct regulator *regulator;
	spinlock_t lock;
	bool running;
	int level;
};

static int vib_run(struct isa1000a_vibrator_data *hap_data, bool en)
{
	int ret = 0;
	int pwm_period = 0, pwm_duty = 0;

	pr_info("[VIB] %s %s [level]%d\n", __func__, en ? "on" : "off", hap_data->level);

	if (hap_data == NULL) {
		pr_info("[VIB] the motor is not ready!!!");
		return -EINVAL;
	}

	if (en) {
		pwm_period = hap_data->pdata->period;
		/* 99% duty cycle isa1000a breaking point */
		pwm_duty = pwm_period / 2 + pwm_period / 2 * hap_data->level / MAX_LEVEL * 48 / 50;

		ret = pwm_config(hap_data->pwm, pwm_duty, pwm_period);
		if (ret < 0) {
			pr_err("[VIB] %s pwm_config fail %d", __func__, ret);
			return ret;
		}
		pwm_enable(hap_data->pwm);

		gpio_set_value(hap_data->pdata->motor_en_gpio, 1);
		if (hap_data->running)
			return ret;
		hap_data->running = true;
	} else {
		pwm_disable(hap_data->pwm);
		gpio_set_value(hap_data->pdata->motor_en_gpio, 0);
		if (!hap_data->running)
			return ret;
		hap_data->running = false;
	}

	return ret;
}

static int isa1000a_haptic_play(struct input_dev *input, void *data,
				struct ff_effect *effect)
{
	struct isa1000a_vibrator_data *hap_data = input_get_drvdata(input);
	int level = effect->u.rumble.strong_magnitude;

	pr_info("[VIB] %s [level]%d\n", __func__, level);

	if (level <= 0)
		level = 0;

	if (level) {
		if(!(hap_data->running && level == hap_data->level)) {
			hap_data->level = level;
			vib_run(hap_data, (bool)ISA1000A_VIBRATOR_ENABLE);
		}
	}
	else
		vib_run(hap_data, (bool)ISA1000A_VIBRATOR_DISABLE);

	return 0;
}

static void isa1000a_haptic_close(struct input_dev *input)
{
	struct isa1000a_vibrator_data *hap_data = input_get_drvdata(input);

	vib_run(hap_data, ISA1000A_VIBRATOR_DISABLE);
}

static ssize_t motor_control_show_motor_on(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* dummy */
	return 0;
}

static ssize_t motor_control_show_motor_off(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* dummy */
	return 0;
}

static DEVICE_ATTR(motor_on, S_IRUGO, motor_control_show_motor_on, NULL);
static DEVICE_ATTR(motor_off, S_IRUGO, motor_control_show_motor_off, NULL);

static struct attribute *motor_control_attributes[] = {
	&dev_attr_motor_on.attr,
	&dev_attr_motor_off.attr,
	NULL
};
static const struct attribute_group motor_control_group = {
	.attrs = motor_control_attributes,
};

static int release_motor(struct isa1000a_vibrator_data *hap_data) {
	int ret = 0;

	pwm_free(hap_data->pwm);
	regulator_put(hap_data->regulator);
	gpio_free(hap_data->pdata->motor_en_gpio);

	return ret;
}

static int init_motor(struct isa1000a_vibrator_data *hap_data) {
	int ret = 0;

	hap_data->regulator
			= regulator_get(NULL, hap_data->pdata->regulator_name);
	if (IS_ERR(hap_data->regulator)) {
		pr_info("[VIB] Failed to get vmoter regulator.\n");
		ret = -EFAULT;
		return ret;
	}
	regulator_set_voltage(hap_data->regulator, hap_data->pdata->motor_vdd, hap_data->pdata->motor_vdd);
	ret = regulator_enable(hap_data->regulator);
	if(ret){
		pr_err("[VIB] Failed to enable regulator\n");
		goto err_regulator_put;
	}

	hap_data->pwm = pwm_request(hap_data->pdata->pwm_id, "vibrator");
	if (IS_ERR(hap_data->pwm)) {
		pr_err("[VIB] Failed to request pwm\n");
		ret = -EFAULT;
		goto err_regulator_put;
	}
	pwm_config(hap_data->pwm, hap_data->pdata->period / 2, hap_data->pdata->period);

	ret = gpio_request_one(hap_data->pdata->motor_en_gpio,
					GPIOF_DIR_OUT | GPIOF_INIT_LOW,
					"vibrator");
	if (ret < 0) {
		pr_err("[VIB] Failed to request vibrator En pin : %d\n", ret);
		goto err_free_pwm;
	}

	return ret;

err_free_pwm:
	pwm_free(hap_data->pwm);
err_regulator_put:
	regulator_put(hap_data->regulator);

	return ret;
}


#if defined(CONFIG_OF)
static int of_isa1000a_vibrator_dt(struct isa1000a_vibrator_platform_data *pdata)
{
	struct device_node *np_haptic;
	const char *temp_str;
	int ret = 0;

	np_haptic = of_find_node_by_path("/isa1000a_vibrator");
	if (np_haptic == NULL) {
		pr_err("[VIB] error to get dt node\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(np_haptic, "haptic,period", &pdata->period);
	if(ret < 0)
		return ret;
	ret = of_property_read_string(np_haptic, "haptic,regulator_name", &temp_str);
	if(ret < 0)
		return ret;
	pdata->regulator_name = (char *)temp_str;
	ret = of_property_read_u32(np_haptic, "haptic,pwm_id", &pdata->pwm_id);
	if(ret < 0)
		return ret;
	ret = pdata->motor_en_gpio = of_get_named_gpio(np_haptic, "haptic,motor_en", 0);
	if(ret < 0)
		return ret;
	ret = of_property_read_u32(np_haptic, "haptic,motor_vdd", &pdata->motor_vdd);
	if(ret < 0)
		return ret;

	return ret;
}
#endif /* CONFIG_OF */

static int isa1000a_vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct input_dev *input_dev;
	struct isa1000a_vibrator_data *hap_data;
	struct isa1000a_vibrator_platform_data *isa1000a_pdata;

	pr_info("[VIB] ++ %s\n", __func__);

	/* platform_data init */
	if(pdev->dev.of_node) {
		isa1000a_pdata = kzalloc(sizeof(struct isa1000a_vibrator_platform_data), GFP_KERNEL);
		if (!isa1000a_pdata) {
			dev_err(&pdev->dev, "[VIB] unable to allocate pdata memory\n");
			return -ENOMEM;
		}
		ret = of_isa1000a_vibrator_dt(isa1000a_pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "[VIB] Failed to read vibrator DT %d\n", ret);
			goto err_kfree_pdata;
		}
	} else
		isa1000a_pdata = dev_get_platdata(&pdev->dev);
	pr_info("[VIB] pdata.period = %d\n", isa1000a_pdata->period);
	pr_info("[VIB] pdata.regulator_name = %s\n", isa1000a_pdata->regulator_name);
	pr_info("[VIB] pdata.pwm_id = %d\n", isa1000a_pdata->pwm_id);
	pr_info("[VIB] pdata.motor_en_gpio = %d\n", isa1000a_pdata->motor_en_gpio);
	pr_info("[VIB] pdata.motor_vdd = %d\n", isa1000a_pdata->motor_vdd);

	/* vibrator_data init */
	hap_data = kzalloc(sizeof(struct isa1000a_vibrator_data), GFP_KERNEL);
	if (!hap_data) {
		dev_err(&pdev->dev, "[VIB] unable to allocate hap_data memory\n");
		ret = -ENOMEM;
#ifdef CONFIG_OF
		goto err_kfree_pdata;
#else
		return ret;
#endif
	}
	hap_data->pdata = isa1000a_pdata;
	spin_lock_init(&(hap_data->lock));

	/* motor input device init */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("[VIB] unable to allocate input device \n");
		ret =  -ENOMEM;
		goto err_kfree_mem;
	}
	input_dev->name = "isa1000a_haptic";
	input_dev->dev.parent = &pdev->dev;
	input_dev->close = isa1000a_haptic_close;
	input_set_capability(input_dev, EV_FF, FF_RUMBLE);
	ret = input_ff_create_memless(input_dev, NULL,
		isa1000a_haptic_play);
	if (ret < 0) {
		pr_err("[VIB] input_ff_create_memless() failed: %d\n", ret);
		goto err_free_input;
	}
	ret = input_register_device(input_dev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"[VIB] couldn't register input device: %d\n",
			ret);
		goto err_destroy_ff;
	}

	ret = init_motor(hap_data);
	if(ret < 0) {
		pr_err("[VIB] failt to init motor %d\n", ret);
		goto err_unregister_input;
	}

	/* sysfs init */
	ret = sysfs_create_group(&pdev->dev.kobj, &motor_control_group);
	if (ret < 0)
		pr_info("[VIB] failed to create motor control attribute group\n");

	input_set_drvdata(input_dev, hap_data);
	platform_set_drvdata(pdev, hap_data);
	pr_info("[VIB] -- %s\n", __func__);

	return ret;

err_unregister_input:
	input_unregister_device(input_dev);
err_destroy_ff:
	input_ff_destroy(input_dev);
err_free_input:
	input_free_device(input_dev);
err_kfree_mem:
	kfree(hap_data);
#ifdef CONFIG_OF
err_kfree_pdata:
	kfree(isa1000a_pdata);
#endif

	return ret;
}

static int __devexit isa1000a_vibrator_remove(struct platform_device *pdev)
{
	struct isa1000a_vibrator_data *hap_data = platform_get_drvdata(pdev);

	pr_info("[VIB] %s\n", __func__);

	if(hap_data != NULL) {
		release_motor(hap_data);
#ifdef CONFIG_OF
		kfree(hap_data->pdata);
#endif
		kfree(hap_data);
		platform_set_drvdata(pdev, NULL);
	}

	return 0;
}

static int isa1000a_vibrator_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct isa1000a_vibrator_data *hap_data = platform_get_drvdata(pdev);

	pr_info("[VIB] %s\n", __func__);

	if (hap_data != NULL) {
		vib_run(hap_data, (bool)ISA1000A_VIBRATOR_DISABLE);
		regulator_disable(hap_data->regulator);
	}

	return 0;
}

static int isa1000a_vibrator_resume(struct platform_device *pdev)
{
	struct isa1000a_vibrator_data *hap_data = platform_get_drvdata(pdev);
	int ret;

	pr_info("[VIB] %s\n", __func__);

	if (hap_data != NULL) {
		ret = regulator_enable(hap_data->regulator);
		if (ret < 0) {
			pr_err("[VIB] vibrator resume regulator fail\n");
			return ret;
		}
		vib_run(hap_data, (bool)ISA1000A_VIBRATOR_DISABLE);
	}

	return 0;
}

static void isa1000a_vibrator_shutdown(struct platform_device *pdev)
{
	struct isa1000a_vibrator_data *hap_data = platform_get_drvdata(pdev);

	pr_info("[VIB] %s\n", __func__);

	if (hap_data != NULL && hap_data->regulator != NULL) {
		regulator_disable(hap_data->regulator);
	}

	return;
}

#if defined(CONFIG_OF)
static struct of_device_id haptic_dt_ids[] = {
	{ .compatible = "isa1000a-vibrator" },
	{ },
};
MODULE_DEVICE_TABLE(of, haptic_dt_ids);
#endif /* CONFIG_OF */


static struct platform_driver isa1000a_vibrator_driver = {
	.probe		= isa1000a_vibrator_probe,
	.remove		= isa1000a_vibrator_remove,
	.suspend	= isa1000a_vibrator_suspend,
	.resume		= isa1000a_vibrator_resume,
	.shutdown	= isa1000a_vibrator_shutdown,
	.driver = {
		.name	= "isa1000a-vibrator",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = haptic_dt_ids,
#endif /* CONFIG_OF */
	},
};

static int __init isa1000a_vibrator_init(void)
{
	pr_info("[VIB] %s\n", __func__);

	return platform_driver_register(&isa1000a_vibrator_driver);
}
module_init(isa1000a_vibrator_init);

static void __exit isa1000a_vibrator_exit(void)
{
	platform_driver_unregister(&isa1000a_vibrator_driver);
}
module_exit(isa1000a_vibrator_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ISA1000A motor driver");
