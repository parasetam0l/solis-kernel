/*
 * Bluetooth Broadcom GPIO and Low Power Mode control
 *
 *  Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *  Copyright (C) 2011 Google, Inc.
 *  Copyright (C) 2016, Broadcom Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id$
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/serial_core.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/wlan_plat.h>

#include <linux/gpio.h>
#include <linux/version.h>
#include "bcmbtsdio.h"

static int gpio_bt_en = -1;
static int gpio_bt_wake = -1;
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
static int gpio_bt_host_wake = -1;
#endif
static int bt_power_state = BT_POWERED_OFF;

static struct rfkill *bt_rfkill;

struct bcm_bt_lpm {
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	struct wake_lock host_wake_lock;
#endif
	struct wake_lock bt_wake_lock;
	char wake_lock_name[100];
} bt_lpm;


/* BT power state change */
int (*g_power_state_handler)(int) = NULL;

/* BT driver invokes the below API to set the callback */
void bt_power_state_register(int (*power_state_handler)(int))
{
	printk("power_state_handler\n");
	g_power_state_handler = power_state_handler;
}
EXPORT_SYMBOL(bt_power_state_register);

/*
* Broadcom Bluetooth platform driver
*/
static int bcm43012_bt_rfkill_set_power(void *data, bool blocked)
{
	pr_info("[BT] RFKILL state : %d\n", bt_power_state);
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	if (!blocked) {
		pr_info("[BT] POWERING ON\n");

		/* update BT driver about BT_POWERING_ON prior to setting BT_REG_ON */
		if (bt_power_state != BT_POWERING_ON &&
			bt_power_state != BT_POWERED_ON) {
			bt_power_state = BT_POWERING_ON;
			if ((g_power_state_handler) (bt_power_state)) {
				pr_err("[BT] Failed Wi-Fi initialize\n");
				bt_power_state = BT_POWERED_OFF;
				return -ENODEV;
			}
			gpio_set_value(gpio_bt_en, 1);

			/* update BT driver about BT_POWERED_ON after setting BT_REG_ON */
			bt_power_state = BT_POWERED_ON;
			(g_power_state_handler)(bt_power_state);
		} else {
			pr_info("[BT] ignore BT_POWER_ON cmd during powering on\n");
		}
		pr_info("[BT] POWERED ON\n");
	} else {
		pr_info("[BT] POWERING OFF\n");

		if (bt_power_state != BT_POWERING_OFF &&
				bt_power_state != BT_POWERED_OFF) {
			/* update BT driver about BT_POWERING_OFF after setting BT_REG_ON */
			bt_power_state = BT_POWERING_OFF;
			(g_power_state_handler)(bt_power_state);

			gpio_set_value(gpio_bt_en, 0);

			/* update BT driver about BT_POWERED_OFF after setting BT_REG_ON */
			bt_power_state = BT_POWERED_OFF;
			(g_power_state_handler)(bt_power_state);
		} else {
			pr_info("[BT] ignore BT_POWER_OFF cmd during powering off\n");
		}

		pr_info("[BT] POWERED OFF\n");
	}
	return 0;
}

static const struct rfkill_ops bcm43012_bt_rfkill_ops = {
	.set_block = bcm43012_bt_rfkill_set_power,
};

void bt_set_power(void *data, bool blocked)
{
	pr_info("[BT] bt_set_power: blocked[%d]\n", blocked);

	if (blocked) {
		gpio_set_value(gpio_bt_en, 0);
	} else {
		gpio_set_value(gpio_bt_en, 1);
	}
}
EXPORT_SYMBOL(bt_set_power);

#ifdef BT_LPM_ENABLE
static void set_wake_locked(int wake)
{
	static int state = 0;

	if (state == wake) {
		return;
	}
	state = wake;
	if (wake)
		wake_lock(&bt_lpm.bt_wake_lock);
	else
		wake_unlock(&bt_lpm.bt_wake_lock);
	gpio_set_value(gpio_bt_wake, wake);
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	set_wake_locked(0);

	return HRTIMER_NORESTART;
}

static void bcm_bt_lpm_exit_lpm_locked(void)
{
	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);
	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
}

#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	bt_is_running = 1;

	if (host_wake) {
		wake_lock(&bt_lpm.host_wake_lock);
	} else  {
		/* Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		wake_lock_timeout(&bt_lpm.host_wake_lock, HZ/2);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;

	host_wake = gpio_get_value(gpio_bt_host_wake);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	update_host_wake_locked(host_wake);

	return IRQ_HANDLED;
}
#endif /* CONFIG_BCMDHD_SHARING_WIFI_OOB */

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	int irq;
	int ret;
#endif
	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(0, BT_WAKE_DELAY_NS);
	bt_lpm.enter_lpm_timer.function = enter_lpm;
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	bt_lpm.host_wake = 0;

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name), "BT_host_wake");
	wake_lock_init(&bt_lpm.host_wake_lock, WAKE_LOCK_SUSPEND, bt_lpm.wake_lock_name);
#endif
	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name), "BCM_BT_WAKE_LOCK");
	wake_lock_init(&bt_lpm.bt_wake_lock, WAKE_LOCK_SUSPEND,	 bt_lpm.wake_lock_name);
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	irq = gpio_to_irq(gpio_bt_host_wake);

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		pr_err("[BT] Set_irq_wake failed.\n");
		return ret;
	}
#endif
	return 0;
}

