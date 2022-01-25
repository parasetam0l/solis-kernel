/*
 *  input_assistant.c
 *  for Samsung Electronics
 *
 *  Copyright (C) 2017 Samsung Electronics
 *  Sang-Min,Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>
#include <linux/input/input-assistant.h>
#if defined(TOUCH_BOOSTER) || defined(ROTARY_BOOSTER)
#include <linux/trm.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#endif

//#define ENABLE_DEBUG
#define GPIO_KEYS_DEV_NAME	"gpio_keys"
#define POWER_KEYS_DEV_NAME	"s2mpw01-power-keys"
#define TSP_DEV_NAME		"sec_touchscreen"
#define BEZEL_DEV_NAME		"tizen_detent"
#define TSP_NUM_MAX		10
#define TSP_FINGER_RELEASE	-1

#define WAKELOCK_TIME		HZ/10

static void input_assistant_tsp_handler(struct input_handle *handle,
			       const struct input_value *vals, unsigned int count)
{
	static struct input_assistant_tsp in_data[TSP_NUM_MAX];
	struct input_handler *handler = handle->handler;
	struct input_assistant_data *data = handler->private;
	static int last_slot;
	static int finger_cnt;
	int i;

	wake_lock_timeout(&data->wake_lock, WAKELOCK_TIME);

	for (i = 0; i < count; i++) {
		if (vals[i].type != EV_ABS)
			continue;

		switch (vals[i].code) {
		case ABS_MT_TRACKING_ID:
			in_data[last_slot].tracking_id = vals[i].value;
			in_data[last_slot].finger_id = last_slot;
			in_data[last_slot].update = true;
			if (in_data[last_slot].finger_id == TSP_FINGER_RELEASE) {
				if (finger_cnt > 0)
					finger_cnt--;
			} else {
				if (finger_cnt < TSP_NUM_MAX)
					finger_cnt++;
			}
			break;
		case ABS_MT_SLOT:
			last_slot = vals[i].value;
			break;
		case ABS_MT_POSITION_X:
			in_data[last_slot].x = vals[i].value;
			break;
		case ABS_MT_POSITION_Y:
			in_data[last_slot].y = vals[i].value;
			break;
		case ABS_MT_PRESSURE:
			in_data[last_slot].z = vals[i].value;
			break;
		case ABS_MT_TOUCH_MAJOR:
			in_data[last_slot].wmajor = vals[i].value;
			break;
		case ABS_MT_TOUCH_MINOR:
			in_data[last_slot].wminor = vals[i].value;
			break;
		case ABS_MT_PALM:
			in_data[last_slot].palm = vals[i].value;
			pr_info("%s: [%s][%d] Palm\n", __func__,
				in_data[last_slot].palm ? "P":"R",
				in_data[last_slot].finger_id);
			break;
		}
	}

	for (i = 0; i < TSP_NUM_MAX; i++) {
		if (in_data[i].update) {
			in_data[i].update = false;
			pr_info("%s: [%s][%d] x=%d, y=%d, z=%d, M=%d, m=%d\n",
				__func__, (in_data[i].tracking_id == TSP_FINGER_RELEASE) ? "R":"P",
				in_data[i].finger_id, in_data[i].x, in_data[i].y,
				in_data[i].z, in_data[i].wmajor, in_data[i].wminor);
		}
	}

#ifdef TOUCH_BOOSTER
	if (finger_cnt)
		touch_booster_press();
	else
		touch_booster_release();
#endif
}

static void input_assistant_key_logger(struct input_handle *handle,
			       const struct input_value *vals, unsigned int count)
{
	struct input_handler *handler = handle->handler;
	struct input_assistant_data *data = handler->private;
	int i;

	wake_lock_timeout(&data->wake_lock, WAKELOCK_TIME);

	for (i = 0; i < count; i++) {
		if (vals[i].type != EV_KEY)
			continue;

		switch (vals[i].code) {
		case KEY_POWER:
			pr_info("%s: [%s]KEY_POWER\n", __func__, vals[i].value ? "P":"R");
			break;
		case KEY_PHONE:
			pr_info("%s: [%s]KEY_PHONE\n", __func__, vals[i].value ? "P":"R");
			break;
		case KEY_BACK:
			pr_info("%s: [%s]KEY_BACK\n", __func__, vals[i].value ? "P":"R");
			break;
		case KEY_HOME:
			pr_info("%s: [%s]KEY_HOME\n", __func__, vals[i].value ? "P":"R");
			break;
		case KEY_VOLUMEDOWN:
			pr_info("%s: [%s]KEY_VOLUMEDOWN\n", __func__, vals[i].value ? "P":"R");
			break;
		case KEY_VOLUMEUP:
			pr_info("%s: [%s]KEY_VOLUMEUP\n", __func__, vals[i].value ? "P":"R");
			break;
		default:
			pr_info("%s: [%s]0x%02x\n", __func__, vals[i].value ? "P":"R", vals[i].code);
			break;
		}
	}
}

static void input_assistant_bezel_logger(struct input_handle *handle,
			       const struct input_value *vals, unsigned int count)
{
	struct input_handler *handler = handle->handler;
	struct input_assistant_data *data = handler->private;
	int i, wheel, x;

	wake_lock_timeout(&data->wake_lock, WAKELOCK_TIME);

	for (i = 0; i < count; i++) {
		if (vals[i].type != EV_REL)
			continue;

		switch (vals[i].code) {
		case REL_WHEEL:
			wheel = vals[i].value;
			break;
		case REL_X:
			x = ~vals[i].value & 0x07;
			break;
		}
	}
#ifdef ROTARY_BOOSTER
	rotary_booster_turn_on();
#endif
	pr_info("%s: s=[%d], d=[%d]\n", __func__, x, wheel);
}

static void input_assistant_events(struct input_handle *handle,
			       const struct input_value *vals, unsigned int count)
{
	struct input_dev *dev = handle->dev;

	if (!strncmp(dev->name, TSP_DEV_NAME, strlen(TSP_DEV_NAME)))
		input_assistant_tsp_handler(handle, vals, count);
	else if (!strncmp(dev->name, GPIO_KEYS_DEV_NAME, strlen(GPIO_KEYS_DEV_NAME)))
		input_assistant_key_logger(handle, vals, count);
	else if (!strncmp(dev->name, POWER_KEYS_DEV_NAME, strlen(POWER_KEYS_DEV_NAME)))
		input_assistant_key_logger(handle, vals, count);
	else if (!strncmp(dev->name, BEZEL_DEV_NAME, strlen(BEZEL_DEV_NAME)))
		input_assistant_bezel_logger(handle, vals, count);
}

static int input_assistant_check_support_dev(struct input_handler *handler, struct input_dev *dev)
{
	struct input_assistant_data *data = handler->private;
	struct input_assistant_pdata *pdata = data->pdata;
	int i;

	for (i = 0; i < pdata->support_dev_num; i++) {
		if (!strncmp(dev->name, pdata->support_dev_name[i], strlen(pdata->support_dev_name[i])))
			return 0;
	}

	dev_dbg(&dev->dev, "%s: not support device[%s]\n", __func__, dev->name);

	return -ENODEV;
}

static int input_assistant_connect(struct input_handler *handler,
				struct input_dev *dev,
				const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	if (input_assistant_check_support_dev(handler, dev))
		return -ENXIO;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "input_assistant";

	ret = input_register_handle(handle);
	if (ret) {
		dev_err(&dev->dev,
			"%s: Failed to register input assistant handler(%d)\n",
			__func__, ret);
		kfree(handle);
		return ret;
	}

	ret = input_open_device(handle);
	if (ret) {
		dev_err(&dev->dev,
			"%s: Failed to open input assistant device(%d)\n",
			__func__, ret);
		input_unregister_handle(handle);
		kfree(handle);
		return ret;
	}

	dev_info(&dev->dev, "%s: connected %s.\n", __func__, dev->name);

	return 0;
}

static void input_assistant_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static struct input_handler input_assistant_handler = {
	.events = input_assistant_events,
	.connect = input_assistant_connect,
	.disconnect = input_assistant_disconnect,
	.name = "input_assistant",
};

static void input_assistant_set_ids(struct input_device_id *ids, unsigned int type,
				 unsigned int code)
{
	switch (type) {
	case EV_KEY:
		ids->flags = INPUT_DEVICE_ID_MATCH_KEYBIT;
		__set_bit(code, ids->keybit);
		break;

	case EV_REL:
		ids->flags = INPUT_DEVICE_ID_MATCH_RELBIT;
		__set_bit(code, ids->relbit);
		break;

	case EV_ABS:
		ids->flags = INPUT_DEVICE_ID_MATCH_ABSBIT;
		__set_bit(code, ids->absbit);
		break;

	case EV_MSC:
		ids->flags = INPUT_DEVICE_ID_MATCH_MSCIT;
		__set_bit(code, ids->mscbit);
		break;

	case EV_SW:
		ids->flags = INPUT_DEVICE_ID_MATCH_SWBIT;
		__set_bit(code, ids->swbit);
		break;

	case EV_LED:
		ids->flags = INPUT_DEVICE_ID_MATCH_LEDBIT;
		__set_bit(code, ids->ledbit);
		break;

	case EV_SND:
		ids->flags = INPUT_DEVICE_ID_MATCH_SNDBIT;
		__set_bit(code, ids->sndbit);
		break;

	case EV_FF:
		ids->flags = INPUT_DEVICE_ID_MATCH_FFBIT;
		__set_bit(code, ids->ffbit);
		break;

	case EV_PWR:
		/* do nothing */
		break;

	default:
		pr_err("%s: unknown type %u (code %u)\n",
			__func__, type, code);
		return;
	}

	ids->flags |= INPUT_DEVICE_ID_MATCH_EVBIT;
	__set_bit(type, ids->evbit);
}

