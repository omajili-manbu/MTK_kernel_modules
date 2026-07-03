/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#if (CFG_SUPPORT_NAN == 1)

#include "precomp.h"
#include "typedef.h"
#include "nanReg.h"
#include "nanRescheduler.h"
#include "rlm_domain.h"

struct _TXM_CMD_EVENT_TEST_T {
	uint32_t u4TestValue0;
	uint32_t u4TestValue1;
	uint8_t ucTestValue2;
	uint8_t aucReserved[3]; /*For 4 bytes alignment*/
};

#define NAN_STATION_TEST_ADDRESS                                               \
	{ 0x22, 0x22, 0x22, 0x22, 0x22, 0x22 }

/* [0] ChnlRaw:0x247301, PriChnl:36
 * [Map], len:64
 * 0x02057668: 00 0e 00 00 00 0e 00 00 00 0e 00 00 00 0e 00 00
 * 0x02057678: 00 0e 00 00 00 0e 00 00 00 0e 00 00 00 0e 00 00
 * 0x02057688: 00 0e 00 00 00 0e 00 00 00 0e 00 00 00 0e 00 00
 * 0x02057698: 00 0e 00 00 00 0e 00 00 00 0e 00 00 00 0e 00 00
 *
 * [1] ChnlRaw:0xa17c01, PriChnl:161
 * [Map], len:64
 * 0x020576c4: 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00
 * 0x020576d4: 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00
 * 0x020576e4: 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00
 * 0x020576f4: 00 20 00 00 00 20 00 00 00 20 00 00 00 20 00 00
 */
uint8_t g_aucPeerAvailabilityAttr[] = { 0x12, 0x1D, 0x0,  0x0,  0x1, 0x0, 0xb,
				       0x0,  0x1,  0x12, 0x58, 0x2, 0x1, 0x7,
				       0x11, 0x73, 0x1,  0x0,  0x0, 0xb, 0x0,
				       0x1,  0x12, 0x58, 0x3,  0x1, 0x1, 0x11,
				       0x7c, 0x8,  0x0,  0x0 };
uint8_t g_aucPeerAvailabilityAttr2[] = { 0x12, 0xc,  0x0,  0x1,  0x21,
					0x0,  0x7,  0x0,  0x1a, 0x0,
					0x11, 0x51, 0xff, 0x7,  0x0 };

/* commit Chnl:6
 * AvailMap, len:64
 * ffffffffc0c9158c: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * ffffffffc0c9159c: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * ffffffffc0c915ac: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * ffffffffc0c915bc: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * Commit Chnl:149
 * AvailMap, len:64
 * ffffffffc0c915d8: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * ffffffffc0c915e8: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * ffffffffc0c915f8: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * ffffffffc0c91608: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 */
uint8_t g_aucPeerAvailabilityAttr3[] = {
	0x12, 0x2b, 0x0,  0x01, 0x11, 0x0,  0xb,  0x0,  0x1,  0x12, 0x18, 0x0,
	0x1,  0x1,  0x11, 0x51, 0x20, 0x0,  0x0,  0xb,  0x0,  0x1,  0x12, 0x18,
	0x2,  0x1,  0x1,  0x11, 0x7c, 0x01, 0x0,  0x0,  0xc,  0x0,  0x2,  0x12,
	0x58, 0x0,  0x4,  0x7f, 0xff, 0xff, 0x7f, 0x20, 0x02, 0x04
};

/* [0], Commit Chnl:6
 * AvailMap, len:64
 * ffffffffc0c915cc: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * ffffffffc0c915dc: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * ffffffffc0c915ec: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * ffffffffc0c915fc: 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00 00
 * [1], Commit Chnl:149
 * AvailMap, len:64
 * ffffffffc0c91618: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * ffffffffc0c91628: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * ffffffffc0c91638: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * ffffffffc0c91648: 00 01 00 00 00 01 00 00 00 01 00 00 00 01 00 00
 * [0], Cond Chnl:149
 * AvailMap, len:64
 * ffffffffc0c918c8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 * ffffffffc0c918d8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 * ffffffffc0c918e8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 * ffffffffc0c918f8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 */
