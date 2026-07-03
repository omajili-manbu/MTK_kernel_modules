// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "@(%s:%d) " fmt, __func__, __LINE__

#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>

#include <connectivity_build_in_adapter.h>

#include "../../../../base/include/osal.h"
#include "../include/consys_hw.h"
#include "../include/consys_reg_util.h"
#include "../include/pmic_mng.h"
#include "include/mt6881.h"
#include "include/mt6881_consys_reg_offset.h"
#include "include/mt6881_pmic.h"
#include "include/mt6881_pos.h"
#include "include/mt6881_pos_gen.h"
#include "conninfra_conf.h"

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
static struct regulator *reg_VCN13;
static struct regulator *reg_VRFIO18; /* MT6363 workaround VCN15 -> VRFIO18 */

static struct regulator *reg_VCN33_1;
static struct regulator *reg_VCN33_2;
static struct regulator *reg_VANT18;

static struct regulator *reg_buckboost;

static struct notifier_block vrfio18_nb;
static struct notifier_block vcn13_nb;

static struct conninfra_dev_cb* g_dev_cb;

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static int consys_plt_pmic_get_from_dts_mt6881(struct platform_device*, struct conninfra_dev_cb*);

static int consys_plt_pmic_common_power_ctrl_mt6881(unsigned int, unsigned int curr_status, unsigned int next_status);
static int consys_plt_pmic_common_power_ctrl_mt6881_6631(unsigned int, unsigned int curr_status, unsigned int next_status);
static int consys_plt_pmic_common_power_ctrl_mt6881_6631_6686(unsigned int, unsigned int curr_status, unsigned int next_status);
static int consys_plt_pmic_common_power_low_power_mode_mt6881(unsigned int, unsigned int curr_status, unsigned int next_status);
static int consys_plt_pmic_common_power_low_power_mode_mt6881_6631(unsigned int, unsigned int curr_status, unsigned int next_status);
static int consys_plt_pmic_common_power_low_power_mode_mt6881_6631_6686(unsigned int, unsigned int curr_status, unsigned int next_status);
/* Function Pointer for Do-nothing*/
static int consys_plt_pmic_no_need_ctrl_mt6881(unsigned int);
/* MT6637, WIFI+BT, control VCN33_1 in Legacy mode */
/* MT6637, WIFI, control VCN33_2 in Legacy mode */
/* MT6637, GPS, control VANT18 in Legacy mode */
static int consys_plt_pmic_wifi_power_ctrl_mt6881(unsigned int);
static int consys_plt_pmic_bt_power_ctrl_mt6881(unsigned int);
static int consys_plt_pmic_gps_power_ctrl_mt6881(unsigned int);
static int consys_pmic_vcn33_1_power_ctl_mt6881_lg(bool);
static int consys_pmic_vcn33_2_power_ctl_mt6881_lg(bool);
#if !IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
static int consys_pmic_vant18_power_ctl_mt6881_lg(bool);
#endif
/* MT6631 only, FM+GPS, control VCN33_2 in RC/Legacy mode */
static int consys_plt_pmic_gps_power_ctrl_mt6881_6631(unsigned int);
static int consys_plt_pmic_fm_power_ctrl_mt6881_6631(unsigned int);
static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_rc(bool);
static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_lg(bool);
/* MT6631+MT6686, GPS, control VANT18 in RC/Legacy mode */
/* MT6631+MT6686, FM, control VCN33_2 in RC/Legacy mode */
static int consys_plt_pmic_gps_power_ctrl_mt6881_6631_6686(unsigned int);
static int consys_plt_pmic_fm_power_ctrl_mt6881_6631_6686(unsigned int);
static int consys_pmic_vant18_power_ctl_mt6881_6631_6686_rc(bool);
static int consys_pmic_vant18_power_ctl_mt6881_6631_6686_lg(bool);
static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_6686_rc(bool);
static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_6686_lg(bool);


static int consys_vcn13_oc_notify(struct notifier_block*, unsigned long, void*);
static int consys_vrfio18_oc_notify(struct notifier_block*, unsigned long, void*);
static int consys_plt_pmic_event_notifier_mt6881(unsigned int, unsigned int);
static int consys_plt_pmic_event_notifier_mt6881_6631(unsigned int, unsigned int);

const struct consys_platform_pmic_ops g_consys_platform_pmic_ops_mt6881_6637 = {
	.consys_pmic_get_from_dts = consys_plt_pmic_get_from_dts_mt6881,
	.consys_pmic_common_power_ctrl = consys_plt_pmic_common_power_ctrl_mt6881,
	.consys_pmic_common_power_low_power_mode = consys_plt_pmic_common_power_low_power_mode_mt6881,
	.consys_pmic_wifi_power_ctrl = consys_plt_pmic_wifi_power_ctrl_mt6881,
	.consys_pmic_bt_power_ctrl = consys_plt_pmic_bt_power_ctrl_mt6881,
	.consys_pmic_gps_power_ctrl = consys_plt_pmic_gps_power_ctrl_mt6881,
	.consys_pmic_fm_power_ctrl = consys_plt_pmic_no_need_ctrl_mt6881,
	.consys_pmic_event_notifier = consys_plt_pmic_event_notifier_mt6881,
};

const struct consys_platform_pmic_ops g_consys_platform_pmic_ops_mt6881_6631 = {
	.consys_pmic_get_from_dts = consys_plt_pmic_get_from_dts_mt6881,
	.consys_pmic_common_power_ctrl = consys_plt_pmic_common_power_ctrl_mt6881_6631,
	.consys_pmic_common_power_low_power_mode = consys_plt_pmic_common_power_low_power_mode_mt6881_6631,
	.consys_pmic_wifi_power_ctrl = consys_plt_pmic_no_need_ctrl_mt6881,
	.consys_pmic_bt_power_ctrl = consys_plt_pmic_no_need_ctrl_mt6881,
	.consys_pmic_gps_power_ctrl = consys_plt_pmic_gps_power_ctrl_mt6881_6631,
	.consys_pmic_fm_power_ctrl = consys_plt_pmic_fm_power_ctrl_mt6881_6631,
	.consys_pmic_event_notifier = consys_plt_pmic_event_notifier_mt6881_6631,
};

const struct consys_platform_pmic_ops g_consys_platform_pmic_ops_mt6881_6631_6686 = {
	.consys_pmic_get_from_dts = consys_plt_pmic_get_from_dts_mt6881,
	.consys_pmic_common_power_ctrl = consys_plt_pmic_common_power_ctrl_mt6881_6631_6686,
	.consys_pmic_common_power_low_power_mode = consys_plt_pmic_common_power_low_power_mode_mt6881_6631_6686,
	.consys_pmic_wifi_power_ctrl = consys_plt_pmic_no_need_ctrl_mt6881,
	.consys_pmic_bt_power_ctrl = consys_plt_pmic_no_need_ctrl_mt6881,
	.consys_pmic_gps_power_ctrl = consys_plt_pmic_gps_power_ctrl_mt6881_6631_6686,
	.consys_pmic_fm_power_ctrl = consys_plt_pmic_fm_power_ctrl_mt6881_6631_6686,
	.consys_pmic_event_notifier = consys_plt_pmic_event_notifier_mt6881_6631,
};

