/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/wakeup_reason.h>
#include <linux/gpio.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <asm/psci.h>
#include <asm/suspend.h>
#include <asm/smp_plat.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/exynos-pd.h>

#include <sound/exynos-audmixer.h>

#include <linux/irqdesc.h>
#include <linux/interrupt.h>
#ifdef CONFIG_IRQ_HISTORY
#include <linux/power/irq_history.h>
#endif

#define WAKEUP_STAT_EINT			(1 << 0)
#define WAKEUP_STAT_RTC_ALARM		(1 << 1)
#define WAKEUP_STAT_CP				(BIT(27) | BIT(26) | BIT(25) | BIT(24))
/*
 * PMU register offset
 */
#define EXYNOS_PMU_WAKEUP_STAT		0x0600
#define EXYNOS_PMU_EINT_WAKEUP_MASK	0x060C
#define BOOT_CPU			0
#define NR_CPUS_PER_CLUSTER		4

extern u32 exynos_eint_to_pin_num(int eint);
#define EXYNOS_EINT_PEND(b, x)      ((b) + 0xA00 + (((x) >> 3) * 4))

struct exynos_pm_info {
	void __iomem *eint_base;		/* GPIO_ALIVE base to check wkup reason */
	void __iomem *gic_base;			/* GICD_ISPENDRn base to check wkup reason */
	unsigned int num_eint;			/* Total number of EINT sources */
	unsigned int num_gic;			/* Total number of GIC sources */
	bool is_early_wakeup;
	bool is_cp_call;
	unsigned int suspend_mode_idx;		/* power mode to be used in suspend scenario */
	unsigned int suspend_psci_idx;		/* psci index to be used in suspend scenario */
	unsigned int cp_call_mode_idx;		/* power mode to be used in cp_call scenario */
	unsigned int cp_call_psci_idx;		/* psci index to be used in cp_call scenario */
};
static struct exynos_pm_info *pm_info;

struct exynos_pm_dbg {
	u32 test_early_wakeup;
	u32 test_cp_call;
};
static struct exynos_pm_dbg *pm_dbg;

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int i, size;
	long unsigned int ext_int_pend;
	u64 eint_wakeup_mask;
	bool found = 0;
	unsigned int val;

	exynos_pmu_read(EXYNOS_PMU_EINT_WAKEUP_MASK, &val);
	eint_wakeup_mask = val;

	for (i = 0, size = 8; i < pm_info->num_eint; i += size) {
		ext_int_pend =
			__raw_readl(EXYNOS_EINT_PEND(pm_info->eint_base, i));

		for_each_set_bit(bit, &ext_int_pend, size) {
			struct irq_desc *desc;
			u32 gpio;
			int irq;

			if (eint_wakeup_mask & (1 << (i + bit)))
				continue;

			gpio = exynos_eint_to_pin_num(i + bit);
			irq = gpio_to_irq(gpio);
			desc = irq_to_desc(irq);

#ifdef CONFIG_SUSPEND
#ifdef CONFIG_IRQ_HISTORY
			add_irq_history(irq, NULL);
#else
			log_wakeup_reason(irq);
#endif
			update_wakeup_reason_stats(irq, i + bit);
#endif
			found = 1;
		}
	}

	if (!found)
		pr_info("%s Resume caused by unknown EINT\n", EXYNOS_PM_PREFIX);
}

static void exynos_show_wakeup_registers(unsigned long wakeup_stat)
{
	int i, size;

	pr_info("WAKEUP_STAT: 0x%08lx\n", wakeup_stat);

	pr_info("EINT_PEND: ");
	for (i = 0, size = 8; i < pm_info->num_eint; i += size)
		pr_info("0x%02x ", __raw_readl(EXYNOS_EINT_PEND(pm_info->eint_base, i)));

	pr_info("\n");
}

static void exynos_show_wakeup_reason(bool sleep_abort)
{
	unsigned int wakeup_stat;
	int i, size;

	if (sleep_abort) {
		pr_info("%s early wakeup! Dumping pending registers...\n", EXYNOS_PM_PREFIX);

		pr_info("EINT_PEND:\n");
		for (i = 0, size = 8; i < pm_info->num_eint; i += size)
			pr_info("0x%x\n", __raw_readl(EXYNOS_EINT_PEND(pm_info->eint_base, i)));

		pr_info("GIC_PEND:\n");
		for (i = 0; i < pm_info->num_gic; i++)
			pr_info("GICD_ISPENDR[%d] = 0x%x\n", i, __raw_readl(pm_info->gic_base + i*4));

		pr_info("%s done.\n", EXYNOS_PM_PREFIX);
		return ;
	}

	exynos_pmu_read(EXYNOS_PMU_WAKEUP_STAT, &wakeup_stat);

	exynos_show_wakeup_registers(wakeup_stat);

	if (wakeup_stat & WAKEUP_STAT_RTC_ALARM)
		pr_info("%s Resume caused by RTC alarm\n", EXYNOS_PM_PREFIX);
	else if (wakeup_stat & WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else if (wakeup_stat & WAKEUP_STAT_CP)
#ifdef CONFIG_IRQ_HISTORY
		add_irq_history(0, "CP");
#else
		pr_info("%s Resume caused by CP (wakeup_stat 0x%08x)\n",
			EXYNOS_PM_PREFIX, wakeup_stat);
#endif
	else
		pr_info("%s Resume caused by wakeup_stat 0x%08x\n",
			EXYNOS_PM_PREFIX, wakeup_stat);
}

#ifdef CONFIG_CPU_IDLE
static DEFINE_RWLOCK(exynos_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_pm_notifier_chain);

static int exynos_pm_notify(enum exynos_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&exynos_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

int exynos_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_register_notifier);

int exynos_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_unregister_notifier);

