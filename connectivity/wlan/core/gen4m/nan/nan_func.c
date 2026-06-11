// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "nan_func.c"
 *  \brief
 */

#if (CFG_SUPPORT_NAN == 1)

#include "precomp.h"
#include "nan/nan_func.h"
#include "nan_sec.h"
#include "nanScheduler.h"

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

void __weak
nanSetFlashCommunication(struct ADAPTER *prAdapter, u_int8_t fgEnable)
{
	struct _NAN_SCHEDULER_T *prScheduler = nanGetScheduler(prAdapter);

	DBGLOG(NAN, TRACE, "Set Flash Communication %u\n", fgEnable);
	/* TODO:
	 * 1. Set 2.4GHz/5GHz bitmap respectively
	 * 2. Call reconfigure function to update customized timeline
	 * 3. Send command to FW to set the bitmap, reuse Instant communication?
	 */
	prScheduler->fgFlashCommunication = fgEnable;

}

u_int8_t __weak
nanGetFlashCommunication(struct ADAPTER *prAdapter)
{
	struct _NAN_SCHEDULER_T *prScheduler = nanGetScheduler(prAdapter);

	DBGLOG(NAN, TRACE, "Get Flash Communication %u\n",
	       prScheduler->fgFlashCommunication);

	return prScheduler->fgFlashCommunication;
}

void __weak
nanExtEnableReq(struct ADAPTER *prAdapter)
{
	nanSetFlashCommunication(prAdapter, TRUE);
	nanInstantCommModeOnHandler(prAdapter);
}

void __weak
nanExtDisableReq(struct ADAPTER *prAdapter)
{
}

void __weak
nanExtClearCustomNdpFaw(uint8_t ucIndex)
{
}

void __weak
nanPeerReportEhtEvent(struct ADAPTER *prAdapter,
					 uint8_t enable)
{
}

void __weak
nanEnableEhtMode(struct ADAPTER *prAdapter, uint8_t mode)
{
}

void __weak nanEnableEht(struct ADAPTER *prAdapter, uint8_t enable)
{
}

u_int8_t __weak
nanExtHoldNdl(struct _NAN_NDL_INSTANCE_T *prNDL)
{
	return FALSE;
}

void __weak
nanExtBackToNormal(struct ADAPTER *prAdapter)
{
	nanInstantCommModeBackToNormal(prAdapter);
}

void __weak
nanExtResetNdlConfig(struct _NAN_NDL_INSTANCE_T *prNDL)
{
}

struct _NAN_NDL_INSTANCE_T * __weak
nanExtGetReusedNdl(struct ADAPTER *prAdapter)
{
	return NULL;
}

u32 __weak
wlanoidNANExtCmd(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	return WLAN_STATUS_NOT_SUPPORTED;
}

u32 __weak
wlanoidNANExtCmdRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	return WLAN_STATUS_NOT_SUPPORTED;
}

void __weak
nanExtComposeBeaconTrack(struct ADAPTER *prAdapter,
				struct _NAN_EVENT_REPORT_BEACON *prFwEvt)
{
}

void __weak
nanExtComposeClusterEvent(struct ADAPTER *prAdapter,
			       struct NAN_DE_EVENT *prDeEvt)
{
}

uint32_t __weak
nanSchedGetVendorEhtAttr(struct ADAPTER *prAdapter,
				  uint8_t **ppucVendorAttr,
				  uint32_t *pu4VendorAttrLength)
{
	return 0;
}

uint32_t __weak
nanSchedGetVendorAttr(struct ADAPTER *prAdapter,
			       uint8_t **ppucVendorAttr,
			       uint32_t *pu4VendorAttrLength)
{
	return 0;
}


uint16_t __weak
nanDataEngineVendorAttrLength(struct ADAPTER *prAdapter,
				 struct _NAN_NDL_INSTANCE_T *prNDL,
				 struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint16_t u2VSAttrLen = 0;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return 0;
	}

	DBGLOG(NAN, INFO,
		"NanCustomAttr = %d\n",
		prAdapter->rNanCustomAttr.length);

	if ((prNDL == NULL) || (prNDP == NULL))
		return 0;

	u2VSAttrLen = prAdapter->rNanCustomAttr.length;

	if (u2VSAttrLen > NAN_CUSTOM_ATTRIBUTE_MAX_SIZE)
		return 0;

	return u2VSAttrLen;
}

