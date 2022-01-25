/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * modifications copyright (C) 2015 NXP B.V.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
//#include "pn5xx_i2c.h"
#include "sec_nfc.h"
//#include <linux/nfc/sec_nfc.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <asm/siginfo.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/signal.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define SIG_NFC 44
#define MAX_BUFFER_SIZE    512
#define NFC_DEBUG           0

#define MODE_OFF    0
#define MODE_RUN    1
#define MODE_FW     2

/* Only pn548, pn547 and pn544 are supported */
//#define CHIP "pn544"
#define CHIP "sec-nfc"
#define DRIVER_CARD    "PN5xx NFC"
#define DRIVER_DESC    "NFC driver for PN5xx Family"

#ifndef CONFIG_OF
#define CONFIG_OF
#endif

struct pn5xx_dev    {
	wait_queue_head_t    read_wq;
	struct mutex        read_mutex;
	struct i2c_client    *client;
	struct miscdevice   pn5xx_device;
	int                 ven_gpio;
	int                 firm_gpio;
	int                 irq_gpio;
	int                 clkreq_gpio;
	struct regulator *pvdd_reg;
	struct regulator *vbat_reg;
	struct regulator *pmuvcc_reg;
	struct regulator *sevdd_reg;
	unsigned int        ese_pwr_gpio; /* gpio used by SPI to provide power to p61 via NFCC */
	struct mutex        p61_state_mutex; /* used to make p61_current_state flag secure */
	p61_access_state_t  p61_current_state; /* stores the current P61 state */
	bool                nfc_ven_enabled; /* stores the VEN pin state powered by Nfc */
	bool                spi_ven_enabled; /* stores the VEN pin state powered by Spi */
	atomic_t irq_enabled;
	atomic_t read_flag;
	bool cancel_read;
	struct wake_lock nfc_wake_lock;
	long                nfc_service_pid; /*used to signal the nfc the nfc service */
};

static struct pn5xx_dev *pn5xx_dev;

static struct semaphore ese_access_sema;
static struct semaphore svdd_sync_onoff_sema;
static unsigned char svdd_sync_wait;
static void release_ese_lock(p61_access_state_t  p61_current_state);
int get_ese_lock(p61_access_state_t  p61_current_state, int timeout);
static unsigned char p61_trans_acc_on = 0;
static void p61_get_access_state(struct pn5xx_dev*, p61_access_state_t*);

#ifdef CONFIG_SLEEP_MONITOR
int nfc_is_flag;
EXPORT_SYMBOL(nfc_is_flag);
int nfc_sleep_monitor_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	int state = DEVICE_UNKNOWN;

	if (check_level == SLEEP_MONITOR_CHECK_SOFT) {
		if (nfc_is_flag)
			state = DEVICE_ON_ACTIVE1;
		else
			state = DEVICE_POWER_OFF;

	} else {
		/* TODO: HARD */
		state = DEVICE_UNKNOWN;
	}

	*raw_val = state;

	pr_debug("nfc_is_flag[%d] state[%d]\n", nfc_is_flag, state);

	return state;
}

static struct sleep_monitor_ops nfc_sleep_monitor_ops = {
		.read_cb_func = nfc_sleep_monitor_cb,
	};
#endif


/**********************************************************
 * Interrupt control and handler
 **********************************************************/

static irqreturn_t pn5xx_dev_irq_handler(int irq, void *dev_id)
{
    struct pn5xx_dev *pn5xx_dev = dev_id;

	if (!gpio_get_value(pn5xx_dev->irq_gpio)) {
#if NFC_DEBUG
		pr_err("%s, irq_gpio = %d\n", __func__,
		gpio_get_value(pn5xx_dev->irq_gpio));
#endif
		return IRQ_HANDLED;
	}

	/* Wake up waiting readers */
	atomic_set(&pn5xx_dev->read_flag, 1);
	wake_up(&pn5xx_dev->read_wq);
#if NFC_DEBUG
	pr_info("pn5xx : call\n");
#endif
	wake_lock_timeout(&pn5xx_dev->nfc_wake_lock, 2 * HZ);
	return IRQ_HANDLED;
}

/**********************************************************
 * private functions
 **********************************************************/
static void p61_update_access_state(struct pn5xx_dev *pn5xx_dev, p61_access_state_t current_state, bool set)
{
    pr_err("%s: Enter current_state = %x\n", __func__, pn5xx_dev->p61_current_state);
    if (current_state)
    {
        if(set){
            if(pn5xx_dev->p61_current_state == P61_STATE_IDLE)
			pn5xx_dev->p61_current_state = P61_STATE_INVALID;
			pn5xx_dev->p61_current_state |= current_state;
        }
        else{
            pn5xx_dev->p61_current_state ^= current_state;
            if(!pn5xx_dev->p61_current_state)
                pn5xx_dev->p61_current_state = P61_STATE_IDLE;
        }
    }
    pr_err("%s: Exit current_state = %x\n", __func__, pn5xx_dev->p61_current_state);
}

static void p61_get_access_state(struct pn5xx_dev *pn5xx_dev, p61_access_state_t *current_state)
{

    if (current_state == NULL) {
        pr_err("%s : invalid state of p61_access_state_t current state  \n", __func__);
    } else {
        *current_state = pn5xx_dev->p61_current_state;
    }
}

static void p61_access_lock(struct pn5xx_dev *pn5xx_dev)
{
    pr_err("%s: Enter\n", __func__);
    mutex_lock(&pn5xx_dev->p61_state_mutex);
    pr_info("%s: Exit\n", __func__);
}

static void p61_access_unlock(struct pn5xx_dev *pn5xx_dev)
{
    pr_info("%s: Enter\n", __func__);
    mutex_unlock(&pn5xx_dev->p61_state_mutex);
    pr_info("%s: Exit\n", __func__);
}

static int signal_handler(p61_access_state_t state, long nfc_pid)
{
	struct siginfo sinfo;
	pid_t pid;
	struct task_struct *task;
	int sigret = 0;
	int ret = 0;
	pr_info("%s: Enter\n", __func__);

	memset(&sinfo, 0, sizeof(struct siginfo));
	sinfo.si_signo = SIG_NFC;
	sinfo.si_code = SI_QUEUE;
	sinfo.si_int = state;
	pid = nfc_pid;

	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	if(task)
	{
		pr_info("%s.\n", task->comm);
		sigret = send_sig_info(SIG_NFC, &sinfo, task);

		if(sigret < 0){
			pr_info("send_sig_info failed..... sigret %d.\n", sigret);
			ret = -1;
		}
	}
	else
	{
		pr_info("finding task from PID failed\r\n");
		ret = -1;
	}
	pr_info("%s: Exit ret = %d\n", __func__, ret);
	return ret;
}

