/*
 * Samsung EXYNOS SoC series USB DRD PHY driver
 *
 * Phy provider for USB 3.0 DRD controller on Exynos SoC series
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 * Author: Vivek Gautam <gautam.vivek@samsung.com>
 *	   Minho Lee <minho55.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/exynos5-pmu.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/samsung_usb.h>
#include <linux/usb/otg.h>

#if IS_ENABLED(CONFIG_SCSC_CLK20MHZ)
#include <scsc/scsc_mx.h>
#endif

#include "phy-exynos-usbdrd.h"

static const char *exynos8890_usbdrd_clk_names[] = {"aclk", "sclk", "phyclock",
						"pipe_pclk", NULL};

static const char *exynos8890_usbhost_clk_names[] = {"aclk", "sclk", "phyclock",
						"phy_ref", NULL};
static const char *exynos7870_usbdrd_clk_names[] = {"usb_pll", "usbdrd20", NULL};
static const char *exynos7870_usbphy_clk_names[] = {"phyumux", NULL};
static const char *exynos7570_usbdrd_clk_names[] = {"usb_pll", "usbdrd20", NULL};
static const char *exynos7570_usbphy_clk_names[] = {"phyumux", NULL};

#if IS_ENABLED(CONFIG_SCSC_CLK20MHZ)
static void
exynos_usbdrd_set_extrefclk_state(void *data, enum mx140_clk20mhz_status state)
{
	if (state != MX140_CLK_STARTED) {
		pr_err("this function should be called by EXTCLK_STARTED.");
		return;
	}

	/* data = &phy_drd->can_use_extrefclk */
	if (!data) {
		pr_err("Error: data is NULL");
		return;
	}

	complete((struct completion *)data);
}
#else
/*
 * USBPLL clock request is only used for Exynos7570 with mx140.
 * This is dummy function for other SoCs.
 */
static void exynos_usbdrd_extrefclk_dummy(void)
{
}
#endif

static int
exynos_usbdrd_register_cb_extrefclk(struct exynos_usbdrd_phy *phy_drd)
{
#if IS_ENABLED(CONFIG_SCSC_CLK20MHZ)
	phy_drd->request_extrefclk_cb = mx140_clk20mhz_request;
	phy_drd->release_extrefclk_cb = mx140_clk20mhz_release;

	return mx140_clk20mhz_register(exynos_usbdrd_set_extrefclk_state,
					(void *)&phy_drd->can_use_extrefclk);
#else
	/* Disable USBPLL request */
	phy_drd->request_extrefclk = false;

	exynos_usbdrd_extrefclk_dummy();

	return 0;
#endif
}

static int exynos_usbdrd_ready_extrefclk(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	struct device_node *node = dev->of_node;

	phy_drd->request_extrefclk = of_property_read_bool(node,
						"request_extrefclk");

	if (!phy_drd->request_extrefclk)
		return 0;

	phy_drd->extrefclk_requested = false;
	init_completion(&phy_drd->can_use_extrefclk);

	return exynos_usbdrd_register_cb_extrefclk(phy_drd);
}

static int
exynos_usbdrd_request_extrefclk(struct exynos_usbdrd_phy *phy_drd, void *data)
{
	int ret;

	if (!phy_drd->request_extrefclk)
		return 0;

	ret = phy_drd->request_extrefclk_cb();
	if (ret)
		dev_err(phy_drd->dev, "%s: Failed to request extrefclk\n",
					__func__);
	else
		phy_drd->extrefclk_requested = true;

	return ret;
}

static void
exynos_usbdrd_release_extrefclk(struct exynos_usbdrd_phy *phy_drd, void *data)
{
	int ret;

	if (!phy_drd->request_extrefclk)
		return;

	phy_drd->extrefclk_requested = false;
	ret = phy_drd->release_extrefclk_cb();
	if (ret)
		dev_err(phy_drd->dev, "%s: Failed to release extrefclk\n",
					__func__);
}

static int exynos_usbdrd_check_extrefclk(struct exynos_usbdrd_phy *phy_drd)
{
	if (!phy_drd->extrefclk_requested)
		return 0;

	if (!wait_for_completion_timeout(&phy_drd->can_use_extrefclk,
				msecs_to_jiffies(5000)))
		return -ETIMEDOUT;

	return 0;
}

static int exynos_usbdrd_clk_prepare(struct exynos_usbdrd_phy *phy_drd)
{
	int i;
	int ret;

	for (i = 0; phy_drd->clocks[i] != NULL; i++) {
		ret = clk_prepare(phy_drd->clocks[i]);
		if (ret)
			goto err;
	}
	for (i = 0; phy_drd->phy_clocks[i] != NULL; i++) {
		ret = clk_prepare(phy_drd->phy_clocks[i]);
		if (ret)
			goto err1;
	}
	return 0;
err:
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(phy_drd->clocks[i]);
	return ret;
err1:
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(phy_drd->phy_clocks[i]);
	return ret;
}

static int exynos_usbdrd_clk_enable(struct exynos_usbdrd_phy *phy_drd,
					bool umux)
{
	int i;
	int ret;

	if (!umux) {
		for (i = 0; phy_drd->clocks[i] != NULL; i++) {
				ret = clk_enable(phy_drd->clocks[i]);
				if (ret)
					goto err;
		}
	} else {
		for (i = 0; phy_drd->phy_clocks[i] != NULL; i++) {
				ret = clk_enable(phy_drd->phy_clocks[i]);
				if (ret)
					goto err1;
			}
	}
	return 0;
err:
	for (i = i - 1; i >= 0; i--)
		clk_disable(phy_drd->clocks[i]);
	return ret;
err1:
	for (i = i -1; i >= 0; i--)
		clk_disable(phy_drd->phy_clocks[i]);
	return ret;
}

