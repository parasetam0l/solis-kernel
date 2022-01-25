/*
 * sec_thermistor.c - SEC Thermistor
 *
 *  Copyright (C) 2013 Samsung Electronics
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/iio/consumer.h>
#include <linux/platform_data/sec_thermistor.h>
#include <linux/sec_sysfs.h>

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
#include <linux/platform_data/sec_thermistor_history.h>
#endif

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define ADC_SAMPLING_CNT	7
#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
#define TEMPERATURE_CHANGE_DIRECT_VALUE		1
#define TEMPERATURE_CHANGE_OFFSET			2
#endif

static struct device *temperature_dev;
static LIST_HEAD(therm_info_lists);

struct sec_therm_info {
	struct device *dev;
	struct device *hwmon_dev;
	struct sec_therm_platform_data *pdata;
	struct iio_channel *chan;
	char name[PLATFORM_NAME_SIZE];
	unsigned type_id;
	struct list_head list;
};

struct sec_therm_device_attribute {
	struct device_attribute dev_attr;
	struct sec_therm_info *info;
};

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
static int debug_mode[NR_TYPE_SEC_THREM];
static int debug_temp[NR_TYPE_SEC_THREM];
#endif

#ifdef CONFIG_OF
static const struct platform_device_id sec_thermistor_id[] = {
	{ "ap_therm", TYPE_SEC_THREM_AP },
	{ "batt_therm", TYPE_SEC_THREM_BATTERY },
	{ "pa_therm0", TYPE_SEC_THREM_PAM0 },
	{ "pa_therm1", TYPE_SEC_THREM_PAM1 },
	{ "xo_therm", TYPE_SEC_THREM_XO },
	{ "sec-cf-thermistor", TYPE_SEC_THREM_CAM_FLASH },
	{ "cp_therm", TYPE_SEC_THREM_CP_SPEC },
	{ },
};

static const struct of_device_id sec_therm_match[] = {
	{ .compatible = "samsung,sec-ap-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_AP] },
	{ .compatible = "samsung,sec-batt-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_BATTERY] },
	{ .compatible = "samsung,sec-pam0-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_PAM0] },
	{ .compatible = "samsung,sec-pam1-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_PAM1] },
	{ .compatible = "samsung,sec-xo-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_XO] },
	{ .compatible = "samsung,sec-cf-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_CAM_FLASH] },
	{ .compatible = "samsung,sec-cpspc-thermistor",
		.data = &sec_thermistor_id[TYPE_SEC_THREM_CP_SPEC] },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_therm_match);

static bool sec_therm_single_inst[NR_TYPE_SEC_THREM] = {0,};

static struct sec_therm_platform_data *
sec_therm_parse_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sec_therm_platform_data *pdata;
	u32 len1, len2;
	int i;
	u32 adc, temp;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	if (!of_get_property(np, "adc_array", &len1))
		return ERR_PTR(-ENOENT);
	if (!of_get_property(np, "temp_array", &len2))
		return ERR_PTR(-ENOENT);

	if (len1 != len2) {
		dev_err(&pdev->dev, "%s: invalid array length(%u,%u)\n",
				__func__, len1, len2);
		return ERR_PTR(-EINVAL);
	}

	pdata->adc_arr_size = len1 / sizeof(u32);
	pdata->adc_table = devm_kzalloc(&pdev->dev,
			sizeof(*pdata->adc_table) * pdata->adc_arr_size,
			GFP_KERNEL);
	if (!pdata->adc_table)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < pdata->adc_arr_size; i++) {
		if (of_property_read_u32_index(np, "adc_array", i, &adc))
			return ERR_PTR(-EINVAL);
		if (of_property_read_u32_index(np, "temp_array", i, &temp))
			return ERR_PTR(-EINVAL);

		pdata->adc_table[i].adc = (int)adc;
		pdata->adc_table[i].temperature = (int)temp;
	}

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	if (of_property_read_u32(np, "history_index", &pdata->history_index) < 0) {
		pr_info("%s : there is no \"history_index\" property\n", __func__);
		pdata->history_index = -1;
	}
#endif

	return pdata;
}
#else
static struct sec_therm_platform_data *
sec_therm_parse_dt(struct platform_device *pdev) { return NULL; }
#endif

static int sec_therm_get_adc_data(struct sec_therm_info *info)
{
	int adc_data;
	int adc_max = 0, adc_min = 0, adc_total = 0;
	int i;

	for (i = 0; i < ADC_SAMPLING_CNT; i++) {
		int ret = iio_read_channel_raw(info->chan, &adc_data);
		if (ret < 0) {
			dev_err(info->dev, "%s : err(%d) returned, skip read\n",
				__func__, adc_data);
			return ret;
		}

		if (i != 0) {
			if (adc_data > adc_max)
				adc_max = adc_data;
			else if (adc_data < adc_min)
				adc_min = adc_data;
		} else {
			adc_max = adc_data;
			adc_min = adc_data;
		}
		adc_total += adc_data;
	}

	return (adc_total - adc_max - adc_min) / (ADC_SAMPLING_CNT - 2);
}

/* CAUTION : This converting algorithm very coupled into Solis_LTE CP */
static int convert_adc_to_cp_temper(unsigned int adc)
{
	int temperature;
	ulong gpadc;
	long scaled_temp = 0;
	long scaling_factor = 1000000;

	gpadc = (unsigned long)adc >> 2;

	if (gpadc <= 89)
		scaled_temp = -2415*gpadc*gpadc + 590196*gpadc - 53479605;
	else if (gpadc <= 232)
		scaled_temp = -308*gpadc*gpadc + 237343*gpadc - 38571121;
	else if (gpadc <= 673)
		scaled_temp = 89007*gpadc - 20300226;
	else if (gpadc <= 878)
		scaled_temp = 278*gpadc*gpadc - 287735*gpadc + 107687285;
	else if (gpadc <= 954)
		scaled_temp = 1770*gpadc*gpadc - 2918099*gpadc + 1267820695;
	else
		scaled_temp = 9801*gpadc*gpadc - 18323445*gpadc + 8655450175;

	if (scaled_temp >= 0)
		temperature = (scaled_temp + (scaling_factor/2))/scaling_factor;
	else
		temperature = (scaled_temp - (scaling_factor/2))/scaling_factor;

	return temperature;
}