static void svdd_sync_onoff(long nfc_service_pid, p61_access_state_t origin)
{
	int timeout = 100; //100 ms timeout
	unsigned long tempJ = msecs_to_jiffies(timeout);
	pr_info("%s: Enter nfc_service_pid: %ld\n", __func__, nfc_service_pid);

	if(nfc_service_pid)
	{
		if (0 == signal_handler(origin, nfc_service_pid))
		{
			svdd_sync_wait = 1;
			sema_init(&svdd_sync_onoff_sema, 0);
			pr_info("Waiting for svdd protection response");
			if(down_timeout(&svdd_sync_onoff_sema, tempJ) != 0)
			{
				pr_info("svdd wait protection: Timeout");
			}
			pr_info("svdd wait protection : released");
		}
	}
	pr_info("%s: Exit\n", __func__);
}
static int release_svdd_wait(void)
{
	pr_info("%s: Enter \n", __func__);
	if(svdd_sync_wait)
	{
		up(&svdd_sync_onoff_sema);
		svdd_sync_wait = 0;
	}
	pr_info("%s: Exit\n", __func__);
	return 0;
}

static int pn5xx_enable(struct pn5xx_dev *dev, int mode)
{
	int r;

	p61_access_state_t current_state;

	pr_info("%s: nfc enable [%d]\n", __func__ , atomic_read(&dev->irq_enabled));

	if (atomic_read(&dev->irq_enabled) == 0) {
		atomic_set(&dev->irq_enabled, 1);
		enable_irq(dev->client->irq);

		r = enable_irq_wake(dev->client->irq);
		if(r <0)
		{
			pr_info("%s : fail nfc enable_irq_wake!! [%d]\n" , __func__ , r);
		}
		else
		{
			pr_info("%s : nfc enable_irq_wake!! [%d]\n" , __func__ , r);
		}
	}

    /* turn on the regulators */
    /* -- if the regulators were specified, they're required */
    if(dev->pvdd_reg != NULL)
    {
        r = regulator_enable(dev->pvdd_reg);
        if (r < 0){
		pr_err("%s: not able to enable pvdd\n", __func__);
		return r;
        }
    }
    if(dev->vbat_reg != NULL)
    {
        r = regulator_enable(dev->vbat_reg);
        if (r < 0){
            pr_err("%s: not able to enable vbat\n", __func__);
            goto enable_exit0;
        }
    }
    if(dev->pmuvcc_reg != NULL)
    {
        r = regulator_enable(dev->pmuvcc_reg);
        if (r < 0){
		pr_err("%s: not able to enable pmuvcc\n", __func__);
		goto enable_exit1;
        }
    }
    if(dev->sevdd_reg != NULL)
    {
        r = regulator_enable(dev->sevdd_reg);
        if (r < 0){
		pr_err("%s: not able to enable sevdd\n", __func__);
		goto enable_exit2;
        }
    }

    current_state = P61_STATE_INVALID;
    p61_get_access_state(dev, &current_state);
    if (MODE_RUN == mode) {
        pr_info("%s power on\n", __func__);
        if (gpio_is_valid(dev->firm_gpio)) {
            if ((current_state & (P61_STATE_WIRED|P61_STATE_SPI|P61_STATE_SPI_PRIO))== 0) {
                p61_update_access_state(dev, P61_STATE_IDLE, true);
            }
            gpio_set_value(dev->firm_gpio, 0);
        }

        dev->nfc_ven_enabled = true;
        if (dev->spi_ven_enabled == false) {
            gpio_set_value(dev->ven_gpio, 1);
            msleep(5);
        }

		pr_info("%s: power on, irq(%d)\n", __func__,
				atomic_read(&dev->irq_enabled));
    }
    else if (MODE_FW == mode) {
        if (current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO))
        {
		/* NFCC fw/download should not be allowed if p61 is used
		* by SPI
		*/
		pr_info("%s NFCC should not be allowed to reset/FW download \n", __func__);
		p61_access_unlock(dev);
		return -EBUSY; /* Device or resource busy */
        }
        dev->nfc_ven_enabled = true;
        if (dev->spi_ven_enabled == false)
        {
            /* power on with firmware download (requires hw reset)
             */
		pr_info("%s power on with firmware\n", __func__);
		if (gpio_is_valid(dev->firm_gpio)) {
		p61_update_access_state(dev, P61_STATE_DWNLD, true);
		gpio_set_value(dev->firm_gpio, 1);
		}
		msleep(5);
		gpio_set_value(dev->ven_gpio, 0);

		msleep(5);
		gpio_set_value(dev->ven_gpio, 1);

		msleep(5);
		pr_info("%s: power on with firmware, irq=%d\n", __func__,
			atomic_read(&dev->irq_enabled));

		pr_info("%s: VEN(%d) FIRM(%d)\n", __func__,
			gpio_get_value(dev->ven_gpio), gpio_get_value(dev->firm_gpio));
        }
    } else {
        pr_err("%s bad arg %d\n", __func__, mode);
        p61_access_unlock(dev);
        return -EINVAL;
    }

    return 0;

enable_exit2:
    if(dev->pmuvcc_reg) regulator_disable(dev->pmuvcc_reg);
enable_exit1:
    if(dev->vbat_reg) regulator_disable(dev->vbat_reg);
enable_exit0:
    if(dev->pvdd_reg) regulator_disable(dev->pvdd_reg);

    return r;
}

