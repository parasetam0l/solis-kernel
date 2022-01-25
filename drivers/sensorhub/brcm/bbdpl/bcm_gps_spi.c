/******************************************************************************
 * Copyright (C) 2015 Broadcom Corporation
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 ******************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/sysrq.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/kthread.h>
#include <linux/circ_buf.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include <linux/kernel.h>
#include <linux/ssp_platformdata.h>
#include <linux/gpio.h>
//#include <plat/gpio-cfg.h>
#include <linux/of_gpio.h>
#include <linux/miscdevice.h>
#include <linux/time.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/kernel_stat.h>
#include <linux/smp.h>
#include <linux/sched.h>

#include "bbd.h"

#define WORD_BURST_SIZE			4
#define CONFIG_SPI_DMA_BYTES_PER_WORD	4
#define CONFIG_SPI_DMA_BITS_PER_WORD	32

#define SSI_MODE_STREAM		0x00
#define SSI_MODE_DEBUG		0x80

#define SSI_MODE_HALF_DUPLEX	0x00
#define SSI_MODE_FULL_DUPLEX	0x40

#define SSI_WRITE_TRANS		0x00
#define SSI_READ_TRANS		0x20

#define SSI_WRITE_HD (SSI_WRITE_TRANS | SSI_MODE_HALF_DUPLEX)
#define SSI_READ_HD  (SSI_READ_TRANS  | SSI_MODE_HALF_DUPLEX)

#define DEBUG_TIME_STAT

#ifdef CONFIG_SENSORS_SSP_BBD
extern void bbd_parse_asic_data(unsigned char *pucData, unsigned short usLen, void (*to_gpsd)(unsigned char *packet, unsigned short len, void* priv), void* priv);
#endif

bool ssi_dbg;

//--------------------------------------------------------------
//
//               Structs
//
//--------------------------------------------------------------

#define BCM_SPI_READ_BUF_SIZE	(8*PAGE_SIZE)
#define BCM_SPI_WRITE_BUF_SIZE	(8*PAGE_SIZE)


#define MAX_SPI_FRAME_LEN 254
struct bcm_ssi_tx_frame
{
	unsigned char cmd;
	unsigned char data[MAX_SPI_FRAME_LEN-1];
} __attribute__((__packed__));

struct bcm_ssi_rx_frame
{
	unsigned char status;
	unsigned char len;
	unsigned char data[MAX_SPI_FRAME_LEN-2];
} __attribute__((__packed__));


struct bcm_spi_priv
{
	struct spi_device *spi;

	/* Char device stuff */
	struct miscdevice misc;
	bool busy;
	struct circ_buf read_buf;
	struct circ_buf write_buf;
	struct mutex rlock;			/* Lock for read_buf */
	struct mutex wlock;			/* Lock for write_buf */
	char _read_buf[BCM_SPI_READ_BUF_SIZE];
	char _write_buf[BCM_SPI_WRITE_BUF_SIZE];
	wait_queue_head_t poll_wait;		/* for poll */

	/* GPIO pins */
	int host_req;
	int mcu_req;
	int mcu_resp;

	/* IRQ and its control */
	atomic_t irq_enabled;
	spinlock_t irq_lock;
	spinlock_t dbg_lock;

	/* Work */
	struct work_struct rxtx_work;
	struct workqueue_struct *serial_wq;
	atomic_t suspending;

	/* SPI tx/rx buf */
	struct bcm_ssi_tx_frame *tx_buf;
	struct bcm_ssi_rx_frame *rx_buf;

	struct wake_lock bcm_wake_lock;

	/* some chip-set(BCM4775) needs to skip sanity-checking */
	bool skip_sanity;
};

static struct bcm_spi_priv *g_bcm_gps;

static bool bbd_ready_done;

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
#define SSP_DEBUG_MAX_MSG	256UL
#define SSP_DEBUG_MSG_LEN	128UL

struct ssp_packet_dump {
	unsigned int dbg_idx;
	char buf[SSP_DEBUG_MAX_MSG][SSP_DEBUG_MSG_LEN];

	char *dp_name;
	char *cp_ubuf;
	bool src_ubuf;
	rwlock_t rw_lock;
};

static char cp_ubuf[SSP_DEBUG_MSG_LEN];
static struct ssp_packet_dump ssp_pkt_dp[2] = {
	{
		.dp_name = "bbd_send_pkt",
		.rw_lock = __RW_LOCK_UNLOCKED(bbd_lock),
	},
	{
		.dp_name = "bcm_spi_wpkt",
		.rw_lock = __RW_LOCK_UNLOCKED(bcm_lock),
		.cp_ubuf = cp_ubuf,
		.src_ubuf = true,
	},
};

