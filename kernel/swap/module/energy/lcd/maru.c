/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  energy/lcd/maru.c
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <kprobe/swap_kprobes.h>
#include <linux/backlight.h>
#include "lcd_base.h"



static const char path_backlight[]	=
	"/sys/class/backlight/emulator/brightness";
static const char path_backlight_min[]	=
	"/sys/class/backlight/emulator/min_brightness";
static const char path_backlight_max[]	=
	"/sys/class/backlight/emulator/max_brightness";
static const char path_power[]		=
	"/sys/class/lcd/emulator/lcd_power";

static const char * const all_path[] = {
	path_backlight,
	path_backlight_min,
	path_backlight_max,
	path_power
};

enum {
	all_path_cnt = sizeof(all_path) / sizeof(char *)
};


static int maru_check(struct lcd_ops *ops)
{
	int i;

	for (i = 0; i < all_path_cnt; ++i) {
		int ret = read_val(all_path[i]);

		if (IS_ERR_VALUE(ret))
			return 0;
	}

	return 1;
}

static unsigned long maru_get_parameter(struct lcd_ops *ops,
					enum lcd_parameter_type type)
{
	switch (type) {
	case LPD_MIN_BRIGHTNESS:
		return read_val(path_backlight_min);
	case LPD_MAX_BRIGHTNESS:
		return read_val(path_backlight_max);
	case LPD_BRIGHTNESS:
		return read_val(path_backlight);
	case LPD_POWER:
		return read_val(path_power);
	default:
		return -EINVAL;
	}
}





static int entry_handler_set_backlight(struct kretprobe_instance *ri,
				       struct pt_regs *regs);
static int ret_handler_set_backlight(struct kretprobe_instance *ri,
				     struct pt_regs *regs);

static struct kretprobe set_backlight_krp = {
	.kp.symbol_name = "marubl_send_intensity",
	.entry_handler = entry_handler_set_backlight,
	.handler = ret_handler_set_backlight,
	.data_size = sizeof(int)
};





static int maru_set(struct lcd_ops *ops)
{
	return swap_register_kretprobe(&set_backlight_krp);
}

static int maru_unset(struct lcd_ops *ops)
{
	swap_unregister_kretprobe(&set_backlight_krp);
	return 0;
}

static struct lcd_ops maru_ops = {
	.name = "maru",
	.check = maru_check,
	.set = maru_set,
	.unset = maru_unset,
	.get = maru_get_parameter
};

struct lcd_ops *LCD_MAKE_FNAME(maru)(void)
{
	return &maru_ops;
}





static int entry_handler_set_backlight(struct kretprobe_instance *ri,
				       struct pt_regs *regs)
{
	int *brightness = (int *)ri->data;
	struct backlight_device *bd;

	bd = (struct backlight_device *)swap_get_karg(regs, 0);
	*brightness = bd->props.brightness;

	return 0;
}

static int ret_handler_set_backlight(struct kretprobe_instance *ri,
				     struct pt_regs *regs)
{
	int ret = regs_return_value(regs);
	int *brightness = (int *)ri->data;

	if (!ret && maru_ops.notifier)
		maru_ops.notifier(&maru_ops, LAT_BRIGHTNESS,
				  INT2VOIDP(*brightness));

	return 0;
}