static void pn5xx_disable(struct pn5xx_dev *dev)
{
	int r;

	/* power off */
	pr_info("%s nfc off irq_enable[%d]\n", __func__ , atomic_read(&dev->irq_enabled));

	if (atomic_read(&dev->irq_enabled) == 1) {
		r = disable_irq_wake(dev->client->irq);
		if(r <0)
		{
			pr_info("%s : fail nfc disable_irq_wake!! [%d]\n" , __func__ , r);
		}
		else
		{
			pr_info("%s : nfc disable_irq_wake!! [%d]\n" , __func__ , r);
			disable_irq_nosync(dev->client->irq);
			atomic_set(&dev->irq_enabled, 0);
		}
	}

	if (gpio_is_valid(dev->firm_gpio))
	{
		p61_access_state_t current_state = P61_STATE_INVALID;
		p61_get_access_state(dev, &current_state);

		if(current_state & P61_STATE_DWNLD)
			p61_update_access_state(pn5xx_dev, P61_STATE_DWNLD, false);
		if ((current_state & (P61_STATE_WIRED|P61_STATE_SPI|P61_STATE_SPI_PRIO))== 0) {
			p61_update_access_state(dev, P61_STATE_IDLE, true);
		}
		gpio_set_value(dev->firm_gpio, 0);
	}

	pr_info("%s: power off, irq(%d)\n", __func__,
		atomic_read(&pn5xx_dev->irq_enabled));

	dev->nfc_ven_enabled = false;
	/* Don't change Ven state if spi made it high */
	if (dev->spi_ven_enabled == false) {
		gpio_set_value(dev->ven_gpio, 0);
		msleep(5);
	}

	if(dev->sevdd_reg) regulator_disable(dev->sevdd_reg);
	if(dev->pmuvcc_reg) regulator_disable(dev->pmuvcc_reg);
	if(dev->vbat_reg) regulator_disable(dev->vbat_reg);
	if(dev->pvdd_reg) regulator_disable(dev->pvdd_reg);

}

/**********************************************************
 * driver functions
 **********************************************************/
static ssize_t pn5xx_dev_read(struct file *filp, char __user *buf,
        size_t count, loff_t *offset)
{
	struct pn5xx_dev *pn5xx_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE] = { 0, };
	int ret = 0;

	if (count > MAX_BUFFER_SIZE)
	    count = MAX_BUFFER_SIZE;

	pr_debug("%s : reading %zu bytes.\n", __func__, count);

	mutex_lock(&pn5xx_dev->read_mutex);

	if (!gpio_get_value(pn5xx_dev->irq_gpio)) {
		atomic_set(&pn5xx_dev->read_flag, 0);
	    if (filp->f_flags & O_NONBLOCK) {
	        ret = -EAGAIN;
	        goto fail;
	    }

#if NFC_DEBUG
	pr_info("%s: wait_event_interruptible : in\n", __func__);
#endif
	if (!gpio_get_value(pn5xx_dev->irq_gpio))
		ret = wait_event_interruptible(pn5xx_dev->read_wq,
				atomic_read(&pn5xx_dev->read_flag));

#if NFC_DEBUG
		pr_info("%s: h\n", __func__);
#endif

		if (pn5xx_dev->cancel_read) {
			pn5xx_dev->cancel_read = false;
			ret = -1;
			goto fail;
		}

		if (ret)
			goto fail;
	}

	/* Read data */
	ret = i2c_master_recv(pn5xx_dev->client, tmp, count);

#if NFC_DEBUG
	pr_info("%s: pn5xx: i2c_master_recv\n", __func__);
#endif
    mutex_unlock(&pn5xx_dev->read_mutex);
    if (ret < 0) {
        pr_err("%s: i2c_master_recv returned %d\n", __func__, ret);
        return ret;
    }
    if (ret > count) {
        pr_err("%s: received too many bytes from i2c (%d)\n",
            __func__, ret);
        return -EIO;
    }
    if (copy_to_user(buf, tmp, ret)) {
		pr_err("%s: failed to copy to user space\n", __func__);
        return -EFAULT;
    }
    return ret;

fail:
    mutex_unlock(&pn5xx_dev->read_mutex);
    return ret;
}

static ssize_t pn5xx_dev_write(struct file *filp, const char __user *buf,
        size_t count, loff_t *offset)
{
	struct pn5xx_dev *pn5xx_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE] = {0, };
	int ret = 0, retry = 2;
#if NFC_DEBUG
	pr_info("%s: pn5xx: + w\n", __func__);
#endif

    pn5xx_dev = filp->private_data;

    if (count > MAX_BUFFER_SIZE)
        count = MAX_BUFFER_SIZE;

    if (copy_from_user(tmp, buf, count)) {
        pr_err("%s : failed to copy from user space\n", __func__);
        return -EFAULT;
    }

    pr_debug("%s : writing %zu bytes.\n", __func__, count);
    /* Write data */
	do {
		retry--;
   		 ret = i2c_master_send(pn5xx_dev->client, tmp, count);
		if (ret == count)
			break;
		usleep_range(6000, 10000); /* Retry, chip was in standby */
#if NFC_DEBUG
		pr_debug("%s: retry = %d\n", __func__, retry);
#endif
	} while (retry);
#if NFC_DEBUG
	pr_info("%s: pn5xx: - w\n", __func__);
#endif
    if (ret != count) {
        pr_err("%s : i2c_master_send returned %d\n", __func__, ret);
        ret = -EIO;
    }

    return ret;
}

static int pn5xx_dev_open(struct inode *inode, struct file *filp)
{
    struct pn5xx_dev *pn5xx_dev = container_of(filp->private_data,
                                               struct pn5xx_dev,
                                               pn5xx_device);

    printk("pn5xx_dev_open!!!!!!!!!!!!!!!!!!!!!!!\n");

    filp->private_data = pn5xx_dev;

    pr_info("%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

    // pn5xx_enable(pn5xx_dev, MODE_RUN);

    return 0;
}

static int pn5xx_dev_release(struct inode *inode, struct file *filp)
{
    // struct pn5xx_dev *pn5xx_dev = container_of(filp->private_data,
    //                                           struct pn5xx_dev,
    //                                           pn5xx_device);
	p61_access_state_t current_state = P61_STATE_INVALID;
	pr_info("%s : closing %d,%d\n", __func__, imajor(inode), iminor(inode));

	p61_get_access_state(pn5xx_dev, &current_state);
	if((p61_trans_acc_on ==  1) && ((current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO)) == 0))
		release_ese_lock(P61_STATE_WIRED);
    // pn5xx_disable(pn5xx_dev);

	svdd_sync_wait = 0;

    return 0;
}

