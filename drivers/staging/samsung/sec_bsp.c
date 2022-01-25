/* sec_bsp.c
 *
 * Copyright (C) 2014 Samsung Electronics
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/sec_bsp.h>
#include <linux/sec_sysfs.h>

struct boot_event {
	unsigned int type;
	const char *string;
	unsigned int time;
};

extern struct class *sec_class;

enum boot_events_type {
	PLATFORM_BOOT_COMPLETE,
	PLATFORM_ENABLE_SCREEN,
	PLATFORM_VOICE_SVC,
	PLATFORM_DATA_SVC,
	PLATFORM_SETRADIOPOWER_ON,
};

static struct boot_event boot_events[] = {
	{PLATFORM_BOOT_COMPLETE, "!@Boot: bootcomplete", 0},
	{PLATFORM_ENABLE_SCREEN, "!@Boot: Enabling Screen!", 0},
	{PLATFORM_VOICE_SVC, "!@Boot: Voice SVC is acquired", 0},
	{PLATFORM_DATA_SVC, "!@Boot: Data SVC is acquired", 0},
	{PLATFORM_SETRADIOPOWER_ON, "!@Boot_SVC : setRadioPower on", 0},
	{0, NULL, 0},
};

static int sec_boot_stat_proc_show(struct seq_file *m, void *v)
{
	unsigned int i;
	int delta;

	i = 0;
	delta = 0;

	seq_printf(m, "boot event                      time (ms)" \
				"        delta\n");
	seq_printf(m, "-----------------------------------------" \
				"------------\n");

	while (boot_events[i].string != NULL) {
		seq_printf(m, "%-35s : %5u    %5d\n", boot_events[i].string,
				boot_events[i].time, delta);
		delta = boot_events[i+1].time - \
			boot_events[i].time;
		i = i + 1;
	}

	return 0;
}

static int sec_boot_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_boot_stat_proc_show, NULL);
}

static const struct file_operations sec_boot_stat_proc_fops = {
	.open    = sec_boot_stat_proc_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

void sec_boot_stat_add(const char *c)
{
	int i;
	unsigned long long t;

	i = 0;
	while (boot_events[i].string != NULL) {
		if (strcmp(c, boot_events[i].string) == 0) {
			t = local_clock();
			do_div(t, 1000000);
			boot_events[i].time = (unsigned int)t;
			break;
		}
		i = i + 1;
	}
}

static struct device *sec_bsp_dev;

static ssize_t store_boot_stat(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long t;

	if (!strncmp(buf, "Voice SVC is acquired", 21)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[PLATFORM_VOICE_SVC].time = (unsigned int)t;
	}
	if (!strncmp(buf, "Data SVC is acquired", 20)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[PLATFORM_DATA_SVC].time = (unsigned int)t;
	}
	if (!strncmp(buf, "setRadioPower on", 16)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[PLATFORM_SETRADIOPOWER_ON].time = (unsigned int)t;
	}
	if (!strncmp(buf, "Enabling Screen!", 16)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[PLATFORM_ENABLE_SCREEN].time = (unsigned int)t;
	}
	if (!strncmp(buf, "bootcomplete", 12)) {
		t = local_clock();
		do_div(t, 1000000);
		boot_events[PLATFORM_BOOT_COMPLETE].time = (unsigned int)t;
	}
	return count;
}

static DEVICE_ATTR(boot_stat, S_IWUSR | S_IWGRP, NULL, store_boot_stat);

static int __init sec_bsp_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("boot_stat", S_IRUGO, NULL,
							&sec_boot_stat_proc_fops);
	if (!entry)
		return -ENOMEM;

	sec_bsp_dev = sec_device_create(NULL, "bsp");
	BUG_ON(!sec_bsp_dev);
	if (IS_ERR(sec_bsp_dev))
		pr_err("%s:Failed to create devce\n", __func__);

	if (device_create_file(sec_bsp_dev, &dev_attr_boot_stat) < 0)
		pr_err("%s: Failed to create device file\n", __func__);

	return 0;
}

module_init(sec_bsp_init);
