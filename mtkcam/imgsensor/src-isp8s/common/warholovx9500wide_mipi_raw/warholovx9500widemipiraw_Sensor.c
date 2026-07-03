// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 warholovx9500widemipiraw_Sensor.c
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
#include "warholovx9500widemipiraw_Sensor.h"
#define EEPROM_READY 1
static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate);
static void mi_stream(void *arg,bool enable);
static void mi_read_CGRatio(void *arg);
#if EEPROM_READY
static void set_sensor_cali(void *arg);
#endif
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int warholovx9500wide_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int warholovx9500wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt, u64 sof_ts);
static int get_csi_param(struct subdrv_ctx *ctx,	enum SENSOR_SCENARIO_ID_ENUM scenario_id,struct mtk_csi_param *csi_param);
static int warholovx9500wide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void warholovx9500wide_set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt);
static int warholovx9500wide_set_hdr_tri_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_multi_shutter_frame_length_2expstagger(struct subdrv_ctx *ctx, u64 *shutters, u16 exp_cnt,	u16 frame_length);
static int warholovx9500wide_set_hdr_tri_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_get_period_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_get_cust_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_get_readout_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int warholovx9500wide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void set_mi_pre_init(void *arg);
static int warholovx9500wide_cphy_lrte_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void mi_stream_check_stream_off(struct subdrv_ctx *ctx);
static void warholovx9500wide_write_frame_length(struct subdrv_ctx *ctx, u32 fll);
static u64 sof_start_time = 0;

struct subdrv_ctx *ctx_ovx9500 = NULL;

#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE 1
#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC 2
#define SEAMLESS_SWITCH_GROUP_VIDEO_MODE      3
#define SEAMLESS_SWITCH_GROUP_HD_CAP_MODE     4
#define SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE          5
#define SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO               6
#define SEAMLESS_SWITCH_GROUP_BOKEH                           7
#define SEAMLESS_SWITCH_GROUP_BOKEH_LEICA                     8
#define SEAMLESS_SWITCH_GROUP_VIDEO_MOVIE_MODE                9

/* STRUCT */
static struct mtk_sensor_saturation_info warholovx9500_saturation_info_14bit = {
	.gain_ratio = 5000,
	.OB_pedestal = 1024,
	.saturation_level = 16383,
	.adc_bit = 10,
	.ob_bm = 64,
	.dummy_padding = MSB_PADDING,
};

//static u32 warholovx9500_dcg_ratio_table_14bit[] = {16000};

static struct mtk_sensor_saturation_info warholovx9500_saturation_info_10bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

static struct SET_SENSOR_AWB_GAIN g_last_awb_gain = {0, 0, 0, 0};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, warholovx9500wide_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, warholovx9500wide_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, warholovx9500wide_seamless_switch},
	{SENSOR_FEATURE_SET_ESHUTTER, warholovx9500wide_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, warholovx9500wide_set_shutter_frame_length},
	//{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, warholovx9500wide_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_DUAL_GAIN,warholovx9500wide_set_hdr_tri_gain},
	{SENSOR_FEATURE_SET_HDR_SHUTTER,warholovx9500wide_set_hdr_tri_shutter},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME,warholovx9500wide_set_multi_shutter_frame_length},
	{SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO,warholovx9500wide_get_period_by_scenario},
	{SENSOR_FEATURE_GET_READOUT_BY_SCENARIO,warholovx9500wide_get_readout_by_scenario},
	{SENSOR_FEATURE_GET_CUST_PIXEL_RATE,warholovx9500wide_get_cust_pixel_rate},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, warholovx9500wide_set_multi_dig_gain},
	{SENSOR_FEATURE_SET_AWB_GAIN, warholovx9500wide_set_awb_gain},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, warholovx9500wide_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_CPHY_LRTE_MODE, warholovx9500wide_cphy_lrte_mode},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		// .header_id = 0x07, // vendor id : 0x07 == ofilm
		// .addr_header_id = 0x01, // vendor id addr
		.i2c_write_id = 0xA2,

		// PDC Calibration 1th
		.pdc_support = TRUE,
		.pdc_size = 4,
		.addr_pdc = 0x3BA7,
		.sensor_reg_addr_pdc = 0x5260,

		// PDC Calibration 2th
		.lrc_support = TRUE,
		.lrc_size = 90,
		.addr_lrc = 0x3BAB,
		.sensor_reg_addr_lrc = 0x6600,

		// QPDC Calibration 1th
		.xtalk_support = TRUE,
		.xtalk_size = 32,
		.addr_xtalk = 0x2DAF,
		.sensor_reg_addr_xtalk = 0x6260,

		// QPDC Calibration 2h
		.qsc_support = TRUE,
		.qsc_size = 3536,
		.addr_qsc = 0x2DCF,
		.sensor_reg_addr_qsc = 0x6BC0,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//1: dense pd
		.i4BinFacX = 2,//
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {480, 462}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 384}, {0, 192}, {64, 228}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {2048, 1536}, {0, 0}, {2048, 1536}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{2048, 1792}, {0, 256}, {0, 256}, {2048, 1792}, {256, 912},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{128, 456}, {0, 384}, {0, 384}, {0, 0}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		// <cust36> <cust37> <cust38> <cust39> <cust40>
		{2048, 1920}, {2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_hspd = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 16,
	.i4PitchY	= 16,
	.i4PairNum	= 8,
	.i4SubBlkW	= 4,
	.i4SubBlkH	= 8,
	.iMirrorFlip = 0,	/*0 IMAGE_NORMAL, 1 IMAGE_H_MIRROR, 2 IMAGE_V_MIRROR, 3 IMAGE_HV_MIRROR*/
	.i4PosL = {
		{2, 1}, {6, 1}, {10, 1}, {14, 1},
		{0, 13}, {4, 13}, {8, 13}, {12, 13}
	},
	.i4PosR = {
		{2, 5}, {6, 5}, {10, 5}, {14, 5},
		{0, 9}, {4, 9}, {8, 9}, {12, 9}
	},
	.i4VolumeX = 1,
	.i4VolumeY = 1,
	.i4BlockNumX = 256,
	.i4BlockNumY = 192,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.i4NoTrs = 1,
	.PDAF_Support = 12,
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,//1: dense pd
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_ME_PIX_1,
		.i4PDPattern = 3,//1: dense pd
		.i4PDRepetition = 4,
		.i4PDOrder = {1,0,0,1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {480, 462}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 384}, {0, 192}, {64, 228}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {2048, 1536}, {0, 0}, {2048, 1536}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{2048, 1792}, {0, 256}, {0, 256}, {2048, 1792}, {256, 912},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{128, 456}, {0, 384}, {0, 384}, {0, 0}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		// <cust36> <cust37> <cust38> <cust39> <cust40>
		{2048, 1920}, {2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_smvr = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//1: dense pd
		.i4BinFacX = 2,
		.i4BinFacY = 2,
		.i4PDRepetition = 2,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {384, 408}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 384}, {0, 192}, {64, 228}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {0, 0}, {0, 0}, {2048, 1536}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{0, 0}, {0, 256}, {0, 256}, {2048, 1792}, {256, 912},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{128, 456}, {0, 384}, {0, 384}, {0, 0}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 0}, {0, 0}, {2048, 1536}, {0, 0}, {2048, 1536},
		// <cust36> <cust37> <cust38> <cust39> <cust40>
		{0, 0}, {2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}
	},
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_isz = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1, /*1: Dense; 2: Sparse LR interleaved; 3: Sparse LR non interleaved*/
		.i4BinFacX = 4, /*for Dense*/
		.i4BinFacY = 4,
		.i4PDRepetition = 4,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {480, 462}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 384}, {0, 192}, {64, 228}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {2048, 1536}, {0, 0}, {2048, 1536}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{2048, 1536}, {2816, 2112}, {0, 0}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{0, 0}, {0, 256}, {0, 256}, {2048, 1792}, {256, 912},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{128, 456}, {0, 384}, {0, 384}, {0, 0}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {0, 0}, {0, 0}, {0, 0}, {2048, 1536},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{2048, 1536}, {0, 0}, {2048, 1536}, {2048, 1536}, {2816, 2112},
		// <cust36> <cust37> <cust38> <cust39> <cust40>
		{0, 0}, {2048, 1536}, {2656, 1992}, {0, 0}, {0, 0}
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
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
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};


static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};


