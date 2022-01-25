/**
 * @file ksyms/ksyms.c
 * @author Alexander Aksenov <a.aksenov@samsung.com>
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2014
 *
 * @section DESCRIPTION
 *
 * SWAP ksyms module.
 */


#include "ksyms.h"
#include "ksyms_init.h"
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/percpu.h>

/**
 * @struct symbol_data
 * @brief Stores symbols data.
 * @var symbol_data::name
 * Pointer to symbol name string.
 * @var symbol_data::len
 * Symbol name length.
 * @var symbol_data::addr
 * Symbol address.
 */
struct symbol_data {
	const char *name;
	size_t len;
	unsigned long addr;
};

static int symbol_cb(void *data, const char *sym, struct module *mod,
		     unsigned long addr)
{
	struct symbol_data *sym_data_p = (struct symbol_data *)data;

	/* We expect that real symbol name should have at least the same
	 * length as symbol name we are looking for. */
	if (strncmp(sym_data_p->name, sym, sym_data_p->len) == 0) {
		sym_data_p->addr = addr;
		/* Return != 0 to stop loop over the symbols */
		return 1;
	}

	return 0;
}

/**
 * @brief Search of symbol address based on substring.
 *
 * @param name Pointer to the substring.
 * @return Symbol address.
 */
unsigned long swap_ksyms_substr(const char *name)
{
	struct symbol_data sym_data = {
		.name = name,
		.len = strlen(name),
		.addr = 0
	};
	kallsyms_on_each_symbol(symbol_cb, (void *)&sym_data);

	return sym_data.addr;
}
EXPORT_SYMBOL_GPL(swap_ksyms_substr);
