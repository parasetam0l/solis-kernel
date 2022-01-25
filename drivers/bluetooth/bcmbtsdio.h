/*
 *
 * Copyright (C) 1999-2016, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 * $Id:$
 *
 */

#ifndef	_BCMBTSDIO_H
#define	_BCMBTSDIO_H

typedef enum {
	BT_POWERING_ON = 0,
	BT_POWERED_ON,
	BT_POWERING_OFF,
	BT_POWERED_OFF,
	BT_HANG_RECOVERY
} bt_power_state_t;

typedef enum {
	TX_STATUS_SEND_NONE = 0,
	TX_STATUS_SEND_READY,
	TX_STATUS_SENDING,
	TX_STATUS_SEND_DONE
} sdio_tx_state_t;

/* io_en */
#define SDIO_FUNC_ENABLE_1	0x02    /* function 1 I/O enable */
#define SDIO_FUNC_ENABLE_2	0x04    /* function 2 I/O enable */
#define SDIO_FUNC_ENABLE_3	0x08    /* function 2 I/O enable */
#define SDIO_FUNC_DISABLE_3 0xF0

#define VERSION "0.6"
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define HCI_DIAG_PKT 0x07
#define HCI_EV_HARDWARE_ERROR 0x10
#endif

#ifdef CUSTOMER_HW4
#define HCI_VSC_WRITE_SLEEP_MODE                0xFC27
#define HCI_VSC_DBFW				0xFD1C

#define IDLE_THRESHOLD_HOST 0x0a /* 125ms */
#define IDLE_THRESHOLD_HC   0x0a /* 125ms */
#define BT_WAKE_DELAY_NS  ((IDLE_THRESHOLD_HOST * 125 / 10) * (NSEC_PER_SEC /1000 /* msec */))
#endif

/* misc defines */
#define SDIO_FUNC_0 0
#define SDIO_FUNC_1 1
#define SDIO_FUNC_2 2
#define SDIO_FUNC_3 3

/* SDIO Device CCCR offsets */
#define SDIOD_CCCR_IOEN 0x02
#define SDIOD_CCCR_IORDY 0x03
#define SDIOD_CCCR_INTEN	0x04
#define SDIOD_CCCR_INTPEND	0x05

#define SDIOD_F3_INTERRUPT_PENDING  0x00013 /* Interrupt Pending */
#define SDIOD_F3_INTERRUPT_ENABLE   0x00014 /* Interrupt Enable */

#define REG_RDAT	 0x00	/* Receiver Data */
#define REG_TDAT	 0x00	/* Transmitter Data */
#define REG_PC_RRT	 0x10	/* Read Packet Control */
#define REG_PC_WRT	 0x11	/* Write Packet Control */
#define REG_RTC_STAT 0x12	/* Retry Control Status */
#define REG_RTC_SET  0x12	/* Retry Control Set */
#define REG_INTRD	 0x13	/* Interrupt Indication */
#define REG_CL_INTRD 0x13	/* Interrupt Clear */
#define REG_EN_INTRD 0x14	/* Interrupt Enable */
#define REG_MD_STAT  0x20	/* Bluetooth Mode Status */

#define BTSDIO_INT_TX_BIT	0x10
#define BTSDIO_INT_RX_BIT	0x20
#define BTSDIO_INT_BITS	(BTSDIO_INT_TX_BIT | BTSDIO_INT_RX_BIT)

#define PACKET_HEADER 4
#define CONTINUE_PACKET_MASK (1 << 7)
#define MAX_ACL_SIZE 1050
#define MOD_PARAM_PATHLEN	256
#define TX_FRAG_LEN		448
#define FW_IORDY_DELAY	10 /* in msecs */
#define FW_READY_DELAY 100 /* Worst case Warm Reboot delay in milliseconds */
#define FW_ROM_BOOT_DELAY 100 /* in msecs */
#define FW_IORDY_CNT 300 /* wait for 3000msec till F3 is enabled */

typedef int (*bt_power_change_t)(int);

#endif /* _BCMBTSDIO_H */