static void exynos_usbdrd_clk_unprepare(struct exynos_usbdrd_phy *phy_drd)
{
	int i;

	for (i = 0; phy_drd->clocks[i] != NULL; i++)
		clk_unprepare(phy_drd->clocks[i]);
	for (i = 0; phy_drd->phy_clocks[i] != NULL; i++)
		clk_unprepare(phy_drd->phy_clocks[i]);
}

static void exynos_usbdrd_clk_disable(struct exynos_usbdrd_phy *phy_drd, bool umux)
{
	int i;

	if (!umux) {
		for (i = 0; phy_drd->clocks[i] != NULL; i++) {
				clk_disable(phy_drd->clocks[i]);
		}
	} else {
		for (i = 0; phy_drd->phy_clocks[i] != NULL; i++) {
				clk_disable(phy_drd->phy_clocks[i]);
		}
	}
}

static int exynos_usbdrd_clk_get(struct exynos_usbdrd_phy *phy_drd)
{
	const char	**clk_ids, **phy_clk_ids;
	struct clk	*clk;
	int		clk_count;
	int		phy_clk_count = 0;
	int		i;

	switch (phy_drd->drv_data->cpu_type) {
	case TYPE_EXYNOS8890:
		if (phy_drd->drv_data->ip_type == TYPE_USB3DRD) {
			clk_ids = exynos8890_usbdrd_clk_names;
			clk_count =
				(int)ARRAY_SIZE(exynos8890_usbdrd_clk_names);
		} else {
			clk_ids = exynos8890_usbhost_clk_names;
			clk_count =
				(int)ARRAY_SIZE(exynos8890_usbhost_clk_names);
		}
		break;
	case TYPE_EXYNOS7870:
		clk_ids = exynos7870_usbdrd_clk_names;
		clk_count =
			(int)ARRAY_SIZE(exynos7870_usbdrd_clk_names);
		phy_clk_ids = exynos7870_usbphy_clk_names;
		phy_clk_count =
			(int)ARRAY_SIZE(exynos7870_usbphy_clk_names);
		break;
	case TYPE_EXYNOS7570:
		clk_ids = exynos7570_usbdrd_clk_names;
		clk_count =
			(int)ARRAY_SIZE(exynos7570_usbdrd_clk_names);
		phy_clk_ids = exynos7570_usbphy_clk_names;
		phy_clk_count =
			(int)ARRAY_SIZE(exynos7570_usbphy_clk_names);
		break;

	default:
		dev_err(phy_drd->dev, "couldn't get clock: unknown cpu type\n");
		return -EINVAL;
	}

	phy_drd->clocks = (struct clk **) devm_kmalloc(phy_drd->dev,
			clk_count * sizeof(struct clk *), GFP_KERNEL);
	if (!phy_drd->clocks) {
		dev_err(phy_drd->dev, "failed to alloc : drd clocks\n");
		return -ENOMEM;
	}

	for (i = 0; clk_ids[i] != NULL; i++) {
		clk = devm_clk_get(phy_drd->dev, clk_ids[i]);
		if (IS_ERR_OR_NULL(clk)) {
			dev_err(phy_drd->dev,
				"couldn't get %s clock\n", clk_ids[i]);
			return -EINVAL;
		}
		phy_drd->clocks[i] = clk;
	}
	phy_drd->clocks[i] = NULL;

	if (!phy_clk_count)
		return 0;

	phy_drd->phy_clocks = (struct clk **) devm_kmalloc(phy_drd->dev,
			phy_clk_count * sizeof(struct clk *), GFP_KERNEL);
	if (!phy_drd->phy_clocks) {
		dev_err(phy_drd->dev, "failed to alloc : phy clocks\n");
		return -ENOMEM;
	}

	for (i = 0; phy_clk_ids[i] != NULL; i++) {
		clk = devm_clk_get(phy_drd->dev, phy_clk_ids[i]);
		if (IS_ERR_OR_NULL(clk)) {
			dev_err(phy_drd->dev,
				"couldn't get %s clock\n", phy_clk_ids[i]);
			return -EINVAL;
		}
		phy_drd->phy_clocks[i] = clk;
	}
	phy_drd->phy_clocks[i] = NULL;

	return 0;
}

static inline
struct exynos_usbdrd_phy *to_usbdrd_phy(struct phy_usb_instance *inst)
{
	return container_of((inst), struct exynos_usbdrd_phy,
			    phys[(inst)->index]);
}

/*
 * exynos_rate_to_clk() converts the supplied clock rate to the value that
 * can be written to the phy register.
 */
