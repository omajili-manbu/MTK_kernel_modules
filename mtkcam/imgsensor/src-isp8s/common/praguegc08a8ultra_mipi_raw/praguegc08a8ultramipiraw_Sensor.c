// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 praguegc08a8ultramipiraw_Sensor.c
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
#include "praguegc08a8ultramipiraw_Sensor.h"

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static u16 get_gain2reg(u32 gain);
static int ultra_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt, u64 sof_ts);
static int praguegc08a8ultra_close(struct subdrv_ctx *ctx);
static int praguegc08a8ultra_control(struct subdrv_ctx *ctx, enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data);
static int praguegc08a8ultra_streamon(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_streamoff(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int praguegc08a8ultra_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_auto_flicker_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_get_csi_param(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id, struct mtk_csi_param *csi_param);
static int praguegc08a8ultra_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int praguegc08a8ultra_set_long_exposure(struct subdrv_ctx *ctx, u64 shutter);

static bool long_exp_status = false;

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND, praguegc08a8ultra_streamoff},
	{SENSOR_FEATURE_SET_STREAMING_RESUME, praguegc08a8ultra_streamon},
	{SENSOR_FEATURE_SET_GAIN, praguegc08a8ultra_set_gain},
	{SENSOR_FEATURE_SET_FRAMELENGTH, praguegc08a8ultra_set_frame_length},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, praguegc08a8ultra_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_ESHUTTER, praguegc08a8ultra_set_shutter},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, praguegc08a8ultra_set_multi_shutter_frame_length},
	{SENSOR_FEATURE_SET_AUTO_FLICKER_MODE, praguegc08a8ultra_set_auto_flicker_mode},
	{SENSOR_FEATURE_SET_TEST_PATTERN, praguegc08a8ultra_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, praguegc08a8ultra_set_max_framerate_by_scenario},
};

// static struct eeprom_info_struct eeprom_info[] = {
// 	{
// 		// .header_id = 0x010B00FF,
// 		// .header_id = 0x0,
// 		// .addr_header_id = 0x0000000B,
// 		.i2c_write_id = 0xA4,

// 		.pdc_support = TRUE,
// 		.pdc_size = 720,
// 		.addr_pdc = 0x12D2,
// 		.sensor_reg_addr_pdc = 0x5F80,

// 	},
// };

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 1836,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2040,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

/* bokeh 1x */
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2880,
			.vsize = 2160,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1472,
			.vsize = 1104,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

// static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
// 	{
// 		.bus.csi2 = {
// 			.channel = 0,
// 			.data_type = 0x2b,
// 			.hsize = 1536,
// 			.vsize = 1152,
// 			.user_data_desc = VC_STAGGER_NE,
// 			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
// 		},
// 	},
// };

#define Preview_Mode	\
	.frame_desc = frame_desc_prev,\
	.num_entries = ARRAY_SIZE(frame_desc_prev),\
	.mode_setting_table = praguegc08a8ultra_preview_setting,\
	.mode_setting_len = ARRAY_SIZE(praguegc08a8ultra_preview_setting),\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 280800000,\
	.linelength = 3672,\
	.framelength = 2548,\
	.max_framerate = 300,\
	.mipi_pixel_rate = 273000000,\
	.framelength_step = 1,\
	.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,\
	.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,\
	.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,\
	.imgsensor_winsize_info = {\
		.full_w = 3264,\
		.full_h = 2448,\
		.x0_offset = 0,\
		.y0_offset = 0,\
		.w0_size = 3264,\
		.h0_size = 2448,\
		.scale_w = 3264,\
		.scale_h = 2448,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 3264,\
		.h1_size = 2448,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 3264,\
		.h2_tg_size = 2448,\
	},\
	.pdaf_cap = FALSE,\
	.imgsensor_pd_info = PARAM_UNDEFINED,\
	.ae_binning_ratio = 1000,\
	.fine_integ_line = 0,\
	.delay_frame = 2

