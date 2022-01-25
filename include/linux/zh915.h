#ifndef __ZH915_H__
#define __ZH915_H__
/*
** =============================================================================
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     zh915.h
**
** Description:
**     Header file for zh915.c
**
** =============================================================================
*/

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>


#define HAPTICS_DEVICE_NAME "zh915"

#define	ZH915_REG_ID				0x2A
#define ZH915_ID					0x00

#define	ZH915_REG_MODE				0x00
#define	ZH915_MODE_MASK				0x07
#define	MODE_PWM					0x04
#define	MODE_I2C					0x05
#define	MODE_STOP					0x00

#define ZH915_REG_STRENGTH_WRITE	0x01

#define ZH915_REG_CONTROL			0x03
#define ZH915_REG_CONTROL_MASK		0x07
#define LOOP_SHIFT					0x02
#define BREAK_SHIFT					0x01
#define MOTOR_TYPE_SHIFT			0x00

#define ZH915_REG_STRENGTH_READ		0x0c

#define ZH915_REG_DS_RATIO			0x2B
#define ZH915_DS_RATIO_MASK			0x3f

#define ZH915_REG_RESONANCE_FREQ	0x2e

#define MAX_LEVEL 0xffff
#define MAX_RTP_INPUT 120
#define POWER_SUPPLY_VOLTAGE 3000000

enum VIBRATOR_CONTROL {
	VIBRATOR_DISABLE = 0,
	VIBRATOR_ENABLE = 1,
};

enum actuator_type {
	LRA,
	ERM
};

enum loop_type {
	CLOSE_LOOP,
	OPEN_LOOP
};

struct motor_data {
	char *motor_name;
	enum actuator_type motor_type;
	int resonance_freq;
};

struct reg_data {
	u32 addr;
	u32 data;
};

struct zh915_platform_data {
	struct motor_data mdata;
	struct reg_data *init_regs;
	int count_init_regs;
	enum loop_type	meLoop;
	bool break_mode;
	int gpio_en;
	const char *regulator_name;
};

struct zh915_data {
	struct zh915_platform_data msPlatData;
	unsigned char mnDeviceID;
	struct device *dev;
	struct regmap *mpRegmap;

    struct wake_lock wklock;
    struct mutex lock;
    struct work_struct vibrator_work;
	struct work_struct delay_en_off;
	struct timespec last_motor_off;

	/* using FF_input_device */
	__u16 level;
	bool running;
	struct regulator *regulator;
	int gpio_en;

	struct notifier_block zh915_pm_nb;
};

struct motor_data init_mdata[] = {
	{
	.motor_name = "NIDEC",
	.motor_type = LRA,
	.resonance_freq = 188,
	},
	{
	.motor_name = "NIDEC-2",
	.motor_type = LRA,
	.resonance_freq = 186,
	},
};

#endif
