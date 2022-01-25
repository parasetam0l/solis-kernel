/*
 *
 *	Generic Bluetooth SDIO driver
 *
 *	Copyright (C) 2007	Cambridge Silicon Radio Ltd.
 *	Copyright (C) 2007	Marcel Holtmann <marcel@holtmann.org>
 *	Copyright (C) 1999-2016, Broadcom Corporation
 *	
 *	     Unless you and Broadcom execute a separate written software license
 *	agreement governing use of this software, this software is licensed to you
 *	under the terms of the GNU General Public License version 2 (the "GPL"),
 *	available at http://www.broadcom.com/licenses/GPLv2.php, with the
 *	following added to such license:
 *	
 *	     As a special exception, the copyright holders of this software give you
 *	permission to link this software with independent modules, and to copy and
 *	distribute the resulting executable under terms of your choice, provided that
 *	you also meet, for each linked independent module, the terms and conditions of
 *	the license of that module.  An independent module is a module which is not
 *	derived from this software.  The special exception does not apply to any
 *	modifications of the software.
 *	
 *	     Notwithstanding the above, under no circumstances may you combine this
 *	software in any way with any other Broadcom software provided under a license
 *	other than the GPL, without Broadcom's express prior written consent.
 *
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id:$
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/skbuff.h>

#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>
#include <net/bluetooth/hci.h>

#include <linux/time.h>
#include <dhd_bt_interface.h>
#include <linux/version.h>
#include "bcmbtsdio.h"

#ifdef CONFIG_IRQ_HISTORY
#include <linux/power/irq_history.h>
#endif

#ifdef BT_SDIO_DEBUG
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define BT_SDIO_DBG(fmt, arg...)	bt_printk(KERN_INFO, pr_fmt(fmt), ##arg)
#else
#define BT_SDIO_DBG BT_INFO
#endif
#else
#define BT_SDIO_DBG(fmt, arg...)
#endif /* BT_SDIO_DEBUG */


#if defined(BT_LPM_ENABLE)
static void bcmbtsdio_set_btwake(bool bAssert, int tx_bt_wake);
#define BCMBTSDIO_SET_BTWAKE(bAssert, btWake) bcmbtsdio_set_btwake(bAssert, btWake)
#else
#define BCMBTSDIO_SET_BTWAKE(bAssert, btWake)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
#define HCI_GET_DRV_DATA(hdev) hci_get_drvdata(hdev)
#define HCI_SET_DRV_DATA(hdev, data) hci_set_drvdata(hdev, data)
#else
#define HCI_GET_DRV_DATA(hdev) (hdev->driver_data)
#define HCI_SET_DRV_DATA(hdev, data) (hdev->driver_data = data)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define HCI_RECV_FRAME(hdev, skb)	hci_recv_frame(skb)
#else
#define HCI_RECV_FRAME(hdev, skb)	hci_recv_frame(hdev, skb)
#endif

#if !defined(SDIO_VENDOR_ID_BROADCOM)
#define SDIO_VENDOR_ID_BROADCOM		0x02d0
#endif /* !defined(SDIO_VENDOR_ID_BROADCOM) */

#define SDIO_DEVICE_ID_BROADCOM_DEFAULT	0x0000

extern void bt_set_power(void *data, bool blocked);

static const struct sdio_device_id bcmbtsdio_table[] = {
	{ SDIO_DEVICE(SDIO_VENDOR_ID_BROADCOM, SDIO_DEVICE_ID_BROADCOM_DEFAULT) },
	/* Generic Bluetooth Type-A SDIO device */
	{ SDIO_DEVICE_CLASS(SDIO_CLASS_BT_A) },

	/* Generic Bluetooth Type-B SDIO device */
	{ SDIO_DEVICE_CLASS(SDIO_CLASS_BT_B) },

	/* Generic Bluetooth AMP controller */
	{ SDIO_DEVICE_CLASS(SDIO_CLASS_BT_AMP) },

	{ }	/* Terminating entry */
};

MODULE_DEVICE_TABLE(sdio, bcmbtsdio_table);

struct bcmbtsdio_data {
	struct hci_dev	 *hdev;
	struct sdio_func *func;

	struct work_struct work;
	struct delayed_work reg_work;
	struct delayed_work dereg_work;

	struct sk_buff_head txq;

	struct sk_buff	*rx_buf;	/* RX packet */

	unsigned int tx_intr_flags;
	unsigned int rx_intr_flags;
};

/* Extern Variables */
/* Interrupt handler is in DHD context.The below functions can be */
/* directly called without sdlock */
unsigned char bcmsdh_cfg_read(void *sdh, uint func, unsigned int addr,
	int *err);
void bcmsdh_cfg_write(void *sdh, uint func, unsigned int addr,
	unsigned char data, int *err);

/* Flag to denote the current wake state */
int g_wake_flag = false;

/* Fragmentation maximum packet length is passed as module param */
char g_btfw_path[MOD_PARAM_PATHLEN];
int g_max_packet_len = TX_FRAG_LEN;
wlan_bt_handle_t g_dhd_handle = NULL;
static int g_tx_intr_status  = TX_STATUS_SEND_NONE;
static struct semaphore work_lock;
static bt_power_state_t g_state = BT_POWERED_OFF;

static int g_bt_sdio_registered;