void __weak
nanDataEngineVendorAttrAppend(struct ADAPTER *prAdapter,
				      struct MSDU_INFO *prMsduInfo,
				      struct _NAN_NDL_INSTANCE_T *prNDL,
				      struct _NAN_NDP_INSTANCE_T *prNDP)
{
	uint8_t *pucVSAttrBuf = NULL;
	uint16_t u2VSAttrLen = 0;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return;
	}

	if (!prMsduInfo) {
		DBGLOG(NAN, ERROR, "prMsduInfo error\n");
		return;
	}

	if ((prNDL == NULL) || (prNDP == NULL))
		return;

	pucVSAttrBuf = prAdapter->rNanCustomAttr.data;
	u2VSAttrLen = prAdapter->rNanCustomAttr.length;

	if (u2VSAttrLen > NAN_CUSTOM_ATTRIBUTE_MAX_SIZE)
		return;

	kalMemCopy(((uint8_t *)prMsduInfo->prPacket) +
		   prMsduInfo->u2FrameLength,
		   pucVSAttrBuf, u2VSAttrLen);

	prMsduInfo->u2FrameLength += u2VSAttrLen;
}

uint16_t __weak
nanDataEngineVendorEhtAttrLength(struct ADAPTER *prAdapter,
				 struct _NAN_NDL_INSTANCE_T *prNDL,
				 struct _NAN_NDP_INSTANCE_T *prNDP)
{
	return 0;
}

void __weak
nanDataEngineVendorEhtAttrAppend(struct ADAPTER *prAdapter,
				      struct MSDU_INFO *prMsduInfo,
				      struct _NAN_NDL_INSTANCE_T *prNDL,
				      struct _NAN_NDP_INSTANCE_T *prNDP)
{
}

uint32_t __weak
nanGetFcSlots(struct ADAPTER *prAdapter)
{
	uint32_t u4Bitmap = 0;

	DBGLOG(NAN, TEMP,
	       "FC slots: %02x-%02x-%02x-%02x\n",
	       ((uint8_t *)&u4Bitmap)[0], ((uint8_t *)&u4Bitmap)[1],
	       ((uint8_t *)&u4Bitmap)[2], ((uint8_t *)&u4Bitmap)[3]);

	return u4Bitmap;
}

uint32_t __weak
nanGetTimelineFcSlots(struct ADAPTER *prAdapter, size_t szTimelineIdx,
			       size_t szSlotIdx)
{
	uint32_t u4Bitmap = 0;

	NAN_DW_DBGLOG(NAN, DEBUG, TRUE, szSlotIdx,
		      "Timeline %u FC slots: %02x-%02x-%02x-%02x\n",
		      szTimelineIdx,
		      ((uint8_t *)&u4Bitmap)[0], ((uint8_t *)&u4Bitmap)[1],
		      ((uint8_t *)&u4Bitmap)[2], ((uint8_t *)&u4Bitmap)[3]);

	return u4Bitmap;
}

/*
 * @szSlotIdx: slot index [0..512) representing the range in 0~8192 TU
 */
u_int8_t __weak
nanIsChnlSwitchSlot(struct ADAPTER *prAdapter,
			     unsigned char fgPrintLog,
			     size_t szTimelineIdx,
			     size_t szSlotIdx)
{
	return FALSE;
}

void __weak
nanExtEnterApNan(struct ADAPTER *prAdapter, uint8_t ucReason)
{

}

void __weak
nanExtTerminateApNan(struct ADAPTER *prAdapter, uint8_t ucReason)
{

}

void __weak
nanExtTerminateApNanEndPs(struct ADAPTER *prAdapter)
{

}

void __weak
nanExtTerminateApNanEndLegacy(struct ADAPTER *prAdapter)
{

}

