/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx09amipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include "imx09amipiraw_Sensor.h"

#define PFX "imx09a_camera_sensor"
#define LOG_INF(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#define GET_SENSOR_ID_RETRY_CNT    5

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int imx09a_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx09a_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx09a_get_min_shutter_by_scenario_adapter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int open(struct subdrv_ctx *ctx);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);
static int imx09a_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx09a_get_linetime_in_ns(void *arg,
	u32 scenario_id, u32 *linetime_in_ns, enum GET_LINETIME_ENUM linetime_type,
	enum IMGSENSOR_EXPOSURE exp_idx);

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, imx09a_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, imx09a_seamless_switch},
	{SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO, imx09a_get_min_shutter_by_scenario_adapter},
	{SENSOR_FEATURE_SET_AWB_GAIN, imx09a_set_awb_gain},
};

static u32 imx09a_dcg_ratio_table_ratio4[] = {4000};
static u32 imx09a_dcg_ratio_table_ratio16[] = {16000};
static struct mtk_sensor_saturation_info imgsensor_saturation_info_10bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_12bit = {
	.gain_ratio = 4000,
	.OB_pedestal = 64,
	.saturation_level = 3900,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_14bit = {
	.OB_pedestal = 64,
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x01AF0144,/* cal_layout_table */
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA0,

		.qsc_support = TRUE,
		.qsc_size = 0x0C00,
		.addr_qsc = 0x4300,
		.sensor_reg_addr_qsc = 0xC000, /*QSC_GAIN_TABLE_R_0*/

		.pdc_support = TRUE,
		.pdc_size = 0x180,
		.addr_pdc = 0x5000,
		.sensor_reg_addr_pdc = 0xD200, /* SPC_GAIN_TABLE_0_0 */
	},
};

