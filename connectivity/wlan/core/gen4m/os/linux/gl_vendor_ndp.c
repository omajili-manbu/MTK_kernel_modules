// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*
 * gl_vendor_ndp.c
 */

#if (CFG_SUPPORT_NAN == 1)
/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_vendor_ndp.h"
#include "debug.h"
#include "gl_cfg80211.h"
#include "gl_os.h"
#include "gl_vendor.h"
#include "gl_wext.h"
#include "nan_data_engine.h"
#include "nan_sec.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include <linux/can/netlink.h>
#include <net/cfg80211.h>
#include <net/netlink.h>

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
uint8_t g_InitiatorMacAddr[6];

const struct nla_policy
	mtk_wlan_vendor_ndp_policy[MTK_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1] = {
			[MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID] = {
				.type = NLA_U16 },
			[MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR] = {
				.type = NLA_NUL_STRING,
				.len = IFNAMSIZ - 1 },
			[MTK_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_CHANNEL] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR] = {
				.type = NLA_BINARY,
				.len = MAC_ADDR_LEN },
			[MTK_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY] = {
				.type = NLA_NESTED },
			[MTK_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO] = {
				.type = NLA_BINARY,
				.len = NDP_APP_INFO_LEN },
			[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR] = {
				.type = NLA_BINARY,
				.len = MAC_ADDR_LEN },
			[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY] = {
				.type = NLA_BINARY,
				.len = NDP_NUM_INSTANCE_ID },
			[MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_CHANNEL_CONFIG] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_PMK] = {
				.type = NLA_BINARY,
				.len = NDP_PMK_LEN },
			[MTK_WLAN_VENDOR_ATTR_NDP_SCID] = {
				.type = NLA_BINARY,
				.len = NDP_SCID_BUF_LEN },
			[MTK_WLAN_VENDOR_ATTR_NDP_CSID] = {
				.type = NLA_U32 },
			[MTK_WLAN_VENDOR_ATTR_NDP_PASSPHRASE] = {
				.type = NLA_BINARY,
				.len = NAN_PASSPHRASE_MAX_LEN },
			[MTK_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME] = {
				.type = NLA_BINARY,
				.len = NAN_MAX_SERVICE_NAME_LEN },
	};

#define NAN_MAX_CHANNEL_INFO_SUPPORTED (4)

/* NAN Channel Info */
struct NanChannelInfo {
	u32 channel;
	u32 bandwidth;
	u32 nss;
};

void
nanGetChannelInfo(
	struct ADAPTER *ad,
	struct _NAN_NDP_INSTANCE_T *prNDP,
	struct NanChannelInfo *info,
	uint32_t *num_info)
{
	uint32_t u4Idx = 0, u4Idx1 = 0, u4Idx2 = 0;
	uint32_t i = 0;
	struct _NAN_NDL_INSTANCE_T *prNDL = NULL;
	struct _NAN_PEER_SCH_DESC_T *p = NULL;
	struct _NAN_AVAILABILITY_DB_T *d = NULL;
	struct _NAN_AVAILABILITY_TIMELINE_T *t = NULL;
	union _NAN_BAND_CHNL_CTRL chctrl;
	uint32_t pch = 0;
	uint32_t opc = 0;

	if (!prNDP)
		return;

	prNDL =
		&ad->rDataPathInfo.arNDL[prNDP->ucNdlIndex];
	if (!prNDL)
		return;

	p = nanSchedSearchPeerSchDescByNmi(
		ad,
		prNDL->aucPeerMacAddr);
	if (!p) {
		DBGLOG(NAN, WARN,
			"PeerSchDesc for " MACSTR " not found\n",
		MAC2STR(prNDL->aucPeerMacAddr));
		return;
	}

	*num_info = 0;

	for (u4Idx = 0; u4Idx < NAN_NUM_AVAIL_DB; u4Idx++) {
		d = &p->arAvailAttr[u4Idx];
		if (d->ucMapId == NAN_INVALID_MAP_ID)
			continue;

		for (u4Idx1 = 0;
			u4Idx1 < NAN_NUM_AVAIL_TIMELINE;
			u4Idx1++) {
			t = &d->arAvailEntryList[u4Idx1];
			if (t->fgActive == FALSE)
				continue;

			if (t->arBandChnlCtrl[0].u4Type ==
				NAN_BAND_CH_ENTRY_LIST_TYPE_BAND)
				continue;

			for (u4Idx2 = 0;
				u4Idx2 < t->ucNumBandChnlCtrl;
				u4Idx2++) {
				chctrl = t->arBandChnlCtrl[u4Idx2];
				pch = chctrl.u4PrimaryChnl;
				opc = chctrl.u4OperatingClass;
				info[i].channel = pch;
				info[i].bandwidth = nanRegGetBw(opc);
				info[i].nss = 2;
				DBGLOG(NAN, DEBUG,
					"[%u][%u]Map:%d,Bw:%d,Ch:%d\n",
					u4Idx, u4Idx1,
					d->ucMapId,
					info[i].bandwidth,
					info[i].channel);
				i++;
				*num_info = i;
				if (i >=
					NAN_MAX_CHANNEL_INFO_SUPPORTED)
					return;
			}
		}
	}
}

uint32_t nanOidDataRequest(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct _NAN_CMD_DATA_REQUEST *prNanCmdDataRequest;
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	struct NanDataReqReceive rDataRcv;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;
	prNanCmdDataRequest =
		(struct _NAN_CMD_DATA_REQUEST *) pvSetBuffer;

	if (u4SetBufferLen <
		sizeof(struct _NAN_CMD_DATA_REQUEST))
		return WLAN_STATUS_INVALID_DATA;

	nanBackToNormal(prAdapter);

	rStatus = nanCmdDataRequest(prAdapter,
		prNanCmdDataRequest,
		&rDataRcv.ndpid,
		rDataRcv.initiator_data_addr);

	DBGLOG(NAN, DEBUG,
	       "Initiator request to peer " MACSTR ", status = %d\n",
	       MAC2STR(prNanCmdDataRequest->aucResponderDataAddress),
	       rStatus);

	return rStatus;
}


uint32_t nanOidDataResponse(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct _NAN_CMD_DATA_RESPONSE *prNanCmdDataResponse;
	int32_t rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;
	prNanCmdDataResponse =
		(struct _NAN_CMD_DATA_RESPONSE *) pvSetBuffer;

	if (u4SetBufferLen <
		sizeof(struct _NAN_CMD_DATA_RESPONSE))
		return WLAN_STATUS_INVALID_DATA;

	nanBackToNormal(prAdapter);

	rStatus = nanCmdDataResponse(prAdapter, prNanCmdDataResponse);

	DBGLOG(NAN, DEBUG,
	   "Responder response to peer " MACSTR ", status = %d\n",
	   MAC2STR(prNanCmdDataResponse->aucInitiatorDataAddress),
	   rStatus);

	return rStatus;
}

uint32_t nanOidEndReq(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct _NAN_CMD_DATA_END *prNanCmdDataEnd;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;
	prNanCmdDataEnd = (struct _NAN_CMD_DATA_END *) pvSetBuffer;

	if (u4SetBufferLen <
		sizeof(struct _NAN_CMD_DATA_END))
		return WLAN_STATUS_INVALID_DATA;

	return nanCmdDataEnd(prAdapter, prNanCmdDataEnd);
}

