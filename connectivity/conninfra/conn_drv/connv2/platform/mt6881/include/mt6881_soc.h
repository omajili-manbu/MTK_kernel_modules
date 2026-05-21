/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6881_SOC_H
#define MT6881_SOC_H

#define PLATFORM_SOC_CHIP 0x6881

int consys_get_co_clock_type_mt6881(void);
int consys_clk_get_from_dts_mt6881(struct platform_device *pdev);
int consys_clk_get_from_dts_mt6881_6686(struct platform_device *pdev);
unsigned int consys_soc_chipid_get_mt6881(void);
void consys_clock_fail_dump_mt6881(void);
unsigned long long consys_soc_timestamp_get_mt6881(void);
int consys_conninfra_on_power_ctrl_mt6881(unsigned int enable);
void consys_set_if_pinmux_mt6881(unsigned int enable, unsigned int curr_status, unsigned int next_status);
void consys_set_if_pinmux_mt6881_6686(unsigned int enable, unsigned int curr_status, unsigned int next_status);
int consys_is_consys_reg_mt6881(unsigned int addr);
void consys_print_platform_debug_mt6881(void);
int consys_reg_init_mt6881(struct platform_device *pdev);
int consys_reg_deinit_mt6881(void);
int consys_register_irq_mt6881(struct platform_device *pdev);
void consys_unregister_irq_mt6881(void);
irqreturn_t consys_irq_handler_mt6881(int irq, void* data);

#endif /* MT6881_SOC_H */