static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1280,
			.vsize = 720,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 1280,
			.vsize = 360,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1920,
			.vsize = 1080,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 1920,
			.vsize = 540,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
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
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1536,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
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
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
    {
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus12[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2560,
			.vsize = 1920,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 1280,
			.vsize = 480,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 1280,
			.vsize = 480,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus14[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
			.saturation_info = &warholovx9500_saturation_info_14bit,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 1024,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus15[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus16[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus17[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus18[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
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
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 1024,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus19[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus20[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 7680,
			.vsize = 4320,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 1080,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 1080,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus21[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 2160,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 540,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus23[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 1024,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus36[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus40[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

#define Preview_Mode	\
	.frame_desc = frame_desc_prev,\
	.num_entries = ARRAY_SIZE(frame_desc_prev),\
	.mode_setting_table = warholovx9500wide_preview_setting,\
	.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),\
	.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,\
	.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,\
	.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 119988000,\
	.linelength = 1200,\
	.framelength = 3333,\
	.max_framerate = 300,\
	.mipi_pixel_rate = 2188800000,\
	.readout_length = 0,\
	.read_margin = 24,\
	.exposure_margin = 8,\
	.framelength_step = 1,\
	.coarse_integ_step = 1,\
	.min_exposure_line = 2,\
	.mi_mode_type = 2, \
	.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,\
	.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,\
	.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,\
	.imgsensor_winsize_info = {\
		.full_w = 8192,\
		.full_h = 6144,\
		.x0_offset = 0,\
		.y0_offset = 0,\
		.w0_size = 8192,\
		.h0_size = 6144,\
		.scale_w = 4096,\
		.scale_h = 3072,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 4096,\
		.h1_size = 3072,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 4096,\
		.h2_tg_size = 3072,\
	},\
	.dpc_enabled = FALSE,\
	.pdc_enabled = FALSE,\
	.pdaf_cap = TRUE,\
	.imgsensor_pd_info = &imgsensor_pd_info,\
	.ae_binning_ratio = 4000,\
	.fine_integ_line = 0,\
	.delay_frame = 2


static struct subdrv_mode_struct mode_struct[] = {
	// preview mode 0
	// (mode_1)4096X3072_4C2p_LIN_DPD_y_4096X768_30p1fps_3200M
	// PD size = 4096x768
	// Tline = 10.00us
	// VB = 25.54ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		Preview_Mode,
	},
	// capture mode 1: same as preview mode
	{
		Preview_Mode,
	},
	// normal_video mode 2
	// (mode_9)4096X2304_4C2p_LIN_10bit_DPD_y_4096X576_30fps
	// PD size = 4096x576
	// Tline = 10us
	// VB = 27.57 ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = warholovx9500wide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119988000,
		.linelength = 1200,
		.framelength = 3333,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	//hs_video mode 3 : smvr 480fps
	// mode21 1280X720_4C2pV2AH2D_LIN_DPD_y_1280X360_480fps	
	// PD Size = 1280x360
	// Tline = 10us
	// VB = 0.28ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = warholovx9500wide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_hs_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_hs_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119808000,
		.linelength = 1200,
		.framelength = 208,
		.max_framerate = 4800,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 2,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 1536,
			.y0_offset = 1632,
			.w0_size = 5120,
			.h0_size = 2880,
			.scale_w = 1280,
			.scale_h = 720,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// slim_video mode 4: same as preview mode
	{
		Preview_Mode,
	},
	// custom1 mode 5: 60.1fps video
	//  (mode_14)4096X2304_4C2p_LIN_10bit_DPD_y_4096X576_60p1fps
	// PD Size = 4096x576
	// Tline = 10us
	// VB = 10.87ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = warholovx9500wide_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom1_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom1_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119736000,
		.linelength = 1200,
		.framelength = 1663,
		.max_framerate = 600,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom2 mode 6 : smvr 120fps
	// mode_18 2048X1152_4C2pV2AH2D_LIN_DPD_y_2048X576_120fps	
	// PD Size = 2048*576
	// Tline = 10us
	// VB = 5.45ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = warholovx9500wide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom2_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119952000,
		.linelength = 1200,
		.framelength = 833,
		.max_framerate = 1200,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom3 mode 7: smvr 240fps
	// mode_19 1920X1080_4C2pV2AH2D_LIN_DPD_y_1920X540_240fps
	// PD Size = 1920x540
	// Tline = 10us
	// VB = 1.46ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = warholovx9500wide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom3_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119808000,
		.linelength = 1200,
		.framelength = 416,
		.max_framerate = 2400,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 256,
			.y0_offset = 912,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 1920,
			.scale_h = 1080,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom4 mode 8 : fullsize quad
	// mode_4 8192X6144_fullsize_hwremosaic_off_10bit_noPD_30fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	// Tline = 10 us
	// VB = 2.61 ms
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = warholovx9500wide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom4_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom4_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119988000,
		.linelength = 1200,
		.framelength = 3333,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	// custom5 mode 9 : full size crop quad
	//mode_5 4096X3072_fullsize_crop_hwremosaic_off_10bit_QPD_y_lr_2048X768_ud_2048X768_30p1fps
	// Tline = 10 us
	// VB = 17.84 ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = warholovx9500wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	// custom6 mode 10: lbmf 2； 4:3 ；
	// mode_8 4096X3072_4C2p_NDOL_10bit_DPD_y_4096X768_30p1fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	// Tline = 10 us
	// VB = 17.86  ms
	{
		.frame_desc = frame_desc_cus15,
		.num_entries = ARRAY_SIZE(frame_desc_cus15),
		.mode_setting_table = warholovx9500wide_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom15_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom15_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom15_setting),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 3192,
		.read_margin = 12,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
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
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
	},
	//custom7 mode 11  4 binning
	// mode_6 2048X1536_4C2pV2AH2D_LIN_DPD_y_2048X768_30p1fps
	// PD Size = 2048x1536
	// Tline = 10 us
	// VB = 29.38ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = warholovx9500wide_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom7_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom7_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom7_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119990640,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 301,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1536,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1536,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom8 mode 12 : full size bayer；4:3；10 bit;
	// mode_2 8192X6144_fullsize_hwremosaic_10bit_noPD_30fps
	// Tline = 10us
	// VB = 2.61ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = warholovx9500wide_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom8_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom8_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom8_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119988000,
		.linelength = 1200,
		.framelength = 3333,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom9 mode 13 : full size crop bayer；4:3
	// mode_3 4096X3072_fullsize_crop_hwremosaic_10bit_QPD_y_lr_2048X768_ud_2048X768_30p1fps
	// PD Size = 2048 * 768 *2
	// Tline = 10us
	// VB = 17.86ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = warholovx9500wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom9_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom9_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom10 mode 14: 1x bokeh；10 bit；24fps
	// same as preview
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = warholovx9500wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, 
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	//custom11 mode 15: 2x bokeh；
	//mode_5 4096X3072_fullsize_crop_hwremosaic_off_10bit_QPD_y_lr_2048X768_ud_2048X768_30p1fps
	// Tline = 10 us
	// VB = 17.84 ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = warholovx9500wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_BOKEH,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	// custom12 mode 16: tele + wide bokeh
	// mode_20 2560x1920_fullsize_crop_hwremosaic_10bit_QPD_y_lr_1280X480_ud_1280X480_30fps
	// PD Size = 1280x480
	// Tline = 10us
	// VB = 22.73 ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = warholovx9500wide_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom12_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom12_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom12_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2816,
			.y0_offset = 2112,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 2560,
			.scale_h = 1920,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2560,
			.h1_size = 1920,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2560,
			.h2_tg_size = 1920,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom13 mode 17: binning；4:3 
	// mode_1 4096X3072_4C2p_LIN_DPD_y_4096X768_30p1fps_3200M
	// PD Size = 4096x768
	// Tline = 10us
	// VB = 25.54ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = warholovx9500wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 1661,
		.max_framerate = 600,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, 
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom14 mode 18: DXG；4:3  ；14bit；
	// mode_7 4096X3072_4C2p_DCG_14bit_DPD_y_4096X768_HSPD_1024X768_MSB_HiGain_30p1fps
	// PD Size = 4096x768
	// Tline = 20us
	// VB = 17.86ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = warholovx9500wide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom14_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom14_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119592000,
		.linelength = 2400,
		.framelength = 1661,
		.max_framerate = 300,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = 45568,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		// .bit_align_type = IMGSENSOR_PIXEL_MSB_ALIGN,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_hspd,
		.ae_binning_ratio = 3440,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom15 mode 19: 2exp stagger； 4:3 
	// mode_23 4096X3072_4C2p_stg2_DPD_single_y_4096X768_30p1fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	// Tline = 20 us
	// VB = 2.6  ms
	{
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = warholovx9500wide_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom6_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom6_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom6_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 119952000,
		.linelength = 2400,
		.framelength = 1666,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 768 * 2, // 3072 / 4 * 2
		.read_margin = 24 * 2, // READ_MARGIN: This value is fixed as 24d.
		.framelength_step = 1 * 2,
		.coarse_integ_step = 1 * 2,
		.min_exposure_line = 2* 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 2; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2 * 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom16 mode 20: ISZ DXG 14
	// mode_13 4096X2560_fullsize_crop_hwremosaic_DCG_14bit_DPD_y_4096X640_HSPD_1024X640_30p1fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	// Tline = 20 us
	// VB = 7.72  ms
	{
		.frame_desc = frame_desc_cus16,
		.num_entries = ARRAY_SIZE(frame_desc_cus16),
		.mode_setting_table = warholovx9500wide_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom16_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom16_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom16_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119952000,
		.linelength = 2400,
		.framelength = 1666,
		.max_framerate = 300,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = 45568,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 3.2f,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		// .bit_align_type = IMGSENSOR_PIXEL_MSB_ALIGN,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 880,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom17 mode 21: binning ；16:10
	// mode_10 4096X2560_4C2p_LIN_10bit_DPD_y_4096X640_30p1fps
	// PD Size = 4096x640
	// Tline = 10us
	// VB = 26.82ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus17,
		.num_entries = ARRAY_SIZE(frame_desc_cus17),
		.mode_setting_table = warholovx9500wide_custom17_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom17_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom17_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom17_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom18 mode 22 : DXG；16:10；14 bit
	// mode_11 4096X2560_4C2p_DCG_14bit_DPD_y_4096X640_HSPD_1024X640_MSB_HiGain_30p1fps
	// PD Size = 4096x640
	// Tline = 20 us
	// VB = 20.42 ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus18,
		.num_entries = ARRAY_SIZE(frame_desc_cus18),
		.mode_setting_table = warholovx9500wide_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom18_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom18_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom18_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119592000,
		.linelength = 2400,
		.framelength = 1661,
		.max_framerate = 300,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = 45568,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		// .bit_align_type = IMGSENSOR_PIXEL_MSB_ALIGN,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_hspd,
		.ae_binning_ratio = 3440,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom19 mode 23: full size crop bayer；16:10；30fps
	//mode_12 4096X2560_fullsize_crop_hwremosaic_10bit_DPD_QPD_y_lr_2048X640_ud_2048X640_30fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus19,
		.num_entries = ARRAY_SIZE(frame_desc_cus19),
		.mode_setting_table = warholovx9500wide_custom19_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom19_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom19_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom19_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom20 mode 24 : 8K video
	// mode_16 7680X4320_fullsize_crop_hwremosaic_10bit_QPD_y_lr_3840X1080_ud_3840X1080_30fps	
	// PD Size = 3840x2160
	// Tline = 10us
	// VB = 11.73ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus20,
		.num_entries = ARRAY_SIZE(frame_desc_cus20),
		.mode_setting_table = warholovx9500wide_custom20_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom20_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom20_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom20_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119988000,
		.linelength = 1200,
		.framelength = 3333,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		// .ana_gain_max = BASEGAIN * 15.9375f,
		// .ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 256,
			.y0_offset = 912,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 7680,
			.scale_h = 4320,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 7680,
			.h1_size = 4320,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 7680,
			.h2_tg_size = 4320,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom21 mode 25 :  4K 120fps
	// mode_17 3840X2160_4C2p_LIN_10bit_DPD_y_3840X540_120fps
	// PD Size = 3840x540
	// Tline = 10us
	// VB = 2.93ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus21,
		.num_entries = ARRAY_SIZE(frame_desc_cus21),
		.mode_setting_table = warholovx9500wide_custom21_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom21_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom21_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom21_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119952000,
		.linelength = 1200,
		.framelength = 833,
		.max_framerate = 1200,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 256,
			.y0_offset = 912,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 3840,
			.scale_h = 2160,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3840,
			.h1_size = 2160,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3840,
			.h2_tg_size = 2160,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom22 mode 26: 16:9；10 bit； 60fps
	//same as custim1
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = warholovx9500wide_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom1_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom1_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom1_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119736000,
		.linelength = 1200,
		.framelength = 1663,
		.max_framerate = 600,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom23 mode 27: 16:9；DXG 14 bit； 60fps
	//mode_15 4096X2304_4C2p_DCG_14bit_DPD_y_4096X576_HSPD_1024X576_MSB_HiGain_60p1fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus23,
		.num_entries = ARRAY_SIZE(frame_desc_cus23),
		.mode_setting_table = warholovx9500wide_custom23_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom23_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom23_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom23_setting_seamless),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119664000,
		.linelength = 2400,
		.framelength = 831,
		.max_framerate = 600,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 8,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = 45568,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_hspd,
		.ae_binning_ratio = 3440,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom24 mode 28: same as preview
	// 2x bokeh fallback
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = warholovx9500wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_BOKEH,
		.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, 
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom25 mode 29: same as preview
	// 2x bokeh fallback leica
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = warholovx9500wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_BOKEH_LEICA,
		.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, 
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom26 mode 30: same as preview
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = warholovx9500wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 1661,
		.max_framerate = 600,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, 
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom27 mode 31: same as mode 18
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = warholovx9500wide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom14_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom14_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119592000,
		.linelength = 2400,
		.framelength = 1661,
		.max_framerate = 300,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = 45568,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		// .bit_align_type = IMGSENSOR_PIXEL_MSB_ALIGN,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_hspd,
		.ae_binning_ratio = 3440,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom28 mode 32 : same as mode 19 stagger 2
	{
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = warholovx9500wide_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom6_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom6_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom6_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 119952000,
		.linelength = 2400,
		.framelength = 1666,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 768 * 2, // 3072 / 4 * 2
		.read_margin = 24 * 2, // READ_MARGIN: This value is fixed as 24d.
		.framelength_step = 1 * 2,
		.coarse_integ_step = 1 * 2,
		.min_exposure_line = 2* 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 2; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2 * 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom29 mode 33 :  same as mode 8
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = warholovx9500wide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom4_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom4_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119988000,
		.linelength = 1200,
		.framelength = 3333,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	// custom30 mode 34 : same as mode 9
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = warholovx9500wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	// custom31 mode 35 :  same as mode 13
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = warholovx9500wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom9_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom9_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom32 mode 36: same as  mode 11
	{
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = warholovx9500wide_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom7_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom7_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom7_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119990640,
		.linelength = 1200,
		.framelength = 3322,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1536,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1536,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom33 mode 37 : ISZ LEICA same as preview
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = warholovx9500wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119592000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, 
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom34 mode 38 :  same as mode 15
	// mode_5 4096X3072_fullsize_crop_hwremosaic_off_10bit_QPD_y_lr_2048X768_ud_2048X768_30p1fps
	// Tline = 10 us
	// VB = 17.84 ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = warholovx9500wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_BOKEH_LEICA,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
			.w0_size = 4096,
			.h0_size = 3072,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	// custom35 mode 39 : ISZ bayer fake 14 LEICA same as custom7
	{
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = warholovx9500wide_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom12_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom12_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom12_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 120000000,
		.linelength = 1200,
		.framelength = 4000,
		.max_framerate = 250,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2816,
			.y0_offset = 2112,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 2560,
			.scale_h = 1920,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2560,
			.h1_size = 1920,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2560,
			.h2_tg_size = 1920,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom36 mode 40 
	// mode_22 4096X2304_fullsize_crop_hwremosaic_10bit_QPD_y_lr_2048X576_ud_2048X576_60p1fps
	// PD Size = 2048*576
	// Tline = 10us
	// VB = 5.11ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus36,
		.num_entries = ARRAY_SIZE(frame_desc_cus36),
		.mode_setting_table = warholovx9500wide_custom36_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom36_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom36_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom36_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119736000,
		.linelength = 1200,
		.framelength = 1663,
		.max_framerate = 600,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 0,
		.exposure_margin = 32,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1920,
			.w0_size = 4096,
			.h0_size = 2304,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom37 mode 41: 16:9；10 bit； 30fps
	//same as custim1
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = warholovx9500wide_custom37_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom37_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MOVIE_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom37_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom37_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119736000,
		.linelength = 1200,
		.framelength = 3326,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom38 mode 42: 16:9；DXG 14 bit； 30fps
	//mode_15 4096X2304_4C2p_DCG_14bit_DPD_y_4096X576_HSPD_1024X576_MSB_HiGain_60p1fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus23,
		.num_entries = ARRAY_SIZE(frame_desc_cus23),
		.mode_setting_table = warholovx9500wide_custom38_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom38_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MOVIE_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom38_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom38_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119664000,
		.linelength = 2400,
		.framelength = 1662,
		.max_framerate = 300,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_hspd,
		.ae_binning_ratio = 3440,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom39 mode 43
	// mode_22 4096X2304_fullsize_crop_hwremosaic_10bit_QPD_y_lr_2048X576_ud_2048X576_30p1fps
	// PD Size = 2048*576
	// Tline = 10us
	// VB = 5.11ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_cus36,
		.num_entries = ARRAY_SIZE(frame_desc_cus36),
		.mode_setting_table = warholovx9500wide_custom39_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom39_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MOVIE_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom39_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom39_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119736000,
		.linelength = 1200,
		.framelength = 3326,
		.max_framerate = 300,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 1,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 2.73f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1920,
			.w0_size = 4096,
			.h0_size = 2304,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2
	},
	// custom40 mode 44: ISZ DXG 14
	// mode_25 4096X2304_fullsize_crop_hwremosaic_DCG_14bit_DPD_y_4096X576_HSPD_1024X640_30p1fps
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	// Tline = 20 us
	// VB = 7.72  ms
	{
		.frame_desc = frame_desc_cus40,
		.num_entries = ARRAY_SIZE(frame_desc_cus40),
		.mode_setting_table = warholovx9500wide_custom40_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom40_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MOVIE_MODE,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom40_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom40_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 119592000,
		.linelength = 2400,
		.framelength = 1661,
		.max_framerate = 300,
		.mipi_pixel_rate = 1563428572,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 15.93f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 3.2f,
		.saturation_info = &warholovx9500_saturation_info_14bit,
		// .bit_align_type = IMGSENSOR_PIXEL_MSB_ALIGN,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
		},
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1920,
			.w0_size = 4096,
			.h0_size = 2304,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 880,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
	},
	// custom41 mode 45: 120fps video
	//  (mode_14)4096X2304_4C2p_LIN_10bit_DPD_y_4096X576_120p1fps
	// PD Size = 4096x576
	// Tline = 10us
	// VB = 10.87ms
	// XIAOMI_P12GL_OV50Q_setting_2025_0909_v1.2
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = warholovx9500wide_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = warholovx9500wide_custom1_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(warholovx9500wide_custom1_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 119736000,
		.linelength = 1200,
		.framelength = 831,
		.max_framerate = 1200,
		.mipi_pixel_rate = 2188800000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = WARHOLOVX9500WIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x300A, 0x300B},
	.i2c_addr_table = {0x20, 0xFF}, // TBD
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.mirror = IMAGE_NORMAL,


	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.dig_gain_min = BASEGAIN,
	.dig_gain_max = BASEGAIN * 32,
	.dig_gain_step = 1,  //If the value is 0, SENSOR_FEATURE_SET_MULTI_DIG_GAIN is disabled
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN,
	.ana_gain_max = BASEGAIN * 63.75f,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = warholovx9500wide_ana_gain_table,
	.ana_gain_table_size = sizeof(warholovx9500wide_ana_gain_table),
	.tuning_iso_base = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xFFFFFF - 32,
	.exposure_step = 2,
	.exposure_margin = 32,
	.saturation_info = &warholovx9500_saturation_info_10bit,

	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 2200000,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_mi_stream = mi_stream,
	.s_mi_read_CGRatio = mi_read_CGRatio,
#if EEPROM_READY
	.s_cali = set_sensor_cali,
#else
	.s_cali = NULL,
#endif	

	//.reg_addr_stream = 0x0100,
	//.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
		{0x3500, 0x3501, 0x3502},
		{0x3540, 0x3541, 0x3542},
		{0x3580, 0x3581, 0x3582}
	},
	.long_exposure_support = TRUE,
//	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
		{0x3508, 0x3509},
		{0x3588, 0x3589},
		{0x3548, 0x3549}
	},
	.reg_addr_dig_gain = {
		{0x350A, 0x350B, 0x350C},
		{0x354A, 0x354B, 0x354C},
		{0x358A, 0x358B, 0x358C}
	},
	.reg_addr_dcg_ratio = PARAM_UNDEFINED, //TBD
	.reg_addr_frame_length = {0x3840, 0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,
//	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x387F,
//	.reg_addr_fast_mode = 0x3010,
	 .s_mi_pre_init = set_mi_pre_init,

	// .mi_enable_async = 1, // enable async setting
//	.mi_i2c_type = 1,
	.mi_hb_vb_cal = true,
	.mi_dxg_reg = 0x5064,
	.mi_disable_set_dummy = 1, // disable set dummy
	.mi_evaluate_frame_rate_by_scenario = evaluate_frame_rate_by_scenario,
	.regulator_oc_notice = 3,
	.init_setting_table = warholovx9500wide_init_setting,
	.init_setting_len = ARRAY_SIZE(warholovx9500wide_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 0,
	.chk_s_off_end = 0,

	//TBD
	.checksum_value = 0xAF3E324E,
};

static struct subdrv_ops ops = {
	.get_id = warholovx9500wide_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_AFVDD, {3300000, 3300000},  1000},
	{HW_ID_DVDD,  {1150000, 1150000},  2000},
	{HW_ID_MCLK,   {24},        0},
	{HW_ID_RST,    {0},         0},
	{HW_ID_AVDD,  {2800000, 2800000},  0},
	{HW_ID_AVDD1,  {1800000, 1800000},  0},
	{HW_ID_DOVDD, {1800000, 1800000},  1000},
	{HW_ID_MCLK_DRIVING_CURRENT, {4},  5000},
	{HW_ID_RST,    {1},       5000},
};

const struct subdrv_entry warholovx9500wide_mipi_raw_entry = {
	.name = SENSOR_DRVNAME_WARHOLOVX9500WIDE_MIPI_RAW,
	.id = WARHOLOVX9500WIDE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 framerateRet = framerate;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM3:
		if (framerate == 600) {
			framerateRet = 601;
		}
		break;
	default:
		break;
	}

	DRV_LOG(ctx, "input/output:%d/%d scenario_id:%d", framerate, framerateRet, scenario_id);
	return framerateRet;
}


static void mi_stream_check_stream_off(struct subdrv_ctx *ctx)
{
	u32 i = 0, framecnt = 0;
	u32 timeout = 5;
	DRV_LOG_MUST(ctx, "mi_stream_check_stream_off enter");

	framecnt = subdrv_ixc_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);

	if (ctx->is_seamless == FALSE)
		return;

	for (i = 0; i < timeout; i++) {
		usleep_range(20000,21000);
		DRV_LOG_MUST(ctx, "mi_stream_check_stream_off check");
		ctx->is_seamless = FALSE;
		if(framecnt != subdrv_ixc_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count))
			return;
	}

	DRV_LOG_MUST(ctx, "mi_stream_check_stream_off fail");
}


static void mi_stream(void *arg,bool enable)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	DRV_LOG_MUST(ctx, "Enter %s enable = %d \n", __FUNCTION__,enable);
	if(enable){
		subdrv_i2c_wr_u8(ctx,0x0100,0x01);
	}else{
		mi_stream_check_stream_off(ctx);
		i2c_table_write(ctx,warholovx9500wide_stream_off,ARRAY_SIZE(warholovx9500wide_stream_off));
	}
}

static void mi_read_CGRatio(void *arg){
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int i = 0;
	DRV_LOG_MUST(ctx, "ctx->s_ctx.mi_dxg_ratio(%d)\n", ctx->s_ctx.mi_dxg_ratio);

	if(ctx->s_ctx.mi_dxg_ratio != 0)
		return;

	if(ctx->s_ctx.mi_dxg_reg && !ctx->s_ctx.mi_dxg_ratio){
		subdrv_i2c_wr_u8(ctx,0x3d90,0x90);
		subdrv_i2c_wr_u8(ctx,0x3d8c,0x01);
		subdrv_i2c_wr_u8(ctx,0x3d8d,0x79);
		subdrv_i2c_wr_u8(ctx,0x0100,0x01);
		mdelay(4);
		ctx->s_ctx.mi_dxg_ratio = (subdrv_i2c_rd_u16(ctx,ctx->s_ctx.mi_dxg_reg) * 1000 ) / 1024;
		subdrv_i2c_wr_u8(ctx,0x0100,0x00);
		DRV_LOG(ctx, "mi_dxg_ratio(%d) \n",ctx->s_ctx.mi_dxg_ratio);
	}

	for (i = 0; i < ctx->s_ctx.sensor_mode_num ; i++) {
		if(ctx->s_ctx.mode[i].hdr_mode == HDR_RAW_DCG_COMPOSE){
			ctx->s_ctx.mode[i].dcg_info.dcg_ratio_group[0] = ctx->s_ctx.mi_dxg_ratio;
			DRV_LOG(ctx, "mode(%d) dcg_ratio_group[0](%d) \n",i,ctx->s_ctx.mode[i].dcg_info.dcg_ratio_group[0]);
		}

		DRV_LOG(ctx,"mode(%d) mi_dxg_ratio(%d) ae_binning_ratio = %d\n",
				i,ctx->s_ctx.mi_dxg_ratio,ctx->s_ctx.mode[i].ae_binning_ratio);
	}
}


#if EEPROM_READY
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

	/* PDC data 1th */
	support = info[idx].pdc_support;
	pbuf = info[idx].preload_pdc_table;
	size = info[idx].pdc_size;
	addr = info[idx].sensor_reg_addr_pdc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 1th PDC calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 1th PDC calibration data fail.",
				pbuf, addr, size);
		}
	}

	/* PDC data 2th */
	support = info[idx].lrc_support;
	pbuf = info[idx].preload_lrc_table;
	size = info[idx].lrc_size;
	addr = info[idx].sensor_reg_addr_lrc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 2th PDC calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 2th PDC calibration data fail.",
				pbuf, addr, size);
		}
	}

	/* QPDC data 1th */
	support = info[idx].xtalk_support;
	pbuf = info[idx].preload_xtalk_table;
	size = info[idx].xtalk_size;
	addr = info[idx].sensor_reg_addr_xtalk;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 1th qpdc calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 1th qpdc calibration data fail.",
				pbuf, addr, size);
		}
	}

	/* QPDC data 2th */
	support = info[idx].qsc_support;
	pbuf = info[idx].preload_qsc_table;
	size = info[idx].qsc_size;
	addr = info[idx].sensor_reg_addr_qsc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 2th qpdc calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 2th qpdc calibration data fail.",
				pbuf, addr, size);
		}
	}
}
#endif
static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int temperature = 0;

	/*TEMP_SEN_CTL */
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);
	temperature = (temperature > 0xC0) ? (temperature - 0x100) : temperature;

	if (ctx->sof_cnt < 3 && -1 == temperature)
		temperature = 0;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature);
	return temperature;

}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	return;

	if (en)
		set_i2c_buffer(ctx, 0x3208, 0x00);
	else{
		set_i2c_buffer(ctx, 0x3208, 0x10);
		set_i2c_buffer(ctx, 0x3208, 0xA0);
	}
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int warholovx9500wide_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];

		do {
			*sensor_id = (subdrv_ixc_rd_u8(ctx, addr_h) << 8) |
				subdrv_ixc_rd_u8(ctx, addr_l);
			if (addr_ll) {
				*sensor_id = ((*sensor_id) << 8) | subdrv_ixc_rd_u8(ctx, addr_ll);
			}

			if (ctx->s_ctx.s_mi_read_CGRatio) {

				ctx->s_ctx.s_mi_read_CGRatio((void *)ctx);
			}

			DRV_LOG(ctx, "i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);

			if (*sensor_id == ctx->s_ctx.sensor_id)
				return ERROR_NONE;
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static int covert_seamless_setting_table(enum SENSOR_SCENARIO_ID_ENUM scenario_id, u16* seamless_setting)
{
	u32 i = 0;
	u32 len = 0;
	DRV_LOG(ctx_ovx9500, "seamless setting convert begin\n");
	for (i = 0; i< ctx_ovx9500->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len/2; i++){
		if ( 2*i < ctx_ovx9500->s_ctx.mode[ctx_ovx9500->current_scenario_id].seamless_switch_mode_setting_len &&
		ctx_ovx9500->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table[2*i+1] !=
		ctx_ovx9500->s_ctx.mode[ctx_ovx9500->current_scenario_id].seamless_switch_mode_setting_table[2*i+1]) {
			seamless_setting[2*len] = ctx_ovx9500->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table[2*i];
			seamless_setting[2*len+1] = ctx_ovx9500->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table[2*i + 1];
			len++;
		} else if (2*i >= ctx_ovx9500->s_ctx.mode[ctx_ovx9500->current_scenario_id].seamless_switch_mode_setting_len) {
			seamless_setting[2*len] = ctx_ovx9500->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table[2*i];
			seamless_setting[2*len+1] = ctx_ovx9500->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table[2*i + 1];
			len++;
		}
	}

	DRV_LOG(ctx_ovx9500, "seamless setting convert end len(%d)\n",len);

	return len;
}

static int warholovx9500wide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length;
	u32 frame_length_step;
	u32 frame_length_min;
	u32 frame_length_max;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	if (!framerate) {
		DRV_LOGE(ctx, "framerate (%u) is invalid\n", framerate);
		return ERROR_NONE;
	}
	if (!ctx->s_ctx.mode[scenario_id].linelength) {
		DRV_LOGE(ctx, "linelength (%u) is invalid\n",
			ctx->s_ctx.mode[scenario_id].linelength);
		return ERROR_NONE;
	}

	if (framerate > ctx->s_ctx.mode[scenario_id].max_framerate) {
		DRV_LOGE(ctx, "framerate (%u) is greater than max_framerate (%u)\n",
			framerate, ctx->s_ctx.mode[scenario_id].max_framerate);
		framerate = ctx->s_ctx.mode[scenario_id].max_framerate;
	}

	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	/* set on the step of frame length */
	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;
	frame_length_min = ctx->s_ctx.mode[scenario_id].framelength;
	frame_length_max = ctx->s_ctx.frame_length_max;
	frame_length_max = frame_length_step ?
		(frame_length_max - (frame_length_max % frame_length_step)) : frame_length_max;


	/* set in the range of frame length */
	ctx->frame_length = max(frame_length, frame_length_min);
	ctx->frame_length = min(ctx->frame_length, frame_length_max);
	ctx->frame_length = frame_length_step ?
		roundup(ctx->frame_length,frame_length_step) : ctx->frame_length;

	/* set default frame length if given default framerate */
	if (framerate == ctx->s_ctx.mode[scenario_id].max_framerate)
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	ctx->current_fps = ctx->s_ctx.mode[scenario_id].pclk /
						ctx->frame_length * 10 /
						ctx->s_ctx.mode[scenario_id].linelength;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1, ctx->frame_length:%u\n",
		framerate, ctx->current_fps, scenario_id, ctx->frame_length);

	return ERROR_NONE;
}