/* Extern Functions */
/* Exported from BT power handling platform driver */
void bt_power_state_register(bt_power_change_t);
void bt_set_wake(int bAssert);

/* Forward Declarations */
static int bcmbtsdio_tx_packet(struct bcmbtsdio_data *data,
	struct sk_buff *skb, int* tx_bt_wake_flag);
static int bcmbtsdio_rx_packet(struct bcmbtsdio_data *data);

static void bcmbtsdio_reg_hci_work(struct work_struct *reg_sdio);
static void bcmbtsdio_dereg_hci_work(struct work_struct *work);

static void bcmbtsdio_dhd_put(void);

static int bcmbtsdio_dhd_get(void);

#if defined(BT_LPM_ENABLE)
static void bcmbtsdio_set_btwake(bool bAssert, int tx_bt_wake)
{
	if (!g_dhd_handle) {
		BT_INFO("%s: g_dhd_handle NULL return", __func__);
		return;
	}

	if (bAssert && !g_wake_flag) {
		/* update WLAN BT is ready to sleep */
		dhd_bus_clk_enable(g_dhd_handle, BT_MODULE);
		g_wake_flag = true;
	}
	else if (!bAssert) {
		if (g_wake_flag) {
			/* update WLAN BT is Waking */
			g_wake_flag = false;
			dhd_bus_clk_disable(g_dhd_handle, BT_MODULE);
		}
		else
			BT_SDIO_DBG("%s: called when g_wake_flag is false", __FUNCTION__);
	}

	if (tx_bt_wake) {
		/* BTWAKE GPIO is pulled high/down */
		/* Required only for TX path as RX path is controlled by hostwake */
		bt_set_wake(bAssert);
	}
}
#endif /* defined (BT_LPM_ENABLE) */


static void fragment_packet(struct bcmbtsdio_data *data,
	struct sk_buff *skb, unsigned char packetType)
{
	int dataLen = 0;
	struct sk_buff *skb_new = NULL;

	int len = skb->len;
	int maxlen = g_max_packet_len - PACKET_HEADER;
	unsigned char* ptr = skb->data;

	if (skb->len > 0) {

		do {
			if (len >= maxlen)
			{
				dataLen = maxlen;
				len -= maxlen;
			}
			else
			{
				dataLen = len;
				len = 0;
			}
			skb_new = bt_skb_alloc((dataLen + PACKET_HEADER), GFP_KERNEL);

			memcpy((skb_new->data + PACKET_HEADER), ptr, dataLen);
			skb_put(skb_new, (dataLen + PACKET_HEADER));
			skb_new->data[0] = (skb_new->len & 0x0000ff);
			skb_new->data[1] = (skb_new->len & 0x00ff00) >> 8;
			skb_new->data[2] = (skb_new->len & 0xff0000) >> 16;
			skb_new->data[3] = (len ? (packetType | CONTINUE_PACKET_MASK) : packetType);
			bt_cb(skb_new)->pkt_type = skb_new->data[3];

			ptr += dataLen;
			BT_SDIO_DBG("Fragment Length %d skb_new->len %d packetType 0x%x \n",
			len, skb_new->len, skb_new->data[3]);
			skb_queue_tail(&data->txq, skb_new);

		} while (len);
	}
	else {
		BT_SDIO_DBG("Invalid Length %d\n", skb->len);
	}
	return;
}

static int bcmbtsdio_tx_packet(struct bcmbtsdio_data *data,
	struct sk_buff *skb, int* tx_bt_wake)
{
	int err;
	BT_SDIO_DBG("+%s\n", __FUNCTION__);

	if (g_state != BT_POWERED_ON) {
		BT_INFO("%s: BT Power not set\n", __FUNCTION__);
		return 0;
	}

	data->tx_intr_flags = false;

	sdio_claim_host(data->func);
	err = sdio_writesb(data->func, REG_TDAT, skb->data, skb->len);
	if (err < 0) {
		skb_pull(skb, 4);
		sdio_writeb(data->func, 0x01, REG_PC_WRT, NULL);
		BT_ERR("%s err = %d", __FUNCTION__, err);
		sdio_release_host(data->func);
		return err;
	}
	sdio_release_host(data->func);
	g_tx_intr_status = TX_STATUS_SENDING;

	if (skb->data[3] & CONTINUE_PACKET_MASK) {
		*tx_bt_wake = false;
		BT_SDIO_DBG("==> FRAG len %d  data: 0x%x\n", skb->len, skb->data[8]);
	}
	else {
		*tx_bt_wake = true;
		BT_SDIO_DBG("==> LAST len %d  data: 0x%x\n", skb->len, skb->data[8]);
	}
	data->hdev->stat.byte_tx += skb->len;
	kfree_skb(skb);
	return 0;

}