static struct subdrv_mode_struct mode_struct[] = {
	/* mode 0 : preview 3264x2448@30fps */
	//setting V20251022
	{
		Preview_Mode,
	},
	/* mode 1 : capture same as preview 3264x2448@30fps */
	{
		Preview_Mode,
	},
	/* mode 2 : video 3264x1836@30fps */
	//setting V20251022
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = praguegc08a8ultra_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(praguegc08a8ultra_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.max_framerate = 300,
		.mipi_pixel_rate = 273000000,
		.framelength_step = 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 3264,
			.full_h = 2448,
			.x0_offset = 0,
			.y0_offset = 306,
			.w0_size = 3264,
			.h0_size = 1836,
			.scale_w = 3264,
			.scale_h = 1836,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 1836,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 1836,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	/* mode 3 : same as preview 3264x2448@30fps */
	{
		Preview_Mode,
	},
	/* mode 4 : same as preview 3264x2448@30fps */
	{
		Preview_Mode,
	},
	/* custom1 mode 5: video 3264x2040@30fps */
	//setting V20251022
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = praguegc08a8ultra_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(praguegc08a8ultra_custom1_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.max_framerate = 300,
		.mipi_pixel_rate = 273000000,
		.framelength_step = 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 3264,
			.full_h = 2448,
			.x0_offset = 0,
			.y0_offset = 204,
			.w0_size = 3264,
			.h0_size = 2040,
			.scale_w = 3264,
			.scale_h = 2040,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3264,
			.h1_size = 2040,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3264,
			.h2_tg_size = 2040,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	/* custom2 mode 6: fullsize crop 2880x2160@30fps */
	/* bokeh 1x */
	//setting V20251113
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = praguegc08a8ultra_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(praguegc08a8ultra_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.max_framerate = 300,
		.mipi_pixel_rate = 273000000,
		.framelength_step = 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 3264,
			.full_h = 2448,
			.x0_offset = 192,
			.y0_offset = 144,
			.w0_size = 2880,
			.h0_size = 2160,
			.scale_w = 2880,
			.scale_h = 2160,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2880,
			.h1_size = 2160,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2880,
			.h2_tg_size = 2160,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	/* custom3 mode 7: fullsize crop 1472x1104@30fps */
	/* bokeh 2x */
	//setting V20251113
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = praguegc08a8ultra_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(praguegc08a8ultra_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 280800000,
		.linelength = 3672,
		.framelength = 2548,
		.max_framerate = 300,
		.mipi_pixel_rate = 273000000,
		.framelength_step = 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 3264,
			.full_h = 2448,
			.x0_offset = 896,
			.y0_offset = 672,
			.w0_size = 1472,
			.h0_size = 1104,
			.scale_w = 1472,
			.scale_h = 1104,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1472,
			.h1_size = 1104,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1472,
			.h2_tg_size = 1104,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = PRAGUEGC08A8ULTRA_SENSOR_ID,
	.reg_addr_sensor_id = {0x03F0, 0x03F1},
	.i2c_addr_table = {0x62, 0xFF}, // TBD
	.i2c_burst_write_support = FALSE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	// .eeprom_info = eeprom_info,
	// .eeprom_num = ARRAY_SIZE(eeprom_info),
	// .resolution = {3264, 2448},
	.mirror = IMAGE_NORMAL, // TBD

	// .mclk = 24,
	// .isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = praguegc08a8ultra_ana_gain_table,
	.ana_gain_table_size = sizeof(praguegc08a8ultra_ana_gain_table),
	.tuning_iso_base = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xFFFF - 16,
	.exposure_step = 1,
	.exposure_margin = 16,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 1486000,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = PARAM_UNDEFINED,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
		{0x0202, 0x0203},
	},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {
		{0x0204, 0x0205},
	},
	// .reg_addr_dig_gain = {
	// 	{0x020e, 0x020f},
	// },
	.mi_i2c_type = 1,//i2c Written individually
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	// .reg_addr_frame_count = {0x0146, 0x0147},

	.init_setting_table = praguegc08a8ultra_init_setting,
	.init_setting_len = ARRAY_SIZE(praguegc08a8ultra_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 0,
	.chk_s_off_end = 0,

	//TBD
	.checksum_value = 0x3a98f032,
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = praguegc08a8ultra_control,
	.feature_control = common_feature_control,
	.close = praguegc08a8ultra_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = praguegc08a8ultra_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = ultra_vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_RST, {0}, 1000},
	{HW_ID_DVDD, {1200000, 1200000}, 0},
	{HW_ID_AVDD, {2800000, 2800000}, 1000},
	{HW_ID_RST, {1}, 1000},
	{HW_ID_MCLK, {26}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 2000},
};

const struct subdrv_entry praguegc08a8ultra_mipi_raw_entry = {
	.name = "praguegc08a8ultra_mipi_raw",
	.id = PRAGUEGC08A8ULTRA_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static int praguegc08a8ultra_control(struct subdrv_ctx *ctx,
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	int i = 0, ret = 0;

	DRV_LOG(ctx, "scenario_id = %d\n", scenario_id);

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = ctx->s_ctx.mode[scenario_id].framelength;

	ctx->current_scenario_id = scenario_id;
	u16 *list = ctx->s_ctx.mode[scenario_id].mode_setting_table;
	u32 len = ctx->s_ctx.mode[scenario_id].mode_setting_len;

	for (i = 0; i < len; i = i+2) {
		ret |= subdrv_i2c_wr_u8(ctx, list[i], list[i+1]&0xff);
	}

	DRV_LOG_MUST(ctx, "-\n");
	return ERROR_NONE;
}


static int praguegc08a8ultra_streamon(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return praguegc08a8ultra_streaming_control(ctx, TRUE);
}

static int praguegc08a8ultra_streamoff(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return praguegc08a8ultra_streaming_control(ctx, FALSE);
}

static int praguegc08a8ultra_streaming_control(struct subdrv_ctx *ctx, bool enable)
{
	DRV_LOG_MUST(ctx, "streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x01);
	} else {
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x00);
	}

	return ERROR_NONE;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int ultra_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt, u64 sof_ts)
{
	u16 sensor_output_cnt;

	sensor_output_cnt = (subdrv_i2c_rd_u8(ctx, 0x0146) << 8);
	sensor_output_cnt |= subdrv_i2c_rd_u8(ctx, 0x0147);

	DRV_LOG_MUST(ctx, "sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->current_scenario_id, sof_cnt, sensor_output_cnt);

	return 0;
}

static u16 get_gain2reg(u32 gain)
{
	return (gain * 1024 / BASEGAIN);
}

static int praguegc08a8ultra_set_auto_flicker_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	bool enable = *((bool *)para);

	if (enable) {
		ctx->autoflicker_en = KAL_TRUE;
		DRV_LOG_MUST(ctx, "flicker enable\n");
	} else {
		ctx->autoflicker_en = KAL_FALSE;
	}

	return ERROR_NONE;
}

static int praguegc08a8ultra_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 gain = *((u32 *)para);
	u16 reg_gain;

	/* check boundary of gain */
	gain = max(gain,
		ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[0].min);
	gain = min(gain,
		ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[0].max);

	reg_gain = get_gain2reg(gain);

	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	ctx->ana_gain[0] = gain;

	DRV_LOG(ctx, "ctx->sensor_mode: %d, gain = %d, ctx->ana_gain[0] = 0x%x ,reg_gain = 0x%x, max_gain:0x%x\n",
		ctx->current_scenario_id, gain, ctx->ana_gain[0], reg_gain, ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[0].max);

	/* write gain */
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_ana_gain[0].addr[0],
		(reg_gain >> 8) & 0xFF);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_ana_gain[0].addr[1],
		reg_gain & 0xFF);