/**
 *		Latency	0		100	     200(tput=20)  520(tput=10)
 * AIS == 0
 * NAN == 2437 (ch 6)	03-00-03-00	-	     -		  -
 * NAN == 5745 (ch 149)	ff-ff-ff-ff	10-42-10-44  0c-06-60-00  00-06-00-00
 *
 * AIS == 2412 (ch 1)
 * NAN == 2437 (ch 6)	03-00-03-00
 * NAN == 5745 (ch 149)	ff-ff-ff-ff	10-42-10-44  0c-06-60-00  00-06-00-00
 *
 * AIS == 5745 (ch 149)
 * NAN == 2437 (ch 6)	03-00-03-00
 * NAN == 5745 (ch 149)	ff-ff-ff-ff	10-42-10-44  0c-06-60-00  00-06-00-00
 *
 * === All the above are the same, 2G conflict with FAW, don't set ===
 * AIS 5G MCC case:
 *
 *		Latency	0		100	     200(tput=20) 520(tput=10)
 * AIS == 5765 (ch 153)
 * NAN == 2437 (ch 6)	03-00-03-00
 * NAN == 5745 (ch 149)	00-ff-00-ff	00-42-00-44  0c-06-00-00  00-06-00-00
 * & (0xFF00FF00)
 * NAN == 5765 (ch 153)	ff-00-ff-00	10-00-10-00  0c-00-60-00  00-06-00-00
 * & (0x00FF00FF)
 */
uint32_t nanOidPsMode(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
	struct _NAN_CMD_DATA_POWERSAVE *prNanDataPowerSaveCmd;
	struct _NAN_PEER_SCH_DESC_T *prPeerSchDesc;
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t au4Levels[NAN_CUSTOM_BITMAP_LEVELS] = {0};
	union _NAN_BAND_CHNL_CTRL rConcurrentChnl = {0};
	uint32_t u4SlotBitmap = {0};
	uint8_t ucAisPhyTypeSet;
	uint32_t u4Level;
	uint32_t u4Mask = 0xFFFFFFFF;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);
	ASSERT(pvSetBuffer);

	*pu4SetInfoLen = u4SetBufferLen;
	prNanDataPowerSaveCmd = (struct _NAN_CMD_DATA_POWERSAVE *)pvSetBuffer;

	if (u4SetBufferLen < sizeof(struct _NAN_CMD_DATA_POWERSAVE))
		return WLAN_STATUS_INVALID_DATA;

	prPeerSchDesc = nanDataUtilSearchPeerSchDescByNdpInstanceId(prAdapter,
				   prNanDataPowerSaveCmd->u4InstanceId);
	if (!prPeerSchDesc)
		return WLAN_STATUS_INVALID_DATA;

	kalMemCopy(au4Levels, prWifiVar->u4NdpCustomThreshold,
		   sizeof(au4Levels));

#if (CFG_SUPPORT_WIFI_6G == 1)
#if (CFG_SUPPORT_NAN_6G == 1)
	/* Concurrent in P2P channel */
	nanSchedGetConnChnlUsage(prAdapter, NETWORK_TYPE_P2P, BAND_6G,
				 &rConcurrentChnl, &u4SlotBitmap,
				 &ucAisPhyTypeSet);
	if (rConcurrentChnl.u4PrimaryChnl)
		return WLAN_STATUS_NOT_ACCEPTED;
