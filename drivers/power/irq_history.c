/*
 * Copyright (C) 2016 SAMSUNG, Inc.
 * Hunsup Jung <hunsup.jung@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>

#include <linux/power/irq_history.h>
#ifdef CONFIG_PM_SLEEP_HISTORY
#include <linux/power/sleep_history.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif

struct irq_history {
	struct list_head entry;
	char name[IRQ_NAME_LENGTH];
	int irq;
	int count;
	int irq_flag;
	int active;
};

static unsigned int irq_history_flag;
static LIST_HEAD(irq_history_list);
static struct dentry *irq_history_dentry;

static struct irq_history parent_irq[] = {
	{{0,}, "s2mpw01-irq", 0, 0, IRQ_HISTORY_S2MPW01_IRQ, 0},
	{{0,}, "bcmsdh_sdmmc", 0, 0, IRQ_HISTORY_BCMSDH_SDMMC, 0},
};

#ifdef CONFIG_SLEEP_MONITOR
static int irq_history_cb(void *priv, unsigned int *raw_val,
						int check_level, int caller_type)
{
	struct irq_history *iter;
	int hit = 0xf;
	int idx = 0;

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &irq_history_list, entry) {
			if (iter->active == IRQ_ACTIVE) {
				*raw_val = idx;
				iter->active = IRQ_NOT_ACTIVE;
				hit = 1;
				break;
			}
			idx++;
		}
		rcu_read_unlock();
	} else
		/* For cases except resume(e.g. suspend/init/dump) , just return 0 */
		return 0;

	return hit;
}

static struct sleep_monitor_ops slp_mon_irq_history = {
	.read_cb_func = irq_history_cb,
};

/* Print irq history for sleep monitor */
static ssize_t slp_mon_read_irq_history(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	struct irq_history *iter;
	char *buf = NULL;
	ssize_t ret = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, PAGE_SIZE);

	if (*ppos == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%08x]%s", special_key,
					get_type_marker(SLEEP_MONITOR_CALL_IRQ_LIST));
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &irq_history_list, entry)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s/", iter->name);
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		rcu_read_unlock();
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static const struct file_operations slp_mon_irq_history_ops = {
	.read =		slp_mon_read_irq_history,
};

static void register_sleep_monitor_cb(void)
{
	sleep_monitor_register_ops(NULL, &slp_mon_irq_history, SLEEP_MONITOR_IRQ);

	if (slp_mon_d) {
		struct dentry *d = debugfs_create_file("slp_mon_irq", 0400,
				slp_mon_d, NULL, &slp_mon_irq_history_ops);
		if (!d)
			pr_err("%s : debugfs_create_file, error\n", "slp_mon_irq");
	} else
		pr_info("%s - dentry not defined\n", __func__);
}
#endif

unsigned int get_irq_history_flag(void)
{
	return irq_history_flag;
}
EXPORT_SYMBOL_GPL(get_irq_history_flag);

void clear_irq_history_flag(void)
{
	irq_history_flag = 0;
}
EXPORT_SYMBOL_GPL(clear_irq_history_flag);

/* Check that this irq number is parent_irq */
static int is_parent_irq(int irq)
{
	struct irq_desc *desc;
	int i, count;

	desc = irq_to_desc(irq);
	if (!desc || !desc->action || !desc->action->name)
		return 0;

	clear_irq_history_flag();
	count = sizeof(parent_irq) / sizeof(struct irq_history);
	for (i = 0; i < count; i++) {
		if (!strncmp(desc->action->name, parent_irq[i].name, IRQ_NAME_LENGTH)) {
			irq_history_flag |= parent_irq[i].irq_flag;
			return 1;
		}
	}

	return 0;
}