long  pn5xx_dev_ioctl(struct file *filp, unsigned int cmd,
                unsigned long arg)
{
    /* struct pn5xx_dev *pn5xx_dev = filp->private_data; */

    //struct pn5xx_dev *pn5xx_dev =  container_of(filp->private_data,struct pn5xx_dev, pn5xx_device);

    //printk("pn5xx_dev_ioctl!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    pr_info("nfc pn5 ioctl !!!!!!!!!! %s, cmd=%d, arg=%lu\n", __func__, cmd, arg);

    if (cmd == PN5XX_GET_ESE_ACCESS)
    {
        return get_ese_lock(P61_STATE_WIRED, arg);
    }
    else if(cmd == PN5XX_REL_SVDD_WAIT)
    {
        return release_svdd_wait();
    }

    p61_access_lock(pn5xx_dev);
    switch (cmd) {
    case PN5XX_SET_PWR:
        if (arg == 2) {
		/* power on w/FW */
		pn5xx_enable(pn5xx_dev, arg);

#ifdef CONFIG_SLEEP_MONITOR
		nfc_is_flag = 1;
		pr_info("nfc nfc_is_flag set true!!\n");
#endif

        } else if (arg == 1) {
		/* power on */
		pn5xx_enable(pn5xx_dev, arg);

#ifdef CONFIG_SLEEP_MONITOR
		nfc_is_flag = 1;
		pr_info("nfc nfc_is_flag set true!!\n");
#endif

        } else  if (arg == 0) {
		/* power off */
		pn5xx_disable(pn5xx_dev);

#ifdef CONFIG_SLEEP_MONITOR
		nfc_is_flag = 0;
		pr_info("nfc nfc_is_flag set false!!\n");
#endif

        } else if (arg == 3)
        {
		pr_info("%s: Read Cancel\n", __func__);
		pn5xx_dev->cancel_read = true;
		atomic_set(&pn5xx_dev->read_flag, 1);
		wake_up(&pn5xx_dev->read_wq);
		break;
        } else {
            pr_err("%s bad SET_PWR arg %lu\n", __func__, arg);
            return -EINVAL;
        }
        break;
    case PN5XX_CLK_REQ:
        if(1 == arg){
            if(gpio_is_valid(pn5xx_dev->clkreq_gpio)){
                gpio_set_value(pn5xx_dev->clkreq_gpio, 1);
            }
        }
        else if(0 == arg) {
            if(gpio_is_valid(pn5xx_dev->clkreq_gpio)){
                gpio_set_value(pn5xx_dev->clkreq_gpio, 0);
            }
        } else {
            pr_err("%s bad CLK_REQ arg %lu\n", __func__, arg);
            return -EINVAL;
        }
        break;
    case P61_SET_SPI_PWR:
    {
        p61_access_state_t current_state = P61_STATE_INVALID;
        p61_get_access_state(pn5xx_dev, &current_state);
        if (arg == 1) {
            pr_err("%s : PN61_SET_SPI_PWR - power on ese\n", __func__);
			if ((current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO|P61_STATE_DWNLD)) == 0)
			{
				p61_update_access_state(pn5xx_dev, P61_STATE_SPI, true);
				/*To handle triple mode protection signal
				  NFC service when SPI session started*/
//				if (current_state & P61_STATE_WIRED)
				{
					if(pn5xx_dev->nfc_service_pid){
						pr_info("nfc service pid %s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
						signal_handler(P61_STATE_SPI, pn5xx_dev->nfc_service_pid);
					}
					else{
						pr_info(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
					}
				}
               		 pn5xx_dev->spi_ven_enabled = true;

				if (pn5xx_dev->nfc_ven_enabled == false) {
					/* provide power to NFCC if, NFC service not provided */
					gpio_set_value(pn5xx_dev->ven_gpio, 1);
					msleep(10);
				}

				/* pull the gpio to high once NFCC is power on*/
				gpio_set_value(pn5xx_dev->ese_pwr_gpio, 1);
				msleep(10);
			} else {
			pr_err("%s : PN61_SET_SPI_PWR -  power on ese failed \n", __func__);
			p61_access_unlock(pn5xx_dev);
			return -EBUSY; /* Device or resource busy */
            }
        } else if (arg == 0) {
            pr_err("%s : PN61_SET_SPI_PWR - power off ese\n", __func__);
            if(current_state & P61_STATE_SPI_PRIO){
                p61_update_access_state(pn5xx_dev, P61_STATE_SPI_PRIO, false);
//                if (current_state & P61_STATE_WIRED)
                {
                    if(pn5xx_dev->nfc_service_pid){
                        pr_err("nfc service pid %s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                        signal_handler(P61_STATE_SPI_PRIO_END, pn5xx_dev->nfc_service_pid);
                }
                else{
                    pr_err(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                }
                }
                if (!(current_state & P61_STATE_WIRED))
                {
			svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_SPI_SVDD_SYNC_START);
			gpio_set_value(pn5xx_dev->ese_pwr_gpio, 0);
			msleep(60);
			svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_SPI_SVDD_SYNC_END);
                }
                pn5xx_dev->spi_ven_enabled = false;
                 if (pn5xx_dev->nfc_ven_enabled == false) {
                     gpio_set_value(pn5xx_dev->ven_gpio, 0);
                     msleep(10);
                 }
              }else if(current_state & P61_STATE_SPI){
                  p61_update_access_state(pn5xx_dev, P61_STATE_SPI, false);
                  if (!(current_state & P61_STATE_WIRED))
			{
				svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_SPI_SVDD_SYNC_START);
				gpio_set_value(pn5xx_dev->ese_pwr_gpio, 0);
				msleep(60);
				svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_SPI_SVDD_SYNC_END);
			}
			/*If JCOP3.2 or 3.3 for handling triple mode protection signal NFC service */
//			else
			{
//				if (current_state & P61_STATE_WIRED)
				{
					if(pn5xx_dev->nfc_service_pid){
						pr_info("nfc service pid %s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
						signal_handler(P61_STATE_SPI_END, pn5xx_dev->nfc_service_pid);
					}
					else{
						pr_info(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
					}
				}
			}
			pn5xx_dev->spi_ven_enabled = false;
			if (pn5xx_dev->nfc_ven_enabled == false) {
				gpio_set_value(pn5xx_dev->ven_gpio, 0);
				msleep(10);
			}
            } else {
			pr_err("%s : PN61_SET_SPI_PWR - failed, current_state = %x \n",
				__func__, pn5xx_dev->p61_current_state);
			p61_access_unlock(pn5xx_dev);
			return -EPERM; /* Operation not permitted */
            }
        }else if (arg == 2) {
            pr_err("%s : PN61_SET_SPI_PWR - reset\n", __func__);
            if (current_state & (P61_STATE_IDLE|P61_STATE_SPI|P61_STATE_SPI_PRIO)) {
                if (pn5xx_dev->spi_ven_enabled == false)
                {
                    pn5xx_dev->spi_ven_enabled = true;
                    if (pn5xx_dev->nfc_ven_enabled == false) {
                        /* provide power to NFCC if, NFC service not provided */
                        gpio_set_value(pn5xx_dev->ven_gpio, 1);
                        msleep(10);
                    }
                }
		svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_SPI_SVDD_SYNC_START);
		gpio_set_value(pn5xx_dev->ese_pwr_gpio, 0);
		msleep(60);
		svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_SPI_SVDD_SYNC_END);
		gpio_set_value(pn5xx_dev->ese_pwr_gpio, 1);
		msleep(10);
            } else {
                pr_err("%s : PN61_SET_SPI_PWR - reset  failed \n", __func__);
                p61_access_unlock(pn5xx_dev);
                return -EBUSY; /* Device or resource busy */
            }
        }else if (arg == 3) {
            pr_err("%s : PN61_SET_SPI_PWR - Prio Session Start power on ese\n", __func__);
			if ((current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO|P61_STATE_DWNLD)) == 0) {
                p61_update_access_state(pn5xx_dev, P61_STATE_SPI_PRIO, true);
//                if (current_state & P61_STATE_WIRED)
				{
                    if(pn5xx_dev->nfc_service_pid){
                        pr_err("nfc service pid %s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                        signal_handler(P61_STATE_SPI_PRIO, pn5xx_dev->nfc_service_pid);
                    }
                    else{
                        pr_err(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                    }
                }
                pn5xx_dev->spi_ven_enabled = true;
                if (pn5xx_dev->nfc_ven_enabled == false) {
                    /* provide power to NFCC if, NFC service not provided */
                    gpio_set_value(pn5xx_dev->ven_gpio, 1);
                    msleep(10);
                }
                /* pull the gpio to high once NFCC is power on*/
                gpio_set_value(pn5xx_dev->ese_pwr_gpio, 1);
                msleep(10);
            }else {
                pr_err("%s : Prio Session Start power on ese failed \n", __func__);
                p61_access_unlock(pn5xx_dev);
                return -EBUSY; /* Device or resource busy */
            }
        }else if (arg == 4) {
            if (current_state & P61_STATE_SPI_PRIO)
            {
                pr_err("%s : PN61_SET_SPI_PWR - Prio Session Ending...\n", __func__);
                p61_update_access_state(pn5xx_dev, P61_STATE_SPI_PRIO, false);
                /*after SPI prio timeout, the state is changing from SPI prio to SPI */
                p61_update_access_state(pn5xx_dev, P61_STATE_SPI, true);
//                if (current_state & P61_STATE_WIRED)
                {
                    if(pn5xx_dev->nfc_service_pid){
                        pr_err("nfc service pid %s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                        signal_handler(P61_STATE_SPI_PRIO_END, pn5xx_dev->nfc_service_pid);
                    }
                    else{
                        pr_err(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                    }
               }
            }
            else
            {
                pr_err("%s : PN61_SET_SPI_PWR -  Prio Session End failed \n", __func__);
                p61_access_unlock(pn5xx_dev);
                return -EBADRQC; /* Device or resource busy */
            }
        } else if(arg == 5){
            release_ese_lock(P61_STATE_SPI);
        }
        else {
            pr_err("%s bad ese pwr arg %lu\n", __func__, arg);
            p61_access_unlock(pn5xx_dev);
            return -EBADRQC; /* Invalid request code */
        }
    }
    break;
    case P61_GET_PWR_STATUS:
    {
        p61_access_state_t current_state = P61_STATE_INVALID;
        p61_get_access_state(pn5xx_dev, &current_state);
        pr_err("%s: P61_GET_PWR_STATUS  = %x",__func__, current_state);
        put_user(current_state, (int __user *)arg);
    }
    break;
    case P61_SET_WIRED_ACCESS:
    {
        p61_access_state_t current_state = P61_STATE_INVALID;
        p61_get_access_state(pn5xx_dev, &current_state);
        if (arg == 1)
        {
            if (current_state)
            {
                pr_err("%s : P61_SET_WIRED_ACCESS - enabling\n", __func__);
                p61_update_access_state(pn5xx_dev, P61_STATE_WIRED, true);
                if (current_state & P61_STATE_SPI_PRIO)
                {
                    if(pn5xx_dev->nfc_service_pid){
                        pr_err("nfc service pid %s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                        signal_handler(P61_STATE_SPI_PRIO, pn5xx_dev->nfc_service_pid);
                    }
                    else{
                        pr_err(" invalid nfc service pid....signalling failed%s   ---- %ld", __func__, pn5xx_dev->nfc_service_pid);
                    }
                }
                if((current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO)) == 0)
				{
                    gpio_set_value(pn5xx_dev->ese_pwr_gpio, 1);
                    msleep(10);
				}
            } else {
                pr_err("%s : P61_SET_WIRED_ACCESS -  enabling failed \n", __func__);
                p61_access_unlock(pn5xx_dev);
                return -EBUSY; /* Device or resource busy */
            }
        } else if (arg == 0) {
            pr_err("%s : P61_SET_WIRED_ACCESS - disabling \n", __func__);
            if (current_state & P61_STATE_WIRED){
                p61_update_access_state(pn5xx_dev, P61_STATE_WIRED, false);
                if((current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO)) == 0)
                {
			svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_DWP_SVDD_SYNC_START);
			gpio_set_value(pn5xx_dev->ese_pwr_gpio, 0);
			msleep(60);
			svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_DWP_SVDD_SYNC_END);
                }
            } else {
                pr_err("%s : P61_SET_WIRED_ACCESS - failed, current_state = %x \n",
                        __func__, pn5xx_dev->p61_current_state);
                p61_access_unlock(pn5xx_dev);
                return -EPERM; /* Operation not permitted */
            }
        }
		else if(arg == 2)
		{
			pr_info("%s : P61 ESE POWER REQ LOW  \n", __func__);
			svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_DWP_SVDD_SYNC_START);
			gpio_set_value(pn5xx_dev->ese_pwr_gpio, 0);
			msleep(60);
			svdd_sync_onoff(pn5xx_dev->nfc_service_pid, P61_STATE_DWP_SVDD_SYNC_END);
		}
		else if(arg == 3)
		{
			pr_info("%s : P61 ESE POWER REQ HIGH  \n", __func__);
			gpio_set_value(pn5xx_dev->ese_pwr_gpio, 1);
			msleep(10);
		}
        else if(arg == 4)
        {
            release_ese_lock(P61_STATE_WIRED);
        }
        else {
            pr_err("%s P61_SET_WIRED_ACCESS - bad arg %lu\n", __func__, arg);
            p61_access_unlock(pn5xx_dev);
            return -EBADRQC; /* Invalid request code */
        }
    }
    break;
    case PN5XX_SET_NFC_SERVICE_PID:
    {
	p61_access_state_t current_state = P61_STATE_INVALID;
	p61_get_access_state(pn5xx_dev, &current_state);

	if((p61_trans_acc_on ==  1) && ((current_state & (P61_STATE_SPI|P61_STATE_SPI_PRIO)) == 0))
		release_ese_lock(P61_STATE_WIRED);
        pr_err("%s : The NFC Service PID is %ld\n", __func__, arg);
        pn5xx_dev->nfc_service_pid = arg;

    }
    break;
    default:
		pr_err("%s bad ioctl %u\n", __func__, cmd);
		p61_access_unlock(pn5xx_dev);
		pr_info("%s :exit cmd = %u, arg = %ld\n", __func__, cmd, arg);
		return -EINVAL;
    }
    p61_access_unlock(pn5xx_dev);
    return 0;
}