uint8_t g_aucPeerAvailabilityAttr4[] = {
	0x12, 0x38, 0x00, 0x01, 0x11, 0x00, 0x0b, 0x00, 0x01, 0x12, 0x18, 0x00,
	0x01, 0x01, 0x11, 0x51, 0x20, 0x00, 0x00, 0x0b, 0x00, 0x01, 0x12, 0x18,
	0x02, 0x01, 0x01, 0x11, 0x7c, 0x01, 0x00, 0x00, 0x0b, 0x00, 0x04, 0x12,
	0x58, 0x00, 0x01, 0x01, 0x11, 0x7c, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x02,
	0x12, 0x58, 0x00, 0x04, 0x7f, 0xff, 0xff, 0x7f, 0x20, 0x02, 0x04
};

uint8_t g_aucPeerAvailabilityAttr5[] = {
	0x12, 0x2f, 0x00, 0x01, 0x11, 0x00,

	0x0e, 0x00, 0x01, 0x12, 0x18, 0x00, 0x04, 0x01, 0x00, 0x00,
	0x00, 0x11, 0x7c, 0x01, 0x00, 0x00,

	0x1a, 0x00, 0x1a, 0x12, 0x18, 0x00, 0x04, 0xff, 0xfe, 0xff,
	0xff, 0x41, 0x7e, 0x03, 0x00, 0x00, 0x7f, 0x03, 0x00, 0x00,
	0x73, 0x0f, 0x00, 0x00, 0x51, 0xff, 0x07, 0x00
};

uint8_t g_aucRangSchEntryList1[] = { 0x1, 0x18, 0x0, 0x4, 0x1, 0x0, 0x0, 0x0 };

uint8_t g_aucPeerSelectedNdcAttr[] = { 0x13, 0xc, 0x0, 0x1,  0x2, 0x3, 0x4, 0x5,
				      0x6,  0x1, 0x1, 0x58, 0x3, 0x1, 0x1 };

/* ffffffffc0c918c8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 * ffffffffc0c918d8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 * ffffffffc0c918e8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 * ffffffffc0c918f8: 02 00 00 00 02 00 00 00 02 00 00 00 02 00 00 00
 */
uint8_t g_aucRangSchEntryList[] = { 0x1, 0x58, 0x0, 0x1, 0x1 };
uint8_t g_aucTestData[100];

#ifdef NAN_UNUSED
UINT_8 g_aucCase_5_3_3_DataReq_AvailAttr[] = {
	0x12, 0x20, 0x00, 0x00, 0x00, 0x00,
	0x0e, 0x00, 0x0c, 0x11, 0x18, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x00, 0x11, 0x51,
	0x20, 0x00, 0x00, 0x0b, 0x00, 0x0a, 0x11,
	0x18, 0x00, 0x04, 0x7e, 0xfe, 0xff,
	0x7f, 0x10, 0x02};
#else
uint8_t g_aucCase_5_3_3_DataReq_AvailAttr[] = {
	0x12, 0x20, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x0c, 0x11, 0x18, 0x00,
	0x04, 0x7e, 0xfe, 0xff, 0x0f, 0x11, 0x51, 0x20, 0x00, 0x00, 0x0b, 0x00,
	0x0a, 0x11, 0x18, 0x00, 0x04, 0x7e, 0xfe, 0xff, 0x7f, 0x10, 0x02
};
#endif
uint8_t g_aucCase_5_3_3_DataReq_NdcAttr[] = {
	0x13, 0x0f, 0x00, 0x50, 0x6f, 0x9a, 0x01, 0x00, 0x00,
	0x01, 0x00, 0x18, 0x00, 0x04, 0x02, 0x00, 0x00, 0x00
};

uint8_t g_aucCase_5_3_3_DataReq_ImmNdl[] = { 0x00, 0x18, 0x00, 0x04,
					    0x00, 0x00, 0x01, 0x00 };