int exynos_pm_lpa_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_ENTER, -1, &nr_calls);
	if (ret)
		/*
		 * Inform listeners (nr_calls - 1) about failure of LPA
		 * entry who are notified earlier to prepare for it.
		 */
		exynos_pm_notify(LPA_ENTER_FAIL, nr_calls - 1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_lpa_enter);

int exynos_pm_lpa_exit(void)
{
	int ret;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_EXIT, -1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_lpa_exit);

int exynos_pm_sicd_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(SICD_ENTER, -1, &nr_calls);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_sicd_enter);

int exynos_pm_sicd_exit(void)
{
	int ret;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(SICD_EXIT, -1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_sicd_exit);
#endif /* CONFIG_CPU_IDLE */

static int exynos_pm_syscore_suspend(void)
{
	if (!exynos_check_cp_status()) {
		pr_info("%s %s: sleep canceled by CP reset \n",
					EXYNOS_PM_PREFIX, __func__);
		return -EINVAL;
	}

	pm_info->is_cp_call = is_cp_aud_enabled();
	if (pm_info->is_cp_call || pm_dbg->test_cp_call) {
		exynos_prepare_sys_powerdown(pm_info->cp_call_mode_idx, true);
		pr_info("%s %s: Enter CP Call scenario. (mode_idx = %d)\n",
				EXYNOS_PM_PREFIX, __func__, pm_info->cp_call_mode_idx);
	} else {
		exynos_prepare_sys_powerdown(pm_info->suspend_mode_idx, true);
		pr_info("%s %s: Enter Suspend scenario. (mode_idx = %d)\n",
				EXYNOS_PM_PREFIX,__func__, pm_info->suspend_mode_idx);
	}

	return 0;
}

static void exynos_pm_syscore_resume(void)
{
	if (pm_info->is_cp_call || pm_dbg->test_cp_call)
		exynos_wakeup_sys_powerdown(pm_info->cp_call_mode_idx, pm_info->is_early_wakeup);
	else
		exynos_wakeup_sys_powerdown(pm_info->suspend_mode_idx, pm_info->is_early_wakeup);

	exynos_show_wakeup_reason(pm_info->is_early_wakeup);

	if (!pm_info->is_early_wakeup)
		pr_debug("%s %s: post sleep, preparing to return\n",
						EXYNOS_PM_PREFIX, __func__);

#if defined(CONFIG_EXYNOS_SNAPSHOT) && defined(CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE)
	if (exynos_ss_get_enable("base", true) == true) {
		void __iomem *dbg_base_addr;
		char buf[64] = {0, };
		int  sz = 0, i;

		/* Suspend/Resume debug */
		pr_info("%s: Suspend/Resume debug\n", __func__);

		sz = 0;
		dbg_base_addr = phys_to_virt(0x461FE000);
		pr_info("[EL3] Cold/Warm branch (base: 0x%p)", dbg_base_addr);
		for (i = 0; i < 10; i++) {
			sz += sprintf(buf + sz, "0x%08x ", readl(dbg_base_addr + (i * sizeof(u32))));

			if (i && i % 4 == 0) {
				pr_info("%s\n", buf);
				memset(buf, 0x0, sizeof(buf));
				sz = 0;
			}
		}

		sz = 0;
		dbg_base_addr = phys_to_virt(0x461FE028);
		pr_info("[EL3] After BL2 (base: 0x%p)", dbg_base_addr);
		for (i = 0; i < 10; i++) {
			sz += sprintf(buf + sz, "0x%08x ", readl(dbg_base_addr + (i * sizeof(u32))));

			if (i && i % 4 == 0) {
				pr_info("%s\n", buf);
				memset(buf, 0x0, sizeof(buf));
				sz = 0;
			}
		}

		pr_info("[BL2] Coldboot: 0x%08x, 0x%08x, 0x%08x\n",
				readl(phys_to_virt(0x461FE804)),
				readl(phys_to_virt(0x461FE80C)),
				readl(phys_to_virt(0x461FE814)));

		pr_info("[BL2] Warmboot: 0x%08x, 0x%08x, 0x%08x\n",
				readl(phys_to_virt(0x461FE824)),
				readl(phys_to_virt(0x461FE82C)),
				readl(phys_to_virt(0x461FE834)));

		memset(phys_to_virt(0x461FE000), 0x0, SZ_4K);
	}
#endif
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_syscore_suspend,
	.resume		= exynos_pm_syscore_resume,
};

#ifdef CONFIG_SEC_GPIO_DVS
extern void gpio_dvs_check_sleepgpio(void);
#endif

static int exynos_pm_enter(suspend_state_t state)
{
	unsigned int psci_index;

	if (pm_info->is_cp_call || pm_dbg->test_cp_call)
		psci_index = pm_info->cp_call_psci_idx;
	else
		psci_index = pm_info->suspend_psci_idx;

	/* Send an IPI if test_early_wakeup flag is set */
	if (pm_dbg->test_early_wakeup)
		arch_send_call_function_single_ipi(0);

#ifdef CONFIG_SEC_GPIO_DVS
	/************************ Caution !!! ****************************/
	/* This function must be located in appropriate SLEEP position
	 * in accordance with the specification of each BB vendor.
	 */
	/************************ Caution !!! ****************************/
	gpio_dvs_check_sleepgpio();
#endif

	/* This will also act as our return point when
	 * we resume as it saves its own register state and restores it
	 * during the resume. */
	printk("PM: SLEEP+++");
	pm_info->is_early_wakeup = (bool)cpu_suspend(psci_index);
	printk("PM: SLEEP---");
	if (pm_info->is_early_wakeup)
		pr_info("%s %s: return to originator\n",
				EXYNOS_PM_PREFIX, __func__);

	return pm_info->is_early_wakeup;
}

static const struct platform_suspend_ops exynos_pm_ops = {
	.enter		= exynos_pm_enter,
	.valid		= suspend_valid_only_mem,
};

static struct bus_type exynos_info_subsys = {
	.name = "exynos_info",
	.dev_name = "exynos_info",
};

static ssize_t core_status_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;
	int cpu, cluster = 0;
	unsigned int mpidr;

	for_each_possible_cpu(cpu) {
		/*
		 * Each cluster has four cores.
		 * "cpu % NR_CPUS_PER_CLUSTER == 0" means that
		 * the cpu is a first one of each cluster.
		 */
		if (!(cpu % NR_CPUS_PER_CLUSTER)) {
			mpidr = cpu_logical_map(cpu);
			cluster =  MPIDR_AFFINITY_LEVEL(mpidr, 1);
			n += scnprintf(buf + n, 24, "%s L2 : %d\n",
				(!cpu) ? "boot" : "nonboot",
				exynos_cpu.l2_state(cluster));

			n += scnprintf(buf + n, 24, "%s Noncpu : %d\n",
				(!cpu) ? "boot" : "nonboot",
				exynos_cpu.noncpu_state(cluster));
		}
		n += scnprintf(buf + n, 24, "CPU%d : %d\n",
				cpu, exynos_cpu.power_state(cpu));
	}

	return n;
}