static int convert_adc_to_temper(struct sec_therm_info *info, unsigned int adc)
{
	int low = 0;
	int high = 0;
	int mid = 0;
	int temp = 0;
	int temp2 = 0;

	if (!info->pdata->adc_table || !info->pdata->adc_arr_size) {
		/* using fake temp */
		return 300;
	}

	high = info->pdata->adc_arr_size - 1;

	if (info->pdata->adc_table[low].adc >= adc)
		return info->pdata->adc_table[low].temperature;
	else if (info->pdata->adc_table[high].adc <= adc)
		return info->pdata->adc_table[high].temperature;

	while (low <= high) {
		mid = (low + high) / 2;
		if (info->pdata->adc_table[mid].adc > adc)
			high = mid - 1;
		else if (info->pdata->adc_table[mid].adc < adc)
			low = mid + 1;
		else
			return info->pdata->adc_table[mid].temperature;
	}

	temp = info->pdata->adc_table[high].temperature;

	temp2 = (info->pdata->adc_table[low].temperature -
			info->pdata->adc_table[high].temperature) *
			(adc - info->pdata->adc_table[high].adc);

	temp += temp2 /
		(info->pdata->adc_table[low].adc -
			info->pdata->adc_table[high].adc);

	return temp;
}

int sec_therm_get_adc(int therm_id, int *adc)
{
	struct sec_therm_info *info;
	int value;

	list_for_each_entry(info, &therm_info_lists, list) {
		if (info->type_id == therm_id) {
			value = sec_therm_get_adc_data(info);
			if (value < 0) {
				pr_err("%s: failed to get adc for %s\n",
					__func__, info->name);
				return -EINVAL;
			}

			*adc = value;
			break;
		}
	}
	pr_debug("%s: id:%d adc:%d\n", __func__, therm_id, *adc);
	return 0;
}
EXPORT_SYMBOL(sec_therm_get_adc);

