/*
 * include/linux/knox_gearlock.h
 *
 * Include file for the KNOX protection for GEAR Samung Pay.
 */

#ifndef _LINUX_KNOX_GEARLOCK_H
#define _LINUX_KNOX_GEARLOCK_H

#include <linux/types.h>

extern const struct file_operations knox_gearlock_fops;
extern int g_lock_layout_active;




typedef enum gearpay_bool_s {
	TOUCH = (u32)0,
	PHYSBUTTON,
	STRAP
} gearpay_bool_t;

#define	GEARPAY_SMC_ID	0x83000016
u32 exynos_smc_gearpay(gearpay_bool_t flag_id, u32 set);
#endif /* _LINUX_KNOX_GEARLOCK_H */