static void bcmbtsdio_work(struct work_struct *work)
{
	struct bcmbtsdio_data *data = container_of(work, struct bcmbtsdio_data, work);
	struct sk_buff *skb_frag;
	int err;
	int tx_bt_wake = false;

	if (g_state != BT_POWERED_ON) {
		BT_INFO("%s: BT Power not set\n", __FUNCTION__);
		return;
	}

	down(&work_lock);

	if (!data->tx_intr_flags && !data->rx_intr_flags)
	{
		up(&work_lock);
		BT_INFO("%s: TX and RX flag not set\n", __FUNCTION__);
		return;
	}

	if (data->tx_intr_flags) {
		skb_frag = skb_dequeue(&data->txq);
		if (skb_frag)
		{
			BT_SDIO_DBG("%s Frag pkt_type 0x%x len %d\n", __FUNCTION__,
			bt_cb(skb_frag)->pkt_type, skb_frag->len);
			BCMBTSDIO_SET_BTWAKE(true, true);

			err = bcmbtsdio_tx_packet(data, skb_frag, &tx_bt_wake);
			if (err < 0) {
				data->hdev->stat.err_tx++;
				skb_queue_head(&data->txq, skb_frag);
			}
		}
		else if (skb_frag == NULL)
		{
			BT_SDIO_DBG("%s: Fragmented packet is NULL!!!\n", __FUNCTION__);
			data->tx_intr_flags = true;
		}
	}
	if (data->rx_intr_flags) {
		BCMBTSDIO_SET_BTWAKE(true, false);
		bcmbtsdio_rx_packet(data);
	}
	BCMBTSDIO_SET_BTWAKE(false, tx_bt_wake);
	up(&work_lock);
}

static int bcmbtsdio_rx_packet(struct bcmbtsdio_data *data)
{
	u8 hdr[4] __attribute__ ((aligned(4)));
	struct sk_buff *skb = NULL;
	int err, len;
	int bytesToRead		= 0;
	int bytesRead		= 0;

	BT_SDIO_DBG("+%s\n", __FUNCTION__);
	if (g_state != BT_POWERED_ON) {
		BT_INFO("%s: BT Power not set\n", __FUNCTION__);
		return 0;
	}

	data->rx_intr_flags = false;

	/* check if we are in the middle of receiving fragments */
	if (data->rx_buf)
	{
		skb = data->rx_buf;
		bytesRead = skb->len;
	}
	else
	{
		/* alloc new */
		skb = bt_skb_alloc(MAX_ACL_SIZE, GFP_KERNEL);
		data->rx_buf = skb;
	}

	sdio_claim_host(data->func);
	/* read header */
	err = sdio_readsb(data->func, hdr, REG_RDAT, 4);
	if (err < 0)
		goto rx_err;

	len = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16);

	if (len < 4 || len > 65543)
	{
		err = -EILSEQ;
		goto rx_err;
	}
	BT_SDIO_DBG("<== 0x%x 0x%x 0x%x 0x%x \n", hdr[0], hdr[1], hdr[2], hdr[3]);

	bytesToRead = len - PACKET_HEADER;

	/* read data */
	err = sdio_readsb(data->func, (skb->data + bytesRead), REG_RDAT, bytesToRead);
	bytesRead += bytesToRead;
	if (err < 0)
		goto rx_err;

	/* set the length correclty */
	skb_put(skb, bytesToRead);

	/* read completed successfully. clear retry and release host */
	sdio_writeb(data->func, 0x00, REG_PC_RRT, NULL);
	sdio_release_host(data->func);

	/* if fragment, queue and return */
	if (hdr[3] & (CONTINUE_PACKET_MASK))
		return 0;

	/* reset reassembly buffer (rx_buf) */
	data->rx_buf = NULL;

	/* pass to hci */
	data->hdev->stat.byte_rx += bytesRead;
	skb->dev = (void *) data->hdev;
	bt_cb(skb)->pkt_type = hdr[3];

	BT_SDIO_DBG("-%s. type=0x%x len=0x%x\n", __FUNCTION__, hdr[3], skb->len);

	err = HCI_RECV_FRAME(data->hdev, skb);
	if (err < 0)
		BT_ERR("%s hci_recv_frame returned err %d", __FUNCTION__, err);

	return err;

rx_err:
	/* in the error scenario set the retry register, increment err_rx and release host.
	 * We should not free skb/rx_buf here as the skb is for the re-assembled buffer and retry
	 * will only be for the fragment
	 */
	data->hdev->stat.err_rx++;
	sdio_writeb(data->func, 0x01, REG_PC_RRT, NULL);
	sdio_release_host(data->func);
	BT_ERR("%s HDR Read err %d", __FUNCTION__, err);
	return err;
}

/* XXX: BT/WIFI wake up check...
 * When call wlan_oob_irq on suspend mode, dhd_mmc_wake is TRUE.
 */
extern bool dhd_mmc_wake;
extern bool dhd_bt_wake;

void bcmbtsdio_process_f3_interrupt(struct sdio_func *func)
{
	struct bcmbtsdio_data *data = sdio_get_drvdata(func);
	int intrd, err = 0;

	/* Read the REG to check interrupt status */
	intrd = bcmsdh_cfg_read(NULL, SDIO_FUNC_3, REG_INTRD, &err);
	if ((!intrd) || err) {
		if (err) {
			BT_ERR("bcmbtsdio_process_f3_interrupt read err : 0x%x\n", err);
		}
		return;
	}

       if (dhd_mmc_wake) {
               /* XXX: BT/WIFI wake up check... */
               dhd_mmc_wake = false;
       }

       if (dhd_bt_wake) {
#ifdef CONFIG_IRQ_HISTORY
               add_irq_history(0, "BT");
#endif
			   dhd_bt_wake = false;
       }



	BT_SDIO_DBG("INTR flags 0x%x\n", intrd);
	bcmsdh_cfg_write(NULL, SDIO_FUNC_3, REG_CL_INTRD, intrd, NULL);

	if (intrd & BTSDIO_INT_TX_BIT) {
		data->tx_intr_flags = true;
		g_tx_intr_status = TX_STATUS_SEND_DONE;
	}
	if (intrd & BTSDIO_INT_RX_BIT) {
		data->rx_intr_flags = true;
	}
	schedule_work(&data->work);
}


