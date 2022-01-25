/*
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
 * Copyright (C) Samsung Electronics, 2016
 *
 * 2016         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include "kp_module.h"


static asmlinkage long (*olog_sys_write)(unsigned int, const char __user *, size_t);

static long write_to_stdout(const char *buf, size_t len)
{
	long ret;

	mm_segment_t fs = get_fs();
	set_fs(get_ds());
	ret = olog_sys_write(1, buf, len);
	set_fs(fs);

	return ret;
}

static int olog_init(void)
{
	olog_sys_write = (void *)kallsyms_lookup_name("sys_write");
	if (olog_sys_write == NULL) {
		pr_err("ERR: not found 'sys_write' symbol\n");
		return -ESRCH;
	}

	return 0;
}

void olog(const char *fmt, ...)
{
	char buf[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	printk("%s", buf);
	write_to_stdout(buf, strlen(buf));
}




static void print_mod_info(void)
{
	struct module *mod = THIS_MODULE;

	printk("### MOD_INFO:\n");
	printk("    core: %p..%p\n", mod->module_init, mod->module_init + mod->init_text_size);
	printk("    init: %p..%p\n", mod->module_core, mod->module_core + mod->core_text_size);
	printk("\n");
}


/* TODO: move declare to header */
int kp_tests_run(void);
int krp_tests_run(void);

static int __init tests_init(void)
{
	int ret;

	ret = olog_init();
	if (ret)
		return ret;

	print_mod_info();

	olog("### Begin tests ###\n");
	kp_tests_run();
	krp_tests_run();
	olog("### End tests ###\n");

	return -1;
}

static void __exit tests_exit(void)
{
}

module_init(tests_init);
module_exit(tests_exit);

MODULE_LICENSE("GPL");