#define pd_i4Crop { \
		/* <pre> <cap> <normal_video> <hs_video> <slim_video> */ \
		{0, 0}, {0, 0}, {0, 256}, {0, 384}, {0, 384}, \
		/* <cust1> <cust2> <cust3> <cust4> <cust5> */ \
		{0, 192}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536}, \
		/* <cust6> <cust7> <cust8> <cust9> <cust10>*/ \
		{0, 192}, {2048, 1536}, {1472, 1104}, {0, 256}, {0, 384}, \
		/* <cust11> <cust12> <cust13> <cust14> <cust15> */ \
		{992, 744}, {0, 0}, {0, 0}, {0, 192}, {0, 384}, \
		/* <cust16> <cust17> <cust18> <cust19> <cust20>*/ \
		{0, 384}, {0, 0}, {2048, 1536}, {0, 0}, {2048, 1792}, \
}/*(i4FullRawW - output_size_W) / 2, (i4FullRawH - output_size_H) / 2*/

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = IMAGE_NORMAL,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,/* all-pd */
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, /* R=1, L=0 */
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_v2h2 = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = IMAGE_NORMAL,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,/* all-pd */
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, /* R=1, L=0 */
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_full = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = IMAGE_NORMAL,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
	.i4ModeIndex = 0x2,
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,/* all-pd */
		.i4BinFacX = 4,
		.i4BinFacY = 2,
		.i4PDRepetition = 0,
		.i4PDOrder = {1}, /* R=1, L=0 */
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0A00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0280,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 288,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_SE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x2000,
			.vsize = 0x1800,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1152,
			.vsize = 864,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1152,
			.vsize = 216,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0A00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0A00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	/*HCG *all-pd*/
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus11[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2112,
			.vsize = 1584,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus12[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x2000,
			.vsize = 0x1800,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus13[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus14[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2112,
			.vsize = 1584,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus15[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus16[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus17[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0C00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0C00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus18[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus19[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus20[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

#define REG2GAIN_ROUNDUP(_reg) ((16384 * BASEGAIN + (16384 - (_reg) - 1))/ (16384 - (_reg)))
#define REG2GAIN_ROUNDDOWN(_reg) (16384 * BASEGAIN / (16384 - (_reg)))

static struct subdrv_mode_struct mode_struct[] = {
	{/* reg B1-S1 4096x3072 @30FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = imx09a_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{/* B3-S1-TEST 4096x3072 @60FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = imx09a_capture_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448,
		.max_framerate = 600,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{/*reg_B1-S2 4096x2560 @30FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = imx09a_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx09a_seamless_normal_video,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_normal_video),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 2530971428,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_B6-TEST 4096x2304 @30FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_hs,
		.num_entries = ARRAY_SIZE(frame_desc_hs),
		.mode_setting_table = imx09a_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 2530971428,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_G4 4096x2304 @60FPS QBIN(VBIN) DCG-HDR 1:4 VB_MAX*/
		.frame_desc = frame_desc_slim,
		.num_entries = ARRAY_SIZE(frame_desc_slim),
		.mode_setting_table = imx09a_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 12304,
		.framelength = 3960,
		.max_framerate = 600,
		.mipi_pixel_rate = 1586290000,
		.readout_length = 0,
		.read_margin = 4 * 2,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1778,
		.csi_param = {
			.cphy_settle = 57,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
		.saturation_info = &imgsensor_saturation_info_12bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_HCG_BASE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx09a_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx09a_dcg_ratio_table_ratio4),
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_V1 2408x1152 @240FPS QBIN(VBIN)-V2H2 VB_MAX*/
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = imx09a_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 6848,
		.framelength = 1752,
		.max_framerate = 2403,
		.mipi_pixel_rate = 1592914285,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 2048,
			.scale_h = 1152,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1152,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1152,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_v2h2,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1474,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{/* Reg_B-2 QBIN(VBIN)_4096x3072 24FPS */
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = imx09a_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom2),
		.hdr_mode = HDR_RAW_DCG_RAW_VS,
		.raw_cnt = 3,
		.exp_cnt = 3,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 3316 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 3244,
		.read_margin = 64,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1503,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 3,
		.saturation_info = &imgsensor_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx09a_dcg_ratio_table_ratio16,
			.dcg_gain_table_size = sizeof(imx09a_dcg_ratio_table_ratio16),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = (0xFFFC-0x64),
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = (0xFFFC-0x64),
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].max = (0xFFFC-0x64),
		.multiexp_s_info[IMGSENSOR_EXPOSURE_SE].belong_to_lut_id = IMGSENSOR_LUT_B,
		.mode_lut_s_info[IMGSENSOR_LUT_B].linelength = 7520,
		.mode_lut_s_info[IMGSENSOR_LUT_B].framelength = 6480,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	{/*reg_F1-S1 8192x6144 @30FPS Full(BAYER) All-PD VB_MAX*/
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = imx09a_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 11632,
		.framelength = 8332,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 8192,
			.scale_h = 6144,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8192,
			.h1_size = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8192,
			.h2_tg_size = 6144,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
		.awb_enabled = true,
	},
	{	/*reg_L1-S1 4096x3072 @30FPS QBIN(VBIN) LBMF_Manual VB_MAX, LB-MF LUTA*/
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = imx09a_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom4_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom4,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom4),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 3254,/* min_fll:3258, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 57,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.dpc_enabled = true,
		.pdc_enabled = true,
	},
	{/*reg_F2-S1 4096x3072 @30FPS Full-RAW-Crop w/ All-PD VB_MAX*/
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = imx09a_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom5_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom5,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom5),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 10256,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.csi_param = {
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_R,
		.awb_enabled = true,
	},
	{	/*reg_V2 2048x1152 @480FPS QBIN-V2H2 w/o PD VB_MAX*/
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = imx09a_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom6_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2928000000,
		.linelength = 4352,
		.framelength = 1388,
		.max_framerate = 4803,
		.mipi_pixel_rate = 2046171428,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 2048,
			.scale_h = 1152,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1152,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1152,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_v2h2,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_L2-S1 4096x3072 @30FPS Full(Quad Bayer)-Crop All-PD LBMF_manual VB_MAX, LB-MF LUTA*/
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = imx09a_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom7_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom7,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom7),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 3288, /*(4607-1536+1)/1+220-4=3288*/
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.csi_param = {
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_R,
		.awb_enabled = true,
	},
	{/*reg_B4 1152x864 @30FPS QBIN(VBIN)-Crop VB_MAX*/
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = imx09a_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 1006628571,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 2208,
			.w0_size = 8192,
			.h0_size = 1728,
			.scale_w = 4096,
			.scale_h = 864,
			.x1_offset = 1472,
			.y1_offset = 0,
			.w1_size = 1152,
			.h1_size = 864,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1152,
			.h2_tg_size = 864,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_G1-S2 4096x2560 @30FPS QBIN(VBIN) DCG(RAW) DirectMode VB_MAX, HSG*/
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = imx09a_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom9_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom9,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom9),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 6664,
		.max_framerate = 300,
		.mipi_pixel_rate = 2530971428,
		.readout_length = 0,
		.read_margin = 4 * 2,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1503,
		.csi_param = {
			.cphy_settle = 57,
		},
		.saturation_info = &imgsensor_saturation_info_10bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx09a_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx09a_dcg_ratio_table_ratio4),
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{/*reg_G3 4096x2304 @60FPS QBIN(VBIN) DCG(RAW) DirectMode VB_MAX, HSG*/
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = imx09a_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom10_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2196000000,
		.linelength = 14560,
		.framelength = 3332,
		.max_framerate = 600,
		.mipi_pixel_rate = 2511090000,
		.readout_length = 0,
		.read_margin = 4 * 2,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1503,
		.csi_param = {
			.cphy_settle = 57,
		},
		.saturation_info = &imgsensor_saturation_info_10bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx09a_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx09a_dcg_ratio_table_ratio4),
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_B3 2112x1584 @30FPS QBIN(VBIN)-Crop RST 4ms VB_MAX*/
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = imx09a_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 1483885714,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1472,
			.w0_size = 8192,
			.h0_size = 3200,
			.scale_w = 4096,
			.scale_h = 1600,
			.x1_offset = 992,
			.y1_offset = 8,
			.w1_size = 2112,
			.h1_size = 1584,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2112,
			.h2_tg_size = 1584,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{/*reg_F1-S1 8192x6144 @30FPS Full(Q BAYER) All-PD VB_MAX*/
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = imx09a_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom12_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom12,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom12),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 11632,
		.framelength = 8332,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 8192,
			.scale_h = 6144,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8192,
			.h1_size = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8192,
			.h2_tg_size = 6144,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_R,
		.awb_enabled = true,
	},
	{	/*reg_B1-S1 4096x3072 @30FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = imx09a_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom13_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {
			.cphy_settle = 57,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_B5 2112x1584 @30FPS QBIN(VBIN)-Crop Rst 12ms VB_MAX*/
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = imx09a_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom14_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 992000000,
		.linelength = 7520,
		.framelength = 4368,
		.max_framerate = 300,
		.mipi_pixel_rate = 486857142,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1472,
			.w0_size = 8192,
			.h0_size = 3200,
			.scale_w = 4096,
			.scale_h = 1600,
			.x1_offset = 992,
			.y1_offset = 8,
			.w1_size = 2112,
			.h1_size = 1584,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2112,
			.h2_tg_size = 1584,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{/*reg_B2-S1 4096x2304 @60FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_cus15,
		.num_entries = ARRAY_SIZE(frame_desc_cus15),
		.mode_setting_table = imx09a_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom15_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448,
		.max_framerate = 600,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_B2 4096x2304 @120FPS QBIN(VBIN) VB_MAX*/
		.frame_desc = frame_desc_cus16,
		.num_entries = ARRAY_SIZE(frame_desc_cus16),
		.mode_setting_table = imx09a_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom16_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 3224,
		.max_framerate = 1200,
		.mipi_pixel_rate = 2530971428,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.csi_param = {
			.cphy_settle = 59,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*reg_G1-S1 4096x3072 @30FPS QBIN(VBIN) DCG(RAW) DirectMode VB_MAX, HSG*/
		.frame_desc = frame_desc_cus17,
		.num_entries = ARRAY_SIZE(frame_desc_cus17),
		.mode_setting_table = imx09a_custom17_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom17_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom17,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom17),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 6664,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 0,
		.read_margin = 4 * 2,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),/*3.11dB(4928)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1503,
		.csi_param = {
			.cphy_settle = 57,
		},
		.saturation_info = &imgsensor_saturation_info_10bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx09a_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx09a_dcg_ratio_table_ratio4),
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
	{	/*not use, reg_L2-S1 4096x3072 @30FPS Full(Quad Bayer)-Crop All-PD LBMF_manual VB_MAX, LB-MF LUTA*/
		.frame_desc = frame_desc_cus18,
		.num_entries = ARRAY_SIZE(frame_desc_cus18),
		.mode_setting_table = imx09a_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom18_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 3288, /*(4607-1536+1)/1+220-4=3288*/
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 8192,
			.scale_h = 3072,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.csi_param = {
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_R,
		.awb_enabled = true,
	},
	{/*not use, reg_L1-S1 4096x3072 @30FPS QBIN(VBIN) LBMF_Manual VB_MAX, LB-MF LUTA*/
		.frame_desc = frame_desc_cus19,
		.num_entries = ARRAY_SIZE(frame_desc_cus19),
		.mode_setting_table = imx09a_custom19_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom19_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3021257143,
		.readout_length = 3254,/* min_fll:3258, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),/*3.10dB(4916)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16320),/*48.16dB(16320)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1382,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 57,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 3,
	},
	{/*reg_G2-S2 4096x2560 @30FPS Full-RMSC-Crop DCG(RAW) DirectMode VB_MAX, HSG*/
		.frame_desc = frame_desc_cus20,
		.num_entries = ARRAY_SIZE(frame_desc_cus20),
		.mode_setting_table = imx09a_custom20_setting,
		.mode_setting_len = ARRAY_SIZE(imx09a_custom20_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx09a_seamless_custom20,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx09a_seamless_custom20),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 2530971428,
		.readout_length = 0,
		.read_margin = 4 * 2,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),/*36.12dB(16128)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),/*0dB(0)*/
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(15360),/*24.08dB(15360)*/
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1792,
			.w0_size = 8192,
			.h0_size = 2560,
			.scale_w = 8192,
			.scale_h = 2560,
			.x1_offset = 2048,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.imgsensor_pd_info = &imgsensor_pd_info_full,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 748,
		.csi_param = {
			.cphy_settle = 57,
		},
		.saturation_info = &imgsensor_saturation_info_10bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx09a_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx09a_dcg_ratio_table_ratio4),
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.delay_frame = 2,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = IMX09A_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x34, 0x35, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.mirror = IMAGE_NORMAL,

	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 256, /*24dB --> 10^(24/10) = 251.189*/
	.ana_gain_type = 0,
	.ana_gain_step = 4,
	.ana_gain_table = imx09a_ana_gain_table,
	.ana_gain_table_size = sizeof(imx09a_ana_gain_table),
	.dig_gain_min = BASE_DGAIN * 1, /* no need (only use ana gain)*/
	.dig_gain_max = BASEGAIN * 16, /* no need (only use ana gain)*/
	.dig_gain_step = 4, /* no need (only use ana gain)*/
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = (0xFFFC - 64) << 8,
	.cit_lshift_max = 8,
	.exposure_step = 4,
	.exposure_margin = 64,

	.frame_length_max = 0xfffc,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 5693360,
	.line_interleave_num = 2,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG|HDR_SUPPORT_LBMF|HDR_SUPPORT_DCG_VS,
	.seamless_switch_support = TRUE,
	.seamless_switch_type = SEAMLESS_SWITCH_CUT_VB_INIT_SHUT,
	.seamless_switch_hw_re_init_time_ns = 0,
	.seamless_switch_prsh_hw_fixed_value = 32,
	.seamless_switch_prsh_length_lc = 0,
	.reg_addr_prsh_length_lines = {0x3058, 0x3059, 0x305A, 0x305B},
	.reg_addr_prsh_mode = 0x3056,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0202, 0x0203},
			{0x3162, 0x3163},
			{0x0224, 0x0225},
	},
	.reg_addr_exposure_in_lut = {
			{0x0E20, 0x0E21},
			{0x0E60, 0x0E61},
			{0x0EA0, 0x0EA1},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
			{0x3164, 0x3165},
			{0x0216, 0x0217},
	},
	.reg_addr_ana_gain_in_lut = {
			{0x0E22, 0x0E23}, /* DCG+VS HCG AGAIN */
			{0x0E62, 0x0E63}, /* DCG+VS VS AGAIN */
			{0x0E38, 0x0E39}, /* DCG+VS LCG AGAIN */
	},
	.reg_addr_dig_gain = {
			{0x020E, 0x020F},
			{0x3166, 0x3167},
			{0x0218, 0x0219},
	},
	.reg_addr_dig_gain_in_lut = {
			{0x0E24, 0x0E25},
			{0x0E64, 0x0E65},
			{0x0EA4, 0x0EA5},
	},
	.reg_addr_dcg_ratio = 0x3182,
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_frame_length_in_lut = {
			{0x0E28, 0x0E29},
			{0x0E68, 0x0E69},
			{0x0EA8, 0x0EA9},
	},
	.reg_addr_temp_en = 0x0138, /* TEMP_SEN_CTL */
	.reg_addr_temp_read = 0x013A, /* TEMP_SEN_OUT */
	.reg_addr_auto_extend = 0x0350, /* FRM_LENGTH_CTL */
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3010,
	.reg_addr_fast_mode_in_lbmf = 0x31B7,
	.reg_addr_gph_delay = 0x3018,
	.seamless_switch_with_gph_delay = 0,

	.init_setting_table = imx09a_init_setting,
	.init_setting_len = ARRAY_SIZE(imx09a_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	.checksum_value = 0xf10e5980,
	.cust_get_linetime_in_ns = imx09a_get_linetime_in_ns,
	.cycle_base_ratio = 16,
};

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 1000},
	{HW_ID_AVDD, {2700000, 2900000}, 0},
	{HW_ID_AVDD1, {1800000, 1800000}, 0},
	{HW_ID_AFVDD, {3300000, 3300000}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 0},
	{HW_ID_DVDD1, {720000, 900000}, 1000},
	{HW_ID_RST, {1}, 4000}
};

const struct subdrv_entry imx09a_mipi_raw_entry = {
	.name = "imx09a_mipi_raw",
	.id = IMX09A_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = GET_SENSOR_ID_RETRY_CNT;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_i2c_rd_u8(ctx, addr_h) << 8) |
				subdrv_i2c_rd_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_i2c_rd_u8(ctx, addr_ll);
			DRV_LOG(ctx, "i2c_write_id(0x%x) sensor_id(0x%x/0x%x)\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == 0xa24a) {
				*sensor_id = ctx->s_ctx.sensor_id;
				return ERROR_NONE;
			}
			DRV_LOGE(ctx, "Read sensor id fail. i2c_write_id: 0x%x\n", ctx->i2c_write_id);
			DRV_LOG(ctx, "sensor_id = 0x%x, ctx->s_ctx.sensor_id = 0x%x\n",
				*sensor_id, ctx->s_ctx.sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = GET_SENSOR_ID_RETRY_CNT;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static u16 imx09a_feedback_awbgain[] = {
	0x0B8E, 0x01,
	0x0B8F, 0x00,
	0x0B90, 0x02,
	0x0B91, 0x28,
	0x0B92, 0x01,
	0x0B93, 0x77,
	0x0B94, 0x01,
	0x0B95, 0x00,
};

/*write AWB gain to sensor*/
static void feedback_awbgain(struct subdrv_ctx *ctx, kal_uint32 r_gain, kal_uint32 b_gain)
{
	UINT32 r_gain_int = 0;
	UINT32 b_gain_int = 0;

	DRV_LOG(ctx, "feedback_awbgain r_gain: %d, b_gain: %d\n", r_gain, b_gain);
	r_gain_int = r_gain / 512;
	b_gain_int = b_gain / 512;
	imx09a_feedback_awbgain[5] = r_gain_int;
	imx09a_feedback_awbgain[7] = (r_gain - r_gain_int * 512) / 2;
	imx09a_feedback_awbgain[9] = b_gain_int;
	imx09a_feedback_awbgain[11] = (b_gain - b_gain_int * 512) / 2;
	subdrv_i2c_wr_regs_u8(ctx, imx09a_feedback_awbgain,
		ARRAY_SIZE(imx09a_feedback_awbgain));
}

static int imx09a_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;

	feedback_awbgain(ctx, awb_gain->ABS_GAIN_R, awb_gain->ABS_GAIN_B);

	return 0;
}

static int open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	sensor_init(ctx);

	/*QSC setting*/
	if (ctx->s_ctx.s_cali != NULL)
		ctx->s_ctx.s_cali((void *)ctx);
	else
		write_sensor_Cali(ctx);

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->current_fps = 10 * ctx->pclk / ctx->line_length / ctx->frame_length;
	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;

	return ERROR_NONE;
}

static void set_sensor_cali(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	/* QSC data */
	support = info[idx].qsc_support;
	pbuf = info[idx].preload_qsc_table;
	size = info[idx].qsc_size;
	addr = info[idx].sensor_reg_addr_qsc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x01);
			DRV_LOG(ctx, "set QSC calibration data done.");
		} else {
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x00);
		}
	}
}

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u8 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);

	if (temperature < 0x55)
		temperature_convert = temperature;
	else if (temperature < 0x80)
		temperature_convert = 85;
	else if (temperature < 0xED)
		temperature_convert = -20;
	else
		temperature_convert = (char)temperature;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en)
		set_i2c_buffer(ctx, 0x0104, 0x01);
	else
		set_i2c_buffer(ctx, 0x0104, 0x00);
}