int consys_plt_pmic_get_from_dts_mt6881(struct platform_device *pdev, struct conninfra_dev_cb* dev_cb)
{
	int ret;
	const struct conninfra_conf *conf = NULL;
	unsigned int vcn33_1_voltage = 0;

	g_dev_cb = dev_cb;
	reg_VCN13 = devm_regulator_get_optional(&pdev->dev, "mt6363_vcn13");
	if (IS_ERR(reg_VCN13)) {
		pr_err("Regulator_get VCN_13 fail\n");
		reg_VCN13 = NULL;
	} else {
		vcn13_nb.notifier_call = consys_vcn13_oc_notify;
		ret = devm_regulator_register_notifier(reg_VCN13, &vcn13_nb);
		if (ret)
			pr_info("VCN13 regulator notifier request failed\n");
	}

	reg_VRFIO18 = devm_regulator_get(&pdev->dev, "mt6363_vrfio18");
	if (IS_ERR(reg_VRFIO18)) {
		pr_err("Regulator_get VCN_18 fail\n");
		reg_VRFIO18 = NULL;
	} else {
		vrfio18_nb.notifier_call = consys_vrfio18_oc_notify;
		ret = devm_regulator_register_notifier(reg_VRFIO18, &vrfio18_nb);
		if (ret)
			pr_info("VRFIO18 regulator notifier request failed\n");
	}

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	reg_VCN33_1 = devm_regulator_get(&pdev->dev, "mt6373_vcn33_1");
#else
	reg_VCN33_1 = devm_regulator_get(&pdev->dev, "mt6368_vcn33_1");
#endif
	if (IS_ERR(reg_VCN33_1)) {
		pr_err("Regulator_get VCN33_1 fail\n");
		reg_VCN33_1 = NULL;
	}

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	reg_VCN33_2 = devm_regulator_get(&pdev->dev, "mt6373_vcn33_2");
#else
	reg_VCN33_2 = devm_regulator_get(&pdev->dev, "mt6368_vcn33_2");
#endif
	if (IS_ERR(reg_VCN33_2)) {
		pr_err("Regulator_get VCN33_2 fail\n");
		reg_VCN33_2 = NULL;
	}

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	reg_VANT18 = devm_regulator_get(&pdev->dev, "mt6373_vant18");
#else
	reg_VANT18 = devm_regulator_get(&pdev->dev, "mt6368_vant18");
#endif
	if (IS_ERR(reg_VANT18)) {
		pr_err("Regulator_get VANT18 fail\n");
		reg_VANT18 = NULL;
	}
	reg_buckboost = devm_regulator_get_optional(&pdev->dev, "rt6160-buckboost-2");
	if (IS_ERR(reg_buckboost)) {
		pr_info("Regulator_get buckboost fail\n");
		reg_buckboost = NULL;
	}

	/* raise VCN33_1 to 3.5V */
	conf = conninfra_conf_get_cfg();
	if (NULL == conf)
		pr_notice("[%s] Get conf fail", __func__);
	else
		vcn33_1_voltage = conf->vcn33_1_voltage;

	if (reg_VCN33_1) {
		if ((vcn33_1_voltage != 0) && (consys_get_adie_chipid_mt6881() != ADIE_6637)) {
			regulator_set_voltage(reg_VCN33_1, vcn33_1_voltage, vcn33_1_voltage);
			pr_info("[%s] Raise VCN33_1 by customized options = %u", __func__, vcn33_1_voltage);
		} else {
			regulator_set_voltage(reg_VCN33_1, 3300000, 3300000);
			pr_info("[%s] Set VCN33_1 by default options = 3300000", __func__);
		}
	}

	return 0;
}

/* Enable [VRFIO18, VCN13] */
/* [VRFIO18, VCN13] is enabled */
int consys_plt_pmic_common_power_ctrl_mt6881(unsigned int enable, unsigned int curr_status, unsigned int next_status)
{
	int sleep_mode;
	int ret = 0;
	struct regmap *r6363 = g_regmap_mt6363;

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	struct regmap *r6373 = g_regmap_mt6373;
#else
	struct regmap *r6368 = g_regmap_mt6368;
#endif

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	if (r6363 == NULL || r6373 == NULL) {
#else
	if (r6363 == NULL || r6368 == NULL) {
#endif

		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if ((curr_status != 0) && (next_status != 0))
		return 0;

	sleep_mode = consys_get_sleep_mode_mt6881();
	if (enable) {
		/* set PMIC VRFIO18 LDO 1.7V */
		regulator_set_voltage(reg_VRFIO18, 1700000, 1700000);

		/*
		 * if (A-die sleep mode 3){
		 * 1. set PMIC VCN13 LDO 1.35V @Normal mode; 1.05V @LPM
		 * }else {
		 * 1. set PMIC VCN13 LDO 1.35V @Normal mode; 0.95V @LPM
		 * }
		 */
		/* no need for LPM because 0.95V is default setting. */
		regulator_set_voltage(reg_VCN13, 1350000, 1350000);

		/* set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VRFIO18);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN13 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN13);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN13, REGULATOR_MODE_NORMAL);
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		if ((consys_co_clock_type_mt6881() == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO) ||
			(consys_co_clock_type_mt6881() == CONNSYS_CLOCK_SCHEMATIC_52M_EXTCXO)) {
				regulator_set_voltage(reg_VANT18, 1800000, 1800000);
				regulator_set_mode(reg_VANT18, REGULATOR_MODE_NORMAL);
				ret = regulator_enable(reg_VANT18);
				pr_info("%s set reg_VANT18 SW_EN=1, ret = %d\n", __func__, ret);
			}
#endif
	} else {
		if (next_status != 0)
			return ret;
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		if (((consys_co_clock_type_mt6881() == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO) ||
			(consys_co_clock_type_mt6881() == CONNSYS_CLOCK_SCHEMATIC_52M_EXTCXO)) &&
			 regulator_is_enabled(reg_VANT18)) {
			ret = regulator_disable(reg_VANT18);
			if (ret)
				pr_notice("%s regulator_disable err:%d", __func__, ret);
		}
		/* set PMIC VCN33_1 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable)  (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#endif
		/* RC sleep_mode = 3, [VCN33_1, VCN33_2] is enabled */
		/* only do disable on this case */
		if (consys_is_rc_mode_enable_mt6881() && sleep_mode == 3) {
			ret = regulator_disable(reg_VCN33_1);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		}
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_2 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable)  (by "standard kernal PMIC API" and "PMIC table") */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#endif
		/* RC sleep_mode = 3, [VCN33_1, VCN33_2] is enabled */
		/* only do disable on this case */
		if (consys_is_rc_mode_enable_mt6881() && sleep_mode == 3) {
			ret = regulator_disable(reg_VCN33_2);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		}
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);

		/* wait 1ms for off VCN33_X */
		msleep(1);

		/* set PMIC VCN13 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable)  (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		ret = regulator_disable(reg_VCN13);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN13, REGULATOR_MODE_NORMAL);

		/* set PMIC VRFIO18 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		if (consys_is_rc_mode_enable_mt6881() == true && sleep_mode == 2) {
		} else {
			ret = regulator_disable(reg_VRFIO18);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		}
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);

		/* Set buckboost to 3.45V (for VCN33_1 & VCN33_2) */
		if (reg_buckboost) {
			regulator_set_voltage(reg_buckboost, 3450000, 3450000);
			pr_info("Set buckboost to 3.45V\n");
		}
	}
	return ret;
}