#endif
#endif

	nanSchedGetConnChnlUsage(prAdapter, NETWORK_TYPE_P2P, BAND_5G,
				 &rConcurrentChnl, &u4SlotBitmap,
				 &ucAisPhyTypeSet);
	if (rConcurrentChnl.u4PrimaryChnl)
		return WLAN_STATUS_NOT_ACCEPTED;


	nanSchedGetConnChnlUsage(prAdapter, NETWORK_TYPE_AIS, BAND_5G,
				 &rConcurrentChnl, &u4SlotBitmap,
				 &ucAisPhyTypeSet);
	/* Only set NDL slots, ignore AIS slots */
	if (rConcurrentChnl.u4PrimaryChnl &&
	    rConcurrentChnl.u4PrimaryChnl != g_r5gDwChnl.u4PrimaryChnl)
		u4Mask = NAN_SLOT_MASK_TYPE_DEFAULT_NDL;

	/* Find a proper latency level less than given latency  */
	for (u4Level = 0; u4Level < ARRAY_SIZE(au4Levels); u4Level++) {
		if (prNanDataPowerSaveCmd->u4Latency <= au4Levels[u4Level])
			break;
	}
	if (u4Level == ARRAY_SIZE(au4Levels))
		u4Level = ARRAY_SIZE(au4Levels) - 1;

	nanUpdateCustomizedNdpBitmap(prAdapter, FALSE, prPeerSchDesc->u4SchIdx,
			     NAN_5G_IDX, TRUE, g_r5gDwChnl.u4PrimaryChnl,
			     prWifiVar->u4NdpCustomBitmap[u4Level] & u4Mask);

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief After NDI interface create, send create response to wifi hal
*
* \param[in] prNDP: NDP info
*
* \return WLAN_STATUS
*/
/*----------------------------------------------------------------------------*/
uint32_t
nanNdiCreateRspEvent(struct ADAPTER *prAdapter,
		struct NdiIfaceCreate rNdiInterfaceCreate) {
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint16_t u2CreateRspLen;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}


	DBGLOG(NAN, DEBUG, "Send NDI Create Rsp event\n");

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2CreateRspLen = (3 * sizeof(uint32_t)) + sizeof(uint16_t) +
			 (4 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2CreateRspLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE) <
		     0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* Transaction ID */
	if (unlikely(nla_put_u16(skb, MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
				rNdiInterfaceCreate.u2NdpTransactionId) < 0)){
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* NMI(same as NDI) */
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR,
				strlen(rNdiInterfaceCreate.pucIfaceName) + 1,
				rNdiInterfaceCreate.pucIfaceName) < 0)){
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* prNDP no NanInternalStatusType field,
	 * set NAN_I_STATUS_SUCCESS as workaround
	 */
	if (unlikely(nla_put_u32(
			     skb,
			     MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			     NAN_I_STATUS_SUCCESS) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
				 DP_REASON_SUCCESS) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief After NDI interface delete, send delete response to wifi hal
 *
 * \param[in] prNDP: NDP info
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdiDeleteRspEvent(struct ADAPTER *prAdapter,
		struct NdiIfaceDelete rNdiInterfaceDelete) {
	struct sk_buff *skb = NULL;
	struct net_device *prNetDevice = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	uint16_t u2CreateRspLen;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDI Delete Rsp event\n");

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	if (wiphy == NULL) {
		DBGLOG(NAN, ERROR, "[%s] wiphy is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	prNetDevice = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (prNetDevice == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNetDevice is NULL\n", __func__);
		return -EFAULT;
	}
	wdev = prNetDevice->ieee80211_ptr;
	u2CreateRspLen = (3 * sizeof(uint32_t)) + sizeof(uint16_t) +
			 (4 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2CreateRspLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE) <
					0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* Transaction ID */
	if (unlikely(nla_put_u16(skb, MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
				rNdiInterfaceDelete.u2NdpTransactionId) < 0)){
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* NMI(same as NDI) */
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR,
				strlen(rNdiInterfaceDelete.pucIfaceName) + 1,
				rNdiInterfaceDelete.pucIfaceName) < 0)){
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}
	/* prNDP no NanInternalStatusType field,
	 * set NAN_I_STATUS_SUCCESS as workaround
	 */
	if (unlikely(nla_put_u32(
			     skb,
			     MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			     NAN_I_STATUS_SUCCESS) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
				 DP_REASON_SUCCESS) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief After Tx initiator req NAF TxDone, send initiator response to wifi hal
 *
 * \param[in] prNDP: NDP info
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdpInitiatorRspEvent(struct ADAPTER *prAdapter,
			struct _NAN_NDP_INSTANCE_T *prNDP,
			uint32_t rTxDoneStatus) {
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint16_t u2InitiatorRspLen;
	uint32_t u4Id = 0;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (prNDP == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDP is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "[%s] Send NDP Initiator Rsp event\n", __func__);

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2InitiatorRspLen = (4 * sizeof(uint32_t)) + (1 * sizeof(uint16_t)) +
			    (5 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2InitiatorRspLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_INITIATOR_RESPONSE) <
		     0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u16(skb, MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
				 prNDP->u2TransId) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (nanGetFeatureIsSigma(prAdapter))
		u4Id = prNDP->ucNDPID;
	else
		u4Id = prNDP->ndp_instance_id;

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
				 u4Id) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* prNDP no NanInternalStatusType field,
	 * set NAN_I_STATUS_SUCCESS and NAN_I_STATUS_TIMEOUT as workaround
	 */
	if (rTxDoneStatus == WLAN_STATUS_SUCCESS) {
		if (unlikely(nla_put_u32(skb,
			    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			    NAN_I_STATUS_SUCCESS) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	} else {
		if (unlikely(nla_put_u32(skb,
			    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			    NAN_I_STATUS_TIMEOUT) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
				 prNDP->eDataPathFailReason) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief After Tx responder req NAF TxDone, send responder response to wifi hal
 *
 * \param[in] prNDP: NDP info
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdpResponderUserTimeoutEvent(struct ADAPTER *prAdapter,
				uint32_t ndp_instance_id,
				uint16_t u2TransId)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint16_t u2ResponderRspLen;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDP Response event NdpId(%u) trans=%u\n",
	       ndp_instance_id, u2TransId);

	if (u2TransId == 0) {
		DBGLOG(NAN, ERROR,
		       "Invalid transaction id NdpId(%u) trans=%u\n",
		       ndp_instance_id, u2TransId);
		return WLAN_STATUS_INVALID_DATA;
	}

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2ResponderRspLen = (3 * sizeof(uint32_t)) + sizeof(uint16_t) +
			    (4 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2ResponderRspLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb,
			MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			MTK_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE) <
		     0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u16(skb,
			MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
			u2TransId) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u32(skb,
		    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
		    NAN_I_STATUS_TIMEOUT) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u32(skb,
			MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
			DP_REASON_USER_SPACE_RESPONSE_TIMEOUT) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

uint32_t
nanNdpResponderRspEvent(struct ADAPTER *prAdapter,
			struct _NAN_NDP_INSTANCE_T *prNDP,
			uint32_t rTxDoneStatus) {
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint16_t u2ResponderRspLen;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (prNDP == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDP is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDP Response event NdpId(%u) trans=%u\n",
	       prNDP->ndp_instance_id, prNDP->u2TransId);

	if (prNDP->u2TransId == 0) {
		DBGLOG(NAN, ERROR,
		       "Invalid transaction id NdpId(%u) trans=%u\n",
		       prNDP->ndp_instance_id, prNDP->u2TransId);
		return WLAN_STATUS_INVALID_DATA;
	}

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2ResponderRspLen = (3 * sizeof(uint32_t)) + sizeof(uint16_t) +
			    (4 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2ResponderRspLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE) <
		     0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u16(skb, MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
				 prNDP->u2TransId) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* prNDP no NanInternalStatusType field,
	 * set NAN_I_STATUS_SUCCESS and NAN_I_STATUS_TIMEOUT as workaround
	 */
	if (rTxDoneStatus == WLAN_STATUS_SUCCESS) {
		if (unlikely(nla_put_u32(skb,
			    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			    NAN_I_STATUS_SUCCESS) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	} else {
		if (unlikely(nla_put_u32(skb,
			    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			    NAN_I_STATUS_TIMEOUT) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
				 prNDP->eDataPathFailReason) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief After Tx termination req NAF TxDone, send end response to wifi hal
 *
 * \param[in] prNDP: NDP info
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
uint32_t
nanNdpEndRspEvent(struct ADAPTER *prAdapter,
	enum _ENUM_DP_PROTOCOL_REASON_CODE_T eReason,
	uint16_t u2TransId,
	uint32_t rTxDoneStatus)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint16_t u2EndRspLen;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prAdapter is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDI End Rsp event\n");

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2EndRspLen = (3 * sizeof(uint32_t)) + sizeof(uint16_t) +
		      (4 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2EndRspLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_END_RESPONSE) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	/* prNDP no NanInternalStatusType field,
	 * set NAN_I_STATUS_SUCCESS and NAN_I_STATUS_TIMEOUT as workaround
	 */
	if (rTxDoneStatus == WLAN_STATUS_SUCCESS) {
		if (unlikely(nla_put_u32(
			    skb,
			    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			    NAN_I_STATUS_SUCCESS) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	} else {
		if (unlikely(nla_put_u32(skb,
			    MTK_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			    NAN_I_STATUS_TIMEOUT) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
				 eReason) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}
	if (unlikely(nla_put_u16(skb, MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
				 u2TransId) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle NAN interface create request vendor cmd.
 *
 * \param[in] prGlueInfo: Pointer to glue info structure.
 *
 * \param[in] tb: NDP vendor cmd attributes.
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
int32_t nanNdiCreateHandler(struct GLUE_INFO *prGlueInfo, struct nlattr **tb)
{
	/* Need implement */
	struct ADAPTER *prAdapter = NULL;
	struct NdiIfaceCreate rNdiInterfaceCreate;

	kalMemZero(&rNdiInterfaceCreate, sizeof(struct NdiIfaceCreate));

	if (prGlueInfo == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prGlueInfo is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Get transaction ID */
	if (!tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		DBGLOG(NAN, ERROR, "Get NDP Transaction ID error!\n");
		return -EINVAL;
	}
	rNdiInterfaceCreate.u2NdpTransactionId =
		nla_get_u16(tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	/* Get interface name */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR]) {
		rNdiInterfaceCreate.pucIfaceName =
			nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR]);
		DBGLOG(NAN, DEBUG,
			"Transaction ID: %d Interface name: %s\n",
			rNdiInterfaceCreate.u2NdpTransactionId,
			rNdiInterfaceCreate.pucIfaceName);
	}

	/* Send event to wifi hal */
	prAdapter = prGlueInfo->prAdapter;
	nanNdiCreateRspEvent(prAdapter, rNdiInterfaceCreate);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle NAN interface delete request vendor cmd.
 *
 * \param[in] prGlueInfo: Pointer to glue info structure.
 *
 * \param[in] tb: NDP vendor cmd attributes.
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
int32_t nanNdiDeleteHandler(struct GLUE_INFO *prGlueInfo, struct nlattr **tb)
{
	/* Need implement */
	struct ADAPTER *prAdapter = NULL;
	struct NdiIfaceDelete rNdiInterfaceDelete;

	kalMemZero(&rNdiInterfaceDelete, sizeof(struct NdiIfaceDelete));

	if (prGlueInfo == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prGlueInfo is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "NAN interface delete request, need implement!\n");

	/* Get transaction ID */
	if (!tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		DBGLOG(NAN, ERROR, "Get NDP Transaction ID error!\n");
		return -EINVAL;
	}
	rNdiInterfaceDelete.u2NdpTransactionId =
		nla_get_u16(tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	/* Get interface name */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR]) {
		rNdiInterfaceDelete.pucIfaceName =
			nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_IFACE_STR]);
		DBGLOG(NAN, DEBUG,
			"Delete transaction ID: %d Interface name: %s\n",
			rNdiInterfaceDelete.u2NdpTransactionId,
			rNdiInterfaceDelete.pucIfaceName);
	}

	/* Workaround: send event to wifi hal */
	prAdapter = prGlueInfo->prAdapter;
	nanNdiDeleteRspEvent(prAdapter, rNdiInterfaceDelete);
	return WLAN_STATUS_SUCCESS;
}



/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle NDP initiator request vendor cmd.
 *
 * \param[in] prGlueInfo: Pointer to glue info structure.
 *
 * \param[in] tb: NDP vendor cmd attributes.
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
int32_t nanNdpInitiatorReqHandler(struct GLUE_INFO *prGlueInfo,
		struct nlattr **tb)
{
	struct _NAN_CMD_DATA_REQUEST rNanCmdDataReq;
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t aucPassphrase[64] = {0};
	uint8_t aucSalt[2 + sizeof(g_aucNanServiceId) +
			sizeof(rNanCmdDataReq.aucResponderDataAddress)] = {
			0x00, 0x01,
	};
	uint32_t u4BufLen;
	struct nlattr *p;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	struct ADAPTER *prAdapter = NULL;
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	kalMemZero(&rNanCmdDataReq, sizeof(rNanCmdDataReq));

	/* Instance ID */
	p = tb[MTK_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID];
	if (!p) {
		DBGLOG(NAN, ERROR, "Get NDP Instance ID unavailable!\n");
		return -EINVAL;
	}

	/* Get transaction ID */
	p = tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID];
	if (p) {
		rNanCmdDataReq.u2NdpTransactionId = nla_get_u32(p);
		DBGLOG(NAN, ERROR, "Get NDP Transaction ID = %d\n",
			rNanCmdDataReq.u2NdpTransactionId);
	}

	/* Peer mac Addr */
	p = tb[MTK_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR];
	if (p) {
		kalMemCopy(rNanCmdDataReq.aucResponderDataAddress,
			   nla_data(p), MAC_ADDR_LEN);
	}
	rNanCmdDataReq.ucPublishID =
		nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID]);
	if (rNanCmdDataReq.ucPublishID == 0)
		rNanCmdDataReq.ucPublishID = g_u2IndPubId;

	/* Security */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY]) {
		if (tb[MTK_WLAN_VENDOR_ATTR_NDP_CSID])
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
		{
			uint8_t cipher_suite_list[4] = {0};

			prAdapter = prGlueInfo->prAdapter;
			if (prAdapter == NULL) {
				DBGLOG(NAN, ERROR, "Adapter NULL\n");
				return WLAN_STATUS_NOT_ACCEPTED;
			}

			kalMemCpyS(cipher_suite_list,
				sizeof(cipher_suite_list),
				nla_data(
				tb[MTK_WLAN_VENDOR_ATTR_NDP_CSID]),
				sizeof(cipher_suite_list));
			rNanCmdDataReq.ucSecurity = cipher_suite_list[0];
			rNanCmdDataReq.ucGtkCipher = cipher_suite_list[1];

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
			prPairingFsm = pairingFsmSearch(prAdapter,
					rNanCmdDataReq.aucResponderDataAddress);
			if (prPairingFsm &&
			    prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
				rNanCmdDataReq.ucSecurity =
					 NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
			}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

			/* Set GTK from wifi.cfg */
			if (prAdapter->rWifiVar.u4NanGtkCipher !=
			    NAN_CIPHER_SUITE_ID_NONE) {
				rNanCmdDataReq.ucGtkCipher =
					prAdapter->rWifiVar.u4NanGtkCipher;
			}

			DBGLOG(NAN, INFO,
				"Cipher:%u, GTKCipher:%u\n",
				rNanCmdDataReq.ucSecurity,
				rNanCmdDataReq.ucGtkCipher);
		}
#else
			rNanCmdDataReq.ucSecurity =
				nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_CSID]);
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

		p = tb[MTK_WLAN_VENDOR_ATTR_NDP_PMK];
		if (rNanCmdDataReq.ucSecurity && p) {
			kalMemCopy(rNanCmdDataReq.aucPMK,
				   nla_data(p), nla_len(p));

#if (ENABLE_SEC_UT_LOG == 1)
			DBGDUMP_HEX(NAN, DEBUG, "PMK from APP",
				    nla_data(p), nla_len(p));
#endif
		}

		if (rNanCmdDataReq.ucSecurity) {
			p = tb[MTK_WLAN_VENDOR_ATTR_NDP_PASSPHRASE];
			if (p) {
				DBGLOG(NAN, DEBUG, "PASSPHRASE\n");
				kalMemCopy(aucPassphrase,
					nla_data(p), nla_len(p));
				kalMemCopy(aucSalt + 2, g_aucNanServiceId,
					   sizeof(g_aucNanServiceId));
				dumpMemory8(g_aucNanServiceId,
					    sizeof(g_aucNanServiceId));
				COPY_MAC_ADDR(aucSalt + 8,
					rNanCmdDataReq.aucResponderDataAddress);
				dumpMemory8(aucPassphrase,
					    sizeof(aucPassphrase));
				dumpMemory8(aucSalt, sizeof(aucSalt));
				PKCS5_PBKDF2_HMAC(aucPassphrase,
						  sizeof(aucPassphrase) - 1,
						  aucSalt, sizeof(aucSalt),
						  4096, 32,
						  rNanCmdDataReq.aucPMK);

				dumpMemory8(rNanCmdDataReq.aucPMK,
					    sizeof(rNanCmdDataReq.aucPMK));
			}

			p = tb[MTK_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME];
			if (p) {
				DBGLOG(NAN, DEBUG, "pmkid(vendor cmd)\n");
				nanSetNdpPmkid(prGlueInfo->prAdapter,
					       &rNanCmdDataReq,
					       nla_data(p));
			} else {
				DBGLOG(NAN, DEBUG, "pmkid(local)\n");
				nanSetNdpPmkid(prGlueInfo->prAdapter,
					       &rNanCmdDataReq,
					       g_aucNanServiceName);
				dumpMemory8(g_aucNanServiceName,
					    NAN_MAX_SERVICE_NAME_LEN);
#ifdef NAN_UNUSED
				kalMemZero(g_aucNanServiceName,
					   NAN_MAX_SERVICE_NAME_LEN);
#endif
			}
		}
	}

	/* QoS: Default set to false for testing */
	p = tb[MTK_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS];
	if (p) {
		rNanCmdDataReq.ucRequireQOS = 0;
		/* rNanCmdDataReq.ucRequireQOS = */
		/* nla_get_u32(p); */
	}

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	/* Multicast Address */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_MULTICAST_ADDR]) {
		kalMemCpyS(rNanCmdDataReq.aucMulticastAddress,
			sizeof(rNanCmdDataReq.aucMulticastAddress),
			nla_data(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_MULTICAST_ADDR]),
			MAC_ADDR_LEN);
	}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	rNanCmdDataReq.fgNDPE = g_ndpReqNDPE.fgEnNDPE;
	/* APP Info */
	p = tb[MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO];
	if (p) {
		rNanCmdDataReq.u2SpecificInfoLength = nla_len(p);
		kalMemCopy(rNanCmdDataReq.aucSpecificInfo,
			   nla_data(p), nla_len(p));

		DBGLOG(NAN, DEBUG, "AppInfoLen = %d\n",
		       rNanCmdDataReq.u2SpecificInfoLength);
	}

	/* Ipv6 */
	p = tb[MTK_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR];
	if (p) {
		rNanCmdDataReq.fgCarryIpv6 = 1;
		kalMemCopy(rNanCmdDataReq.aucIPv6Addr,
			   nla_data(p), IPV6MACLEN);
	}

	/* NDPE */
	DBGLOG(NAN, DEBUG, "[%s] NDPEenable = %d\n",
		__func__, g_ndpReqNDPE.fgEnNDPE);

	/* Send cmd request */
	rStatus =  kalIoctl(prGlueInfo, nanOidDataRequest, &rNanCmdDataReq,
			    sizeof(struct _NAN_CMD_DATA_REQUEST),
			    &u4BufLen);

	/* Return status */
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle NDP responder request vendor cmd.
 *
 * \param[in] prGlueInfo: Pointer to glue info structure.
 *
 * \param[in] tb: NDP vendor cmd attributes.
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
int32_t nanNdpResponderReqHandler(struct GLUE_INFO *prGlueInfo,
		struct nlattr **tb)
{
	struct _NAN_CMD_DATA_RESPONSE rNanCmdDataResponse;
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t aucPassphrase[64] = {0};
	uint8_t aucSalt[] = { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
				 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	struct BSS_INFO *prBssInfo;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNanSpecificBssInfo;
	uint32_t u4BufLen;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	struct ADAPTER *prAdapter = NULL;
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	if (prGlueInfo->prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "prAdapter is null\n");
		return -EINVAL;
	}

	/* Get BSS info */
	prNanSpecificBssInfo = nanGetSpecificBssInfo(
		prGlueInfo->prAdapter,
		NAN_BSS_INDEX_BAND0);
	if (prNanSpecificBssInfo == NULL) {
		DBGLOG(NAN, ERROR, "prNanSpecificBssInfo is null\n");
		return -EINVAL;
	}
	prBssInfo = GET_BSS_INFO_BY_INDEX(
			prGlueInfo->prAdapter,
			prNanSpecificBssInfo->ucBssIndex);
	if (prBssInfo == NULL) {
		DBGLOG(NAN, ERROR, "prBssInfo is null\n");
		return -EINVAL;
	}

	kalMemZero(&rNanCmdDataResponse, sizeof(rNanCmdDataResponse));
	/* Decision status */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE]) {
		if (nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE])
				== NAN_DP_REQUEST_AUTO)
			rNanCmdDataResponse.ucDecisionStatus =
				NAN_DP_REQUEST_ACCEPT;
		else
			rNanCmdDataResponse.ucDecisionStatus =
				nla_get_u32(
				tb[MTK_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE]);
	}

	/* Instance ID */
	if (!tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]) {
		DBGLOG(NAN, ERROR, "Get NDP Instance ID unavailable!\n");
		return -EINVAL;
	}

	if (nanGetFeatureIsSigma(prGlueInfo->prAdapter)) {
		rNanCmdDataResponse.ucNDPId =
			nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]);
		DBGLOG(NAN, DEBUG, "[Data Resp] RespID:%d\n",
	       rNanCmdDataResponse.ucNDPId);
		if (rNanCmdDataResponse.ucNDPId == 0)
			rNanCmdDataResponse.ucNDPId = g_u2IndPubId;
	} else {
		rNanCmdDataResponse.ndp_instance_id =
			nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]);
		DBGLOG(NAN, DEBUG, "[Data Resp] InstanceRespID:%d\n",
			rNanCmdDataResponse.ndp_instance_id);
	}

	/* Get transaction ID */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		rNanCmdDataResponse.u2NdpTransactionId = nla_get_u32(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);
		DBGLOG(NAN, ERROR, "Get NDP Transaction ID =%d\n",
			rNanCmdDataResponse.u2NdpTransactionId);
	}

	/* QoS: Default set to false for testing */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS]) {
		rNanCmdDataResponse.ucRequireQOS = 0;
		/* rNanCmdDataResponse.ucRequireQOS =
		 * nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS]);
		 */
	}

	/* Security type */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_CSID])
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
		{
			uint8_t cipher_suite_list[4] = {0};
			prAdapter = prGlueInfo->prAdapter;

			kalMemCpyS(cipher_suite_list,
				sizeof(cipher_suite_list),
				nla_data(
				tb[MTK_WLAN_VENDOR_ATTR_NDP_CSID]),
				sizeof(cipher_suite_list));
			rNanCmdDataResponse.ucSecurity = cipher_suite_list[0];
			rNanCmdDataResponse.ucGtkCipher = cipher_suite_list[1];

			/* Set GTK from wifi.cfg */
			if (prAdapter->rWifiVar.u4NanGtkCipher !=
			    NAN_CIPHER_SUITE_ID_NONE) {
				rNanCmdDataResponse.ucGtkCipher =
					prAdapter->rWifiVar.u4NanGtkCipher;
			}

			DBGLOG(NAN, INFO,
				"Cipher:%u, GTKCipher:%u\n",
				rNanCmdDataResponse.ucSecurity,
				rNanCmdDataResponse.ucGtkCipher);
		}