void warholovx9500wide_update_mode_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num)
	{
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
				 scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	if (ctx->current_fps < (ctx->s_ctx.mode[scenario_id].max_framerate - 20))
	{
		ctx->frame_length = ctx->pclk / ctx->line_length / (ctx->current_fps / 10);
		ctx->frame_length_rg = ctx->frame_length;
	}
	else
	{
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
		ctx->frame_length_rg = ctx->frame_length;
		ctx->current_fps = ctx->pclk / ctx->line_length / ctx->frame_length * 10;
	}

	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->l_shift = 0;
	ctx->min_vblanking_line = ctx->s_ctx.mode[scenario_id].min_vblanking_line;
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF)
	{
		memset(ctx->frame_length_in_lut, 0,
			   sizeof(ctx->frame_length_in_lut));

		switch (ctx->s_ctx.mode[scenario_id].exp_cnt)
		{
		case 2:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->frame_length -
										  ctx->frame_length_in_lut[0];
			break;
		case 3:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[2] = ctx->frame_length -
										  ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[0];
			break;
		default:
			break;
		}
		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			   sizeof(ctx->frame_length_in_lut_rg));
	}

	/* MCSS low power mode update para */
	if (ctx->s_ctx.mcss_update_subdrv_para != NULL)
		ctx->s_ctx.mcss_update_subdrv_para((void *)ctx, scenario_id);
}