/* [VRFIO18, VCN13] is enabled */
/* RC sleep_mode = 3, Enable [VCN33_1, VCN33_2] */
int consys_plt_pmic_common_power_low_power_mode_mt6881(unsigned int enable, unsigned int curr_status, unsigned int next_status)
{
	int ret = 0;
	int sleep_mode;
	struct regmap *r6363 = g_regmap_mt6363;
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	struct regmap *r6373 = g_regmap_mt6373;
#else
	struct regmap *r6368 = g_regmap_mt6368;
#endif

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	if (r6363 == NULL || r6373 == NULL) {
#else
	if (r6363 == NULL || r6368 == NULL) {
#endif
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if ((curr_status != 0) && (next_status != 0))
		return 0;

	/* Set buckboost to 3.65V (for VCN33_1 & VCN33_2) */
	/* Notice that buckboost might not be enabled. */
	if (reg_buckboost) {
		regulator_set_voltage(reg_buckboost, 3650000, 3650000);
		pr_info("Set buckboost to 3.65V\n");
	}

	sleep_mode = consys_get_sleep_mode_mt6881();
	/*
	 * if (A-die sleep mode 3){
	 * 1. set PMIC VCN13 LDO 1.35V @Normal mode; 1.05V @LPM
	 * }else {
	 * 1. set PMIC VCN13 LDO 1.35V @Normal mode; 0.95V @LPM
	 * }
	 */
	if (sleep_mode == 3)
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_VOSEL_SLEEP_ADDR, 0x7f, 0x1);

	if (consys_is_rc_mode_enable_mt6881()) {
		/* 1. set PMIC VRFIO18 LDO PMIC HW mode control by PMRC_EN[9][8][7][6]  (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VRFIO18 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * if (A-die sleep mode-2 ){
		 * 2. set PMIC VRFIO18 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table")
		 * else{ //A-die sleep mode-1 or A-die sleep mode-3
		 * 2. set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by "standard kernal PMIC API" and "PMIC table")
		 * }
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_MODE_ADDR, 1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_EN_ADDR,   1 << 1, 1 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_CFG_ADDR,  1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_MODE_ADDR, 1 << 6, 0 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_EN_ADDR,   1 << 6, 1 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_CFG_ADDR,  1 << 6, 0 << 6);
		if (sleep_mode == 2) {
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
			/* may disable twice, but regulator will take care it, ok */
			ret = regulator_disable(reg_VRFIO18);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
			regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);
		} else if (sleep_mode == 1 || sleep_mode == 3) {
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
			/* pmic_common_power_ctrl has turned it on */
			/*
			ret = regulator_enable(reg_VRFIO18);
			if (ret)
				pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
			*/
			regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_IDLE);
		}

		/* 1. set PMIC VCN13 LDO PMIC HW mode control by PMRC_EN[9][8][7][6]  (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VCN13 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VCN13 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC9_OP_MODE_ADDR, 1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC9_OP_EN_ADDR,   1 << 1, 1 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC9_OP_CFG_ADDR,  1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC6_OP_MODE_ADDR, 1 << 6, 0 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC6_OP_EN_ADDR,   1 << 6, 1 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_RC6_OP_CFG_ADDR,  1 << 6, 0 << 6);
		/* 2. set PMIC VCN13 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		/* pmic_common_power_ctrl has turned it on */
		/*
		ret = regulator_enable(reg_VCN13);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN13, REGULATOR_MODE_IDLE);

		/*  */
		/* 1. set PMIC VCN33_1 LDO PMIC HW mode control by PMRC_EN[8][7]  (by ""standard kernal PMIC API"" and ""PMIC table"")
		 * 1.1. set PMIC VCN33_1 LDO op_mode = 0 (by ""standard kernal PMIC API"" and ""PMIC table"")
		 * 1.2. set PMIC VCN33_1 LDO  HW_OP_EN = 1, HW_OP_CFG = 0 (by ""standard kernal PMIC API"" and ""PMIC table"")
		 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
#endif
		if (sleep_mode == 3) {
			/*
			 * 2. set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW enable, SW ON) (by ""standard kernal PMIC API"" and ""PMIC table"")
			 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
			regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
#else
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
#endif
			ret = regulator_enable(reg_VCN33_1);
			if (ret)
				pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
			regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);

			/* 3. wait 210us */
			usleep_range(210, 1000);

			/*
			 * 4. set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by ""standard kernal PMIC API"" and ""PMIC table"")
			 */
			regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_IDLE);
		} else {
			/*
			 * 2. set PMIC VCN33_1 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (sw disable)
			 * (by ""standard kernal PMIC API"" and ""PMIC table"")
			 * No need to turn off MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR
			 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
			regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#else
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#endif
			/*
			ret = regulator_disable(reg_VCN33_1);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
			*/
			regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);
		}

		/* 1. set PMIC VCN33_2 LDO PMIC HW mode control by PMRC_EN[8]  (by ""standard kernal PMIC API"" and ""PMIC table"")
		 * 1.1. set PMIC VCN33_2 LDO op_mode = 0 (by ""standard kernal PMIC API"" and ""PMIC table"")
		 * 1.2. set PMIC VCN33_2 LDO  HW_OP_EN = 1, HW_OP_CFG = 0 (by ""standard kernal PMIC API"" and ""PMIC table"")
		 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
#endif
		if (sleep_mode == 3) {
			/*
			 * 2. set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW enable, SW ON) (by ""standard kernal PMIC API"" and ""PMIC table"")
			 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
			regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
#else
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
#endif
			ret = regulator_enable(reg_VCN33_2);
			if (ret)
				pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
			regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);

			/* 3. wait 210us */
			usleep_range(210, 1000);

			/*
			 * 4. set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by ""standard kernal PMIC API"" and ""PMIC table"")
			 */
			regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_IDLE);
		} else {
			/*
			 * 2. set PMIC VCN33_2 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by ""standard kernal PMIC API"" and ""PMIC table"")
			 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
			regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#else
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#endif
			/*
			ret = regulator_disable(reg_VCN33_2);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
			*/
			regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
		}

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		if ((consys_co_clock_type_mt6881() == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO) ||
	       (consys_co_clock_type_mt6881() == CONNSYS_CLOCK_SCHEMATIC_52M_EXTCXO)) {
			/* 1. set PMIC VANT18 LDO PMIC HW mode control by PMRC_EN[9][8][7][6] */
			/* 1.1. set PMIC VANT18 LDO op_mode = 0 */
			/* 1.2. set PMIC VANT18 LDO  HW_OP_EN = 1, HW_OP_CFG = 0 */
			/* 1. set PMIC VANT18 LDO PMIC HW mode control by PMRC_EN[9][8][7][6] */
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC9_OP_MODE_ADDR, 1 << 1, 0 << 1);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC9_OP_EN_ADDR,   1 << 1, 1 << 1);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC9_OP_CFG_ADDR,  1 << 1, 0 << 1);

			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);

			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);

			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC6_OP_MODE_ADDR, 1 << 6, 0 << 6);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC6_OP_EN_ADDR,   1 << 6, 1 << 6);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC6_OP_CFG_ADDR,  1 << 6, 0 << 6);

			ret = regulator_disable(reg_VANT18);
			pr_info("%s set reg_VANT18 SW_EN=0 for TCXO, ret = %d\n", __func__, ret);
		} else {
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC10_OP_MODE_ADDR, 1 << 2, 0 << 2);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC10_OP_EN_ADDR,   1 << 2, 1 << 2);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC10_OP_CFG_ADDR,  1 << 2, 0 << 2);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC6_OP_MODE_ADDR,  1 << 6, 0 << 6);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC6_OP_EN_ADDR,    1 << 6, 1 << 6);
			regmap_update_bits(r6373, MT6373_RG_LDO_VANT18_RC6_OP_CFG_ADDR,   1 << 6, 0 << 6);
		}
