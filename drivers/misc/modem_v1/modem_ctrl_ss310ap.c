/*
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/mcu_ipc.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/pmu-cp.h>
#include <soc/samsung/exynos-devfreq.h>
#include <soc/samsung/tmu.h>
#include <soc/samsung/ect_parser.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "link_device_memory.h"

#include <linux/modem_notifier.h>
#include "../../../drivers/soc/samsung/pwrcal/pwrcal.h"
#include "../../../drivers/soc/samsung/pwrcal/S5E7570/S5E7570-vclk.h"

#include <linux/modem_notifier.h>

#ifdef CONFIG_EXYNOS_BUSMONITOR
#include <linux/exynos-busmon.h>
#endif

#define MIF_INIT_TIMEOUT	(15 * HZ)

#ifdef CONFIG_REGULATOR_S2MPU01A
#include <linux/mfd/samsung/s2mpu01a.h>
static inline int change_cp_pmu_manual_reset(void)
{
	return change_mr_reset();
}
#else
static inline int change_cp_pmu_manual_reset(void) {return 0; }
#endif

#ifdef CONFIG_UART_SEL
extern void cp_recheck_uart_dir(void);
#endif

extern unsigned int system_rev;

static struct modem_ctl *g_mc;

static irqreturn_t cp_wdt_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct io_device *iod;
	enum modem_state new_state;

	mif_disable_irq(&mc->irq_cp_wdt);
	mif_err("%s: ERR! CP_WDOG occurred\n", mc->name);

	/* Disable debug Snapshot */
	mif_set_snapshot(false);

	if (mc->phone_state == STATE_ONLINE)
		modem_notify_event(MODEM_EVENT_WATCHDOG);

	exynos_clear_cp_reset();
	new_state = STATE_CRASH_WATCHDOG;

	mif_err("new_state = %s\n", cp_state_str(new_state));

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, new_state);
	}

	return IRQ_HANDLED;
}

static irqreturn_t cp_fail_handler(int irq, void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct io_device *iod;
	enum modem_state new_state;

	mif_disable_irq(&mc->irq_cp_fail);
	mif_err("%s: ERR! CP_FAIL occurred\n", mc->name);

	exynos_cp_active_clear();
	new_state = STATE_CRASH_RESET;

	mif_err("new_state = %s\n", cp_state_str(new_state));

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, new_state);
	}

	return IRQ_HANDLED;
}

static void cp_active_handler(void *arg)
{
	struct modem_ctl *mc = (struct modem_ctl *)arg;
	struct io_device *iod;
	int cp_on = exynos_get_cp_power_status();
	int cp_active = mbox_extract_value(MCU_CP, mc->mbx_cp_status,
					mc->sbi_lte_active_mask, mc->sbi_lte_active_pos);
	enum modem_state old_state = mc->phone_state;
	enum modem_state new_state = mc->phone_state;

	mif_err("old_state:%s cp_on:%d cp_active:%d\n",
		cp_state_str(old_state), cp_on, cp_active);

	if (!cp_active) {
		if (cp_on > 0) {
			new_state = STATE_OFFLINE;
			complete_all(&mc->off_cmpl);
		} else {
			mif_err("don't care!!!\n");
		}
	}

	if (old_state != new_state) {
		mif_err("new_state = %s\n", cp_state_str(new_state));

		if (old_state == STATE_ONLINE)
			modem_notify_event(MODEM_EVENT_EXIT);

		list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
			if (iod && atomic_read(&iod->opened) > 0)
				iod->modem_state_changed(iod, new_state);
		}
	}
}

static int get_system_rev(struct device_node *np)
{
	int value, cnt, gpio_cnt;
	unsigned gpio_hw_rev, hw_rev = 0;

	gpio_cnt = of_gpio_count(np);
	if (gpio_cnt < 0) {
		mif_err("failed to get gpio_count from DT(%d)\n", gpio_cnt);
		return 0;
	}

	for (cnt = 0; cnt < gpio_cnt; cnt++) {
		gpio_hw_rev = of_get_gpio(np, cnt);
		if (!gpio_is_valid(gpio_hw_rev)) {
			mif_err("gpio_hw_rev%d: Invalied gpio\n", cnt);
			return -EINVAL;
		}

		value = gpio_get_value(gpio_hw_rev);
		hw_rev |= (value & 0x1) << cnt;
	}

	return hw_rev;
}