#ifdef CONFIG_OF
static int input_assistant_parse_dt(struct device *dev,
			struct input_assistant_pdata *pdata)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	enum mkey_check_option option;
	unsigned int *map;
	unsigned int *type;
	const char *out_prop;
	int i, j, rc, map_size, type_size;

	rc = of_property_read_u32(np, "input_assistant,num_map",
			(unsigned int *)&pdata->num_map);
	if (rc) {
		dev_err(dev, "%s: failed to get num_map.\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(np, "input_assistant,map_key",
			(unsigned int *)&option);
	if (rc) {
		dev_err(dev, "Unable to read %s\n", "input_assistant,map_key");
		goto error;
	}

	pdata->mmap = devm_kzalloc(dev,
			sizeof(struct input_assistant_mmap)*pdata->num_map, GFP_KERNEL);
	if (!pdata->mmap) {
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < pdata->num_map; i++) {
		rc = of_property_read_string_index(np, "input_assistant,map_codes", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "%s: failed to get %d map_codes string.[%s]\n", __func__, i, out_prop);
			goto error;

		}
		prop = of_find_property(np, out_prop, NULL);
		if (!prop) {
			dev_err(dev, "%s: failed to get %d map_codes property[%s]\n", __func__, i, out_prop);
			rc = -EINVAL;
			goto error;
		}
		map_size = prop->length / sizeof(unsigned int);
		pdata->mmap[i].num_mkey = map_size;
		map = devm_kzalloc(dev, sizeof(unsigned int)*map_size, GFP_KERNEL);
		if (!map) {
			dev_err(dev, "%s: Failed to allocate map memory.\n", __func__);
			rc = -ENOMEM;
			goto error;
		}

		rc = of_property_read_u32_array(np, out_prop, map, map_size);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "%s: Unable to read %s array\n", __func__, out_prop);
			goto error;
		}

		rc = of_property_read_string_index(np, "input_assistant,map_types", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "%s: failed to get %d map_types string[%s]\n", __func__, i, out_prop);
			rc = -EINVAL;
			goto error;

		}
		prop = of_find_property(np, out_prop, NULL);
		if (!prop) {
			dev_err(dev, "%s: failed to get %d map_codes property[%s]\n", __func__, i, out_prop);
			rc = -EINVAL;
			goto error;
		}

		type_size = prop->length / sizeof(unsigned int);
		type = devm_kzalloc(dev, sizeof(unsigned int)*type_size, GFP_KERNEL);
		if (!type) {
			dev_err(dev, "%s: Failed to allocate key memory.\n", __func__);
			rc = -ENOMEM;
			goto error;
		}

		rc = of_property_read_u32_array(np, out_prop, type, type_size);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read %s\n", out_prop);
			goto error;
		}

		pdata->mmap[i].mkey_map = devm_kzalloc(dev,
					sizeof(struct input_assistant_mkey)*
					map_size, GFP_KERNEL);
		if (!pdata->mmap[i].mkey_map) {
			dev_err(dev, "%s: Failed to allocate memory\n", __func__);
			rc = -ENOMEM;
			goto error;
		}

		for (j = 0; j < map_size; j++) {
			pdata->mmap[i].mkey_map[j].type = type[j];
			pdata->mmap[i].mkey_map[j].code = map[j];
		}
	}

	rc = of_property_read_u32(np, "input_assistant,dev_num",
			(unsigned int *)&pdata->support_dev_num);
	if (rc) {
		dev_err(dev, "%s: failed to get support_dev_num.\n", __func__);
		goto error;
	}

	pdata->support_dev_name = devm_kzalloc(dev,
			pdata->support_dev_num * sizeof(char *), GFP_KERNEL);
	if (!pdata->support_dev_name) {
		rc = -ENOMEM;
		goto error;
	}

	for (i = 0; i < pdata->support_dev_num; i++) {
		rc = of_property_read_string_index(np, "input_assistant,dev_name_str", i, &out_prop);
		if (rc < 0) {
			dev_err(dev, "failed to get %d dev_name_str string\n", i);
			goto error;
		}
		pdata->support_dev_name[i] = (char *)out_prop;
	}