#else
		/* 1. set PMIC VANT18 LDO PMIC HW mode control by PMRC_EN[10][6]  (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VANT18 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VANT18 LDO  HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC10_OP_MODE_ADDR, 1 << 2, 0 << 2);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC10_OP_EN_ADDR,   1 << 2, 1 << 2);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC10_OP_CFG_ADDR,  1 << 2, 0 << 2);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC6_OP_MODE_ADDR,  1 << 6, 0 << 6);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC6_OP_EN_ADDR,    1 << 6, 1 << 6);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC6_OP_CFG_ADDR,   1 << 6, 0 << 6);
#endif
	} else {
		/* 1. set PMIC VRFIO18 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VRFIO18 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		/* if (A-die sleep mode-2 ){
		 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * }else{ //A-die sleep mode-1, 3
		 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * }
		 */
		if (sleep_mode == 2) {
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
		} else {
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_CFG_ADDR,  1 << 0, 1 << 0);
		}

		/* 1. set PMIC VCN13 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VCN13 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VCN13 LDO HW_OP_EN = 1, HW_OP_CFG = 1 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VCN13_HW0_OP_CFG_ADDR,  1 << 0, 1 << 0);
	}

	return ret;
}

/* [VRFIO18, VCN13] is enabled */
/* RC sleep_mode = 3, [VCN33_1, VCN33_2] is enabled */
/* Legacy WIFI/BT enable [VCN33_1] */
/* Legacy WIFI enable [VCN33_2] */
/* Legacy GPS enable [VANT18] */
int consys_plt_pmic_wifi_power_ctrl_mt6881(unsigned int enable)
{
	int ret;

	/* necessary in legacy mode only */
	if (consys_is_rc_mode_enable_mt6881())
		return 0;

	ret = consys_pmic_vcn33_1_power_ctl_mt6881_lg(enable);
	if (ret)
		pr_info("%s VCN33_1 fail\n", (enable? "Enable" : "Disable"));

	ret = consys_pmic_vcn33_2_power_ctl_mt6881_lg(enable);
	if (ret)
		pr_info("%s VCN33_2 fail\n", (enable? "Enable" : "Disable"));

	return ret;
}

int consys_plt_pmic_bt_power_ctrl_mt6881(unsigned int enable)
{
	/* necessary in legacy mode only */
	if (consys_is_rc_mode_enable_mt6881())
		return 0;

	return consys_pmic_vcn33_1_power_ctl_mt6881_lg(enable);
}

int consys_plt_pmic_gps_power_ctrl_mt6881(unsigned int enable)
{
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	return 0;
#else
	/* necessary in legacy mode only */
	if (consys_is_rc_mode_enable_mt6881())
		return 0;

	return consys_pmic_vant18_power_ctl_mt6881_lg(enable);
#endif
}

int consys_plt_pmic_no_need_ctrl_mt6881(unsigned int enable)
{
	return 0;
}

static int consys_pmic_vcn33_1_power_ctl_mt6881_lg(bool enable)
{
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	struct regmap *r6373 = g_regmap_mt6373;
#else
	struct regmap *r6368 = g_regmap_mt6368;
#endif
	static int enable_count = 0;
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	if (r6373 == NULL) {
#else
	if (r6368 == NULL) {
#endif
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	/* In legacy mode, VCN33_1 should be turned on either WIFI or BT is on */
	/* we use a counter to record the usage. */
	if (enable)
		enable_count++;
	else
		enable_count--;

	pr_info("%s enable_count %d\n", __func__, enable_count);
	if (enable_count < 0 || enable_count >= 2) {
		pr_info("enable_count %d is unexpected!!!\n", enable_count);
		return 0;
	}

	if (enable_count == 1 && enable == 1) { /* enable_count fron 0 to 1 */
		/* 1. set PMIC VCN33_1 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VCN33_1 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VCN33_1  LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
#endif
	} else if (enable_count == 1 && enable == 0) { /* enable_count fron 2 to 1 */
		return 0;
	} else if (enable_count == 0) { /* enable_count fron 1 to 0 */
		/* set PMIC VCN33_1 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable)  (by "standard kernal PMIC API" and "PMIC table") */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#endif
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VCN33_1);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);
		return 0;
	}

	return ret;
}

