/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef MT6881_EMI_H
#define MT6881_EMI_H

int consys_emi_mpu_set_region_protection_mt6881(void);
void consys_emi_get_md_shared_emi_mt6881(phys_addr_t* base, unsigned int* size);

#endif /* MT6881_EMI_H */