/* Reset HCI device */
int bcmbtsdio_hci_reset_dev(struct hci_dev *hdev)
{
#ifdef CUSTOMER_HW4
	/* Set a flag for prevening DBFW to be run when HW error occurred by DHD hang */
	hdev->dhd_hang = true;
#endif

/* hci_reset_dev function is added from kernel 3.19 version.
 * But Samsung uses hci_reset_dev function and their kernel version is 3.18.14.
 * So kernel version check routine added.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)

	const unsigned char hw_err[] = { HCI_EV_HARDWARE_ERROR, 0x01, 0x00 };
	struct sk_buff *skb;

	BT_INFO("%s", __FUNCTION__);

	skb = bt_skb_alloc(3, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;

	bt_cb(skb)->pkt_type = HCI_EVENT_PKT;

	memcpy(skb_put(skb, 3), hw_err, 3);

	/* Send Hardware Error to upper stack */
	return HCI_RECV_FRAME(hdev, skb);
#else
	return hci_reset_dev(hdev);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0) */
}

void bcmbtsdio_hang_notification(struct sdio_func *func, bool wifi_recovery_completed)
{
	struct bcmbtsdio_data *data = sdio_get_drvdata(func);

	BT_INFO("%s: wifi_recovery_completed[%d]", __FUNCTION__, wifi_recovery_completed);

	if (wifi_recovery_completed)
	{
		/* If WiFi was off, then start BT recovery directly */
		bcmsdh_btsdio_interface_init(NULL, NULL, NULL);

		if (data != NULL && data->hdev != NULL && data->hdev->name != NULL)
		{
			BT_INFO("%s : %s", __FUNCTION__, data->hdev->name);
			bcmbtsdio_hci_reset_dev(data->hdev);
		}
	}
	else
	{
		g_state = BT_HANG_RECOVERY;
		/* If WiFi was on, Wait for WiFi is completed to be recovered */
		bt_set_power(NULL, true);

		bcmbtsdio_dhd_put();
	}
}

static int bcmbtsdio_open(struct hci_dev *hdev)
{
	struct bcmbtsdio_data *data = HCI_GET_DRV_DATA(hdev);

	int err = 0;
	struct sdio_func *func = NULL;
	unsigned char ready = 0;
	unsigned char f3_int_en = 0;
	unsigned char enable = 0;
	int ready_cnt = 0;

	if (!data)
	{
		BT_INFO("%s: Called when driver data is NULL", __FUNCTION__);
		return err;
	}

	if (g_dhd_handle == NULL) {
		BT_INFO("%s: BT Power not set\n", __FUNCTION__);
		return -1;
	}

	BT_INFO("%s hci device = %s", __FUNCTION__, hdev->name);

	/* Initialize RX queue */
	data->rx_buf = NULL;

	data->tx_intr_flags = true;
	data->rx_intr_flags = false;

	BCMBTSDIO_SET_BTWAKE(true, true);

	if (test_and_set_bit(HCI_RUNNING, &hdev->flags))
		goto release;

	func = (struct sdio_func*)data->func;

	/* pass fun 3 to sdmmc to enable F3 */
	bcmsdh_btsdio_interface_init(func, NULL, NULL);

	/* Enable F3 Function */
	enable = SDIO_FUNC_ENABLE_3;

	BT_INFO("%s func = %p", __FUNCTION__, func);

	if (g_dhd_handle == NULL) {
		BT_INFO("%s: BT Power not set step #2\n", __func__);
		clear_bit(HCI_RUNNING, &hdev->flags);
		return -ENODEV;
	}
	dhd_bus_cfg_write(g_dhd_handle, SDIO_FUNC_0, SDIOD_CCCR_IOEN, enable, &err);
	if (err) {
		BT_INFO("%s: Could not enable F3 err %d", __FUNCTION__, err);
	}

	/* Wait till F3 is enabled */
	while (!(ready & enable)) {
		msleep(FW_IORDY_DELAY);
		/* Read the REG to check enabled is success */
		if (g_dhd_handle == NULL) {
			BT_INFO("%s: BT Power not set step #3\n", __func__);
			clear_bit(HCI_RUNNING, &hdev->flags);
			return -ENODEV;
		}
		ready = dhd_bus_cfg_read(g_dhd_handle, SDIO_FUNC_0, SDIOD_CCCR_IORDY, NULL);
		ready_cnt++;
		if (ready_cnt == FW_IORDY_CNT)
			goto release;
	}
	BT_INFO("%s IOREADY cnt = %d ready = 0x%x", __FUNCTION__, ready_cnt, ready);

	/* Avoid interrupt handler being called during F3 enable.
	 * Function handler for F3 interrupt should be passed before F3 interrupt is enabled
	 */
	bcmsdh_btsdio_interface_init(func, bcmbtsdio_process_f3_interrupt,
		bcmbtsdio_hang_notification);

	/* Enable F3 interrupts */
	f3_int_en = BTSDIO_INT_BITS;
	BT_INFO("%s Enable F3 Interrupts", __FUNCTION__);

	if (g_dhd_handle == NULL) {
		BT_INFO("%s: BT Power not set step #4\n", __func__);
		clear_bit(HCI_RUNNING, &hdev->flags);
		return -ENODEV;
	}
	dhd_bus_cfg_write(g_dhd_handle, SDIO_FUNC_3, SDIOD_F3_INTERRUPT_ENABLE, f3_int_en, &err);
	if (err) {
		BT_INFO("%s: Could not enable F3 0x%x ints", __FUNCTION__, f3_int_en);
		goto release;
	} else {
		int int_enable;

		if (g_dhd_handle == NULL) {
			BT_INFO("%s: BT Power not set step #5\n", __func__);
			clear_bit(HCI_RUNNING, &hdev->flags);
			return -ENODEV;
		}
		int_enable = dhd_bus_cfg_read(g_dhd_handle, SDIO_FUNC_3,
			SDIOD_F3_INTERRUPT_ENABLE, NULL);
		BT_INFO("%s: Enabled F3 0x%x ints. int_enable=0x%x",
			__FUNCTION__, f3_int_en, int_enable);
	}
	if (data->func->class == SDIO_CLASS_BT_B) {
		if (g_dhd_handle == NULL) {
			BT_INFO("%s: BT Power not set step #6\n", __func__);
			clear_bit(HCI_RUNNING, &hdev->flags);
			return -ENODEV;
		}
		dhd_bus_cfg_write(g_dhd_handle, SDIO_FUNC_3, REG_MD_STAT, 0x00, NULL);
	}

release:
	BCMBTSDIO_SET_BTWAKE(false, true);
	return err;
}

