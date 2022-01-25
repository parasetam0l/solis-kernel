/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  energy/lcd/lcd_base.c
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


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <energy/tm_stat.h>
#include <energy/debugfs_energy.h>
#include "lcd_base.h"
#include "lcd_debugfs.h"


/**
 * @brief Read the number of file
 *
 * @param path of the file
 * @return Value or error(when negative)
 */
int read_val(const char *path)
{
	int ret;
	struct file *f;
	unsigned long val;
	enum { buf_len = 32 };
	char buf[buf_len];

	f = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(f)) {
		printk(KERN_INFO "cannot open file \'%s\'", path);
		return PTR_ERR(f);
	}

	ret = kernel_read(f, 0, buf, sizeof(buf));
	filp_close(f, NULL);
	if (ret < 0)
		return ret;

	buf[ret >= buf_len ? buf_len - 1 : ret] = '\0';

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	return (int)val;
}

enum {
	brt_no_init = -1,
	brt_cnt = 10
};

enum power_t {
	PW_ON,
	PW_OFF
};

struct lcd_priv_data {
	int min_brt;
	int max_brt;

	size_t tms_brt_cnt;
	struct tm_stat *tms_brt;
	spinlock_t lock_tms;
	int brt_old;
	enum power_t power;

	u64 min_denom;
	u64 min_num;
	u64 max_denom;
	u64 max_num;
};

static void *create_lcd_priv(struct lcd_ops *ops, size_t tms_brt_cnt)
{
	int i;
	struct lcd_priv_data *lcd;

	if (tms_brt_cnt <= 0) {
		printk(KERN_INFO "error variable tms_brt_cnt=%zu\n",
		       tms_brt_cnt);
		return NULL;
	}

	lcd = kmalloc(sizeof(*lcd) + sizeof(*lcd->tms_brt) * tms_brt_cnt,
		      GFP_KERNEL);
	if (lcd == NULL) {
		printk(KERN_INFO "error: %s - out of memory\n", __func__);
		return NULL;
	}

	lcd->tms_brt = (void *)lcd + sizeof(*lcd);
	lcd->tms_brt_cnt = tms_brt_cnt;

	lcd->min_brt = ops->get(ops, LPD_MIN_BRIGHTNESS);
	lcd->max_brt = ops->get(ops, LPD_MAX_BRIGHTNESS);

	for (i = 0; i < tms_brt_cnt; ++i)
		tm_stat_init(&lcd->tms_brt[i]);

	spin_lock_init(&lcd->lock_tms);

	lcd->brt_old = brt_no_init;
	lcd->power = PW_OFF;

	lcd->min_denom = 1;
	lcd->min_num = 1;
	lcd->max_denom = 1;
	lcd->max_num = 1;

	return (void *)lcd;
}

static void destroy_lcd_priv(void *data)
{
	kfree(data);
}

static struct lcd_priv_data *get_lcd_priv(struct lcd_ops *ops)
{
	return (struct lcd_priv_data *)ops->priv;
}

static void clean_brightness(struct lcd_ops *ops)
{
	struct lcd_priv_data *lcd = get_lcd_priv(ops);
	int i;

	spin_lock(&lcd->lock_tms);
	for (i = 0; i < lcd->tms_brt_cnt; ++i)
		tm_stat_init(&lcd->tms_brt[i]);

	lcd->brt_old = brt_no_init;
	spin_unlock(&lcd->lock_tms);
}

static int get_brt_num_of_array(struct lcd_priv_data *lcd, int brt)
{
	if (brt > lcd->max_brt || brt < lcd->min_brt) {
		printk(KERN_INFO "LCD energy error: set brightness=%d, "
		       "when brightness[%d..%d]\n",
		       brt, lcd->min_brt, lcd->max_brt);
		brt = brt > lcd->max_brt ? lcd->max_brt : lcd->min_brt;
	}

	return lcd->tms_brt_cnt * (brt - lcd->min_brt) /
	       (lcd->max_brt - lcd->min_brt + 1);
}

