/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6881_ATF_H
#define MT6881_ATF_H

#include "conninfra.h"

int consys_spi_read_mt6881_atf(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int *data);
int consys_spi_write_mt6881_atf(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data);
int consys_spi_update_bits_mt6881_atf(enum sys_spi_subsystem subsystem, unsigned int addr, unsigned int data, unsigned int mask);

#endif /* MT6881_ATF_H */
