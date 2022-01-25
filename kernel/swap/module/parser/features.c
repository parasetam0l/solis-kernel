/**
 * parser/features.c
 * @author Vyacheslav Cherkashin
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * @section DESCRIPTION
 *
 * Features control implementation.
 */


#include <linux/types.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <ks_features/ks_features.h>
#include <us_manager/us_manager.h>
#include "parser_defs.h"
#include "features.h"
#include "msg_parser.h"

#include <writer/swap_msg.h>
#include <writer/event_filter.h>
#include <sampler/swap_sampler_module.h>
#include <energy/energy.h>

/**
 * @enum features_list
 * List of supported features.
 */
enum features_list {
	syscall_file	= (1 << 10),	/**< File operation syscalls tracing */
	syscall_ipc	= (1 << 11),	/**< IPC syscall tracing */
	syscall_process	= (1 << 12),	/**< Process syscalls tracing */
	syscall_signal	= (1 << 13),	/**< Signal syscalls tracing */
	syscall_network	= (1 << 14),	/**< Network syscalls tracing */
	syscall_desc	= (1 << 15),	/**< Descriptor syscalls tracing */
	context_switch	= (1 << 16),	/**< Context switch tracing */
	func_sampling	= (1 << 19)	/**< Function sampling */
};

/**
 * @brief Start user-space instrumentation event handling.
 *
 * @param conf Pointer to conf_data config.
 * @return 0.
 */
int set_us_inst(struct conf_data *conf)
{
	set_quiet(QT_OFF);

	return 0;
}

/**
 * @brief Stop user-space instrumentation event handling.
 *
 * @return 0.
 */
int unset_us_inst(void)
{
	set_quiet(QT_ON);

	return 0;
}

/**
 * @brief Set syscall file feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_syscall_file(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_FILE);

	return ret;
}

/**
 * @brief Set syscall file feature off.
 *
 * @return unset_feature results.
 */
int unset_syscall_file(void)
{
	int ret;

	ret = unset_feature(FID_FILE);

	return ret;
}

/**
 * @brief Set syscall ipc feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_syscall_ipc(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_IPC);

	return ret;
}

/**
 * @brief Set syscall ipc feature off.
 *
 * @return unset_feature results.
 */
int unset_syscall_ipc(void)
{
	int ret;

	ret = unset_feature(FID_IPC);

	return ret;
}

/**
 * @brief Set syscall process feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_syscall_process(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_PROCESS);

	return ret;
}

/**
 * @brief Set syscall process feature off.
 *
 * @return unset_feature results.
 */
int unset_syscall_process(void)
{
	int ret;

	ret = unset_feature(FID_PROCESS);

	return ret;
}

/**
 * @brief Set syscall signal feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_syscall_signal(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_SIGNAL);

	return ret;
}

/**
 * @brief Set syscall signal feature off.
 *
 * @return unset_feature results.
 */
int unset_syscall_signal(void)
{
	int ret;

	ret = unset_feature(FID_SIGNAL);

	return ret;
}

/**
 * @brief Set syscall network feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_syscall_network(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_NET);

	return ret;
}

/**
 * @brief Set syscall network feature off.
 *
 * @return unset_feature results.
 */
int unset_syscall_network(void)
{
	int ret;

	ret = unset_feature(FID_NET);

	return ret;
}

/**
 * @brief Set syscall desc feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_syscall_desc(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_DESC);

	return ret;
}

/**
 * @brief Set syscall desc feature off.
 *
 * @return unset_feature results.
 */
int unset_syscall_desc(void)
{
	int ret;

	ret = unset_feature(FID_DESC);

	return ret;
}

/**
 * @brief Set context switch feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_context_switch(struct conf_data *conf)
{
	int ret;

	ret = set_feature(FID_SWITCH);

	return ret;
}
/**
 * @brief Set context switch feature off.
 *
 * @return unset_feature results.
 */
int unset_context_switch(void)
{
	int ret;

	ret = unset_feature(FID_SWITCH);

	return ret;
}