	return ERROR_NONE;
}

void praguegc08a8ultra_write_frame_length(struct subdrv_ctx *ctx, u32 fll)
{
	u32 addr_h = ctx->s_ctx.reg_addr_frame_length.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_frame_length.addr[1];
	u32 fll_step = 0;

	check_current_scenario_id_bound(ctx);
	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	if (fll_step)
		fll = roundup(fll, fll_step);
	ctx->frame_length = fll;

	subdrv_i2c_wr_u8(ctx, addr_h, (fll >> 8) & 0xFF);
	subdrv_i2c_wr_u8(ctx, addr_l, fll & 0xFF);

	/* update FL RG value after setting buffer for writing RG */
	ctx->frame_length_rg = ctx->frame_length;

	DRV_LOG(ctx,
		"ctx:(fl(RG):%u), fll[0x%x], fll_step:%u\n",
		ctx->frame_length_rg, fll, fll_step);
}
static int praguegc08a8ultra_set_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u16 frame_length = *((u16 *)para);
	if (frame_length)
		ctx->frame_length = frame_length;
	ctx->frame_length = max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);

	praguegc08a8ultra_write_frame_length(ctx, ctx->frame_length);

	DRV_LOG(ctx, "fll(input/output/min):%u/%u/%u\n",
		frame_length, ctx->frame_length, ctx->min_frame_length);

	return ERROR_NONE;
}

