// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "../../../../include/conninfra.h"
#include "../include/connsys_smc.h"
#include "../include/consys_hw.h"
#include "include/mt6881.h"
#include "include/mt6881_atf.h"
#include "include/mt6881_connsyslog.h"
#include "include/mt6881_consys_reg_offset.h"
#include "include/mt6881_pos.h"
#include "include/mt6881_soc.h"

#define DURATION_OVER_10MS	10*1000*1000   // ns
int consys_spi_read_mt6881_atf(enum sys_spi_subsystem subsystem, unsigned int addr,
	unsigned int *data)
{
	struct arm_smccc_res res;
	int ret;
	ktime_t start, end;
	u64 duration = 0;

	start = ktime_get();
	arm_smccc_smc(MTK_SIP_KERNEL_CONNSYS_CONTROL, SMC_CONNSYS_SPI_READ_OPID,
		subsystem, addr, 0, 0, 0, 0, &res);
	*data = res.a1;
	ret = res.a0;
	end = ktime_get();
	duration = ktime_to_ns(ktime_sub(end, start));
	if (duration > DURATION_OVER_10MS)
		pr_info("%s execution time: %lld ns\n", __func__, duration);
	return ret;
}

int consys_spi_write_mt6881_atf(enum sys_spi_subsystem subsystem, unsigned int addr,
				unsigned int data)
{
	int ret = 0;
	ktime_t start, end;
	u64 duration = 0;

	start = ktime_get();
	CONNSYS_SMC_CALL_RET(SMC_CONNSYS_SPI_WRITE_OPID, subsystem, addr, data, 0, 0, 0, ret);
	end = ktime_get();
	duration = ktime_to_ns(ktime_sub(end, start));
	if (duration > DURATION_OVER_10MS)
		pr_info("%s execution time: %lld ns\n", __func__, duration);
	return ret;
}

int consys_spi_update_bits_mt6881_atf(enum sys_spi_subsystem subsystem, unsigned int addr,
	unsigned int data, unsigned int mask)
{
	int ret = 0;
	ktime_t start, end;
	u64 duration = 0;

	start = ktime_get();
	CONNSYS_SMC_CALL_RET(SMC_CONNSYS_SPI_UPDATE_BITS_OPID,
			     subsystem, addr, data, mask, 0, 0, ret);
	end = ktime_get();
	duration = ktime_to_ns(ktime_sub(end, start));
	if (duration > DURATION_OVER_10MS)
		pr_info("%s execution time: %lld ns\n", __func__, duration);
	return ret;
}

unsigned int consys_emi_set_remapping_reg_mt6881_atf(
	phys_addr_t con_emi_base_addr,
	phys_addr_t md_shared_emi_base_addr,
	phys_addr_t gps_emi_base_addr)
{
	unsigned int ret = 1;

	CONNSYS_SMC_CALL_RET(SMC_CONNSYS_EMI_SET_REMAPPING_REG_OPID,
			     0, 0, 0, 0, 0, 0, ret);

	return ret;
}