static int bcmbtsdio_close(struct hci_dev *hdev)
{
	struct bcmbtsdio_data *data = HCI_GET_DRV_DATA(hdev);

	unsigned char f3_int_en = 0;
	unsigned char disable = 0;
	int err = 0;

	BT_INFO("%s %s", __FUNCTION__, hdev->name);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 13, 0)
	/* HCI_RUNNING is cleared on hci_dev_do_close() */
	if (!g_dhd_handle || !data)
		return 0;
#else
	if ((!test_and_clear_bit(HCI_RUNNING, &hdev->flags)) || !g_dhd_handle ||
		!data)
		return 0;
#endif
	data->rx_buf = NULL;

	BCMBTSDIO_SET_BTWAKE(true, true);

	/* Disable F3 interrupts */
	f3_int_en = 0x00;
	BT_INFO("%s disable F3 Interrupts", __FUNCTION__);

	if (g_dhd_handle == NULL) {
		BT_INFO("%s: BT Power not set disable F3 Intr\n", __func__);
		goto close_done;
	}
	dhd_bus_cfg_write(g_dhd_handle, SDIO_FUNC_3, SDIOD_F3_INTERRUPT_ENABLE, f3_int_en, &err);
	if (err) {
		BT_ERR("%s: Could not disable F3 ints", __FUNCTION__);
	}

	/* Disable F3 */
	disable = SDIO_FUNC_DISABLE_3;
	BT_INFO("%s disable F3 Function", __FUNCTION__);

	if (g_dhd_handle == NULL) {
		BT_INFO("%s: BT Power not set disable F3 Func\n", __func__);
		goto close_done;
	}
	dhd_bus_cfg_write(g_dhd_handle, SDIO_FUNC_0, SDIOD_CCCR_IOEN, disable, &err);
	if (err) {
		BT_ERR("%s: Could not disable F3", __FUNCTION__);
	}

close_done:
	/* No Rx interrupt should be received during the close sequence */
	bcmsdh_btsdio_interface_init(NULL, NULL, NULL);
	cancel_work_sync(&data->work);

	BCMBTSDIO_SET_BTWAKE(false, true);

	BT_INFO("--%s", __FUNCTION__);
	return 0;
}

static int bcmbtsdio_flush(struct hci_dev *hdev)
{
	struct bcmbtsdio_data *data = HCI_GET_DRV_DATA(hdev);

	if (!data)
		return 0;

	BT_INFO("%s %s", __FUNCTION__, hdev->name);

	skb_queue_purge(&data->txq);

	return 0;
}
#ifdef CUSTOMER_HW4

#define TIZEN_BLUETOOTH_CSA	"/csa/bluetooth/.bd_addr"

static int bcmbtsdio_read_bt_macaddr(bdaddr_t *bdaddr)
{
	char *filepath_csa       = TIZEN_BLUETOOTH_CSA;
	char buf[18]         = {0};
	struct file *fp      = NULL;
	int ret = 0;
	int n;

	fp = filp_open(filepath_csa, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		BT_ERR("File open error(%s)", filepath_csa);
		return PTR_ERR(fp);
	}

	ret = kernel_read(fp, 0, buf, 15);
	if (ret) {
		n = sscanf(buf, "%02hhX%02hhX\n%02hhX\n%02hhX%02hhX%02hhX",
				&bdaddr->b[5], &bdaddr->b[4], &bdaddr->b[3],
				&bdaddr->b[2], &bdaddr->b[1], &bdaddr->b[0]);

		if (n < 6) {
			filp_close(fp, NULL);
			return -EINVAL;
		}
	} else {
		BT_ERR("read fail");
	}

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
	BT_INFO("Read Address is %pMR", bdaddr);
#else
	BT_INFO("Read address is done from csa\n");
#endif

	if (fp)
		filp_close(fp, NULL);

	return 0;
}

