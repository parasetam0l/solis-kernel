/******************************************************************************
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *	Intel Corporation.
 *	linux-mei@linux.intel.com
 *	http://www.intel.com
 *
 * BSD LICENSE
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <crypto/hash.h>
#include <linux/scatterlist.h>
#include <linux/sizes.h>

#include <linux/rpmb.h>

static const char id[] = "RPMB:SIM";
#define CAPACITY_UNIT SZ_128K
#define CAPACITY_MIN  SZ_128K
#define CAPACITY_MAX  SZ_16M
#define BLK_UNIT      SZ_256

static unsigned int max_wr_blks = 1;
module_param(max_wr_blks, uint, 0644);
MODULE_PARM_DESC(max_wr_blks,
	"max blocks that can be written in a single command (default: 1)");

static unsigned int daunits = 1;
module_param(daunits, uint, 0644);
MODULE_PARM_DESC(daunits,
	"number of data area units of 128K (default: 1)");

struct blk {
	u8 data[BLK_UNIT];
};

/**
 * struct rpmb_sim_dev
 *
 * @dev:  back pointer to the platform device
 * @rdev: rpmb device
 * @auth_key: Authentication key register which is used to authenticate
 *            accesses when MAC is calculated;
 * @auth_key_set: true if auth key was set
 * @write_counter: Counter value for the total amount of successful
 *             authenticated data write requests made by the host.
 *             The initial value of this register after production is 00000000h.
 *             The value will be incremented by one along with each successful
 *             programming access. The value cannot be reset. After the counter
 *             has reached the maximum value of FFFFFFFFh,
 *             it will not be incremented anymore (overflow prevention)
 * @hash_tfm:  hmac(sha256) tfm
 *
 * @capacity: size of the partition in bytes multiple of 128K
 * @blkcnt:   block count
 * @da:       data area in blocks
 */
struct rpmb_sim_dev {
	struct device *dev;
	struct rpmb_dev *rdev;
	u8 auth_key[32];
	bool auth_key_set;
	u32 write_counter;
	struct crypto_shash *hash_tfm;

	size_t capacity;
	size_t blkcnt;
	struct blk *da;
};

static __be16 op_result(struct rpmb_sim_dev *rsdev, u16 result)
{
	if (!rsdev->auth_key_set)
		return cpu_to_be16(RPMB_ERR_NO_KEY);

	if (rsdev->write_counter == 0xFFFFFFFF)
		result |=  RPMB_ERR_COUNTER_EXPIRED;

	return cpu_to_be16(result);
}

static __be16 req_to_resp(u16 req)
{
	return cpu_to_be16(RPMB_REQ2RESP(req));
}

static int rpmb_sim_calc_hmac(struct rpmb_sim_dev *rsdev,
			      struct rpmb_frame *frames,
			      unsigned int blks, u8 *mac)
{
	SHASH_DESC_ON_STACK(desc, rsdev->hash_tfm);
	int i;
	int ret;

	desc->tfm = rsdev->hash_tfm;
	desc->flags = 0;

	ret = crypto_shash_init(desc);
	if (ret)
		goto out;

	for (i = 0; i < blks; i++) {
		ret = crypto_shash_update(desc, frames[i].data, hmac_data_len);
		if (ret)
			goto out;
	}
	ret = crypto_shash_final(desc, mac);
out:
	if (ret)
		dev_err(rsdev->dev, "digest error = %d", ret);

	return ret;
}

static int rpmb_op_not_programmed(struct rpmb_sim_dev *rsdev,
				  struct rpmb_data *rpmbd)
{
	struct rpmb_frame *in_frame, *out_frame;
	u16 req;

	in_frame = &rpmbd->in_frames[0];
	req = be16_to_cpu(in_frame->req_resp);

	out_frame = &rpmbd->out_frames[0];

	out_frame->req_resp = req_to_resp(req);
	out_frame->result = op_result(rsdev, RPMB_ERR_NO_KEY);

	dev_err(rsdev->dev, "not programmed\n");

	return 0;
}

