/*
   BlueZ - Bluetooth protocol stack for Linux

   Copyright (C) 2010  Nokia Corporation
   Copyright (C) 2011-2012  Intel Corporation

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
   SOFTWARE IS DISCLAIMED.
*/

#ifdef CONFIG_TIZEN_WIP

#define TIZEN_OP_CODE_BASE	0xff00
#define TIZEN_EV_BASE		0xff00

#define MGMT_OP_SET_ADVERTISING_PARAMS		(TIZEN_OP_CODE_BASE + 0x01)
struct mgmt_cp_set_advertising_params {
	__le16  interval_min;
	__le16  interval_max;
	__u8 filter_policy;
	__u8 type;
} __packed;
#define MGMT_SET_ADVERTISING_PARAMS_SIZE	6

#define MGMT_OP_SET_ADVERTISING_DATA		(TIZEN_OP_CODE_BASE + 0x02)
struct mgmt_cp_set_advertising_data {
	__u8    data[HCI_MAX_AD_LENGTH];
} __packed;
#define MGMT_SET_ADVERTISING_DATA_SIZE		HCI_MAX_AD_LENGTH
#define MGMT_SET_ADV_MIN_APP_DATA_SIZE		1

#define MGMT_OP_SET_SCAN_RSP_DATA		(TIZEN_OP_CODE_BASE + 0x03)
struct mgmt_cp_set_scan_rsp_data {
	__u8    data[HCI_MAX_AD_LENGTH];
} __packed;
#define MGMT_SET_SCAN_RSP_DATA_SIZE		HCI_MAX_AD_LENGTH
#define MGMT_SET_SCAN_RSP_MIN_APP_DATA_SIZE	1

#define MGMT_OP_ADD_DEV_WHITE_LIST		(TIZEN_OP_CODE_BASE + 0x04)
struct mgmt_cp_add_dev_white_list {
	__u8	bdaddr_type;
	bdaddr_t bdaddr;
} __packed;
#define MGMT_ADD_DEV_WHITE_LIST_SIZE		7

#define MGMT_OP_REMOVE_DEV_FROM_WHITE_LIST	(TIZEN_OP_CODE_BASE + 0x05)
struct mgmt_cp_remove_dev_from_white_list {
	__u8	bdaddr_type;
	bdaddr_t bdaddr;
} __packed;
#define MGMT_REMOVE_DEV_FROM_WHITE_LIST_SIZE	7

#define MGMT_OP_CLEAR_DEV_WHITE_LIST		(TIZEN_OP_CODE_BASE + 0x06)
#define MGMT_OP_CLEAR_DEV_WHITE_LIST_SIZE	0

/* BEGIN TIZEN_Bluetooth :: RSSI monitoring   */
#define MGMT_OP_SET_RSSI_ENABLE			(TIZEN_OP_CODE_BASE + 0x07)
#define MGMT_SET_RSSI_ENABLE_SIZE		10

struct mgmt_cp_set_enable_rssi {
	__s8    low_th;
	__s8    in_range_th;
	__s8    high_th;
	bdaddr_t bdaddr;
	__s8    link_type;
} __packed;

struct mgmt_cc_rsp_enable_rssi {
	__u8     status;
	__u8     le_ext_opcode;
	bdaddr_t bt_address;
	__s8    link_type;
} __packed;

#define MGMT_OP_GET_RAW_RSSI			(TIZEN_OP_CODE_BASE + 0x08)
#define MGMT_GET_RAW_RSSI_SIZE			7

struct mgmt_cp_get_raw_rssi {
	bdaddr_t bt_address;
	__u8     link_type;
} __packed;

#define MGMT_OP_SET_RSSI_DISABLE		(TIZEN_OP_CODE_BASE + 0x09)
#define MGMT_SET_RSSI_DISABLE_SIZE		7
struct mgmt_cp_disable_rssi {
	bdaddr_t   bdaddr;
	__u8     link_type;
} __packed;
struct mgmt_cc_rp_disable_rssi {
	__u8 status;
	__u8 le_ext_opcode;
	bdaddr_t bt_address;
	__s8    link_type;
} __packed;
/* END TIZEN_Bluetooth :: RSSI monitoring */

#define MGMT_OP_START_LE_DISCOVERY		(TIZEN_OP_CODE_BASE + 0x0a)
struct mgmt_cp_start_le_discovery {
	    __u8 type;
} __packed;
#define MGMT_START_LE_DISCOVERY_SIZE		1

#define MGMT_OP_STOP_LE_DISCOVERY		(TIZEN_OP_CODE_BASE + 0x0b)
struct mgmt_cp_stop_le_discovery {
	    __u8 type;
} __packed;
#define MGMT_STOP_LE_DISCOVERY_SIZE		1