int sec_therm_get_temp(int therm_id, int *temp)
{
	struct sec_therm_info *info;
	int adc, t = 0;

	list_for_each_entry(info, &therm_info_lists, list) {
		if (info->type_id == therm_id) {
			adc = sec_therm_get_adc_data(info);
			if (adc < 0) {
				pr_err("%s: failed to get adc for %s\n",
					__func__, info->name);
				return -EINVAL;
			}

			if (info->type_id == TYPE_SEC_THREM_CP_SPEC)
				t = convert_adc_to_cp_temper(adc);
			else
				t = (convert_adc_to_temper(info, adc) / 10);

			break;
		}
	}

	*temp = t;

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
	switch (debug_mode[therm_id]) {
	case TEMPERATURE_CHANGE_DIRECT_VALUE:
		*temp = debug_temp[therm_id];
		break;
	case TEMPERATURE_CHANGE_OFFSET:
		*temp += debug_temp[therm_id];
		break;
	default:
		break;
	}
#endif

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	if (info->pdata->history_index >= 0)
		sec_therm_history_update(info->pdata->history_index, *temp);
#endif

	return 0;
}
EXPORT_SYMBOL(sec_therm_get_temp);


#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
static ssize_t sec_therm_debug_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t cnt)
{
	struct sec_therm_info *info;
	struct sec_therm_device_attribute *attr =
		container_of(devattr, struct sec_therm_device_attribute, dev_attr);
	int temp;

	if (!attr)
		return -ENODEV;

	info = attr->info;

	if (*buf == 'o' && *(buf+1) == ':') {
		buf += 2;
		if (kstrtoint(buf, 10, &temp))
			return -EINVAL;
		debug_mode[info->type_id] = TEMPERATURE_CHANGE_OFFSET;
	} else {
		if (kstrtoint(buf, 10, &temp))
			return -EINVAL;
		debug_mode[info->type_id] = TEMPERATURE_CHANGE_DIRECT_VALUE;
	}

	debug_temp[info->type_id] = temp;

	pr_info("%s: Set therm_id %d temp to %d\n",
			__func__, info->type_id, temp);

	return cnt;
}
#endif


static ssize_t sec_therm_show(struct device *dev,
	struct device_attribute *devattr, char *buf)
{
	struct sec_therm_info *info;
	struct sec_therm_device_attribute *attr =
		container_of(devattr, struct sec_therm_device_attribute, dev_attr);
	int adc, temp;

	info = attr->info;
	adc = sec_therm_get_adc_data(info);

	if (adc < 0)
		return adc;
	else {
		if (info->type_id == TYPE_SEC_THREM_CP_SPEC)
			temp = convert_adc_to_cp_temper(adc);
		else
			temp = (convert_adc_to_temper(info, adc))/10;
	}

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
	switch (debug_mode[info->type_id]) {
	case TEMPERATURE_CHANGE_DIRECT_VALUE:
		temp = debug_temp[info->type_id];
		break;
	case TEMPERATURE_CHANGE_OFFSET:
		temp += debug_temp[info->type_id];
		break;
	default:
		break;
	}
#endif

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	if (info->pdata->history_index >= 0)
		sec_therm_history_update(info->pdata->history_index, temp);
#endif

	return sprintf(buf, "temp:%d adc:%d\n", temp, adc);
}

static ssize_t sec_therm_show_temperature(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc, temp;

	adc = sec_therm_get_adc_data(info);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(info, adc);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t sec_therm_show_temp_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	int adc;

	adc = sec_therm_get_adc_data(info);

	return sprintf(buf, "%d\n", adc);
}

static ssize_t sec_therm_show_name(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", info->name);
}

static SENSOR_DEVICE_ATTR(temperature, S_IRUGO, sec_therm_show_temperature,
		NULL, 0);
static SENSOR_DEVICE_ATTR(temp_adc, S_IRUGO, sec_therm_show_temp_adc, NULL, 0);
static DEVICE_ATTR(name, S_IRUGO, sec_therm_show_name, NULL);

static struct attribute *sec_therm_attributes[] = {
	&sensor_dev_attr_temperature.dev_attr.attr,
	&sensor_dev_attr_temp_adc.dev_attr.attr,
	&dev_attr_name.attr,
	NULL
};

static const struct attribute_group sec_therm_attr_group = {
	.attrs = sec_therm_attributes,
};

static struct sec_therm_info *g_ap_therm_info;
int sec_therm_get_ap_temperature(void)
{
	int adc;
	int temp;

	if (unlikely(!g_ap_therm_info))
		return -ENODEV;

	adc = sec_therm_get_adc_data(g_ap_therm_info);

	if (adc < 0)
		return adc;
	else
		temp = convert_adc_to_temper(g_ap_therm_info, adc);

	return temp;
}