static int get_ds_detect(struct device_node *np)
{
	unsigned gpio_ds_det;

	gpio_ds_det = of_get_named_gpio(np, "mif,gpio_ds_det", 0);
	if (!gpio_is_valid(gpio_ds_det)) {
		mif_err("gpio_ds_det: Invalid gpio\n");
		return 0;
	}

	return gpio_get_value(gpio_ds_det);
}

#define CP_CPU_FREQ_700	(700000)
#define CP_CPU_FREQ_830	(830000)
static int get_cp_volt_information(u32 *volt700, u32 *volt830)
{
	void *dvfs_block;
	struct ect_dvfs_domain *dvfs_domain;
	u32 max_state;
	struct dvfs_rate_volt cp_rate_volt[32];
	int table_size;
	int i;

	dvfs_block = ect_get_block(BLOCK_DVFS);
	if (dvfs_block == NULL) {
		mif_err("ect_get_block error\n");
		return -ENODEV;
	}

	dvfs_domain = ect_dvfs_get_domain(dvfs_block, "dvs_cp");
	if (dvfs_domain == NULL) {
		mif_err("ect_dvfs_get_domain error\n");
	        return -ENODEV;
	}

	max_state = dvfs_domain->num_of_level;
	mif_err("dvs_cp max_state: %u\n", max_state);

	table_size = cal_dfs_get_rate_asv_table(dvs_cp, cp_rate_volt);
	if (!table_size) {
		mif_err("failed get ASV table\n");
	        return -ENODEV;
	}

	*volt700 = 0;
	*volt830 = 0;
	for (i = 0; i < max_state; i++) {
		mif_err("dvs_cp[%d] freq:%u volt:%u\n", i,
			(u32)cp_rate_volt[i].rate, (u32)cp_rate_volt[i].volt);
		if (cp_rate_volt[i].rate == CP_CPU_FREQ_700)
			*volt700 = (u32)(cp_rate_volt[i].volt);
		if (cp_rate_volt[i].rate == CP_CPU_FREQ_830)
			*volt830 = (u32)(cp_rate_volt[i].volt);
	}

	return 0;
}

