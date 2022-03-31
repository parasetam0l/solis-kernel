/*
 * cyttsp5_mt_common.c
 * Cypress TrueTouch(TM) Standard Product V5 Multi-Touch Reports Module.
 * For use with Cypress Txx5xx parts.
 * Supported parts include:
 * TMA5XX
 *
 * Copyright (C) 2012-2013 Cypress Semiconductor
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include "cyttsp5_regs.h"
#include <linux/input/mt.h>
#ifdef CONFIG_KNOX_GEARPAY
#include <linux/knox_gearlock.h>
#endif
#ifdef TOUCH_BOOSTER
#include <linux/trm.h>
#endif

#define CYTTSP5_TOUCHLOG_ENABLE 0

static void cyttsp5_final_sync(struct input_dev *input, int max_slots,
		int mt_sync_count, unsigned long *ids)
{
	int t;

	for (t = 0; t < max_slots; t++) {
		if (test_bit(t, ids))
			continue;
		input_mt_slot(input, t);
		input_mt_report_slot_state(input, MT_TOOL_FINGER, false);
	}

	input_sync(input);
}

static void cyttsp5_input_report(struct input_dev *input, int sig,
		int t, int type)
{
	input_mt_slot(input, t);

	if (type == CY_OBJ_STANDARD_FINGER || type == CY_OBJ_GLOVE)
		input_mt_report_slot_state(input, MT_TOOL_FINGER, true);
	else if (type == CY_OBJ_STYLUS)
		input_mt_report_slot_state(input, MT_TOOL_PEN, true);
}

#ifdef SAMSUNG_PALM_MOTION
static void report_sumsize_palm(struct cyttsp5_mt_data *md, bool palm)
{
	dev_dbg(md->dev, "%s: palm=%d\n", __func__, palm);
	input_report_abs(md->input, ABS_MT_PALM, palm);
	if (palm){
        input_report_key(md->input, KEY_SLEEP, 1);
        input_sync(md->input);
        input_report_key(md->input, KEY_SLEEP, 0);
        input_sync(md->input);
    }
}
#endif

static void cyttsp5_report_slot_liftoff(struct cyttsp5_mt_data *md,
		int max_slots)
{
	int t;

	if (md->num_prv_tch == 0)
		return;

	for (t = 0; t < max_slots; t++) {
		input_mt_slot(md->input, t);
#ifdef SAMSUNG_PALM_MOTION
		report_sumsize_palm(md, 0);
#endif
		input_mt_report_slot_state(md->input,
			MT_TOOL_FINGER, false);
	}
	dev_info(md->dev, "%s\n", __func__);
}

void cyttsp5_mt_lift_all(struct cyttsp5_mt_data *md)
{
	int max = md->si->tch_abs[CY_TCH_T].max;

	if (md->num_prv_tch != 0) {
		cyttsp5_report_slot_liftoff(md, max);
		input_sync(md->input);
#ifdef TOUCH_BOOSTER
		touch_booster_release_all();
#endif
		md->num_prv_tch = 0;
	}
	dev_info(md->dev, "%s\n", __func__);
}

static void cyttsp5_get_touch_axis(struct cyttsp5_mt_data *md,
	int *axis, int size, int max, u8 *xy_data, int bofs)
{
	int nbyte;
	int next;

	for (nbyte = 0, *axis = 0, next = 0; nbyte < size; nbyte++) {
		dev_vdbg(md->dev,
			"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
			" xy_data[%d]=%02X(%d) bofs=%d\n",
			__func__, *axis, *axis, size, max, xy_data, next,
			xy_data[next], xy_data[next], bofs);
		*axis = *axis + ((xy_data[next] >> bofs) << (nbyte * 8));
		next++;
	}

	*axis &= max - 1;

	dev_vdbg(md->dev,
		"%s: *axis=%02X(%d) size=%d max=%08X xy_data=%p"
		" xy_data[%d]=%02X(%d)\n",
		__func__, *axis, *axis, size, max, xy_data, next,
		xy_data[next], xy_data[next]);
}

static void cyttsp5_get_touch_hdr(struct cyttsp5_mt_data *md,
	struct cyttsp5_touch *touch, u8 *xy_mode)
{
	struct device *dev = md->dev;
	struct cyttsp5_sysinfo *si = md->si;
	enum cyttsp5_tch_hdr hdr;

	for (hdr = CY_TCH_TIME; hdr < CY_TCH_NUM_HDR; hdr++) {
		if (!si->tch_hdr[hdr].report)
			continue;
		cyttsp5_get_touch_axis(md, &touch->hdr[hdr],
			si->tch_hdr[hdr].size,
			si->tch_hdr[hdr].max,
			xy_mode + si->tch_hdr[hdr].ofs,
			si->tch_hdr[hdr].bofs);
		dev_vdbg(dev, "%s: get %s=%04X(%d)\n", __func__,
			cyttsp5_tch_hdr_string[hdr],
			touch->hdr[hdr], touch->hdr[hdr]);
	}
#if CYTTSP5_TOUCHLOG_ENABLE
	dev_dbg(dev,
		"%s: time=%X tch_num=%d lo=%d noise=%d counter=%d\n",
		__func__,
		touch->hdr[CY_TCH_TIME],
		touch->hdr[CY_TCH_NUM],
		touch->hdr[CY_TCH_LO],
		touch->hdr[CY_TCH_NOISE],
		touch->hdr[CY_TCH_COUNTER]);
#endif
}

static void cyttsp5_get_touch(struct cyttsp5_mt_data *md,
	struct cyttsp5_touch *touch, u8 *xy_data)
{
	struct device *dev = md->dev;
	struct cyttsp5_sysinfo *si = md->si;
	enum cyttsp5_tch_abs abs;
	int tmp;
	bool flipped;
#ifdef CONFIG_KNOX_GEARPAY
	u32 ret = 0;
	/*
	 * call exynos_smc_gearpay for every touch event.
	 * g_lock_layout_active is set in the ioctl handler once
	 * Privacy Lock sends an event that it is active and
	 * it has drawn the Lock Screen layout.
	 */
	if (g_lock_layout_active) {
		/* send smc only once */
		g_lock_layout_active = 0;
		ret = exynos_smc_gearpay(TOUCH, 1);
		if (ret)
			dev_info(dev, "Touch Event SMC failure %u\n",
				ret);
		else
			dev_info(dev, "Touch Event SMC success\n");
	}