uint8_t g_aucCase_5_3_1_Publish_AvailAttr[] = { 0x12, 0x0c, 0x00, 0x01, 0x20,
					       0x00, 0x07, 0x00, 0x1a, 0x00,
					       0x11, 0x51, 0xff, 0x07, 0x00 };

uint8_t g_aucCase_5_3_1_Publish_AvailAttr2[] = {
	0x12, 0x23, 0x00, 0x07, 0x00, 0x00, 0x0e, 0x00, 0x1a, 0x10,
	0x18, 0x00, 0x04, 0x00, 0x00, 0x80, 0xff, 0x11, 0x51, 0xff,
	0x07, 0x00, 0x0e, 0x00, 0x02, 0x10, 0x18, 0x00, 0x04, 0xfe,
	0xff, 0x7f, 0x00, 0x11, 0x51, 0x20, 0x00, 0x00
};

uint8_t g_aucCase_5_3_1_DataRsp_AvailAttr[] = {
	0x12, 0x23, 0x00, 0x08, 0xb0, 0x00, 0x0e, 0x00, 0x1a, 0x10,
	0x18, 0x00, 0x04, 0x00, 0x00, 0x80, 0xff, 0x11, 0x51, 0xff,
	0x07, 0x00, 0x0e, 0x00, 0x04, 0x10, 0x18, 0x00, 0x04, 0xfe,
	0xff, 0x7f, 0x00, 0x11, 0x51, 0x20, 0x00, 0x00
};

uint8_t g_aucCase_5_3_11_DataReq_Avail1Attr[] = {
	0x12, 0x31, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x0c, 0x11, 0x18,
	0x00, 0x04, 0x7e, 0x00, 0x00, 0x00, 0x11, 0x51, 0x20, 0x00, 0x00,
	0x0e, 0x00, 0x0c, 0x11, 0x18, 0x00, 0x04, 0x00, 0xfe, 0xff, 0x7f,
	0x11, 0x7d, 0x01, 0x00, 0x00, 0x0c, 0x00, 0x0a, 0x11, 0x18, 0x00,
	0x04, 0x7e, 0xfe, 0xff, 0x7f, 0x20, 0x02, 0x04
};

uint8_t g_aucCase_5_3_11_DataReq_Avail2Attr[] = {
	0x12, 0x31, 0x00, 0x00, 0x01, 0x00, 0x0e, 0x00, 0x0c, 0x11, 0x18,
	0x00, 0x04, 0x7e, 0x00, 0x00, 0x00, 0x11, 0x7d, 0x01, 0x00, 0x00,
	0x0e, 0x00, 0x0c, 0x11, 0x18, 0x00, 0x04, 0x00, 0xfe, 0xff, 0x7f,
	0x11, 0x51, 0x20, 0x00, 0x00, 0x0c, 0x00, 0x0a, 0x11, 0x18, 0x00,
	0x04, 0x7e, 0xfe, 0xff, 0x7f, 0x20, 0x02, 0x04
};

uint8_t g_aucCase_5_3_11_DataReq_NdcAttr[] = { 0x13, 0x0f, 0x00, 0x50, 0x6f,
					      0x9a, 0x01, 0x00, 0x00, 0x01,
					      0x00, 0x18, 0x00, 0x04, 0x00,
					      0x02, 0x00, 0x00 };

struct _TXM_CMD_EVENT_TEST_T grCmdInfoQueryTestBuffer;

void nicNanGetCmdInfoQueryTestBuffer(
	struct _TXM_CMD_EVENT_TEST_T **prCmdInfoQueryTestBuffer)
{
	*prCmdInfoQueryTestBuffer =
		(struct _TXM_CMD_EVENT_TEST_T *)&grCmdInfoQueryTestBuffer;
}

void
nanScheduleNegoTestFunc(struct ADAPTER *prAdapter, uint8_t *pucNmiAddr,
			enum _ENUM_NAN_NEGO_TYPE_T eType,
			enum _ENUM_NAN_NEGO_ROLE_T eRole, void *pvToken)
{
	uint8_t *pucBuf = NULL;
	uint32_t u4Length = 0;
	uint32_t rRetStatus = 0;
	uint32_t u4RejectCode = 0;
	uint8_t aucNmiAddr[] = NAN_STATION_TEST_ADDRESS;
	uint8_t aucTestData[100];

