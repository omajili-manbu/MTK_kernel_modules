/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT8875_SOC_H
#define MT8875_SOC_H

#include <linux/irqreturn.h>

#define PLATFORM_SOC_CHIP_MT8875 0x8875

int consys_co_clock_type_mt8875(void);
int consys_clk_get_from_dts_mt8875(struct platform_device *pdev);
int consys_clock_buffer_ctrl_mt8875(unsigned int enable);
unsigned int consys_soc_chipid_get_mt8875(void);
int consys_platform_spm_conn_ctrl_mt8875(unsigned int enable);
void consys_set_if_pinmux_mt8875(unsigned int enable);
int consys_register_irq_mt8875(struct platform_device *pdev);
void consys_unregister_irq_mt8875(void);
irqreturn_t consys_irq_handler_mt8875(int irq, void* data);

#endif /* MT8875_SOC_H */