#else
		rNanCmdDataResponse.ucSecurity =
			nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_CSID]);
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	/* App Info */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO]) {
		rNanCmdDataResponse.u2SpecificInfoLength =
			nla_len(tb[MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO]);
		kalMemCopy(rNanCmdDataResponse.aucSpecificInfo,
			nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO]),
			nla_len(tb[MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO]));

		DBGLOG(NAN, ERROR, "appInfoLen= %d\n",
			rNanCmdDataResponse.u2SpecificInfoLength);
	}

	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR]) {
		kalMemCopy(rNanCmdDataResponse.aucIPv6Addr,
			nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR]),
			IPV6MACLEN);
		rNanCmdDataResponse.fgCarryIpv6 = 1;
	}

	if (nanGetFeatureIsSigma(prGlueInfo->prAdapter)) {
		/* PortNum: vendor cmd did not fill this attribute,
		 * default set to 9000
		 */
		rNanCmdDataResponse.u2PortNum = 9000;

		/* Service protocol type:
		 * vendor cmd did not fill this attribute,
		 * default set to 0xFF
		 */
		rNanCmdDataResponse.ucServiceProtocolType = IP_PRO_TCP;
	}

	/* Peer mac addr */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]) {
		kalMemCopy(
			rNanCmdDataResponse.aucInitiatorDataAddress,
			nla_data(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]),
			MAC_ADDR_LEN);
	} else {
		kalMemZero(rNanCmdDataResponse.aucInitiatorDataAddress,
			MAC_ADDR_LEN);
	}
	DBGLOG(NAN, DEBUG, "aucInitiatorDataAddress = " MACSTR "\n",
		MAC2STR(rNanCmdDataResponse.aucInitiatorDataAddress));
	/* PMK */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_PMK]) {
		kalMemCopy(rNanCmdDataResponse.aucPMK,
			   nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_PMK]),
			   nla_len(tb[MTK_WLAN_VENDOR_ATTR_NDP_PMK]));