EXPORT_SYMBOL(pn5xx_dev_ioctl);

int get_ese_lock(p61_access_state_t  p61_current_state, int timeout)
{
	unsigned long tempJ = msecs_to_jiffies(timeout);
	pr_info("get_ese_lock: enter p61_current_state =(0x%x), timeout = %d, jiffies = %lu\n"
		, p61_current_state, timeout, tempJ);
	if(down_timeout(&ese_access_sema, tempJ) != 0)
	{
		printk("get_ese_lock: timeout p61_current_state = %d\n", p61_current_state);
		return -EBUSY;
	}

	p61_trans_acc_on = 1;
	pr_info("get_ese_lock: exit p61_trans_acc_on =%d, timeout = %d\n"
		, p61_trans_acc_on, timeout);
	return 0;
}
EXPORT_SYMBOL(get_ese_lock);

static void release_ese_lock(p61_access_state_t  p61_current_state)
{
	pr_info("%s: enter p61_current_state = (0x%x)\n", __func__, p61_current_state);
	up(&ese_access_sema);
	p61_trans_acc_on = 0;
	pr_info("%s: p61_trans_acc_on =%d exit\n", __func__, p61_trans_acc_on);
}

static const struct file_operations pn5xx_dev_fops = {
    .owner    = THIS_MODULE,
   // .llseek    = no_llseek,
    .read    = pn5xx_dev_read,
    .write    = pn5xx_dev_write,
    .open    = pn5xx_dev_open,
    .release  = pn5xx_dev_release,
    .compat_ioctl = pn5xx_dev_ioctl,
};