void ssp_pkt_dump(const void *buf, const char __user *ubuf, size_t size)
{
	struct ssp_packet_dump *pkt_dp;
	unsigned long flags;
	unsigned long long s;
	unsigned long nsec;
	size_t raw_dump_sz;
	unsigned char idx;
	const void *base_buf;
	int wsz;

	if (!bbd_ready_done)
		return;

	if (ubuf) {
		idx = 1;
		if (size > SSP_DEBUG_MSG_LEN)
			raw_dump_sz = SSP_DEBUG_MSG_LEN;
		else
			raw_dump_sz = size;

		if (copy_from_user((void *)ssp_pkt_dp[idx].cp_ubuf,
				ubuf, raw_dump_sz)) {
			pr_warn("[SSPBCM] %s: raw_data cpy_fuser fail!!(sz:%lu)\n",
				__func__, size);
			return;
		}
		base_buf = (const void *)ssp_pkt_dp[idx].cp_ubuf;
	} else if (buf) {
		idx = 0;
		base_buf = buf;
	} else {
		pr_err("[SSPBCM] %s: NULL buf!!\n", __func__);
		return;
	}

	pkt_dp = &ssp_pkt_dp[idx];

	write_lock_irqsave(&pkt_dp->rw_lock, flags);
	s = cpu_clock(smp_processor_id());
	nsec = do_div(s, 1000000000)/1000;

	wsz = scnprintf(pkt_dp->buf[pkt_dp->dbg_idx], SSP_DEBUG_MSG_LEN,
			"[%llu.%06lu]:SZ:%lu, ", s, nsec, size);
	if ((wsz) > 0 && (wsz < SSP_DEBUG_MSG_LEN)) {
		hex_dump_to_buffer(base_buf, size, 32, 1,
			(pkt_dp->buf[pkt_dp->dbg_idx] + wsz),
				(SSP_DEBUG_MSG_LEN - wsz), false);

		pkt_dp->dbg_idx++;
		pkt_dp->dbg_idx = pkt_dp->dbg_idx % SSP_DEBUG_MAX_MSG;
	}
	write_unlock_irqrestore(&pkt_dp->rw_lock, flags);
}

static void dbg_inc(unsigned int *idx)
{
	*idx = (*idx + 1) & (SSP_DEBUG_MAX_MSG-1);
}

void print_ssp_pkt_dump(const char *dp_name)
{
	struct ssp_packet_dump *pkt_dp;
	unsigned long flags;
	unsigned int i;
	unsigned int idx;
	int find_dp = 0;

	for (i = 0; i < ARRAY_SIZE(ssp_pkt_dp); i++) {
		if (!strncmp(ssp_pkt_dp[i].dp_name,
				dp_name, strnlen(dp_name, 16))) {
			find_dp = 1;
			idx = i;
			break;
		}
	}

	if (!find_dp) {
		pr_err("[SSPBCM] %s: can't find dump for %s\n",
			__func__, dp_name);
		return;
	}

	pkt_dp = &ssp_pkt_dp[idx];

	read_lock_irqsave(&pkt_dp->rw_lock, flags);
	i = pkt_dp->dbg_idx;

	pr_info("[SSPBCM] Start %s dump_print --->\n", pkt_dp->dp_name);
	if (strnlen(pkt_dp->buf[i], SSP_DEBUG_MSG_LEN))
		pr_info("[SSPBCM] %s: %s\n", pkt_dp->dp_name, pkt_dp->buf[i]);

	for (dbg_inc(&i); i != pkt_dp->dbg_idx; dbg_inc(&i)) {
		if (!strnlen(pkt_dp->buf[i], SSP_DEBUG_MSG_LEN))
			continue;

		pr_info("[SSPBCM] %s: %s\n", pkt_dp->dp_name, pkt_dp->buf[i]);
	}

	read_unlock_irqrestore(&pkt_dp->rw_lock, flags);
}

#else

void ssp_pkt_dump(const void *buf, const char __user *ubuf, size_t size) {}
void print_ssp_pkt_dump(const char *dp_name) {}
#endif

//--------------------------------------------------------------
//
//               File Operations
//
//--------------------------------------------------------------
static int bcm_spi_open(struct inode *inode, struct file *filp)
{
	/* Initially, file->private_data points device itself and we can get our priv structs from it. */
	struct bcm_spi_priv *priv = container_of(filp->private_data, struct bcm_spi_priv, misc);
	unsigned long int flags;

	pr_info("%s++\n", __func__);

	if (priv->busy)
		return -EBUSY;

	priv->busy = true;

	/* Reset circ buffer */
	priv->read_buf.head = priv->read_buf.tail = 0;
	priv->write_buf.head = priv->write_buf.tail = 0;


	/* Enable irq */
	spin_lock_irqsave( &priv->irq_lock, flags);
	if (!atomic_xchg(&priv->irq_enabled, 1))
		enable_irq(priv->spi->irq);

	spin_unlock_irqrestore( &priv->irq_lock, flags);

	enable_irq_wake(priv->spi->irq);

	filp->private_data = priv;
#ifdef DEBUG_1HZ_STAT
	bbd_enable_stat();
#endif
	pr_info("%s--\n", __func__);
	return 0;
}

