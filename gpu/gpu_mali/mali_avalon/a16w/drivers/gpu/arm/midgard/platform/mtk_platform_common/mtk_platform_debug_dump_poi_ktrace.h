// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_PLATFORM_DEBUG_DUMP_POI_KTRACE_H__
#define __MTK_PLATFORM_DEBUG_DUMP_POI_KTRACE_H__

struct mtk_common_poi_ktrace_data *mtk_poi_ktrace_get_recording_entry(enum kbase_ktrace_code code);
void mtk_debug_dump_poi_ktrace(struct kbase_device *kbdev);
void mtk_debug_dump_poi_ktrace_init(void);

#endif /* __MTK_PLATFORM_DEBUG_DUMP_POI_KTRACE_H__ */