/*
 * Handlers for alternative sources of platform_data
 */
#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static int pn5xx_get_pdata(struct device *dev,
                           struct pn5xx_i2c_platform_data *pdata)
{
    struct device_node *node;
    u32 flags;
    int val;

    /* make sure there is actually a device tree node */
    node = dev->of_node;
    if (!node)
        return -ENODEV;

    memset(pdata, 0, sizeof(*pdata));

    /* read the dev tree data */

    /* ven pin - enable's power to the chip - REQUIRED */
    val = of_get_named_gpio_flags(node, "sec-nfc,ven-gpio", 0, &flags);
    if (val >= 0) {
        pdata->ven_gpio = val;
        pr_info("%s : nfc ven gpio %d\n" , __func__ , pdata->ven_gpio);
    }
    else{
        dev_err(dev, "VEN GPIO error getting from OF node\n");
        return val;
    }

    /* firm pin - controls firmware download - OPTIONAL */
    val = of_get_named_gpio_flags(node, "sec-nfc,firm-gpio", 0, &flags);
    if (val >= 0) {
        pdata->firm_gpio = val;
        pr_info("%s : nfc firm_gpio %d\n" , __func__ , pdata->firm_gpio);
    }
    else {
        pdata->firm_gpio = -EINVAL;
        dev_warn(dev, "FIRM GPIO <OPTIONAL> error getting from OF node\n");
    }

    /* irq pin - data available irq - REQUIRED */
    val = of_get_named_gpio_flags(node, "sec-nfc,irq-gpio", 0, &flags);
    if (val >= 0) {
        pdata->irq_gpio = val;
        pr_info("%s : nfc irq_gpio %d\n" , __func__ , pdata->irq_gpio);
    }
    else {
        dev_err(dev, "IRQ GPIO error getting from OF node\n");
        return val;
    }

    /* ese-pwr pin - enable's power to the ese- REQUIRED */
    val = of_get_named_gpio_flags(node, "sec-nfc,ese-pwr", 0, &flags);
    if (val >= 0) {
        pdata->ese_pwr_gpio = val;
    }
    else {
        dev_err(dev, "ESE PWR GPIO error getting from OF node\n");
	/* working from REV0.3 in Solis */
        /* return val; */
    }
#if 0
    /* clkreq pin - controls the clock to the PN547 - OPTIONAL */
    val = of_get_named_gpio_flags(node, "nxp,pn5xx-clkreq", 0, &flags);
    if (val >= 0) {
        pdata->clkreq_gpio = val;
    }
    else {
        pdata->clkreq_gpio = -EINVAL;
        dev_warn(dev, "CLKREQ GPIO <OPTIONAL> error getting from OF node\n");
    }
#endif
    /* handle the regulator lines - these are optional
     * PVdd - pad Vdd (544, 547)
     * Vbat - Battery (544, 547)
     * PMUVcc - UICC Power (544, 547)
     * SEVdd - SE Power (544)
     *
     * Will attempt to load a matching Regulator Resource for each
     * If no resource is provided, then the input will not be controlled
     * Example: if only PVdd is provided, it is the only one that will be
     *  turned on/off.
     */
     #if 0
    pdata->pvdd_reg = regulator_get(dev, "nxp,pn5xx-pvdd");
    if(IS_ERR(pdata->pvdd_reg)) {
        pr_err("%s: could not get nxp,pn5xx-pvdd, rc=%ld\n", __func__, PTR_ERR(pdata->pvdd_reg));
        pdata->pvdd_reg = NULL;
    }

    pdata->vbat_reg = regulator_get(dev, "nxp,pn5xx-vbat");
    if (IS_ERR(pdata->vbat_reg)) {
        pr_err("%s: could not get nxp,pn5xx-vbat, rc=%ld\n", __func__, PTR_ERR(pdata->vbat_reg));
        pdata->vbat_reg = NULL;
    }

    pdata->pmuvcc_reg = regulator_get(dev, "nxp,pn5xx-pmuvcc");
    if (IS_ERR(pdata->pmuvcc_reg)) {
        pr_err("%s: could not get nxp,pn5xx-pmuvcc, rc=%ld\n", __func__, PTR_ERR(pdata->pmuvcc_reg));
        pdata->pmuvcc_reg = NULL;
    }

