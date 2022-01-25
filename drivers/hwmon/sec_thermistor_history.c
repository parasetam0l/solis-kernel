#include <linux/debugfs.h>
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
#include <linux/platform_data/sec_thermistor_history.h>
#include <linux/sec_sysfs.h>
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif

struct sec_therm_history_info* history_table[MAX_SEC_THERM_DEVICE_NUM];
char sec_therm_dev_name[MAX_SEC_THERM_DEVICE_NUM][SEC_THERM_NAME_LEN];

void sec_therm_history_remove(int device_num)
{
	if (!history_table[device_num]) {
		pr_debug("%s: device(%d) has no history table.\n", __func__, device_num);
		return;
	}
	kfree(history_table[device_num]);
}

void sec_therm_history_update(int device_num, int temp)
{
	int group_num;

	if (device_num < 0 || device_num >= MAX_SEC_THERM_DEVICE_NUM) {
		pr_debug("%s: history is not checked for device(%d).\n", __func__, device_num);
		return;
	}
	if (!history_table[device_num]) {
		pr_debug("%s: device(%d) has no history table.\n", __func__, device_num);
		return;
	}
	if (temp > SEC_THERM_TEMP_MAX || temp < SEC_THERM_TEMP_MIN) {
		pr_info("%s: temp %d is out of range.\n", __func__, temp);
		return;
	}

	for(group_num = 0; group_num < MAX_SEC_THERM_GROUP_NUM; group_num++)
	{
		/* To prevent to send initialized value to MONITOR,
		   reset data before get new data. */
		if (history_table[device_num][group_num].reset)
			sec_therm_history_reset(&history_table[device_num][group_num]);

		if (temp > history_table[device_num][group_num].max)
			history_table[device_num][group_num].max = temp;
		if (temp < history_table[device_num][group_num].min)
			history_table[device_num][group_num].min = temp;
		history_table[device_num][group_num].cnt++;
		history_table[device_num][group_num].sum += temp;
		pr_debug("%s: [%d][%d] max %d, min %d, cnt %d, sum %d\n",
				__func__, device_num, group_num,
				history_table[device_num][group_num].max,
				history_table[device_num][group_num].min,
				history_table[device_num][group_num].cnt,
				history_table[device_num][group_num].sum);
	}
}

void sec_therm_history_reset(struct sec_therm_history_info *history)
{
	history->round++;
	history->max = SEC_THERM_TEMP_MIN;
	history->min = SEC_THERM_TEMP_MAX;
	history->cnt = 0;
	history->sum = 0;
	history->reset = 0;
}

void sec_therm_history_device_init(int device_num, char* name)
{
	int group_num;

	if (device_num < 0 || device_num >= MAX_SEC_THERM_DEVICE_NUM) {
		pr_debug("%s: history is not checked for device(%d).\n", __func__, device_num);
		return;
	}

	history_table[device_num] = kzalloc(
			sizeof(struct sec_therm_history_info) * MAX_SEC_THERM_GROUP_NUM,
			GFP_KERNEL);

	if (!history_table[device_num]) {
		pr_err("%s: FAIL to memory alloc for history_info\n", __func__);
		return;
	}

	for(group_num = 0; group_num < MAX_SEC_THERM_GROUP_NUM; group_num++) {
		history_table[device_num][group_num].round = 0;
		history_table[device_num][group_num].max = SEC_THERM_TEMP_MIN;
		history_table[device_num][group_num].min = SEC_THERM_TEMP_MAX;
		history_table[device_num][group_num].cnt = 0;
		history_table[device_num][group_num].sum = 0;
		history_table[device_num][group_num].reset = 0;
	}
	strncpy(sec_therm_dev_name[device_num], name, SEC_THERM_NAME_LEN-1);
	sec_therm_dev_name[device_num][SEC_THERM_NAME_LEN-1] = '\0';
	pr_info("%s: history_table[%d], %s is initialized.\n", __func__, device_num, sec_therm_dev_name[device_num]);
}