static int rpmb_op_program_key(struct rpmb_sim_dev *rsdev,
			       struct rpmb_data *rpmbd)
{
	struct rpmb_frame *in_frame, *out_frame;
	u16 req;
	int ret;
	u16 err = RPMB_ERR_OK;

	in_frame = rpmbd->in_frames;
	req = be16_to_cpu(in_frame[0].req_resp);

	if (req != RPMB_PROGRAM_KEY)
		return -EINVAL;

	if (rsdev->auth_key_set) {
		dev_err(rsdev->dev, "key allread set\n");
		err = RPMB_ERR_WRITE;
		goto out;
	}

	ret = crypto_shash_setkey(rsdev->hash_tfm, in_frame[0].key_mac, 32);
	if (ret) {
		dev_err(rsdev->dev, "set key failed = %d\n", ret);
		err = RPMB_ERR_GENERAL;
		goto out;
	}

	dev_dbg(rsdev->dev, "digest size %u",
		crypto_shash_digestsize(rsdev->hash_tfm));

	memcpy(rsdev->auth_key, in_frame[0].key_mac, 32);
	rsdev->auth_key_set = true;
out:
	out_frame = rpmbd->out_frames;
	memset(out_frame, 0, sizeof(struct rpmb_frame));

	out_frame[0].req_resp = req_to_resp(req);
	out_frame[0].result = op_result(rsdev, err);

	return 0;
}


static int rpmb_op_get_wr_conter(struct rpmb_sim_dev *rsdev,
				 struct rpmb_data *rpmbd)
{
	struct rpmb_frame *in_frame, *out_frame;
	u16 req;
	u16 err;

	in_frame = rpmbd->in_frames;
	req = be16_to_cpu(in_frame[0].req_resp);

	if (req != RPMB_GET_WRITE_COUNTER)
		return -EINVAL;

	out_frame = rpmbd->out_frames;
	memset(out_frame, 0, sizeof(struct rpmb_frame));

	out_frame[0].req_resp = req_to_resp(req);
	out_frame[0].write_counter = cpu_to_be32(rsdev->write_counter);
	memcpy(out_frame[0].nonce, in_frame[0].nonce, 16);

	err = RPMB_ERR_OK;
	out_frame[0].result = op_result(rsdev, err);

	if (rpmb_sim_calc_hmac(rsdev, out_frame, 1, out_frame->key_mac))
		err = RPMB_ERR_READ;

	out_frame[0].result = op_result(rsdev, err);

	return 0;
}

static int rpmb_op_write_data(struct rpmb_sim_dev *rsdev,
			      struct rpmb_data *rpmbd)
{
	struct rpmb_frame *in_frame, *out_frame;
	u8 mac[32];
	u16 req, err, addr, blks;
	unsigned int i;
	int ret = 0;

	in_frame = rpmbd->in_frames;
	req = be16_to_cpu(in_frame[0].req_resp);

	if (req != RPMB_WRITE_DATA)
		return -EINVAL;


	if (rsdev->write_counter == 0xFFFFFFFF) {
		err = RPMB_ERR_WRITE;
		goto out;
	}

	blks = be16_to_cpu(in_frame[0].block_count);
	if (blks == 0 || blks > rpmbd->in_frames_cnt) {
		ret = -EINVAL;
		err = RPMB_ERR_GENERAL;
		goto out;
	}

	if (blks > max_wr_blks) {
		err = RPMB_ERR_WRITE;
		goto out;
	}


	addr = be16_to_cpu(in_frame[0].addr);
	if (addr >= rsdev->blkcnt) {
		err = RPMB_ERR_ADDRESS;
		goto out;
	}

	if (rpmb_sim_calc_hmac(rsdev, in_frame, blks, mac)) {
		err = RPMB_ERR_AUTH;
		goto out;
	}

	/* mac is in the last frame */
	if (memcmp(mac, in_frame[blks - 1].key_mac, sizeof(mac)) != 0) {
		err = RPMB_ERR_AUTH;
		goto out;
	}

	if (be32_to_cpu(in_frame[0].write_counter) != rsdev->write_counter) {
		err = RPMB_ERR_COUNTER;
		goto out;
	}

	if (addr + blks > rsdev->blkcnt) {
		err = RPMB_ERR_WRITE;
		goto out;
	}