static int init_mailbox_regs(struct modem_ctl *mc)
{
	struct platform_device *pdev = to_platform_device(mc->dev);
	struct device_node *np = pdev->dev.of_node;
	unsigned int mbx_ap_status;
	unsigned int mbx_cp_vol_700;
	unsigned int mbx_cp_vol_830;
	unsigned int sbi_ds_det_mask, sbi_ds_det_pos;
	unsigned int sbi_sys_rev_mask, sbi_sys_rev_pos;
	unsigned int sbi_cp_cpu_freq_asv_mask, sbi_cp_cpu_freq_asv_pos;
	int sys_rev, ds_det, i;
	int cp_cpu_freq_asv;
	u32 cp_volt_700;
	u32 cp_volt_830;
#ifdef CONFIG_USIM_DETECT
	unsigned int ap_status;
	unsigned int sbi_pda_active_mask;
	unsigned int sbi_pda_active_pos;
	unsigned int sbi_ap_status_mask;
	unsigned int sbi_ap_status_pos;

	/* Save USIM detection value before reset */
	if (np) {
		mif_dt_read_u32(np, "sbi_pda_active_mask", sbi_pda_active_mask);
		mif_dt_read_u32(np, "sbi_pda_active_pos", sbi_pda_active_pos);
		mif_dt_read_u32(np, "sbi_ap_status_mask", sbi_ap_status_mask);
		mif_dt_read_u32(np, "sbi_ap_status_pos", sbi_ap_status_pos);

		mif_dt_read_u32(np, "mbx_ap2cp_united_status", mbx_ap_status);
		ap_status = mbox_get_value(MCU_CP, mbx_ap_status);
	}
#endif

	if (np) {
		mif_dt_read_u32(np, "mbx_ap2cp_united_status", mbx_ap_status);
		mif_dt_read_u32(np, "mbx_ap2cp_cp_voltage_700", mbx_cp_vol_700);
		mif_dt_read_u32(np, "mbx_ap2cp_cp_voltage_830", mbx_cp_vol_830);
		mif_dt_read_u32(np, "sbi_cp_cpu_freq_asv_mask",
			sbi_cp_cpu_freq_asv_mask);
		mif_dt_read_u32(np, "sbi_cp_cpu_freq_asv_pos",
			sbi_cp_cpu_freq_asv_pos);

		/* Save values */
		cp_volt_700 = mbox_get_value(MCU_CP, mbx_cp_vol_700);
		cp_volt_830 = mbox_get_value(MCU_CP, mbx_cp_vol_830);
		cp_cpu_freq_asv = mbox_extract_value(MCU_CP, mbx_ap_status,
			sbi_cp_cpu_freq_asv_mask, sbi_cp_cpu_freq_asv_pos);
        }

	for (i = 0; i < MAX_MBOX_NUM; i++)
		mbox_set_value(MCU_CP, i, 0);

	if (np) {
		mif_dt_read_u32(np, "mbx_ap2cp_united_status", mbx_ap_status);
		mif_dt_read_u32(np, "sbi_sys_rev_mask", sbi_sys_rev_mask);
		mif_dt_read_u32(np, "sbi_sys_rev_pos", sbi_sys_rev_pos);
		mif_dt_read_u32(np, "sbi_ds_det_mask", sbi_ds_det_mask);
		mif_dt_read_u32(np, "sbi_ds_det_pos", sbi_ds_det_pos);

		mif_dt_read_u32(np, "mbx_ap2cp_cp_voltage_700", mbx_cp_vol_700);
		mif_dt_read_u32(np, "mbx_ap2cp_cp_voltage_830", mbx_cp_vol_830);
		mif_dt_read_u32(np, "sbi_cp_cpu_freq_asv_mask",
				sbi_cp_cpu_freq_asv_mask);
		mif_dt_read_u32(np, "sbi_cp_cpu_freq_asv_pos",
				sbi_cp_cpu_freq_asv_pos);

		sys_rev = get_system_rev(np);
		sys_rev = system_rev;

		ds_det = get_ds_detect(np);
		if (sys_rev < 0 || ds_det < 0)
			return -EINVAL;

#ifdef CONFIG_USIM_DETECT
		/* Resotre USIM detection value */
		mbox_set_value(MCU_CP, mbx_ap_status, ap_status);
		mbox_update_value(MCU_CP, mbx_ap_status, 0,
			sbi_pda_active_mask, sbi_pda_active_pos);
		mbox_update_value(MCU_CP, mc->mbx_ap_status, 0,
			sbi_ap_status_mask, sbi_ap_status_pos);
#endif
		mbox_update_value(MCU_CP, mbx_ap_status, sys_rev,
			sbi_sys_rev_mask, sbi_sys_rev_pos);
		mbox_update_value(MCU_CP, mbx_ap_status, ds_det,
			sbi_ds_det_mask, sbi_ds_det_pos);

		mif_info("sys_rev:%d, ds_det:%u (0x%x)\n",
			sys_rev, ds_det, mbox_get_value(MCU_CP, mbx_ap_status));

		/* Resotre value */
		mbox_set_value(MCU_CP, mbx_cp_vol_700, cp_volt_700);
		mbox_set_value(MCU_CP, mbx_cp_vol_830, cp_volt_830);
		mbox_update_value(MCU_CP, mbx_ap_status, cp_cpu_freq_asv,
			sbi_cp_cpu_freq_asv_mask, sbi_cp_cpu_freq_asv_pos);

	} else {
		mif_info("non-DT project, can't set system_rev\n");
	}

	return 0;
}