/* Add a irq_history instance to list */
static int add_irq_history_instance(int irq, char *name)
{
	struct irq_history *irq_history_ins, *iter;
	char irq_name[IRQ_NAME_LENGTH] = {0};

	/* If name is NULL */
	if (!name)
		snprintf(irq_name, IRQ_NAME_LENGTH, "IRQ%d", irq);
	else
		strncpy(irq_name, name, IRQ_NAME_LENGTH);
	irq_name[IRQ_NAME_LENGTH - 1] = 0;

	/* Already added irq */
	rcu_read_lock();
	list_for_each_entry_rcu(iter, &irq_history_list, entry) {
		if (!strncmp(iter->name, irq_name, IRQ_NAME_LENGTH)) {
			iter->count++;
			iter->active = IRQ_ACTIVE;
			rcu_read_unlock();
			return 0;
		}
	}

	/* Add new irq */
	irq_history_ins = kmalloc(sizeof(struct irq_history), GFP_ATOMIC);
	if (!irq_history_ins) {
		rcu_read_unlock();
		return -ENOMEM;
	}
	irq_history_ins->irq = irq;
	strncpy(irq_history_ins->name, irq_name, IRQ_NAME_LENGTH);
	irq_history_ins->name[IRQ_NAME_LENGTH - 1] = 0;
	irq_history_ins->count = 1;
	irq_history_ins->active = IRQ_ACTIVE;
	list_add_tail(&irq_history_ins->entry, &irq_history_list);
	rcu_read_unlock();

	return 0;
}

/*
 * Add wakeup reason to irq_history_list
 * name: first priority
 * irq: second priority
 *		should be zero, if name is exist
 */
void add_irq_history(int irq, const char *name)
{
	struct irq_desc *desc;
	char irq_name[IRQ_NAME_LENGTH] = {0};

	if (!irq && !name) {
		pr_info("%s: Invalid argument\n", __func__);
		return;
	}

	/* parent_irq is not saved because child_irq call this func again */
	if (is_parent_irq(irq) && !name)
		return;

	if (name) {
		strncpy(irq_name, name, IRQ_NAME_LENGTH);
		irq_name[IRQ_NAME_LENGTH - 1] = 0;
	} else {
		desc = irq_to_desc(irq);
		if (desc && desc->action && desc->action->name) {
			strncpy(irq_name, desc->action->name, IRQ_NAME_LENGTH);
			irq_name[IRQ_NAME_LENGTH - 1] = 0;
		}
	}

	if (*irq_name && irq)
		pr_info("Resume caused by IRQ %d, %s\n", irq, irq_name);
	else if (*irq_name)
		pr_info("Resume caused by %s\n", irq_name);
	else
		pr_info("Resume caused by IRQ %d\n", irq);

	add_irq_history_instance(irq, irq_name);
#ifdef CONFIG_PM_SLEEP_HISTORY
	sleep_history_marker(SLEEP_HISTORY_WAKEUP_IRQ, NULL, NULL, irq, irq_name);
#endif
#ifdef CONFIG_ENERGY_MONITOR
	energy_monitor_record_wakeup_reason(irq, irq_name);
#endif
}
EXPORT_SYMBOL_GPL(add_irq_history);

static int irq_history_show(struct seq_file *m, void *unused)
{
	struct irq_history *iter;
	unsigned int idx = 1;

	seq_puts(m, "[suspend_count]\n");
	seq_printf(m, "%d\n", suspend_stats.success);
	seq_printf(m, "idx  name            count\n");
	rcu_read_lock();
	list_for_each_entry_rcu(iter, &irq_history_list, entry)
		seq_printf(m, "%3d  %-15s %5d\n",
			idx++, iter->name, iter->count);
	rcu_read_unlock();

	return 0;
}

static int irq_history_open(struct inode *inode, struct file *file)
{
	return single_open(file, irq_history_show, NULL);
}

static const struct  file_operations irq_history_fops = {
	.owner = THIS_MODULE,
	.open = irq_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int irq_history_init(void)
{
	pr_info("%s\n", __func__);

	irq_history_dentry = debugfs_create_file("irq_history", 0440, NULL,
											NULL, &irq_history_fops);

	add_irq_history(0, "UNKNOWN");
#ifdef CONFIG_SLEEP_MONITOR
	register_sleep_monitor_cb();
#endif

	return 0;
}

static void irq_history_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(irq_history_init);
module_exit(irq_history_exit);