#if (ENABLE_SEC_UT_LOG == 1)
		DBGDUMP_HEX(NAN, DEBUG, "PMK from APP",
			    nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_PMK]),
			    nla_len(tb[MTK_WLAN_VENDOR_ATTR_NDP_PMK]));
#endif
	}
	/* PASSPHRASE */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_PASSPHRASE]) {
		DBGLOG(NAN, DEBUG, "PASSPHRASE\n");
		kalMemCopy(aucPassphrase,
			   nla_data(tb[MTK_WLAN_VENDOR_ATTR_NDP_PASSPHRASE]),
			   nla_len(tb[MTK_WLAN_VENDOR_ATTR_NDP_PASSPHRASE]));
		kalMemCopy(aucSalt + 2, g_aucNanServiceId, 6);
		COPY_MAC_ADDR(aucSalt + 8, prBssInfo->aucOwnMacAddr);
		DBGDUMP_HEX(NAN, DEBUG, "Passphrase",
			    aucPassphrase, sizeof(aucPassphrase));
		DBGDUMP_HEX(NAN, DEBUG, "Salt", aucSalt, sizeof(aucSalt));
		PKCS5_PBKDF2_HMAC((unsigned char *)aucPassphrase,
				  sizeof(aucPassphrase) - 1,
				  (unsigned char *)aucSalt,
				  sizeof(aucSalt),
				  4096, 32,
				  (unsigned char *)rNanCmdDataResponse.aucPMK);

		DBGDUMP_HEX(NAN, INFO, "PMK",
			    rNanCmdDataResponse.aucPMK,
			    sizeof(rNanCmdDataResponse.aucPMK));
	}

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	/* Multicast Address */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_MULTICAST_ADDR]) {
		kalMemCpyS(rNanCmdDataResponse.aucMulticastAddress,
			sizeof(rNanCmdDataResponse.aucMulticastAddress),
			nla_data(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_MULTICAST_ADDR]),
			MAC_ADDR_LEN);
	}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	/* Send data response */
	rStatus =  kalIoctl(prGlueInfo,
		nanOidDataResponse,
		&rNanCmdDataResponse,
		sizeof(struct _NAN_CMD_DATA_RESPONSE),
		&u4BufLen);

	/* Return */
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Handle NDP end vendor cmd.
 *
 * \param[in] prGlueInfo: Pointer to glue info structure.
 *
 * \param[in] tb: NDP vendor cmd attributes.
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/
int32_t nanNdpEndReqHandler(struct GLUE_INFO *prGlueInfo, struct nlattr **tb)
{
	struct _NAN_CMD_DATA_END rNanCmdDataEnd;
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t instanceIdNum;
	uint32_t i;

	kalMemZero(&rNanCmdDataEnd, sizeof(rNanCmdDataEnd));

	/* trial run! */
	DBGLOG(NAN, DEBUG, "NDP end request\n");

	if (!tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY]) {
		DBGLOG(NAN, ERROR, "Get NDP Instance ID unavailable!\n");
		return -EINVAL;
	}
	instanceIdNum =
		nla_len(tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY]) /
		sizeof(uint32_t);
	if (instanceIdNum <= 0) {
		DBGLOG(NAN, ERROR, "No NDP Instance ID!\n");
		return -EINVAL;
	}

	/* Get transaction ID */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		rNanCmdDataEnd.u2NdpTransactionId = nla_get_u32(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);
		DBGLOG(NAN, ERROR, "Get NDP Transaction ID =%d\n",
			rNanCmdDataEnd.u2NdpTransactionId);
	}

	for (i = 0; i < instanceIdNum; i++) {
		uint32_t u4BufLen;

		if (nanGetFeatureIsSigma(prGlueInfo->prAdapter))
			rNanCmdDataEnd.ucNDPId = nla_get_u32(
				tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY] +
				i * sizeof(uint32_t));
		else
			rNanCmdDataEnd.ndp_instance_id = nla_get_u32(
				tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY] +
				i * sizeof(uint32_t));
		rStatus =  kalIoctl(prGlueInfo,
			nanOidEndReq,
			&rNanCmdDataEnd,
			sizeof(struct _NAN_CMD_DATA_END),
			&u4BufLen);
	}
	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Send data indication event to hal.
 *
 * \param[in] prNDP: NDP info attribute
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/