	DBGLOG(NAN, TRACE, "IN\n");

	if (pvToken == (void *)5) {
		nanSchedNegoAddNdcCrb(prAdapter, &g_r5gDwChnl, 16, 1,
				      ENUM_TIME_BITMAP_CTRL_PERIOD_512);
		nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[NDC Attr]", pucBuf, u4Length);

		nanSchedNegoStop(prAdapter);
	} else if (pvToken == (void *)7) {
		nanSchedNegoGenLocalCrbProposal(prAdapter);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStop(prAdapter);
	} else if (pvToken == (void *)8) {
		nanSchedNegoAddQos(prAdapter, 10, 3);
		nanSchedNegoGenLocalCrbProposal(prAdapter);
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);

		nanSchedNegoStop(prAdapter);
	} else if (pvToken == (void *)9) {
		nanSchedNegoAddNdcCrb(prAdapter, &g_r2gDwChnl, 14, 1,
				      ENUM_TIME_BITMAP_CTRL_PERIOD_256);
		nanSchedNegoGenLocalCrbProposal(prAdapter);
		nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Selected NDC]", pucBuf, u4Length);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)10) {
		/* nanSchedNegoAddQos(prAdapter, 10, 3); */
		rRetStatus =
			nanSchedNegoChkRmtCrbProposal(prAdapter, &u4RejectCode);
		DBGLOG(NAN, DEBUG, "nanSchedNegoChkRmtCrbProposal: %x, %u\n",
		       rRetStatus, u4RejectCode);
		nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[NDC Attr]", pucBuf, u4Length);
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStop(prAdapter);
	} else if (pvToken == (void *)11) {
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)14) {

		rRetStatus = nanSchedNegoGenLocalCrbProposal(prAdapter);
		DBGLOG(NAN, DEBUG, "nanSchedNegoGenLocalCrbProposal: %x\n",
		       rRetStatus);
		nanSchedNegoGetRangingScheduleList(prAdapter, &pucBuf,
						   &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Ranging]", pucBuf, u4Length);

		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);
		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)15) {
		rRetStatus =
			nanSchedNegoChkRmtCrbProposal(prAdapter, &u4RejectCode);
		DBGLOG(NAN, DEBUG, "nanSchedNegoChkRmtCrbProposal: %x, %u\n",
		       rRetStatus, u4RejectCode);
		nanSchedNegoGetRangingScheduleList(prAdapter, &pucBuf,
						   &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Ranging]", pucBuf, u4Length);
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)18) {

		rRetStatus =
			nanSchedNegoChkRmtCrbProposal(prAdapter, &u4RejectCode);
		DBGLOG(NAN, DEBUG, "nanSchedNegoChkRmtCrbProposal: %x, %u\n",
		       rRetStatus, u4RejectCode);

		DBGLOG(NAN, DEBUG, "DUMP#5\n");
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);

		nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[NDC Attr]", pucBuf, u4Length);

		nanSchedNegoGetImmuNdlScheduleList(prAdapter, &pucBuf,
						   &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Imm NDL Attr]", pucBuf, u4Length);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)19) {
		rRetStatus = nanSchedNegoGenLocalCrbProposal(prAdapter);
		DBGLOG(NAN, DEBUG, "nanSchedNegoGenLocalCrbProposal: %x\n",
		       rRetStatus);

		DBGLOG(NAN, DEBUG, "DUMP#2\n");
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)20) {

		rRetStatus = nanSchedNegoGenLocalCrbProposal(prAdapter);
		DBGLOG(NAN, DEBUG, "nanSchedNegoGenLocalCrbProposal: %x\n",
		       rRetStatus);

		DBGLOG(NAN, DEBUG, "DUMP#4\n");
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);

		nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[NDC Attr]", pucBuf, u4Length);

		nanSchedNegoStop(prAdapter);

	} else if (pvToken == (void *)23) {
		rRetStatus = nanSchedNegoGenLocalCrbProposal(prAdapter);
		DBGLOG(NAN, DEBUG, "nanSchedNegoGenLocalCrbProposal: %x\n",
		       rRetStatus);

		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);

		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr5,
			   sizeof(g_aucPeerAvailabilityAttr5));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		kalMemCopy(aucTestData, g_aucRangSchEntryList1,
			   sizeof(g_aucRangSchEntryList1));
		nanSchedPeerUpdateRangingScheduleList(
			prAdapter, aucNmiAddr,
			(struct _NAN_SCHEDULE_ENTRY_T *)aucTestData,
			sizeof(g_aucRangSchEntryList1));

		rRetStatus =
			nanSchedNegoChkRmtCrbProposal(prAdapter, &u4RejectCode);
		DBGLOG(NAN, DEBUG, "nanSchedNegoChkRmtCrbProposal: %x, %u\n",
		       rRetStatus, u4RejectCode);

		nanSchedNegoStop(prAdapter);
	} else if (pvToken == (void *)24) {
		rRetStatus =
			nanSchedNegoChkRmtCrbProposal(prAdapter, &u4RejectCode);
		DBGLOG(NAN, DEBUG, "nanSchedNegoChkRmtCrbProposal: %x, %u\n",
		       rRetStatus, u4RejectCode);

		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[AVAIL ATTR]", pucBuf, u4Length);

		nanSchedNegoGetSelectedNdcAttr(prAdapter, &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[NDC ATTR]", pucBuf, u4Length);

		nanSchedNegoStop(prAdapter);
	}
}