static int init_mailbox_regs_volt_info(struct modem_ctl *mc)
{
	struct platform_device *pdev = to_platform_device(mc->dev);
	struct device_node *np = pdev->dev.of_node;
	unsigned int mbx_ap_status;
	unsigned int mbx_cp_vol_700;
	unsigned int mbx_cp_vol_830;
	unsigned int sbi_cp_cpu_freq_asv_mask, sbi_cp_cpu_freq_asv_pos;
	int cp_cpu_freq_asv;
	u32 cp_volt_700;
	u32 cp_volt_830;

	if (np) {
		mif_dt_read_u32(np, "mbx_ap2cp_united_status", mbx_ap_status);
		mif_dt_read_u32(np, "mbx_ap2cp_cp_voltage_700", mbx_cp_vol_700);
		mif_dt_read_u32(np, "mbx_ap2cp_cp_voltage_830", mbx_cp_vol_830);
		mif_dt_read_u32(np, "sbi_cp_cpu_freq_asv_mask",
				sbi_cp_cpu_freq_asv_mask);
		mif_dt_read_u32(np, "sbi_cp_cpu_freq_asv_pos",
				sbi_cp_cpu_freq_asv_pos);

		cp_cpu_freq_asv = cal_get_cp_cpu_freq();
		mif_err("cp_cpu_freq_asv: %d\n", cp_cpu_freq_asv);
		mbox_update_value(MCU_CP, mbx_ap_status, cp_cpu_freq_asv,
			sbi_cp_cpu_freq_asv_mask, sbi_cp_cpu_freq_asv_pos);

		if (get_cp_volt_information(&cp_volt_700, &cp_volt_830) == 0) {
			mif_err("cp_volt_700:%u cp_volt_830:%u\n", cp_volt_700,
					cp_volt_830);
			mbox_set_value(MCU_CP, mbx_cp_vol_700, cp_volt_700);
			mbox_set_value(MCU_CP, mbx_cp_vol_830, cp_volt_830);
		}
	} else {
		mif_info("non-DT project, can't set volt_info\n");
	}

	return 0;
}

static int ss310ap_on(struct modem_ctl *mc)
{
	int ret;
	int cp_active = mbox_extract_value(MCU_CP, mc->mbx_cp_status,
						mc->sbi_lte_active_mask, mc->sbi_lte_active_pos);
	int cp_status = mbox_extract_value(MCU_CP, mc->mbx_cp_status,
						mc->sbi_cp_status_mask, mc->sbi_cp_status_pos);
	struct modem_data __maybe_unused *modem = mc->mdm_data;

	mif_err("+++\n");
	mif_err("cp_active:%d cp_status:%d\n", cp_active, cp_status);

	/* Enable debug Snapshot */
	mif_set_snapshot(true);

	mc->phone_state = STATE_OFFLINE;

	if (init_mailbox_regs(mc))
		mif_err("Failed to initialize mbox regs\n");
#if !defined(CONFIG_SOC_EXYNOS7570)
	memmove(modem->ipc_base + 0x1000, mc->sysram_alive, 0x800);
#endif

	mbox_update_value(MCU_CP, mc->mbx_ap_status, 1,
			mc->sbi_pda_active_mask, mc->sbi_pda_active_pos);

	if (exynos_get_cp_power_status() > 0) {
		mif_err("CP aleady Power on, Just start!\n");
		exynos_cp_release();
	} else {
		exynos_set_cp_power_onoff(CP_POWER_ON);
	}

	msleep(300);
	ret = change_cp_pmu_manual_reset();
	mif_err("change_mr_reset -> %d\n", ret);

#ifdef CONFIG_UART_SEL
	mif_err("Recheck UART direction.\n");
	cp_recheck_uart_dir();
#endif

	mif_err("---\n");
	return 0;
}

static int ss310ap_off(struct modem_ctl *mc)
{
	mif_err("+++\n");

	exynos_set_cp_power_onoff(CP_POWER_OFF);

	mif_err("---\n");
	return 0;
}

static int ss310ap_shutdown(struct modem_ctl *mc)
{
	struct io_device *iod;
	unsigned long timeout = msecs_to_jiffies(3000);
	unsigned long remain;

	mif_err("+++\n");

	if (mc->phone_state == STATE_OFFLINE
		|| exynos_get_cp_power_status() <= 0)
		goto exit;

	init_completion(&mc->off_cmpl);
	remain = wait_for_completion_timeout(&mc->off_cmpl, timeout);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		mc->phone_state = STATE_OFFLINE;
		list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
			if (iod && atomic_read(&iod->opened) > 0)
				iod->modem_state_changed(iod, STATE_OFFLINE);
		}
	}