#ifdef ENABLE_DEBUG
	for (i = 0; i < pdata->num_map; i++) {

		dev_info(dev, "%s: pdata->mmap[%d].num_mkey=[%d]\n",
				__func__, i, pdata->mmap[i].num_mkey);
		for (j = 0; j < pdata->mmap[i].num_mkey; j++) {
			dev_info(dev,
				"%s: pdata->mmap[%d].mkey_map[%d].type=[%d]\n",
				__func__, i, j, pdata->mmap[i].mkey_map[j].type);
			dev_info(dev,
				"%s: pdata->mmap[%d].mkey_map[%d].code=[%d]\n",
				__func__, i, j, pdata->mmap[i].mkey_map[j].code);
			dev_info(dev,
				"%s: pdata->mmap[%d].mkey_map[%d].option=[%d]\n",
				__func__, i, j, pdata->mmap[i].mkey_map[j].option);
		}
	}

	dev_info(dev, "%s: pdata->support_dev_num=[%d]\n", __func__, pdata->support_dev_num);
	for (i = 0; i < pdata->support_dev_num; i++)
		dev_info(dev, "%s: pdata->support_dev_name[%d] = [%s]\n",
			__func__, i, pdata->support_dev_name[i]);
#endif
	return 0;
error:
	return rc;
}