#ifdef CONFIG_ENERGY_MONITOR
int get_sec_therm_history_energy_mon(int type, struct sec_therm_history_info *eng_history)
{
	int i;

	for(i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++) {
		if (!history_table[i]) {
			pr_debug("%s: device(%d) has no history table.\n", __func__, i);
			eng_history[i].reset = 1;
			continue;
		}
		eng_history[i] = history_table[i][SEC_THERM_HISTORY_ENERGY];
		pr_debug("%s: [%d:%d] %d~%d, %d[%d/%d]\n", __func__,
				i, eng_history[i].round,
				eng_history[i].min, eng_history[i].max,
				eng_history[i].sum / eng_history[i].cnt,
				eng_history[i].sum, eng_history[i].cnt);
		if (type != ENERGY_MON_TYPE_DUMP)
			history_table[i][SEC_THERM_HISTORY_ENERGY].reset = 1;
	}

	return 0;
}
#endif

#ifdef CONFIG_SLEEP_MONITOR
int sec_therm_his_get_temp_cb(void *priv, unsigned int *raw, int chk_lv, int caller_type)
{
	int device_num, temp, raw_temp, pretty = 0;

	for (device_num = 0; device_num < MAX_SEC_THERM_DEVICE_NUM; device_num++) {
		if (!history_table[device_num]) {
			pr_debug("%s: device(%d) has no history table.\n", __func__, device_num);
			continue;
		}
		temp = history_table[device_num][SEC_THERM_HISTORY_SLEEP].max;
		history_table[device_num][SEC_THERM_HISTORY_SLEEP].reset = 1;

		if (temp > 0xFF) {
			pr_warn("%s: temp exceed MAX [%d]/%d\n",
				__func__, device_num, temp);
			raw_temp = 0xFF;
		} else if (temp < 0x01) {
			pr_warn("%s: temp exceed MIN [%d]/%d\n",
				__func__, device_num, temp);
			raw_temp = 0x01;
		} else {
			raw_temp = (unsigned int)temp;
		}
		pr_debug("%s: temp is [%d]/%u, %d\n",
				__func__, device_num, raw_temp, temp);

		*raw = *raw | (raw_temp << (device_num * 8));

		if (device_num == SEC_THERM_HISTORY_AP_THERM) {
			if (raw_temp > 70)
				pretty = 0x0F;
			else if (raw_temp < 10)
				pretty = 0x00;
			else
				pretty = (raw_temp - 10) / 4;

			pr_debug("%s: pretty is %d\n", __func__, pretty);
		}
	}
	return pretty;
}
#endif

static int therm_history_show(struct seq_file *m, void *unused)
{
	int i, ret = 0;
	char buf[400];

	seq_printf(m, "[sec_therm_history]\n");
	for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
		ret += snprintf(buf + ret, sizeof(buf),
				"/_____ %12s ______", sec_therm_dev_name[i]); /* len : 25 */
	ret += snprintf(buf + ret, sizeof(buf), "\n");
	for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++)
		ret += snprintf(buf + ret, sizeof(buf), "/MIN/MAX/AVG/____SUM/__CNT");
	ret += snprintf(buf + ret, sizeof(buf), "\n");

	for (i = 0; i < MAX_SEC_THERM_DEVICE_NUM; i++) {
		if (!history_table[i]) {
			pr_debug("%s: device(%d) has no history table.\n", __func__, i);
			snprintf(buf + ret, sizeof(buf), "/  -/  -/  -/      -/    -");
		} else {
			ret += snprintf(buf + ret, sizeof(buf),	"/%3d/%3d/%3d/%7d/%5d",
				history_table[i][SEC_THERM_HISTORY_BOOT].min, history_table[i][SEC_THERM_HISTORY_BOOT].max,
				history_table[i][SEC_THERM_HISTORY_BOOT].sum / history_table[i][SEC_THERM_HISTORY_BOOT].cnt,
				history_table[i][SEC_THERM_HISTORY_BOOT].sum, history_table[i][SEC_THERM_HISTORY_BOOT].cnt);
		}
	}

	seq_printf(m, "%s\n", buf);

	return 0;
}   

static int therm_history_open(struct inode *inode, struct file *file)
{ 
	return single_open(file, therm_history_show, NULL);
}

static const struct  file_operations therm_history_fops = {
	.owner = THIS_MODULE,
	.open = therm_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int therm_history_init(void)
{
	int device_num;

    pr_info("%s\n", __func__);

	for (device_num = 0; device_num < MAX_SEC_THERM_DEVICE_NUM; device_num++) {
		strncpy(sec_therm_dev_name[device_num], "NULL", 4);
	}

    debugfs_create_file("therm_history", 0440, NULL, NULL, &therm_history_fops);

			    return 0;
}

static void therm_history_exit(void)
{
	    pr_info("%s\n", __func__);
}

module_init(therm_history_init);
module_exit(therm_history_exit);