uint32_t
nanNdpDataIndEvent(struct ADAPTER *prAdapter,
		   struct _NAN_NDP_INSTANCE_T *prNDP,
		   struct _NAN_NDL_INSTANCE_T *prNDL) {
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint16_t u2IndiEventLen;
	uint32_t u4Id = 0;

	if (prNDP == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDP is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	if (prNDL == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDL is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDP Data Indication event\n");

	if (prAdapter->rNanNetRegState == ENUM_NET_REG_STATE_UNREGISTERED) {
		DBGLOG(NAN, ERROR, "Net device for NAN unregistered\n");
		return WLAN_STATUS_FAILURE;
	}

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2IndiEventLen = (3 * sizeof(uint32_t)) + (2 * MAC_ADDR_LEN) +
			 prNDP->u2AppInfoLen + NAN_SCID_DEFAULT_LEN +
			 (6 * NLA_HDRLEN) + NLMSG_HDRLEN;

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2IndiEventLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_REQUEST_IND) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u32(skb,
				 MTK_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID,
				 prNDP->ucPublishId) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR,
			     MAC_ADDR_LEN, prNDP->aucPeerNDIAddr) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}
	COPY_MAC_ADDR(g_InitiatorMacAddr, prNDP->aucPeerNDIAddr);
	DBGLOG(NAN, DEBUG, "[%s] gInitiatorMacAddr = " MACSTR "\n", __func__,
	       MAC2STR(g_InitiatorMacAddr));

	if (unlikely(nla_put(skb,
			     MTK_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR,
			     MAC_ADDR_LEN, prNDL->aucPeerMacAddr) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (nanGetFeatureIsSigma(prAdapter))
		u4Id = prNDP->ucNDPID;
	else
		u4Id = prNDP->ndp_instance_id;

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
				 u4Id) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (prNDP->u2PeerAppInfoLen) {
		if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO,
				     prNDP->u2PeerAppInfoLen,
				     prNDP->pucPeerAppInfo) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	}

/* Todo:
 * 1. QoS currently not support.
 * 2. Need to clarify security parameter
 */