static struct kobj_attribute exynos_info_attr =
	__ATTR(core_status, 0644, core_status_show, NULL);

static struct attribute *exynos_info_sysfs_attrs[] = {
	&exynos_info_attr.attr,
	NULL,
};

static struct attribute_group exynos_info_sysfs_group = {
	.attrs = exynos_info_sysfs_attrs,
};

static const struct attribute_group *exynos_info_sysfs_groups[] = {
	&exynos_info_sysfs_group,
	NULL,
};

bool is_test_cp_call_set(void)
{
	if (!pm_dbg)
		return false;

	return pm_dbg->test_cp_call;
}
EXPORT_SYMBOL_GPL(is_test_cp_call_set);

#ifdef CONFIG_DEBUG_FS
static void __init exynos_pm_debugfs_init(void)
{
	struct dentry *root, *d;

	root = debugfs_create_dir("exynos-pm", NULL);
	if (!root) {
		pr_err("%s %s: could't create debugfs dir\n", EXYNOS_PM_PREFIX, __func__);
		return;
	}

	d = debugfs_create_u32("test_early_wakeup", 0644, root, &pm_dbg->test_early_wakeup);
	if (!d) {
		pr_err("%s %s: could't create debugfs test_early_wakeup\n",
					EXYNOS_PM_PREFIX, __func__);
		return;
	}

	d = debugfs_create_u32("test_cp_call", 0644, root, &pm_dbg->test_cp_call);
	if (!d) {
		pr_err("%s %s: could't create debugfs test_cp_call\n",
					EXYNOS_PM_PREFIX, __func__);
		return;
	}
}
#endif

