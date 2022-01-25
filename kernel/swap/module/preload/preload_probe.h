/*
 *  SWAP uprobe manager
 *  modules/us_manager/probes/preload_probe.h
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014	 Alexander Aksenov: FBI implement
 *
 */

#ifndef __PRELOAD_HANDLERS_PROBE_H__
#define __PRELOAD_HANDLERS_PROBE_H__

/* Probe flags description:
 *
 *    0 - handler is ran only when probe has fired from a target binary;
 *    1 - handler is always ran;
 *
 *   00 - probe is disabling internal probes;
 *   10 - probe is non blocking one;
 *
 *  000 - probe is executed for instrumented binaries
 *  100 - probe is executed for non-instrumented binaries
 */

enum {
	SWAP_PRELOAD_ALWAYS_RUN =       (1 << 0),
	SWAP_PRELOAD_NON_BLOCK_PROBE =  (1 << 1),
	SWAP_PRELOAD_INVERTED_PROBE =   (1 << 2)
};

/* Preload probe info. */
struct preload_info {
	unsigned long handler;              /* Handler offset in probe library. */
	unsigned char flags;                /* Preload probe flags. */
	const char *path;                   /* Library with handler */
	struct dentry *dentry;              /* Handler file dentry */
};

/* Get caller probe info */
struct get_caller_info {
};

/* Get call type probe info */
struct get_call_type_info {
};

/* Write message probe info */
struct write_msg_info {
};

int register_preload_probes(void);
void unregister_preload_probes(void);

#endif /* __PRELOAD_HANDLERS_PROBE_H__ */