static int bcm_spi_release(struct inode *inode, struct file *filp)
{
	struct bcm_spi_priv *priv = filp->private_data;
	unsigned long int flags;

	pr_info("%s++\n", __func__);
	priv->busy = false;

#ifdef DEBUG_1HZ_STAT
	bbd_disable_stat();
#endif
	/* Disable irq */
	spin_lock_irqsave( &priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(priv->spi->irq);

	spin_unlock_irqrestore( &priv->irq_lock, flags);

	disable_irq_wake(priv->spi->irq);

	pr_info("%s--\n", __func__);
	return 0;
}

int bcm_spi_rw_buf_cnt(int rw)
{
	struct bcm_spi_priv *priv = NULL;
	struct circ_buf *circ = NULL;
	struct mutex *mlock;
	unsigned int max_buf_size;
	int remain_cnt;

	if (!g_bcm_gps)
		return -1;

	priv = g_bcm_gps;
	if (rw) {
		circ = &g_bcm_gps->write_buf;
		mlock = &priv->wlock;
		max_buf_size = BCM_SPI_WRITE_BUF_SIZE;
	} else {
		circ = &g_bcm_gps->read_buf;
		mlock = &priv->rlock;
		max_buf_size = BCM_SPI_READ_BUF_SIZE;
	}

	mutex_lock(mlock);
	remain_cnt = CIRC_CNT(circ->head, circ->tail, max_buf_size);
	mutex_unlock(mlock);

	return remain_cnt;
}

static ssize_t bcm_spi_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	struct bcm_spi_priv *priv = filp->private_data;
	struct circ_buf *circ = &priv->read_buf;
	size_t rd_size=0;

	//pr_info("[SSPBBD] %s++\n", __func__);
	mutex_lock(&priv->rlock);

	/* Copy from circ buffer to user
	 * We may require 2 copies from [tail..end] and [end..head]
	 */
	do {
		size_t cnt_to_end = CIRC_CNT_TO_END(circ->head, circ->tail, BCM_SPI_READ_BUF_SIZE);
		size_t copied = min(cnt_to_end, size);

		WARN_ON(copy_to_user(buf + rd_size, (void*) circ->buf + circ->tail, copied));
		size -= copied;
		rd_size += copied;
		circ->tail = (circ->tail + copied) & (BCM_SPI_READ_BUF_SIZE-1);

	} while (size>0 && CIRC_CNT(circ->head, circ->tail, BCM_SPI_READ_BUF_SIZE));
	mutex_unlock(&priv->rlock);

#ifdef DEBUG_1HZ_STAT
	bbd_update_stat(STAT_RX_LHD, rd_size);
#endif

	//pr_info("[SSPBBD] %s--(%zd bytes)\n", __func__, rd_size);
	return rd_size;
}

static ssize_t bcm_spi_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	struct bcm_spi_priv *priv = filp->private_data;
	struct circ_buf *circ = &priv->write_buf;
	size_t wr_size=0;

	//pr_info("[SSPBBD] %s++(%zd bytes)\n", __func__, size);
	ssp_pkt_dump(NULL, buf, size);

	mutex_lock(&priv->wlock);
	/* Copy from user into circ buffer
	 * We may require 2 copies from [tail..end] and [end..head]
	 */
	do {
		size_t space_to_end = CIRC_SPACE_TO_END(circ->head, circ->tail, BCM_SPI_WRITE_BUF_SIZE);
		size_t copied = min(space_to_end, size);


		WARN_ON(copy_from_user((void*) circ->buf + circ->head, buf + wr_size, copied));
		size -= copied;
		wr_size += copied;
		circ->head = (circ->head + copied) & (BCM_SPI_WRITE_BUF_SIZE-1);
	} while (size>0 && CIRC_SPACE(circ->head, circ->tail, BCM_SPI_WRITE_BUF_SIZE));

	mutex_unlock(&priv->wlock);

	//pr_info("[SSPBBD] %s--(%zd bytes)\n", __func__, wr_size);

	/* kick start rxtx thread */
	/* we don't want to queue work in suspending and shutdown */
	if (!atomic_read(&priv->suspending))
		queue_work(priv->serial_wq, &(priv->rxtx_work) );

#ifdef DEBUG_1HZ_STAT
	bbd_update_stat(STAT_TX_LHD, wr_size);
#endif
	return wr_size;
}

static unsigned int bcm_spi_poll(struct file *filp, poll_table *wait)
{
	struct bcm_spi_priv *priv = filp->private_data;
	struct circ_buf *rd_circ = &priv->read_buf;
	struct circ_buf *wr_circ = &priv->write_buf;
	unsigned int mask = 0;

	poll_wait(filp, &priv->poll_wait, wait);

	if (CIRC_CNT(rd_circ->head, rd_circ->tail, BCM_SPI_READ_BUF_SIZE))
		mask |= POLLIN;

	if (CIRC_SPACE(wr_circ->head, wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE))
		mask |= POLLOUT;

	return mask;
}


static const struct file_operations bcm_spi_fops = {
	.owner          =  THIS_MODULE,
	.open           =  bcm_spi_open,
	.release        =  bcm_spi_release,
	.read           =  bcm_spi_read,
	.write          =  bcm_spi_write,
	.poll           =  bcm_spi_poll,
};



//--------------------------------------------------------------
//
//               Misc. functions
//
//--------------------------------------------------------------

/*
 * bcm477x_hello - wakeup chip by toggling mcu_req while
 * monitoring mcu_resp to check if awake
 */