static const struct of_device_id input_assistant_of_match[] = {
	{ .compatible = "input-assistant", },
	{ },
};
MODULE_DEVICE_TABLE(of, input_assistant_of_match);
#else
static int input_assistant_parse_dt(struct device *dev,
			struct input_assistant_pdata *pdata)
{
	dev_err(dev, "%s\n", __func__);
	return -ENODEV;
}
#endif

static int input_assistant_probe(struct platform_device *pdev)
{

	struct input_assistant_pdata *pdata;
	struct input_assistant_data *assistant_data;
	struct input_device_id *input_assistant_ids;
	int ret, i, j, k;
	int total_num_key = 0;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct input_assistant_pdata),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "%s: Failed to allocate pdata memory\n", __func__);
			ret = -ENOMEM;
			goto error_1;
		}
		ret = input_assistant_parse_dt(&pdev->dev, pdata);
		if (ret) {
			dev_err(&pdev->dev, "%s: Fail parse device tree.\n", __func__);
			ret = -EINVAL;
			goto error_1;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "%s: Fail input assistant platform data.\n", __func__);
			ret = -EINVAL;
			goto error_1;
		}
	}

	if (pdata->num_map == 0) {
		dev_err(&pdev->dev,
			"%s: No input platform data. num_mkey is NULL.\n", __func__);
		ret = -EINVAL;
		goto error_1;
	}

	assistant_data = kzalloc(sizeof(struct input_assistant_data), GFP_KERNEL);
	if (!assistant_data) {
		ret = -ENOMEM;
		goto error_1;
	}

	for (i = 0; i < pdata->num_map; i++)
		total_num_key += pdata->mmap[i].num_mkey;

	input_assistant_ids =
		kzalloc(sizeof(struct input_device_id[(total_num_key + 1)]),
			GFP_KERNEL);
	if (!input_assistant_ids) {
		dev_err(&pdev->dev, "Failed to allocate input_assistant_ids memory\n");
		ret = -ENOMEM;
		goto error_2;
	}
	memset(input_assistant_ids, 0x00, sizeof(struct input_device_id));

	for (i = 0, k = 0; i < pdata->num_map; i++) {
		for (j = 0; j < pdata->mmap[i].num_mkey; j++) {
			input_assistant_set_ids(&input_assistant_ids[k++],
					     pdata->mmap[i].mkey_map[j].type,
					     pdata->mmap[i].mkey_map[j].code);
		}
	}

	dev_set_drvdata(&pdev->dev, assistant_data);

	assistant_data->pdev = pdev;
	assistant_data->pdata = pdata;

	input_assistant_handler.private = assistant_data;
	input_assistant_handler.id_table = input_assistant_ids;


	wake_lock_init(&assistant_data->wake_lock,
			WAKE_LOCK_SUSPEND, "input_assistant_wake_lock");

	ret = input_register_handler(&input_assistant_handler);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register input_assistant_handler\n");
		goto error_3;
	}

	dev_info(&pdev->dev, "%s: done.\n", __func__);

	return 0;