static int consys_pmic_vcn33_2_power_ctl_mt6881_lg(bool enable)
{
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	struct regmap *r6373 = g_regmap_mt6373;
#else
	struct regmap *r6368 = g_regmap_mt6368;
#endif
	int ret = 0;

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	if (r6373 == NULL) {
#else
	if (r6368 == NULL) {
#endif
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		/* 1. set PMIC VCN33_2 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VCN33_2 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VCN33_2 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
#endif
	} else {
		/* set PMIC VCN33_2 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
		regmap_update_bits(r6373, MT6373_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#else
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
#endif
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	}

	return ret;
}

#if !IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
static int consys_pmic_vant18_power_ctl_mt6881_lg(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;
	int ret = 0;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		/* 1. set PMIC VANT18 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VANT18 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VANT18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_EN_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_CFG_ADDR, 1 << 0, 0 << 0);

		/* 2. set PMIC VANT18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW enable, SW ON) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VANT18);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VANT18, REGULATOR_MODE_NORMAL);
	} else {
		/* set PMIC VANT18 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		ret = regulator_disable(reg_VANT18);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VANT18, REGULATOR_MODE_NORMAL);
	}

	return ret;
}
#endif

#define LOG_ADIE_REG_ARRAY_SZ 512
static char adie_reg_array_buf[LOG_ADIE_REG_ARRAY_SZ] = {'\0'};
static void dump_adie_cr(enum sys_spi_subsystem subsystem, const unsigned int *adie_cr, int num, char *title)
{
#define LOG_TMP_REG_SZ 32
	char tmp[LOG_TMP_REG_SZ] = {'\0'};
	unsigned int adie_value;
	int i;

	memset(adie_reg_array_buf, '\0', LOG_ADIE_REG_ARRAY_SZ);
	for (i = 0; i < num; i++) {
		if (consys_hw_spi_read(subsystem, adie_cr[i], &adie_value) < 0) {
			pr_notice("[%s] consys_hw_spi_read failed\n", __func__);
			continue;
		}
		if (snprintf(tmp, LOG_TMP_REG_SZ, "[0x%04x: 0x%08x]", adie_cr[i], adie_value) >= 0)
			strncat(adie_reg_array_buf, tmp,
				LOG_ADIE_REG_ARRAY_SZ - strlen(adie_reg_array_buf) - 1);
	}
	pr_info("%s:%s\n", title, adie_reg_array_buf);
}

static int consys_plt_pmic_event_notifier_mt6881(unsigned int id, unsigned int event)
{
#define ATOP_DUMP_NUM 14
#define ABT_DUMP_NUM 6
#define AWF_DUMP_NUM 3
	int ret = 0;
	const unsigned int adie_top_cr_list[ATOP_DUMP_NUM] = {
		0x03C, 0x090, 0x094, 0x0A0,
		0x0C8, 0x0FC, 0xA10, 0xB00,
		0xAFC, 0x160, 0xC54, 0xC58,
		0x0BC, 0x078,
	};
	const unsigned int adie_bt_cr_list[ABT_DUMP_NUM] = {
		0xFF, 0xA4, 0x41, 0x42, 0x18, 0x15,
	};
	const unsigned int adie_wf_cr_list[AWF_DUMP_NUM] = {
		0xFFF, 0x81, 0x80,
	};


	consys_pmic_debug_log_mt6881();

	ret = consys_hw_force_conninfra_wakeup();
	if (ret) {
		pr_info("[%s] force conninfra wakeup fail\n", __func__);
		return -1;
	}

	/* dump d-die cr */
	consys_hw_is_bus_hang();

	/* dump a-die cr */
	dump_adie_cr(SYS_SPI_TOP, adie_top_cr_list, ATOP_DUMP_NUM, "A-die TOP");
	dump_adie_cr(SYS_SPI_BT, adie_bt_cr_list, ABT_DUMP_NUM, "A-die BT");
	consys_hw_adie_top_ck_en_on(CONNSYS_ADIE_CTL_HOST_CONNINFRA);
	consys_hw_spi_update_bits(SYS_SPI_TOP, 0x580, 0x00, 0x10);
	consys_hw_adie_top_ck_en_off(CONNSYS_ADIE_CTL_HOST_CONNINFRA);
	dump_adie_cr(SYS_SPI_TOP, adie_top_cr_list, ATOP_DUMP_NUM, "A-die TOP");
	dump_adie_cr(SYS_SPI_WF, adie_wf_cr_list, AWF_DUMP_NUM, "A-die WF0");
	dump_adie_cr(SYS_SPI_WF1, adie_wf_cr_list, AWF_DUMP_NUM, "A-die WF1");

	consys_hw_force_conninfra_sleep();

	return 0;
}

static int consys_plt_pmic_event_notifier_mt6881_6631(unsigned int id, unsigned int event)
{
#define ATOP_DUMP_NUM_6631 8
#define ABT_DUMP_NUM_6631 1
#define AGPS_DUMP_NUM_6631 2
	int ret = 0;
	const unsigned int adie_top_cr_list[ATOP_DUMP_NUM_6631] = {
		0x080, 0x084, 0x0C0, 0xA00,
		0xA04, 0xA08, 0xA0C, 0xA10,
	};
	const unsigned int adie_bt_cr_list[ABT_DUMP_NUM_6631] = {
		0xA4,
	};
	const unsigned int adie_gps_cr_list[AGPS_DUMP_NUM_6631] = {
		0x500, 0x501,
	};

	consys_pmic_debug_log_mt6881();

	ret = consys_hw_force_conninfra_wakeup();
	if (ret) {
		pr_info("[%s] force conninfra wakeup fail\n", __func__);
		return -1;
	}

	/* dump d-die cr */
	consys_hw_is_bus_hang();

	/* dump a-die cr */
	dump_adie_cr(SYS_SPI_TOP, adie_top_cr_list, ATOP_DUMP_NUM_6631, "A-die TOP");
	dump_adie_cr(SYS_SPI_BT, adie_bt_cr_list, ABT_DUMP_NUM_6631, "A-die BT");
	dump_adie_cr(SYS_SPI_GPS, adie_gps_cr_list, AGPS_DUMP_NUM_6631, "A-die GPS");

	consys_hw_force_conninfra_sleep();

	return 0;
}

static int consys_vcn13_oc_notify(struct notifier_block *nb, unsigned long event,
				  void *unused)
{
	static int oc_counter = 0;
	static int oc_dump = 0;

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;

	oc_counter++;
	pr_info("[%s] VCN13 OC times: %d\n", __func__, oc_counter);

	if (oc_counter <= 30)
		oc_dump = 1;
	else if (oc_counter == (oc_dump * 100))
		oc_dump++;
	else
		return NOTIFY_OK;

	if (g_dev_cb != NULL && g_dev_cb->conninfra_pmic_event_notifier != NULL)
		g_dev_cb->conninfra_pmic_event_notifier(0, 0);

	return NOTIFY_OK;
}

static int consys_vrfio18_oc_notify(struct notifier_block *nb, unsigned long event,
				  void *unused)
{
	static int oc_counter = 0;
	static int oc_dump = 0;

	if (event != REGULATOR_EVENT_OVER_CURRENT)
		return NOTIFY_OK;

	oc_counter++;
	pr_info("[%s] VRFIO18 OC times: %d\n", __func__, oc_counter);

	if (oc_counter <= 30)
		oc_dump = 1;
	else if (oc_counter == (oc_dump * 100))
		oc_dump++;
	else
		return NOTIFY_OK;

	if (g_dev_cb != NULL && g_dev_cb->conninfra_pmic_event_notifier != NULL)
		g_dev_cb->conninfra_pmic_event_notifier(0, 0);

	return NOTIFY_OK;
}

void consys_pmic_debug_log_mt6881(void)
{
	struct regmap *r6363 = g_regmap_mt6363;
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	struct regmap *r6373 = g_regmap_mt6373;
#else
	struct regmap *r6368 = g_regmap_mt6368;
#endif
	int vcn13 = 0, vrfio18 = 0, vcn33_1 = 0, vcn33_2 = 0, vant18 = 0;

#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	if (r6363 == NULL || r6373 == NULL) {
#else
	if (r6363 == NULL || r6368 == NULL) {
#endif
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return;
	}

	regmap_read(r6363, MT6363_RG_LDO_VCN13_MON_ADDR, &vcn13);
	regmap_read(r6363, MT6363_RG_LDO_VRFIO18_MON_ADDR, &vrfio18);
#if IS_ENABLED(CONFIG_MTK_AUTO_SUPPORT)
	regmap_read(r6373, MT6373_RG_LDO_VCN33_1_MON_ADDR, &vcn33_1);
	regmap_read(r6373, MT6373_RG_LDO_VCN33_2_MON_ADDR, &vcn33_2);
	regmap_read(r6373, MT6373_RG_LDO_VANT18_MON_ADDR, &vant18);
#else
	regmap_read(r6368, MT6368_RG_LDO_VCN33_1_MON_ADDR, &vcn33_1);
	regmap_read(r6368, MT6368_RG_LDO_VCN33_2_MON_ADDR, &vcn33_2);
	regmap_read(r6368, MT6368_RG_LDO_VANT18_MON_ADDR, &vant18);
#endif

	pr_info("%s vcn13:0x%x,vrfio18:0x%x,vcn33_1:0x%x,vcn33_2:0x%x,vant18:0x%x\n",
		__func__, vcn13, vrfio18, vcn33_1, vcn33_2, vant18);
}

/* Enable [VRFIO18, VCN33_1, VCN33_2] */
/* [VRFIO18, VCN33_1, VCN33_2] is enabled */
int consys_plt_pmic_common_power_ctrl_mt6881_6631(unsigned int enable, unsigned int curr_status, unsigned int next_status)
{
	int ret = 0;
	struct regmap *r6363 = g_regmap_mt6363;
	struct regmap *r6368 = g_regmap_mt6368;

	if (r6363 == NULL || r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if ((curr_status != 0) && (next_status != 0))
		return 0;

	if (enable) {
		/* set PMIC VCN33_2 LDO 2.8V */
		regulator_set_voltage(reg_VCN33_2, 2800000, 2800000);

		/* set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VRFIO18);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN33_1);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	} else {
		/* set PMIC VCN33_2 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (sw disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VCN33_1);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);
		/* set PMIC VCN33_1 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table") */
		/* op_mode doesn't matter when HW_OP_EN = 0 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_EN_ADDR,   1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_EN_ADDR,   1 << 7, 0 << 7);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);

		/* set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (sw disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VRFIO18);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);
		/* set PMIC VRFIO18 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table") */
		/* op_mode doesn't matter when HW_OP_EN = 0 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_EN_ADDR,   1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_EN_ADDR,   1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_EN_ADDR,   1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_EN_ADDR,   1 << 6, 0 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);

		/* Set buckboost to 3.45V (for VCN33_1 & VCN33_2) */
		if (reg_buckboost) {
			regulator_set_voltage(reg_buckboost, 3450000, 3450000);
			pr_info("Set buckboost to 3.45V\n");
		}
	}
	return ret;
}

/* For [BT/WIFI/FM] */
/* [VRFIO18, VCN33_1, VCN33_2] is enabled */
/* Disable VCN33_2
 * First Power on for Efuse request
 * turn on VCN33_2 at PART#0, read efuse, turn off VCN33_2 at PART#2
 */
/* [VRFIO18, VCN33_1] is enabled */
int consys_plt_pmic_common_power_low_power_mode_mt6881_6631(unsigned int enable, unsigned int curr_status, unsigned int next_status)
{
	int ret = 0;
	struct regmap *r6363 = g_regmap_mt6363;
	struct regmap *r6368 = g_regmap_mt6368;

	if (r6363 == NULL || r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if ((curr_status != 0) && (next_status != 0))
		return 0;

	/* Set buckboost to 3.65V (for VCN33_1 & VCN33_2) */
	/* Notice that buckboost might not be enabled. */
	if (reg_buckboost) {
		regulator_set_voltage(reg_buckboost, 3650000, 3650000);
		pr_info("Set buckboost to 3.65V\n");
	}

	if (consys_is_rc_mode_enable_mt6881()) {
		/* 1. set PMIC VRFIO18 LDO PMIC HW mode control by PMRC_EN[9][8][7][6]  (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VRFIO18 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_MODE_ADDR, 1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_EN_ADDR,   1 << 1, 1 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC9_OP_CFG_ADDR,  1 << 1, 0 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_MODE_ADDR, 1 << 6, 0 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_EN_ADDR,   1 << 6, 1 << 6);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC6_OP_CFG_ADDR,  1 << 6, 0 << 6);
		/* 2. set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		/* pmic_common_power_ctrl has turned it on */
		/*
		ret = regulator_enable(reg_VRFIO18);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_IDLE);

		/* 1. set PMIC VCN33_1 LDO PMIC HW mode control by PMRC_EN[8][7]  (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VCN33_1 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VCN33_1 LDO  HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
		/* 2. set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW enable, SW ON) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		/* pmic_common_power_ctrl has turned it on */
		/*
		ret = regulator_enable(reg_VCN33_1);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);
		/* 3. wait 210us */
		usleep_range(210, 1000);
		/* 4. set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by "standard kernal PMIC API" and "PMIC table") */
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_IDLE);

		/* 1. set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
		/* 0. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table") (step 0. new add in 2025/9/27)
		 * 2. set PMIC VCN33_2 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
	} else {
		/* 1. set PMIC VRFIO18 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VRFIO18 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 1 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_CFG_ADDR,  1 << 0, 1 << 0);

		/* 1. set PMIC VCN33_1 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VCN33_1 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VCN33_1 LDO HW_OP_EN = 1, HW_OP_CFG = 1 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_CFG_ADDR,  1 << 0, 1 << 0);

		/* 1. set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
		/* 0. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table") (step 0. new add in 2025/9/27)
		 * 2. set PMIC VCN33_2 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);

	}

	return ret;
}

/* For [BT/WIFI/FM] */
/* [VRFIO18, VCN33_1] is enabled */
int consys_plt_pmic_gps_power_ctrl_mt6881_6631(unsigned int enable)
{
	if (consys_is_rc_mode_enable_mt6881()) {
		consys_pmic_vcn33_2_power_ctl_mt6881_6631_rc(enable);
	} else {
		consys_pmic_vcn33_2_power_ctl_mt6881_6631_lg(enable);
	}

	return 0;
}

int consys_plt_pmic_fm_power_ctrl_mt6881_6631(unsigned int enable)
{
	if (consys_is_rc_mode_enable_mt6881()) {
		consys_pmic_vcn33_2_power_ctl_mt6881_6631_rc(enable);
	} else {
		consys_pmic_vcn33_2_power_ctl_mt6881_6631_lg(enable);
	}

	return 0;
}

static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_rc(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;
	static int VCN28_enable_flag = 0;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		VCN28_enable_flag++;
		if (VCN28_enable_flag > 1)
			return 0;

		/* 1. set PMIC VCN33_2 LDO PMIC HW mode control by PMRC_EN[9][6]  (by "standard kernal PMIC API" and "PMIC table")
		 * 2. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 4. set PMIC VCN33_2 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC9_OP_MODE_ADDR, 1 << 1, 0 << 1);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC9_OP_EN_ADDR,   1 << 1, 1 << 1);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC9_OP_CFG_ADDR,  1 << 1, 0 << 1);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC6_OP_MODE_ADDR, 1 << 6, 0 << 6);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC6_OP_EN_ADDR,   1 << 6, 1 << 6);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC6_OP_CFG_ADDR,  1 << 6, 0 << 6);
		/* 3. set PMIC VCN33_2 LDO SW_OP_EN = 0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	} else {
		VCN28_enable_flag--;

		/* No Disable in POS, due to current HW Control no need to change */
	}

	return 0;
}

static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_lg(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;
	static int VCN33_2_enable_flag = 0;
	int ret = 0;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		VCN33_2_enable_flag++;
		if (VCN33_2_enable_flag > 1)
			return 0;

		/* 0-1. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 0-2. set PMIC VCN33_2 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);

		/* 0-3. set PMIC VCN33_2 LDO SW_OP_EN = 1, SW_EN = 1, SW_LP = 0 (SW enable, SW ON) */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	} else {
		VCN33_2_enable_flag--;
		if (VCN33_2_enable_flag > 0)
			return 0;

		/* 0. set PMIC VCN33_2 LDO SW_OP_EN = 1, SW_EN = 0, SW_LP = 0 (SW enable, SW OFF) (step 0. new add in 2025/9/27)
		 * 1. set PMIC VCN33_2 LDO to SW control mode (*1) (by "PMIC API" and "PMIC table")
		 * 2. turn off PMIC VCN33_2 LDO by driver (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	}

	return ret;
}

/* For [BT/WIFI/FM] */
/* Enable [VRFIO18, VCN33_1, VCN33_2] */
/* [VRFIO18, VCN33_1, VCN33_2] is enabled */
int consys_plt_pmic_common_power_ctrl_mt6881_6631_6686(unsigned int enable, unsigned int curr_status, unsigned int next_status)
{
	int ret = 0;
	unsigned int curr_bt = 0;
	unsigned int curr_wifi = 0;
	unsigned int curr_fm = 0;
	unsigned int next_bt = 0;
	unsigned int next_wifi = 0;
	unsigned int next_fm = 0;
	bool bt_wifi_fm_on = false;
	bool bt_wifi_fm_off = false;
	struct regmap *r6363 = g_regmap_mt6363;
	struct regmap *r6368 = g_regmap_mt6368;

	if (r6363 == NULL || r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	curr_bt = (curr_status & (0x1U << CONNDRV_TYPE_BT)) >> CONNDRV_TYPE_BT;
	curr_wifi = (curr_status & (0x1U << CONNDRV_TYPE_WIFI)) >> CONNDRV_TYPE_WIFI;
	curr_fm = (curr_status & (0x1U << CONNDRV_TYPE_FM)) >> CONNDRV_TYPE_FM;

	next_bt = (next_status & (0x1U << CONNDRV_TYPE_BT)) >> CONNDRV_TYPE_BT;
	next_wifi = (next_status & (0x1U << CONNDRV_TYPE_WIFI)) >> CONNDRV_TYPE_WIFI;
	next_fm = (next_status & (0x1U << CONNDRV_TYPE_FM)) >> CONNDRV_TYPE_FM;

	if (((curr_bt + curr_wifi + curr_fm) == 0) && ((next_bt + next_wifi + next_fm) != 0))
		bt_wifi_fm_on = true;

	if (((curr_bt + curr_wifi + curr_fm) != 0) && ((next_bt + next_wifi + next_fm) == 0))
		bt_wifi_fm_off = true;

	if (bt_wifi_fm_on) {
		/* set PMIC VCN33_2 LDO 2.8V */
		regulator_set_voltage(reg_VCN33_2, 2800000, 2800000);

		/* set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VRFIO18);
		if (ret != 0)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN33_1);
		if (ret != 0)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW ON)
		 * (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	}

	if (bt_wifi_fm_off == true){
		/* set build fail, it is controlled by FM. This can be remove */
		/* consys_m10_srclken_cfg_mt6881_gen(curr_status, next_status, 0); */

		/* set PMIC VCN33_2 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);

		/* set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VCN33_1);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);
		/* set PMIC VCN33_1 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table") */
		/* op_mode doesn't matter when HW_OP_EN = 0 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC10_OP_EN_ADDR,   1 << 2, 1 << 2);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);

		/* wait 1ms for off VCN33_X */
		msleep(1);

		/* set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VRFIO18);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_NORMAL);
		/* set PMIC VRFIO18 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table") */
		/* op_mode doesn't matter when HW_OP_EN = 0 */
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC10_OP_EN_ADDR,   1 << 1, 1 << 1);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
		regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);

		/* Set buckboost to 3.45V (for VCN33_1 & VCN33_2) */
		if (reg_buckboost) {
			regulator_set_voltage(reg_buckboost, 3450000, 3450000);
			pr_info("Set buckboost to 3.45V\n");
		}
	}

	return ret;
}

/* For [BT/WIFI/FM] */
/* [VRFIO18, VCN33_1, VCN33_2] is enabled */
/* Disable VCN33_2
 * First Power on for Efuse request
 * turn on VCN33_2 at PART#0, read efuse, turn off VCN33_2 at PART#2
 */
/* [VRFIO18, VCN33_1] is enabled */
int consys_plt_pmic_common_power_low_power_mode_mt6881_6631_6686(unsigned int enable, unsigned int curr_status, unsigned int next_status)
{
	int ret = 0;
	struct regmap *r6363 = g_regmap_mt6363;
	struct regmap *r6368 = g_regmap_mt6368;
	unsigned int curr_bt = 0;
	unsigned int curr_wifi = 0;
	unsigned int curr_fm = 0;
	unsigned int next_bt = 0;
	unsigned int next_wifi = 0;
	unsigned int next_fm = 0;
	bool bt_wifi_fm_on = false;

	if (r6363 == NULL || r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	curr_bt = (curr_status & (0x1U << CONNDRV_TYPE_BT)) >> CONNDRV_TYPE_BT;
	curr_wifi = (curr_status & (0x1U << CONNDRV_TYPE_WIFI)) >> CONNDRV_TYPE_WIFI;
	curr_fm = (curr_status & (0x1U << CONNDRV_TYPE_FM)) >> CONNDRV_TYPE_FM;

	next_bt = (next_status & (0x1U << CONNDRV_TYPE_BT)) >> CONNDRV_TYPE_BT;
	next_wifi = (next_status & (0x1U << CONNDRV_TYPE_WIFI)) >> CONNDRV_TYPE_WIFI;
	next_fm = (next_status & (0x1U << CONNDRV_TYPE_FM)) >> CONNDRV_TYPE_FM;

	if (((curr_bt + curr_wifi + curr_fm) == 0) && ((next_bt + next_wifi + next_fm) != 0))
		bt_wifi_fm_on = true;

	/* Set buckboost to 3.65V (for VCN33_1 & VCN33_2) */
	/* Notice that buckboost might not be enabled. */
	if (reg_buckboost) {
		regulator_set_voltage(reg_buckboost, 3650000, 3650000);
		pr_info("Set buckboost to 3.65V\n");
	}

	if (consys_is_rc_mode_enable_mt6881()) {
		if (bt_wifi_fm_on) {
			/* 1. set PMIC VRFIO18 LDO PMIC HW mode control by PMRC_EN[10][8][7]  (by "standard kernal PMIC API" and "PMIC table")
			 * 1.1. set PMIC VRFIO18 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
			 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
			 */
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC10_OP_MODE_ADDR, 1 << 1, 0 << 1);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC10_OP_EN_ADDR,   1 << 1, 1 << 1);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC10_OP_CFG_ADDR,  1 << 1, 0 << 1);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
			/* 2. set PMIC VRFIO18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (SW enable, SW LP) (by "standard kernal PMIC API" and "PMIC table") */
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
			/* pmic_common_power_ctrl has turned it on */
			/*
			ret = regulator_enable(reg_VRFIO18);
			if (ret)
				pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
			*/
			regulator_set_mode(reg_VRFIO18, REGULATOR_MODE_IDLE);

			/* 1. set PMIC VCN33_1 LDO PMIC HW mode control by PMRC_EN[10][8][7]   (by "standard kernal PMIC API" and "PMIC table")
			 * 1.1. set PMIC VCN33_1 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
			 * 1.2. set PMIC VCN33_1 LDO  HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
			 */
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC10_OP_MODE_ADDR, 1 << 2, 0 << 2);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC10_OP_EN_ADDR,   1 << 2, 1 << 2);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC10_OP_CFG_ADDR,  1 << 2, 0 << 2);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_MODE_ADDR, 1 << 0, 0 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC8_OP_CFG_ADDR,  1 << 0, 0 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_MODE_ADDR, 1 << 7, 0 << 7);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_EN_ADDR,   1 << 7, 1 << 7);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_RC7_OP_CFG_ADDR,  1 << 7, 0 << 7);
			/* 2. set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW enable, SW ON) (by "standard kernal PMIC API" and "PMIC table") */
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_1_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
			/* pmic_common_power_ctrl has turned it on */
			/*
			ret = regulator_enable(reg_VCN33_1);
			if (ret)
				pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
			*/
			regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_NORMAL);
			/* 3. wait 210us */
			usleep_range(210, 1000);
			/* 4. set PMIC VCN33_1 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =1 (sw lp) (by "standard kernal PMIC API" and "PMIC table") */
			regulator_set_mode(reg_VCN33_1, REGULATOR_MODE_IDLE);

			/* 1. set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
			ret = regulator_disable(reg_VCN33_2);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
			regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
			/* 0. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table") (step 0. new add in 2025/9/27)
			 * 2. set PMIC VCN33_2 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
			 */
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 0 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
		}
	} else {
		if (bt_wifi_fm_on) {
			/* If(GPS on), config VANT18 */

			/* 1. set PMIC VRFIO18 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
			 * 1.1. set PMIC VRFIO18 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
			 * 1.2. set PMIC VRFIO18 LDO HW_OP_EN = 1, HW_OP_CFG = 1 (by "standard kernal PMIC API" and "PMIC table")
			 */
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6363_RG_LDO_VRFIO18_HW0_OP_CFG_ADDR,  1 << 0, 1 << 0);

			/* 1. set PMIC VCN33_1 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
			 * 1.1. set PMIC VCN33_1 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
			 * 1.2. set PMIC VCN33_1 LDO HW_OP_EN = 1, HW_OP_CFG = 1 (by "standard kernal PMIC API" and "PMIC table")
			 */
			regmap_update_bits(r6363, MT6368_RG_LDO_VCN33_1_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6368_RG_LDO_VCN33_1_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
			regmap_update_bits(r6363, MT6368_RG_LDO_VCN33_1_HW0_OP_CFG_ADDR,  1 << 0, 1 << 0);

			/* 1. set PMIC VCN33_2 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
			ret = regulator_disable(reg_VCN33_2);
			if (ret)
				pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
			regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
			/* 0. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table") (step 0. new add in 2025/9/27)
			 * 2. set PMIC VCN33_2 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
			 */
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 0 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);
			regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
		}
	}

	return ret;
}