#endif
	for (abs = CY_TCH_X; abs < CY_TCH_NUM_ABS; abs++) {
		if (!si->tch_abs[abs].report)
			continue;
		cyttsp5_get_touch_axis(md, &touch->abs[abs],
			si->tch_abs[abs].size,
			si->tch_abs[abs].max,
			xy_data + si->tch_abs[abs].ofs,
			si->tch_abs[abs].bofs);
		dev_vdbg(dev, "%s: get %s=%04X(%d)\n", __func__,
			cyttsp5_tch_abs_string[abs],
			touch->abs[abs], touch->abs[abs]);
	}

	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		tmp = touch->abs[CY_TCH_X];
		touch->abs[CY_TCH_X] = touch->abs[CY_TCH_Y];
		touch->abs[CY_TCH_Y] = tmp;
		flipped = true;
	} else
		flipped = false;

	if (md->pdata->flags & CY_MT_FLAG_INV_X) {
		if (flipped)
			touch->abs[CY_TCH_X] = si->sensing_conf_data.res_y -
				touch->abs[CY_TCH_X];
		else
			touch->abs[CY_TCH_X] = si->sensing_conf_data.res_x -
				touch->abs[CY_TCH_X];
	}
	if (md->pdata->flags & CY_MT_FLAG_INV_Y) {
		if (flipped)
			touch->abs[CY_TCH_Y] = si->sensing_conf_data.res_x -
				touch->abs[CY_TCH_Y];
		else
			touch->abs[CY_TCH_Y] = si->sensing_conf_data.res_y -
				touch->abs[CY_TCH_Y];
	}

	dev_vdbg(dev, "%s: flip=%s inv-x=%s inv-y=%s x=%04X(%d) y=%04X(%d)\n",
		__func__, flipped ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_X ? "true" : "false",
		md->pdata->flags & CY_MT_FLAG_INV_Y ? "true" : "false",
		touch->abs[CY_TCH_X], touch->abs[CY_TCH_X],
		touch->abs[CY_TCH_Y], touch->abs[CY_TCH_Y]);
}