static unsigned int exynos_rate_to_clk(struct exynos_usbdrd_phy *phy_drd)
{
	const char **clk_ids;
	int ret, i;

	switch (phy_drd->drv_data->cpu_type) {
	case TYPE_EXYNOS7570:
	case TYPE_EXYNOS7870:
		if (phy_drd->drv_data->cpu_type == TYPE_EXYNOS7570)
			clk_ids = exynos7570_usbdrd_clk_names;
		else
			clk_ids = exynos7870_usbdrd_clk_names;
		for (i = 0; clk_ids[i] != NULL; i++) {
			if (!strcmp("usb_pll", clk_ids[i])) {
				phy_drd->ref_clk = phy_drd->clocks[i];
				break;
			}
		}
		break;
	case TYPE_EXYNOS8890:
		if (phy_drd->drv_data->ip_type == TYPE_USB2HOST) {
			clk_ids = exynos8890_usbhost_clk_names;
			for (i = 0; clk_ids[i] != NULL; i++) {
				if (!strcmp("phy_ref", clk_ids[i])) {
					phy_drd->ref_clk = phy_drd->clocks[i];
					break;
				}
			}
			phy_drd->extrefclk = EXYNOS_FSEL_12MHZ;
			return 0;
		}
		phy_drd->ref_clk = devm_clk_get(phy_drd->dev, "ext_xtal");
		break;
	default:
		phy_drd->ref_clk = devm_clk_get(phy_drd->dev, "ext_xtal");
		break;
	}
	if (IS_ERR_OR_NULL(phy_drd->ref_clk)) {
		dev_err(phy_drd->dev, "%s failed to get ref_clk", __func__);
		return 0;
	}
	ret = clk_prepare_enable(phy_drd->ref_clk);
	if (ret) {
		dev_err(phy_drd->dev, "%s failed to enable ref_clk", __func__);
		return 0;
	}

	/* EXYNOS_FSEL_MASK */
	switch (clk_get_rate(phy_drd->ref_clk)) {
	case 9600 * KHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_9MHZ6;
		break;
	case 10 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_10MHZ;
		break;
	case 12 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_12MHZ;
		break;
	case 19200 * KHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_19MHZ2;
		break;
	case 20 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_20MHZ;
		break;
	case 24 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_24MHZ;
		break;
	case 26 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_26MHZ;
		break;
	case 50 * MHZ:
		phy_drd->extrefclk = EXYNOS_FSEL_50MHZ;
		break;
	default:
		phy_drd->extrefclk = 0;
		clk_disable_unprepare(phy_drd->ref_clk);
		return -EINVAL;
	}

	clk_disable_unprepare(phy_drd->ref_clk);

	return 0;
}

static void exynos_usbdrd_pipe3_phy_isol(struct phy_usb_instance *inst,
					unsigned int on, unsigned int mask)
{
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);
	unsigned int val;

	if (!inst->reg_pmu)
		return;

	if (on && inst->uart_io_share_en) {
		if (!regmap_read(inst->reg_pmu,
				 inst->uart_io_share_offset, &val)) {
			if (val & inst->uart_io_share_mask) {
				dev_info(phy_drd->dev,
					"is using now - Skip power off\n");
				return;
			}
		} else {
			dev_err(phy_drd->dev,
					"failed to read UART IO Share reg\n");
		}
	}

	val = on ? 0 : mask;

	regmap_update_bits(inst->reg_pmu, inst->pmu_offset,
			   mask, val);
}

static void exynos_usbdrd_utmi_phy_isol(struct phy_usb_instance *inst,
					unsigned int on, unsigned int mask)
{
	return;
}

/*
 * Sets the pipe3 phy's clk as EXTREFCLK (XXTI) which is internal clock
 * from clock core. Further sets multiplier values and spread spectrum
 * clock settings for SuperSpeed operations.
 */
static unsigned int
exynos_usbdrd_pipe3_set_refclk(struct phy_usb_instance *inst)
{
	static u32 reg;
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* restore any previous reference clock settings */
	reg = readl(phy_drd->reg_phy + EXYNOS_DRD_PHYCLKRST);

	/* Use EXTREFCLK as ref clock */
	reg &= ~PHYCLKRST_REFCLKSEL_MASK;
	reg |=	PHYCLKRST_REFCLKSEL_EXT_REFCLK;

	/* FSEL settings corresponding to reference clock */
	reg &= ~PHYCLKRST_FSEL_PIPE_MASK |
		PHYCLKRST_MPLL_MULTIPLIER_MASK |
		PHYCLKRST_SSC_REFCLKSEL_MASK;
	switch (phy_drd->extrefclk) {
	case EXYNOS_FSEL_50MHZ:
		reg |= (PHYCLKRST_MPLL_MULTIPLIER_50M_REF |
			PHYCLKRST_SSC_REFCLKSEL(0x00));
		break;
	case EXYNOS_FSEL_24MHZ:
		reg |= (PHYCLKRST_MPLL_MULTIPLIER_24MHZ_REF |
			PHYCLKRST_SSC_REFCLKSEL(0x88));
		break;
	case EXYNOS_FSEL_20MHZ:
		reg |= (PHYCLKRST_MPLL_MULTIPLIER_20MHZ_REF |
			PHYCLKRST_SSC_REFCLKSEL(0x00));
		break;
	case EXYNOS_FSEL_19MHZ2:
		reg |= (PHYCLKRST_MPLL_MULTIPLIER_19200KHZ_REF |
			PHYCLKRST_SSC_REFCLKSEL(0x88));
		break;
	default:
		dev_dbg(phy_drd->dev, "unsupported ref clk\n");
		break;
	}

	return reg;
}

/*
 * Sets the utmi phy's clk as EXTREFCLK (XXTI) which is internal clock
 * from clock core. Further sets the FSEL values for HighSpeed operations.
 */
static unsigned int
exynos_usbdrd_utmi_set_refclk(struct phy_usb_instance *inst)
{
	static u32 reg;
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* restore any previous reference clock settings */
	reg = readl(phy_drd->reg_phy + EXYNOS_DRD_PHYCLKRST);

	reg &= ~PHYCLKRST_REFCLKSEL_MASK;
	reg |=	PHYCLKRST_REFCLKSEL_EXT_REFCLK;

	reg &= ~PHYCLKRST_FSEL_UTMI_MASK |
		PHYCLKRST_MPLL_MULTIPLIER_MASK |
		PHYCLKRST_SSC_REFCLKSEL_MASK;
	reg |= PHYCLKRST_FSEL(phy_drd->extrefclk);

	return reg;
}

/*
 * Sets the default PHY tuning values for high-speed connection.
 */