uint32_t
nanSchedSwDbg4(struct ADAPTER *prAdapter, uint32_t u4Data) /* 0x7426000d */
{
	uint32_t u4Ret = 0;
	uint8_t *pucBuf = NULL;
	uint32_t u4Length = 0;
	uint8_t aucNmiAddr[] = NAN_STATION_TEST_ADDRESS;
	uint32_t au4Map[NAN_TOTAL_DW];
	uint32_t u4Idx = 0;
	uint8_t aucTestData[100];
	uint32_t rRetStatus = 0;
	struct _NAN_SCHEDULER_T *prNanScheduler;
	uint32_t u4TestDataLen;
	union _NAN_BAND_CHNL_CTRL rChnlInfo = {
		.u4Type = NAN_BAND_CH_ENTRY_LIST_TYPE_CHNL,
		.u4OperatingClass = NAN_5G_LOW_DISC_CH_OP_CLASS,
		.u4PrimaryChnl = 36,
		.u4AuxCenterChnl = 0
	};
	uint8_t aucBitmap[4];

	prNanScheduler = nanGetScheduler(prAdapter);

	if (prNanScheduler->fgInit == FALSE)
		nanSchedInit(prAdapter);

	switch (u4Data) {
	case 0:
#ifdef NAN_UNUSED
		nanSchedConfigAllowedBand(prAdapter, TRUE, TRUE, TRUE, TRUE);
#else
		nanSchedConfigAllowedBand(prAdapter, TRUE, FALSE, FALSE, FALSE);
#endif
		nanSchedConfigDefNdlNumSlots(prAdapter, 3);
		nanSchedConfigDefRangingNumSlots(prAdapter, 1);
		break;

	case 1:
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);
		break;

	case 2:
		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);
		break;

	case 3:
		kalMemZero(au4Map, sizeof(au4Map));
		for (u4Idx = 0; u4Idx < NAN_TOTAL_DW; u4Idx++) {
			au4Map[u4Idx] |= (BIT(NAN_5G_DW_INDEX + 1));
			au4Map[u4Idx] |= (BIT(NAN_5G_DW_INDEX + 2));
			au4Map[u4Idx] |= (BIT(NAN_5G_DW_INDEX + 3));
			au4Map[u4Idx] |= (BIT(NAN_5G_DW_INDEX + 4));
		}

		DBGDUMP_HEX(NAN, INFO, "[Map]", au4Map, sizeof(au4Map));
		nanParserGenTimeBitmapField(prAdapter, au4Map, g_aucNanIEBuffer,
					    &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[TimeBitmap]", g_aucNanIEBuffer,
			    u4Length);
		break;

	case 4:
		kalMemZero(au4Map, sizeof(au4Map));
		for (u4Idx = 0; u4Idx < NAN_TOTAL_DW; u4Idx++)
			au4Map[u4Idx] |= (BIT(NAN_5G_DW_INDEX + 5));

		DBGDUMP_HEX(NAN, INFO, "[Map]", au4Map, sizeof(au4Map));
		nanParserGenTimeBitmapField(prAdapter, au4Map, g_aucNanIEBuffer,
					    &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[TimeBitmap]", g_aucNanIEBuffer,
			    u4Length);
		break;

	case 5:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr,
			   sizeof(g_aucPeerAvailabilityAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)5);
		break;

	case 6:
		rChnlInfo.u4OperatingClass = NAN_5G_LOW_DISC_CH_OP_CLASS;
		rChnlInfo.u4PrimaryChnl = 36;
		rRetStatus = nanSchedAddCrbToChnlList(prAdapter, &rChnlInfo,
				9, 2, ENUM_TIME_BITMAP_CTRL_PERIOD_512,
				TRUE, NULL);

		rChnlInfo.u4OperatingClass = NAN_5G_HIGH_DISC_CH_OP_CLASS;
		rChnlInfo.u4PrimaryChnl = 161;
		rRetStatus = nanSchedAddCrbToChnlList(prAdapter, &rChnlInfo,
				13, 1, ENUM_TIME_BITMAP_CTRL_PERIOD_512,
				TRUE, NULL);
		break;

	case 7:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr,
			   sizeof(g_aucPeerAvailabilityAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)7);
		break;

	case 8:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr,
			   sizeof(g_aucPeerAvailabilityAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)8);

		break;

	case 9:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr,
			   sizeof(g_aucPeerAvailabilityAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)9);
		break;

	case 10:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr,
			   sizeof(g_aucPeerAvailabilityAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_RESPONDER,
				  nanScheduleNegoTestFunc, (void *)10);
		break;

	case 11:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr2,
			   sizeof(g_aucPeerAvailabilityAttr2));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_RESPONDER,
				  nanScheduleNegoTestFunc, (void *)11);

		break;

	case 12:
		break;

	case 13:
		prNanScheduler->fgInit = FALSE;
		break;

	case 14:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr3,
			   sizeof(g_aucPeerAvailabilityAttr3));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		nanSchedNegoStart(prAdapter, aucNmiAddr, ENUM_NAN_NEGO_RANGING,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)14);
		break;

	case 15:
		kalMemCopy(aucTestData, g_aucRangSchEntryList,
			   sizeof(g_aucRangSchEntryList));
		u4TestDataLen = sizeof(g_aucRangSchEntryList);
		nanSchedPeerUpdateRangingScheduleList(
			prAdapter, aucNmiAddr,
			(struct _NAN_SCHEDULE_ENTRY_T *)aucTestData,
			u4TestDataLen);

		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr4,
			   sizeof(g_aucPeerAvailabilityAttr4));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);
		nanSchedNegoStart(prAdapter, aucNmiAddr, ENUM_NAN_NEGO_RANGING,
				  ENUM_NAN_NEGO_ROLE_RESPONDER,
				  nanScheduleNegoTestFunc, (void *)15);

		break;

	case 16:
		nanSchedInit(prAdapter);
		break;

	case 17:
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);
		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);
		break;

	case 18:
		DBGLOG(NAN, DEBUG, "DUMP#1\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		kalMemCopy(aucTestData, g_aucCase_5_3_3_DataReq_AvailAttr,
			   sizeof(g_aucCase_5_3_3_DataReq_AvailAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		DBGLOG(NAN, DEBUG, "DUMP#2\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		kalMemCopy(aucTestData, g_aucCase_5_3_3_DataReq_NdcAttr,
			   sizeof(g_aucCase_5_3_3_DataReq_NdcAttr));
		nanSchedPeerUpdateNdcAttr(prAdapter, aucNmiAddr, aucTestData);

		nanSchedPeerUpdateImmuNdlScheduleList(
			prAdapter, aucNmiAddr,
			(struct _NAN_SCHEDULE_ENTRY_T *)
				g_aucCase_5_3_3_DataReq_ImmNdl,
			sizeof(g_aucCase_5_3_3_DataReq_ImmNdl));

		DBGLOG(NAN, DEBUG, "DUMP#3\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		DBGLOG(NAN, DEBUG, "DUMP#4\n");
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_RESPONDER,
				  nanScheduleNegoTestFunc, (void *)18);
		break;

	case 19:
		DBGLOG(NAN, DEBUG, "DUMP#1\n");
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)19);

		break;

	case 20:
		DBGLOG(NAN, DEBUG, "DUMP#1\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		kalMemCopy(aucTestData, g_aucCase_5_3_1_Publish_AvailAttr,
			   sizeof(g_aucCase_5_3_1_Publish_AvailAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		DBGLOG(NAN, DEBUG, "DUMP#2\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		DBGLOG(NAN, DEBUG, "DUMP#3\n");
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)20);
		break;

	case 21:
		DBGLOG(NAN, DEBUG, "DUMP#1\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		kalMemCopy(aucTestData, g_aucCase_5_3_1_DataRsp_AvailAttr,
			   sizeof(g_aucCase_5_3_1_DataRsp_AvailAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		DBGLOG(NAN, DEBUG, "DUMP#2\n");
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);
		break;

	case 22:
		nanSchedDbgDumpTimelineDb(prAdapter, __func__, __LINE__);
		nanSchedGetAvailabilityAttr(prAdapter, NULL,
					    &pucBuf, &u4Length);
		DBGDUMP_HEX(NAN, INFO, "[Availability Attr]", pucBuf, u4Length);
		break;

	case 23:
		kalMemCopy(aucTestData, g_aucPeerAvailabilityAttr,
			   sizeof(g_aucPeerAvailabilityAttr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		nanSchedNegoStart(prAdapter, aucNmiAddr, ENUM_NAN_NEGO_RANGING,
				  ENUM_NAN_NEGO_ROLE_INITIATOR,
				  nanScheduleNegoTestFunc, (void *)23);
		break;

	case 24:
		kalMemCopy(aucTestData, g_aucCase_5_3_11_DataReq_Avail1Attr,
			   sizeof(g_aucCase_5_3_11_DataReq_Avail1Attr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);

		kalMemCopy(aucTestData, g_aucCase_5_3_11_DataReq_Avail2Attr,
			   sizeof(g_aucCase_5_3_11_DataReq_Avail2Attr));
		nanSchedPeerUpdateAvailabilityAttr(prAdapter, 0, aucNmiAddr,
						   aucTestData, NULL);
		nanSchedDbgDumpPeerAvailability(prAdapter, aucNmiAddr);

		kalMemCopy(aucTestData, g_aucCase_5_3_11_DataReq_NdcAttr,
			   sizeof(g_aucCase_5_3_11_DataReq_NdcAttr));
		nanSchedPeerUpdateNdcAttr(prAdapter, aucNmiAddr, aucTestData);

		nanSchedNegoStart(prAdapter, aucNmiAddr,
				  ENUM_NAN_NEGO_DATA_LINK,
				  ENUM_NAN_NEGO_ROLE_RESPONDER,
				  nanScheduleNegoTestFunc, (void *)24);
		break;

	case 25:
		halPrintHifDbgInfo(prAdapter);
		break;

	case 26:
		aucBitmap[0] = 0xaa;
		aucBitmap[1] = aucBitmap[2] = aucBitmap[3] = 0;
		kalMemZero(aucTestData, sizeof(aucTestData));
		nanParserInterpretTimeBitmapField(
		    prAdapter, 0x0008, 4, aucBitmap, (uint32_t *)aucTestData);
		DBGDUMP_HEX(NAN, INFO, "time map", aucTestData, 64);
		break;

	default:
		break;
	}

	return u4Ret;
}

#endif /* CFG_SUPPORT_NAN */