struct sample {
	u32 pid;
	u64 pc_addr;
	u32 tid;
	u32 cpu_num;
} __packed;

static void sample_msg(struct pt_regs *regs)
{
	struct swap_msg *m;
	struct sample *s;
	struct task_struct *task = current;

	m = swap_msg_get(MSG_SAMPLE);

	s = swap_msg_payload(m);
	s->pid = task->tgid;
	s->pc_addr = instruction_pointer(regs);
	s->tid = task->pid;
	s->cpu_num = raw_smp_processor_id();

	swap_msg_flush(m, sizeof(*s));
	swap_msg_put(m);
}

static void sampler_cb(struct pt_regs *regs)
{
	if (check_event(current))
		sample_msg(regs);
}

/**
 * @brief Set sampling feature on.
 *
 * @param conf Pointer to conf_data config.
 * @return set_feature results.
 */
int set_func_sampling(struct conf_data *conf)
{
	int ret;

	ret = swap_sampler_start(conf->data_msg_period, sampler_cb);

	return ret;
}

/**
 * @brief Set sampling feature off.
 *
 * @return unset_feature results.
 */
int unset_func_sampling(void)
{
	int ret;

	ret = swap_sampler_stop();

	return ret;
}

static int set_func_energy(struct conf_data *conf)
{
	return set_energy();
}

static int unset_func_energy(void)
{
	return unset_energy();
}

static int set_sysfile_activity(struct conf_data *conf)
{
	return set_feature(FID_SYSFILE_ACTIVITY);
}

static int unset_sysfile_activity(void)
{
	return unset_feature(FID_SYSFILE_ACTIVITY);
}

/**
 * @struct feature_item
 * @brief Struct that describes feature.
 * @var feature_item::name
 * Feature name.
 * @var feature_item::set
 * Set feature callback.
 * @var feature_item::unset
 * Unset feature callback.
 */
struct feature_item {
	char *name;
	int (*set)(struct conf_data *conf);
	int (*unset)(void);
};

static struct feature_item feature_us_inst = {
	.name = "user space instrumentation",
	.set = set_us_inst,
	.unset = unset_us_inst
};

static struct feature_item feature_syscall_file = {
	.name = "file operation syscalls",
	.set = set_syscall_file,
	.unset = unset_syscall_file
};

static struct feature_item feature_syscall_ipc = {
	.name = "IPC syscall",
	.set = set_syscall_ipc,
	.unset = unset_syscall_ipc
};

static struct feature_item feature_syscall_process = {
	.name = "process syscalls",
	.set = set_syscall_process,
	.unset = unset_syscall_process
};

static struct feature_item feature_syscall_signal = {
	.name = "signal syscalls",
	.set = set_syscall_signal,
	.unset = unset_syscall_signal
};

static struct feature_item feature_syscall_network = {
	.name = "network syscalls",
	.set = set_syscall_network,
	.unset = unset_syscall_network
};

static struct feature_item feature_syscall_desc = {
	.name = "descriptor syscalls",
	.set = set_syscall_desc,
	.unset = unset_syscall_desc
};

static struct feature_item feature_context_switch = {
	.name = "context switch",
	.set = set_context_switch,
	.unset = unset_context_switch
};

static struct feature_item feature_func_sampling = {
	.name = "function sampling",
	.set = set_func_sampling,
	.unset = unset_func_sampling
};

static struct feature_item feature_func_energy = {
	.name = "energy",
	.set = set_func_energy,
	.unset = unset_func_energy
};

static struct feature_item feature_sysfile_activity = {
	.name = "system file activity",
	.set = set_sysfile_activity,
	.unset = unset_sysfile_activity
};