static void exynos_usbdrd_fill_hstune(struct exynos_usbdrd_phy *phy_drd,
							struct device_node *node)
{
	struct device *dev = phy_drd->dev;
	struct exynos_usbphy_hs_tune *hs_tune = phy_drd->hs_value;
	int ret;
	u32 res[2];
	u32 value;

	if ( node == NULL)
		return;

	ret = of_property_read_u32_array(node, "tx_vref", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_vref = res[0];
		hs_tune[1].tx_vref = res[1];
	} else {
		dev_err(dev, "can't get tx_vref value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_pre_emp", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_pre_emp = res[0];
		hs_tune[1].tx_pre_emp = res[1];
	} else {
		dev_err(dev, "can't get tx_pre_emp value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_pre_emp_puls", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_pre_emp_puls = res[0];
		hs_tune[1].tx_pre_emp_puls = res[1];
	} else {
		dev_err(dev, "can't get tx_pre_emp_puls value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_res", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_res = res[0];
		hs_tune[1].tx_res = res[1];
	} else {
		dev_err(dev, "can't get tx_res value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_rise", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_rise = res[0];
		hs_tune[1].tx_rise = res[1];
	} else {
		dev_err(dev, "can't get tx_rise value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_hsxv", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_hsxv = res[0];
		hs_tune[1].tx_hsxv = res[1];
	} else {
		dev_err(dev, "can't get tx_hsxv value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_fsls", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].tx_fsls = res[0];
		hs_tune[1].tx_fsls = res[1];
	} else {
		dev_err(dev, "can't get tx_fsls value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "rx_sqrx", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].rx_sqrx = res[0];
		hs_tune[1].rx_sqrx = res[1];
	} else {
		dev_err(dev, "can't get tx_sqrx value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "compdis", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].compdis = res[0];
		hs_tune[1].compdis = res[1];
	} else {
		dev_err(dev, "can't get compdis value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "otg", res, 2);
	if ( ret == 0 ) {
		hs_tune[0].otg = res[0];
		hs_tune[1].otg = res[1];
	} else {
		dev_err(dev, "can't get otg_tune value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "enable_user_imp", res, 2);
	if ( ret == 0 ) {
		if ( res[0] ) {
			hs_tune[0].enable_user_imp = true;
			hs_tune[1].enable_user_imp = true;
			hs_tune[0].user_imp_value = res[1];
			hs_tune[1].user_imp_value = res[1];
		} else {
			hs_tune[0].enable_user_imp = false;
			hs_tune[1].enable_user_imp = false;
		}
	} else {
		dev_err(dev, "can't get enable_user_imp value, error = %d\n",ret);
	}

	ret = of_property_read_u32(node, "is_phyclock", &value);
	if ( ret == 0 ) {
		if ( value == 1) {
			hs_tune[0].utmi_clk = USBPHY_UTMI_PHYCLOCK;
			hs_tune[1].utmi_clk = USBPHY_UTMI_PHYCLOCK;
		} else {
			hs_tune[0].utmi_clk = USBPHY_UTMI_FREECLOCK;
			hs_tune[1].utmi_clk = USBPHY_UTMI_FREECLOCK;
		}
	} else {
		dev_err(dev, "can't get is_phyclock value, error = %d\n",ret);
	}

	return;
}


/*
 * Sets the default PHY tuning values for super-speed connection.
 */
static void exynos_usbdrd_fill_sstune(struct exynos_usbdrd_phy *phy_drd,
							struct device_node *node)
{
	struct device *dev = phy_drd->dev;
	struct exynos_usbphy_ss_tune *ss_tune = phy_drd->ss_value;
	u32 res[2];
	int ret;

	if ( node == NULL)
		return;

	ret = of_property_read_u32_array(node, "tx_boost_level", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_boost_level = res[0];
		ss_tune[1].tx_boost_level = res[1];
	} else {
		dev_err(dev, "can't get tx_boost_level value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_swing_level", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_swing_level = res[0];
		ss_tune[1].tx_swing_level = res[1];
	} else {
		dev_err(dev, "can't get tx_swing_level value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_swing_full", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_swing_full = res[0];
		ss_tune[1].tx_swing_full = res[1];
	} else {
		dev_err(dev, "can't get tx_swing_full value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_swing_low", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_swing_low = res[0];
		ss_tune[1].tx_swing_low = res[1];
	} else {
		dev_err(dev, "can't get tx_swing_low value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_deemphasis_mode", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_deemphasis_mode = res[0];
		ss_tune[1].tx_deemphasis_mode = res[1];
	} else {
		dev_err(dev, "can't get tx_deemphasis_mode value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_deemphasis_3p5db", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_deemphasis_3p5db = res[0];
		ss_tune[1].tx_deemphasis_3p5db = res[1];
	} else {
		dev_err(dev, "can't get tx_deemphasis_3p5db value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "tx_deemphasis_6db", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].tx_deemphasis_6db = res[0];
		ss_tune[1].tx_deemphasis_6db = res[1];
	} else {
		dev_err(dev, "can't get tx_deemphasis_6db value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "enable_ssc", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].enable_ssc = res[0];
		ss_tune[1].enable_ssc = res[1];
	} else {
		dev_err(dev, "can't get enable_ssc value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "ssc_range", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].ssc_range = res[0];
		ss_tune[1].ssc_range = res[1];
	} else {
		dev_err(dev, "can't get ssc_range value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "los_bias", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].los_bias = res[0];
		ss_tune[1].los_bias = res[1];
	} else {
		dev_err(dev, "can't get los_bias value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "los_mask_val", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].los_mask_val = res[0];
		ss_tune[1].los_mask_val = res[1];
	} else {
		dev_err(dev, "can't get los_mask_val value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "enable_fixed_rxeq_mode", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].enable_fixed_rxeq_mode = res[0];
		ss_tune[1].enable_fixed_rxeq_mode = res[1];
	} else {
		dev_err(dev, "can't get enable_fixed_rxeq_mode value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "fix_rxeq_value", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].fix_rxeq_value = res[0];
		ss_tune[1].fix_rxeq_value = res[1];
	} else {
		dev_err(dev, "can't get fix_rxeq_value value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "set_crport_level_en", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].set_crport_level_en = res[0];
		ss_tune[1].set_crport_level_en = res[1];
	} else {
		dev_err(dev, "can't get set_crport_level_en value, error = %d \n",ret);
	}

	ret = of_property_read_u32_array(node, "set_crport_mpll_charge_pump", res, 2);
	if ( ret == 0 ) {
		ss_tune[0].set_crport_mpll_charge_pump = res[0];
		ss_tune[1].set_crport_mpll_charge_pump = res[1];
	} else {
		dev_err(dev, "can't get set_crport_mpll_charge_pump value, error = %d \n",ret);
	}

	return;
}

static int exynos_usbdrd_get_phyinfo(struct exynos_usbdrd_phy *phy_drd)
{
	struct device *dev = phy_drd->dev;
	struct device_node *node = dev->of_node;
	struct device_node *tune_node;

	switch (phy_drd->drv_data->cpu_type) {
	case TYPE_EXYNOS8890:
		if (phy_drd->drv_data->ip_type == TYPE_USB3DRD) {
			phy_drd->usbphy_info.version = EXYNOS_USBCON_VER_01_0_1;
			phy_drd->usbphy_info.refsel =
						USBPHY_REFSEL_DIFF_INTERNAL;
			phy_drd->usbphy_info.use_io_for_ovc = true;
			phy_drd->usbphy_info.common_block_enable = false;
		} else {
			phy_drd->usbphy_info.version = EXYNOS_USBCON_VER_02_1_1;
			phy_drd->usbphy_info.refsel =
						USBPHY_REFCLK_EXT_12MHZ;
			phy_drd->usbphy_info.use_io_for_ovc = false;
			phy_drd->usbphy_info.common_block_enable = false;
		}
		break;
	case TYPE_EXYNOS7870:
	case TYPE_EXYNOS7570:
		phy_drd->usbphy_info.version = EXYNOS_USBCON_VER_02_1_0,
		phy_drd->usbphy_info.refsel = USBPHY_REFSEL_CLKCORE,
		phy_drd->usbphy_info.use_io_for_ovc = false;
		break;
	default:
		dev_err(phy_drd->dev, "%s: unknown cpu type\n", __func__);
		return -EINVAL;
	}

	phy_drd->usbphy_info.refclk = phy_drd->extrefclk;
	phy_drd->usbphy_info.regs_base = phy_drd->reg_phy;
	phy_drd->usbphy_info.not_used_vbus_pad = of_property_read_bool(node,
							"is_not_vbus_pad");

	tune_node = of_parse_phandle(dev->of_node, "ss_tune_info",0);
	if (tune_node == NULL) {
		dev_info(dev, "don't need usbphy tuning value for super speed\n");
	} else if (of_device_is_available(tune_node)) {
		phy_drd->ss_value = devm_kmalloc(phy_drd->dev,
				2 * sizeof(struct exynos_usbphy_ss_tune), GFP_KERNEL);
		if (!phy_drd->ss_value) {
			dev_err(phy_drd->dev, "%s: failed to alloc for ss tune\n",
					__func__);
			return -ENOMEM;
		}

		exynos_usbdrd_fill_sstune(phy_drd, tune_node);
	}

	tune_node = of_parse_phandle(dev->of_node, "hs_tune_info",0);
	if (tune_node == NULL) {
		dev_info(dev, "don't need usbphy tuning value for high speed\n");
		goto done;
	}else if (of_device_is_available(tune_node)) {
		phy_drd->hs_value = devm_kmalloc(phy_drd->dev,
				2 * sizeof(struct exynos_usbphy_hs_tune), GFP_KERNEL);
		if (!phy_drd->hs_value) {
			dev_err(phy_drd->dev, "%s: failed to alloc for hs tune\n",
					__func__);
			return -ENOMEM;
		}

		exynos_usbdrd_fill_hstune(phy_drd, tune_node);
	}

done:
	dev_info(phy_drd->dev, "usbphy info: version:0x%x, refclk:0x%x\n",
		phy_drd->usbphy_info.version, phy_drd->usbphy_info.refclk);

	return 0;
}

static void exynos_usbdrd_pipe3_init(struct exynos_usbdrd_phy *phy_drd)
{
	int ret;

	ret = exynos_usbdrd_clk_enable(phy_drd, false);
	if (ret) {
		dev_err(phy_drd->dev, "%s: Failed to enable clk\n", __func__);
		return;
	}

	samsung_exynos_cal_usb3phy_enable(&phy_drd->usbphy_info);

	if (phy_drd->drv_data->phy_usermux) {
		/* Check external reference clock supply */
		if (phy_drd->request_extrefclk) {
			ret = exynos_usbdrd_check_extrefclk(phy_drd);
			if (ret) {
				dev_err(phy_drd->dev,
				"%s ref_clk request timeout\n", __func__);
				return;
			}
		}
		/* USB User MUX enable */
		ret = exynos_usbdrd_clk_enable(phy_drd, true);
		if (ret) {
			dev_err(phy_drd->dev, "%s: Failed to enable clk\n", __func__);
			return;
		}
	}
}

static void exynos_usbdrd_utmi_init(struct exynos_usbdrd_phy *phy_drd)
{
	return;
}

static int exynos_usbdrd_phy_init(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* UTMI or PIPE3 specific init */
	inst->phy_cfg->phy_init(phy_drd);

	return 0;
}

static void __exynos_usbdrd_phy_shutdown(struct exynos_usbdrd_phy *phy_drd)
{
	samsung_exynos_cal_usb3phy_disable(&phy_drd->usbphy_info);
}

static void exynos_usbdrd_pipe3_exit(struct exynos_usbdrd_phy *phy_drd)
{
	if (phy_drd->drv_data->phy_usermux) {
		/*USB User MUX disable */
		exynos_usbdrd_clk_disable(phy_drd, true);
	}
	__exynos_usbdrd_phy_shutdown(phy_drd);

	exynos_usbdrd_clk_disable(phy_drd, false);
}

static void exynos_usbdrd_utmi_exit(struct exynos_usbdrd_phy *phy_drd)
{
	return;
}

static int exynos_usbdrd_phy_exit(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	/* UTMI or PIPE3 specific exit */
	inst->phy_cfg->phy_exit(phy_drd);

	return 0;
}

static void exynos_usbdrd_pipe3_tune(struct exynos_usbdrd_phy *phy_drd,
							int phy_state)
{
	struct exynos_usbphy_ss_tune *ss_value = phy_drd->ss_value;
	struct exynos_usbphy_hs_tune *hs_value = phy_drd->hs_value;

	if (!ss_value && !hs_value) {
		dev_dbg(phy_drd->dev, "There are no phytune values\n");
		return;
	} else {
		dev_info(phy_drd->dev, "Try to phytune for state : %d\n", phy_state);
	}

	if (phy_state >= OTG_STATE_A_IDLE) {
		/* for host mode */
		phy_drd->usbphy_info.ss_tune = &ss_value[USBPHY_MODE_HOST];
		phy_drd->usbphy_info.hs_tune = &hs_value[USBPHY_MODE_HOST];

		samsung_exynos_cal_usb3phy_tune_host(&phy_drd->usbphy_info);
	} else {
		/* for device mode */
		phy_drd->usbphy_info.ss_tune = &ss_value[USBPHY_MODE_DEV];
		phy_drd->usbphy_info.hs_tune = &hs_value[USBPHY_MODE_DEV];

		samsung_exynos_cal_usb3phy_tune_dev(&phy_drd->usbphy_info);
	}
}

static void exynos_usbdrd_utmi_tune(struct exynos_usbdrd_phy *phy_drd,
							int phy_state)
{
	return;
}

static int exynos_usbdrd_phy_tune(struct phy *phy, int phy_state)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	inst->phy_cfg->phy_tune(phy_drd, phy_state);

	return 0;
}

static void exynos_usbdrd_pipe3_set(struct exynos_usbdrd_phy *phy_drd,
						int option, void *info)
{
	int *ret;

	switch (option) {
	case SET_DPPULLUP_ENABLE:
		samsung_exynos_cal_usb3phy_enable_dp_pullup(
					&phy_drd->usbphy_info);
		break;
	case SET_DPPULLUP_DISABLE:
		samsung_exynos_cal_usb3phy_disable_dp_pullup(
					&phy_drd->usbphy_info);
		break;
	case SET_DPDM_PULLDOWN:
		samsung_exynos_cal_usb3phy_config_host_mode(
					&phy_drd->usbphy_info);
		break;
	case SET_EXTREFCLK_REQUEST:
		ret = (int *)info;
		*ret = exynos_usbdrd_request_extrefclk(phy_drd, NULL);
		break;
	case SET_EXTREFCLK_RELEASE:
		exynos_usbdrd_release_extrefclk(phy_drd, NULL);
		break;
	default:
		break;
	}
}

static void exynos_usbdrd_utmi_set(struct exynos_usbdrd_phy *phy_drd,
						int option, void *info)
{
	return;
}

static int exynos_usbdrd_phy_set(struct phy *phy, int option, void *info)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	inst->phy_cfg->phy_set(phy_drd, option, info);

	return 0;
}

static int exynos_usbdrd_phy_power_on(struct phy *phy)
{
	int ret;
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	dev_dbg(phy_drd->dev, "Request to power_on usbdrd_phy phy\n");

	/* Enable VBUS supply */
	if (phy_drd->vbus) {
		ret = regulator_enable(phy_drd->vbus);
		if (ret) {
			dev_err(phy_drd->dev, "Failed to enable VBUS supply\n");
			return ret;
		}
	}

	/* Power-on PHY*/
	switch (phy_drd->drv_data->cpu_type) {
	case TYPE_EXYNOS8890:
		if (phy_drd->drv_data->ip_type == TYPE_USB3DRD)
			inst->phy_cfg->phy_isol(inst, 0, EXYNOS_USB3PHY_ENABLE);
		else
			inst->phy_cfg->phy_isol(inst, 0, EXYNOS_USB2PHY_ENABLE);
		break;
	case TYPE_EXYNOS7870:
	case TYPE_EXYNOS7570:
		inst->phy_cfg->phy_isol(inst, 0, EXYNOS_USB2PHY_ENABLE);
		break;
	default:
		inst->phy_cfg->phy_isol(inst, 0, EXYNOS5_PHY_ENABLE);
		break;
	}

	return 0;
}

static int exynos_usbdrd_phy_power_off(struct phy *phy)
{
	struct phy_usb_instance *inst = phy_get_drvdata(phy);
	struct exynos_usbdrd_phy *phy_drd = to_usbdrd_phy(inst);

	dev_dbg(phy_drd->dev, "Request to power_off usbdrd_phy phy\n");

	/* Power-off the PHY */
	switch (phy_drd->drv_data->cpu_type) {
	case TYPE_EXYNOS8890:
		if (phy_drd->drv_data->ip_type == TYPE_USB3DRD)
			inst->phy_cfg->phy_isol(inst, 1, EXYNOS_USB3PHY_ENABLE);
		else
			inst->phy_cfg->phy_isol(inst, 1, EXYNOS_USB2PHY_ENABLE);
		break;
	case TYPE_EXYNOS7870:
	case TYPE_EXYNOS7570:
		inst->phy_cfg->phy_isol(inst, 1, EXYNOS_USB2PHY_ENABLE);
		break;
	default:
		inst->phy_cfg->phy_isol(inst, 1, EXYNOS5_PHY_ENABLE);
		break;
	}

	/* Disable VBUS supply */
	if (phy_drd->vbus)
		regulator_disable(phy_drd->vbus);

	return 0;
}

static struct phy *exynos_usbdrd_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_usbdrd_phy *phy_drd = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] > EXYNOS_DRDPHYS_NUM))
		return ERR_PTR(-ENODEV);

	return phy_drd->phys[args->args[0]].phy;
}

static struct phy_ops exynos_usbdrd_phy_ops = {
	.init		= exynos_usbdrd_phy_init,
	.exit		= exynos_usbdrd_phy_exit,
	.tune		= exynos_usbdrd_phy_tune,
	.set		= exynos_usbdrd_phy_set,
	.power_on	= exynos_usbdrd_phy_power_on,
	.power_off	= exynos_usbdrd_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct exynos_usbdrd_phy_config phy_cfg_exynos[] = {
	{
		.id		= EXYNOS_DRDPHY_UTMI,
		.phy_isol	= exynos_usbdrd_utmi_phy_isol,
		.phy_init	= exynos_usbdrd_utmi_init,
		.phy_exit	= exynos_usbdrd_utmi_exit,
		.phy_tune	= exynos_usbdrd_utmi_tune,
		.phy_set	= exynos_usbdrd_utmi_set,
		.set_refclk	= exynos_usbdrd_utmi_set_refclk,
	},
	{
		.id		= EXYNOS_DRDPHY_PIPE3,
		.phy_isol	= exynos_usbdrd_pipe3_phy_isol,
		.phy_init	= exynos_usbdrd_pipe3_init,
		.phy_exit	= exynos_usbdrd_pipe3_exit,
		.phy_tune	= exynos_usbdrd_pipe3_tune,
		.phy_set	= exynos_usbdrd_pipe3_set,
		.set_refclk	= exynos_usbdrd_pipe3_set_refclk,
	},
};

static const struct exynos_usbdrd_phy_drvdata exynos8890_usbdrd_phy = {
	.phy_cfg		= phy_cfg_exynos,
	.pmu_offset_usbdrd0_phy	= EXYNOS_USBDEV_PHY_CONTROL,
	.cpu_type		= TYPE_EXYNOS8890,
	.ip_type		= TYPE_USB3DRD,
	.phy_usermux		= false,
};

static const struct exynos_usbdrd_phy_drvdata exynos8890_usbhost_phy = {
	.phy_cfg		= phy_cfg_exynos,
	.pmu_offset_usbdrd0_phy	= EXYNOS_USBDEV_PHY_CONTROL,
	.cpu_type		= TYPE_EXYNOS8890,
	.ip_type		= TYPE_USB2HOST,
	.phy_usermux		= false,
};

static const struct exynos_usbdrd_phy_drvdata exynos7870_usbdrd_phy = {
	.phy_cfg		= phy_cfg_exynos,
	.pmu_offset_usbdrd0_phy	= EXYNOS_USBDEV_PHY_CONTROL,
	.cpu_type		= TYPE_EXYNOS7870,
	.phy_usermux		= true,
};

static const struct exynos_usbdrd_phy_drvdata exynos7570_usbdrd_phy = {
	.phy_cfg		= phy_cfg_exynos,
	.pmu_offset_usbdrd0_phy	= EXYNOS_USBDEV_PHY_CONTROL,
	.cpu_type		= TYPE_EXYNOS7570,
	.phy_usermux		= true,
};

static const struct of_device_id exynos_usbdrd_phy_of_match[] = {
	{
		.compatible = "samsung,exynos8890-usbdrd-phy",
		.data = &exynos8890_usbdrd_phy
	}, {
		.compatible = "samsung,exynos8890-usbhost-phy",
		.data = &exynos8890_usbhost_phy
	}, {
		.compatible = "samsung,exynos7870-usbdrd-phy",
		.data = &exynos7870_usbdrd_phy
	}, {
		.compatible = "samsung,exynos7570-usbdrd-phy",
		.data = &exynos7570_usbdrd_phy
	},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos5_usbdrd_phy_of_match);

static int exynos_usbdrd_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	struct exynos_usbdrd_phy *phy_drd;
	struct phy_provider *phy_provider;
	struct resource *res;
	const struct of_device_id *match;
	const struct exynos_usbdrd_phy_drvdata *drv_data;
	struct regmap *reg_pmu;
	u32 pmu_offset;
	u32 uart_io_share_en, uart_io_share_offset, uart_io_share_mask;
	int i, ret;
	int channel;

	phy_drd = devm_kzalloc(dev, sizeof(*phy_drd), GFP_KERNEL);
	if (!phy_drd)
		return -ENOMEM;

	dev_set_drvdata(dev, phy_drd);
	phy_drd->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy_drd->reg_phy = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy_drd->reg_phy))
		return PTR_ERR(phy_drd->reg_phy);

	match = of_match_node(exynos_usbdrd_phy_of_match, pdev->dev.of_node);

	drv_data = match->data;
	phy_drd->drv_data = drv_data;

	ret = exynos_usbdrd_clk_get(phy_drd);
	if (ret) {
		dev_err(dev, "%s: Failed to get clocks\n", __func__);
		return ret;
	}

	ret = exynos_usbdrd_clk_prepare(phy_drd);
	if (ret) {
		dev_err(dev, "%s: Failed to prepare clocks\n", __func__);
		return ret;
	}

	ret = exynos_rate_to_clk(phy_drd);
	if (ret) {
		dev_err(phy_drd->dev, "%s: Not supported ref clock\n",
				__func__);
		goto err1;
	}

	ret = exynos_usbdrd_ready_extrefclk(phy_drd);
	if (ret) {
		dev_err(dev, "%s: Failed to ready extrefclk\n", __func__);
		return ret;
	}

	reg_pmu = syscon_regmap_lookup_by_phandle(dev->of_node,
						   "samsung,pmu-syscon");
	if (IS_ERR(reg_pmu)) {
		dev_err(dev, "Failed to lookup PMU regmap\n");
		goto err1;
	}

	channel = of_alias_get_id(node, "usbdrdphy");
	if (channel < 0)
		dev_dbg(dev, "Not a multi-controller usbdrd phy\n");

	switch (channel) {
	case 1:
		pmu_offset = phy_drd->drv_data->pmu_offset_usbdrd1_phy;
		break;
	case 0:
	default:
		pmu_offset = phy_drd->drv_data->pmu_offset_usbdrd0_phy;
		break;
	}

	ret = of_property_read_u32(node, "uart_io_share_enable", &uart_io_share_en);
	if (ret) {
		dev_err(dev, "Failed to get UART IO Share info\n");
		goto err1;
	}
	if (uart_io_share_en) {
		ret = of_property_read_u32(node,
				"uart_io_share_offset", &uart_io_share_offset);
		if (ret) {
			dev_err(dev, "Failed to get UART IO Share offset\n");
			goto err1;
		}
		ret = of_property_read_u32(node,
				"uart_io_share_mask", &uart_io_share_mask);
		if (ret) {
			dev_err(dev, "Failed to get UART IO Share mask\n");
			goto err1;
		}
	}
	dev_vdbg(dev, "Creating usbdrd_phy phy\n");

	ret = exynos_usbdrd_get_phyinfo(phy_drd);
	if (ret)
		goto err1;

	for (i = 0; i < EXYNOS_DRDPHYS_NUM; i++) {
		struct phy *phy = devm_phy_create(dev, NULL,
						  &exynos_usbdrd_phy_ops,
						  NULL);
		if (IS_ERR(phy)) {
			dev_err(dev, "Failed to create usbdrd_phy phy\n");
			goto err1;
		}

		phy_drd->phys[i].phy = phy;
		phy_drd->phys[i].index = i;
		phy_drd->phys[i].reg_pmu = reg_pmu;
		phy_drd->phys[i].pmu_offset = pmu_offset;
		phy_drd->phys[i].uart_io_share_en = uart_io_share_en;
		phy_drd->phys[i].uart_io_share_offset = uart_io_share_offset;
		phy_drd->phys[i].uart_io_share_mask = uart_io_share_mask;
		phy_drd->phys[i].phy_cfg = &drv_data->phy_cfg[i];
		phy_set_drvdata(phy, &phy_drd->phys[i]);
	}

	phy_provider = devm_of_phy_provider_register(dev,
						     exynos_usbdrd_phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(phy_drd->dev, "Failed to register phy provider\n");
		goto err1;
	}

	return 0;
err1:
	exynos_usbdrd_clk_unprepare(phy_drd);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_usbdrd_phy_resume(struct device *dev)
{
	int ret;
	struct exynos_usbdrd_phy *phy_drd = dev_get_drvdata(dev);

	/*
	 * There is issue, when USB3.0 PHY is in active state
	 * after resume. This leads to increased power consumption
	 * if no USB drivers use the PHY.
	 *
	 * The following code shutdowns the PHY, so it is in defined
	 * state (OFF) after resume. If any USB driver already got
	 * the PHY at this time, we do nothing and just exit.
	 */

	dev_dbg(dev, "%s\n", __func__);

	ret = exynos_usbdrd_clk_enable(phy_drd, false);
	if (ret) {
		dev_err(phy_drd->dev, "%s: Failed to enable clk\n", __func__);
		return ret;
	}

	__exynos_usbdrd_phy_shutdown(phy_drd);

	exynos_usbdrd_clk_disable(phy_drd, false);

	return 0;
}

static const struct dev_pm_ops exynos_usbdrd_phy_dev_pm_ops = {
	.resume	= exynos_usbdrd_phy_resume,
};

#define EXYNOS_USBDRD_PHY_PM_OPS	&(exynos_usbdrd_phy_dev_pm_ops)
#else
#define EXYNOS_USBDRD_PHY_PM_OPS	NULL
#endif

static struct platform_driver phy_exynos_usbdrd = {
	.probe	= exynos_usbdrd_phy_probe,
	.driver = {
		.of_match_table	= exynos_usbdrd_phy_of_match,
		.name		= "phy_exynos_usbdrd",
		.pm		= EXYNOS_USBDRD_PHY_PM_OPS,
	}
};

module_platform_driver(phy_exynos_usbdrd);
MODULE_DESCRIPTION("Samsung EXYNOS SoCs USB DRD controller PHY driver");
MODULE_AUTHOR("Vivek Gautam <gautam.vivek@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:phy_exynos_usbdrd");