static void set_mi_pre_init(void *arg)
{
  	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (ctx->eeprom_index == 1) // for aac
		return;

// add for af start
	ctx->i2c_client->addr = 0x18;
	ctx->i2c_write_id = 0x18;

	subdrv_i2c_wr_u8_u8(ctx, 0x02, 0x40);// sleep mode

	ctx->i2c_client->addr = 0x20;
	ctx->i2c_write_id = 0x20;
// add for af end

	DRV_LOG_MUST(ctx, "Enter %s\n", __FUNCTION__);

}

static void write_vsync_offset(struct subdrv_ctx *ctx)
{
	u32 offset = 0;
	u32 rot = 0;
	u32 vblanking = 0;
	u32 ratio = 1;
	u32 shutter = ctx->exposure[0];
	u32 framelength = ctx->frame_length;

	if ((ctx->current_scenario_id == 17 || ctx->current_scenario_id == 18) && !ctx->is_seamless && ctx->is_streaming) {
		offset = 13000000LL/(((int64_t)ctx->line_length * 1000000000LL) / ctx->pclk);	//capture 13ms

		if(ctx->s_ctx.mi_hb_vb_cal){
			if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 1)
				ratio = 2;
			if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 2)
				ratio = 4;
			if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 3)
				ratio = 4;
		}
		rot = ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size / ratio;
		vblanking = framelength - shutter - rot;

		if (ctx->current_scenario_id == 17 || ctx->current_scenario_id == 30) {
			if (ctx->extraVB) {
				set_i2c_buffer(ctx, 0x3208, 0x00);
				set_i2c_buffer(ctx, 0x3002, 0x80);
				set_i2c_buffer(ctx, 0x38f4, 0x14);	//offset寄存器每增加1，就增加0.5 Tline
				set_i2c_buffer(ctx, 0x38f5, 0x05);
				set_i2c_buffer(ctx, 0x38f6, 0x00);
				set_i2c_buffer(ctx, 0x3208, 0x10);
				set_i2c_buffer(ctx, 0x3208, 0xA0);
			} else {
				set_i2c_buffer(ctx, 0x3208, 0x00);
				set_i2c_buffer(ctx, 0x3002, 0x00);
				set_i2c_buffer(ctx, 0x38f4, 0x00);	//offset寄存器每增加1，就增加0.5 Tline
				set_i2c_buffer(ctx, 0x38f5, 0x00);
				set_i2c_buffer(ctx, 0x38f6, 0x00);
				set_i2c_buffer(ctx, 0x3208, 0x10);
				set_i2c_buffer(ctx, 0x3208, 0xA0);
			}
		} else if(ctx->current_scenario_id == 18 || ctx->current_scenario_id == 31) {
			if (ctx->extraVB) {
				set_i2c_buffer(ctx, 0x3208, 0x00);
				set_i2c_buffer(ctx, 0x3002, 0x80);
				set_i2c_buffer(ctx, 0x38f4, 0x8a);	//DXG offset寄存器每增加1，就增加1 Tline
				set_i2c_buffer(ctx, 0x38f5, 0x02);
				set_i2c_buffer(ctx, 0x38f6, 0x00);
				set_i2c_buffer(ctx, 0x3208, 0x10);
				set_i2c_buffer(ctx, 0x3208, 0xA0);
			} else {
				set_i2c_buffer(ctx, 0x3208, 0x00);
				set_i2c_buffer(ctx, 0x3002, 0x00);
				set_i2c_buffer(ctx, 0x38f4, 0x00);	//DXG offset寄存器每增加1，就增加1 Tline
				set_i2c_buffer(ctx, 0x38f5, 0x00);
				set_i2c_buffer(ctx, 0x38f6, 0x00);
				set_i2c_buffer(ctx, 0x3208, 0x10);
				set_i2c_buffer(ctx, 0x3208, 0xA0);
			}
		}

		DRV_LOG_MUST(ctx, "Enter %s, shutter:%d, offset:%d, framelength:%d, rot:%d extraVB:%d, vblanking:%d\n", __FUNCTION__, shutter, offset, framelength, rot, ctx->extraVB, vblanking);
	}
}

