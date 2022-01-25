/**
 * @file parser/msg_parser.h
 * @author Vyacheslav Cherkashin
 * @author Vitaliy Cherepanov
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
 * Message parsing interface declaration.
 */


#ifndef _MSG_PARSER_H
#define _MSG_PARSER_H

#include <linux/types.h>
#include <us_manager/probes/probes.h>

struct img_ip;
struct msg_buf;

/**
 * @enum APP_TYPE
 * Supported application types.
 */
enum APP_TYPE {
	AT_TIZEN_NATIVE_APP	= 0x01,     /**< Tizen native application. */
	AT_PID			= 0x02,         /**< App with specified PID. */
	AT_COMMON_EXEC		= 0x03,     /**< Common application. */
	AT_TIZEN_WEB_APP	= 0x04      /**< Tizen web application. */
};

/**
 * @brief App type size defenition.
 */
enum {
	SIZE_APP_TYPE = 4
};

/**
 * @struct conf_data
 * @brief Configuration struct.
 */
struct conf_data {
	u64 use_features0;          /**< Feature flags. */
	u64 use_features1;          /**< Feature flags. */
	u32 sys_trace_period;       /**< Trace period. */
	u32 data_msg_period;        /**< Data message period. */
};

/**
 * @brief The pr_app_info struct
 */
struct pr_app_info {
	enum APP_TYPE type;	/**< Application type. */
	pid_t tgid;		/**< Application PID. */
	char *id;		/**< Application ID */
	char *path;		/**< Application execution path. */
};

/**
 * @brief The pr_bin_info struct
 */
struct pr_bin_info {
	char *path;
};

/**
 * @brief The pr_app_desc struct
 */
struct pr_app_desc {
	struct list_head list;

	struct pr_app_info *info;
	struct pf_group *pfg;
	struct list_head bin_head;
};

/**
 * @brief The pr_bin_desc struct
 */
struct pr_bin_desc {
	struct list_head list;

	struct pr_bin_info *info;

	struct list_head probe_head;
};

struct pr_probe_desc {
	struct list_head list;

	/* register info */
	u64 addr;
	struct probe_desc p_desc;

	/* unregister info */
	struct img_ip *ip;
};


struct pr_app_info *pr_app_info_create(struct msg_buf *mb);
void pr_app_info_free(struct pr_app_info *app_info);
int pr_app_info_cmp(struct pr_app_info *app0, struct pr_app_info *app1);

struct pr_bin_desc *pr_bin_desc_create(const char *path,
				       struct list_head *probe_list);
void pr_bin_desc_free(struct pr_bin_desc *bin);

int pr_bin_info_cmp(struct pr_bin_info *b0, struct pr_bin_info *b1);

struct pr_probe_desc *pr_probe_desc_create(struct msg_buf *mb);
void pr_probe_desc_free(struct pr_probe_desc *probe_info);
int probe_inst_info_cmp(struct pr_probe_desc *p0, struct pr_probe_desc *p1);

struct conf_data *create_conf_data(struct msg_buf *mb);
void destroy_conf_data(struct conf_data *conf);

void save_config(const struct conf_data *conf);
void restore_config(struct conf_data *conf);

struct pr_app_desc *pr_app_desc_create(struct msg_buf *mb);
void pr_app_desc_free(struct pr_app_desc *app);

u32 create_us_inst_data(struct msg_buf *mb, struct list_head *head);
void destroy_us_inst_data(struct list_head *head);


/**
 * @brief Constant defenitions.
 */
enum {
	MIN_SIZE_STRING = 1,
	MIN_SIZE_FUNC_INST = 8 /* address size */ +
			     MIN_SIZE_STRING,
	MIN_SIZE_LIB_INST = MIN_SIZE_STRING +
			    4 /* lib counter */,
	MIN_SIZE_APP_INFO = SIZE_APP_TYPE + MIN_SIZE_STRING + MIN_SIZE_STRING,
	MIN_SIZE_APP_INST = MIN_SIZE_APP_INFO +
			    4 /* probe counter */ +
			    4 /* lib counter */,
};

#endif /* _MSG_PARSER_H */
