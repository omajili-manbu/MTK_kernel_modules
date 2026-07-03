/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Louis Kuo <louis.kuo@mediatek.com>
 */
#include <linux/component.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>

#include <linux/pm_runtime.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include "mtk_cam-hsf_dummy.h"

int rproc_bootx(struct rproc *rproc, unsigned int uid)
{
	int ret = 0;

	(void)rproc;
	(void)uid;

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_bootx);

int rproc_shutdownx(struct rproc *rproc, unsigned int uid)
{
	int ret = 0;

	(void)rproc;
	(void)uid;

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_shutdownx);

int mtk_ccu_rproc_ipc_send(struct platform_device *pdev,
	enum mtk_ccu_feature_type featureType,
	uint32_t msgId, void *inDataPtr, uint32_t inDataSize)
{
	int ret = 0;

	(void)pdev;
	(void)featureType;
	(void)msgId;
	(void)inDataPtr;
	(void)inDataSize;

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_ccu_rproc_ipc_send);


MODULE_DESCRIPTION("Camera ISP driver");
MODULE_LICENSE("GPL");