#ifdef SAMSUNG_PALM_MOTION
static void fakeEventForLargeObject(struct cyttsp5_mt_data *md,
		struct cyttsp5_touch *tch, int t)
{
	if (t != 0)
		return;

	dev_info(md->dev, "%s: \n", __func__);

	tch->abs[CY_TCH_T] = t;
	tch->abs[CY_TCH_X] = md->si->sensing_conf_data.res_x/2;
	tch->abs[CY_TCH_Y] = md->si->sensing_conf_data.res_y/2;
	tch->abs[CY_TCH_P] = 10;
	tch->abs[CY_TCH_E] = CY_EV_TOUCHDOWN;
	tch->abs[CY_TCH_O] = CY_OBJ_STANDARD_FINGER;
	tch->abs[CY_TCH_MAJ] = 100;
	tch->abs[CY_TCH_MIN] = 100;
	tch->abs[CY_TCH_OR] = 0;
}
#endif

#define ABS_PARAM(_abs, _param) \
	(md->pdata->frmwrk->abs[((_abs) * CY_NUM_ABS_SET) + (_param)])

static void cyttsp5_get_mt_touches(struct cyttsp5_mt_data *md,
		struct cyttsp5_touch *tch, int num_cur_tch)
{
	struct device *dev = md->dev;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_sysinfo *si = md->si;
	int sig, value;
	int i, j, t = 0;
	DECLARE_BITMAP(ids, MAX_TOUCH_NUMBER);
	int mt_sync_count = 0;
	static int mt_count[MAX_TOUCH_NUMBER] = {0, };
#ifdef SAMSUNG_PALM_MOTION
	u16 sum_maj_mnr;
#endif

	if (!cd->md.input_device_registered) {
		dev_err(dev,
			"%s: device is not registerd.", __func__);
		goto out;
	}

	bitmap_zero(ids, MAX_TOUCH_NUMBER);
	memset(tch->abs, 0, sizeof(tch->abs));

	for (i = 0; i < num_cur_tch; i++) {
#ifdef SAMSUNG_PALM_MOTION
		if (md->largeobj) {
#ifdef TOUCH_BOOSTER
			touch_booster_release();
#endif
			fakeEventForLargeObject(md, tch, i);
		}
		else
#endif
		cyttsp5_get_touch(md, tch, si->xy_data +
			(i * si->desc.tch_record_size));

		if (tch->abs[CY_TCH_T] < ABS_PARAM(CY_ABS_ID_OST, CY_MIN_OST) ||
			tch->abs[CY_TCH_T] >
			ABS_PARAM(CY_ABS_ID_OST, CY_MAX_OST)) {
			dev_err(dev, "%s: tch=%d -> bad trk_id=%d max_id=%d\n",
				__func__, i, tch->abs[CY_TCH_T],
				ABS_PARAM(CY_ABS_ID_OST, CY_MAX_OST));
			mt_sync_count++;
			continue;
		}

		/* use 0 based track id's */
		sig = ABS_PARAM(CY_ABS_ID_OST, CY_SIGNAL_OST);
		if (sig != CY_IGNORE_VALUE) {
			t = tch->abs[CY_TCH_T]
				- ABS_PARAM(CY_ABS_ID_OST, CY_MIN_OST);

#ifdef SAMSUNG_TOUCH_MODE
			if (tch->abs[CY_TCH_E] == CY_EV_TOUCHDOWN) {
				input_report_key(md->input, BTN_TOUCH, 1);
				input_report_key(md->input, BTN_TOOL_FINGER, 1);
			}
#endif
			if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF) {
				if (num_cur_tch == 1) {
#ifdef SAMSUNG_TOUCH_MODE
					input_report_key(md->input,
							BTN_TOUCH, 0);
					input_report_key(md->input,
							BTN_TOOL_FINGER, 0);
#endif
#ifdef TOUCH_BOOSTER
					touch_booster_release();
#endif
#ifdef CONFIG_SLEEP_MONITOR
					if (cd->release_cnt < 0xffff)
						cd->release_cnt++;
#endif
				}
				goto cyttsp5_get_mt_touches_pr_tch;
			}

			cyttsp5_input_report(md->input, sig,
						t, tch->abs[CY_TCH_O]);
			__set_bit(t, ids);
		}

