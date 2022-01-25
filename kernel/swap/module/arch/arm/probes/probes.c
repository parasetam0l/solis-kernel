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


#include "probes.h"
#include "probes_arm.h"
#include "probes_thumb.h"


int make_tramp(struct arch_insn_arm *ainsn, u32 vaddr, u32 insn,
	       u32 *tramp, u32 tramp_len)
{
	int ret;
	int thumb_mode = vaddr & 1;

	vaddr &= ~1;
	if (thumb_mode)
		ret = make_trampoline_thumb(ainsn, vaddr, insn,
					    tramp, tramp_len);
	else
		ret = make_trampoline_arm(vaddr, insn, tramp);

	return ret;
}