exit:
	exynos_set_cp_power_onoff(CP_POWER_OFF);
	mif_err("---\n");
	return 0;
}

static int ss310ap_reset(struct modem_ctl *mc)
{
	mif_err("+++\n");

	//mc->phone_state = STATE_OFFLINE;
	if (mc->phone_state == STATE_OFFLINE)
		return 0;

	if (*(unsigned int *)(mc->mdm_data->ipc_base + 0xF80)
			== 0xDEB)
		return 0;

	if (mc->phone_state == STATE_ONLINE)
		modem_notify_event(MODEM_EVENT_RESET);

	if (exynos_get_cp_power_status() > 0) {
		mif_err("CP aleady Power on, try reset\n");
		exynos_cp_reset();
	}

	mif_err("---\n");
	return 0;
}

static int ss310ap_boot_on(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->iod);
	struct io_device *iod;
	int cnt = 100;

	mif_info("+++\n");

	if (ld->boot_on)
		ld->boot_on(ld, mc->bootd);

	init_completion(&mc->init_cmpl);
	init_completion(&mc->off_cmpl);

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, STATE_BOOTING);
	}

	while (mbox_extract_value(MCU_CP, mc->mbx_cp_status,
				mc->sbi_cp_status_mask, mc->sbi_cp_status_pos) == 0) {
		if (--cnt > 0)
			usleep_range(10000, 20000);
		else
			return -EACCES;
	}

	mif_disable_irq(&mc->irq_cp_wdt);
	mif_enable_irq(&mc->irq_cp_fail);

	mif_info("cp_status=%u\n",
			mbox_extract_value(MCU_CP, mc->mbx_cp_status,
					mc->sbi_cp_status_mask, mc->sbi_cp_status_pos));

	mif_info("---\n");
	return 0;
}

static int ss310ap_boot_off(struct modem_ctl *mc)
{
	struct io_device *iod;
	unsigned long remain;
	int err = 0;
	mif_info("+++\n");

	remain = wait_for_completion_timeout(&mc->init_cmpl, MIF_INIT_TIMEOUT);
	if (remain == 0) {
		mif_err("T-I-M-E-O-U-T\n");
		err = -EAGAIN;
		goto exit;
	}

	mif_enable_irq(&mc->irq_cp_wdt);

	list_for_each_entry(iod, &mc->modem_state_notify_list, list) {
		if (iod && atomic_read(&iod->opened) > 0)
			iod->modem_state_changed(iod, STATE_ONLINE);
	}

	mif_info("---\n");

exit:
	mif_disable_irq(&mc->irq_cp_fail);
	return err;
}

static int ss310ap_boot_done(struct modem_ctl *mc)
{
	mif_info("+++\n");
	mif_info("---\n");
	return 0;
}

static int ss310ap_force_crash_exit(struct modem_ctl *mc)
{
	struct link_device *ld = get_current_link(mc->bootd);
	mif_err("+++\n");

	/* Make DUMP start */
	ld->force_dump(ld, mc->bootd);

	mif_err("---\n");
	return 0;
}

int ss310ap_force_crash_exit_ext(void)
{
	if (g_mc)
		ss310ap_force_crash_exit(g_mc);

	return 0;
}
EXPORT_SYMBOL(ss310ap_force_crash_exit_ext);

u32 ss310ap_get_evs_mode_ext(void)
{
	int evs_mode = 0;
	u32 requested_mif_clk = 0;
	struct modem_data *modem;
	struct mem_link_device *mld;

	if (g_mc) {
		modem = g_mc->mdm_data;
		evs_mode = mbox_extract_value(MCU_CP, g_mc->mbx_cp_status,
			g_mc->sbi_evs_mode_mask, g_mc->sbi_evs_mode_pos);

		if (modem) {
			mld = modem->mld;
			if (evs_mode && mld)
				requested_mif_clk = mld->requested_mif_clk;
		}
	}
	return requested_mif_clk;
}
EXPORT_SYMBOL(ss310ap_get_evs_mode_ext);