		/* all devices: position and pressure fields */
		for (j = 0; j <= CY_ABS_W_OST; j++) {
			if (!si->tch_abs[j].report)
				continue;
			sig = ABS_PARAM(CY_ABS_X_OST + j, CY_SIGNAL_OST);
			if (sig == CY_IGNORE_VALUE)
				continue;
			value = tch->abs[CY_TCH_X + j];
			input_report_abs(md->input, sig, value);
		}

		/* Get the extended touch fields */
#ifdef SAMSUNG_PALM_MOTION
		sum_maj_mnr = 0;
#endif
		for (j = 0; j < CY_NUM_EXT_TCH_FIELDS; j++) {
			if (!si->tch_abs[j].report)
				continue;
			sig = ABS_PARAM((CY_ABS_MAJ_OST + j), CY_SIGNAL_OST);
			if (sig == CY_IGNORE_VALUE)
				continue;
			value = tch->abs[CY_TCH_MAJ + j];
			input_report_abs(md->input, sig, value);

#ifdef SAMSUNG_PALM_MOTION
			if (sig == ABS_MT_TOUCH_MAJOR
				|| sig == ABS_MT_TOUCH_MINOR) {
				sum_maj_mnr += value;
			}
#endif
		}

		mt_sync_count++;

cyttsp5_get_mt_touches_pr_tch:

#if CYTTSP5_TOUCHLOG_ENABLE
		dev_dbg(dev,
			"%s: t=%d x=%d y=%d z=%d M=%d m=%d o=%d e=%d obj=%d tip=%d\n",
			__func__, t,
			tch->abs[CY_TCH_X],
			tch->abs[CY_TCH_Y],
			tch->abs[CY_TCH_P],
			tch->abs[CY_TCH_MAJ],
			tch->abs[CY_TCH_MIN],
			tch->abs[CY_TCH_OR],
			tch->abs[CY_TCH_E],
			tch->abs[CY_TCH_O],
			tch->abs[CY_TCH_TIP]);

		if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF)
			dev_dbg(dev, "%s: t=%d e=%d lift-off\n",
				__func__, t, tch->abs[CY_TCH_E]);

#else /* CYTTSP5_TOUCHLOG_ENABLE == 0 */

		if (tch->abs[CY_TCH_E] == CY_EV_TOUCHDOWN) {
#ifdef TOUCH_BOOSTER
#ifdef SAMSUNG_PALM_MOTION
			if (md->largeobj == false)
#endif
				touch_booster_press();
#endif
			dev_info(dev, "P [%d] x=%d y=%d z=%d M=%d m=%d\n",
				t, tch->abs[CY_TCH_X],
				tch->abs[CY_TCH_Y],
				tch->abs[CY_TCH_P],
				tch->abs[CY_TCH_MAJ],
				tch->abs[CY_TCH_MIN]);
		} else if (tch->abs[CY_TCH_E] == CY_EV_LIFTOFF) {
			dev_info(dev, "R [%d] x=%d y=%d C=%d V=%02x\n",
				t, tch->abs[CY_TCH_X],
				tch->abs[CY_TCH_Y],
				mt_count[t],
				md->fw_ver_ic);
			mt_count[t] = 0;
		} else if (tch->abs[CY_TCH_E] == CY_EV_MOVE) {
			mt_count[t]++;
			dev_dbg(dev, "M [%d] x=%d y=%d z=%d M=%d m=%d\n",
				t, tch->abs[CY_TCH_X],
				tch->abs[CY_TCH_Y],
				tch->abs[CY_TCH_P],
				tch->abs[CY_TCH_MAJ],
				tch->abs[CY_TCH_MIN]);
		}