static bool bcm477x_hello(struct bcm_spi_priv *priv)
{
	int count=0, retries=0;

	gpio_set_value(priv->mcu_req, 1);
	while (!gpio_get_value(priv->mcu_resp)) {
		if (count++ > 100) {
			gpio_set_value(priv->mcu_req, 0);
			printk("[SSPBBD] MCU_REQ_RESP timeout. MCU_RESP(gpio%d) not responding to MCU_REQ(gpio%d)\n", 
					priv->mcu_resp, priv->mcu_req);
			return false;
		}

		mdelay(1);

		/*if awake, done */
		if (gpio_get_value(priv->mcu_resp)) break;

		if (count%20==0 && retries++ < 3) {
			gpio_set_value(priv->mcu_req, 0);
			mdelay(1);
			gpio_set_value(priv->mcu_req, 1);
			mdelay(1);
		}
	}
	return true;
}

/*
 * bcm477x_bye - set mcu_req low to let chip go to sleep
 *
 */
static void bcm477x_bye(struct bcm_spi_priv *priv)
{
	gpio_set_value(priv->mcu_req, 0);
}


static unsigned long init_time = 0;
static unsigned long clock_get_ms(void)
{
	struct timeval t;
	unsigned long now;

	do_gettimeofday(&t);
	now = t.tv_usec / 1000 + t.tv_sec * 1000;
	if ( init_time == 0 )
		init_time = now;

	return now - init_time;
}


static void pk_log(char* dir, unsigned char* data, int len)
{
	char acB[960];
	char *p = acB;
	int  the_trans = len;
	int i,j,n;

	char ic = 'D';
	char xc = dir[0] == 'r' || dir[0] == 'w' ? 'x':'X';

	if (likely(!ssi_dbg))
		return;

	//FIXME: There is print issue. Printing 7 digits instead of 6 when clock is over 1000000. "% 1000000" added
	// E.g.
	//#999829D w 0x68,     1: A2
	//#999829D r 0x68,    34: 8D 00 01 52 5F B0 01 B0 00 8E 00 01 53 8B B0 01 B0 00 8F 00 01 54 61 B0 01 B0 00 90 00 01 55 B5
	//         r              B0 01
	//#1000001D w 0x68,     1: A1
	//#1000001D r 0x68,     1: 00
	n = len;
	p += snprintf(acB,sizeof(acB),"#%06ld%c %2s,      %5d: ",
			clock_get_ms() % 1000000,ic, dir, n);

	for (i=0, n=32; i<len; i=j, n=32)
	{
		for(j = i; j < (i + n) && j < len && the_trans != 0; j++,the_trans--) {
			p += snprintf(p,sizeof(acB) - (p - acB), "%02X ", data[j]);
		}
		pr_info("%s\n",acB);
		if(j < len) {
			p = acB;
			if ( the_trans == 0 )  { dir[0] = xc; the_trans--; }
			p += snprintf(acB,sizeof(acB),"         %2s              ",dir);
		}
	}

	if (i==0)
		pr_info("%s\n",acB);
}



//--------------------------------------------------------------
//
//               SSI tx/rx functions
//
//--------------------------------------------------------------
static int bcm_spi_sync(struct bcm_spi_priv *priv, void *tx_buf, void *rx_buf, int len, int bits_per_word)
{
	struct spi_message msg;
	struct spi_transfer xfer;
	int ret;

	/* Init */
	spi_message_init(&msg);
	memset(&xfer, 0, sizeof(xfer));
	spi_message_add_tail(&xfer, &msg);

	/* Setup */
	msg.spi = priv->spi;
	xfer.len = len; 
	xfer.tx_buf = tx_buf;
	xfer.rx_buf = rx_buf;

	/* Sync */
	pk_log("w", (unsigned char *)xfer.tx_buf, len);
	ret = spi_sync(msg.spi, &msg);
	pk_log("r", (unsigned char *)xfer.rx_buf, len);

	if (ret)
		pr_err("[SSPBBD] spi_sync error for cmd:0x%x, return=%d\n",
			((struct bcm_ssi_tx_frame *)xfer.tx_buf)->cmd, ret);

	return ret;
}


static int bcm_ssi_tx(struct bcm_spi_priv *priv, int length)
{
	struct bcm_ssi_tx_frame *tx = priv->tx_buf;
	struct bcm_ssi_rx_frame *rx = priv->rx_buf;
	int bits_per_word = (length >= 255) ? CONFIG_SPI_DMA_BITS_PER_WORD : 8;
	int ret;

	//	pr_info("[SSPBBD]: %s ++ (%d bytes)\n", __func__, length);

	tx->cmd = SSI_WRITE_HD;

	ret = bcm_spi_sync(priv, tx, rx, length+1, bits_per_word); //1 for tx cmd

	//	pr_info("[SSPBBD]: %s --\n", __func__);
	return ret;
}

