#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <linux/gpio.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#endif
#include <linux/battery/sec_battery.h>
#include <linux/sec_batt.h>
#include <linux/time_history.h>

#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>
#include <linux/soc/samsung/exynos-soc.h>



#define EXYNOS_INFORM2 0x808
#define EXYNOS_INFORM3 0x80c
#define EXYNOS_PS_HOLD_CONTROL 0x330c


void (*mach_restart)(char str, const char *cmd);
EXPORT_SYMBOL(mach_restart);

static void sec_power_off(void)
{
	int poweroff_try = 0;
	unsigned int ps_hold_control;
	union power_supply_propval ac_val, usb_val, wpc_val;

#ifndef CONFIG_KEYBOARD_S2MPW01
#ifdef CONFIG_OF
	int powerkey_gpio = -1;
	struct device_node *np, *pp;

	np = of_find_node_by_path("/gpio_keys");
	if (!np)
		return;
	for_each_child_of_node(np, pp) {
		uint keycode = 0;
		if (!of_find_property(pp, "gpios", NULL))
			continue;
		of_property_read_u32(pp, "linux,code", &keycode);
		if (keycode == KEY_POWER) {
			pr_info("%s: <%u>\n", __func__,  keycode);
			powerkey_gpio = of_get_gpio(pp, 0);
			break;
		}
	}
	of_node_put(np);

	if (!gpio_is_valid(powerkey_gpio)) {
		pr_err("Couldn't find power key node\n");
		return;
	}
#else
	int powerkey_gpio = GPIO_nPOWER;
#endif
#endif

	local_irq_disable();

	psy_do_property("ac", get, POWER_SUPPLY_PROP_ONLINE, ac_val);
	psy_do_property("usb", get, POWER_SUPPLY_PROP_ONLINE, usb_val);
	psy_do_property("wireless", get, POWER_SUPPLY_PROP_ONLINE, wpc_val);
	pr_info("[%s] AC[%d] : USB[%d], WPC[%d]\n", __func__,
		ac_val.intval, usb_val.intval, wpc_val.intval);

	while (1) {
		/* Check reboot charging */
		if (ac_val.intval || usb_val.intval || wpc_val.intval || (poweroff_try >= 5)) {

			pr_emerg("%s: charger connected() or power"
			     "off failed(%d), reboot!\n",
			     __func__, poweroff_try);
			/* To enter LP charging */
			exynos_pmu_write(EXYNOS_INFORM2, 0x0);

			flush_cache_all();
			//outer_flush_all();
			mach_restart(0, 0);

			pr_emerg("%s: waiting for reboot\n", __func__);
			while (1)
				;
		}
/* New PMIC(S2MPW01) do not support HW power key,
    so it is impossible to use gpio to check power key state */
#ifdef CONFIG_KEYBOARD_S2MPW01
		if (1) { /*TODO:: Need to replace to proper function */
#else
		/* wait for power button release */
		if (gpio_get_value(powerkey_gpio)) {
#endif
			pr_emerg("%s: set PS_HOLD low\n", __func__);

			/* power off code
			 * PS_HOLD Out/High -->
			 * Low PS_HOLD_CONTROL, R/W, 0x1002_330C
			 */
			exynos_pmu_read(EXYNOS_PS_HOLD_CONTROL,&ps_hold_control);
			exynos_pmu_write( EXYNOS_PS_HOLD_CONTROL, ps_hold_control & 0xFFFFFEFF);

			++poweroff_try;

			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, poweroff_try);
		} else {
		/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton is not released.\n", __func__);
		}

		mdelay(1000);
	}
}

#ifdef CONFIG_LCD_RES
enum lcd_res_type {
	LCD_RES_DEFAULT = 0,
	LCD_RES_FHD = 1920,
	LCD_RES_HD = 1280,
	LCD_RES_MAX
};
#endif

#define REBOOT_MODE_PREFIX      0x12345670
#define REBOOT_MODE_NONE        0x0
#define REBOOT_MODE_DOWNLOAD    0x1
#define REBOOT_MODE_UPLOAD      0x2
#define REBOOT_MODE_CHARGING    0x3
#define REBOOT_MODE_RECOVERY    0x4
#define REBOOT_MODE_FOTA        0x5
#define REBOOT_MODE_FOTA_BL     0x6   /* update bootloader */
#define REBOOT_MODE_SECURE      0x7   /* image secure check fail */
#define REBOOT_MODE_NORMAL      0x8   /* Normal reboot */
#define REBOOT_MODE_FWUP        0x9   /* emergency firmware update */
#define REBOOT_MODE_SILENT      0xA   /* silent reboot */
#define REBOOT_MODE_WDOWNLOAD   0xB   /* wireless-download */
#define REBOOT_MODE_WUPLOAD     0xC   /* wireless-upload */
#define REBOOT_MODE_WDOWNLOAD_UPDATE  0xD /* wireless-download update binary */

#define REBOOT_MODE_POW_PREFIX 0xaec80000 /*poweroff watch prefix*/
#define REBOOT_MODE_FACTORY_COMMAND     0x12345611
#define REBOOT_MODE_WDOWNLOAD_RW_UPDATE 0x12345612