static void set_brightness(struct lcd_ops *ops, int brt)
{
	struct lcd_priv_data *lcd = get_lcd_priv(ops);
	int n = get_brt_num_of_array(lcd, brt);

	spin_lock(&lcd->lock_tms);

	if (lcd->power == PW_ON && lcd->brt_old != n) {
		u64 time = get_ntime();
		if (lcd->brt_old != brt_no_init)
			tm_stat_update(&lcd->tms_brt[lcd->brt_old], time);

		tm_stat_set_timestamp(&lcd->tms_brt[n], time);
	}
	lcd->brt_old = n;

	spin_unlock(&lcd->lock_tms);
}

static void set_power_on_set_brt(struct lcd_priv_data *lcd)
{
	if (lcd->brt_old != brt_no_init) {
		u64 time = get_ntime();
		tm_stat_set_timestamp(&lcd->tms_brt[lcd->brt_old], time);
	}
}

static void set_power_on(struct lcd_priv_data *lcd)
{
	if (lcd->power == PW_OFF)
		set_power_on_set_brt(lcd);

	lcd->power = PW_ON;
}

static void set_power_off_update_brt(struct lcd_priv_data *lcd)
{
	if (lcd->brt_old != brt_no_init) {
		u64 time = get_ntime();
		tm_stat_update(&lcd->tms_brt[lcd->brt_old], time);
		lcd->brt_old = brt_no_init;
	}
}

static void set_power_off(struct lcd_priv_data *lcd)
{
	if (lcd->power == PW_ON)
		set_power_off_update_brt(lcd);

	lcd->power = PW_OFF;
}

static void set_power(struct lcd_ops *ops, int val)
{
	struct lcd_priv_data *lcd = get_lcd_priv(ops);

	spin_lock(&lcd->lock_tms);

	switch (val) {
	case FB_BLANK_UNBLANK:
		set_power_on(lcd);
		break;
	case FB_BLANK_POWERDOWN:
		set_power_off(lcd);
		break;
	default:
		printk(KERN_INFO "LCD energy error: set power=%d\n", val);
		break;
	}

	spin_unlock(&lcd->lock_tms);
}

static int func_notifier_lcd(struct lcd_ops *ops, enum lcd_action_type action,
			     void *data)
{
	switch (action) {
	case LAT_BRIGHTNESS:
		set_brightness(ops, VOIDP2INT(data));
		break;
	case LAT_POWER:
		set_power(ops, VOIDP2INT(data));
		break;
	default:
		printk(KERN_INFO "LCD energy error: action=%d\n", action);
		return -EINVAL;
	}

	return 0;
}

/**
 * @brief Get the array size of LCD
 *
 * @param ops LCD operations
 * @return Array size
 */
size_t get_lcd_size_array(struct lcd_ops *ops)
{
	struct lcd_priv_data *lcd = get_lcd_priv(ops);

	return lcd->tms_brt_cnt;
}

/**
 * @brief Get an array of times
 *
 * @param ops LCD operations
 * @param array_time[out] Array of times
 * @return Void
 */
void get_lcd_array_time(struct lcd_ops *ops, u64 *array_time)
{
	struct lcd_priv_data *lcd = get_lcd_priv(ops);
	int i;

	spin_lock(&lcd->lock_tms);
	for (i = 0; i < lcd->tms_brt_cnt; ++i)
		array_time[i] = tm_stat_running(&lcd->tms_brt[i]);

	if (lcd->power == PW_ON && lcd->brt_old != brt_no_init) {
		int old = lcd->brt_old;
		struct tm_stat *tm = &lcd->tms_brt[old];

		array_time[old] += get_ntime() - tm_stat_timestamp(tm);
	}
	spin_unlock(&lcd->lock_tms);
}

static int register_lcd(struct lcd_ops *ops)
{
	int ret = 0;

	ops->priv = create_lcd_priv(ops, brt_cnt);

	/* TODO: create init_func() for 'struct rational' */
	ops->min_coef.num = 1;
	ops->min_coef.denom = 1;
	ops->max_coef.num = 1;
	ops->max_coef.denom = 1;

	ops->notifier = func_notifier_lcd;

	ret = register_lcd_debugfs(ops);
	if (ret)
		destroy_lcd_priv(ops->priv);

	return ret;
}

static void unregister_lcd(struct lcd_ops *ops)
{
	unregister_lcd_debugfs(ops);
	destroy_lcd_priv(ops->priv);
}