static int sec_therm_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(of_match_ptr(sec_therm_match), &pdev->dev);
	const struct platform_device_id *pdev_id;
	struct sec_therm_platform_data *pdata;
	struct sec_therm_info *info;
	int ret;

	dev_dbg(&pdev->dev, "%s: SEC Thermistor Driver Loading\n", __func__);

	pdata = sec_therm_parse_dt(pdev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);
	else if (pdata == NULL)
		pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "No platform init data supplied.\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	pdev_id = of_id ? of_id->data : platform_get_device_id(pdev);

	info->dev = &pdev->dev;
	info->pdata = pdata;
	strlcpy(info->name, pdev_id->name, sizeof(info->name));
	platform_set_drvdata(pdev, info);

	info->chan = iio_channel_get(&pdev->dev, NULL);
	if (IS_ERR(info->chan)) {
		dev_err(&pdev->dev, "%s: fail to get iio channel\n", __func__);
		return PTR_ERR(info->chan);
	}

	switch (pdev_id->driver_data) {
	case TYPE_SEC_THREM_AP:
	case TYPE_SEC_THREM_BATTERY:
	case TYPE_SEC_THREM_PAM0:
	case TYPE_SEC_THREM_PAM1:
	case TYPE_SEC_THREM_XO:
	case TYPE_SEC_THREM_CAM_FLASH:
	case TYPE_SEC_THREM_CP_SPEC:
		/* Allow only a single device instance for each device type */
		if (sec_therm_single_inst[pdev_id->driver_data])
			return -EPERM;
		else
			sec_therm_single_inst[pdev_id->driver_data] = true;

		info->dev = sec_device_create(info, pdev_id->name);
		if (IS_ERR(info->dev)) {
			dev_err(&pdev->dev, "%s: fail to create sec_dev\n",
					__func__);
			return PTR_ERR(info->dev);
		}
		break;
	default:
		dev_err(&pdev->dev, "%s: Unknown device type: %lu\n", __func__,
				pdev_id->driver_data);
		return -EINVAL;
	}

	ret = sysfs_create_group(&info->dev->kobj, &sec_therm_attr_group);
	if (ret) {
		dev_err(info->dev, "failed to create sysfs group\n");
		goto err_create_sysfs;
	}

	info->hwmon_dev = hwmon_device_register(info->dev);
	if (IS_ERR(info->hwmon_dev)) {
		dev_err(&pdev->dev, "unable to register as hwmon device.\n");
		ret = PTR_ERR(info->hwmon_dev);
		goto err_register_hwmon;
	}

	if (pdev_id->driver_data == TYPE_SEC_THREM_AP)
		g_ap_therm_info = info;

	if (temperature_dev) {
		struct sec_therm_device_attribute *attrs = devm_kzalloc(&pdev->dev,
				sizeof(*attrs), GFP_KERNEL);

		if (!attrs) {
			pr_err("%s: Failed to allocate attrs mem\n", __func__);
			ret = -ENOMEM;
			goto err_register_hwmon;
		}

		attrs->info = info;
		attrs->dev_attr.attr.name = info->name;
		attrs->dev_attr.show = sec_therm_show;
		info->type_id = (unsigned)pdev_id->driver_data;

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
		attrs->dev_attr.store = sec_therm_debug_store;
		attrs->dev_attr.attr.mode = (S_IRUGO | S_IWUGO);
#else
		attrs->dev_attr.store = NULL;
		attrs->dev_attr.attr.mode = S_IRUGO;
#endif

		sysfs_attr_init(&attrs->dev_attr.attr);

		ret = device_create_file(temperature_dev, &attrs->dev_attr);
		if (ret)
			pr_err("%s: Failed to create attr: %d\n", __func__, ret);
	}

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	if (pdata->history_index >= 0)
		sec_therm_history_device_init(pdata->history_index, info->name);
#endif

	list_add_tail(&info->list, &therm_info_lists);
	dev_info(&pdev->dev, "%s successfully probed.\n", pdev_id->name);

	return 0;

err_register_hwmon:
	sysfs_remove_group(&info->dev->kobj, &sec_therm_attr_group);
err_create_sysfs:
	sec_device_destroy(info->dev->devt);
	return ret;
}

