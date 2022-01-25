#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/sec_sysfs.h>

static struct device *gps_dev;
struct class *gps_class;
EXPORT_SYMBOL_GPL(gps_class);

static unsigned int gps_pwr_on;
static struct platform_device bcm477x = {
	.id = -1,
};

static ssize_t show_gps_pwr_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	ret = gpio_get_value(gps_pwr_on);

	pr_info("%s:%d GPIO_GPS_PWR_EN is %d\n", __func__, __LINE__, ret);
	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}

static ssize_t set_gps_pwr_en(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int enable;

	if (kstrtoint(buf, 10, &enable) < 0)
		return -EINVAL;

	pr_info("%s:%d endable:%d\n", __func__, __LINE__, enable);
	if (enable)
		gpio_direction_output(gps_pwr_on, 1);
	else
		gpio_direction_output(gps_pwr_on, 0);

	return size;
}

static DEVICE_ATTR(gps_pwr_en, 0640, show_gps_pwr_en, set_gps_pwr_en);

static const struct of_device_id gps_match_table[] = {
	{
		.compatible = "broadcom,bcm4774",
		/* specific node name for Tizen_FW */
		.data = (void *)"bcm4774",
	},
	{
		.compatible = "broadcom,bcm4775",
		/* specific node name for Tizen_FW */
		.data = (void *)"bcm47752",
	},
	{},
};

static int __init gps_bcm477x_init(void)
{
	const char *gps_pwr_en = "gps-pwr-en";
	const struct of_device_id *matched_np;
	struct device_node *np;
	int ret = 0;

	pr_info("%s\n", __func__);

	np = of_find_matching_node_and_match(NULL, gps_match_table,
			&matched_np);
	if (!np) {
		pr_err("failed to get any device nodes for bcm_gps\n");
		return -ENODEV;
	}

	bcm477x.name = (char *)matched_np->data;
	ret = platform_device_register(&bcm477x);
	if (ret) {
		pr_err("failed to pdev register for bcm_gps(%d)\n", ret);
		return ret;
	}

	if (!gps_class) {
		gps_class = class_create(THIS_MODULE, "gps");
		if (IS_ERR(gps_class)) {
			ret = PTR_ERR(gps_class);
			goto err_pdev_register;
		}
	}

	gps_dev = device_create(gps_class, NULL, 0, NULL, "gps");
	if (IS_ERR(gps_dev)) {
		pr_err("%s Failed to create device(gps)!\n", __func__);
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	gps_pwr_on = of_get_named_gpio(np, gps_pwr_en, 0);
	if (!gpio_is_valid(gps_pwr_on)) {
		pr_err("%s: Invalid gpio pin : %d\n", __func__, gps_pwr_on);
		ret = -ENODEV;
		goto err_find_node;
	}

	ret = gpio_request(gps_pwr_on, "GPS_PWR_EN");
	if (ret) {
		pr_err("%s, fail to request gpio(GPS_PWR_EN)\n", __func__);
		goto err_find_node;
	}

	gpio_direction_output(gps_pwr_on, 0);

	ret = device_create_file(gps_dev, &dev_attr_gps_pwr_en);
	if (ret) {
		pr_err("%s, fail to create file gps_pwr_en\n", __func__);
		goto err_find_node;
	}

	return 0;

err_find_node:
	device_destroy(gps_class, 0);
err_sec_device_create:
	class_destroy(gps_class);
	gps_class = NULL;
err_pdev_register:
	platform_device_unregister(&bcm477x);

	return ret;
}

device_initcall(gps_bcm477x_init);