void __weak
nanExtRxAuthHandler(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{

}

void __weak
nanExtAisConnectHandler(struct ADAPTER *prAdapter)
{

}
uint32_t __weak
nanExtSetCountryCodeHandler(struct ADAPTER *prAdapter)
{
	return 0;
}

uint32_t __weak
nanExtAisAssocDoneHandler(
		struct ADAPTER *prAdapter,
		uint32_t rJoinStatus,
		struct STA_RECORD *prStaRec)
{
	return 0;
}

uint32_t __weak
nanExtScanStartHandler(
		struct ADAPTER *prAdapter,
		struct cfg80211_scan_request *request)
{
	return 0;
}

uint32_t __weak
nanExtAisChangeHandler(
		struct ADAPTER *prAdapter)
{
	return 0;
}

uint32_t __weak
nanExtScanCompleteHandler(
		struct ADAPTER *prAdapter,
		uint8_t ucStatus)
{
	return 0;
}

uint32_t __weak
nanExtEventHandler(struct ADAPTER *prAdapter, uint8_t *pucBody)
{
	uint32_t u4SubEvent;

	u4SubEvent = nanGetEventTag(pucBody);
	if (u4SubEvent >= NAN_EVENT_NUM)
		return WLAN_STATUS_FAILURE;

	switch (u4SubEvent) {
	case NAN_EVENT_DFSP_CSA:
	case NAN_EVENT_DFSP_CSA_COMPLETE:
	case NAN_EVENT_DFSP_SUSPEND_RESUME:
	case NAN_EVENT_REPORT_DW_START:
	case NAN_EVENT_REPORT_DW_END:
	case NAN_EVENT_DEVICE_ROLE:
		DBGLOG(NAN, TRACE, "Got Event %u\n", u4SubEvent);
		break;
	}

	return 0;
}

uint32_t __weak
nanExtProcessRsvdFrame(struct ADAPTER *prAdapter,
		 struct SW_RFB *prSwRfb)
{
	return 0;
}

uint32_t __weak
nanExtAisAssocStartHandler(
		struct ADAPTER *prAdapter,
		enum ENUM_BAND eBand,
		uint8_t ucChannelNum,
		uint8_t ucChnlBw,
		enum ENUM_CHNL_EXT eSco)
{
	return 0;
}

uint32_t __weak
nanExtRxAssocHandler(
		struct ADAPTER *prAdapter,
		uint8_t *buf)
{
	return 0;
}

/* Sample command format: Tid=1,Conf=2,Inst=3,Lat=4,Tput=5 */
static int nan_parsing_ps_command(char *pcCommand,
			  struct _NAN_CMD_DATA_POWERSAVE *prNanDataPowerSaveCmd)
{
	char *pToken = NULL;
	char *pDelim = ",";
	char *pValue = NULL;
	unsigned long ulVal = 0;

	if (!pcCommand || !prNanDataPowerSaveCmd)
		return WLAN_STATUS_FAILURE;

	/* Skip command name, find first space */
	pToken = kalStrChr(pcCommand, ' ');
	if (!pToken)
		return WLAN_STATUS_FAILURE;

	pToken++; /* Skip the space */

	/* Parse each key=value pair */
	pToken = kalStrtokR(pToken, pDelim, &pValue);
	while (pToken != NULL) {

		if (kalStrniCmp(pToken, "Tid=", 4) == 0) {
			kalStrtoul(pToken + 4, 10, &ulVal);
			prNanDataPowerSaveCmd->u4NdpTransactionId = ulVal;
		} else if (kalStrniCmp(pToken, "Conf=", 5) == 0) {
			kalStrtoul(pToken + 5, 10, &ulVal);
			prNanDataPowerSaveCmd->u2UpdateConfig = (uint16_t)ulVal;
		} else if (kalStrniCmp(pToken, "Inst=", 5) == 0) {
			kalStrtoul(pToken + 5, 10, &ulVal);
			prNanDataPowerSaveCmd->u4InstanceId = ulVal;
		} else if (kalStrniCmp(pToken, "Lat=", 4) == 0) {
			kalStrtoul(pToken + 4, 10, &ulVal);
			if (ulVal != 0 && ulVal != 100 && ulVal != 200 &&
			    ulVal != 520)
				return WLAN_STATUS_FAILURE;
			prNanDataPowerSaveCmd->u4Latency = ulVal;
		} else if (kalStrniCmp(pToken, "Tput=", 5) == 0) {
			kalStrtoul(pToken + 5, 10, &ulVal);
			if (ulVal != 20 && ulVal != 10)
				return WLAN_STATUS_FAILURE;
			prNanDataPowerSaveCmd->u4Tput = ulVal;
		}
		pToken = kalStrtokR(NULL, pDelim, &pValue);
	}

	return WLAN_STATUS_SUCCESS;
}

int __weak
nan_ext_set_powersave(struct ADAPTER *prAdapter,
		      char *pcCommand, int32_t i4TotalLen)
{
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct _NAN_CMD_DATA_POWERSAVE rNanDataPowerSaveCmd = {0};
	uint32_t u4BufLen;

	prGlueInfo = prAdapter->prGlueInfo;

	/* iwpriv wlan0 driver "nan_setps 123" => pcCommand=nan_setps 123 */
	if (nan_parsing_ps_command(pcCommand, &rNanDataPowerSaveCmd) !=
	    WLAN_STATUS_SUCCESS)
		return WLAN_STATUS_FAILURE;

	DBGLOG(NAN, INFO,
	       "PowerSave: Tid=%u, conf=%u, inst=%u, lat=%u, tput=%u",
	       rNanDataPowerSaveCmd.u4NdpTransactionId,
	       rNanDataPowerSaveCmd.u2UpdateConfig,
	       rNanDataPowerSaveCmd.u4InstanceId,
	       rNanDataPowerSaveCmd.u4Latency,
	       rNanDataPowerSaveCmd.u4Tput);

	rStatus =  kalIoctl(prGlueInfo, nanOidPsMode,
			    &rNanDataPowerSaveCmd,
			    sizeof(struct _NAN_CMD_DATA_POWERSAVE),
			    &u4BufLen);

	return rStatus;
}

#endif /* CFG_SUPPORT_NAN == 1 */