enum dvfs_id {
	cal_asv_dvfs_cpucl0,
	cal_asv_dvfs_g3d,
	cal_asv_dvfs_mif,
	cal_asv_dvfs_int,
	cal_asv_dvfs_cam,
	cal_asv_dvfs_disp,
	num_of_dvfs,
};

static ssize_t show_asv_cpu_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* {ASV Tbl ver} {CPU ASV (high)} {CPU ASV (mid)} {CPU ASV (low)} {CPU IDS} */
	return sprintf(buf, "%d 0 0 %d %d\n",
			cal_asv_get_tablever(),
			cal_asv_get_grp(cal_asv_dvfs_cpucl0, 0),
			cal_asv_get_cpu_ids());
}

static ssize_t show_asv_gpu_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* {GPU ASV (high)} {GPU ASV (mid)} {GPU ASV (low)} {GPU IDS} */
	return sprintf(buf, "0 0 %d 0\n",
			cal_asv_get_grp(cal_asv_dvfs_g3d, 0));
}

static DEVICE_ATTR(asv_cpu_info, 0664, show_asv_cpu_info, NULL);
static DEVICE_ATTR(asv_gpu_info, 0664, show_asv_gpu_info, NULL);

static __init int exynos_pm_drvinit(void)
{
	int ret;

	pm_info = kzalloc(sizeof(struct exynos_pm_info), GFP_KERNEL);
	if (pm_info == NULL) {
		pr_err("%s %s: failed to allocate memory for exynos_pm_info\n",
					EXYNOS_PM_PREFIX, __func__);
		BUG();
	}

	pm_dbg = kzalloc(sizeof(struct exynos_pm_dbg), GFP_KERNEL);
	if (pm_dbg == NULL) {
		pr_err("%s %s: failed to allocate memory for exynos_pm_dbg\n",
					EXYNOS_PM_PREFIX, __func__);
		BUG();
	}

	if (of_have_populated_dt()) {
		struct device_node *np;
		np = of_find_compatible_node(NULL, NULL, "samsung,exynos-pm");
		if (!np) {
			pr_err("%s %s: unabled to find compatible node (%s)\n",
					EXYNOS_PM_PREFIX, __func__, "samsung,exynos-pm");
			BUG();
		}

		pm_info->eint_base = of_iomap(np, 0);
		if (!pm_info->eint_base) {
			pr_err("%s %s: unabled to ioremap EINT base address\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		pm_info->gic_base = of_iomap(np, 1);
		if (!pm_info->gic_base) {
			pr_err("%s %s: unbaled to ioremap GIC base address\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "num-eint", &pm_info->num_eint);
		if (ret) {
			pr_err("%s %s: unabled to get the number of eint from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "num-gic", &pm_info->num_gic);
		if (ret) {
			pr_err("%s %s: unabled to get the number of gic from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "suspend_mode_idx", &pm_info->suspend_mode_idx);
		if (ret) {
			pr_err("%s %s: unabled to get suspend_mode_idx from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "suspend_psci_idx", &pm_info->suspend_psci_idx);
		if (ret) {
			pr_err("%s %s: unabled to get suspend_psci_idx from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "cp_call_mode_idx", &pm_info->cp_call_mode_idx);
		if (ret) {
			pr_err("%s %s: unabled to get cp_call_mode_idx from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

		ret = of_property_read_u32(np, "cp_call_psci_idx", &pm_info->cp_call_psci_idx);
		if (ret) {
			pr_err("%s %s: unabled to get cp_call_psci_idx from DT\n",
					EXYNOS_PM_PREFIX, __func__);
			BUG();
		}

	} else {
		pr_err("%s %s: failed to have populated device tree\n",
					EXYNOS_PM_PREFIX, __func__);
		BUG();
	}

	if (subsys_system_register(&exynos_info_subsys,
					exynos_info_sysfs_groups))
		pr_err("%s %s: fail to register exynos_info subsys\n",
					EXYNOS_PM_PREFIX, __func__);

	suspend_set_ops(&exynos_pm_ops);
	register_syscore_ops(&exynos_pm_syscore_ops);
#ifdef CONFIG_DEBUG_FS
	exynos_pm_debugfs_init();
#endif

	ret = sysfs_create_file(power_kobj, &dev_attr_asv_cpu_info.attr);
	if (ret) {
		pr_err("%s: failed to create exynos7570 asv cpu attribute file\n", __func__);
	}

	ret = sysfs_create_file(power_kobj, &dev_attr_asv_gpu_info.attr);
	if (ret) {
		pr_err("%s: failed to create exynos7570 asv gpu attribute file\n", __func__);
	}

	return 0;
}
arch_initcall(exynos_pm_drvinit);