#endif /* CYTTSP5_TOUCHLOG_ENABLE */
	}

#ifdef SAMSUNG_PALM_MOTION
	report_sumsize_palm(md, md->largeobj);
#endif
	cyttsp5_final_sync(md->input, MAX_TOUCH_NUMBER,
				mt_sync_count, ids);

	md->num_prv_tch = num_cur_tch;

out:
	return;
}


/* read xy_data for all current touches */
static int cyttsp5_xy_worker(struct cyttsp5_mt_data *md)
{
	struct device *dev = md->dev;
	struct cyttsp5_sysinfo *si = md->si;
	struct cyttsp5_touch tch;
	u8 num_cur_tch;

	cyttsp5_get_touch_hdr(md, &tch, si->xy_mode + 3);

	num_cur_tch = tch.hdr[CY_TCH_NUM];
	if (num_cur_tch > MAX_TOUCH_NUMBER) {
		dev_err(dev, "%s: Num touch err detected (n=%d)\n",
			__func__, num_cur_tch);
		num_cur_tch = MAX_TOUCH_NUMBER;
	}

	if (tch.hdr[CY_TCH_LO]) {
		dev_dbg(dev, "%s: Large area detected\n", __func__);
		if (md->pdata->flags & CY_MT_FLAG_NO_TOUCH_ON_LO)
			num_cur_tch = 0;
	}

#ifdef SAMSUNG_PALM_MOTION
	if (!md->largeobj) {
		if (tch.hdr[CY_TCH_LO]) {
			md->largeobj = true;
			cyttsp5_get_mt_touches(md, &tch, 1);
			goto skip_num_cur_tch;
		}
	} else {
		if (!num_cur_tch && !tch.hdr[CY_TCH_LO]) {
			md->largeobj = false;
			cyttsp5_get_mt_touches(md, &tch, 1);
			cyttsp5_mt_lift_all(md);
		}
		goto skip_num_cur_tch;
	}
#endif

	/* extract xy_data for all currently reported touches */
	dev_vdbg(dev, "%s: extract data num_cur_tch=%d\n", __func__,
		num_cur_tch);
	if (num_cur_tch)
		cyttsp5_get_mt_touches(md, &tch, num_cur_tch);
#if !defined(SAMSUNG_PALM_MOTION)
	else
		cyttsp5_mt_lift_all(md);
#endif

#ifdef SAMSUNG_PALM_MOTION
skip_num_cur_tch:
#endif
	return 0;
}

static void cyttsp5_mt_send_dummy_event(struct cyttsp5_mt_data *md)
{
	unsigned long ids = 0;

	dev_info(md->dev, "%s: Sending wake up event.\n", __func__);

	/* for easy wakeup */
	cyttsp5_input_report(md->input, ABS_MT_TRACKING_ID,
			0, CY_OBJ_STANDARD_FINGER);
	input_report_abs(md->input, ABS_MT_POSITION_X,\
			md->si->sensing_conf_data.res_x >> 1);
	input_report_abs(md->input, ABS_MT_POSITION_Y,\
			md->si->sensing_conf_data.res_y >> 1);
	input_report_key(md->input, KEY_WAKEUP, 1);
	cyttsp5_final_sync(md->input, 0, 1, &ids);

	msleep(10);

	cyttsp5_report_slot_liftoff(md, 1);
	input_report_key(md->input, KEY_WAKEUP, 0);
	cyttsp5_final_sync(md->input, 1, 1, &ids);
}