static int bcm_ssi_rx( struct bcm_spi_priv *priv, size_t *length)
{
	struct bcm_ssi_tx_frame *tx = priv->tx_buf;
	struct bcm_ssi_rx_frame *rx = priv->rx_buf;

	//	pr_info("[SSPBBD]: %s ++\n", __func__);

	memset(tx, 0, MAX_SPI_FRAME_LEN);
	tx->cmd = SSI_READ_HD;
	rx->status = 0;
	if (bcm_spi_sync(priv, tx, rx, 2, 8))  // 2 for rx status + len
		return -1;

	/* Check Sanity */
	if (!(priv->skip_sanity) && rx->status) {
		pr_err("[SSPBBD] spi_sync error, status = 0x%02X\n", rx->status);
		return -1;
	}

	if (rx->len == 0)
	{
		rx->len = MAX_SPI_FRAME_LEN;
		pr_err("[SSPBBD] rx->len is still read to 0. set MAX_SPI_FRAME_LEN\n");
	}

	/* limit max payload to 254 because of exynos3 bug */
	*length = min((unsigned char)(MAX_SPI_FRAME_LEN-2), rx->len);
	if (bcm_spi_sync(priv, tx, rx, *length+2, 8)) // 2 for rx status + len
		return -1;

	/* Check Sanity */
	if (!(priv->skip_sanity) && rx->status) {
		pr_err("[SSPBBD] spi_sync error, status = 0x%02X\n", rx->status);
		return -1;
	}

	if (rx->len < *length) {
		//pr_err("[SSPBBD]: %s read error. Expected %zd but read %d\n", __func__, *length, rx->len);
		*length = rx->len; //workaround
		//return -1;
	}

#ifdef DEBUG_1HZ_STAT
	bbd_update_stat(STAT_RX_SSI, *length);
#endif
	//	pr_info("[SSPBBD]: %s -- (%d bytes)\n", __func__, *length);

	return 0;
}

void bcm_on_packet_recieved(void *_priv, unsigned char *data, unsigned int size)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *)_priv;
	struct circ_buf *rd_circ = &priv->read_buf;
	size_t written=0, avail = size;

	//pr_info("[SSPBBD]: %s ++\n", __func__);
	/* Copy into circ buffer */
	mutex_lock(&priv->rlock);
	do {
		size_t space_to_end = CIRC_SPACE_TO_END(rd_circ->head, rd_circ->tail, BCM_SPI_READ_BUF_SIZE);
		size_t copied = min(space_to_end, avail);

		memcpy((void*) rd_circ->buf + rd_circ->head, data + written, copied);
		avail -= copied;
		written += copied;
		rd_circ->head = (rd_circ->head + copied) & (BCM_SPI_READ_BUF_SIZE-1);

	} while (avail>0 && CIRC_SPACE(rd_circ->head, rd_circ->tail, BCM_SPI_READ_BUF_SIZE));
	mutex_unlock(&priv->rlock);
	wake_up(&priv->poll_wait);

	if (avail>0)
		pr_err("[SSPBBD]: input overrun error by %zd bytes!\n", avail);

	//pr_info("[SSPBBD]: %s received -- (%d bytes)\n", __func__, size);
}

static void bcm_rxtx_work( struct work_struct *work )
{
#ifdef DEBUG_1HZ_STAT
	u64 ts_rx_start = 0;
	u64 ts_rx_end = 0;
#endif
	struct bcm_spi_priv *priv = container_of(work, struct bcm_spi_priv, rxtx_work);
	struct circ_buf *wr_circ = &priv->write_buf;

	if (!bcm477x_hello(priv)) {
		pr_err("[SSPBBD]: %s timeout!!\n", __func__);
		return;
	}

	do {
		/* Read first */
		if (gpio_get_value(priv->host_req)) {
			size_t avail;
#ifdef DEBUG_1HZ_STAT
			struct timespec ts;
			if (stat1hz.ts_irq) {
				ts = ktime_to_timespec(ktime_get_boottime());
				ts_rx_start = ts.tv_sec * 1000000000ULL
					+ ts.tv_nsec;
			}
#endif
			/* Receive SSI frame */
			if (bcm_ssi_rx(priv, &avail))
				break;

#ifdef DEBUG_1HZ_STAT
			bbd_update_stat(STAT_RX_SSI, avail);
			if (ts_rx_start && !gpio_get_value(priv->host_req)) {
				ts = ktime_to_timespec(ktime_get_boottime());
				ts_rx_end = ts.tv_sec * 1000000000ULL
					+ ts.tv_nsec;
			}
#endif
			/* Call BBD */
			bbd_parse_asic_data(priv->rx_buf->data, avail, NULL, priv);
		}

		/* Next, write */
		if (CIRC_CNT(wr_circ->head, wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE)) {
			size_t written=0, avail=0;

			mutex_lock(&priv->wlock);
			avail = CIRC_CNT(wr_circ->head, wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE);
			/* For big packet, we should align xfer size to DMA word size and burst size.
			 * That is, SSI payload + one byte command should be multiple of (DMA word size * burst size)
			 */
			//if (avail + 1 > 256)
			//	avail -= (avail % (CONFIG_SPI_DMA_BYTES_PER_WORD * WORD_BURST_SIZE)) + 1; //1 for "1 byte cmd"
			if (avail > MAX_SPI_FRAME_LEN - 1)
			       	avail = MAX_SPI_FRAME_LEN - 1;

			/* Copy from wr_circ the data */
			do {
				size_t cnt_to_end = CIRC_CNT_TO_END(wr_circ->head, wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE);
				size_t copied = min(cnt_to_end, avail);

				//pr_info("[SSPBBD]: %s writing ++ (%d bytes)\n", __func__, copied);

				memcpy(priv->tx_buf->data + written, wr_circ->buf + wr_circ->tail, copied);
				avail -= copied;
				written += copied;
				wr_circ->tail = (wr_circ->tail + copied) & (BCM_SPI_WRITE_BUF_SIZE-1);
				//pr_info("[SSPBBD]: %s writing --\n", __func__);
			} while (avail>0);
			mutex_unlock(&priv->wlock);

			/* Transmit SSI frame */
			if (bcm_ssi_tx(priv, written))
				break;

			wake_up(&priv->poll_wait);
#ifdef DEBUG_1HZ_STAT
			bbd_update_stat(STAT_TX_SSI, written);
#endif
		}

	} while (gpio_get_value(priv->host_req) || CIRC_CNT(wr_circ->head, wr_circ->tail, BCM_SPI_WRITE_BUF_SIZE));

	bcm477x_bye(priv);

	/* Enable irq */
	{
		unsigned long int flags;
		spin_lock_irqsave( &priv->irq_lock, flags);

		/* we dont' want to enable irq when going to suspending */
		if (!atomic_read(&priv->suspending))
			if (!atomic_xchg(&priv->irq_enabled, 1))
				enable_irq(priv->spi->irq);

		spin_unlock_irqrestore( &priv->irq_lock, flags);
	}

#ifdef DEBUG_1HZ_STAT
	if (stat1hz.ts_irq && ts_rx_start && ts_rx_end) {
		u64 lat = ts_rx_start - stat1hz.ts_irq;
		u64 dur = ts_rx_end - ts_rx_start;
		stat1hz.min_rx_lat = (lat < stat1hz.min_rx_lat) ?
			lat : stat1hz.min_rx_lat;
		stat1hz.max_rx_lat = (lat > stat1hz.max_rx_lat) ?
			lat : stat1hz.max_rx_lat;
		stat1hz.min_rx_dur = (dur < stat1hz.min_rx_dur) ?
			dur : stat1hz.min_rx_dur;
		stat1hz.max_rx_dur = (dur > stat1hz.max_rx_dur) ?
			dur : stat1hz.max_rx_dur;
		stat1hz.ts_irq = 0;
	}
#endif
}