/* BEGIN TIZEN_Bluetooth :: LE auto connection */
#define MGMT_OP_DISABLE_LE_AUTO_CONNECT		(TIZEN_OP_CODE_BASE + 0x0c)
#define MGMT_DISABLE_LE_AUTO_CONNECT_SIZE	0
/* END TIZEN_Bluetooth */

#define MGMT_LE_CONN_UPDATE_SIZE		14
#define MGMT_OP_LE_CONN_UPDATE			(TIZEN_OP_CODE_BASE + 0x0d)
struct mgmt_cp_le_conn_update {
	__le16  conn_interval_min;
	__le16  conn_interval_max;
	__le16  conn_latency;
	__le16  supervision_timeout;
	bdaddr_t bdaddr;
} __packed;

#define MGMT_OP_SET_MANUFACTURER_DATA		(TIZEN_OP_CODE_BASE + 0x0e)
struct mgmt_cp_set_manufacturer_data {
	__u8 data[100];
} __packed;
#define MGMT_SET_MANUFACTURER_DATA_SIZE		100

#define MGMT_OP_LE_SET_SCAN_PARAMS		(TIZEN_OP_CODE_BASE + 0x0f)
struct mgmt_cp_le_set_scan_params {
	__u8	type;	/* le scan type */
	__le16	interval;
	__le16	window;
} __packed;
#define MGMT_LE_SET_SCAN_PARAMS_SIZE		5

#define MGMT_SCO_ROLE_HANDSFREE			0x00
#define MGMT_SCO_ROLE_AUDIO_GATEWAY		0x01
#define MGMT_OP_SET_VOICE_SETTING		(TIZEN_OP_CODE_BASE + 0x10)
struct mgmt_cp_set_voice_setting {
	bdaddr_t bdaddr;
	uint8_t  sco_role;
	uint16_t voice_setting;
} __packed;
#define MGMT_SET_VOICE_SETTING_SIZE	9

#define MGMT_OP_GET_ADV_TX_POWER		(TIZEN_OP_CODE_BASE + 0x11)
#define MGMT_GET_ADV_TX_POWER_SIZE			0
struct mgmt_rp_get_adv_tx_power {
	__s8 adv_tx_power;
} __packed;

#define MGMT_OP_ENABLE_6LOWPAN		(TIZEN_OP_CODE_BASE + 0x12)
#define MGMT_ENABLE_BT_6LOWPAN_SIZE			1
struct mgmt_cp_enable_6lowpan {
	__u8 enable_6lowpan;
} __packed;

#define MGMT_OP_CONNECT_6LOWPAN	(TIZEN_OP_CODE_BASE + 0x13)
struct mgmt_cp_connect_6lowpan {
	struct mgmt_addr_info addr;
} __packed;
#define MGMT_CONNECT_6LOWPAN_SIZE	7

#define MGMT_OP_DISCONNECT_6LOWPAN	(TIZEN_OP_CODE_BASE + 0x14)
struct mgmt_cp_disconnect_6lowpan {
	struct mgmt_addr_info addr;
} __packed;
#define MGMT_DISCONNECT_6LOWPAN_SIZE	7

#define MGMT_OP_LE_READ_MAXIMUM_DATA_LENGTH	(TIZEN_OP_CODE_BASE + 0x15)
struct mgmt_rp_le_read_maximum_data_length {
	__u8 status;
	__le16 max_tx_octets;
	__le16 max_tx_time;
	__le16 max_rx_octets;
	__le16 max_rx_time;
} __packed;
#define MGMT_LE_READ_MAXIMUM_DATA_LENGTH_SIZE	0

#define MGMT_OP_LE_WRITE_HOST_SUGGESTED_DATA_LENGTH	(TIZEN_OP_CODE_BASE + 0x16)
struct mgmt_cp_le_write_host_suggested_data_length {
	__le16 def_tx_octets;
	__le16 def_tx_time;
} __packed;
#define MGMT_LE_WRITE_HOST_SUGGESTED_DATA_LENGTH_SIZE	4

#define MGMT_OP_LE_READ_HOST_SUGGESTED_DATA_LENGTH	(TIZEN_OP_CODE_BASE + 0x17)
struct mgmt_rp_le_read_host_suggested_data_length {
	__u8 status;
	__le16 def_tx_octets;
	__le16 def_tx_time;
} __packed;
#define MGMT_LE_READ_HOST_SUGGESTED_DATA_LENGTH_SIZE	0

#define MGMT_OP_LE_SET_DATA_LENGTH	(TIZEN_OP_CODE_BASE + 0x18)
struct mgmt_cp_le_set_data_length {
	bdaddr_t bdaddr;
	__le16 max_tx_octets;
	__le16 max_tx_time;
} __packed;
#define MGMT_LE_SET_DATA_LENGTH_SIZE	 10

#define MGMT_OP_SET_IRK				(TIZEN_OP_CODE_BASE + 0x19)
struct mgmt_cp_set_irk {
	uint8_t irk[16];
} __packed;
#define MGMT_SET_IRK_SIZE			16