#if 0
	if (prNDP->fgQoSRequired)
		continue;

	if (prNDP->fgSecurityRequired) {
		if (unlikely(nla_put_u32(skb,
					MTK_WLAN_VENDOR_ATTR_NDP_NCS_SK_TYPE,
					prNDP->ucCipherType) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
		if (unlikely(nla_put(skb,
				     MTK_WLAN_VENDOR_ATTR_NDP_SCID,
				     NAN_SCID_DEFAULT_LEN,
				     prNDP->au1Scid) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
	}
#endif
	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Send data confirm event to hal.
 *
 * \param[in] prNDP: NDP info attribute
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/

uint32_t
nanNdpDataConfirmEvent(struct ADAPTER *prAdapter,
		       struct _NAN_NDP_INSTANCE_T *prNDP) {
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint32_t u2ConfirmEventLen;
	uint32_t u4Id = 0;
	struct NanChannelInfo
		info[NAN_MAX_CHANNEL_INFO_SUPPORTED];
	uint32_t num_info = 0;
	struct nlattr *array, *item;

	if (prNDP == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDP is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDP Data Confirm event\n");

	if (prAdapter->rNanNetRegState == ENUM_NET_REG_STATE_UNREGISTERED) {
		DBGLOG(NAN, ERROR, "Net device for NAN unregistered\n");
		return WLAN_STATUS_FAILURE;
	}

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2ConfirmEventLen = (4 * sizeof(uint32_t)) + MAC_ADDR_LEN +
			    +NLMSG_HDRLEN + (6 * NLA_HDRLEN) +
			    prNDP->u2AppInfoLen;
	/* WIFI_EVENT_SUBCMD_NDP: Event Idx is 13 for kernel,
	 *  but for WifiHal is 81
	 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2ConfirmEventLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (nanGetFeatureIsSigma(prAdapter))
		u4Id = prNDP->ucNDPID;
	else
		u4Id = prNDP->ndp_instance_id;

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
				 u4Id) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR,
			     MAC_ADDR_LEN, prNDP->aucPeerNDIAddr) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (prNDP->fgCarryIPV6 && unlikely(nla_put(skb,
		MTK_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR,
		IPV6MACLEN, prNDP->aucRspInterfaceId) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (prNDP->fgCarryIPV6)
		DBGLOG(NAN, DEBUG, "[%s] fgCarryIPV6 = " IPV6STR "\n",
		__func__, IPV6TOSTR(prNDP->aucRspInterfaceId));

	if (prNDP->pucPeerAppInfo &&
	    unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO,
	    prNDP->u2PeerAppInfoLen,
		    prNDP->pucPeerAppInfo) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (prNDP->pucPeerAppInfo)
		DBGLOG(NAN, DEBUG, "[%s] u2PeerAppInfoLen = %d\n", __func__,
		prNDP->u2PeerAppInfoLen);

#ifdef NAN_TODO
	if (prNDP->pucAttrList) {
		if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_APP_INFO,
				prNDP->u2AttrListLength,
				prNDP->pucAttrList) < 0)) {
			DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
			kfree_skb(skb);
			return WLAN_STATUS_INVALID_DATA;
		}
	}

	if (prNDP->pucAttrList)
		DBGLOG(NAN, INFO, "u2PeerAppInfoLen = %d\n",
			prNDP->u2AttrListLength);
#endif

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE,
				 prNDP->ucReasonCode) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
				 prNDP->eDataPathFailReason) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	if (!prAdapter->rWifiVar.ucNanReportChInfo)
		goto SKIP_REPORT_CHANNEL_INFO;

	nanGetChannelInfo(prAdapter,
		prNDP, info, &num_info);

	if (num_info) {
		uint32_t i = 0;

		if (unlikely(nla_put_u32(skb,
			MTK_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS,
			num_info) < 0))
			goto SKIP_REPORT_CHANNEL_INFO;

		array = nla_nest_start(skb,
			MTK_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO);
		if (!array)
			goto SKIP_REPORT_CHANNEL_INFO;

		for (i = 0; i < num_info; i++) {
			item = nla_nest_start(skb, i);
			if (!item)
				goto SKIP_REPORT_CHANNEL_INFO;

			if (unlikely(nla_put_u32(skb,
				MTK_WLAN_VENDOR_ATTR_NDP_CHANNEL,
				info[i].channel) < 0))
				goto SKIP_REPORT_CHANNEL_INFO;

			if (unlikely(nla_put_u32(skb,
				MTK_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH,
				info[i].bandwidth) < 0))
				goto SKIP_REPORT_CHANNEL_INFO;

			if (unlikely(nla_put_u32(skb,
				MTK_WLAN_VENDOR_ATTR_NDP_NSS,
				info[i].nss) < 0))
				goto SKIP_REPORT_CHANNEL_INFO;

			nla_nest_end(skb, item);
		}
		nla_nest_end(skb, array);
	}

SKIP_REPORT_CHANNEL_INFO:

	DBGLOG(NAN, DEBUG, "NDP Data Confirm event, ndp instance: %d,",
		u4Id);
	DBGLOG(NAN, DEBUG, "peer MAC addr : "MACSTR "rsp reason code: %d,",
		MAC2STR(prNDP->aucPeerNDIAddr), prNDP->ucReasonCode);
	DBGLOG(NAN, DEBUG, "protocol reason code: %d\n ",
		prNDP->eDataPathFailReason);

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Send data termination event to hal.
 *
 * \param[in] prNDP: NDP info attribute
 *
 * \return WLAN_STATUS
 */
/*----------------------------------------------------------------------------*/

uint32_t
nanNdpDataTerminationEvent(struct ADAPTER *prAdapter,
			   struct _NAN_NDP_INSTANCE_T *prNDP) {
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDevHandler = NULL;
	uint32_t u2ConfirmEventLen;
	uint32_t *pu2NDPInstance;
	uint32_t u4Id = 0;

	if (prNDP == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prNDP is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	pu2NDPInstance = kalMemAlloc(1 * sizeof(*pu2NDPInstance), VIR_MEM_TYPE);
	if (pu2NDPInstance == NULL) {
		DBGLOG(NAN, ERROR, "[%s] pu2NDPInstance is NULL\n", __func__);
		return WLAN_STATUS_INVALID_DATA;
	}

	DBGLOG(NAN, DEBUG, "Send NDP Data Termination event\n");

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDevHandler = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDevHandler) {
		DBGLOG(INIT, ERROR, "prDevHandler is NULL\n");
		return -EFAULT;
	}
	wdev = prDevHandler->ieee80211_ptr;
	u2ConfirmEventLen = sizeof(uint32_t) + NLMSG_HDRLEN + (2 * NLA_HDRLEN) +
			    1 * sizeof(*pu2NDPInstance);

	skb = kalCfg80211VendorEventAlloc(wiphy, wdev, u2ConfirmEventLen,
					  WIFI_EVENT_SUBCMD_NDP, GFP_KERNEL);
	if (!skb) {
		DBGLOG(REQ, ERROR, "Allocate skb failed\n");
		kalMemFree(pu2NDPInstance,
				   VIR_MEM_TYPE,
				   1 * sizeof(*pu2NDPInstance));
		return -ENOMEM;
	}

	if (unlikely(nla_put_u32(skb, MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD,
				 MTK_WLAN_VENDOR_ATTR_NDP_END_IND) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kalMemFree(pu2NDPInstance,
				   VIR_MEM_TYPE,
				   1 * sizeof(*pu2NDPInstance));
		kfree_skb(skb);
		return -EFAULT;
	}

	if (nanGetFeatureIsSigma(prAdapter))
		u4Id = prNDP->ucNDPID;
	else
		u4Id = prNDP->ndp_instance_id;
	*pu2NDPInstance = (uint32_t)u4Id;

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY,
			     1 * sizeof(*pu2NDPInstance),
			     pu2NDPInstance) < 0)) {
		DBGLOG(REQ, ERROR, "nla_put_nohdr failed\n");
		kalMemFree(pu2NDPInstance,
				   VIR_MEM_TYPE,
				   1 * sizeof(*pu2NDPInstance));
		kfree_skb(skb);
		return -EFAULT;
	}

	DBGLOG(NAN, DEBUG, "NDP Data Termination event, ndp instance: %d\n",
	       u4Id);

	cfg80211_vendor_event(skb, GFP_KERNEL);

	kalMemFree(pu2NDPInstance,
			   VIR_MEM_TYPE,
			   1 * sizeof(*pu2NDPInstance));
	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is the enter point of NDP vendor cmd.
 *
 * \param[in] wiphy: for AIS STA
 *
 * \param[in] wdev (not used here).
 *
 * \param[in] data: Content of NDP vendor cmd .
 *
 * \param[in] data_len: NDP vendor cmd length.
 *
 * \return int
 */
/*----------------------------------------------------------------------------*/
int mtk_cfg80211_vendor_ndp(struct wiphy *wiphy, struct wireless_dev *wdev,
			const void *data, int data_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct nlattr *tb[MTK_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];
	uint32_t u4NdpCmdType;
	int32_t rStatus;

	if (wiphy == NULL) {
		DBGLOG(NAN, ERROR, "[%s] wiphy is NULL\n", __func__);
		return -EINVAL;
	}

	if (wdev == NULL) {
		DBGLOG(NAN, ERROR, "[%s] wdev is NULL\n", __func__);
		return -EINVAL;
	}

	if (data == NULL || data_len <= 0) {
		log_dbg(REQ, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(NAN, ERROR, "[%s] prGlueInfo is NULL\n", __func__);
		return -EINVAL;
	}

	if (prGlueInfo->prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "prAdapter is null\n");
		return -EINVAL;
	}

	if (!prGlueInfo->prAdapter->fgIsNANRegistered) {
		DBGLOG(NAN, ERROR, "NAN Not Register yet!\n");
		return -EINVAL;
	}

	DBGLOG(NAN, TRACE, "DATA len from user %d\n", data_len);

	/* Parse NDP vendor cmd */
	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX, data, data_len,
		      mtk_wlan_vendor_ndp_policy)) {
		DBGLOG(NAN, ERROR, "Parse NDP cmd fail!\n");
		return -EINVAL;
	}

	/* Get NDP command type*/
	if (!tb[MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD]) {
		DBGLOG(NAN, ERROR, "Get NDP cmd type error!\n");
		return -EINVAL;
	}
	u4NdpCmdType = nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD]);

	switch (u4NdpCmdType) {
	/* Command to create a NAN data path interface */
	case MTK_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE:
		rStatus = nanNdiCreateHandler(prGlueInfo, tb);
		break;
	/* Command to delete a NAN data path interface */
	case MTK_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE:
		rStatus = nanNdiDeleteHandler(prGlueInfo, tb);
		break;
	/* Command to initiate a NAN data path session */
	case MTK_WLAN_VENDOR_ATTR_NDP_INITIATOR_REQUEST:
		rStatus = nanNdpInitiatorReqHandler(prGlueInfo, tb);
		break;
	/* Command to respond to NAN data path session */
	case MTK_WLAN_VENDOR_ATTR_NDP_RESPONDER_REQUEST:
		rStatus = nanNdpResponderReqHandler(prGlueInfo, tb);
		break;
	/* Command to initiate a NAN data path end */
	case MTK_WLAN_VENDOR_ATTR_NDP_END_REQUEST:
		rStatus = nanNdpEndReqHandler(prGlueInfo, tb);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return rStatus;
}

int32_t nanNdpPsModeHandler(
	struct GLUE_INFO *prGlueInfo, struct nlattr **tb)
{
	uint32_t u4NdpTransactionId = 0;
	uint32_t u4InstanceId = 0;
	uint32_t u4Tput = 0;
	uint32_t u4Latency = 0;
	uint16_t u2UpdateConfig = 0;
	struct _NAN_CMD_DATA_POWERSAVE rNanDataPowerSaveCmd = {0};
	int32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen;

	if (prGlueInfo == NULL) {
		DBGLOG(NAN, ERROR, "prGlueInfo is NULL\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	/* TODO: customer use uint16_t for transaction ID? */
	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		rNanDataPowerSaveCmd.u4NdpTransactionId = nla_get_u32(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);
		DBGLOG(NAN, INFO, "Get NDP Transaction ID =%d\n",
			u4NdpTransactionId);
	}

	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_UPDATE_CONFIG]) {
		rNanDataPowerSaveCmd.u2UpdateConfig = nla_get_u16(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_UPDATE_CONFIG]);
		DBGLOG(NAN, INFO, "Get update config =%d\n", u2UpdateConfig);
	}

	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]) {
		rNanDataPowerSaveCmd.u4InstanceId = nla_get_u32(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]);
		DBGLOG(NAN, INFO, "Get NDP Instance ID =%d\n", u4InstanceId);
	}

	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_LATENCY]) { /* ms */
		rNanDataPowerSaveCmd.u4Latency = nla_get_u32(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_LATENCY]);
		DBGLOG(NAN, INFO, "Get latency =%d\n", u4Latency);
	}

	if (tb[MTK_WLAN_VENDOR_ATTR_NDP_TPUT]) {
		rNanDataPowerSaveCmd.u4Tput = nla_get_u32(
			tb[MTK_WLAN_VENDOR_ATTR_NDP_TPUT]);
		DBGLOG(NAN, INFO, "Get Tput =%d\n", u4Tput);
	}

	rStatus =  kalIoctl(prGlueInfo, nanOidPsMode, &rNanDataPowerSaveCmd,
			    sizeof(struct _NAN_CMD_DATA_POWERSAVE),
			    &u4BufLen);

	return rStatus;
}