//--------------------------------------------------------------
//
//               IRQ Handler
//
//--------------------------------------------------------------

static unsigned long long bcm_irq_sec;
static unsigned long bcm_irq_nsec;

void get_bcm_irq_time_last(unsigned long long *sec,
		unsigned long *nanosec)
{
	unsigned long int flags;

	if (!g_bcm_gps) {
		*sec = 0;
		*nanosec = 0;
		return;
	}

	spin_lock_irqsave(&g_bcm_gps->dbg_lock, flags);
	*sec = bcm_irq_sec;
	*nanosec = bcm_irq_nsec;
	spin_unlock_irqrestore(&g_bcm_gps->dbg_lock, flags);
}

static irqreturn_t bcm_irq_handler(int irq, void *pdata)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv *) pdata;
	unsigned long int flags;

	if (!gpio_get_value(priv->host_req))
		return IRQ_HANDLED;
#ifdef DEBUG_1HZ_STAT
	{
		struct timespec ts;
		ts = ktime_to_timespec(ktime_get_boottime());
		stat1hz.ts_irq = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
	}
#endif
	/* Disable irq */
	spin_lock(&priv->irq_lock);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(priv->spi->irq);

	spin_unlock(&priv->irq_lock);

	/* we don't want to queue work in suspending and shutdown */
	if (!atomic_read(&priv->suspending))
		queue_work(priv->serial_wq, &priv->rxtx_work);

	wake_lock_timeout(&priv->bcm_wake_lock, HZ/2);

	spin_lock_irqsave(&priv->dbg_lock, flags);
	bcm_irq_sec = cpu_clock(smp_processor_id());
	bcm_irq_nsec = do_div(bcm_irq_sec, 1000000000)/1000;
	spin_unlock_irqrestore(&priv->dbg_lock, flags);

	return IRQ_HANDLED;
}

//--------------------------------------------------------------
//
//               SPI driver operations
//
//--------------------------------------------------------------
static int bcm_spi_suspend(struct spi_device *spi, pm_message_t state)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv*) spi_get_drvdata(spi);
	unsigned long int flags;

	printk("[SSPBBD]: %s ++ \n", __func__);

	atomic_set(&priv->suspending, 1);

	/* Disable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(spi->irq);

	spin_unlock_irqrestore(&priv->irq_lock, flags);

	flush_workqueue(priv->serial_wq);

	printk("[SSPBBD]: %s -- \n", __func__);
	return 0;
}

static int bcm_spi_resume(struct spi_device *spi)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv*) spi_get_drvdata(spi);
	unsigned long int flags;

	pr_info("[SSPBBD]: %s ++ \n", __func__);
	atomic_set(&priv->suspending, 0);

	/* Enable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (!atomic_xchg(&priv->irq_enabled, 1))
		enable_irq(spi->irq);

	spin_unlock_irqrestore( &priv->irq_lock, flags);

	//	wake_lock_timeout(&spi->bcm_wake_lock, HZ/2);

	pr_info("[SSPBBD]: %s -- \n", __func__);
	return 0;
}

static void bcm_spi_shutdown(struct spi_device *spi)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv*) spi_get_drvdata(spi);
	unsigned long int flags;

	printk("[SSPBBD]: %s ++ \n", __func__);

	atomic_set(&priv->suspending, 1);

	/* Disable irq */
	spin_lock_irqsave( &priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(spi->irq);

	spin_unlock_irqrestore( &priv->irq_lock, flags);

	printk("[SSPBBD]: %s ** \n", __func__);

	flush_workqueue(priv->serial_wq );
	destroy_workqueue( priv->serial_wq );

	printk("[SSPBBD]: %s -- \n", __func__);
}