static u16 get_gain2reg(u32 gain)
{
	return (16384 - (16384 * BASEGAIN) / gain);
}

void imx09a_get_min_shutter_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *min_shutter, u64 *exposure_step)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u set default\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = 0;
	}
	DRV_LOG(ctx, "sensor_mode_num[%d]", ctx->s_ctx.sensor_mode_num);
	if (scenario_id < ctx->s_ctx.sensor_mode_num) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
		case HDR_NONE:
		case HDR_RAW_LBMF:
		case HDR_RAW_DCG_RAW:
			if (ctx->s_ctx.mode[scenario_id].coarse_integ_step &&
				ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min) {
				*exposure_step = ctx->s_ctx.mode[scenario_id].coarse_integ_step;
				*min_shutter =
				ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min;
			} else {
				*exposure_step = ctx->s_ctx.exposure_step;
				*min_shutter = ctx->s_ctx.exposure_min;
			}
			break;
		default:
			*exposure_step = ctx->s_ctx.exposure_step;
			*min_shutter = ctx->s_ctx.exposure_min;
			break;
		}
	} else {
		DRV_LOG(ctx, "over sensor_mode_num[%d], use default", ctx->s_ctx.sensor_mode_num);
		*exposure_step = ctx->s_ctx.exposure_step;
		*min_shutter = ctx->s_ctx.exposure_min;
	}
	DRV_LOG(ctx, "scenario_id[%d] exposure_step[%llu] min_shutter[%llu]\n",
		scenario_id, *exposure_step, *min_shutter);
}

