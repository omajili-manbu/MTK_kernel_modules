/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_HSF_DEF_H
#define __MTK_CAM_HSF_DEF_H

//#include "tz_m4u.h"
#include <linux/time64.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/scatterlist.h>

#define MAX_INIT_REQUEST_NUM                3
/* #define QOF_CCU_READY */
#include "mtk_cam-hsf-pkvm.h"
#include "kree/system.h"
#include "kree/mem.h"
#include "tz_cross/trustzone.h"
//#define PERFORMANCE_HSF
#define USING_HSF_SENSOR
#define MTK_CAM_HSF_SUPPORT
#define MSG_TO_CCU_HSF_APPLY_CQ 0
#define MSG_TO_CCU_WRITE_READ_COUNT 1
#define MSG_TO_CCU_STREAM_ON 2
#define MSG_TO_CCU_HSF_CONFIG 3
#define MSG_TO_CCU_AID 4

#define MSG_TO_CCU_QOF_CONFIG 5

#define MSG_TO_CCU_START_FIFO_DETECT 6
#define MSG_TO_CCU_FIFO_DUMP 7
#define MSG_TO_CCU_STOP_FIFO_DETECT 8
#define MSG_TO_CCU_CAMSV_HSF_CONFIG 9

struct mtk_cam_hsf_info {
	u32 cq_size;
	u64 cq_dst_iova;
	u32 cam_tg;
	u32 enable_raw;
	u32 cam_module;
	u64 chunk_iova;
	u64 chunk_hsfhandle;
};

struct cq_info {
	uint32_t tg;
	uint64_t dst_addr;
	uint64_t src_addr;
	uint64_t chunk_iova;
	uint32_t cq_size;
	uint32_t init_value;
	uint32_t cq_offset;
	uint32_t sub_cq_size;
	uint32_t sub_cq_offset;
	int32_t  ipc_status;
};

struct raw_info {
	uint32_t tg_idx;
	uint64_t chunk_iova;
	uint64_t cq_iova;
	uint32_t Hsf_en;
	uint32_t enable_raw;
	uint32_t hsf_status;
	uint32_t without_tg;
};

struct sv_info{
	uint32_t tg_idx;
	uint32_t hsf_en;
	uint32_t enable_sv;
};

struct mtk_cam_dma_map {
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	dma_addr_t dma_addr;
	uint64_t hsf_handle;
};

struct mtk_cam_hsf_ctrl {
	struct platform_device *ccu_pdev;
	phandle ccu_handle;
	int power_on_cnt;
	struct mtk_cam_hsf_info *share_buf;
	struct mtk_cam_dma_map *cq_buf;
	struct mtk_cam_dma_map *chk_buf;
};

struct aid_info {
	uint32_t enable;
	uint32_t feature;
	uint32_t used_engine;
};

struct fifo_info {
	uint32_t camsv_idx;
	uint64_t AP_time;
};

enum mtk_cam_aid_feature {
	AID_START = 0,
	AID_VAINR,
	AID_CAM_DC,
	AID_END,
};

struct qof_config {
	uint32_t raw_id;
	bool on_lock;
	bool off_lock;
	bool out_lock;
};

#endif /*__MTK_CAM_HSF_DEF_H*/