    pdata->sevdd_reg = regulator_get(dev, "nxp,pn5xx-sevdd");
    if (IS_ERR(pdata->sevdd_reg)) {
        pr_err("%s: could not get nxp,pn5xx-sevdd, rc=%ld\n", __func__, PTR_ERR(pdata->sevdd_reg));
        pdata->sevdd_reg = NULL;
    }
    #endif
    return 0;
}
#else
static int pn5xx_get_pdata(struct device *dev,
                           struct pn5xx_i2c_platform_data *pdata)
{
    pdata = dev->platform_data;
    return 0;
}
#endif


/*
 *  pn5xx_probe
 */
#ifdef KERNEL_3_4_AND_OLDER
 static int __devinit pn5xx_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
#else
static int pn5xx_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
#endif
{
	int ret;
	struct pn5xx_i2c_platform_data *pdata;  // gpio values, from board file or DT
	struct pn5xx_i2c_platform_data tmp_pdata;
	/* struct pn5xx_dev *pn5xx_dev;            // internal device specific data */

	pr_info("nfc %s\n", __func__);
	printk("nfc pn5xx_probe!!\n");

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(NULL, &nfc_sleep_monitor_ops,
	SLEEP_MONITOR_NFC);
#endif

    /* ---- retrieve the platform data ---- */
    /* If the dev.platform_data is NULL, then */
    /* attempt to read from the device tree */
    if(!client->dev.platform_data)
    {
        ret = pn5xx_get_pdata(&(client->dev), &tmp_pdata);
        if(ret){
            return ret;
        }

        pdata = &tmp_pdata;
    }
    else
    {
        pdata = client->dev.platform_data;
    }

    if (pdata == NULL) {
        pr_err("%s : nfc probe fail\n", __func__);
        return  -ENODEV;
    }

    /* validate the the adapter has basic I2C functionality */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        pr_err("%s : nfc need I2C_FUNC_I2C\n", __func__);
        return  -ENODEV;
    }

    /* reserve the GPIO pins */
    pr_info("%s: nfc request irq_gpio %d\n", __func__, pdata->irq_gpio);
    ret = gpio_request(pdata->irq_gpio, "nfc_int");
    if (ret){
        pr_err("%s :nfc not able to get GPIO irq_gpio\n", __func__);
        return  -ENODEV;
    }
    ret = gpio_to_irq(pdata->irq_gpio);
    if (ret < 0){
        pr_err("%s :nfc not able to map GPIO irq_gpio to an IRQ\n", __func__);
        goto err_ven;
    }
    else{
        client->irq = ret;
    }

    pr_info("%s: nfc request ven_gpio %d\n", __func__, pdata->ven_gpio);
    ret = gpio_request(pdata->ven_gpio, "nfc_ven");
    if (ret){
        pr_err("%s :nfc not able to get GPIO ven_gpio\n", __func__);
        goto err_ven;
    }

    if (gpio_is_valid(pdata->firm_gpio)) {
        pr_info("%s: nfc request firm_gpio %d\n", __func__, pdata->firm_gpio);
        ret = gpio_request(pdata->firm_gpio, "nfc_firm");
        if (ret){
            pr_err("%s :nfc not able to get GPIO firm_gpio\n", __func__);
            goto err_firm;
        }
    }

    if (gpio_is_valid(pdata->ese_pwr_gpio)) {
        pr_info("%s: nfc request ese_pwr_gpio %d\n", __func__, pdata->ese_pwr_gpio);
        ret = gpio_request(pdata->ese_pwr_gpio, "nfc_ese_pwr");
        if (ret){
            pr_err("%s :not able to get GPIO ese_pwr_gpio\n", __func__);
            goto err_ese_pwr;
        }
    }
#if 0
    if (gpio_is_valid(pdata->clkreq_gpio)) {
        pr_info("%s: request clkreq_gpio %d\n", __func__, pdata->clkreq_gpio);
        ret = gpio_request(pdata->clkreq_gpio, "nfc_clkreq");
        if (ret){
            pr_err("%s :not able to get GPIO clkreq_gpio\n", __func__);
            goto err_clkreq;
        }
    }
#endif
    /* allocate the pn5xx driver information structure */
    pn5xx_dev = kzalloc(sizeof(*pn5xx_dev), GFP_KERNEL);
    if (pn5xx_dev == NULL) {
        dev_err(&client->dev, "failed to allocate memory for module data\n");
        ret = -ENOMEM;
        goto err_exit;
    }

	wake_lock_init(&pn5xx_dev->nfc_wake_lock,
			WAKE_LOCK_SUSPEND, "nfc_wake_lock");

	/* store the platform data in the driver info struct */
	pn5xx_dev->irq_gpio = pdata->irq_gpio;
	pn5xx_dev->ven_gpio  = pdata->ven_gpio;
	pn5xx_dev->firm_gpio  = pdata->firm_gpio;
	pn5xx_dev->ese_pwr_gpio  = pdata->ese_pwr_gpio;
	pn5xx_dev->p61_current_state = P61_STATE_IDLE;
	pn5xx_dev->nfc_ven_enabled = false;
	pn5xx_dev->spi_ven_enabled = false;
#if 1
	pn5xx_dev->clkreq_gpio = pdata->clkreq_gpio;
	pn5xx_dev->pvdd_reg = pdata->pvdd_reg;
	pn5xx_dev->vbat_reg = pdata->vbat_reg;
	pn5xx_dev->pmuvcc_reg = pdata->vbat_reg;
	pn5xx_dev->sevdd_reg = pdata->sevdd_reg;
#endif
	pn5xx_dev->client   = client;

	/* finish configuring the I/O */
	ret = gpio_direction_input(pn5xx_dev->irq_gpio);
	if (ret < 0) {
		pr_err("%s :nfc not able to set irq_gpio as input\n", __func__);
		goto err_exit;
	}

	ret = gpio_direction_output(pn5xx_dev->ven_gpio, 0);
	if (ret < 0) {
		pr_err("%s : nfc not able to set ven_gpio as output\n", __func__);
		goto err_exit;
	}

	if (gpio_is_valid(pn5xx_dev->firm_gpio)) {
	ret = gpio_direction_output(pn5xx_dev->firm_gpio, 0);
	if (ret < 0) {
		pr_err("%s : nfc not able to set firm_gpio as output\n",
		__func__);
		goto err_exit;
		}
	}

	if (gpio_is_valid(pn5xx_dev->ese_pwr_gpio)) {
		ret = gpio_direction_output(pn5xx_dev->ese_pwr_gpio, 0);
		if (ret < 0) {
		pr_err("%s : not able to set ese_pwr gpio as output\n", __func__);
		goto err_ese_pwr;
		}
	}
