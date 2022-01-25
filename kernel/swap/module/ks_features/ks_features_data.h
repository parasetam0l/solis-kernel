/**
 * ks_features/ks_features_data.h
 * @author Vitaliy Cherepanov: SWAP ks_features implement
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
 * Copyright (C) Samsung Electronics, 2016
 *
 * @section DESCRIPTION
 *
 *  SWAP kernel features
 */

/**
 * @struct ks_probe
 * @brief Kernel-space probe. Struct used as a container of syscall probes.
 * @var ks_probe::rp
 * Pointer to kretprobe.
 * @var ks_probe::counter
 * Installed probes counter.
 * @var ks_probe::args
 * Pointer to args format string.
 * @var ks_probe::type
 * Probe sub type.
 */
#ifndef __KS_FEATURE_DATA_H__
#define __KS_FEATURE_DATA_H__

#include "syscall_list.h"

#ifdef CONFIG_SWAP_HOOK_SYSCALL
# include <swap/hook_syscall.h>
#else
# include "kprobe/swap_kprobes.h"
#endif

struct ks_probe {
#ifdef CONFIG_SWAP_HOOK_SYSCALL
	struct hook_syscall hook;
	u64 sys_addr;
#else /* CONFIG_SWAP_HOOK_SYSCALL */
	struct kretprobe rp;
#endif /* CONFIG_SWAP_HOOK_SYSCALL */

	int counter;
	char *args;
	int type;

	const char *name;
	unsigned int id;
};

/**
 * @enum
 * Syscall name count defenition
 */
#define X(name__, args__) + 1
enum {
	syscall_cnt = 0 SYSCALL_LIST
};
#undef X

extern struct ks_probe ksp[syscall_cnt];

const char *get_sys_name(size_t id);
int get_counter(size_t id);
void inc_counter(size_t id);
void dec_counter(size_t id);

#endif /* __KS_FEATURE_DATA_H__ */