static int praguegc08a8ultra_set_long_exposure(struct subdrv_ctx *ctx, u64 shutter)
{
	u64 cal_shutter = 0;

	long_exp_status = true;
	cal_shutter = (shutter - 0xa00)/4 - 1;
	DRV_LOG(ctx, "set longExp : shutter =%llu, cal_shutter =%llu, long_exp_status : %d\n", shutter, cal_shutter, long_exp_status);

	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_frame_length.addr[0], 0x0a);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_frame_length.addr[1], 0x10);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[0], 0x0a);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[1], 0x00);
	subdrv_i2c_wr_u8(ctx, 0x022d, 0x30 | ((cal_shutter >> 16) & 0xF));
	subdrv_i2c_wr_u8(ctx, 0x022e, (cal_shutter >> 8) & 0xFF);
	subdrv_i2c_wr_u8(ctx, 0x022f, cal_shutter & 0xFF);

	return ERROR_NONE;
}

static int praguegc08a8ultra_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 shutter = *((u64 *)para);
	u32 frame_length = 0;
	u32 fine_integ_line = 0;
	// u16 realtime_fps = 0;

	DRV_LOG(ctx, "set shutter =%llu\n", shutter);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;

	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;

	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
 
	if (shutter >= 0xFFEE) {
		praguegc08a8ultra_set_long_exposure(ctx, shutter);
	} else {
		shutter = max_t(u64, shutter,
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].min);
		shutter = min_t(u64, shutter,
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].max);
		/* check boundary of framelength */
		ctx->frame_length = max((u32)shutter + ctx->s_ctx.exposure_margin, ctx->min_frame_length);
		ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);

		/* restore shutter */
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		ctx->exposure[0] = (u32)shutter;
		if (set_auto_flicker(ctx, 0) || ctx->frame_length) {
			praguegc08a8ultra_write_frame_length(ctx, ctx->frame_length);
		}
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[1],
			ctx->exposure[0] & 0xFF);
		if (long_exp_status) {
			long_exp_status = false;
			subdrv_i2c_wr_u8(ctx, 0x022d, 0x20);
			subdrv_i2c_wr_u8(ctx, 0x022e, 0x00);
			subdrv_i2c_wr_u8(ctx, 0x022f, 0x00);
		}
		DRV_LOG(ctx, "set shutter =%llu, framelength =%d, long_exp_status : %d\n", shutter, ctx->frame_length, long_exp_status);
	}

	return ERROR_NONE;
}

static int praguegc08a8ultra_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 *shutters = (u64 *)(* feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));
	u16 frame_length = (u16) (*(feature_data + 2));

	if (exp_cnt == 1) {
		ctx->shutter = shutters[0];

		DRV_LOG(ctx, "set multi_shutter =%llu\n", shutters[0]);

		if (shutters[0] >= 0xFFEE) {
			praguegc08a8ultra_set_long_exposure(ctx, shutters[0]);
		} else {
			if (shutters[0] > ctx->s_ctx.mode[ctx->current_scenario_id].framelength - ctx->s_ctx.exposure_margin)
				ctx->frame_length = shutters[0] + ctx->s_ctx.exposure_margin;
			else
				ctx->frame_length = ctx->s_ctx.mode[ctx->current_scenario_id].framelength;
			if (frame_length > ctx->frame_length)
				ctx->frame_length = frame_length;

			if (ctx->frame_length > ctx->exposure_max)
				ctx->frame_length = ctx->exposure_max;

			if (shutters[0] < ctx->exposure_min)
				shutters[0] = ctx->exposure_min;

			if (set_auto_flicker(ctx, 0) || ctx->frame_length) {
				praguegc08a8ultra_write_frame_length(ctx, ctx->frame_length);
			}
			subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[0],
				(shutters[0] >> 8) & 0xFF);
			subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[1],
				shutters[0] & 0xFF);
			if (long_exp_status) {
				long_exp_status = false;
				subdrv_i2c_wr_u8(ctx, 0x022d, 0x20);
				subdrv_i2c_wr_u8(ctx, 0x022e, 0x00);
				subdrv_i2c_wr_u8(ctx, 0x022f, 0x00);
			}
			DRV_LOG(ctx, "shutter =%llu, framelength =%d, long_exp_status : %d\n", shutters[0], ctx->frame_length, long_exp_status);
		}
	}

	return ERROR_NONE;
}

