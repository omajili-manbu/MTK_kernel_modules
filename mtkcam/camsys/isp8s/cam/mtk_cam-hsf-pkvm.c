/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/version.h>
#if (KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE)
#include <asm/kvm_pkvm_module.h>
#endif
#include "mtk_cam.h"
#include "mtk_cam-hsf-pkvm.h"
#include <pkvm_mgmt/pkvm_mgmt.h>

int mtk_cam_pkvm_init(struct mtk_cam_ctx *ctx)
{
	return CAMSYS_PKVM_RETURN_SUCCESS;
}

int mtk_cam_pkvm_sv_config(struct mtk_cam_ctx *ctx, bool En)
{
	int ret = 0;

	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer\n", __func__);
		return -1;
	}

	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;
#if IS_ENABLED(CONFIG_MTK_PKVM_ISP)
	struct arm_smccc_res res;
	unsigned long hvc_id;
#endif
	hsf_config = ctx->hsf;

	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return -1;
	}
	share_buf = hsf_config->share_buf;
	dev_info(cam->dev, "%s 0x%x/0x%x/0x%x", __func__, (unsigned int)share_buf->cam_tg,
			(unsigned int)share_buf->enable_raw, (unsigned int)En);
#if IS_ENABLED(CONFIG_MTK_PKVM_ISP)
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ISP_SETHSFCAMSV,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	ret = pkvm_el2_mod_call(hvc_id, share_buf->cam_tg, share_buf->enable_raw, En);
#endif
	dev_info(cam->dev, "SMC CALL: SMC_ID_MTK_PKVM_ISP_SETHSFCAMSV\n");

	return ret;
}

int mtk_cam_pkvm_stream_ctrl(struct mtk_cam_ctx *ctx, unsigned int without_tg)
{
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;

	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer\n", __func__);
		return -1;
	}

	struct mtk_cam_device *cam = ctx->cam;
	int ret = 0, raw_id = 0, raw_engine = 0;
	uint64_t chk_pa;
#if IS_ENABLED(CONFIG_MTK_PKVM_ISP)
	struct arm_smccc_res res;
	unsigned long hvc_id;
#endif

	raw_engine = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);
	raw_id = find_first_bit_set(raw_engine);
	pr_info("%s used_engine:%lx raw_engine:%d raw_id:%d without_tg:%d\n", __func__, ctx->used_engine,
		raw_engine, raw_id, without_tg);
	if (raw_id < 0) {
		pr_info("%s error: raw_id is not found\n", __func__);
		return -1;
	}

	hsf_config = ctx->hsf;
	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return -1;
	}
	share_buf = hsf_config->share_buf;
	chk_pa = get_chk_pa();
#if IS_ENABLED(CONFIG_MTK_PKVM_ISP)
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ISP_STREAMON,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	ret = pkvm_el2_mod_call(hvc_id, (u32)chk_pa, (u32)(chk_pa >> 32), raw_id, without_tg);
#endif
	dev_info(cam->dev, "SMC CALL: CMD_STREAMON\n");

	return ret;
}

int mtk_cam_setcam_topKVM(struct mtk_cam_ctx *ctx, bool En)
{
	struct mtk_cam_hsf_ctrl *hsf_config = NULL;
	struct mtk_cam_hsf_info *share_buf = NULL;

	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer\n", __func__);
		return -1;
	}

	struct mtk_cam_device *cam = ctx->cam;
	int ret = 0;
	uint64_t chk_pa, tg_idx;
#if IS_ENABLED(CONFIG_MTK_PKVM_ISP)
	struct arm_smccc_res res;
	unsigned long hvc_id;
#endif

	hsf_config = ctx->hsf;
	if (hsf_config == NULL) {
		pr_info("%s error: hsf_config is NULL pointer check hsf initial\n", __func__);
		return -1;
	}
	share_buf = hsf_config->share_buf;
	chk_pa = get_chk_pa();
	tg_idx = share_buf->cam_tg;

	dev_info(cam->dev, "%s 0x%x/0x%x/0x%x/0x%x", __func__, (unsigned int)chk_pa,
			(unsigned int)share_buf->enable_raw, (unsigned int)tg_idx, (unsigned int)En);
#if IS_ENABLED(CONFIG_MTK_PKVM_ISP)
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_ISP_SETHSFCAM,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	ret = pkvm_el2_mod_call(hvc_id, (u32)chk_pa, (u32)(chk_pa >> 32), share_buf->enable_raw, share_buf->cam_tg, En);
#endif
	dev_info(cam->dev, "SMC CALL: CMD_SETHSFCAM\n");

	return ret;
}

int mtk_cam_pkvm_setcam(struct mtk_cam_ctx *ctx)
{
	int ret = 0;

	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer\n", __func__);
		return -1;
	}

	struct mtk_cam_device *cam = ctx->cam;

	ret = mtk_cam_setcam_topKVM(ctx, 1);
	if(ret != 0) {
		dev_info(cam->dev, "mtk_cam_setcam_topKVM fail  ret = %d\n", ret);
		return -1;
	}

	return 0;
}

int mtk_cam_pkvm_free(struct mtk_cam_ctx *ctx)
{
	if (ctx == NULL) {
		pr_info("%s error: ctx is NULL pointer\n", __func__);
		return -1;
	}

	struct mtk_cam_device *cam = ctx->cam;

	dev_info(cam->dev, "[%s] +\n", __func__);

#ifdef PERFORMANCE_HSF
	int ms_0 = 0, ms_1 = 0, ms = 0;
	struct timeval time;

	do_gettimeofday(&time);
	ms_0 = time.tv_sec + time.tv_usec;
#endif

	mtk_cam_pkvm_sv_config(ctx, 0);
	mtk_cam_setcam_topKVM(ctx, 0);

#ifdef PERFORMANCE_HSF
	do_gettimeofday(&time);
	ms_1 = time.tv_sec + time.tv_usec;
	ms = ms_1 - ms_0;
	dev_info(cam->dev, "%s %d us\n", __func__, ms);
#endif

	return 0;
}