/* For [BT/WIFI/FM] */
/* [VRFIO18, VCN33_1] is enabled */
int consys_plt_pmic_gps_power_ctrl_mt6881_6631_6686(unsigned int enable)
{
	if (consys_is_rc_mode_enable_mt6881()) {
		consys_pmic_vant18_power_ctl_mt6881_6631_6686_rc(enable);
	} else {
		consys_pmic_vant18_power_ctl_mt6881_6631_6686_lg(enable);
	}

	return 0;
}

int consys_plt_pmic_fm_power_ctrl_mt6881_6631_6686(unsigned int enable)
{
	if (consys_is_rc_mode_enable_mt6881()) {
		consys_pmic_vcn33_2_power_ctl_mt6881_6631_6686_rc(enable);
		/* align API protocol, hard code curr_status, next_status */
		consys_m10_srclken_cfg_mt6881_gen(
			(enable ? 0 : (0x1U << CONNDRV_TYPE_FM)),
			(enable ? (0x1U << CONNDRV_TYPE_FM) : 0),
			(enable ? (0x1U << CONNDRV_TYPE_FM) : 0));
	} else {
		consys_pmic_vcn33_2_power_ctl_mt6881_6631_6686_lg(enable);
	}

	return 0;
}

static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_6686_rc(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		/* 1. set PMIC VCN33_2 LDO PMIC HW mode control by PMRC_EN[10]  (by "standard kernal PMIC API" and "PMIC table")
		 * 2. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 4. set PMIC VCN33_2 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC10_OP_MODE_ADDR, 1 << 2, 0 << 2);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC10_OP_EN_ADDR,   1 << 2, 1 << 2);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_RC10_OP_CFG_ADDR,  1 << 2, 0 << 2);
		/* 3. set PMIC VCN33_2 LDO SW_OP_EN = 0, SW_EN = 0, SW_LP =0 (SW disable) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	} else {
		/* No Disable in POS, due to current HW Control no need to change */
	}

	return 0;
}