#define MGMT_OP_SET_DEV_RPA_RES_SUPPORT		(TIZEN_OP_CODE_BASE + 0x1A)
struct mgmt_cp_set_dev_rpa_res_support {
	struct	mgmt_addr_info addr;
	__u8	res_support;
} __packed;
#define MGMT_OP_SET_DEV_RPA_RES_SUPPORT_SIZE	8

#define MGMT_OP_SET_STREAMING_MODE		(TIZEN_OP_CODE_BASE + 0x1B)
struct mgmt_cp_set_streaming_mode {
	uint8_t streaming_mode;
	bdaddr_t bdaddr;
} __packed;
#define MGMT_SET_STREAMING_MODE_SIZE		7

struct mgmt_rp_le_set_data_length {
	__u8 status;
	__le16 handle;
} __packed;
#define MGMT_LE_SET_DATA_LENGTH_RSP_SIZE	 3

/* BEGIN TIZEN_Bluetooth :: name update changes */
#define MGMT_EV_DEVICE_NAME_UPDATE		(TIZEN_EV_BASE + 0x01)
struct mgmt_ev_device_name_update {
	struct mgmt_addr_info addr;
	__le16  eir_len;
	__u8    eir[0];
} __packed;
/* END TIZEN_Bluetooth :: name update changes */

/* BEGIN TIZEN_Bluetooth :: Add handling of hardware error event   */
#define MGMT_EV_HARDWARE_ERROR			(TIZEN_EV_BASE + 0x02)
struct mgmt_ev_hardware_error {
	__u8	error_code;
} __packed;
/* END TIZEN_Bluetooth */

/* BEGIN TIZEN_Bluetooth :: HCI TX Timeout Error   */
#define MGMT_EV_TX_TIMEOUT_ERROR		(TIZEN_EV_BASE + 0x03)
/* END TIZEN_Bluetooth */

/* BEGIN TIZEN_Bluetooth :: Add handling of RSSI Events   */
#define MGMT_EV_RSSI_ALERT			(TIZEN_EV_BASE + 0x04)
struct mgmt_ev_vendor_specific_rssi_alert {
	bdaddr_t bdaddr;
	__s8     link_type;
	__s8     alert_type;
	__s8     rssi_dbm;
} __packed;

#define MGMT_EV_RAW_RSSI			(TIZEN_EV_BASE + 0x05)
struct mgmt_cc_rp_get_raw_rssi {
	__u8     status;
	__s8     rssi_dbm;
	__u8     link_type;
	bdaddr_t bt_address;
} __packed;

#define MGMT_EV_RSSI_ENABLED			(TIZEN_EV_BASE + 0x06)

#define MGMT_EV_RSSI_DISABLED			(TIZEN_EV_BASE + 0x07)
/* END TIZEN_Bluetooth :: Handling of RSSI Events */

/* BEGIN TIZEN_Bluetooth :: Add LE connection update Events   */
#define MGMT_EV_CONN_UPDATED			(TIZEN_EV_BASE + 0x08)
struct mgmt_ev_conn_updated {
	struct	mgmt_addr_info addr;
	__le16	conn_interval;
	__le16	conn_latency;
	__le16	supervision_timeout;
} __packed;

#define MGMT_EV_CONN_UPDATE_FAILED		(TIZEN_EV_BASE + 0x09)
struct mgmt_ev_conn_update_failed {
	struct	mgmt_addr_info addr;
	__u8	status;
} __packed;
/* END TIZEN_Bluetooth :: Add LE connection update Events */

#define MGMT_EV_LE_DEVICE_FOUND			(TIZEN_EV_BASE + 0x0a)
struct mgmt_ev_le_device_found {
	struct mgmt_addr_info addr;
	__s8	rssi;
	__le32	flags;
	__s8	adv_type;
	__le16	eir_len;
	__u8	eir[0];
} __packed;

#define MGMT_EV_MULTI_ADV_STATE_CHANGED			(TIZEN_EV_BASE + 0x0b)
struct mgmt_ev_vendor_specific_multi_adv_state_changed {
	__u8	adv_instance;
	__u8	state_change_reason;
	__le16	connection_handle;
} __packed;

#define MGMT_EV_6LOWPAN_CONN_STATE_CHANGED		(TIZEN_EV_BASE + 0x0c)
struct mgmt_ev_6lowpan_conn_state_changed {
	struct	mgmt_addr_info addr;
	__u8	connected;
} __packed;


#define MGMT_EV_LE_DATA_LENGTH_CHANGED		(TIZEN_EV_BASE + 0x0d)
struct mgmt_ev_le_data_length_changed {
	struct	mgmt_addr_info addr;
	__le16 max_tx_octets;
	__le16 max_tx_time;
	__le16 max_rx_octets;
	__le16 max_rx_time;
} __packed;

#endif   /* CONFIG_TIZEN_WIP */