error_3:
	wake_lock_destroy(&assistant_data->wake_lock);
	kfree(input_assistant_ids);
error_2:
	kfree(assistant_data);
error_1:
	return ret;

}

static int input_assistant_remove(struct platform_device *dev)
{
	struct input_assistant_data *assistant_data = platform_get_drvdata(dev);

	wake_lock_destroy(&assistant_data->wake_lock);
	kfree(input_assistant_handler.id_table);
	input_unregister_handler(&input_assistant_handler);
	kfree(assistant_data);
	platform_set_drvdata(dev, NULL);

	return 0;
}

static struct platform_driver input_assistant_driver = {
	.probe = input_assistant_probe,
	.remove = input_assistant_remove,
	.driver = {
		   .name = "input_assistant",
#ifdef CONFIG_OF
		   .of_match_table = input_assistant_of_match,
#endif
		   },
};

static int __init input_assistant_init(void)
{
	return platform_driver_register(&input_assistant_driver);
}

static void __exit input_assistant_exit(void)
{
	platform_driver_unregister(&input_assistant_driver);
}

device_initcall(input_assistant_init);
module_exit(input_assistant_exit);

MODULE_AUTHOR("Sang-Min,Lee <lsmin.lee@samsung.com>");
MODULE_LICENSE("GPL");