// static bool warholovx9500wide_check_seamless_time(struct subdrv_ctx *ctx)
// {
// 	u32 ratio = 1;
// 	u32 MIratio = 1;
// 	u64 readout_time = 0;

// 	if(ctx->s_ctx.mi_hb_vb_cal){
// 		if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 1)
// 			ratio = 2;
// 		if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 2)
// 			ratio = 4;
// 		if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 3)
// 			ratio = 4;
// 	}

// 	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER)
// 		MIratio = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
// 	readout_time =
// 		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].linelength
// 		* ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size
// 		* 1000000000 / ctx->s_ctx.mode[ctx->current_scenario_id].pclk / ratio * MIratio;

// 	DRV_LOG_MUST(ctx, "readout_time = %lld, sof_time = %lld\n",
// 			readout_time,ktime_get_boottime_ns() - sof_start_time);

// 	return (ktime_get_boottime_ns() - sof_start_time) < readout_time - 500000;

// }

static int warholovx9500wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	enum IMGSENSOR_HDR_MODE_ENUM src_hdr;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;
	static u16 warholovx9500wide_setting_seamless[1082] = {0};
	u32 setting_seamless_len = 0;
	// u32 retLen = 0;
	bool is_full = false;
	bool is_video = false;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return ERROR_NONE;
	}

	if ((ctx->current_fps > 400) && (ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM13 ||
		ctx->current_scenario_id == SENSOR_SCENARIO_ID_CUSTOM26) ) {
		warholovx9500wide_write_frame_length(ctx, ctx->s_ctx.mode[ctx->current_scenario_id].framelength * 2);
	}

	memset(warholovx9500wide_setting_seamless, 0, sizeof(warholovx9500wide_setting_seamless));

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

	DRV_LOG(ctx, "E: frame_length_rg 0x%x; frame_length 0x%x",ctx->frame_length_rg, ctx->frame_length);

	if (ctx->current_scenario_id == 12 && ctx->frame_length_rg > 0xBEEE)
		is_full = true;

	if (ctx->current_scenario_id == 26 || ctx->current_scenario_id == 27 || ctx->current_scenario_id == 40)
		is_video = true;

	// if ((
	// 	(ctx->current_scenario_id == 26 && scenario_id == 27) ||
	// 	(ctx->current_scenario_id == 27 && scenario_id == 26))
	// 	&& warholovx9500wide_check_seamless_time(ctx) && ae_ctrl->exposure.le_exposure < 800)
	// 	is_video  = true;

	if(!is_video)
	setting_seamless_len = covert_seamless_setting_table(scenario_id, warholovx9500wide_setting_seamless);

	DRV_LOG(ctx, "seamless switch setting convert begin\n");

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

	ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE] = ae_ctrl->gain.le_gain;
	ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME] = ae_ctrl->gain.me_gain;

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;
	src_hdr = ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode;

	warholovx9500wide_update_mode_info(ctx, scenario_id);

	if(!is_video){
		i2c_table_write(ctx,warholovx9500wide_seamless_switch_step1,
			ARRAY_SIZE(warholovx9500wide_seamless_switch_step1));

		i2c_table_write(ctx,warholovx9500wide_setting_seamless, 2*setting_seamless_len);
	}
	// i2c_table_write(ctx,
	// 	ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
	// 	ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	// if (ctx->s_ctx.mode[scenario_id].mi_mode_type == 1)
	// 	warholovx9500wide_set_awb_gain(ctx, (u8 *)&g_last_awb_gain, &retLen);

	if ((ctx->current_fps > 290) && (scenario_id == SENSOR_SCENARIO_ID_CUSTOM13 ||
		scenario_id == SENSOR_SCENARIO_ID_CUSTOM26) ) {
		set_max_framerate_by_scenario(ctx, scenario_id, 300);
	}

	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			//first frame not Calculate constraint conditions
			warholovx9500wide_set_multi_shutter_frame_length_2expstagger(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_LBMF:
			set_multi_shutter_frame_length_in_lut(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_COMPOSE:
			warholovx9500wide_set_shutter_frame_length(ctx, (u8 *)&ae_ctrl->exposure, len);
			warholovx9500wide_set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		default:
			warholovx9500wide_set_shutter(ctx, (u8 *)&ae_ctrl->exposure.le_exposure,len);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}

	if(!is_video){
		if (is_full) {
		i2c_table_write(ctx,warholovx9500wide_seamless_switch_step3,
			ARRAY_SIZE(warholovx9500wide_seamless_switch_step3));
		} else {
		i2c_table_write(ctx,warholovx9500wide_seamless_switch_step2,
			ARRAY_SIZE(warholovx9500wide_seamless_switch_step2));
		}
	} else {
		i2c_table_write(ctx,
			ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
			ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);
	}

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	DRV_LOG(ctx, "X: ctx->current_scenario_id %d\n",ctx->current_scenario_id);
	DRV_LOG_MUST(ctx, "X: set seamless switch done\n");

	return ERROR_NONE;
}

