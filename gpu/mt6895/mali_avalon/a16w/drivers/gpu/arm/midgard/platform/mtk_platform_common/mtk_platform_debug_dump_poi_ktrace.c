// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2022 MediaTek Inc.
*/

#include <linux/time.h>
#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#if IS_ENABLED(CONFIG_MALI_MTK_LOG_BUFFER)
#include "mtk_platform_logbuffer.h"
#endif /* CONFIG_MALI_MTK_LOG_BUFFER */
#include <platform/mtk_platform_common.h>

/*
 * Porting guide
 * 1. Add the POI Ktrace code to the variable poi_array[].
 * 2. Add the dump in the function mtk_debug_dump_poi_ktrace_entry().
 *    Remember to use the union member according to the ktrace add api.
 *    | Code               | Add API                  | Union Member
 *----------------------------------------------------------------------
 *    | SCHEDULER_TOP_GRP  | KBASE_KTRACE_ADD_CSF_GRP   | csf_group
 *    | CSG_SLOT_START_REQ | KBASE_KTRACE_ADD_CSF_GRP   | csf_group
 *    |                    | KBASE_KTRACE_ADD_CSF_GRP_Q | csf_group
 *    |                    | KBASE_KTRACE_ADD_CSF_KCPU  | csf_kcpu
 *    |                    | KBASE_KTRACE_ADD           | basic
 */

#if IS_ENABLED(CONFIG_MALI_MTK_POI_KTRACE)
#define MAX_POI_KTRACE_RECORDINGS 100

static const char *const kbasep_ktrace_code_string[] = {
	/*
	 * IMPORTANT: USE OF SPECIAL #INCLUDE OF NON-STANDARD HEADER FILE
	 * THIS MUST BE USED AT THE START OF THE ARRAY
	 */
#define KBASE_KTRACE_CODE_MAKE_CODE(X) #X
#include "debug/mali_kbase_debug_ktrace_codes.h"
#undef KBASE_KTRACE_CODE_MAKE_CODE
};

static int poi_array[] = {
	KBASE_KTRACE_CODE(SCHEDULER_TOP_GRP),
	KBASE_KTRACE_CODE(CSG_SLOT_START_REQ)
};

static struct mtk_common_poi_ktrace_data recordings[MAX_POI_KTRACE_RECORDINGS];
static DEFINE_SPINLOCK(recording_lock);
static int recording_ptr = 0;
static DECLARE_BITMAP(poi_bitmap, KBASE_KTRACE_CODE_COUNT);

struct mtk_common_poi_ktrace_data *mtk_poi_ktrace_get_recording_entry(enum kbase_ktrace_code code)
{
	unsigned long flags;
	u32 current_ptr = 0;

	/* Check if code in POI bitmap */
	if (!test_bit(code, poi_bitmap))
		return NULL;

	/* Calculate the current_ptr and move recording_ptr to next */
	spin_lock_irqsave(&recording_lock, flags);
	current_ptr = recording_ptr;
	recording_ptr = (recording_ptr + 1) % MAX_POI_KTRACE_RECORDINGS;
	spin_unlock_irqrestore(&recording_lock, flags);

	return (struct mtk_common_poi_ktrace_data *)&recordings[current_ptr];
}

void mtk_debug_dump_poi_ktrace_entry(struct kbase_device *kbdev,
	struct mtk_common_poi_ktrace_data recording_data)
{
	if (recording_data.code == KBASE_KTRACE_CODE(SCHEDULER_TOP_GRP) ||
		recording_data.code == KBASE_KTRACE_CODE(CSG_SLOT_START_REQ)) {
		dev_info(kbdev->dev, "%6d.%.6d, kctx %5d_%-4d, %s, group %d, slot %d, prio %d, val 0x%llx",
			(int)recording_data.timestamp.tv_sec,
			(int)(recording_data.timestamp.tv_nsec / 1000),
			recording_data.kctx_tgid,
			recording_data.kctx_id,
			kbasep_ktrace_code_string[recording_data.code],
			recording_data.u.csf_group.group_handle,
			recording_data.u.csf_group.csg_slot,
			recording_data.u.csf_group.csg_slot_prio,
			recording_data.u.csf_group.info_val1
		);
#if IS_ENABLED(CONFIG_MALI_MTK_LOG_BUFFER)
		mtk_logbuffer_type_print(kbdev, MTK_LOGBUFFER_TYPE_CRITICAL | MTK_LOGBUFFER_TYPE_EXCEPTION,
			"%6d.%.6d, kctx %5d_%-4d, %s, group %d, slot %d, prio %d, val 0x%llx\n",
			(int)recording_data.timestamp.tv_sec,
			(int)(recording_data.timestamp.tv_nsec / 1000),
			recording_data.kctx_tgid,
			recording_data.kctx_id,
			kbasep_ktrace_code_string[recording_data.code],
			recording_data.u.csf_group.group_handle,
			recording_data.u.csf_group.csg_slot,
			recording_data.u.csf_group.csg_slot_prio,
			recording_data.u.csf_group.info_val1
		);
#endif /* CONFIG_MALI_MTK_LOG_BUFFER */
	}
}

void mtk_debug_dump_poi_ktrace(struct kbase_device *kbdev)
{
	unsigned long flags;
	int i;

	spin_lock_irqsave(&recording_lock, flags);
	/* Dump from the oldest entry to the end of ring buffer */
	for (i = recording_ptr; i < MAX_POI_KTRACE_RECORDINGS; i++) {
		mtk_debug_dump_poi_ktrace_entry(kbdev, recordings[i]);
	}

	/* Dump from the start to the newest entry of ring buffer */
	for (i = 0; i < recording_ptr; i++) {
		mtk_debug_dump_poi_ktrace_entry(kbdev, recordings[i]);
	}
	spin_unlock_irqrestore(&recording_lock, flags);
}

void mtk_debug_dump_poi_ktrace_init(void)
{
	int i;

	bitmap_zero(poi_bitmap, KBASE_KTRACE_CODE_COUNT);

	for (i = 0; i < ARRAY_SIZE(poi_array); i++) {
		set_bit(poi_array[i], poi_bitmap);
	}
}
#endif /* CONFIG_MALI_MTK_POI_KTRACE */