	err = RPMB_ERR_OK;
	for (i = 0; i < blks; i++)
		memcpy(rsdev->da[addr + i].data, in_frame[i].data, BLK_UNIT);

	rsdev->write_counter++;

out:
	out_frame = &rpmbd->out_frames[0];
	memset(out_frame, 0, sizeof(struct rpmb_frame));

	if (err == RPMB_ERR_OK) {
		out_frame[0].write_counter = cpu_to_be32(rsdev->write_counter);
		memcpy(out_frame[0].key_mac, mac, sizeof(mac));
	}
	out_frame[0].req_resp = req_to_resp(req);
	out_frame[0].result = op_result(rsdev, err);

	return ret;
}

static int rpmb_op_read_data(struct rpmb_sim_dev *rsdev,
			     struct rpmb_data *rpmbd)
{
	struct rpmb_frame *in_frame, *out_frame;
	u8 mac[32];
	u16 req, err, addr, blks;
	unsigned int i;
	int ret;

	in_frame = rpmbd->in_frames;
	req = be16_to_cpu(in_frame[0].req_resp);
	if (req != RPMB_READ_DATA)
		return -EINVAL;


	out_frame = rpmbd->out_frames;

	blks = be16_to_cpu(in_frame[0].block_count);
	if (blks == 0 || blks > rpmbd->out_frames_cnt) {
		ret = -EINVAL;
		memset(out_frame, 0, sizeof(struct rpmb_frame));
		err = RPMB_ERR_READ;
		goto out;
	}

	ret = 0;
	memset(out_frame, 0, sizeof(struct rpmb_frame) * blks);

	addr = be16_to_cpu(in_frame[0].addr);
	if (addr >= rsdev->blkcnt) {
		err = RPMB_ERR_ADDRESS;
		goto out;
	}

	if (addr + blks > rsdev->blkcnt) {
		err = RPMB_ERR_READ;
		goto out;
	}

	for (i = 0; i < blks; i++) {
		memcpy(out_frame[i].data, rsdev->da[addr + i].data, BLK_UNIT);
		memcpy(out_frame[i].nonce, in_frame[0].nonce, 16);
		out_frame[i].req_resp = req_to_resp(req);
		out_frame[i].addr = in_frame[0].addr;
		out_frame[i].block_count = cpu_to_be16(blks);
	}

	if (rpmb_sim_calc_hmac(rsdev, out_frame, blks, mac)) {
		err = RPMB_ERR_AUTH;
		goto out;
	}

	memcpy(out_frame[blks - 1].key_mac, mac, sizeof(mac));

	err = RPMB_ERR_OK;
out:

	/* FIXME: not sure if this has to be updated in each frame */
	out_frame[0].result = op_result(rsdev, err);


	return ret;
}

static int rpmb_sim_sequence(struct rpmb_sim_dev *rsdev,
			     struct rpmb_data *rpmbd)
{
	u16 type;
	int ret;

	type = rpmbd->req_type;

	if (rpmbd->out_frames == NULL || rpmbd->in_frames == NULL ||
	    rpmbd->out_frames_cnt == 0 || rpmbd->in_frames_cnt == 0)
		return -EINVAL;

	if (rsdev->auth_key_set == false && type != RPMB_PROGRAM_KEY)
		return rpmb_op_not_programmed(rsdev, rpmbd);

	switch (type) {
	case RPMB_PROGRAM_KEY:
		ret = rpmb_op_program_key(rsdev, rpmbd);
		break;
	case RPMB_WRITE_DATA:
		ret = rpmb_op_write_data(rsdev, rpmbd);
		break;
	case RPMB_GET_WRITE_COUNTER:
		ret = rpmb_op_get_wr_conter(rsdev, rpmbd);
		break;
	case RPMB_READ_DATA:
		ret = rpmb_op_read_data(rsdev, rpmbd);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}
out:
	return ret;
}

static int rpmb_sim_send_rpmb_req(struct device *dev, struct rpmb_data *rpmbd)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rpmb_sim_dev *rsdev = platform_get_drvdata(pdev);

	if (!rsdev)
		return -EINVAL;

	if (!rpmbd)
		return -EINVAL;

	return rpmb_sim_sequence(rsdev, rpmbd);
}

