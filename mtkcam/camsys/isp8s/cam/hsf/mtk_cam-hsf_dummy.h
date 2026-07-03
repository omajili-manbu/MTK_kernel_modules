/* SPDX-License-Identifier: GPL-2.0
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
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/version.h>

int rproc_bootx(struct rproc *rproc, unsigned int uid);
int rproc_shutdownx(struct rproc *rproc, unsigned int uid);
int mtk_ccu_rproc_ipc_send(struct platform_device *pdev,
	enum mtk_ccu_feature_type featureType,
	uint32_t msgId, void *inDataPtr, uint32_t inDataSize);