static int ss310ap_dump_start(struct modem_ctl *mc)
{
	int err;
	struct link_device *ld = get_current_link(mc->bootd);
	mif_err("+++\n");

	if (!ld->dump_start) {
		mif_err("ERR! %s->dump_start not exist\n", ld->name);
		return -EFAULT;
	}

	err = ld->dump_start(ld, mc->bootd);
	if (err)
		return err;

	exynos_cp_release();

	mbox_update_value(MCU_CP, mc->mbx_ap_status, 1,
			mc->sbi_ap_status_mask, mc->sbi_ap_status_pos);

	mif_err("---\n");
	return err;
}

static void ss310ap_modem_boot_confirm(struct modem_ctl *mc)
{
	mbox_update_value(MCU_CP,mc->mbx_ap_status, 1,
			mc->sbi_ap_status_mask, mc->sbi_ap_status_pos);
	mif_info("ap_status=%u\n",
			mbox_extract_value(MCU_CP, mc->mbx_ap_status,
					mc->sbi_ap_status_mask, mc->sbi_ap_status_pos));
}

static void ss310ap_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = ss310ap_on;
	mc->ops.modem_off = ss310ap_off;
	mc->ops.modem_shutdown = ss310ap_shutdown;
	mc->ops.modem_reset = ss310ap_reset;
	mc->ops.modem_boot_on = ss310ap_boot_on;
	mc->ops.modem_boot_off = ss310ap_boot_off;
	mc->ops.modem_boot_done = ss310ap_boot_done;
	mc->ops.modem_force_crash_exit = ss310ap_force_crash_exit;
	mc->ops.modem_dump_start = ss310ap_dump_start;
	mc->ops.modem_boot_confirm = ss310ap_modem_boot_confirm;
}

static void ss310ap_get_pdata(struct modem_ctl *mc, struct modem_data *modem)
{
	struct modem_mbox *mbx = modem->mbx;

	mc->int_pda_active = mbx->int_ap2cp_active;

	mc->irq_phone_active = mbx->irq_cp2ap_active;

	mc->mbx_ap_status = mbx->mbx_ap2cp_status;
	mc->mbx_cp_status = mbx->mbx_cp2ap_status;

	mc->mbx_perf_req = mbx->mbx_cp2ap_perf_req;

	mc->int_uart_noti = mbx->int_ap2cp_uart_noti;

	mc->sbi_evs_mode_mask = mbx->sbi_evs_mode_mask;
	mc->sbi_evs_mode_pos = mbx->sbi_evs_mode_pos;
	mc->sbi_lte_active_mask = mbx->sbi_lte_active_mask;
	mc->sbi_lte_active_pos = mbx->sbi_lte_active_pos;
	mc->sbi_cp_status_mask = mbx->sbi_cp_status_mask;
	mc->sbi_cp_status_pos = mbx->sbi_cp_status_pos;

	mc->sbi_pda_active_mask = mbx->sbi_pda_active_mask;
	mc->sbi_pda_active_pos = mbx->sbi_pda_active_pos;
	mc->sbi_ap_status_mask = mbx->sbi_ap_status_mask;
	mc->sbi_ap_status_pos = mbx->sbi_ap_status_pos;

	mc->sbi_uart_noti_mask = mbx->sbi_uart_noti_mask;
	mc->sbi_uart_noti_pos = mbx->sbi_uart_noti_pos;
}

#ifdef CONFIG_EXYNOS_BUSMONITOR
static int ss310ap_busmon_notifier(struct notifier_block *nb,
						unsigned long event, void *data)
{
	struct busmon_notifier *info = (struct busmon_notifier *)data;
	char *init_desc = info->init_desc;

	if (init_desc != NULL &&
		(strncmp(init_desc, "CP", strlen(init_desc)) == 0 ||
		strncmp(init_desc, "APB_CORE_CP", strlen(init_desc)) == 0 ||
		strncmp(init_desc, "MIF_CP", strlen(init_desc)) == 0)) {
		struct modem_ctl *mc =
			container_of(nb, struct modem_ctl, busmon_nfb);

		ss310ap_force_crash_exit(mc);
	}
	return 0;
}
#endif