static struct feature_item *feature_list[] = {
 /*  0 */	NULL,
 /*  1 */	NULL,
 /*  2 */	&feature_us_inst,
 /*  3 */	NULL,
 /*  4 */	NULL,
 /*  5 */	NULL,
 /*  6 */	NULL,
 /*  7 */	NULL,
 /*  8 */	NULL,
 /*  9 */	NULL,
 /* 10 */	&feature_syscall_file,
 /* 11 */	&feature_syscall_ipc,
 /* 12 */	&feature_syscall_process,
 /* 13 */	&feature_syscall_signal,
 /* 14 */	&feature_syscall_network,
 /* 15 */	&feature_syscall_desc,
 /* 16 */	&feature_context_switch,
 /* 17 */	NULL,
 /* 18 */	NULL,
 /* 19 */	&feature_func_sampling,
 /* 20 */	NULL,
 /* 21 */	NULL,
 /* 22 */	NULL,
 /* 23 */	NULL,
 /* 24 */	NULL,
 /* 25 */	NULL,
 /* 26 */	&feature_func_energy,
 /* 27 */	NULL,
 /* 28 */	NULL,
 /* 29 */	NULL,
 /* 30 */	NULL,
 /* 31 */	NULL,
 /* 32 */	NULL,
 /* 33 */	NULL,
 /* 34 */	NULL,
 /* 35 */	NULL,
 /* 36 */	NULL,
 /* 37 */	NULL,
 /* 38 */	NULL,
 /* 39 */	NULL,
 /* 40 */	NULL,
 /* 41 */	NULL,
 /* 42 */	NULL,
 /* 43 */	NULL,
 /* 44 */	NULL,
 /* 45 */	NULL,
 /* 46 */	NULL,
 /* 47 */	NULL,
 /* 48 */	&feature_sysfile_activity,
};

/**
 * @brief SIZE_FEATURE_LIST definition.
 */
enum {
	SIZE_FEATURE_LIST =
	sizeof(feature_list) / sizeof(struct feature_item *),
};

static u64 feature_inst;
static u64 feature_mask;

/**
 * @brief Inits features list.
 *
 * @return 0.
 */
int once_features(void)
{
	int i;
	for (i = 0; i < SIZE_FEATURE_LIST; ++i) {
		printk(KERN_INFO "### f init_feature_mask[%2d]=%p\n", i,
		       feature_list[i]);
		if (feature_list[i] != NULL) {
			feature_mask |= ((u64)1) << i;
			printk(KERN_INFO "### f name=%s\n",
			       feature_list[i]->name);
		}
	}

	return 0;
}

static int do_set_features(struct conf_data *conf)
{
	int i, ret;
	u64 feature_XOR;
	u64 features, features_backup;

	/* TODO: field use_features1 is not used*/
	features_backup = features = conf->use_features0;

	features &= feature_mask;
	feature_XOR = features ^ feature_inst;

	for (i = 0; feature_XOR && i < SIZE_FEATURE_LIST; ++i) {
		if ((feature_XOR & 1) && feature_list[i] != NULL) {
			u64 f_mask;
			if (features & 1)
				ret = feature_list[i]->set(conf);
			else
				ret = feature_list[i]->unset();

			if (ret) {
				char *func = features & 1 ? "set" : "unset";
				print_err("%s '%s' ret=%d\n",
					  func, feature_list[i]->name, ret);

				return ret;
			}
			f_mask = ~((u64)1 << i);
			feature_inst = (feature_inst & f_mask) |
				       (features_backup & ~f_mask);
		}

		features >>= 1;
		feature_XOR >>= 1;
	}

	return 0;
}

static DEFINE_MUTEX(mutex_features);

/**
 * @brief Wrapper for do_set_features with locking.
 *
 * @param conf Pointer to the current config.
 * @return do_set_features result.
 */
int set_features(struct conf_data *conf)
{
	int ret;

	mutex_lock(&mutex_features);
	ret = do_set_features(conf);
	mutex_unlock(&mutex_features);

	return ret;
}

void disable_all_features(void)
{
	struct conf_data conf;

	conf.use_features0 = 0;
	conf.use_features1 = 0;
	conf.data_msg_period = 0;
	conf.sys_trace_period = 0;

	BUG_ON(set_features(&conf));
	BUG_ON(feature_inst);
}
