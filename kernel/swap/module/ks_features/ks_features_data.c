/**
 * @author Vitaliy Cherepanov: SWAP ks_features_data implement
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

#include "syscall_list.h"
#include "ks_features_data.h"
#include "ksf_msg.h"


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

#define CREATE_RP(name)						\
{								\
	.entry_handler = NULL,					\
	.handler = NULL						\
}

#define CREATE_HOOK_SYSCALL(name)				\
{								\
	.entry = NULL,					\
	.exit = NULL					\
}

#define SYSCALL_NAME_STR(name) #name

#ifdef CONFIG_SWAP_HOOK_SYSCALL

#include <asm/unistd32.h>	/* FIXME: for only arm64 compat mode */

#define X(name__, args__)					\
{								\
	.hook = CREATE_HOOK_SYSCALL(name__),			\
	.sys_addr = 0xdeadbeef,					\
	.counter = 0,						\
	.args = #args__,					\
	.type = PT_KS_NONE,					\
	.name = SYSCALL_NAME_STR(sys_ ## name__),		\
	.id = __NR_ ## name__,					\
},
#else /* !CONFIG_SWAP_HOOK_SYSCALL */
#define X(name__, args__)					\
{								\
	.rp = CREATE_RP(name__),				\
	.counter = 0,						\
	.args = #args__,					\
	.type = PT_KS_NONE,					\
	.name = SYSCALL_NAME_STR(sys_ ## name__),		\
},
#endif /* CONFIG_SWAP_HOOK_SYSCALL */

struct ks_probe ksp[syscall_cnt] = {
	SYSCALL_LIST
};

#undef X

const char *get_sys_name(size_t id)
{
	return ksp[id].name;
}

int get_counter(size_t id)
{
	return ksp[id].counter;
}

void inc_counter(size_t id)
{
	++ksp[id].counter;
}

void dec_counter(size_t id)
{
	--ksp[id].counter;
}