int ss310ap_init_modemctl_device(struct modem_ctl *mc, struct modem_data *pdata)
{
	struct platform_device *pdev = to_platform_device(mc->dev);
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	unsigned int irq_num;
	struct resource __maybe_unused *sysram_alive;
	unsigned long flags = IRQF_NO_SUSPEND | IRQF_NO_THREAD;
	unsigned int cp_rst_n ;

	mif_err("+++\n");

	g_mc = mc;

	ss310ap_get_ops(mc);
	ss310ap_get_pdata(mc, pdata);
	dev_set_drvdata(mc->dev, mc);

	/*
	** Register CP_FAIL interrupt handler
	*/
	irq_num = platform_get_irq(pdev, 0);
	mif_init_irq(&mc->irq_cp_fail, irq_num, "cp_fail", flags);

	ret = mif_request_irq(&mc->irq_cp_fail, cp_fail_handler, mc);
	if (ret)	{
		mif_err("Failed to request_irq with(%d)", ret);
		return ret;
	}

	/* CP_FAIL interrupt must be enabled only during CP booting */
	mc->irq_cp_fail.active = true;
	mif_disable_irq(&mc->irq_cp_fail);

	/*
	** Register CP_WDT interrupt handler
	*/
	irq_num = platform_get_irq(pdev, 1);
	mif_init_irq(&mc->irq_cp_wdt, irq_num, "cp_wdt", flags);

	ret = mif_request_irq(&mc->irq_cp_wdt, cp_wdt_handler, mc);
	if (ret) {
		mif_err("Failed to request_irq with(%d)", ret);
		return ret;
	}

	/* CP_WDT interrupt must be enabled only after CP booting */
	mc->irq_cp_wdt.active = true;
	mif_disable_irq(&mc->irq_cp_wdt);

#ifdef CONFIG_SOC_EXYNOS8890
	sysram_alive = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mc->sysram_alive = devm_ioremap_resource(&pdev->dev, sysram_alive);
	if (IS_ERR(mc->sysram_alive)) {
		ret = PTR_ERR(mc->sysram_alive);
		return ret;
	}
#endif

#if defined (CONFIG_SOC_EXYNOS7870)
	mc->sysram_alive = shm_request_region(shm_get_sysram_base(),
					shm_get_sysram_size());
	if (!mc->sysram_alive)
		mif_err("Failed to memory allocation\n");
#endif

	/*
	** Register LTE_ACTIVE MBOX interrupt handler
	*/
	ret = mbox_request_irq(MCU_CP, mc->irq_phone_active, cp_active_handler, mc);
	if (ret) {
		mif_err("Failed to mbox_request_irq %u with(%d)",
				mc->irq_phone_active, ret);
		return ret;
	}

	init_completion(&mc->off_cmpl);

	/*
	** Get/set CP_RST_N
	*/
	if (np)	{
		cp_rst_n = of_get_named_gpio(np, "modem_ctrl,gpio_cp_rst_n", 0);
		if (gpio_is_valid(cp_rst_n)) {
			mif_err("cp_rst_n: %d\n", cp_rst_n);
			ret = gpio_request(cp_rst_n, "CP_RST_N");
			if (ret)	{
				mif_err("fail req gpio %s:%d\n", "CP_RST_N", ret);
				return -ENODEV;
			}

			gpio_direction_output(cp_rst_n, 1);
		} else {
			mif_err("cp_rst_n: Invalied gpio pins\n");
		}
	} else {
		mif_err("cannot find device_tree for pmu_cu!\n");
		return -ENODEV;
	}

#ifdef CONFIG_EXYNOS_BUSMONITOR
	/*
	 ** Register BUS Mon notifier
	 */
	mc->busmon_nfb.notifier_call = ss310ap_busmon_notifier;
	busmon_notifier_chain_register(&mc->busmon_nfb);
#endif

	if (init_mailbox_regs_volt_info(mc))
		mif_err("Failed to initialize mbox regs\n");

	mif_err("---\n");
	return 0;
}