static int bcm_spi_probe(struct spi_device *spi)
{
	int host_req, mcu_req, mcu_resp;
	struct bcm_spi_priv *priv;
	bool skip_sanity;
	bool legacy_patch = false;
	int ret;

	/* Check GPIO# */
#ifndef CONFIG_OF
	pr_err("[SSPBBD]: Check platform_data for bcm device\n");
#else
	if (!spi->dev.of_node) {
		pr_err("[SSPBBD]: Failed to find of_node\n");
		goto err_exit;
	}
#endif

	host_req = of_get_named_gpio(spi->dev.of_node, "ssp-host-req", 0);
	mcu_req  = of_get_named_gpio(spi->dev.of_node, "ssp-mcu-req", 0);
	mcu_resp = of_get_named_gpio(spi->dev.of_node, "ssp-mcu-resp", 0);

	skip_sanity = of_property_read_bool(spi->dev.of_node,
					"ssp-skip-sanity");
#ifdef CONFIG_SENSORS_BBD_LEGACY_PATCH
	legacy_patch = of_property_read_bool(spi->dev.of_node,
					"ssp-legacy-patch");
#endif
/*
	host_req = EXYNOS3_GPX0(2);
	mcu_req  = EXYNOS3_GPX0(0);
	mcu_resp = EXYNOS3_GPX0(4);
*/

	pr_info("[SSPBBD] ssp-host-req=%d, ssp-mcu_req=%d, ssp-mcu-resp=%d\n",
		host_req, mcu_req, mcu_resp);
	pr_info("[SSPBBD] [Option] skip_sanity=%u, use_legacy_patch=%u\n",
		skip_sanity, legacy_patch);

	if (host_req<0 || mcu_req<0 || mcu_resp<0) {
		pr_err("[SSPBBD]: GPIO value not correct\n");
		goto err_exit;
	}

	/* Check IRQ# */
	spi->irq = gpio_to_irq(host_req);
	if (spi->irq < 0) {
		pr_err("[SSPBBD]: irq=%d for host_req=%d not correct\n", spi->irq, host_req);
		goto err_exit;
	}
#if 0
	/* Config GPIO */
	ret = gpio_request(mcu_req, "MCU REQ");
	if (ret){
		pr_err("[SSPBBD]: failed to request MCU REQ, ret:%d", ret);
		goto err_exit;
	}
	ret = gpio_direction_output(mcu_req, 0);
	if (ret) {
		pr_err("[SSPBBD]: failed set MCU REQ as input mode, ret:%d", ret);
		goto err_exit;
	}
	ret = gpio_request(mcu_resp, "MCU RESP");
	if (ret){
		pr_err("[SSPBBD]: failed to request MCU RESP, ret:%d", ret);
		goto err_exit;
	}
	ret = gpio_direction_input(mcu_resp);
	if (ret) {
		pr_err("[SSPBBD]: failed set MCU RESP as input mode, ret:%d", ret);
		goto err_exit;
	}
#endif
	/* Alloc everything */
	priv = (struct bcm_spi_priv*) kmalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		pr_err("[SSPBBD]: Failed to allocate memory for the BCM SPI driver\n");
		goto err_exit;
	}

	memset(priv, 0, sizeof(*priv));

	priv->skip_sanity = skip_sanity;
	priv->spi = spi;

	priv->tx_buf = kmalloc(sizeof(struct bcm_ssi_tx_frame), GFP_KERNEL);
	priv->rx_buf = kmalloc(sizeof(struct bcm_ssi_rx_frame), GFP_KERNEL);
	if (!priv->tx_buf || !priv->rx_buf) {
		pr_err("[SSPBBD]: Failed to allocate xfer buffer. tx_buf=%p, rx_buf=%p\n",
				priv->tx_buf, priv->rx_buf);
		goto free_mem;
	}

	priv->serial_wq =
		alloc_workqueue("bcm477x_wq",
			WQ_HIGHPRI|WQ_UNBOUND|WQ_MEM_RECLAIM, 1);
	if (!priv->serial_wq) {
		pr_err("[SSPBBD]: Failed to allocate workqueue\n");
		goto free_mem;
	}

	/* Request IRQ */
	ret = request_irq(spi->irq, bcm_irq_handler, IRQF_TRIGGER_HIGH, "ttyBCM", priv);
	if (ret) {
		pr_err("[SSPBBD]: Failed to register BCM477x SPI TTY IRQ %d.\n",
			spi->irq);
		goto free_wq;
	}

	disable_irq(spi->irq);

	pr_notice("[SSPBBD]: Probe OK. ssp-host-req=%d, irq=%d, priv=0x%p\n", host_req, spi->irq, priv);

	/* Register misc device */
	priv->misc.minor = MISC_DYNAMIC_MINOR;
	priv->misc.name = "ttyBCM";
	priv->misc.fops = &bcm_spi_fops;

	ret = misc_register(&priv->misc);
	if (ret) {
		pr_err("[SSPBBD]: Failed to register bcm_gps_spi's misc dev. err=%d\n", ret);
		goto free_irq;
	}

	/* Set driver data */
	spi_set_drvdata(spi, priv);

	/* Init - miscdev stuff */
	init_waitqueue_head(&priv->poll_wait);
	priv->read_buf.buf = priv->_read_buf;
	priv->write_buf.buf = priv->_write_buf;
	mutex_init(&priv->rlock);
	mutex_init(&priv->wlock);
	priv->busy = false;

	/* Init - work */
	INIT_WORK(&priv->rxtx_work, bcm_rxtx_work);

	/* Init - irq stuff */
	spin_lock_init(&priv->irq_lock);
	spin_lock_init(&priv->dbg_lock);

	atomic_set(&priv->irq_enabled, 0);
	atomic_set(&priv->suspending, 0);

	/* Init - gpios */
	priv->host_req = host_req;
	priv->mcu_req  = mcu_req;
	priv->mcu_resp = mcu_resp;

	/* Init - etc */
	wake_lock_init(&priv->bcm_wake_lock, WAKE_LOCK_SUSPEND, "bcm_spi_wake_lock");

	g_bcm_gps = priv;
	/* Init BBD & SSP */
	bbd_init(&spi->dev, legacy_patch);

	return 0;