static int warholovx9500wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x5001, 0x01);
		subdrv_i2c_wr_u8(ctx, 0x5101, 0x01);
		subdrv_i2c_wr_u8(ctx, 0x5102, 0x05);
		subdrv_i2c_wr_u8(ctx, 0x5401, 0x01);
		subdrv_i2c_wr_u8(ctx, 0x5402, 0x05);
		subdrv_i2c_wr_u8(ctx, 0x5701, 0x01);
		subdrv_i2c_wr_u8(ctx, 0x5702, 0x05);
		break;
	default:
		if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ) {
			subdrv_i2c_wr_u8(ctx, 0x5001, 0x2c);
		} else {
			subdrv_i2c_wr_u8(ctx, 0x5001, 0x00);
		}
			subdrv_i2c_wr_u8(ctx, 0x5101, 0x00);
			subdrv_i2c_wr_u8(ctx, 0x5401, 0x00);
			subdrv_i2c_wr_u8(ctx, 0x5701, 0x00);
		break;
	}

	ctx->test_pattern = mode;
	return ERROR_NONE;

}

static int warholovx9500wide_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
/*	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	subdrv_i2c_wr_u8(ctx, 0x0602, (R >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0603, (R & 0xff));
	subdrv_i2c_wr_u8(ctx, 0x0604, (Gr >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0605, (Gr & 0xff));
	subdrv_i2c_wr_u8(ctx, 0x0606, (B >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0606, (B & 0xff));
	subdrv_i2c_wr_u8(ctx, 0x0608, (Gb >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0608, (Gb & 0xff));

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);*/
	return ERROR_NONE;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx_ovx9500 = ctx;
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt, u64 sof_ts)
{
	kal_uint16 sensor_output_cnt;

	sof_start_time = ktime_get_boottime_ns();
	sensor_output_cnt = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);
	DRV_LOG_MUST(ctx, "sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->current_scenario_id, sof_cnt, sensor_output_cnt);

	ctx->is_seamless = FALSE;

	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		//set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		//commit_i2c_buffer(ctx);
	}

	return 0;
};

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	switch (scenario_id) {
	default:
		csi_param->cdr_delay_enable = 1;
		csi_param->cdr_delay        = 9;
		csi_param->cphy_lrte_support = 1;
		break;
	}
	DRV_LOG(ctx, "scenario_id:%u, cdr param custom:%d/%d\n", scenario_id, csi_param->cdr_delay_enable, csi_param->cdr_delay);
	return 0;
}

static u32 dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	return dgain * 64;
}

static int warholovx9500wide_set_hdr_tri_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int i = 0;
	int exp_cnt = 2;
	u32 values[3] = {0};
	u64 *gains = (u64 *)para;

	if (gains != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u32) *(gains + i);
	}
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_gain_in_lut(ctx, values, exp_cnt);
		return 0;
	}
	warholovx9500wide_set_multi_gain(ctx,values, exp_cnt);
	return 0;
}

static void set_dgain(struct subdrv_ctx *ctx, u32 *dgain)
{
	int i = 0;

	for (i = 0; i < 2; i++) {
		DRV_LOG(ctx, "dgain[%d]=%d\n", i, dgain[i]);
		dgain[i] = dgain2reg(ctx, dgain[i]);

		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[0],
				(dgain[i] >> 16) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				(dgain[i] >> 8) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[2]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[2],
				dgain[i] & 0xFF);
		}
	}

	commit_i2c_buffer(ctx);

	DRV_LOG(ctx, "dgain reg[lg/mg]: 0x%x 0x%x \n",
		dgain[0], dgain[1] );
	return;
}

static void warholovx9500wide_set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u32 rg_gains[3] = {0};
	u8 has_gains[3] = {0};
	u32 dgain[2] = {1024,1024};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (exp_cnt > ARRAY_SIZE(ctx->ana_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->ana_gain));
		exp_cnt = ARRAY_SIZE(ctx->ana_gain);
	}

	/*check DXG gain*/
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ){

		if((gains[0] *1050 /gains[1] * 1024 ) > (ctx->s_ctx.mi_dxg_ratio *1000)){
			DRV_LOG_MUST(ctx,"dxg ratio error! before LE %d ,ME %d",gains[0],gains[1]);
			gains[0] = (gains[1] *1024/ctx->s_ctx.mi_dxg_ratio)*1050/1000;
			DRV_LOG_MUST(ctx,"dxg ratio error! after LE 0x%x ,ME 0x%x",gains[0],gains[1]);
			ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE]=gains[0];
			ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME]=gains[1];
		}
	}

	for (i = 0; i < exp_cnt; i++) {
		DRV_LOG(ctx, "gains[i]=%d\n",gains[i]);
		if (gains[i] > ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max)
		dgain[i] = dgain[i] * ( (100 * gains[i]) / ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max) / 100;
		/* check boundary of gain */
		gains[i] = max(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		gains[i] = min(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max);

		DRV_LOG(ctx, "mode[%d].multi_exposure_ana_gain_range[%d], max: 0x%x, min:0x%x \n",
						ctx->current_scenario_id,i,ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max,
						ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		/* mapping of gain to register value */
		if (ctx->s_ctx.g_gain2reg != NULL)
			gains[i] = ctx->s_ctx.g_gain2reg(gains[i]);
		else
			gains[i] = gain2reg(gains[i]);
	}

	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->ana_gain[i] = gains[i];
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* write gain */
	memset(has_gains, 1, sizeof(has_gains));
	switch (exp_cnt) {
	case 2:
		rg_gains[0] = gains[0];
		has_gains[1] = 0;
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		has_gains[0] = 0;
		has_gains[1] = 0;
		has_gains[2] = 0;
		break;
	}
	for (i = 0; i < 3; i++) {
		if (has_gains[i]) {
			set_i2c_buffer(ctx,ctx->s_ctx.reg_addr_ana_gain[i].addr[0],(rg_gains[i] >> 8) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[i].addr[1],rg_gains[i] & 0xFF);
		}
	}

	if (ctx->is_seamless == TRUE)
	set_dgain(ctx, dgain);

	DRV_LOG(ctx, "reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n", rg_gains[0], rg_gains[1], rg_gains[2]);

	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	/* group hold end */
}

static void warholovx9500wide_write_frame_length(struct subdrv_ctx *ctx, u32 fll)
{
	u32 addr_h = ctx->s_ctx.reg_addr_frame_length.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_frame_length.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_frame_length.addr[2];
	u32 fll_step = 0;
	u32 dol_cnt = 1;

	u32 ratio = 1;
	if (ctx->extraVB) {
		if(ctx->s_ctx.mi_hb_vb_cal){
			if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 1)
				ratio = 2;
			if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 2)
				ratio = 4;
			if(ctx->s_ctx.mode[ctx->current_scenario_id].mi_mode_type == 3)
				ratio = 4;
		}
		fll = ctx->extraVB * (ctx->pclk / ctx->line_length) / 1000 + ctx->exposure[0] +  ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size / ratio;
		DRV_LOG(ctx, "ctx->exposure[0] = %d, framelength = %d, extraVB(%d)", ctx->exposure[0], fll, ctx->extraVB);
	}

	check_current_scenario_id_bound(ctx);
	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	if (fll_step)
		fll = roundup(fll, fll_step);
	ctx->frame_length = fll;

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER){
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
		if(ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt == 2) dol_cnt = 1; //temp fixed 2exp stagger error
	}

	fll = fll / dol_cnt;

	if (addr_ll) {
		set_i2c_buffer(ctx,	addr_h,	(fll >> 16) & 0xFF);
		set_i2c_buffer(ctx,	addr_l, (fll >> 8) & 0xFF);
		set_i2c_buffer(ctx,	addr_ll, fll & 0xFF);
	} else {
		set_i2c_buffer(ctx,	addr_h, (fll >> 8) & 0xFF);
		set_i2c_buffer(ctx,	addr_l, fll & 0xFF);
	}
	/* update FL RG value after setting buffer for writting RG */
	ctx->frame_length_rg = ctx->frame_length;

	DRV_LOG(ctx,
		"ctx:(fl(RG):%u), fll[0x%x] multiply %u, fll_step:%u\n",
		ctx->frame_length_rg, fll, dol_cnt, fll_step);

}