static int bcmbtsdio_set_bdaddr(struct hci_dev *hdev, const bdaddr_t *bdaddr)
{
	struct sk_buff *skb;
	int err;

	skb = __hci_cmd_sync(hdev, 0xfc01, 6, bdaddr, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		err = PTR_ERR(skb);
		BT_ERR("%s: BCM: Change address command failed (%d)",
				hdev->name, err);
		return err;
	}
	kfree_skb(skb);

	return 0;
}

static int bcmbtsdio_setup(struct hci_dev *hdev)
{
	struct sk_buff *skb;
	int ret = 0;

#if defined(BT_LPM_ENABLE)
	u8 lp_param[] = {0x06, /* Sleep_Mode : SDIO */
					IDLE_THRESHOLD_HOST,  /* Idle_Threshold_Host  */
					IDLE_THRESHOLD_HC,  /* Idle_Threshold_HC */
					0x01,  /* BT_WAKE_Active_Mode : Active high */
					0x01,  /* HOST_WAKE_Active_Mode  : Active high */
					0x01,  /* Allow_Host_Sleep_During_SCO  */
					0x01,  /* Combine_Sleep_Mode_And_LPM  */
					0x00,  /* not applicable */
					0x00,  /* not applicable */
					0x00,  /* not applicable */
					0x00,  /* not applicable */
					0x00}; /* not applicable */
#endif

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
	u8 param[] = {0x24, 0x01};	/* Enable Core Dump */
	/* u8 param[] = {0x24, 0x00};	Disable Core Dump
	 * - 81 Base fw * default setting
	 */
#endif
	ret = bcmbtsdio_read_bt_macaddr(&hdev->public_addr);

	if (ret < 0) {
		BT_INFO("Fail to read BT address from CSA(%d)", ret);
		return 0;
	}

	ret = bcmbtsdio_set_bdaddr(hdev, &hdev->public_addr);
	if (ret < 0) {
		BT_ERR("Fail to write BT address to controller(%d)", ret);
		return ret;
	}

#if defined(BT_LPM_ENABLE)
	skb = __hci_cmd_sync(hdev, HCI_VSC_WRITE_SLEEP_MODE, 12, lp_param, HCI_INIT_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: BCM: Set LPM command failed (%d)", hdev->name, ret);
		return ret;
	}
	kfree_skb(skb);
	BT_INFO("%s: lpm param sent", __FUNCTION__);
#endif
	skb = __hci_cmd_sync(hdev, HCI_OP_RESET, 0, NULL, HCI_CMD_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: Reset Fail (%d)", __func__, ret);
		return ret;
	}
	kfree_skb(skb);

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
	skb = __hci_cmd_sync(hdev, HCI_VSC_DBFW, 2, param, HCI_CMD_TIMEOUT);
	if (IS_ERR(skb)) {
		ret = PTR_ERR(skb);
		BT_ERR("%s: VSC DBFW Fail (%d)", __func__, ret);
	} else
		kfree_skb(skb);
#endif
	return 0;
}