static int consys_pmic_vcn33_2_power_ctl_mt6881_6631_6686_lg(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;
	int ret = 0;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		/* 0-1. set PMIC VCN33_2 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 0-2. set PMIC VCN33_2 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (HW disable)
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_MODE_ADDR, 1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);

		/* 0-3. set PMIC VCN33_2 LDO SW_OP_EN = 1, SW_EN = 1, SW_LP = 0 (SW enable, SW ON) */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
        } else {
		/* 0. set PMIC VCN33_2 LDO SW_OP_EN = 1, SW_EN = 0, SW_LP = 0 (SW enable, SW OFF) */
		regmap_update_bits(r6368, MT6368_RG_LDO_VCN33_2_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VCN33_2);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VCN33_2, REGULATOR_MODE_NORMAL);
	}
	return ret;
}

static int consys_pmic_vant18_power_ctl_mt6881_6631_6686_rc(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		/* 1. set PMIC VANT18 LDO PMIC HW mode control by PMRC_EN[6]  (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VANT18 LDO op_mode = 0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VANT18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC6_OP_MODE_ADDR, 1 << 6, 0 << 6);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC6_OP_EN_ADDR,   1 << 6, 1 << 6);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_RC6_OP_CFG_ADDR,  1 << 6, 0 << 6);
		/* 2. set PMIC VANT18 LDO SW_OP_EN =0, SW_EN = 0, SW_LP =0 (sw disable ) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_SW_OP_EN_ADDR, 1 << 7, 0 << 7);
		/* No enable, no need to disable */
		/*
		ret = regulator_disable(reg_VANT18);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		*/
		regulator_set_mode(reg_VANT18, REGULATOR_MODE_NORMAL);
	} else {
		/* No Disable in POS, due to current HW Control no need to change */
	}

	return 0;
}