int cyttsp5_mt_attention(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;
	int rc;

	if (md->si->xy_mode[2] !=  md->si->desc.tch_report_id)
		return 0;

	mutex_lock(&md->mt_lock);
	if (md->prevent_touch) {
		mutex_unlock(&md->mt_lock);

		dev_dbg(dev, "%s: touch is now prevented\n", __func__);
		return 0;
	}

	/* core handles handshake */
	rc = cyttsp5_xy_worker(md);
	mutex_unlock(&md->mt_lock);
	if (rc < 0)
		dev_err(dev, "%s: xy_worker error r=%d\n", __func__, rc);

	return rc;
}

int cyttsp5_mt_wake_attention(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;

	mutex_lock(&md->mt_lock);
	cyttsp5_mt_send_dummy_event(md);
	mutex_unlock(&md->mt_lock);
	return 0;
}

int cyttsp5_startup_attention(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;

	mutex_lock(&md->mt_lock);
	md->largeobj = false;
	cyttsp5_mt_lift_all(md);
	mutex_unlock(&md->mt_lock);

	return 0;
}

int cyttsp5_prevent_touch(struct device *dev, bool prevent)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;

	mutex_lock(&md->mt_lock);
	md->prevent_touch = prevent;
	if (prevent)
		cyttsp5_mt_lift_all(md);
	mutex_unlock(&md->mt_lock);

	return 0;
}
extern int cyttsp5_core_start(struct device *dev);
static int cyttsp5_mt_open(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	int rc;

	dev_info(dev, "%s:\n", __func__);

	if (!cd->probe_done) {
		dev_info(dev, "%s: probe is not done.\n", __func__);
		return -1;
	}

	if (cd->device_enabled) {
		dev_info(dev, "%s: already device enabled.\n", __func__);
		return 0;
	}

	if (cd->irq_wake) {
		dev_err(dev, "%s: irq_wake is enable\n", __func__);
		return 0;
	}

	/* set up touch call back */
	_cyttsp5_subscribe_attention(dev, CY_ATTEN_IRQ, CY_MODULE_MT,
		cyttsp5_mt_attention, CY_MODE_OPERATIONAL);

	/* set up startup call back */
	_cyttsp5_subscribe_attention(dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp5_startup_attention, 0);

	/* set up wakeup call back */
	_cyttsp5_subscribe_attention(dev, CY_ATTEN_WAKE, CY_MODULE_MT,
		cyttsp5_mt_wake_attention, 0);

	rc = cyttsp5_core_start(dev);
	if (rc < 0)
		dev_err(dev, "%s: device open failed.\n", __func__);
	else
		cd->device_enabled = true;

	dev_info(dev, "%s: done\n", __func__);

	return 0;
}