static int bcmbtsdio_cmd_timeout(struct hci_dev *hdev)
{
	int val = 0;
	int err = 0;
	int result = 0;
	struct bcmbtsdio_data *data = HCI_GET_DRV_DATA(hdev);

	val = bcmsdh_cfg_read(NULL, SDIO_FUNC_0, SDIOD_CCCR_INTEN, &err);
	BT_ERR("%s: [0x00_0x04] = val[0x%x] err[%d]", __FUNCTION__, val, err);

	val = 0; err = 0;
	val = bcmsdh_cfg_read(NULL, SDIO_FUNC_0, SDIOD_CCCR_INTPEND, &err);
	BT_ERR("%s: [0x00_0x05] = val[0x%x] err[%d]", __FUNCTION__, val, err);

	val = 0; err = 0;
	val = bcmsdh_cfg_read(NULL, SDIO_FUNC_3, SDIOD_F3_INTERRUPT_PENDING, &err);
	BT_ERR("%s: [0x03_0x13] = val[0x%x] err[%d]", __FUNCTION__, val, err);

	BT_ERR("%s: g_tx_intr_status %d tx_intr_flags : %d",
			__FUNCTION__, g_tx_intr_status, (data != NULL) ? data->tx_intr_flags : 0);

	return result;
}
#endif /* CUSTOMER_HW4 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
static int bcmbtsdio_send_frame(struct hci_dev *hdev, struct sk_buff *skb)
{
#else
static int bcmbtsdio_send_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;
#endif
	struct bcmbtsdio_data *data = HCI_GET_DRV_DATA(hdev);

	if (!test_bit(HCI_RUNNING, &hdev->flags))
		return -EBUSY;

	switch (bt_cb(skb)->pkt_type) {
	case HCI_COMMAND_PKT:
		hdev->stat.cmd_tx++;
		break;

	case HCI_ACLDATA_PKT:
		hdev->stat.acl_tx++;
		break;

	case HCI_SCODATA_PKT:
		hdev->stat.sco_tx++;
		break;

	case HCI_DIAG_PKT:
		BT_SDIO_DBG("%s Type %d", __FUNCTION__, bt_cb(skb)->pkt_type);
		break;

	default:
		return -EILSEQ;
	}

	fragment_packet(data, skb, bt_cb(skb)->pkt_type);
	kfree_skb(skb);

	BT_SDIO_DBG("%s: schedule work\n", __FUNCTION__);
	if (g_tx_intr_status != TX_STATUS_SENDING)
		g_tx_intr_status = TX_STATUS_SEND_READY;
	schedule_work(&data->work);
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static void bcmbtsdio_destruct(struct hci_dev *hdev)
{
	BT_INFO("%s %s", __FUNCTION__, hdev->name);
}
#endif

static int bcmbtsdio_probe(struct sdio_func *func,
	const struct sdio_device_id *id)
{
	struct bcmbtsdio_data *data;
	struct hci_dev *hdev;
	struct sdio_func_tuple *tuple = func->tuples;
	int ret = 0;

	BT_INFO("%s func %p id %p", __FUNCTION__, func, id);

	BT_INFO("class 0x%04x sdio_vendor: 0x%04x sdio_device: 0x%04x Function#: 0x%04x",
	 func->class, func->vendor, func->device, func->num);

	while (tuple) {
		BT_DBG("code 0x%x size %d", tuple->code, tuple->size);
		tuple = tuple->next;
	}

	if (func->num != 3) {
		ret =  -EINVAL;
		goto release;
	}
	BT_INFO("%s Function 3 probe received", __FUNCTION__);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto release;
	}

	INIT_WORK(&data->work, bcmbtsdio_work);
	INIT_DELAYED_WORK(&data->reg_work, bcmbtsdio_reg_hci_work);
	INIT_DELAYED_WORK(&data->dereg_work, bcmbtsdio_dereg_hci_work);

	skb_queue_head_init(&data->txq);

	sdio_set_drvdata(func, data);
	data->func = func;

	hdev = hci_alloc_dev();
	if (!hdev) {
		kfree(data);
		ret = -ENOMEM;
		goto release;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 2)
	set_bit(HCI_QUIRK_RESET_ON_CLOSE, &hdev->quirks);
#endif
	hdev->bus = HCI_SDIO;

	HCI_SET_DRV_DATA(hdev, data);

	if (id->class == SDIO_CLASS_BT_AMP)
		hdev->dev_type = HCI_AMP;
	else
		hdev->dev_type = HCI_BREDR;

	data->hdev = hdev;

	SET_HCIDEV_DEV(hdev, &func->dev);

	hdev->open	   = bcmbtsdio_open;
	hdev->close    = bcmbtsdio_close;
	hdev->flush    = bcmbtsdio_flush;
	hdev->send	   = bcmbtsdio_send_frame;
#ifdef CUSTOMER_HW4
	hdev->setup	   = bcmbtsdio_setup;
	hdev->cmd_timeout = bcmbtsdio_cmd_timeout;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	hdev->destruct = bcmbtsdio_destruct;
	hdev->owner	   = THIS_MODULE;
#endif

	/* schedule work to register hci device */
	schedule_delayed_work(&data->reg_work, 0);

	release:
	BCMBTSDIO_SET_BTWAKE(false, true);
	return ret;
}

static void bcmbtsdio_reg_hci_work(struct work_struct *work)
{
	struct bcmbtsdio_data *data = container_of(work, struct bcmbtsdio_data, reg_work.work);
	int err = -1;

	if (!data)
		return;

	BT_INFO("%s", __FUNCTION__);

	err = hci_register_dev(data->hdev);
	if (err < 0) {
		BT_ERR("%s hci_register_dev failure %d", __FUNCTION__, err);
		hci_free_dev(data->hdev);
		kfree(data);
		return;
	}
	BT_INFO("%s hci_register_dev success", __FUNCTION__);
}

static void bcmbtsdio_dereg_hci_work(struct work_struct *work)
{
	struct bcmbtsdio_data *data = container_of(work, struct bcmbtsdio_data, dereg_work.work);
	struct sdio_func *func = NULL;

	if (!data)
		return;

	BT_INFO("++%s", __FUNCTION__);

	cancel_delayed_work(&data->reg_work);

	func = (struct sdio_func*)data->func;
	sdio_set_drvdata(func, NULL);

	hci_unregister_dev(data->hdev);
	HCI_SET_DRV_DATA(data->hdev, NULL);
	hci_free_dev(data->hdev);
	kfree(data);

	BT_INFO("--%s", __FUNCTION__);

}

static void bcmbtsdio_remove(struct sdio_func *func)
{
	struct bcmbtsdio_data *data = sdio_get_drvdata(func);

	if (!data)
		return;

	BT_INFO("++%s func %p data %p", __FUNCTION__, func, data);

	if (test_bit(HCI_RUNNING, &data->hdev->flags))
	{
		BT_INFO("%s: close not called, forcing it now", __FUNCTION__);
		bcmbtsdio_close(data->hdev);
	}

	/* schedule work to deregister hci device */
	schedule_delayed_work(&data->dereg_work, 0);

	BT_INFO("--%s", __FUNCTION__);
}
static struct sdio_driver bcmbtsdio_driver = {
	.name		= "bcmbtsdio",
	.probe		= bcmbtsdio_probe,
	.remove		= bcmbtsdio_remove,
	.id_table	= bcmbtsdio_table,
};

