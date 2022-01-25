#ifndef _LCD_BASE_H
#define _LCD_BASE_H

/**
 * @file energy/lcd/lcd_base.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * @section COPYRIGHT
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 * Description of the interface for interacting with LСD
 */


#include <linux/errno.h>
#include <energy/rational_debugfs.h>


#define VOIDP2INT(x)	((int)(unsigned long)(x))
#define INT2VOIDP(x)	((void *)(unsigned long)(x))


/** Description of actions */
enum lcd_action_type {
	LAT_BRIGHTNESS,		/**< LCD brightness */
	LAT_POWER		/**< LCD power */
};


/** Description of parameters */
enum lcd_parameter_type {
	LPD_MIN_BRIGHTNESS,	/**< minimum brightness value */
	LPD_MAX_BRIGHTNESS,	/**< maximum brightness value */
	LPD_BRIGHTNESS,		/**< current brightness value */

	LPD_POWER		/**< current power value */
};

struct lcd_ops;

/**
 * @brief LCD callback type
 *
 * @param ops LCD operations
 * @return Error code
 */
typedef int (*call_lcd)(struct lcd_ops *ops);

/**
 * @brief LCD notifier type
 *
 * @param ops LCD operations
 * @param action Event type
 * @param data Date
 * @return Error code
 */
typedef int (*notifier_lcd)(struct lcd_ops *ops, enum lcd_action_type action,
			    void *data);

/**
 * @brief LCD parameter type
 *
 * @param ops LCD operations
 * @param type Requested parameter type
 * @return Requested parameter value
 *
 */
typedef unsigned long (*get_parameter_lcd)(struct lcd_ops *ops,
					   enum lcd_parameter_type type);


/**
 * @struct lcd_ops
 * @breaf set of operations available for LСD
 */
struct lcd_ops {
	char *name;			/**< LCD driver name */
	notifier_lcd notifier;		/**< Notifier */
	get_parameter_lcd get;		/**< Method to obtain the parameters */

	call_lcd check;			/**< LCD check on device */
	call_lcd set;			/**< LCD initialization */
	call_lcd unset;			/**< LCD deinitialization */

	/* for debugfs */
	struct dentry *dentry;		/**< Dentry of debugfs for this LCD */
	struct rational min_coef;	/**< Minimum coefficient */
	struct rational max_coef;	/**< Maximum coefficient */

	void *priv;			/**< Private data */
};

size_t get_lcd_size_array(struct lcd_ops *ops);
void get_lcd_array_time(struct lcd_ops *ops, u64 *array_time);

int read_val(const char *path);

int lcd_set_energy(void);
void lcd_unset_energy(void);

int lcd_init(void);
void lcd_exit(void);

#endif /* _LCD_BASE_H */