#if 0
    if (gpio_is_valid(pn5xx_dev->clkreq_gpio)) {
        ret = gpio_direction_output(pn5xx_dev->clkreq_gpio, 0);
        if (ret < 0) {
            pr_err("%s : not able to set clkreq_gpio as output\n",
                   __func__);
            goto err_exit;
        }
    }
#endif
	/* init mutex and queues */
	init_waitqueue_head(&pn5xx_dev->read_wq);
	mutex_init(&pn5xx_dev->read_mutex);
	mutex_init(&pn5xx_dev->p61_state_mutex);
	sema_init(&ese_access_sema, 1);
	//spin_lock_init(&pn5xx_dev->irq_enabled_lock);

	/* register as a misc device - character based with one entry point */
	pn5xx_dev->pn5xx_device.minor = MISC_DYNAMIC_MINOR;
	//pn5xx_dev->pn5xx_device.name = CHIP;
	pn5xx_dev->pn5xx_device.name = "sec-nfc";
	pn5xx_dev->pn5xx_device.fops = &pn5xx_dev_fops;

	ret = misc_register(&pn5xx_dev->pn5xx_device);
	if (ret) {
		pr_err("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	* for reading.  it is cleared when all data has been read.
	*/
	pr_info("%s : nfc requesting IRQ %d\n", __func__, client->irq);
	pr_info("%s : nfc requesting IRQ %s\n", __func__, client->name);

	ret = request_irq(client->irq, pn5xx_dev_irq_handler,
		IRQF_TRIGGER_RISING, SEC_NFC_DRIVER_NAME/*client->name*/, pn5xx_dev);

	if (ret) {
		dev_err(&client->dev, "request_irq failed\n");
		goto err_request_irq_failed;
	}

	disable_irq_nosync(pn5xx_dev->client->irq);
	atomic_set(&pn5xx_dev->irq_enabled, 0);


	i2c_set_clientdata(client, pn5xx_dev);

	printk("nfc pn5xx_probe END!!!!!!!!!!!!!!!!\n");
	return 0;

err_request_irq_failed:
    misc_deregister(&pn5xx_dev->pn5xx_device);
	wake_lock_destroy(&pn5xx_dev->nfc_wake_lock);
err_misc_register:
    mutex_destroy(&pn5xx_dev->read_mutex);
    mutex_destroy(&pn5xx_dev->p61_state_mutex);
    kfree(pn5xx_dev);
err_exit:
    if (gpio_is_valid(pdata->clkreq_gpio))
        gpio_free(pdata->clkreq_gpio);
err_ese_pwr:
    if (gpio_is_valid(pdata->ese_pwr_gpio))
        gpio_free(pdata->ese_pwr_gpio);
	#if 0
err_clkreq:
    if (gpio_is_valid(pdata->firm_gpio))
        gpio_free(pdata->firm_gpio);
	#endif
err_firm:
    gpio_free(pdata->ven_gpio);
err_ven:
    gpio_free(pdata->irq_gpio);
    return ret;
}

#ifdef KERNEL_3_4_AND_OLDER
static int __devexit pn5xx_remove(struct i2c_client *client)
#else
static int pn5xx_remove(struct i2c_client *client)
#endif
{
    struct pn5xx_dev *pn5xx_dev;

	pr_info("nfc %s\n", __func__);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_NFC);
#endif

	pn5xx_dev = i2c_get_clientdata(client);
	wake_lock_destroy(&pn5xx_dev->nfc_wake_lock);
	free_irq(client->irq, pn5xx_dev);
	misc_deregister(&pn5xx_dev->pn5xx_device);
	mutex_destroy(&pn5xx_dev->read_mutex);
	mutex_destroy(&pn5xx_dev->p61_state_mutex);
	gpio_free(pn5xx_dev->irq_gpio);
	gpio_free(pn5xx_dev->ven_gpio);

	if (gpio_is_valid(pn5xx_dev->ese_pwr_gpio))
		gpio_free(pn5xx_dev->ese_pwr_gpio);

	pn5xx_dev->p61_current_state = P61_STATE_INVALID;
	pn5xx_dev->nfc_ven_enabled = false;
	pn5xx_dev->spi_ven_enabled = false;

	if (gpio_is_valid(pn5xx_dev->firm_gpio))
		gpio_free(pn5xx_dev->firm_gpio);
	if (gpio_is_valid(pn5xx_dev->clkreq_gpio))
		gpio_free(pn5xx_dev->clkreq_gpio);

	regulator_put(pn5xx_dev->pvdd_reg);
	regulator_put(pn5xx_dev->vbat_reg);
	regulator_put(pn5xx_dev->pmuvcc_reg);
	regulator_put(pn5xx_dev->sevdd_reg);

	kfree(pn5xx_dev);

	return 0;
}

/*
 *
 */
#ifdef CONFIG_OF
static struct of_device_id pn5xx_dt_match[] = {
    { .compatible = "sec-nfc,i2c"},
    {},
};
MODULE_DEVICE_TABLE(of, pn5xx_dt_match);
#endif

static const struct i2c_device_id pn5xx_id[] = {
    { SEC_NFC_DRIVER_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, pn5xx_id);

static struct i2c_driver pn5xx_driver = {
    .probe        = pn5xx_probe,
    .id_table    = pn5xx_id,
#ifdef KERNEL_3_4_AND_OLDER
    .remove        = __devexit_p(pn5xx_remove),
#else
    .remove        = pn5xx_remove,
#endif
    .driver        = {
        .name    = SEC_NFC_DRIVER_NAME,
        .of_match_table = pn5xx_dt_match,
    },
};

/*
 * module load/unload record keeping
 */

static int __init pn5xx_dev_init(void)
{
	int ret = 0;
	pr_info("nfc %s\n", __func__);
	printk("nfc pn5xx_dev_init!!!!!!!!!!!!\n");
	ret = i2c_add_driver(&pn5xx_driver);

	printk("nfc pn5xx_dev_init!! = [%d]\n" , ret);

	return ret;
}

static void __exit pn5xx_dev_exit(void)
{
	pr_info("nfc %s\n", __func__);
	printk("pn5xx_dev_exit!!\n");
	i2c_del_driver(&pn5xx_driver);
}

module_init(pn5xx_dev_init);
module_exit(pn5xx_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

