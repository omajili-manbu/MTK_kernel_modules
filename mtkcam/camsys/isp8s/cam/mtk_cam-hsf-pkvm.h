#ifndef __MTK_CAM_HSF_PKVM_H
#define __MTK_CAM_HSF_PKVM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/limits.h>
#include <kree/system.h>
#include <kree/mem.h>
#include "tz_cross/trustzone.h"

enum CAMSYS_PKVM_RETURN {
	CAMSYS_PKVM_RETURN_ERROR,
	CAMSYS_PKVM_RETURN_SUCCESS
};

#define CMD_INIT 1
#define CMD_UNINIT 2
#define CMD_SETHSFCAM 3
#define CMD_SETHSFCAMSV 4
#define CMD_STREAMON 5

extern uint64_t get_chk_pa(void);

int mtk_cam_pkvm_init(struct mtk_cam_ctx *ctx);
int mtk_cam_pkvm_sv_config(struct mtk_cam_ctx *ctx, bool En);
int mtk_cam_pkvm_stream_ctrl(struct mtk_cam_ctx *ctx, unsigned int without_tg);
int mtk_cam_pkvm_setcam(struct mtk_cam_ctx *ctx);
int mtk_cam_pkvm_free(struct mtk_cam_ctx *ctx);

#endif