static int warholovx9500wide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 shutter = *((u64 *)para);
	u32 frame_length = 0;
	u32 fine_integ_line = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 exposure_margin = 0;

	exposure_margin = ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin ?
                  ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin :
                  ctx->s_ctx.exposure_margin;

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	check_current_scenario_id_bound(ctx);
	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].min);
	shutter = min_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].max);
	/* check boundary of framelength */
	ctx->frame_length = max((u32)shutter + exposure_margin, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = (u32) shutter;
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		warholovx9500wide_write_frame_length(ctx, ctx->frame_length);
	/* write shutter */
//	set_long_exposure(ctx);
	if (ctx->s_ctx.reg_addr_exposure[0].addr[2]) {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 16) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[2],
			ctx->exposure[0] & 0xFF);
		//DXG
		DRV_LOG(ctx, "mode id = %d hdr_mode = %d",ctx->current_scenario_id,ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode);
	} else {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			ctx->exposure[0] & 0xFF);
	}
	DRV_LOG(ctx, "exp[0x%x], fll(input/output):%u/%u, flick_en:%d, exposure_margin:%d\n",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en, exposure_margin);
	write_vsync_offset(ctx);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}/* group hold end */
	return 0;
}

static int warholovx9500wide_cphy_lrte_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	u8 cphy_lrte_en = 0;

	scenario_id = *((u64 *)para);
	cphy_lrte_en = 1;

	DRV_LOG_MUST(ctx, "cphy_lrte_en = %d, scen = %u\n",
		cphy_lrte_en, scenario_id);
	return ERROR_NONE;
}

static int warholovx9500wide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	warholovx9500wide_set_shutter_frame_length(ctx, para, len);
	return 0;
}

static int warholovx9500wide_set_multi_shutter_frame_length_2expstagger(struct subdrv_ctx *ctx,
		u64 *shutters, u16 exp_cnt,	u16 frame_length)
{
	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[3] = {0};
	u32 calc_fll = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;
	int dol_cnt = 1;

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;

	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = roundup(shutters[i], cit_step);
	}

	//2EXP STAGGER
	//ctx->exposure is last exp;shutters and ctx->frame_length is now exp;
	//3072 = ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER &&
		ctx->s_ctx.mode[ctx->current_scenario_id].raw_cnt == 2 &&
		ctx->exposure[0] > 0 && ctx->exposure[1] > 0 ){
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
		//frame n. Frame n+2 image data is for Exp_L(n+2), Exp_M(n+2), Exp_S(n+2)
		//starts in frame n+1, Exp_M(n+2) and Exp_S(n+2) start in frame n+2.
		calc_fl[0] = shutters[0]/2 + ctx->exposure[1]/2 + 72 ;
		DRV_LOG(ctx, "calc_fl[0] = %u",calc_fl[0]);

		//Below condition makes sure M/S frame in frame n+1 doesn't conflict to M/S frame in frame n+2
		if(ctx->frame_length + shutters[1]/2 < ctx->exposure[1]/2 + (3072/4) ){
			if(((int)(ctx->exposure[1]/2 + (3072/4) - shutters[1]/2)) > 0)
				calc_fl[1] = ctx->exposure[1]/2  + (3072/4)  - shutters[1]/2;
			else
				calc_fl[1] = 0;
		}
		// //adjust stagger HDR to non-overlap
		// if(shutters[1] + (3072/4 + 78) * dol_cnt >= ctx->frame_length){
		// 	calc_fl[2] = shutters[1] + (3072/4 + 78 + cit_step) * dol_cnt;
		// 	calc_fl[2] = calc_fl[2] + ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
		// }
		for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
			ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
		DRV_LOG(ctx, "2exp dol_cnt[%d] calc_fl[%u/%u/%u] shutters[%llu/%llu/%llu], exposure[%u/%u/%u]\n",
			dol_cnt, calc_fl[0], calc_fl[1], calc_fl[2], shutters[0], shutters[1], shutters[2],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2]);
	}

	//3EXP STAGGER
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER &&
		ctx->s_ctx.mode[ctx->current_scenario_id].raw_cnt == 3 &&
		ctx->exposure[0] > 0 && ctx->exposure[1] > 0 && ctx->exposure[2] > 0){
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
		//frame n. Frame n+2 image data is for Exp_L(n+2), Exp_M(n+2), Exp_S(n+2). Exp_L(n+2)
		//starts in frame n+1, Exp_M(n+2) and Exp_S(n+2) start in frame n+2.
		if(shutters[0] + ctx->exposure[1] + ctx->exposure[2] >= ctx->frame_length - 100 * dol_cnt){
			calc_fl[0] = shutters[0] + ctx->exposure[1] + ctx->exposure[2] + (100 + cit_step) * dol_cnt ;
			DRV_LOG(ctx, "calc_fl[0] = %u",calc_fl[0]);
		}
		//Below condition makes sure M/S frame in frame n+1 doesn't conflict to M/S frame in frame n+2
		if(ctx->frame_length + shutters[1] + shutters[2] < ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt ){
			if(((int)(ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt  - (shutters[1] + shutters[2]))) > 0)
				calc_fl[1] = ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt  - (shutters[1] + shutters[2]);
			else
				calc_fl[1] = 0;
		}
		//adjust stagger HDR to non-overlap
		if(shutters[1] + shutters[2] + (3072/4 + 110) * dol_cnt >= ctx->frame_length){
			calc_fl[2] = shutters[1] + shutters[2] + (3072/4 + 110 + cit_step) * dol_cnt;
			calc_fl[2] = calc_fl[2] + ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
		}
		for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
			ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
		DRV_LOG(ctx, "dol_cnt[%d] calc_fl[%u/%u/%u] shutters[%llu/%llu/%llu], exposure[%u/%u/%u]\n",
			dol_cnt, calc_fl[0], calc_fl[1], calc_fl[2], shutters[0], shutters[1], shutters[2],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2]);
	}
	//frame need > shutter + magin
	for (i = 0; i < exp_cnt; i++)
		calc_fll += (u32) shutters[i];
	calc_fll += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
	ctx->frame_length =	max(ctx->frame_length, calc_fll);
	ctx->frame_length =	max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length =	min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	for (i = 0; i < exp_cnt; i++)
		ctx->exposure[i] = (u32) shutters[i];
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	/* write framelength */
	if (set_auto_flicker(ctx, 0) || ctx->frame_length){
		warholovx9500wide_write_frame_length(ctx, ctx->frame_length);
	}
	/* write shutter */
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = (u32) shutters[0] ;
		break;
	case 2:
		rg_shutters[0] = (u32) shutters[0] ;
		rg_shutters[1] = (u32) shutters[1] ;
		break;
	case 3:
		rg_shutters[0] = (u32) shutters[0] ;
		rg_shutters[1] = (u32) shutters[1] ;
		rg_shutters[2] = (u32) shutters[2] ;
		break;
	default:
		break;
	}
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
		ctx->l_shift = 0;
	}
	for (i = 0; i < 3; i++) {
		if (rg_shutters[i]) {
			if (ctx->s_ctx.reg_addr_exposure[i].addr[2]) {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[2],
					rg_shutters[i] & 0xFF);
				//DXG
				DRV_LOG(ctx, "mode id = %d hdr_mode = %d",ctx->current_scenario_id,ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode);
				if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ){
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[0],
						(rg_shutters[i] >> 16) & 0xFF);
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[1],
						(rg_shutters[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[2],
						rg_shutters[i] & 0xFF);
				}
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG(ctx, "exp[0x%x/0x%x/0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		rg_shutters[0], rg_shutters[1], rg_shutters[2],
		frame_length, ctx->frame_length, ctx->autoflicker_en);
	write_vsync_offset(ctx);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
	return 0;
}

static int warholovx9500wide_set_hdr_tri_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int i = 0;
	u64 values[3] = {0};
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u64 *shutters = (u64 *)para;
	int exp_cnt = 2;

	if (shutters != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u64) *(shutters + i);
	}
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_shutter_frame_length_in_lut(ctx,
			values, exp_cnt, 0, frame_length_in_lut);
		return 0;
	}
	DRV_LOG(ctx, "warholovx9500wide_set_hdr_tri_shutter");

	warholovx9500wide_set_multi_shutter_frame_length_2expstagger(ctx, values, exp_cnt, 0);
	return 0;
}

