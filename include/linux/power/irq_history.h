/*
 * Copyright (C) 2016 SAMSUNG, Inc.
 * Hunsup Jung <hunsup.jung@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _IRQ_HISTORY_H
#define _IRQ_HISTORY_H

enum IRQ_STATUS {
	IRQ_NOT_ACTIVE = 0,
	IRQ_ACTIVE = 1,
};

enum IRQ_HISTORY_FLAG_TABLE {
	IRQ_HISTORY_S2MPW01_IRQ = BIT(0),
	IRQ_HISTORY_BCMSDH_SDMMC = BIT(1),
	IRQ_HISTORY_LIST_MAX = BIT(32),
};

/* Length of valid character is 15 */
#define IRQ_NAME_LENGTH 16

extern void add_irq_history(int irq, const char *name);
extern unsigned int get_irq_history_flag(void);
extern void clear_irq_history_flag(void);

#endif /* _IRQ_HISTORY_H */