static int bcmbtsdio_dhd_get(void)
{
	int ret = -1;

	if (g_dhd_handle != NULL) {
		BT_INFO("%s: BT Already powered On", __FUNCTION__);
		return ret;
	}
	g_dhd_handle = dhd_bt_get_pub_hndl();

	if (g_dhd_handle == NULL) {
		BT_ERR("%s: g_dhd_handle NULL", __FUNCTION__);
		return ret;
	}
	dhd_bus_reset_bt_use_count(g_dhd_handle);

	/* WL RG_ON and FW Download */
	if ((ret = dhd_bus_get(g_dhd_handle, BT_MODULE)) != 0) {
		BT_ERR("%s: dhd_bus_get failed ret %d", __FUNCTION__, ret);
		dhd_bus_put(g_dhd_handle, BT_MODULE);
		g_dhd_handle = NULL;
	}
	return ret;
}
static void bcmbtsdio_dhd_put(void)
{
	/* Fuctions related to DHD off sequence can be added here */

	if (!g_dhd_handle)
		return;

	if (dhd_bus_put(g_dhd_handle, BT_MODULE) != 0)
		BT_ERR("%s: dhd_bus_put failed", __FUNCTION__);

	dhd_bus_reset_bt_use_count(g_dhd_handle);

	g_dhd_handle = NULL;
}

static int bcmbtsdio_handle_powering_on(void)
{
	if (bcmbtsdio_dhd_get() != 0)
		return -1;

	BCMBTSDIO_SET_BTWAKE(true, true);

	return 0;
}

static int bcmbtsdio_handle_powered_on(void)
{
	int ret = -1;
	BT_INFO("++%s", __FUNCTION__);

	/* Wait for the BT ROM to boot till it goes past the WL_WAKE clear stage This is to
	 * ensure that we have WL_WAKE bit set throughtout when the download is happening
	 */
	msleep(FW_ROM_BOOT_DELAY);
	/* BT FW Download */
	ret = dhd_download_btfw(g_dhd_handle, g_btfw_path);
	if (ret < 0) {
		BT_ERR("%s: failed to download btfw from: %s",
			__FUNCTION__, g_btfw_path);
		return ret;
	}
	/* Worst case Warm Reboot delay in milliseconds */
	msleep(FW_READY_DELAY);

	ret = sdio_register_driver(&bcmbtsdio_driver);

	if (ret == 0)
		g_bt_sdio_registered = true;

	BT_INFO("--%s ret = %d", __FUNCTION__, ret);
	return ret;
}

static int bcmbtsdio_handle_powering_off(void)
{
	int ret = -1;

	BT_INFO("++%s", __FUNCTION__);

	if ((!g_dhd_handle) && (g_state != BT_HANG_RECOVERY)) {
		BT_ERR("%s: g_dhd_handle NULL", __FUNCTION__);
		return ret;
	}

	if (g_bt_sdio_registered) {
		sdio_unregister_driver(&bcmbtsdio_driver);
		g_bt_sdio_registered = false;
	} else
		BT_ERR("Skip sdio_unregister");

	return 0;
}

static void bcmbtsdio_handle_powered_off(void)
{
	/* Fuctions related to off sequence can be added here */
	BT_INFO("++%s", __FUNCTION__);

	BCMBTSDIO_SET_BTWAKE(false, true);

	bcmbtsdio_dhd_put();

	BT_INFO("--%s", __FUNCTION__);
}

int bcmbtsdio_power_state_handler(int state)
{
	int ret = 0;
	BT_INFO("++%s state %d", __FUNCTION__, state);
	switch (state) {
		case BT_POWERING_ON:
			ret = bcmbtsdio_handle_powering_on();
			break;
		case BT_POWERED_ON:
			bcmbtsdio_handle_powered_on();
			break;
		case BT_POWERING_OFF:
			ret = bcmbtsdio_handle_powering_off();
			break;
		case BT_POWERED_OFF:
			bcmbtsdio_handle_powered_off();
			break;
	}
	/* Avoid g_state assigned with a bad state */
	if (ret == 0)
		g_state = state;
	BT_INFO("--%s ", __FUNCTION__);
	return ret;
}

static int __init bcmbtsdio_init(void)
{
	int ret = 0;
	BT_INFO("Generic Bluetooth SDIO driver ver %s", VERSION);

	sema_init(&work_lock, 1);
	bt_power_state_register((bt_power_change_t)bcmbtsdio_power_state_handler);

	BT_INFO("--%s", __FUNCTION__);
	g_tx_intr_status = TX_STATUS_SEND_NONE;

	return ret;
}

static void __exit bcmbtsdio_exit(void)
{
	BT_INFO("%s", __FUNCTION__);
	bt_power_state_register(NULL);
}

module_init(bcmbtsdio_init);
module_exit(bcmbtsdio_exit);

module_param(g_max_packet_len, int, S_IRUGO);

module_param_string(g_btfw_path, g_btfw_path, MOD_PARAM_PATHLEN, 0660);

MODULE_AUTHOR("Marcel Holtmann <marcel@holtmann.org>");
MODULE_DESCRIPTION("Generic Bluetooth SDIO driver ver " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");