static int praguegc08a8ultra_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	DRV_LOG(ctx, "+\n");
	praguegc08a8ultra_set_shutter_frame_length(ctx, para, len);
	return ERROR_NONE;
}

static int praguegc08a8ultra_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x008d, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x008c, 0x01);
		break;
	default:
		break;
	}

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static void praguegc08a8ultra_set_dummy(struct subdrv_ctx *ctx)
{
	u32 addr_h = ctx->s_ctx.reg_addr_frame_length.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_frame_length.addr[1];
	u32 fll = ctx->frame_length;
	u32 fll_step = 0;

	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	if (fll_step)
		fll = roundup(fll, fll_step);
	ctx->frame_length = fll;

	subdrv_i2c_wr_u8(ctx, addr_h, (fll >> 8) & 0xFF);
	subdrv_i2c_wr_u8(ctx, addr_l, fll & 0xFF);

	/* update FL RG value after setting buffer for writing RG */
	ctx->frame_length_rg = ctx->frame_length;

	DRV_LOG(ctx,
		"ctx:(fl(RG):%u), fll[0x%x], fll_step:%u\n",
		ctx->frame_length_rg, fll, fll_step);
}


static int praguegc08a8ultra_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if (framerate == 0) {
		DRV_LOG(ctx, "framerate should not be 0\n");
		return ERROR_NONE;
	}

	if (ctx->s_ctx.mode[scenario_id].linelength == 0) {
		DRV_LOG(ctx, "linelength should not be 0\n");
		return ERROR_NONE;
	}

	if (ctx->line_length == 0) {
		DRV_LOG(ctx, "ctx->line_length should not be 0\n");
		return ERROR_NONE;
	}

	if (ctx->frame_length == 0) {
		DRV_LOG(ctx, "ctx->frame_length should not be 0\n");
		return ERROR_NONE;
	}

	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1\n",
		framerate, ctx->current_fps, scenario_id);
	if (ctx->frame_length > (ctx->exposure[0] + ctx->s_ctx.exposure_margin))
		praguegc08a8ultra_set_dummy(ctx);
	return ERROR_NONE;
}

static int praguegc08a8ultra_close(struct subdrv_ctx *ctx)
{
	DRV_LOG(ctx, "praguegc08a8ultra_close\n");
	return ERROR_NONE;
}

static int praguegc08a8ultra_get_csi_param(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id, struct mtk_csi_param *csi_param){
	switch (scenario_id) {
	default:
		csi_param->cdr_delay_enable = 1;
		csi_param->cdr_delay        = 16;
		csi_param->eq_enable = 1;
		csi_param->eq_bw     = 3;
		csi_param->eq_dg0_en = 0;
		csi_param->eq_sr0    = 0;
		csi_param->eq_dg1_en = 0;
		csi_param->eq_sr1    = 0;
		break;
	}
	DRV_LOG(ctx, "scenario_id:%u, eq param custom:%d/%d %d/%d %d/%d\n", scenario_id,
		csi_param->eq_enable, csi_param->eq_bw,
		csi_param->eq_dg0_en, csi_param->eq_sr0,
		csi_param->eq_dg1_en, csi_param->eq_sr1);

	DRV_LOG(ctx, "praguegc08a8ultra_get_csi_param func\n");
	return 0;
}