#define REBOOT_SET_PREFIX      0xabc00000
#define REBOOT_SET_SALES_CODE  0x000a0000
#define REBOOT_SET_LCD_RES     0x000b0000
#define REBOOT_SET_DEBUG       0x000d0000
#define REBOOT_SET_SWSEL       0x000e0000
#define REBOOT_SET_SUD         0x000f0000

#define REBOOT_SALES_CODE_BASE_PHYS     (0x40000000)
#define REBOOT_SALES_CODE_MAGIC_ADDR    (phys_to_virt(REBOOT_SALES_CODE_BASE_PHYS))
#define REBOOT_SALES_CODE_ADDR          (phys_to_virt(REBOOT_SALES_CODE_BASE_PHYS+4))

static void sec_reboot(char str, const char *cmd)
{
	local_irq_disable();

	pr_emerg("%s (%d, %s)\n", __func__, str, cmd ? cmd : "(null)");

	exynos_pmu_write(EXYNOS_INFORM2, 0x12345678);

	if (!cmd) {
		exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_PREFIX | REBOOT_MODE_NONE);
	} else {
		unsigned long value;
		if (!strncmp(cmd, "custom", 6))
			cmd += 6;
		if (!strcmp(cmd, "fota"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA);
		else if (!strcmp(cmd, "fota_bl"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA_BL);
		else if (!strcmp(cmd, "recovery"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_RECOVERY);
		else if (!strcmp(cmd, "download"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_DOWNLOAD);
		else if (!strcmp(cmd, "upload"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_UPLOAD);
		else if (!strcmp(cmd, "secure"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_SECURE);
		else if (!strcmp(cmd, "fwup"))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_FWUP);
		else if (!strcmp(cmd, "silent"))
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_PREFIX | REBOOT_MODE_SILENT);
		else if (!strcmp(cmd, "wdownload"))
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_PREFIX | REBOOT_MODE_WDOWNLOAD);
		else if (!strcmp(cmd, "wdownload_update"))
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_PREFIX | REBOOT_MODE_WDOWNLOAD_UPDATE);
		else if (!strcmp(cmd, "wdownload_rw_update"))
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_WDOWNLOAD_RW_UPDATE);
		else if (!strcmp(cmd, "wupload"))
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_MODE_PREFIX | REBOOT_MODE_WUPLOAD);
#ifdef CONFIG_LCD_RES
		else if (!strncmp(cmd, "lcdres_", 7)) {
			if( !strcmp(cmd, "lcdres_fhd") ) value = LCD_RES_FHD;
			else if( !strcmp(cmd, "lcdres_hd") ) value = LCD_RES_HD;
			else value = LCD_RES_DEFAULT;
			pr_info( "%s : lcd_res, %d\n", __func__, (int)value );
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_SET_PREFIX | REBOOT_SET_LCD_RES | value);
		}
#endif
		else if (!strncmp(cmd, "csc", 3) && (cmd + 3) != NULL) {
			memset(REBOOT_SALES_CODE_MAGIC_ADDR, 0, 8);
			writel(REBOOT_SET_PREFIX | REBOOT_SET_SALES_CODE, REBOOT_SALES_CODE_MAGIC_ADDR);
			snprintf(REBOOT_SALES_CODE_ADDR, 4, "%s", cmd + 3);
			exynos_pmu_write(EXYNOS_INFORM3, REBOOT_SET_PREFIX | REBOOT_SET_SALES_CODE);
		} else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_SET_PREFIX | REBOOT_SET_DEBUG | value);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_SET_PREFIX | REBOOT_SET_SWSEL | value);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_SET_PREFIX | REBOOT_SET_SUD | value);
		else if (!strncmp(cmd, "emergency", 9))
			exynos_pmu_write(EXYNOS_INFORM3, 0x0);
		else if (!strncmp(cmd, "panic", 5)){
			/*
			 * This line is intentionally blanked because the INFORM3 is used for upload cause
			 * in sec_debug_set_upload_cause() only in case of  panic() .
			 */
		}
		else if (!strncmp(cmd, "pow", 3)) {
			int tz, sign = 0;
			int min, hr;
			tz = alarm_get_tz();
			if (tz<0) {
				sign = 1;
				tz = tz * -1;
			}
			hr = tz / 3600 ;
			min = (tz / 60) % 60 ;
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_POW_PREFIX | sign << 12 | hr << 8 | min);
		}
		else
			exynos_pmu_write(EXYNOS_INFORM3,REBOOT_MODE_PREFIX | REBOOT_MODE_NONE);
	}

	flush_cache_all();
	//outer_flush_all();

	mach_restart(0, 0);

	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1)
		;
}

static int __init sec_reboot_init(void)
{
	mach_restart = (void*)arm_pm_restart;
	pm_power_off = sec_power_off;
	arm_pm_restart = (void*)sec_reboot;
	return 0;
}

subsys_initcall(sec_reboot_init);