static struct rpmb_ops rpmb_sim_ops = {
	.send_rpmb_req = rpmb_sim_send_rpmb_req,
	.type = RPMB_TYPE_EMMC,
};

static int rpmb_sim_hmac_256_alloc(struct rpmb_sim_dev *rsdev)
{
	struct crypto_shash *hash_tfm;

	hash_tfm = crypto_alloc_shash("hmac(sha256)", 0, 0);
	if (IS_ERR(hash_tfm))
		return PTR_ERR(hash_tfm);

	rsdev->hash_tfm = hash_tfm;

	dev_dbg(rsdev->dev, "hamac(sha256) registered\n");
	return 0;
}

static void rpmb_sim_hmac_256_free(struct rpmb_sim_dev *rsdev)
{
	if (rsdev->hash_tfm)
		crypto_free_shash(rsdev->hash_tfm);

	rsdev->hash_tfm = NULL;
}

static int rpmb_sim_probe(struct platform_device *pdev)
{
	struct rpmb_sim_dev *rsdev;
	int ret;

	rsdev = kzalloc(sizeof(struct rpmb_sim_dev), GFP_KERNEL);
	if (!rsdev)
		return -ENOMEM;

	rsdev->dev = &pdev->dev;

	ret = rpmb_sim_hmac_256_alloc(rsdev);
	if (ret)
		goto err;

	rsdev->capacity = CAPACITY_UNIT * daunits;
	rsdev->blkcnt  = rsdev->capacity / BLK_UNIT;
	rsdev->da = kzalloc(rsdev->capacity, GFP_KERNEL);
	if (!rsdev->da) {
		ret = -ENOMEM;
		goto err;
	}

	rpmb_sim_ops.dev_id_len = strlen(id);
	rpmb_sim_ops.dev_id = id;
	rpmb_sim_ops.reliable_wr_cnt = max_wr_blks;

	rsdev->rdev = rpmb_dev_register(rsdev->dev, &rpmb_sim_ops);
	if (IS_ERR(rsdev->rdev)) {
		ret = PTR_ERR(rsdev->rdev);
		goto err;
	}

	dev_info(&pdev->dev, "registered RPMB capacity = %zu of %zu blocks\n",
		 rsdev->capacity, rsdev->blkcnt);

	platform_set_drvdata(pdev, rsdev);

	return 0;
err:
	rpmb_sim_hmac_256_free(rsdev);
	if (rsdev)
		kfree(rsdev->da);
	kfree(rsdev);
	return ret;
}

static int rpmb_sim_remove(struct platform_device *pdev)
{
	struct rpmb_sim_dev *rsdev;

	rsdev = platform_get_drvdata(pdev);

	rpmb_dev_unregister(rsdev->dev);

	platform_set_drvdata(pdev, NULL);

	rpmb_sim_hmac_256_free(rsdev);

	kfree(rsdev->da);
	kfree(rsdev);
	return 0;
}

static struct platform_driver rpmb_sim_driver = {
	.driver = {
		.name  = "rpmb_sim",
		.owner = THIS_MODULE,
	},
	.probe         = rpmb_sim_probe,
	.remove        = rpmb_sim_remove,
};

static struct platform_device *rpmb_sim_pdev;

static int __init rpmb_sim_init(void)
{
	int ret;

	rpmb_sim_pdev = platform_device_register_simple("rpmb_sim", -1,
							NULL, 0);

	if (IS_ERR(rpmb_sim_pdev))
		return PTR_ERR(rpmb_sim_pdev);

	ret = platform_driver_register(&rpmb_sim_driver);
	if (ret)
		platform_device_unregister(rpmb_sim_pdev);

	return ret;
}

static void __exit rpmb_sim_exit(void)
{
	platform_device_unregister(rpmb_sim_pdev);
	platform_driver_unregister(&rpmb_sim_driver);
}

module_init(rpmb_sim_init);
module_exit(rpmb_sim_exit);

MODULE_AUTHOR("Tomas Winkler <tomas.winkler@intel.com");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("platform:rpmb_sim");