int imx09a_get_min_shutter_by_scenario_adapter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;

	imx09a_get_min_shutter_by_scenario(ctx,
		(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
		feature_data + 1, feature_data + 2);

	return 0;
}

static int imx09a_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return ERROR_NONE;
	}
	scenario_id = *feature_data;
	if ((feature_data + 1) != NULL)
		ae_ctrl = (struct mtk_hdr_ae *)((uintptr_t)(*(feature_data + 1)));
	else
		DRV_LOGE(ctx, "no ae_ctrl input");

	check_current_scenario_id_bound(ctx);
	DRV_LOG(ctx, "E: set seamless switch %u %u\n", ctx->current_scenario_id, scenario_id);
	if (!ctx->extend_frame_length_en)
		DRV_LOGE(ctx, "please extend_frame_length before seamless_switch!\n");
	ctx->extend_frame_length_en = FALSE;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_group == 0 ||
		ctx->s_ctx.mode[scenario_id].seamless_switch_group !=
			ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_group) {
		DRV_LOGE(ctx, "seamless_switch not supported\n");
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table == NULL) {
		DRV_LOGE(ctx, "Please implement seamless_switch setting\n");
		return ERROR_NONE;
	}

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;

	set_i2c_buffer(ctx, 0x0104, 0x01);
	set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x02);
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
	   (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF &&
		ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS)) {
		ctx->s_ctx.seamless_switch_with_gph_delay = 1;
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_gph_delay, 0x01);
	}

	update_mode_info_seamless_switch(ctx, scenario_id);
	set_table_to_buffer(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	ctx->ae_ctrl_gph_en = 1;
	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_LBMF:
			set_multi_shutter_frame_length_in_lut(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_RAW:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		case HDR_RAW_DCG_RAW_VS:
		case HDR_RAW_DCG_COMPOSE_VS:
			set_dcg_vs_multi_shutter_frame_length_in_lut(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_dcg_vs_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}

	set_i2c_buffer(ctx, 0x0104, 0x00);
	ctx->ae_ctrl_gph_en = 0;
	commit_i2c_buffer(ctx);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int imx09a_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	if (mode) {
	/* 1:Solid Color 2:Color Bar 5:Black */
		switch (mode) {
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x0601, 0x01);
			break;
		default:
			subdrv_i2c_wr_u8(ctx, 0x0601, mode);
			break;
		}
	} else if (ctx->test_pattern) {
		subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /*No pattern*/
	}
	ctx->test_pattern = mode;

	return 0;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	ctx->sof_cnt = sof_cnt;
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		ctx->s_ctx.seamless_switch_with_gph_delay = 0;
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_gph_delay, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}

static int imx09a_get_linetime_in_ns(void *arg,
	u32 scenario_id, u32 *linetime_in_ns, enum GET_LINETIME_ENUM linetime_type,
	enum IMGSENSOR_EXPOSURE exp_idx)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 ret = 0;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*linetime_in_ns = 4200;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM10:
		*linetime_in_ns = 5000;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_CUSTOM16:
	case SENSOR_SCENARIO_ID_CUSTOM17:
	case SENSOR_SCENARIO_ID_CUSTOM18:
	case SENSOR_SCENARIO_ID_CUSTOM19:
	case SENSOR_SCENARIO_ID_CUSTOM20:
	default:
		ret = common_get_cycle_base_v1_linetime_in_ns(ctx, scenario_id,
			linetime_in_ns, linetime_type, exp_idx);
		break;
	}

	DRV_LOG(ctx, "linetime(%d)ns\n", *linetime_in_ns);
	return ret;
}