static int consys_pmic_vant18_power_ctl_mt6881_6631_6686_lg(bool enable)
{
	struct regmap *r6368 = g_regmap_mt6368;
	int ret = 0;

	if (r6368 == NULL) {
		pr_err("%s[%d], regmap is NULL\n", __func__, __LINE__);
		return 0;
	}

	if (enable) {
		/* 1. set PMIC VANT18 LDO PMIC HW mode control by SRCCLKENA0 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.1. set PMIC VANT18 LDO op_mode = 1 (by "standard kernal PMIC API" and "PMIC table")
		 * 1.2. set PMIC VANT18 LDO HW_OP_EN = 1, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table")
		 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_EN_ADDR,   1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
		/* 2. set PMIC VANT18 LDO SW_OP_EN =1, SW_EN = 1, SW_LP =0 (SW enable, SW ON) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_enable(reg_VANT18);
		if (ret)
			pr_notice("%s[%d] regulator_enable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VANT18, REGULATOR_MODE_NORMAL);
	} else {
		/* 1. set PMIC VANT18 LDO HW_OP_EN = 0, HW_OP_CFG = 0 (by "standard kernal PMIC API" and "PMIC table") */
		/* op_mode doesn't matter when HW_OP_EN = 0 */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_MODE_ADDR, 1 << 0, 1 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_EN_ADDR,   1 << 0, 0 << 0);
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_HW0_OP_CFG_ADDR,  1 << 0, 0 << 0);
		/* 2. set PMIC VANT18 LDO SW_OP_EN =1, SW_EN = 0, SW_LP =0 (SW enable, SW OFF) (by "standard kernal PMIC API" and "PMIC table") */
		regmap_update_bits(r6368, MT6368_RG_LDO_VANT18_SW_OP_EN_ADDR, 1 << 7, 1 << 7);
		ret = regulator_disable(reg_VANT18);
		if (ret)
			pr_notice("%s[%d] regulator_disable err: %d", __func__, __LINE__, ret);
		regulator_set_mode(reg_VANT18, REGULATOR_MODE_NORMAL);
	}

	return ret;
}
