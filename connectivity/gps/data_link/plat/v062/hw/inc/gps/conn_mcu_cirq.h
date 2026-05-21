/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __CONN_MCU_CIRQ_REGS_H__
#define __CONN_MCU_CIRQ_REGS_H__

#define CONN_MCU_CIRQ_BASE                                     0x81028000

#define CONN_MCU_CIRQ_IRQ_SEL0_ADDR                            (CONN_MCU_CIRQ_BASE + 0x000)
#define CONN_MCU_CIRQ_IRQ_SEL1_ADDR                            (CONN_MCU_CIRQ_BASE + 0x004)
#define CONN_MCU_CIRQ_IRQ_SEL2_ADDR                            (CONN_MCU_CIRQ_BASE + 0x008)
#define CONN_MCU_CIRQ_IRQ_SEL3_ADDR                            (CONN_MCU_CIRQ_BASE + 0x00C)
#define CONN_MCU_CIRQ_IRQ_SEL4_ADDR                            (CONN_MCU_CIRQ_BASE + 0x010)
#define CONN_MCU_CIRQ_IRQ_SEL5_ADDR                            (CONN_MCU_CIRQ_BASE + 0x014)
#define CONN_MCU_CIRQ_IRQ_SEL6_ADDR                            (CONN_MCU_CIRQ_BASE + 0x018)
#define CONN_MCU_CIRQ_IRQ_SEL7_ADDR                            (CONN_MCU_CIRQ_BASE + 0x01C)
#define CONN_MCU_CIRQ_FIQ_SEL_ADDR                             (CONN_MCU_CIRQ_BASE + 0x06C)
#define CONN_MCU_CIRQ_IRQ_MASK_ADDR                            (CONN_MCU_CIRQ_BASE + 0x070)
#define CONN_MCU_CIRQ_IRQ_MASK_CLR_ADDR                        (CONN_MCU_CIRQ_BASE + 0x080)
#define CONN_MCU_CIRQ_IRQ_MASK_SET_ADDR                        (CONN_MCU_CIRQ_BASE + 0x090)
#define CONN_MCU_CIRQ_IRQ_EOI_ADDR                             (CONN_MCU_CIRQ_BASE + 0x0A0)
#define CONN_MCU_CIRQ_IRQ_SENS_ADDR                            (CONN_MCU_CIRQ_BASE + 0x0B0)
#define CONN_MCU_CIRQ_IRQ_SOFT_ADDR                            (CONN_MCU_CIRQ_BASE + 0x0C0)
#define CONN_MCU_CIRQ_FIQ_CON_ADDR                             (CONN_MCU_CIRQ_BASE + 0x0D0)
#define CONN_MCU_CIRQ_FIQ_EOI_ADDR                             (CONN_MCU_CIRQ_BASE + 0x0D4)
#define CONN_MCU_CIRQ_IRQ_STA2_ADDR                            (CONN_MCU_CIRQ_BASE + 0x0D8)
#define CONN_MCU_CIRQ_IRQ_EOI2_ADDR                            (CONN_MCU_CIRQ_BASE + 0x0DC)
#define CONN_MCU_CIRQ_IRQ_ASTA_ADDR                            (CONN_MCU_CIRQ_BASE + 0x0E0)
#define CONN_MCU_CIRQ_IRQ_SRC_ADDR                             (CONN_MCU_CIRQ_BASE + 0x400)
#define CONN_MCU_CIRQ_IRQ_DIS_ADDR                             (CONN_MCU_CIRQ_BASE + 0x40C)

#endif /* __CONN_MCU_CIRQ_REGS_H__*/