extern int cyttsp5_core_stop(struct device *dev);
static void cyttsp5_mt_close(struct input_dev *input)
{
	struct device *dev = input->dev.parent;
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;

	dev_info(dev, "%s:\n", __func__);

	if (!cd->device_enabled) {
		dev_err(dev, "%s: already device disabled.\n", __func__);
		return;
	}

	if (cd->stay_awake) {
		dev_err(dev, "%s: doing firmware update\n", __func__);
		return;
	}

	if (cd->irq_wake) {
		dev_err(dev, "%s: irq_wake is enable\n", __func__);
		return;
	}

	_cyttsp5_unsubscribe_attention(dev, CY_ATTEN_IRQ, CY_MODULE_MT,
		cyttsp5_mt_attention, CY_MODE_OPERATIONAL);

	_cyttsp5_unsubscribe_attention(dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp5_startup_attention, 0);

	cyttsp5_mt_lift_all(md);

	cyttsp5_core_stop(dev);

	cd->device_enabled = false;

	dev_info(dev, "%s: done\n", __func__);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cyttsp5_mt_early_suspend(struct early_suspend *h)
{
	struct cyttsp5_mt_data *md =
		container_of(h, struct cyttsp5_mt_data, es);
	struct device *dev = md->dev;

	/* pm_runtime_put_sync(dev); */
	cyttsp5_core_suspend(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = true;
	mutex_unlock(&md->mt_lock);
}

static void cyttsp5_mt_late_resume(struct early_suspend *h)
{
	struct cyttsp5_mt_data *md =
		container_of(h, struct cyttsp5_mt_data, es);
	struct device *dev = md->dev;

	/*pm_runtime_get(dev); */
	cyttsp5_core_resume(dev);

	mutex_lock(&md->mt_lock);
	md->is_suspended = false;
	mutex_unlock(&md->mt_lock);
}

static void cyttsp5_setup_early_suspend(struct cyttsp5_mt_data *md)
{
	md->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	md->es.suspend = cyttsp5_mt_early_suspend;
	md->es.resume = cyttsp5_mt_late_resume;

	register_early_suspend(&md->es);
}
#endif

static int cyttsp5_setup_input_device(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;
	int signal = CY_IGNORE_VALUE;
	int max_x, max_y, max_p, min, max;
	int max_x_tmp, max_y_tmp;
	int i;

	dev_dbg(dev, "%s: Initialize event signals\n", __func__);
	__set_bit(EV_ABS, md->input->evbit);
	__set_bit(EV_KEY, md->input->evbit);
#ifdef INPUT_PROP_DIRECT
	__set_bit(INPUT_PROP_DIRECT, md->input->propbit);
#endif
#ifdef SAMSUNG_TOUCH_MODE
	__set_bit(BTN_TOUCH, md->input->keybit);
#endif

    __set_bit(KEY_SLEEP, md->input->keybit);

	/* If virtualkeys enabled, don't use all screen */
	if (md->pdata->flags & CY_MT_FLAG_VKEYS) {
		max_x_tmp = md->pdata->vkeys_x;
		max_y_tmp = md->pdata->vkeys_y;
	} else {
		max_x_tmp = md->si->sensing_conf_data.res_x;
		max_y_tmp = md->si->sensing_conf_data.res_y;
	}

	/* get maximum values from the sysinfo data */
	if (md->pdata->flags & CY_MT_FLAG_FLIP) {
		max_x = max_y_tmp - 1;
		max_y = max_x_tmp - 1;
	} else {
		max_x = max_x_tmp - 1;
		max_y = max_y_tmp - 1;
	}
	max_p = md->si->sensing_conf_data.max_z;

	/* set event signal capabilities */
	for (i = 0; i < (md->pdata->frmwrk->size / CY_NUM_ABS_SET); i++) {
		signal = ABS_PARAM(i, CY_SIGNAL_OST);
		if (signal != CY_IGNORE_VALUE) {
			__set_bit(signal, md->input->absbit);
			min = ABS_PARAM(i, CY_MIN_OST);
			max = ABS_PARAM(i, CY_MAX_OST);
			if (i == CY_ABS_ID_OST) {
				/* shift track ids down to start at 0 */
				max = max - min;
				/*min = min - min;*/
				min = 0;
			} else if (i == CY_ABS_X_OST)
				max = max_x;
			else if (i == CY_ABS_Y_OST)
				max = max_y;
			else if (i == CY_ABS_P_OST)
				max = max_p;
			input_set_abs_params(md->input, signal, min, max,
				ABS_PARAM(i, CY_FUZZ_OST),
				ABS_PARAM(i, CY_FLAT_OST));
			dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
				__func__, signal, min, max);
		}
	}

	input_set_abs_params(md->input, ABS_X, 0, max_x+1, 0, 0);
	input_set_abs_params(md->input, ABS_Y, 0, max_y+1, 0, 0);

#if defined(SAMSUNG_PALM_MOTION)
	input_set_abs_params(md->input, signal = ABS_MT_PALM,
		min = 0, max = 1, 0, 0);
	dev_dbg(dev, "%s: register signal=%02X min=%d max=%d\n",
				__func__, signal, min, max);
#endif

    input_set_capability(md->input, EV_KEY, KEY_SLEEP);
	input_mt_init_slots(md->input,
			md->si->tch_abs[CY_TCH_T].max, 0);

	return 0;
}

static int cyttsp5_setup_input_attention(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;
	int rc;

	md->si = _cyttsp5_request_sysinfo(dev);
	if (!md->si)
		return -EINVAL;

	rc = cyttsp5_setup_input_device(dev);

	_cyttsp5_unsubscribe_attention(dev, CY_ATTEN_STARTUP, CY_MODULE_MT,
		cyttsp5_setup_input_attention, 0);

	return rc;
}

//extern int get_panel_id(void);
#define S6E36W2X01_ID3		0x11
#define PANEL_ID3_MASK		0xff
int cyttsp5_mt_probe(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;
	struct cyttsp5_platform_data *pdata = dev_get_platdata(dev);
	struct cyttsp5_mt_platform_data *mt_pdata;
	int rc = 0;

	if (!pdata || !pdata->mt_pdata) {
		dev_err(dev, "%s: Missing platform data\n", __func__);
		rc = -ENODEV;
		goto error_no_pdata;
	}
	mt_pdata = pdata->mt_pdata;

	mutex_init(&md->mt_lock);
	md->dev = dev;
	md->pdata = mt_pdata;

	/* Create the input device and register it. */
	dev_vdbg(dev, "%s: Create the input device and register it\n",
		__func__);
	md->input = input_allocate_device();
	if (!md->input) {
		dev_err(dev, "%s: Error, failed to allocate input device\n",
			__func__);
		rc = -ENOSYS;
		goto error_alloc_failed;
	}

	/*if ((get_panel_id() & PANEL_ID3_MASK) == S6E36W2X01_ID3) {
		dev_info(dev, "%s: detected 8x8 panel.\n", __func__);
		md->pdata->flags = 0x00;
	}*/

	md->input->name = SEC_TSP_NAME;
	scnprintf(md->phys, sizeof(md->phys)-1, "%s/input0", CYTTSP5_MT_NAME);
	md->input->phys = md->phys;
	md->input->dev.parent = md->dev;
	md->input->open = cyttsp5_mt_open;
	md->input->close = cyttsp5_mt_close;
	input_set_drvdata(md->input, md);

	/* get sysinfo */
	md->si = _cyttsp5_request_sysinfo(dev);

	if (md->si) {
		rc = cyttsp5_setup_input_device(dev);
		if (rc)
			goto error_init_input;
	} else {
		dev_err(dev, "%s: Fail get sysinfo pointer from core p=%p\n",
			__func__, md->si);
		_cyttsp5_subscribe_attention(dev, CY_ATTEN_STARTUP,
			CY_MODULE_MT, cyttsp5_setup_input_attention, 0);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	cyttsp5_setup_early_suspend(md);
#endif

	return 0;

error_init_input:
	input_free_device(md->input);
error_alloc_failed:
error_no_pdata:
	dev_err(dev, "%s failed.\n", __func__);
	return rc;
}
EXPORT_SYMBOL(cyttsp5_mt_probe);

int cyttsp5_mt_release(struct device *dev)
{
	struct cyttsp5_core_data *cd = dev_get_drvdata(dev);
	struct cyttsp5_mt_data *md = &cd->md;

#ifdef CONFIG_HAS_EARLYSUSPEND
	/*
	 * This check is to prevent pm_runtime usage_count drop below zero
	 * because of removing the module while in suspended state
	 */
	/*if (md->is_suspended)
		pm_runtime_get_noresume(dev);*/

	unregister_early_suspend(&md->es);
#endif

	if (md->input_device_registered) {
		input_unregister_device(md->input);
	} else {
		input_free_device(md->input);
		_cyttsp5_unsubscribe_attention(dev, CY_ATTEN_STARTUP,
			CY_MODULE_MT, cyttsp5_setup_input_attention, 0);
	}

	return 0;
}
EXPORT_SYMBOL(cyttsp5_mt_release);