static int sec_therm_remove(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(of_match_ptr(sec_therm_match), &pdev->dev);
	struct sec_therm_info *info = platform_get_drvdata(pdev);
	const struct platform_device_id *pdev_id;

	if (!info)
		return 0;

	list_del(&info->list);

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	sec_therm_history_remove(info->pdata->history_index);
#endif

	pdev_id = of_id ? of_id->data : platform_get_device_id(pdev);
	if (pdev_id->driver_data == TYPE_SEC_THREM_AP)
		g_ap_therm_info = NULL;

	hwmon_device_unregister(info->hwmon_dev);
	sysfs_remove_group(&info->dev->kobj, &sec_therm_attr_group);
	iio_channel_release(info->chan);
	sec_device_destroy(info->dev->devt);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		.name = "sec-thermistor",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_therm_match),
	},
	.probe = sec_therm_probe,
	.remove = sec_therm_remove,
};

#ifdef CONFIG_SLEEP_MONITOR
#define MAX_LOG_CNT	(4)

static int sec_therm_get_temp_cb(void *priv, unsigned int *raw, int chk_lv, int caller_type)
{
	struct list_head *info_lists;
	struct sec_therm_info *info;
	unsigned int raw_temp;
	int temp, adc, total_cnt = 0, pretty = 0;

	if (unlikely(!priv)) {
		pr_err("%s: context is null\n", __func__);
		return -ENODEV;
	}

	 info_lists = (struct list_head *)priv;
	*raw = 0;

	if (list_empty(info_lists)) {
		pr_warn("%s: There are no therm_info list!!\n", __func__);
		return pretty;
	}

	list_for_each_entry(info, info_lists, list) {
		if (total_cnt >= MAX_LOG_CNT)
			break;

		adc = sec_therm_get_adc_data(info);
		if (adc < 0) {
			pr_err("%s: failed to get adc for %s\n",
				__func__, info->name);
			continue;
		}

		if (info->type_id == TYPE_SEC_THREM_CP_SPEC)
			temp = convert_adc_to_cp_temper(adc);
		else
			temp = (convert_adc_to_temper(info, adc) / 10);

		if (temp > 0xFF) {
			pr_warn("%s: temp exceed MAX %s/%d\n",
				__func__, info->name, temp);
			raw_temp = 0xFF;
		} else if (temp < 0x01) {
			pr_warn("%s: temp exceed MIN %s/%d\n",
				__func__, info->name, temp);
			raw_temp = 0x01;
		} else {
			raw_temp = (unsigned int)temp;
		}
		pr_debug("%s: temp is %s/%u, %d\n",
				__func__, info->name, raw_temp, temp);

		*raw = *raw | (raw_temp << (info->pdata->history_index * 8));

		if (info->type_id == TYPE_SEC_THREM_AP) {
			if (raw_temp > 70)
				pretty = 0x0F;
			else if (raw_temp < 10)
				pretty = 0x00;
			else
				pretty = (raw_temp - 10) / 4;

			pr_debug("%s: pretty is %d\n", __func__, pretty);
		}
		total_cnt++;
	}

	return pretty;
}

static struct sleep_monitor_ops therm_slp_mon_ops = {
		.read_cb_func = sec_therm_get_temp_cb,
};

#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
static struct sleep_monitor_ops therm_his_slp_mon_ops = {
		.read_cb_func = sec_therm_his_get_temp_cb,
};
#endif

static void sec_therm_slp_mon_enable(struct list_head *info_lists)
{
	int ret;
	ret = sleep_monitor_register_ops(info_lists,
			&therm_slp_mon_ops, SLEEP_MONITOR_TEMP);
	if (ret)
		pr_err("%s: Failed to slp_mon register(%d)\n",
				__func__, ret);
#ifdef CONFIG_SENSORS_SEC_THERM_HISTORY
	ret = sleep_monitor_register_ops(info_lists,
			&therm_his_slp_mon_ops, SLEEP_MONITOR_TEMPMAX);
	if (ret)
		pr_err("%s: Failed to slp_mon register(%d)\n",
				__func__, ret);
#endif

	return;
}

#else
static inline void sec_therm_slp_mon_enable(struct list_head *info_lists) { }
#endif

module_platform_driver(sec_thermistor_driver);

static int __init sec_therm_temperature_dev_init(void)
{
	temperature_dev = sec_device_create(NULL, "temperature");
	if (IS_ERR(temperature_dev)) {
		pr_err("%s: Failed to create temperature dev\n", __func__);
		return -ENODEV;
	}

	sec_therm_slp_mon_enable(&therm_info_lists);

	return 0;
}
fs_initcall(sec_therm_temperature_dev_init);

MODULE_DESCRIPTION("SEC Thermistor Driver");
MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sec-thermistor");