int mtk_cfg80211_vendor_ndp_qca(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *data,
	int data_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct nlattr *tb[MTK_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];
	uint32_t u4NdpCmdType;
	int32_t rStatus;

	if (wiphy == NULL) {
		DBGLOG(NAN, ERROR, "wiphy is NULL\n");
		return -EINVAL;
	}

	if (wdev == NULL) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -EINVAL;
	}

	if (data == NULL || data_len <= 0) {
		log_dbg(REQ, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (prGlueInfo == NULL) {
		DBGLOG(NAN, ERROR, "prGlueInfo is NULL\n");
		return -EINVAL;
	}

	if (prGlueInfo->prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "prAdapter is null\n");
		return -EINVAL;
	}

	if (!prGlueInfo->prAdapter->fgIsNANRegistered) {
		DBGLOG(NAN, ERROR, "NAN Not Register yet!\n");
		return -EINVAL;
	}

	DBGLOG(NAN, TRACE, "DATA len from user %d\n", data_len);

	/* Parse NDP vendor cmd */
	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX, data, data_len,
		      mtk_wlan_vendor_ndp_policy)) {
		DBGLOG(NAN, ERROR, "Parse NDP cmd fail!\n");
		return -EINVAL;
	}

	/* Get NDP command type*/
	if (!tb[MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD]) {
		DBGLOG(NAN, ERROR, "Get NDP cmd type error!\n");
		return -EINVAL;
	}
	u4NdpCmdType = nla_get_u32(tb[MTK_WLAN_VENDOR_ATTR_NDP_SUBCMD]);

	switch (u4NdpCmdType) {
	case MTK_WLAN_VENDOR_ATTR_NDP_POWERSAVE_MODE:
		rStatus = nanNdpPsModeHandler(prGlueInfo, tb);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return rStatus;
}

#endif /* CFG_SUPPORT_NAN */