void bt_set_wake(int bAssert)
{
	if (bAssert) {
		bcm_bt_lpm_exit_lpm_locked();
	}
}
EXPORT_SYMBOL(bt_set_wake);
#endif /* BT_LPM_ENABLE */

int bcm43012_bluetooth_init_gpio(void)
{
	const char *bt_node = "samsung,bcm43012_bluetooth";
	struct device_node *root_node = NULL;

	root_node = of_find_compatible_node(NULL, NULL, bt_node);
	if (!root_node) {
		pr_err("[BT] failed to get device node of bcm43012\n");
		return -ENODEV;
	}

	/* ========== BLUETOOTH_PWR_EN ============ */
	gpio_bt_en = of_get_named_gpio(root_node, "bluetooth_pwr_en", 0);
	if (!gpio_is_valid(gpio_bt_en)) {
		pr_err("[BT] Invalied gpio pin : %d\n", gpio_bt_en);
		return -ENODEV;
	}

	/* ========== BLUETOOTH_WAKE ============ */
	gpio_bt_wake = of_get_named_gpio(root_node, "bluetooth_wake", 0);
	if (!gpio_is_valid(gpio_bt_wake)) {
		pr_err("[BT] Invalied gpio pin : %d\n", gpio_bt_wake);
		return -ENODEV;
	}
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	/* ========== BLUETOOTH_HOST_WAKE ============ */
	gpio_bt_host_wake = of_get_named_gpio(root_node, "bluetooth_host_wake", 0);
	if (!gpio_is_valid(gpio_bt_host_wake)) {
		pr_err("[BT] Invalied gpio pin : %d\n", gpio_bt_host_wake);
		return -ENODEV;
	}
#endif
	return 0;
}

static int bcm43012_bluetooth_probe(struct platform_device *pdev)
{
	struct pinctrl	*pinctrl;
	struct pinctrl_state	*clk_drive_base;
	int rc = 0;
	int ret;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!pinctrl) {
		pr_err("[BT] devm_pinctrl_get failed.\n");
		return -ENODEV;
	}

	clk_drive_base = pinctrl_lookup_state(pinctrl, "default");
	pinctrl_select_state(pinctrl, clk_drive_base);

	ret = bcm43012_bluetooth_init_gpio();
	if (ret < 0) {
		pr_err("[BT] failed to initiate GPIO, ret=%d\n", ret);
		return ret;
	}

	rc = gpio_request(gpio_bt_en, "bcm43012_bten_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] gpio_bt_en request failed.\n");
		return rc;
	}

	rc = gpio_request(gpio_bt_wake, "bcm43012_btwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] gpio_bt_wake request failed.\n");
		gpio_free(gpio_bt_en);
		return rc;
	}
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	rc = gpio_request(gpio_bt_host_wake, "bcm43012_bthostwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] gpio_bt_host_wake request failed.\n");
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_en);
		return rc;
	}

	gpio_direction_input(gpio_bt_host_wake);
#endif
	gpio_direction_output(gpio_bt_wake, 0);
	gpio_direction_output(gpio_bt_en, 0);

	bt_rfkill = rfkill_alloc("bcm43012 Bluetooth",  &pdev->dev, RFKILL_TYPE_BLUETOOTH,
								&bcm43012_bt_rfkill_ops, NULL);

	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
		gpio_free(gpio_bt_host_wake);
#endif
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_en);
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, 0);

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rfkill_destroy(bt_rfkill);
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
		gpio_free(gpio_bt_host_wake);
#endif
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_en);
		return -1;
	}

	rfkill_set_sw_state(bt_rfkill, true);

#ifdef BT_LPM_ENABLE
	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
		gpio_free(gpio_bt_host_wake);
#endif
		gpio_free(gpio_bt_wake);
		gpio_free(gpio_bt_en);
	}
#endif /* BT_LPM_ENABLE */
	return rc;
}

static int bcm43012_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(gpio_bt_en);
	gpio_free(gpio_bt_wake);
#ifndef CONFIG_BCMDHD_SHARING_WIFI_OOB
	gpio_free(gpio_bt_host_wake);
	wake_lock_destroy(&bt_lpm.host_wake_lock);
#endif
	wake_lock_destroy(&bt_lpm.bt_wake_lock);
	bt_power_state = BT_POWERED_OFF;

	return 0;
}

static const struct of_device_id bcm43012_of_match[] = {
	{ .compatible = "samsung,bcm43012_bluetooth" },
	{ /* sentinel */ }
};

static struct platform_driver bcm43012_bluetooth_platform_driver = {
	.probe  = bcm43012_bluetooth_probe,
	.remove = bcm43012_bluetooth_remove,
	.driver = {
		.name	        = "bcm43012_bluetooth",
		.of_match_table = bcm43012_of_match,
		.owner	        = THIS_MODULE,
	},
};

static int __init bcm43012_bluetooth_init(void)
{
	return platform_driver_register(&bcm43012_bluetooth_platform_driver);
}

static void __exit bcm43012_bluetooth_exit(void)
{
	platform_driver_unregister(&bcm43012_bluetooth_platform_driver);
}

module_init(bcm43012_bluetooth_init);
module_exit(bcm43012_bluetooth_exit);

MODULE_ALIAS("platform:bcm43012");
MODULE_DESCRIPTION("bcm43012_bluetooth");
MODULE_LICENSE("GPL");