/* ============================================================================
 * ===                          LCD_INIT/LCD_EXIT                           ===
 * ============================================================================
 */
typedef struct lcd_ops *(*get_ops_t)(void);

DEFINITION_LCD_FUNC;

get_ops_t lcd_ops[] = DEFINITION_LCD_ARRAY;
enum { lcd_ops_cnt = sizeof(lcd_ops) / sizeof(get_ops_t) };

enum ST_LCD_OPS {
	SLO_REGISTER	= 1 << 0,
	SLO_SET		= 1 << 1
};

static DEFINE_MUTEX(lcd_lock);
static enum ST_LCD_OPS stat_lcd_ops[lcd_ops_cnt];

static void do_lcd_exit(void)
{
	int i;
	struct lcd_ops *ops;

	mutex_lock(&lcd_lock);
	for (i = 0; i < lcd_ops_cnt; ++i) {
		ops = lcd_ops[i]();

		if (stat_lcd_ops[i] & SLO_SET) {
			ops->unset(ops);
			stat_lcd_ops[i] &= ~SLO_SET;
		}

		if (stat_lcd_ops[i] & SLO_REGISTER) {
			unregister_lcd(ops);
			stat_lcd_ops[i] &= ~SLO_REGISTER;
		}
	}
	mutex_unlock(&lcd_lock);
}

/**
 * @brief LCD deinitialization
 *
 * @return Void
 */
void lcd_exit(void)
{
	do_lcd_exit();
}

static int do_lcd_init(void)
{
	int i, ret, count = 0;
	struct lcd_ops *ops;

	mutex_lock(&lcd_lock);
	for (i = 0; i < lcd_ops_cnt; ++i) {
		ops = lcd_ops[i]();
		if (ops == NULL) {
			printk(KERN_INFO "error %s [ops == NULL]\n", __func__);
			continue;
		}

		if (0 == ops->check(ops)) {
			printk(KERN_INFO "error checking %s\n", ops->name);
			continue;
		}

		ret = register_lcd(ops);
		if (ret) {
			printk(KERN_INFO "error register_lcd %s\n", ops->name);
			continue;
		}

		stat_lcd_ops[i] |= SLO_REGISTER;
		++count;
	}
	mutex_unlock(&lcd_lock);

	return count ? 0 : -EPERM;
}

/**
 * @brief LCD initialization
 *
 * @return Error code
 */
int lcd_init(void)
{
	int ret;

	ret = do_lcd_init();
	if (ret)
		printk(KERN_INFO "LCD is not supported\n");

	return ret;
}



/* ============================================================================
 * ===                     LCD_SET_ENERGY/LCD_UNSET_ENERGY                  ===
 * ============================================================================
 */

/**
 * @brief Start measuring the energy consumption of LСD
 *
 * @return Error code
 */
int lcd_set_energy(void)
{
	int i, ret, count = 0;
	struct lcd_ops *ops;

	mutex_lock(&lcd_lock);
	for (i = 0; i < lcd_ops_cnt; ++i) {
		ops = lcd_ops[i]();
		if (stat_lcd_ops[i] & SLO_REGISTER) {
			ret = ops->set(ops);
			if (ret) {
				printk(KERN_INFO "error %s set LCD energy",
				       ops->name);
				continue;
			}

			set_brightness(ops, ops->get(ops, LPD_BRIGHTNESS));
			set_power(ops, ops->get(ops, LPD_POWER));

			stat_lcd_ops[i] |= SLO_SET;
			++count;
		}
	}
	mutex_unlock(&lcd_lock);

	return count ? 0 : -EPERM;
}

/**
 * @brief Stop measuring the energy consumption of LСD
 *
 * @return Void
 */
void lcd_unset_energy(void)
{
	int i, ret;
	struct lcd_ops *ops;

	mutex_lock(&lcd_lock);
	for (i = 0; i < lcd_ops_cnt; ++i) {
		ops = lcd_ops[i]();
		if (stat_lcd_ops[i] & SLO_SET) {
			ret = ops->unset(ops);
			if (ret)
				printk(KERN_INFO "error %s unset LCD energy",
				       ops->name);

			clean_brightness(ops);
			stat_lcd_ops[i] &= ~SLO_SET;
		}
	}
	mutex_unlock(&lcd_lock);
}