static int warholovx9500wide_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 *shutters = (u64 *)(* feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));
	u16 frame_length = (u16) (*(feature_data + 2));

	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[4] = {0};
	u32 calc_fll = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;
	int dol_cnt = 1;

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;

	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = roundup(shutters[i], cit_step);
	}

	//2EXP STAGGER
	//ctx->exposure is last exp;shutters and ctx->frame_length is now exp;
	//3072 = ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER &&
		ctx->s_ctx.mode[ctx->current_scenario_id].raw_cnt == 2 &&
		ctx->exposure[0] > 0 && ctx->exposure[1] > 0 ){
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
		//frame n. Frame n+2 image data is for Exp_L(n+2), Exp_M(n+2), Exp_S(n+2)
		//starts in frame n+1, Exp_M(n+2) and Exp_S(n+2) start in frame n+2.
		calc_fl[0] = max(shutters[0]/2 + ctx->exposure[1]/2 + 96,shutters[0]/2 + shutters[1]/2 + 96);
		DRV_LOG(ctx, "calc_fl[0] = %u",calc_fl[0]);

		//Below condition makes sure M/S frame in frame n+1 doesn't conflict to M/S frame in frame n+2
		if(ctx->frame_length + shutters[1]/2 < ctx->exposure[1]/2 + (3072/4) ){
			if(((int)(ctx->exposure[1]/2 + (3072/4)  - shutters[1]/2)) > 0)
				calc_fl[1] = ctx->exposure[1]/2  + (3072/4)  - shutters[1]/2;
			else
				calc_fl[1] = 0;
		}
		DRV_LOG(ctx, "calc_fl[1] = %u",calc_fl[1]);
		// //adjust stagger HDR to non-overlap
		calc_fl[2] = max(ctx->exposure[1]/2 + 3072/4 + 96,shutters[1]/2 + 3072/4 + 96);
		DRV_LOG(ctx, "calc_fl[2] = %u",calc_fl[2]);

		calc_fl[3] = ctx->s_ctx.mode[ctx->current_scenario_id].framelength
		+ ctx->s_ctx.mode[ctx->current_scenario_id].read_margin*4;
		DRV_LOG(ctx, "calc_fl[3] = %u",calc_fl[3]);

		for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
			ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
		DRV_LOG(ctx, "2exp dol_cnt[%d] calc_fl[%u/%u/%u] shutters[%llu/%llu/%llu], exposure[%u/%u/%u]\n",
			dol_cnt, calc_fl[0], calc_fl[1], calc_fl[2], shutters[0], shutters[1], shutters[2],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2]);
	}

	//3EXP STAGGER
	//ctx->exposure is last exp;shutters and ctx->frame_length is now exp;
	//3072 = ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER &&
		ctx->s_ctx.mode[ctx->current_scenario_id].raw_cnt == 3 &&
		ctx->exposure[0] > 0 && ctx->exposure[1] > 0 && ctx->exposure[2] > 0){
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
		//frame n. Frame n+2 image data is for Exp_L(n+2), Exp_M(n+2), Exp_S(n+2). Exp_L(n+2)
		//starts in frame n+1, Exp_M(n+2) and Exp_S(n+2) start in frame n+2.
		if(shutters[0] + ctx->exposure[1] + ctx->exposure[2] >= ctx->frame_length - 100 * dol_cnt){
			calc_fl[0] = shutters[0] + ctx->exposure[1] + ctx->exposure[2] + (100 + cit_step) * dol_cnt ;
			DRV_LOG(ctx, "calc_fl[0] = %u",calc_fl[0]);
		}
		//Below condition makes sure M/S frame in frame n+1 doesn't conflict to M/S frame in frame n+2
		if(ctx->frame_length + shutters[1] + shutters[2] < ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt ){
			if(((int)(ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt  - (shutters[1] + shutters[2]))) > 0)
				calc_fl[1] = ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt  - (shutters[1] + shutters[2]);
			else
				calc_fl[1] = 0;
		}
		//adjust stagger HDR to non-overlap
		if(shutters[1] + shutters[2] + (3072/4 + 110) * dol_cnt >= ctx->frame_length){
			calc_fl[2] = shutters[1] + shutters[2] + (3072/4 + 110 + cit_step) * dol_cnt;
			calc_fl[2] = calc_fl[2] + ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
		}
		for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
			ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
		DRV_LOG(ctx, "dol_cnt[%d] calc_fl[%u/%u/%u] shutters[%llu/%llu/%llu], exposure[%u/%u/%u]\n",
			dol_cnt, calc_fl[0], calc_fl[1], calc_fl[2], shutters[0], shutters[1], shutters[2],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2]);
	}
	//frame need > shutter + magin
	for (i = 0; i < exp_cnt; i++)
		calc_fll += (u32) shutters[i];
	calc_fll += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
	ctx->frame_length =	max(ctx->frame_length, calc_fll);
	ctx->frame_length =	max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length =	min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	for (i = 0; i < exp_cnt; i++)
		ctx->exposure[i] = (u32) shutters[i];
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || ctx->frame_length)
		warholovx9500wide_write_frame_length(ctx, ctx->frame_length);

	/* write shutter */
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		break;
	case 2:
		if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER){
			exp_cnt = 1;
		}
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[1] = (u32) shutters[1] / exp_cnt;
		break;
	case 3:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[1] = (u32) shutters[1] / exp_cnt;
		rg_shutters[2] = (u32) shutters[2] / exp_cnt;
		break;
	default:
		break;
	}
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
		ctx->l_shift = 0;
	}
	for (i = 0; i < 3; i++) {
		if (rg_shutters[i]) {
			if (ctx->s_ctx.reg_addr_exposure[i].addr[2]) {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[2],
					rg_shutters[i] & 0xFF);
				//DXG
				DRV_LOG(ctx, "mode id = %d hdr_mode = %d",ctx->current_scenario_id,ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode);
				if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ){
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[0],
						(rg_shutters[i] >> 16) & 0xFF);
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[1],
						(rg_shutters[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[2],
						rg_shutters[i] & 0xFF);
				}
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG(ctx, "exp[0x%x/0x%x/0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		rg_shutters[0], rg_shutters[1], rg_shutters[2],
		frame_length, ctx->frame_length, ctx->autoflicker_en);
	write_vsync_offset(ctx);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
	return 0;
}

 static int warholovx9500wide_get_period_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len){
	u32 ratio = 1;
	u32 de_ratio= 1;
	u64 *feature_data = (u64 *) para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*(feature_data);
	u32 *period = (u32 *)(uintptr_t)(*(feature_data + 1));
	u64 flag =*(feature_data + 2);

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 1 && flag & SENSOR_GET_LINELENGTH_FOR_READOUT)
		de_ratio = 2;
	else if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 2 && flag & SENSOR_GET_LINELENGTH_FOR_READOUT)
		de_ratio = 4;
	else if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 3 && flag & SENSOR_GET_LINELENGTH_FOR_READOUT)
		de_ratio = 8;

	DRV_LOG(ctx, "flag:%llu, scenario_id:%u, mode_num:%u de_ratio = %u\n",
		flag, scenario_id, ctx->s_ctx.sensor_mode_num,de_ratio);

	if (flag & SENSOR_GET_LINELENGTH_FOR_READOUT &&
		ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;
	*period = (ctx->s_ctx.mode[scenario_id].framelength << 16)
			+ (ctx->s_ctx.mode[scenario_id].linelength * ratio / de_ratio);

	return 0;
}

static int warholovx9500wide_get_cust_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	DRV_LOG_MUST(ctx, "warholovx9500wide_get_cust_pixel_rate sensor mode = %llu",*feature_data);
	*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =  3415000000;

	return 0;
}

 static int warholovx9500wide_get_readout_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = *feature_data;
	u64 *readout_time = feature_data + 1;
	u32 ratio = 1;
	u32 MIratio = 1;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return 0;
	}

	if(ctx->s_ctx.mi_hb_vb_cal){
		if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 1)
			ratio = 2;
		if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 2)
			ratio = 4;
		if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 3)
			ratio = 4;
	}

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		MIratio = ctx->s_ctx.mode[scenario_id].exp_cnt;
	*readout_time =
		(u64)ctx->s_ctx.mode[scenario_id].linelength
		* ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info.h2_tg_size
		* 1000000000 / ctx->s_ctx.mode[scenario_id].pclk / ratio * MIratio;

	DRV_LOG_MUST(ctx, "sid:%u, mode_num:%u, readout_time = %lld, MIratio =%d \n",
			scenario_id, ratio, *readout_time,MIratio);
	return 0;
}

static int warholovx9500wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 *gains  = (u32 *)(*feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));

	int i = 0;
	u32 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = dgain2reg(ctx, gains[i]);
	}

	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain */
	switch (exp_cnt) {
	case 1:
		rg_gains[0] = gains[0];
		break;
	case 2:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		break;
	}
	for (i = 0; i < exp_cnt; i++) {
		if (!rg_gains[i])
			continue; // skip zero gain setting

		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[0],
				(rg_gains[i] >> 16) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				(rg_gains[i] >> 8) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[2]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[2],
				rg_gains[i] & 0xFF);
		}
	}

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}

	DRV_LOG(ctx, "dgain reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n",
		rg_gains[0], rg_gains[1], rg_gains[2]);
	return 0;
}

static int warholovx9500wide_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)feature_data;
	MUINT32 r_Gain = awb_gain->ABS_GAIN_R << 1;
	MUINT32 g_Gain = awb_gain->ABS_GAIN_GR << 1;
	MUINT32 b_Gain = awb_gain->ABS_GAIN_B << 1;
	u8 data = 0;
	u16 i = 0;

	g_last_awb_gain = *awb_gain;

	return 0;

	if (r_Gain == 0 || g_Gain == 0 || b_Gain == 0) {
		DRV_LOG(ctx, "error awb gain [r/g/b]: 0x%x 0x%x 0x%x\n",
			r_Gain, g_Gain, b_Gain);
		return 0;
	}

	// en manual awb gain
	data = subdrv_i2c_rd_u8(ctx, 0x5001);
	if(ctx->is_seamless){
		for (i = 0; i< ctx->s_ctx.mode[ctx->current_scenario_id].mode_setting_len/2; i++){
			if (ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_mode_setting_table[2*i] == 0x5001)
			data = ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_mode_setting_table[2*i+1];
		}
	}
	data &= ~0x08; // bit[3] = 0
	subdrv_i2c_wr_u8(ctx, 0x5001, data); // Simple AWB enable

	data = subdrv_i2c_rd_u8(ctx, 0x58b5);
	data |= 0x01; // bit[0] = 1
	subdrv_i2c_wr_u8(ctx, 0x58b5, data); // man_qpdwb_gain_enable

	data = subdrv_i2c_rd_u8(ctx, 0x5ab0);
	data |= 0x02; // bit[1] = 1
	subdrv_i2c_wr_u8(ctx, 0x5ab0, data);

	// set awb gain
	subdrv_i2c_wr_u8(ctx, 0x58ac, (b_Gain >> 8) & 0x7F); //B Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58ad, (b_Gain >> 0) & 0xFF); //B Gain [7:0]
	subdrv_i2c_wr_u8(ctx, 0x58ae, (g_Gain >> 8) & 0x7F); //Gb Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58af, (g_Gain >> 0) & 0xFF); //Gb Gain [7:0]
	subdrv_i2c_wr_u8(ctx, 0x58b0, (g_Gain >> 8) & 0x7F); //Gr Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58b1, (g_Gain >> 0) & 0xFF); //Gr Gain [7:0]
	subdrv_i2c_wr_u8(ctx, 0x58b2, (r_Gain >> 8) & 0x7F); //R Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58b3, (r_Gain >> 0) & 0xFF); //R Gain [7:0]

	DRV_LOG(ctx, "awb gain [r/g/b]: 0x%x 0x%x 0x%x\n",
		r_Gain, g_Gain, b_Gain);

	return 0;
}

