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


#include <kprobe/swap_kprobes.h>
#include "kp_module.h"


static struct kretprobe *krp_create(char *name,
				    int (*eh)(struct kretprobe_instance *, struct pt_regs *),
				    int (*rh)(struct kretprobe_instance *, struct pt_regs *),
				    size_t data_size)
{
	struct kretprobe *rp;

	rp = kzalloc(sizeof(*rp), GFP_KERNEL);
	if (rp) {
		rp->kp.symbol_name = name;
		rp->entry_handler = eh;
		rp->handler = rh;
		rp->data_size = data_size;
	}

	return rp;
}

static void krp_free(struct kretprobe *rp)
{
	memset(rp, 0x10, sizeof(*rp));
}

#define krp_reg(ptr, name, eh, rh, sz) \
	do { \
		ptr = krp_create(name, eh, rh, sz); \
		swap_register_kretprobe(ptr); \
	} while (0)

#define krp_unreg(ptr) \
	do { \
		swap_unregister_kretprobe(ptr); \
		krp_free(ptr); \
		ptr = NULL; \
	} while (0)





struct test_func_data {
	long v0, v1, v2, v3, v4, v5, v6, v7;
};

static struct test_func_data tf_data_tmp;
static unsigned long tf_data_tmp_ret;


static long do_test_func(long v0, long v1, long v2, long v3,
			 long v4, long v5, long v6, long v7)
{
	return v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7;
}

static noinline long test_func(long v0, long v1, long v2, long v3,
			       long v4, long v5, long v6, long v7)
{
	return do_test_func(v0, v1, v2, v3, v4, v5, v6, v7);
}

static int test_func_eh(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct test_func_data *data = (struct test_func_data *)ri->data;

	data->v0 = swap_get_karg(regs, 0);
	data->v1 = swap_get_karg(regs, 1);
	data->v2 = swap_get_karg(regs, 2);
	data->v3 = swap_get_karg(regs, 3);
	data->v4 = swap_get_karg(regs, 4);
	data->v5 = swap_get_karg(regs, 5);
	data->v6 = swap_get_karg(regs, 6);
	data->v7 = swap_get_karg(regs, 7);

	pr_info("E data=[%ld %ld %ld %ld %ld %ld %ld %ld]\n",
		data->v0, data->v1, data->v2, data->v3,
		data->v4, data->v5, data->v6, data->v7);

	return 0;
}

static int test_func_rh(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct test_func_data *data = (struct test_func_data *)ri->data;

	tf_data_tmp_ret = regs_return_value(regs);
	tf_data_tmp = *data;

	pr_info("R data=[%ld %ld %ld %ld %ld %ld %ld %ld] ret=%ld\n",
		data->v0, data->v1, data->v2, data->v3,
		data->v4, data->v5, data->v6, data->v7,
		tf_data_tmp_ret);

	return 0;
}

struct kretprobe *test_func_krp;

static struct test_func_data tf_data_gage = {
	.v0 = 0,
	.v1 = 1,
	.v2 = 2,
	.v3 = 3,
	.v4 = 4,
	.v5 = 5,
	.v6 = 6,
	.v7 = 7,
};

static void pre_test_krp(void)
{
	memset(&tf_data_tmp, 0, sizeof(tf_data_tmp));
	tf_data_tmp_ret = 0;
}

static void post_test_krp(void)
{
	long ret;

	ret = do_test_func(tf_data_gage.v0, tf_data_gage.v1, tf_data_gage.v2, tf_data_gage.v3,
			   tf_data_gage.v4, tf_data_gage.v5, tf_data_gage.v6, tf_data_gage.v7);

	if (tf_data_tmp_ret == ret &&
	    memcmp(&tf_data_gage, &tf_data_tmp, sizeof(tf_data_gage)) == 0) {
		olog("    OK\n");
	} else {
		olog("    ERROR:\n"
		     "        tf_data_gage=[%ld %ld %ld %ld %ld %ld %ld %ld] ret=%ld\n"
		     "        tf_data_tmp =[%ld %ld %ld %ld %ld %ld %ld %ld] ret=%ld\n",
		     tf_data_gage.v0, tf_data_gage.v1,
		     tf_data_gage.v2, tf_data_gage.v3,
		     tf_data_gage.v4, tf_data_gage.v5,
		     tf_data_gage.v6, tf_data_gage.v7, ret,
		     tf_data_tmp.v0, tf_data_tmp.v1,
		     tf_data_tmp.v2, tf_data_tmp.v3,
		     tf_data_tmp.v4, tf_data_tmp.v5,
		     tf_data_tmp.v6, tf_data_tmp.v7, tf_data_tmp_ret);
	}
}

static void do_test_krp(void)
{
	krp_reg(test_func_krp, "test_func",
		test_func_eh, test_func_rh,
		sizeof(struct test_func_data));

	test_func(tf_data_gage.v0, tf_data_gage.v1, tf_data_gage.v2, tf_data_gage.v3,
		  tf_data_gage.v4, tf_data_gage.v5, tf_data_gage.v6, tf_data_gage.v7);

	krp_unreg(test_func_krp);
}

static void test_krp(void)
{
	olog("Kretprobe (get_args{8} and ri->data):\n");

	pre_test_krp();

	do_test_krp();

	post_test_krp();
}

int krp_tests_run(void)
{
	test_krp();

	return 0;
}