free_irq:
	if (spi->irq)
		free_irq( spi->irq, priv );
free_wq:
	if (priv->serial_wq)
		destroy_workqueue(priv->serial_wq);
free_mem:
	if (priv->tx_buf)
		kfree(priv->tx_buf);
	if (priv->rx_buf)
		kfree(priv->rx_buf);
	if (priv)
		kfree(priv);
err_exit:
	return -ENODEV;
}


static int bcm_spi_remove(struct spi_device *spi)
{
	struct bcm_spi_priv *priv = (struct bcm_spi_priv*) spi_get_drvdata(spi);
	unsigned long int flags;

	pr_notice("[SSPBBD]:  %s : called\n", __func__);

	atomic_set(&priv->suspending, 1);

	bbd_exit();

	/* Disable irq */
	spin_lock_irqsave(&priv->irq_lock, flags);
	if (atomic_xchg(&priv->irq_enabled, 0))
		disable_irq_nosync(spi->irq);

	spin_unlock_irqrestore( &priv->irq_lock, flags);

	/* Flush work */
	flush_workqueue(priv->serial_wq);
	destroy_workqueue(priv->serial_wq);

	/* Free everything */
	free_irq( spi->irq, priv );
	kfree(priv->tx_buf);
	kfree(priv->rx_buf);
	kfree( priv );

	g_bcm_gps = NULL;

	return 0;
}

void bcm477x_debug_info(const char *buf)
{
	int pin_ttyBCM, pin_MCU_REQ, pin_MCU_RESP;
	int irq_enabled, irq_count;

	if (g_bcm_gps) {
		pin_ttyBCM = gpio_get_value(g_bcm_gps->host_req);
		pin_MCU_REQ = gpio_get_value(g_bcm_gps->mcu_req);
		pin_MCU_RESP = gpio_get_value(g_bcm_gps->mcu_resp);

		irq_enabled = atomic_read(&g_bcm_gps->irq_enabled);
		irq_count = kstat_irqs_cpu(g_bcm_gps->spi->irq, 0);

		printk("[SSPBBD]:[%s] pin_ttyBCM:%d, pin_MCU_REQ:%d, pin_MCU_RESP:%d\n",
			buf, pin_ttyBCM, pin_MCU_REQ, pin_MCU_RESP);
		printk("[SSPBBD]:[%s] irq_enabled:%d, irq_count:%d\n",
			buf, irq_enabled, irq_count);
	} else {
		printk("[SSPBBD]:[%s] WARN!! bcm_gps didn't yet init, or removed\n", buf);
	}

	if (!strncmp(ESW_CTRL_READY, buf, 9))
		bbd_ready_done = true;
	else if (!strncmp(ESW_CTRL_NOTREADY, buf, 12))
		bbd_ready_done = false;
}


static const struct spi_device_id bcm_spi_id[] = {
	{"ssp", 0},
	{}
};
MODULE_DEVICE_TABLE(spi, bcm_spi_id);

#ifdef CONFIG_OF
static struct of_device_id match_table[] = {
	{ .compatible = "ssp,BCM4774",},
	{ .compatible = "ssp,BCM4775",},
	{},
};
#endif

static struct spi_driver bcm_spi_driver =
{
	.id_table = bcm_spi_id,
	.probe = bcm_spi_probe,
	.remove = bcm_spi_remove,
	.suspend = bcm_spi_suspend,
	.resume = bcm_spi_resume,
	.shutdown = bcm_spi_shutdown,
	.driver = {
		.name = "ssp",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = match_table
#endif
	},
};


//--------------------------------------------------------------
//
//               Module init/exit
//
//--------------------------------------------------------------
static int __init bcm_spi_init(void)
{
	return spi_register_driver(&bcm_spi_driver);
}

static void __exit bcm_spi_exit(void)
{
	spi_unregister_driver(&bcm_spi_driver);
}

module_init(bcm_spi_init);
module_exit(bcm_spi_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BCM SPI/SSI Driver");

