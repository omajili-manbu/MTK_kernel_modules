// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*
 * gl_vendor_nan.c
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
#include "gl_cfg80211.h"
#include "gl_os.h"
#include "debug.h"
#include "gl_vendor.h"
#include "gl_wext.h"
#include "wlan_lib.h"
#include "wlan_oid.h"
#include <linux/can/netlink.h>
#include <net/cfg80211.h>
#include <net/netlink.h>

#if CFG_SUPPORT_NAN_R4_PAIRING
#include "nan_pairing.h"
#include <linux/delay.h>
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
#include "nan_sec.h"
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

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
#if CFG_SUPPORT_NAN_R4_PAIRING
/* NAN pairing uses many test commands to compose one request for sigma */
struct NanPairingPubReq g_pairingPubReq;
struct NanBootstrap g_bootstrapMethod;
struct NanPairingSetupCmd g_pairingSetupCmd;
struct NanNikExchange g_nikExchange;
struct NanBootstrapPassword g_bootstrapPassword;

bool sigma_nik_exchange_run;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
uint8_t g_enableNAN = TRUE;
uint8_t g_disableNAN = FALSE;
uint8_t g_deEvent;
uint8_t g_aucNanServiceName[NAN_MAX_SERVICE_NAME_LEN];
uint8_t g_aucNanServiceId[6];
uint8_t g_ucNanLowPowerMode = FALSE;

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

void hexdump_nan(void *buf, u16 len)
{
	int i = 0;
	char *bytes = (char *) buf;
	uint64_t addr;
	int remain;
	char remain_buf[16+2+3*4+2+3*3+1];
	int result;

	if (len) {
		pr_info("HEXDUMP: buf=%p, len=%d, len=%d * 8 + %d,",
		buf, len, len / 8, len % 8);
		pr_info("++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		addr = (uint64_t) buf;
		for (i = 0; ((i + 7) < len); i += 8) {
			pr_info("| %02x %02x %02x %02x %02x %02x %02x %02x\n",
			(int) bytes[i],   (int) bytes[i+1],
			(int) bytes[i+2], (int) bytes[i+3],
			(int) bytes[i+4], (int) bytes[i+5],
			(int) bytes[i+6], (int) bytes[i+7]);
		}
		remain = len - i;
		if (remain > 0) {
			memset(remain_buf, 0, sizeof(remain_buf));
			result = snprintf(remain_buf, sizeof(remain_buf),
				"| %02x %02x %02x %02x %02x %02x %02x\n",
				(int) bytes[i],
				remain > 1 ? (int) bytes[i+1] : 0,
				remain > 2 ? (int) bytes[i+2] : 0,
				remain > 3 ? (int) bytes[i+3] : 0,
				remain > 4 ? (int) bytes[i+4] : 0,
				remain > 5 ? (int) bytes[i+5] : 0,
				remain > 6 ? (int) bytes[i+6] : 0);
				remain_buf[1 + 3 * remain] = 0;
			if (result >= 0)
				pr_info("%s", remain_buf);
		}
		pr_info("--------------------------------------------------\n");
	} else {
		return;
	}
}

void
nanAbortOngoingScan(struct ADAPTER *prAdapter)
{
	struct SCAN_INFO *prScanInfo;

	if (!prAdapter)
		return;

	prScanInfo = &(prAdapter->rWifiVar.rScanInfo);

	if (!prScanInfo || (prScanInfo->eCurrentState != SCAN_STATE_SCANNING))
		return;

	if (IS_BSS_INDEX_AIS(prAdapter, prScanInfo->rScanParam.ucBssIndex))
		aisFsmStateAbort_SCAN(prAdapter,
			prScanInfo->rScanParam.ucBssIndex);
	else if (prScanInfo->rScanParam.ucBssIndex ==
			prAdapter->ucP2PDevBssIdx)
		p2pDevFsmRunEventScanAbort(prAdapter,
			prAdapter->ucP2PDevBssIdx);
}

uint32_t nanOidAbortOngoingScan(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_FAILURE;
	}

	nanAbortOngoingScan(prAdapter);

	DBGLOG(NAN, TRACE, "After\n");

	return WLAN_STATUS_SUCCESS;
}

void
nanNdpAbortScan(struct ADAPTER *prAdapter)
{
	uint32_t rStatus = 0;
	uint32_t u4SetInfoLen = 0;

	if (!prAdapter->rWifiVar.fgNanOnAbortScan)
		return;

	rStatus = kalIoctl(
		prAdapter->prGlueInfo,
		nanOidAbortOngoingScan, NULL, 0,
		&u4SetInfoLen);
}

uint32_t nanOidDissolveReq(
	struct ADAPTER *prAdapter,
	void *pvSetBuffer,
	uint32_t u4SetBufferLen,
	uint32_t *pu4SetInfoLen)
{
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;
	struct _NAN_NDP_INSTANCE_T *prNDP;
	uint8_t i, j;
	u_int8_t found = FALSE;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(NAN, TRACE, "Before\n");

	prDataPathInfo = &(prAdapter->rDataPathInfo);

	for (i = 0; i < NAN_MAX_SUPPORT_NDL_NUM; i++) {
		if (!prDataPathInfo->arNDL[i].fgNDLValid)
			continue;

		for (j = 0; j < NAN_MAX_SUPPORT_NDP_NUM; j++) {
			prNDP = &prDataPathInfo->arNDL[i].arNDP[j];

			if (prNDP->eCurrentNDPProtocolState !=
				NDP_NORMAL_TR)
				continue;
			prNDP->eLastNDPProtocolState =
				NDP_NORMAL_TR;
			prNDP->eCurrentNDPProtocolState =
				NDP_TX_DP_TERMINATION;
			nanNdpUpdateTypeStatus(prAdapter, prNDP);
			nanNdpSendDataPathTermination(prAdapter,
				prNDP);

			found = TRUE;
		}

		if (i >= prAdapter->rWifiVar.ucNanMaxNdpDissolve)
			break;
	}

	/* Make the frame send to FW ASAP. */
#if !CFG_SUPPORT_MULTITHREAD
	ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter,
		DRV_OWN_SRC_NAN_REQ);
#endif
	wlanProcessCommandQueue(prAdapter,
		&prAdapter->prGlueInfo->rCmdQueue);
#if !CFG_SUPPORT_MULTITHREAD
	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE, DRV_OWN_SRC_NAN_REQ);
#endif

	if (!found) {
		DBGLOG(NAN, TRACE,
			"Dissolve: Complete NAN\n");
		complete(&prAdapter->prGlueInfo->rNanDissolveComp);
	}

	DBGLOG(NAN, TRACE, "After\n");

	return WLAN_STATUS_SUCCESS;
}

void
nanNdpDissolve(struct ADAPTER *prAdapter,
	uint32_t u4Timeout)
{
	uint32_t waitRet = 0;
	uint32_t rStatus = 0;
	uint32_t u4SetInfoLen = 0;

	reinit_completion(&prAdapter->prGlueInfo->rNanDissolveComp);

	if (prAdapter->rWifiVar.fgNanDissolveAbortScan)
		rStatus = kalIoctl(
			prAdapter->prGlueInfo,
			nanOidAbortOngoingScan, NULL, 0,
			&u4SetInfoLen);

	rStatus = kalIoctl(
		prAdapter->prGlueInfo,
		nanOidDissolveReq, NULL, 0,
		&u4SetInfoLen);

	waitRet = wait_for_completion_timeout(
		&prAdapter->prGlueInfo->rNanDissolveComp,
		MSEC_TO_JIFFIES(
		u4Timeout));
	if (!waitRet)
		DBGLOG(NAN, TRACE, "Disconnect timeout.\n");
	else
		DBGLOG(NAN, TRACE, "Disconnect complete.\n");
}

/* Helper function to Write and Read TLV called in indication as well as
 * request
 */
u16
nanWriteTlv(struct _NanTlv *pInTlv, u8 *pOutTlv)
{
	u16 writeLen = 0;
	u16 i;

	if (!pInTlv) {
		DBGLOG(NAN, ERROR, "NULL pInTlv\n");
		return writeLen;
	}

	if (!pOutTlv) {
		DBGLOG(NAN, ERROR, "NULL pOutTlv\n");
		return writeLen;
	}

	*pOutTlv++ = pInTlv->type & 0xFF;
	*pOutTlv++ = (pInTlv->type & 0xFF00) >> 8;
	writeLen += 2;

	DBGLOG(NAN, LOUD, "Write TLV type %u, writeLen %u\n", pInTlv->type,
	       writeLen);

	*pOutTlv++ = pInTlv->length & 0xFF;
	*pOutTlv++ = (pInTlv->length & 0xFF00) >> 8;
	writeLen += 2;

	DBGLOG(NAN, LOUD, "Write TLV length %u, writeLen %u\n", pInTlv->length,
	       writeLen);

	for (i = 0; i < pInTlv->length; ++i)
		*pOutTlv++ = pInTlv->value[i];

	writeLen += pInTlv->length;
	DBGLOG(NAN, LOUD, "Write TLV value, writeLen %u\n", writeLen);
	return writeLen;
}

u16
nan_read_tlv(u8 *pInTlv, struct _NanTlv *pOutTlv)
{
	u16 readLen = 0;

	if (!pInTlv)
		return readLen;

	if (!pOutTlv)
		return readLen;

	pOutTlv->type = *pInTlv++;
	pOutTlv->type |= *pInTlv++ << 8;
	readLen += 2;

	pOutTlv->length = *pInTlv++;
	pOutTlv->length |= *pInTlv++ << 8;
	readLen += 2;

	if (pOutTlv->length) {
		pOutTlv->value = pInTlv;
		readLen += pOutTlv->length;
	} else {
		pOutTlv->value = NULL;
	}
	return readLen;
}

u8 *
nanAddTlv(u16 type, u16 length, u8 *value, u8 *pOutTlv)
{
	struct _NanTlv nanTlv;
	u16 len;

	nanTlv.type = type;
	nanTlv.length = length;
	nanTlv.value = (u8 *)value;

	len = nanWriteTlv(&nanTlv, pOutTlv);
	return (pOutTlv + len);
}

u16
nanMapPublishReqParams(u16 *pIndata, struct NanPublishRequest *pOutparams)
{
	u16 readLen = 0;
	u32 *pPublishParams = NULL;

	DBGLOG(NAN, DEBUG, "Enter\n");

	/* Get value of ttl(time to live) */
	pOutparams->ttl = *pIndata;

	/* Get value of ttl(time to live) */
	pIndata++;
	readLen += 2;

	/* Get value of period */
	pOutparams->period = *pIndata;

	/* Assign default value */
	if (pOutparams->period == 0)
		pOutparams->period = 1;

	pIndata++;
	readLen += 2;

	pPublishParams = (u32 *)pIndata;
	dumpMemory32(pPublishParams, 4);
	pOutparams->recv_indication_cfg =
		(u8)(GET_PUB_REPLY_IND_FLAG(*pPublishParams) |
		     GET_PUB_FOLLOWUP_RX_IND_DISABLE_FLAG(*pPublishParams) |
		     GET_PUB_MATCH_EXPIRED_IND_DISABLE_FLAG(*pPublishParams) |
		     GET_PUB_TERMINATED_IND_DISABLE_FLAG(*pPublishParams));
	pOutparams->publish_type = GET_PUB_PUBLISH_TYPE(*pPublishParams);
	pOutparams->tx_type = GET_PUB_TX_TYPE(*pPublishParams);
	pOutparams->rssi_threshold_flag =
		(u8)GET_PUB_RSSI_THRESHOLD_FLAG(*pPublishParams);
	pOutparams->publish_match_indicator =
		GET_PUB_MATCH_ALG(*pPublishParams);
	pOutparams->publish_count = (u8)GET_PUB_COUNT(*pPublishParams);
	pOutparams->connmap = (u8)GET_PUB_CONNMAP(*pPublishParams);
	readLen += 4;

	DBGLOG(NAN, INFO,
	       "[Publish Req] ttl: %u, period: %u, recv_indication_cfg: %x, publish_type: %u,tx_type: %u, rssi_threshold_flag: %u, publish_match_indicator: %u, publish_count:%u, connmap:%u, readLen:%u\n",
	       pOutparams->ttl, pOutparams->period,
	       pOutparams->recv_indication_cfg, pOutparams->publish_type,
	       pOutparams->tx_type, pOutparams->rssi_threshold_flag,
	       pOutparams->publish_match_indicator, pOutparams->publish_count,
	       pOutparams->connmap, readLen);

	return readLen;
}

u16
nanMapSubscribeReqParams(u16 *pIndata, struct NanSubscribeRequest *pOutparams)
{
	u16 readLen = 0;
	u32 *pSubscribeParams = NULL;

	DBGLOG(NAN, TRACE, "Enter\n");

	pOutparams->ttl = *pIndata;
	pIndata++;
	readLen += 2;

	pOutparams->period = *pIndata;
	pIndata++;
	readLen += 2;

	pSubscribeParams = (u32 *)pIndata;

	pOutparams->subscribe_type = GET_SUB_SUBSCRIBE_TYPE(*pSubscribeParams);
	pOutparams->serviceResponseFilter = GET_SUB_SRF_ATTR(*pSubscribeParams);
	pOutparams->serviceResponseInclude =
		GET_SUB_SRF_INCLUDE(*pSubscribeParams);
	pOutparams->useServiceResponseFilter =
		GET_SUB_SRF_SEND(*pSubscribeParams);
	pOutparams->ssiRequiredForMatchIndication =
		GET_SUB_SSI_REQUIRED(*pSubscribeParams);
	pOutparams->subscribe_match_indicator =
		GET_SUB_MATCH_ALG(*pSubscribeParams);
	pOutparams->subscribe_count = (u8)GET_SUB_COUNT(*pSubscribeParams);
	pOutparams->rssi_threshold_flag =
		(u8)GET_SUB_RSSI_THRESHOLD_FLAG(*pSubscribeParams);
	pOutparams->recv_indication_cfg =
		(u8)GET_SUB_FOLLOWUP_RX_IND_DISABLE_FLAG(*pSubscribeParams) |
		GET_SUB_MATCH_EXPIRED_IND_DISABLE_FLAG(*pSubscribeParams) |
		GET_SUB_TERMINATED_IND_DISABLE_FLAG(*pSubscribeParams);

	DBGLOG(NAN, INFO,
	       "[Subscribe Req] ttl: %u, period: %u, subscribe_type: %u, ssiRequiredForMatchIndication: %u, subscribe_match_indicator: %x, rssi_threshold_flag: %u\n",
	       pOutparams->ttl, pOutparams->period,
	       pOutparams->subscribe_type,
	       pOutparams->ssiRequiredForMatchIndication,
	       pOutparams->subscribe_match_indicator,
	       pOutparams->rssi_threshold_flag);
	pOutparams->connmap = (u8)GET_SUB_CONNMAP(*pSubscribeParams);
	readLen += 4;
	DBGLOG(NAN, LOUD, "Subscribe readLen : %d\n", readLen);
	return readLen;
}

u16
nanMapFollowupReqParams(u32 *pIndata,
			struct NanTransmitFollowupRequest *pOutparams)
{
	u16 readLen = 0;
	u32 *pXmitFollowupParams = NULL;

	pOutparams->requestor_instance_id = *pIndata;
	pIndata++;
	readLen += 4;

	pXmitFollowupParams = pIndata;

	pOutparams->priority = GET_FLWUP_PRIORITY(*pXmitFollowupParams);
	pOutparams->dw_or_faw = GET_FLWUP_WINDOW(*pXmitFollowupParams);
	pOutparams->recv_indication_cfg =
		GET_FLWUP_TX_RSP_DISABLE_FLAG(*pXmitFollowupParams);
	readLen += 4;

	DBGLOG(NAN, DEBUG,
	       "priority:%u, dw_or_faw: %u, recv_indication_cfg: %u\n",
	       pOutparams->priority, pOutparams->dw_or_faw,
	       pOutparams->recv_indication_cfg);

	return readLen;
}

#if CFG_SUPPORT_NAN_R4_PAIRING
u16
nanMapPairingRequestParams(u8 *pIndata,
			struct _NanPairingRequestParams *pOutparams)
{
	u16 readLen = sizeof(struct _NanPairingRequestParams);

	memcpy(pOutparams, pIndata, sizeof(struct _NanPairingRequestParams));
	return readLen;
}

u16
nanMapPairingResponseParams(u8 *pIndata,
			struct _NanPairingResponseParams *pOutparams)
{
	u16 readLen;

	readLen = sizeof(struct _NanPairingResponseParams);
	memcpy(pOutparams, pIndata, sizeof(struct _NanPairingResponseParams));
	return readLen;
}

#if TEST_FIXED_NIK
u8 test_dummy_nik[NAN_NIK_LEN] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
#endif /* TEST_FIXED_NIK */
#if TEST_FIXED_IGTK_BIGTK
u8 test_dummy_igtk[NAN_IGTK_LEN] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
	};
u8 test_dummy_bigtk[NAN_IGTK_LEN] = {
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
	};
#endif /* TEST_FIXED_IGTK_BIGTK */

uint32_t
nanCommandPairingRequest_Pairing(struct ADAPTER *prAdapter,
	struct _NanPairingRequestParams *pairingRequest)
{
	u8 passphrase[NAN_SECURITY_MAX_PASSPHRASE_LEN];
	u16 password_length = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;

	memset(passphrase, 0, NAN_SECURITY_MAX_PASSPHRASE_LEN);
	password_length = pairingRequest->key_len;
	if (password_length > NAN_SECURITY_MAX_PASSPHRASE_LEN)
		password_length = NAN_SECURITY_MAX_PASSPHRASE_LEN;
	memcpy(passphrase, pairingRequest->key_data, password_length);
	DBGLOG(NAN, INFO, "pairing param requestor_instance_id:%d\n",
			pairingRequest->requestor_instance_id);
	DBGLOG(NAN, INFO, "pairing param MAC address:" MACSTR "\n",
			MAC2STR(pairingRequest->peer_disc_mac_addr));
	DBGLOG(NAN, INFO, "pairing param pairing_request_type:%d\n",
			pairingRequest->nan_pairing_request_type);
	DBGLOG(NAN, INFO, "pairing param is_opportunistic:%d\n",
			pairingRequest->is_opportunistic);
	DBGLOG(NAN, INFO, "pairing param akm:%u\n",
			pairingRequest->akm);
	DBGLOG(NAN, INFO, "pairing param key_type=%u\n",
			pairingRequest->key_type);
	DBGLOG(NAN, INFO, "pairing param key_len=%u\n",
			pairingRequest->key_len);
	DBGLOG(NAN, INFO, "pairing param cipher_type:%u\n",
			pairingRequest->cipher_type);

	if (password_length > 0) {
		u8 password[NAN_SECURITY_MAX_PASSPHRASE_LEN+1] = {0};

		memcpy(password, passphrase, NAN_SECURITY_MAX_PASSPHRASE_LEN);
		DBGLOG(NAN, INFO,
			"[pairing-intf][initiator] pairing request passwd=%s\n",
			password);
	}

	DBGLOG(NAN, INFO, "Enter Beacon SDF Request.\n");

	/* save Local Nik(and akm) to peer PairingFsm.*/
	prPairingFsm = pairingFsmSearch(prAdapter,
			pairingRequest->peer_disc_mac_addr);
	if (prPairingFsm) {
		DBGLOG(NAN, INFO, "[pairing-intf][initiator] install Nik\n");
		hexdump_nan((void *)pairingRequest->nan_identity_key,
				sizeof(pairingRequest->nan_identity_key));
#if TEST_FIXED_NIK
		nanPairingInstallLocalNik(prAdapter, prPairingFsm,
				test_dummy_nik);
#else /* TEST_FIXED_NIK */
		nanPairingInstallLocalNik(prAdapter, prPairingFsm,
				pairingRequest->nan_identity_key);
#endif /* TEST_FIXED_NIK */
		prPairingFsm->akm = pairingRequest->akm;
#if TEST_FIXED_IGTK_BIGTK
		nanPairingInstallLocalIGtk(prAdapter, prPairingFsm,
				test_dummy_igtk);
		nanPairingInstallLocalBIGtk(prAdapter, prPairingFsm,
				test_dummy_bigtk);
#else
#endif
	} else {
		DBGLOG(NAN, ERROR,
			"[pairing-intf][initiator] install Nik Fail\n");
		rRetStatus = WLAN_STATUS_FAILURE;
		return rRetStatus;
	}

	hexdump_nan((void *)pairingRequest,
		sizeof(struct _NanPairingRequestParams));
	{
		/* 1. send pasn request to supplicant */
		struct NAN_PASN_START_PARAM PasnStartParam;

		memcpy(PasnStartParam.peer_mac_addr,
				pairingRequest->peer_disc_mac_addr,
				MAC_ADDR_LEN);
		PasnStartParam.nan_pairing_request_type =
			pairingRequest->nan_pairing_request_type;
		PasnStartParam.akm = pairingRequest->akm;
		PasnStartParam.cipher_type = pairingRequest->cipher_type;
		PasnStartParam.key_type = pairingRequest->key_type;
		PasnStartParam.key_len = pairingRequest->key_len;
		/* Copy password */
		memcpy((void *)PasnStartParam.key_data,
				pairingRequest->key_data,
				pairingRequest->key_len);

		rRetStatus =
			nanPasnRequestM1_Tx(prAdapter, &PasnStartParam);
	}
	return rRetStatus;
}

uint32_t
nanCommandPairingRequest_Verify(struct ADAPTER *prAdapter,
	struct _NanPairingRequestParams *pairingRequest,
	uint16_t publish_subscribe_id)
{
	u16 npk_length = NAN_NPK_LEN;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;

	/*hard code for Sigma*/
	if (nanGetFeatureIsSigma(prAdapter)) {
		pairingRequest->cipher_type =
			NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
	}

	npk_length = pairingRequest->key_len;
	if (npk_length > NAN_NPK_LEN)
		npk_length = NAN_NPK_LEN;
	DBGLOG(NAN, INFO, "pairing param requestor_instance_id:%d\n",
			pairingRequest->requestor_instance_id);
	DBGLOG(NAN, INFO, "pairing param MAC address:" MACSTR "\n",
			MAC2STR(pairingRequest->peer_disc_mac_addr));
	DBGLOG(NAN, INFO, "pairing param pairing_request_type:%d\n",
			pairingRequest->nan_pairing_request_type);
	DBGLOG(NAN, INFO, "pairing param is_opportunistic:%d\n",
			pairingRequest->is_opportunistic);
	DBGLOG(NAN, INFO, "pairing param akm:%u\n",
			pairingRequest->akm);
	DBGLOG(NAN, INFO, "pairing param key_type=%u\n",
			pairingRequest->key_type);
	DBGLOG(NAN, INFO, "pairing param key_len=%u\n",
			pairingRequest->key_len);
	DBGLOG(NAN, INFO, "pairing param cipher_type:%u\n",
			pairingRequest->cipher_type);

	if (npk_length > 0) {
		DBGLOG(NAN, INFO,
			"[pairing-intf][initiator] verification request NPK dump\n");
		hexdump_nan((void *)pairingRequest->key_data, npk_length);
	}


	/* save Local Nik(and akm) to peer PairingFsm.*/
	/* check if there is existing prPairingFsm created by other service
	 * or create PairingFsm here.
	 */
	prPairingFsm = pairingFsmSearch(prAdapter,
			pairingRequest->peer_disc_mac_addr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "prep verification request as Subscriber\n");
		DBGLOG(NAN, DEBUG, "Alloc new Pairing FSM\n");
		prPairingFsm = pairingFsmAlloc(prAdapter);
		if (prPairingFsm != NULL) {
			/* setup PairingFsm for verification */
			prPairingFsm->FsmMode =
				NAN_PAIRING_FSM_MODE_VERIFICATION;

			/* 2. NIK , NONCE, NIRA calculation  */
			memcpy((void *)prPairingFsm->aucNik,
				pairingRequest->nan_identity_key,
				NAN_IDENTITY_KEY_LEN);
			pairingDeriveNirNonce(prPairingFsm);
#if TEST_FIXED_NIK
			pairingDeriveNirTag(prAdapter, test_dummy_nik,
				NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				prPairingFsm->aucNonce,
				&prPairingFsm->u8Tag);
#else /* TEST_FIXED_NIK */
			pairingDeriveNirTag(prAdapter, prPairingFsm->aucNik,
				NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				prPairingFsm->aucNonce,
				&prPairingFsm->u8Tag);
#endif /* TEST_FIXED_NIK */

				/* FSM setup at subscriber*/
			pairingFsmSetup(prAdapter,
			prPairingFsm, NAN_BOOTSTRAPPING_HANDSHAKE_SKIP,
			TRUE, pairingRequest->requestor_instance_id,
			publish_subscribe_id, FALSE, 0);

			/* create sta record for verification */
			rRetStatus =
			nanPairingVerification_FsmFF(prAdapter, prPairingFsm,
				pairingRequest->peer_disc_mac_addr);

			if (rRetStatus == WLAN_STATUS_SUCCESS && prPairingFsm) {
				DBGLOG(NAN, INFO,
					"[pairing-intf][initiator] install Nik\n");
				hexdump_nan(
				(void *)pairingRequest->nan_identity_key,
				sizeof(pairingRequest->nan_identity_key));
#if TEST_FIXED_NIK
				nanPairingInstallLocalNik(prAdapter,
				prPairingFsm,
				test_dummy_nik);
#else /* TEST_FIXED_NIK */
				nanPairingInstallLocalNik(prAdapter,
				prPairingFsm,
				pairingRequest->nan_identity_key);
#endif /* TEST_FIXED_NIK */
#if TEST_FIXED_IGTK_BIGTK
				nanPairingInstallLocalIGtk(prAdapter,
				prPairingFsm,
				test_dummy_igtk);
				nanPairingInstallLocalBIGtk(prAdapter,
				prPairingFsm,
				test_dummy_bigtk);
#else
#endif /* TEST_FIXED_IGTK_BIGTK */
				prPairingFsm->akm = pairingRequest->akm;
			} else {
				if (rRetStatus != WLAN_STATUS_SUCCESS) {
				DBGLOG(NAN, ERROR,
					"Pairing verification FSM Setup Failed!\n");
				pairingFsmFree(prAdapter, prPairingFsm);
				return WLAN_STATUS_FAILURE;
				}
			}

		} else {
			DBGLOG(NAN, ERROR, "Pairing FSM Allocation Failed!\n");
			return WLAN_STATUS_FAILURE;
		}
	} else {
		DBGLOG(NAN, INFO, "prep verification request as Subscriber\n");
		DBGLOG(NAN, DEBUG,
			"Pairing FSM %p and reuse it\n", prPairingFsm);
	}

	hexdump_nan((void *)pairingRequest,
		sizeof(struct _NanPairingRequestParams));
	{
		/* 1. send pasn request to supplicant */
		struct NAN_PASN_START_PARAM PasnStartParam;

		memcpy(PasnStartParam.peer_mac_addr,
				pairingRequest->peer_disc_mac_addr,
				MAC_ADDR_LEN);
		PasnStartParam.nan_pairing_request_type =
			pairingRequest->nan_pairing_request_type;
		PasnStartParam.akm = pairingRequest->akm;
		PasnStartParam.cipher_type = pairingRequest->cipher_type;
		PasnStartParam.key_type = pairingRequest->key_type;
		PasnStartParam.key_len = pairingRequest->key_len;

		/* Copy NPK */
		memcpy((void *)PasnStartParam.key_data,
				pairingRequest->key_data,
				pairingRequest->key_len);

		rRetStatus =
			nanPasnRequestM1_Tx(prAdapter, &PasnStartParam);
	}
	return rRetStatus;
}

uint32_t
nanCommandPairingRespond_Pairing(struct ADAPTER *prAdapter,
	struct _NanPairingResponseParams *pairingResponse,
	const void **prdata, int remainingLen)
{
	u8 passphrase[NAN_SECURITY_MAX_PASSPHRASE_LEN];
	u16 password_length = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame = NULL;
	const void *data = *prdata;
	struct NanPairingPASNMsg pasnframe;
	struct _NanTlv outputTlv;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	u16 readLen = 0;

	memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
	memset(passphrase, 0, NAN_SECURITY_MAX_PASSPHRASE_LEN);
	password_length = pairingResponse->key_len;
	if (password_length > NAN_SECURITY_MAX_PASSPHRASE_LEN)
		password_length = NAN_SECURITY_MAX_PASSPHRASE_LEN;
	memcpy(passphrase, pairingResponse->key_data, password_length);
	DBGLOG(NAN, INFO, "pairing param pairing_instance_id:%d\n",
		pairingResponse->pairing_instance_id);
	DBGLOG(NAN, INFO, "pairing param nan_pairing_request_type:%d\n",
		pairingResponse->nan_pairing_request_type);
	DBGLOG(NAN, INFO, "pairing param rsp_code:%d\n",
		pairingResponse->rsp_code);
	DBGLOG(NAN, INFO, "pairing param is_opportunistic:%d\n",
		pairingResponse->is_opportunistic);
	DBGLOG(NAN, INFO, "pairing param akm:%u\n",
		pairingResponse->akm);
	DBGLOG(NAN, INFO, "pairing param key_type=%u\n",
		pairingResponse->key_type);
	DBGLOG(NAN, INFO, "pairing param key_len=%u\n",
		pairingResponse->key_len);
	DBGLOG(NAN, INFO, "pairing param cipher_type:%d\n",
		pairingResponse->cipher_type);
	if (password_length > 0) {
		u8 password[NAN_SECURITY_MAX_PASSPHRASE_LEN+1] = {0};

		memcpy(password, passphrase, NAN_SECURITY_MAX_PASSPHRASE_LEN);
		DBGLOG(NAN, INFO,
			"[pairing-intf][responder] pairing response passwd=%s\n",
			password);
	}
	hexdump_nan((void *)&pairingResponse,
		sizeof(struct _NanPairingResponseParams));
	while ((remainingLen >= 4) &&
	(0 != (readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
		switch (outputTlv.type) {
		case NAN_TLV_TYPE_NAN40_PAIRING_RAWFRAME:
			memcpy((void *)&pasnframe, outputTlv.value,
				sizeof(struct NanPairingPASNMsg));
			break;
		default:
			break;
		}
		remainingLen -= readLen;
		data += readLen;
		memset(&outputTlv, 0, sizeof(outputTlv));
	}

	/* save Local Nik(and akm) to peer PairingFsm */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)
		pasnframe.PASN_FRAME;
	prPairingFsm = pairingFsmSearch(prAdapter,
		prAuthFrame->aucSrcAddr);
	if (prPairingFsm) {
		DBGLOG(NAN, INFO, "[pairing-intf][responder] install Nik\n");
		hexdump_nan((void *)
			pairingResponse->nan_identity_key,
			sizeof(pairingResponse->
			nan_identity_key));
		nanPairingInstallLocalNik(prAdapter,
			prPairingFsm, pairingResponse->
			nan_identity_key);
		prPairingFsm->akm = pairingResponse->akm;
#if TEST_FIXED_IGTK_BIGTK
		nanPairingInstallLocalIGtk(prAdapter,
			prPairingFsm,
			test_dummy_igtk);
		nanPairingInstallLocalBIGtk(prAdapter,
			prPairingFsm,
			test_dummy_bigtk);
#else
#endif /* TEST_FIXED_IGTK_BIGTK */
	} else {
		DBGLOG(NAN, ERROR,
			"[pairing-intf][responder] sinstall NIK Fail\n");
		rRetStatus = WLAN_STATUS_FAILURE;
		return rRetStatus;
	}

	if ((enum NanPairingResponseCode)pairingResponse->rsp_code ==
		NAN_PAIRING_REQUEST_ACCEPT && pasnframe.framesize > 0) {
		/* 2. report pasn M1 to supplicant */

		DBGLOG(NAN, INFO,
			"[pairing-pasn] report pasn M1 to supplicant\n");
		memcpy(&pasnframe.localhostconfig.hostM1param,
			pairingResponse,
			sizeof(struct _NanPairingResponseParams));

		rRetStatus = nanPasnRequestM1_Rx(prAdapter, &pasnframe);
	} else {
		DBGLOG(NAN, ERROR,
			"[pairing-pasn] rsp_code(%u)/framesize(%u) error\n",
			pairingResponse->rsp_code,
			pasnframe.framesize);
	}
	return rRetStatus;
}
uint32_t
nanCommandPairingRespond_Verify(struct ADAPTER *prAdapter,
	struct _NanPairingResponseParams *pairingResponse,
	const void **prdata, int remainingLen,
	uint16_t publish_subscribe_id)
{
	u16 npk_length = NAN_NPK_LEN;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame = NULL;
	const void *data = *prdata;
	struct NanPairingPASNMsg pasnframe;
	struct _NanTlv outputTlv;
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	u16 readLen = 0;

	/*hard code for Sigma*/
	if (nanGetFeatureIsSigma(prAdapter)) {
		pairingResponse->cipher_type =
			NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
	}

	memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
	npk_length = pairingResponse->key_len;
	if (npk_length > NAN_NPK_LEN)
		npk_length = NAN_NPK_LEN;
	DBGLOG(NAN, INFO, "pairing param pairing_instance_id:%d\n",
		pairingResponse->pairing_instance_id);
	DBGLOG(NAN, INFO, "pairing param nan_pairing_request_type:%d\n",
		pairingResponse->nan_pairing_request_type);
	DBGLOG(NAN, INFO, "pairing param rsp_code:%d\n",
		pairingResponse->rsp_code);
	DBGLOG(NAN, INFO, "pairing param is_opportunistic:%d\n",
		pairingResponse->is_opportunistic);
	DBGLOG(NAN, INFO, "pairing param akm:%u\n",
		pairingResponse->akm);
	DBGLOG(NAN, INFO, "pairing param key_type=%u\n",
		pairingResponse->key_type);
	DBGLOG(NAN, INFO, "pairing param key_len=%u\n",
		pairingResponse->key_len);
	DBGLOG(NAN, INFO, "pairing param cipher_type:%d\n",
		pairingResponse->cipher_type);
	hexdump_nan((void *)&pairingResponse,
		sizeof(struct _NanPairingResponseParams));

	while ((remainingLen >= 4) &&
	(0 != (readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
		switch (outputTlv.type) {
		case NAN_TLV_TYPE_NAN40_PAIRING_RAWFRAME:
			memcpy((void *)&pasnframe, outputTlv.value,
				sizeof(struct NanPairingPASNMsg));
			break;
		default:
			break;
		}
		remainingLen -= readLen;
		data += readLen;
		memset(&outputTlv, 0, sizeof(outputTlv));
	}

	if (npk_length > 0) {
		DBGLOG(NAN, INFO,
			"[pairing-intf][responder] verification response NPK dump\n");
		hexdump_nan((void *)pairingResponse->key_data, npk_length);
	}

	/* save Local Nik(and akm) to peer PairingFsm */
	prAuthFrame = (struct WLAN_AUTH_FRAME *)
		pasnframe.PASN_FRAME;
	prPairingFsm = pairingFsmSearch(prAdapter,
		prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO,
			"preparing verification response as Publisher\n");
		DBGLOG(NAN, INFO, "Alloc new Pairing FSM\n");
		prPairingFsm = pairingFsmAlloc(prAdapter);
		if (prPairingFsm != NULL) {
			uint64_t nonce = nanPairingLoadPublisherNonce();
			/* setup PairingFsm for verification */
			prPairingFsm->FsmMode =
				NAN_PAIRING_FSM_MODE_VERIFICATION;

			/* 2. NIK , NONCE, NIRA calcuration  */
			memcpy((void *)prPairingFsm->aucNik,
				pairingResponse->nan_identity_key,
				NAN_IDENTITY_KEY_LEN);
			memcpy(prPairingFsm->aucNonce, &nonce,
				NAN_NIR_NONCE_LEN);
			pairingDeriveNirTag(prAdapter,
				prPairingFsm->aucNik, NAN_NIK_LEN,
				prAdapter->rDataPathInfo.aucLocalNMIAddr,
				prPairingFsm->aucNonce,
				&prPairingFsm->u8Tag);

			pairingFsmSetup(prAdapter,
			prPairingFsm, NAN_BOOTSTRAPPING_HANDSHAKE_SKIP,
			TRUE, publish_subscribe_id,
			0 /*requestor_instance_id*/, TRUE, 0);

			/* create sta record for verification */
			rRetStatus =
			nanPairingVerification_FsmFF(prAdapter, prPairingFsm,
				prAuthFrame->aucSrcAddr);
			if (rRetStatus == WLAN_STATUS_SUCCESS) {
				if (prPairingFsm) {
				DBGLOG(NAN, INFO,
					"[pairing-intf][responder] install Nik\n");
				hexdump_nan(
				(void *)pairingResponse->nan_identity_key,
				sizeof(pairingResponse->nan_identity_key));
				nanPairingInstallLocalNik(prAdapter,
				prPairingFsm,
				pairingResponse->nan_identity_key);
#if TEST_FIXED_IGTK_BIGTK
				nanPairingInstallLocalIGtk(prAdapter,
				prPairingFsm,
				test_dummy_igtk);
				nanPairingInstallLocalBIGtk(prAdapter,
				prPairingFsm,
				test_dummy_bigtk);
#else
#endif /* TEST_FIXED_IGTK_BIGTK */
				prPairingFsm->akm = pairingResponse->akm;
				}
			} else {
				DBGLOG(NAN, ERROR,
					"Pairing verification FSM Setup Failed!\n");
				pairingFsmFree(prAdapter, prPairingFsm);
				return WLAN_STATUS_FAILURE;
			}

		} else {
			DBGLOG(NAN, ERROR, "Pairing FSM Allocation Failed!\n");
			return WLAN_STATUS_FAILURE;
		}
	} else {
		DBGLOG(NAN, INFO,
			"preparing verification response as Publisher\n");
		DBGLOG(NAN, INFO,
			"Pairing FSM %p and reuse it\n", prPairingFsm);
	}

	if ((enum NanPairingResponseCode)pairingResponse->rsp_code ==
		NAN_PAIRING_REQUEST_ACCEPT && pasnframe.framesize > 0) {
		/* 2. report pasn M1 to supplicant */

		DBGLOG(NAN, INFO,
			"[pairing-verification] report pasn M1 to supplicant\n");
		memcpy(&pasnframe.localhostconfig.hostM1param,
			pairingResponse,
			sizeof(struct _NanPairingResponseParams));

		rRetStatus = nanPasnRequestM1_Rx(prAdapter, &pasnframe);
	} else {
		DBGLOG(NAN, INFO,
			"[pairing-verification] rsp_code(%u)/framesize(%u) error\n",
		pairingResponse->rsp_code,
		pasnframe.framesize);
	}
	return rRetStatus;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
void
nanMapSdeaCtrlParams(u32 *pIndata,
		     struct NanSdeaCtrlParams *prNanSdeaCtrlParms,
		     struct ADAPTER *prAdapter)
{
	prNanSdeaCtrlParms->config_nan_data_path =
		GET_SDEA_DATA_PATH_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->ndp_type = GET_SDEA_DATA_PATH_TYPE(*pIndata);
	prNanSdeaCtrlParms->security_cfg = GET_SDEA_SECURITY_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->ranging_state = GET_SDEA_RANGING_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->range_report = GET_SDEA_RANGE_REPORT(*pIndata);
	prNanSdeaCtrlParms->fgFSDRequire = GET_SDEA_FSD_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->fgGAS = GET_SDEA_FSD_WITH_GAS(*pIndata);
	prNanSdeaCtrlParms->fgQoS = GET_SDEA_QOS_REQUIRED(*pIndata);
	prNanSdeaCtrlParms->fgRangeLimit =
		GET_SDEA_RANGE_LIMIT_PRESENT(*pIndata);
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	prNanSdeaCtrlParms->fgGtkRequired =
		GET_SDEA_SERVICE_GTK_REQUIRED(*pIndata);
	if (prAdapter->rWifiVar.u4NanGtkCipher != NAN_CIPHER_SUITE_ID_NONE)
		prNanSdeaCtrlParms->fgGtkRequired = TRUE;
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	DBGLOG(NAN, DEBUG,
	       "config_nan_data_path: %u, ndp_type: %u, security_cfg: %u\n",
	       prNanSdeaCtrlParms->config_nan_data_path,
	       prNanSdeaCtrlParms->ndp_type, prNanSdeaCtrlParms->security_cfg);
	DBGLOG(NAN, DEBUG,
	       "ranging_state: %u, range_report: %u, fgFSDRequire: %u, fgGAS: %u, fgQoS: %u, fgRangeLimit: %u\n",
	       prNanSdeaCtrlParms->ranging_state,
	       prNanSdeaCtrlParms->range_report,
	       prNanSdeaCtrlParms->fgFSDRequire,
	       prNanSdeaCtrlParms->fgGAS, prNanSdeaCtrlParms->fgQoS,
	       prNanSdeaCtrlParms->fgRangeLimit);
}

void
nanMapRangingConfigParams(u32 *pIndata, struct NanRangingCfg *prNanRangingCfg)
{
	struct NanFWRangeConfigParams *prNanFWRangeCfgParams;

	prNanFWRangeCfgParams = (struct NanFWRangeConfigParams *)pIndata;

	prNanRangingCfg->ranging_resolution =
		prNanFWRangeCfgParams->range_resolution;
	prNanRangingCfg->ranging_interval_msec =
		prNanFWRangeCfgParams->range_interval;
	prNanRangingCfg->config_ranging_indications =
		prNanFWRangeCfgParams->ranging_indication_event;

	if (prNanRangingCfg->config_ranging_indications &
	    NAN_RANGING_INDICATE_INGRESS_MET_MASK)
		prNanRangingCfg->distance_ingress_cm =
			prNanFWRangeCfgParams->geo_fence_threshold
				.inner_threshold /
			10;
	if (prNanRangingCfg->config_ranging_indications &
	    NAN_RANGING_INDICATE_EGRESS_MET_MASK)
		prNanRangingCfg->distance_egress_cm =
			prNanFWRangeCfgParams->geo_fence_threshold
				.outer_threshold /
			10;

	DBGLOG(NAN, DEBUG,
	       "[%s]ranging_resolution: %u, ranging_interval_msec: %u, config_ranging_indications: %u\n",
	       __func__, prNanRangingCfg->ranging_resolution,
	       prNanRangingCfg->ranging_interval_msec,
	       prNanRangingCfg->config_ranging_indications);
	DBGLOG(NAN, DEBUG, "[%s]distance_egress_cm: %u\n", __func__,
	       prNanRangingCfg->distance_egress_cm);
}

void
nanMapNan20RangingReqParams(struct ADAPTER *prAdapter, u32 *pIndata,
			    struct NanRangeResponseCfg *prNanRangeRspCfgParms)
{
	struct NanFWRangeReqMsg *pNanFWRangeReqMsg;

	pNanFWRangeReqMsg = (struct NanFWRangeReqMsg *)pIndata;

	prNanRangeRspCfgParms->requestor_instance_id =
		pNanFWRangeReqMsg->range_id;
	COPY_MAC_ADDR(&prNanRangeRspCfgParms->peer_addr,
		      &pNanFWRangeReqMsg->range_mac_addr);
	if (pNanFWRangeReqMsg->ranging_accept == 1)
		prNanRangeRspCfgParms->ranging_response_code =
			NAN_RANGE_REQUEST_ACCEPT;
	else if (pNanFWRangeReqMsg->ranging_reject == 1)
		prNanRangeRspCfgParms->ranging_response_code =
			NAN_RANGE_REQUEST_REJECT;
	else
		prNanRangeRspCfgParms->ranging_response_code =
			NAN_RANGE_REQUEST_CANCEL;

	DBGLOG(NAN, DEBUG,
	       "requestor_instance_id: %u, ranging_response_code:%u\n",
	       prNanRangeRspCfgParms->requestor_instance_id,
	       prNanRangeRspCfgParms->ranging_response_code);
	DBGFWLOG(NAN, INFO, prAdapter, "addr=>%02x:%02x:%02x:%02x:%02x:%02x\n",
		 prNanRangeRspCfgParms->peer_addr[0],
		 prNanRangeRspCfgParms->peer_addr[1],
		 prNanRangeRspCfgParms->peer_addr[2],
		 prNanRangeRspCfgParms->peer_addr[3],
		 prNanRangeRspCfgParms->peer_addr[4],
		 prNanRangeRspCfgParms->peer_addr[5]);
}

#if CFG_SUPPORT_NAN_R4_PAIRING
u16
nanMapPublishPairingReqParams(u32 *pIndata,
			struct NanPublishRequest *prNanPublishReqParams)
{
	struct NanPairingCapabilityMsg *pPairingCap;

	pPairingCap = (struct NanPairingCapabilityMsg *)pIndata;
	prNanPublishReqParams->pairing_enable =
	    pPairingCap->enable_pairing_setup;
	prNanPublishReqParams->key_caching_enable =
	    pPairingCap->enable_pairing_cache;
	prNanPublishReqParams->bootstrap_type =
	    NAN_BOOTSTRAPPING_TYPE_ADVERTISE;
	prNanPublishReqParams->bootstrap_method =
	    pPairingCap->supported_bootstrapping_methods;
	prNanPublishReqParams->nira_enable =
	    pPairingCap->enable_pairing_verification;
	if (prNanPublishReqParams->nira_enable) {
		memcpy(prNanPublishReqParams->nik,
			pPairingCap->nan_identity_key,
			NAN_IDENTITY_KEY_LEN);
	}
	return 0;
}

u16
nanMapSubscribePairingReqParams(u32 *pIndata,
			struct NanSubscribeRequest *pOutparams)
{
	struct NanPairingCapabilityMsg *pPairingCap;

	pPairingCap = (struct NanPairingCapabilityMsg *)pIndata;
	pOutparams->pairing_enable =
	    pPairingCap->enable_pairing_setup;
	pOutparams->key_caching_enable =
	    pPairingCap->enable_pairing_cache;
	pOutparams->bootstrap_type =
	    NAN_BOOTSTRAPPING_TYPE_ADVERTISE;
	pOutparams->bootstrap_method =
	    pPairingCap->supported_bootstrapping_methods;
	return 0;
}
u16
nanMapBootstrapPasswordReqParams(u32 *pIndata,
			struct NanBootstrapPassword *pOutparams)
{

	u16 readLen = 0;
#if 0
	u32 *pXmitFollowupParams = NULL;

	pOutparams->requestor_instance_id = *pIndata;
	pIndata++;
	readLen += 4;

	pXmitFollowupParams = pIndata;

	pOutparams->priority = GET_FLWUP_PRIORITY(*pXmitFollowupParams);
	pOutparams->dw_or_faw = GET_FLWUP_WINDOW(*pXmitFollowupParams);
	pOutparams->recv_indication_cfg =
		GET_FLWUP_TX_RSP_DISABLE_FLAG(*pXmitFollowupParams);
	readLen += 4;

	DBGLOG(NAN, INFO,
	       "[%s]priority: %u, dw_or_faw: %u, recv_indication_cfg: %u\n",
	       __func__, pOutparams->priority, pOutparams->dw_or_faw,
	       pOutparams->recv_indication_cfg);
#endif
	return readLen;
}

void
nanClearPairingGlobalSettings(struct ADAPTER *prAdapter)
{
	DBGLOG(NAN, INFO, "[%s]\n]", __func__);
#if 0
	kalMemZero(&g_pairingPubReq, sizeof(g_pairingPubReq));
	kalMemZero(&g_bootstrapMethod, sizeof(g_bootstrapMethod));
	kalMemZero(&g_pairingSetupCmd, sizeof(g_pairingSetupCmd));
	kalMemZero(&g_nikExchange, sizeof(g_nikExchange));
#endif
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

u32
wlanoidGetNANCapabilitiesRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			     uint32_t u4SetBufferLen,
			     uint32_t *pu4SetInfoLen)
{
	struct NanCapabilitiesRspMsg nanCapabilitiesRsp;
	struct NanCapabilitiesRspMsg *pNanCapabilitiesRsp =
		(struct NanCapabilitiesRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	nanCapabilitiesRsp.fwHeader.msgVersion = 1;
	nanCapabilitiesRsp.fwHeader.msgId = NAN_MSG_ID_CAPABILITIES_RSP;
	nanCapabilitiesRsp.fwHeader.msgLen =
		sizeof(struct NanCapabilitiesRspMsg);
	nanCapabilitiesRsp.fwHeader.transactionId =
		pNanCapabilitiesRsp->fwHeader.transactionId;
	nanCapabilitiesRsp.status = 0;
	nanCapabilitiesRsp.max_concurrent_nan_clusters = 1;
	nanCapabilitiesRsp.max_service_name_len = NAN_MAX_SERVICE_NAME_LEN;
	nanCapabilitiesRsp.max_match_filter_len = NAN_FW_MAX_MATCH_FILTER_LEN;
	nanCapabilitiesRsp.max_service_specific_info_len =
		NAN_MAX_SERVICE_SPECIFIC_INFO_LEN;
	nanCapabilitiesRsp.max_sdea_service_specific_info_len =
		NAN_FW_MAX_TX_FOLLOW_UP_SDEA_LEN;
	nanCapabilitiesRsp.max_scid_len = NAN_MAX_SCID_BUF_LEN;
	nanCapabilitiesRsp.max_total_match_filter_len =
		(NAN_FW_MAX_MATCH_FILTER_LEN * 2);
	nanCapabilitiesRsp.cipher_suites_supported =
		NAN_CIPHER_SUITE_SHARED_KEY_128_MASK;
#if CFG_SUPPORT_NAN_R4_PAIRING
	/* for SKEA KDE encryption */
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_2WDH_128_MASK;
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_2WDH_256_MASK;
	/* for PASN PDU encryption */
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
#if 0   /* disable NCS-PK-PASN-256 */
	nanCapabilitiesRsp.cipher_suites_supported |=
		NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_256_MASK;
#endif
#endif
	nanCapabilitiesRsp.max_ndi_interfaces = 1;
	nanCapabilitiesRsp.max_publishes = NAN_MAX_PUBLISH_NUM;
	nanCapabilitiesRsp.max_subscribes = NAN_MAX_SUBSCRIBE_NUM;
	nanCapabilitiesRsp.max_ndp_sessions =
		prAdapter->rWifiVar.ucNanMaxNdpSession;
	nanCapabilitiesRsp.max_app_info_len = NAN_DP_MAX_APP_INFO_LEN;
	nanCapabilitiesRsp.max_queued_transmit_followup_msgs =
		NAN_MAX_QUEUE_FOLLOW_UP;
	nanCapabilitiesRsp.max_subscribe_address =
		NAN_MAX_SUBSCRIBE_MAX_ADDRESS;
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	nanCapabilitiesRsp.is_pairing_supported =
		prAdapter->rWifiVar.ucNanEnablePairing;
	DBGLOG(NAN, INFO,
	"[pairing-capability]nanCapabilitiesRsp.is_pairing_supported=%d\n",
	nanCapabilitiesRsp.is_pairing_supported);
#endif
	nanCapabilitiesRsp.is_instant_mode_supported = FALSE;
	nanCapabilitiesRsp.is_set_cluster_id_supported = FALSE;
	nanCapabilitiesRsp.is_suspension_supported = FALSE;
#if (CFG_SUPPORT_802_11AX == 1)
	nanCapabilitiesRsp.is_he_supported = TRUE;
#else
	nanCapabilitiesRsp.is_he_supported = FALSE;
#endif
#if (CFG_SUPPORT_WIFI_6G == 1)
	nanCapabilitiesRsp.is_6g_supported = prAdapter->fgIsHwSupport6G;
#else
	nanCapabilitiesRsp.is_6g_supported = FALSE;
#endif

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
		sizeof(struct NanCapabilitiesRspMsg) +
		NLMSG_HDRLEN, WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanCapabilitiesRspMsg),
			     &nanCapabilitiesRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANEnableRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		    uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanEnableRspMsg nanEnableRsp;
	struct NanEnableRspMsg *pNanEnableRsp =
		(struct NanEnableRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	nanSchedUpdateP2pAisMcc(prAdapter);
	nanExtEnableReq(prAdapter);

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	nanEnableRsp.fwHeader.msgVersion = 1;
	nanEnableRsp.fwHeader.msgId = NAN_MSG_ID_ENABLE_RSP;
	nanEnableRsp.fwHeader.msgLen = sizeof(struct NanEnableRspMsg);
	nanEnableRsp.fwHeader.transactionId =
		pNanEnableRsp->fwHeader.transactionId;
	nanEnableRsp.status = 0;
	nanEnableRsp.value = 0;

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanEnableRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanEnableRspMsg),
			     &nanEnableRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	g_disableNAN = TRUE;

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANDisableRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanDisableRspMsg nanDisableRsp;
	struct NanDisableRspMsg *pNanDisableRsp =
		(struct NanDisableRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	nanExtDisableReq(prAdapter);

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	nanDisableRsp.fwHeader.msgVersion = 1;
	nanDisableRsp.fwHeader.msgId = NAN_MSG_ID_DISABLE_RSP;
	nanDisableRsp.fwHeader.msgLen = sizeof(struct NanDisableRspMsg);
	nanDisableRsp.fwHeader.transactionId =
		pNanDisableRsp->fwHeader.transactionId;
	nanDisableRsp.status = 0;

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanDisableRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanDisableRspMsg),
			     &nanDisableRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANConfigRsp(struct ADAPTER *prAdapter,
			      void *pvSetBuffer, uint32_t u4SetBufferLen,
			      uint32_t *pu4SetInfoLen)
{
	struct NanConfigRspMsg nanConfigRsp;
	struct NanConfigRspMsg *pNanConfigRsp =
		(struct NanConfigRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	nanConfigRsp.fwHeader.msgVersion = 1;
	nanConfigRsp.fwHeader.msgId = NAN_MSG_ID_CONFIGURATION_RSP;
	nanConfigRsp.fwHeader.msgLen = sizeof(struct NanConfigRspMsg);
	nanConfigRsp.fwHeader.transactionId =
		pNanConfigRsp->fwHeader.transactionId;
	nanConfigRsp.status = 0;
	nanConfigRsp.value = 0;

	/*  Fill values of nanCapabilitiesRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanConfigRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanConfigRspMsg),
			     &nanConfigRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	if (prAdapter->rWifiVar.ucNanVendorIoctl) {
		DBGLOG(NAN, DEBUG,
		   "ucNanVendorIoctl is enabled, config\n");
		nanDevSetDWInterval(prAdapter,
			prAdapter->rWifiVar.ucNanCommittedDw);
#ifdef NAN_TODO
		DBGLOG(NAN, INFO, "Set DW timeline\n");
		nanSchedSetDwTimeline(prAdapter, FALSE);
		nanSchedCmdUpdateAvailability(prAdapter);
#endif
	}

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNanPublishRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		     uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanPublishServiceRspMsg nanPublishRsp;
	struct NanPublishServiceRspMsg *pNanPublishRsp =
		(struct NanPublishServiceRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanPairingBootStrapMsg bootstrap_msg = {0};
	uint8_t *tlvs = NULL;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	size_t message_len = 0;

	kalMemZero(&nanPublishRsp, sizeof(struct NanPublishServiceRspMsg));
	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	/* Prepare publish response header*/
	nanPublishRsp.fwHeader.msgVersion = 1;
	nanPublishRsp.fwHeader.msgId = NAN_MSG_ID_PUBLISH_SERVICE_RSP;
	nanPublishRsp.fwHeader.msgLen = sizeof(struct NanPublishServiceRspMsg);
	nanPublishRsp.fwHeader.handle = pNanPublishRsp->fwHeader.handle;
	nanPublishRsp.fwHeader.transactionId =
		pNanPublishRsp->fwHeader.transactionId;
	nanPublishRsp.value = 0;

	if (nanPublishRsp.fwHeader.handle != 0)
		nanPublishRsp.status = NAN_I_STATUS_SUCCESS;
	else
		nanPublishRsp.status = NAN_I_STATUS_INVALID_HANDLE;

	DBGLOG(NAN, DEBUG, "publish ID:%u, msgId:%u, msgLen:%u, tranID:%u\n",
	       nanPublishRsp.fwHeader.handle, nanPublishRsp.fwHeader.msgId,
	       nanPublishRsp.fwHeader.msgLen,
	       nanPublishRsp.fwHeader.transactionId);

	message_len = sizeof(struct NanPublishServiceRspMsg) + NLMSG_HDRLEN;
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingBootStrapMsg));
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	/*  Fill values of nanPublishRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, message_len,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanPublishServiceRspMsg *prNanPublishRsp;

	prNanPublishRsp = kmalloc(message_len, GFP_KERNEL);
	if (!prNanPublishRsp) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(prNanPublishRsp, message_len);
	memcpy(prNanPublishRsp, &nanPublishRsp,
		sizeof(struct NanPublishServiceRspMsg));
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		bootstrap_msg.u2ComebackAfter =
			prAdapter->rWifiVar.u2NanPairingComeback;
		tlvs = prNanPublishRsp->ptlv;
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING,
			sizeof(struct NanPairingBootStrapMsg),
			(u8 *)&bootstrap_msg, tlvs);
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     message_len, prNanPublishRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanPublishRsp);
		return -EFAULT;
	}
	kfree(prNanPublishRsp);
#else
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanPublishServiceRspMsg),
			     &nanPublishRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	cfg80211_vendor_event(skb, GFP_KERNEL);

	/* Free the memory due to no use anymore */
	kfree(pvSetBuffer);

	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANCancelPublishRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			   uint32_t u4SetBufferLen,
			   uint32_t *pu4SetInfoLen)
{
	struct NanPublishServiceCancelRspMsg nanPublishCancelRsp;
	struct NanPublishServiceCancelRspMsg *pNanPublishCancelRsp =
		(struct NanPublishServiceCancelRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	kalMemZero(&nanPublishCancelRsp,
		   sizeof(struct NanPublishServiceCancelRspMsg));
	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	DBGLOG(NAN, DEBUG, "Enter\n");

	nanPublishCancelRsp.fwHeader.msgVersion = 1;
	nanPublishCancelRsp.fwHeader.msgId =
		NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_RSP;
	nanPublishCancelRsp.fwHeader.msgLen =
		sizeof(struct NanPublishServiceCancelRspMsg);
	nanPublishCancelRsp.fwHeader.handle =
		pNanPublishCancelRsp->fwHeader.handle;
	nanPublishCancelRsp.fwHeader.transactionId =
		pNanPublishCancelRsp->fwHeader.transactionId;
	nanPublishCancelRsp.value = 0;
	nanPublishCancelRsp.status = pNanPublishCancelRsp->status;

	DBGLOG(NAN, DEBUG, "nanPublishCancelRsp.fwHeader.handle = %d\n",
	       nanPublishCancelRsp.fwHeader.handle);
	DBGLOG(NAN, DEBUG, "nanPublishCancelRsp.fwHeader.transactionId = %d\n",
	       nanPublishCancelRsp.fwHeader.transactionId);

	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanPublishServiceCancelRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanPublishServiceCancelRspMsg),
			     &nanPublishCancelRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	kfree(pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNanSubscribeRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		       uint32_t u4SetBufferLen,
		       uint32_t *pu4SetInfoLen)
{
	struct NanSubscribeServiceRspMsg nanSubscribeRsp;
	struct NanSubscribeServiceRspMsg *pNanSubscribeRsp =
		(struct NanSubscribeServiceRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	kalMemZero(&nanSubscribeRsp, sizeof(struct NanSubscribeServiceRspMsg));
	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	DBGLOG(NAN, DEBUG, "Enter\n");

	nanSubscribeRsp.fwHeader.msgVersion = 1;
	nanSubscribeRsp.fwHeader.msgId = NAN_MSG_ID_SUBSCRIBE_SERVICE_RSP;
	nanSubscribeRsp.fwHeader.msgLen =
		sizeof(struct NanSubscribeServiceRspMsg);
	nanSubscribeRsp.fwHeader.handle = pNanSubscribeRsp->fwHeader.handle;
	nanSubscribeRsp.fwHeader.transactionId =
		pNanSubscribeRsp->fwHeader.transactionId;
	nanSubscribeRsp.value = 0;
	if (nanSubscribeRsp.fwHeader.handle != 0)
		nanSubscribeRsp.status = NAN_I_STATUS_SUCCESS;
	else
		nanSubscribeRsp.status = NAN_I_STATUS_INVALID_HANDLE;

	/*  Fill values of nanSubscribeRsp */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanSubscribeServiceRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanSubscribeServiceRspMsg),
			     &nanSubscribeRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	DBGLOG(NAN, INFO, "handle:%u,transactionId:%u\n",
	       nanSubscribeRsp.fwHeader.handle,
	       nanSubscribeRsp.fwHeader.transactionId);

	kfree(pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANCancelSubscribeRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
			     uint32_t u4SetBufferLen,
			     uint32_t *pu4SetInfoLen)
{
	struct NanSubscribeServiceCancelRspMsg nanSubscribeCancelRsp;
	struct NanSubscribeServiceCancelRspMsg *pNanSubscribeCancelRsp =
		(struct NanSubscribeServiceCancelRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;

	kalMemZero(&nanSubscribeCancelRsp,
		   sizeof(struct NanSubscribeServiceCancelRspMsg));
	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	DBGLOG(NAN, DEBUG, "Enter\n");

	nanSubscribeCancelRsp.fwHeader.msgVersion = 1;
	nanSubscribeCancelRsp.fwHeader.msgId =
		NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_RSP;
	nanSubscribeCancelRsp.fwHeader.msgLen =
		sizeof(struct NanSubscribeServiceCancelRspMsg);
	nanSubscribeCancelRsp.fwHeader.handle =
		pNanSubscribeCancelRsp->fwHeader.handle;
	nanSubscribeCancelRsp.fwHeader.transactionId =
		pNanSubscribeCancelRsp->fwHeader.transactionId;
	nanSubscribeCancelRsp.value = 0;
	nanSubscribeCancelRsp.status = pNanSubscribeCancelRsp->status;

	/*  Fill values of NanSubscribeServiceCancelRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct NanSubscribeServiceCancelRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanSubscribeServiceCancelRspMsg),
			     &nanSubscribeCancelRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	DBGLOG(NAN, ERROR, "handle:%u, transactionId:%u\n",
	       nanSubscribeCancelRsp.fwHeader.handle,
	       nanSubscribeCancelRsp.fwHeader.transactionId);

	kfree(pvSetBuffer);
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANFollowupRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct NanTransmitFollowupRspMsg nanXmitFollowupRsp;
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanPairingBootStrapMsg bootstrap_msg = {0};
	uint8_t *tlvs = NULL;
	struct NanTransmitFollowupRspMsg_p
	*pNanXmitFollowupRsp_p =
	(struct NanTransmitFollowupRspMsg_p *)pvSetBuffer;

	struct NanTransmitFollowupRspMsg *pNanXmitFollowupRsp =
		pNanXmitFollowupRsp_p->pfollowRsp;
#else
	struct NanTransmitFollowupRspMsg *pNanXmitFollowupRsp =
		(struct NanTransmitFollowupRspMsg *)pvSetBuffer;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct net_device *prDev;
	struct wireless_dev *wdev;
	size_t message_len = 0;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	kalMemZero(&nanXmitFollowupRsp,
		   sizeof(struct NanTransmitFollowupRspMsg));

	DBGLOG(NAN, DEBUG, "Enter\n");

	/* Prepare Transmit Follow up response */
	nanXmitFollowupRsp.fwHeader.msgVersion = 1;
	nanXmitFollowupRsp.fwHeader.msgId = NAN_MSG_ID_TRANSMIT_FOLLOWUP_RSP;
	nanXmitFollowupRsp.fwHeader.msgLen =
		sizeof(struct NanTransmitFollowupRspMsg);
	nanXmitFollowupRsp.fwHeader.handle =
		pNanXmitFollowupRsp->fwHeader.handle;
	nanXmitFollowupRsp.fwHeader.transactionId =
		pNanXmitFollowupRsp->fwHeader.transactionId;
	nanXmitFollowupRsp.status = pNanXmitFollowupRsp->status;
	nanXmitFollowupRsp.value = 0;

	message_len = sizeof(struct NanTransmitFollowupRspMsg) + NLMSG_HDRLEN;
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingBootStrapMsg));
		bootstrap_msg.bootstrap_type =
		pNanXmitFollowupRsp_p->bootstrap_type;
		bootstrap_msg.bootstrap_status =
		pNanXmitFollowupRsp_p->bootstrap_status;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	/*  Fill values of NanSubscribeServiceCancelRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, message_len,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanTransmitFollowupRspMsg *prNanTransmitFollowupRsp;

	prNanTransmitFollowupRsp = kmalloc(message_len, GFP_KERNEL);

	if (!prNanTransmitFollowupRsp) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_RESOURCES;
	}

	kalMemZero(prNanTransmitFollowupRsp, message_len);
	memcpy(prNanTransmitFollowupRsp, &nanXmitFollowupRsp,
		sizeof(struct NanTransmitFollowupRspMsg));

	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		tlvs = prNanTransmitFollowupRsp->ptlv;
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING,
			sizeof(struct NanPairingBootStrapMsg),
			(u8 *)&bootstrap_msg, tlvs);
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     message_len, prNanTransmitFollowupRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanTransmitFollowupRsp);
		return WLAN_STATUS_INVALID_DATA;
	}
#else
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanTransmitFollowupRspMsg),
			     &nanXmitFollowupRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	cfg80211_vendor_event(skb, GFP_KERNEL);

#if CFG_SUPPORT_NAN_R4_PAIRING
	kfree(pNanXmitFollowupRsp_p->pfollowRsp);
	kfree(prNanTransmitFollowupRsp);
#else
	kfree(pvSetBuffer);
#endif
	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_NAN_R4_PAIRING
u32
wlanoidNANPairingRequestRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct _NanPairingRequestRspMsg nanPairingRequestRsp;
	struct _NanPairingRequestRspMsg *pNanPairingRequestRsp =
		(struct _NanPairingRequestRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;

	wiphy = wlanGetWiphy();
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EFAULT;
	}
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;
	kalMemZero(&nanPairingRequestRsp,
		   sizeof(struct _NanPairingRequestRspMsg));

	DBGLOG(NAN, INFO, "Enter\n");

	/* Prepare Pairing request response */
	nanPairingRequestRsp.fwHeader.msgVersion = 1;
	nanPairingRequestRsp.fwHeader.msgId = NAN_MSG_ID_PAIRING_REQUEST_RSP;
	nanPairingRequestRsp.fwHeader.msgLen =
		sizeof(struct _NanPairingRequestRspMsg);
	nanPairingRequestRsp.fwHeader.handle =
		pNanPairingRequestRsp->fwHeader.handle;
	nanPairingRequestRsp.fwHeader.transactionId =
		pNanPairingRequestRsp->fwHeader.transactionId;
	nanPairingRequestRsp.status = pNanPairingRequestRsp->status;
	nanPairingRequestRsp.value = 0;

	/*  Fill values of _NanPairingRequestRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct _NanPairingRequestRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct _NanPairingRequestRspMsg),
			     &nanPairingRequestRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	hexdump_nan(&nanPairingRequestRsp,
		sizeof(struct _NanPairingRequestRspMsg));
	cfg80211_vendor_event(skb, GFP_KERNEL);

	/* kfree(pvSetBuffer); */ /* This makes exception */
	return WLAN_STATUS_SUCCESS;
}

u32
wlanoidNANPairingResponseRsp(struct ADAPTER *prAdapter, void *pvSetBuffer,
		      uint32_t u4SetBufferLen, uint32_t *pu4SetInfoLen)
{
	struct _NanPairingResponseRspMsg nanPairingResponseRsp;
	struct _NanPairingResponseRspMsg *pNanPairingResponseRsp =
		(struct _NanPairingResponseRspMsg *)pvSetBuffer;
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;

	wiphy = wlanGetWiphy();
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);
	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;
	kalMemZero(&nanPairingResponseRsp,
		   sizeof(struct _NanPairingResponseRspMsg));

	DBGLOG(NAN, INFO, "%s\n", __func__);

	/* Prepare Transmit Follow up response */
	nanPairingResponseRsp.fwHeader.msgVersion = 1;
	nanPairingResponseRsp.fwHeader.msgId = NAN_MSG_ID_PAIRING_RESPONSE_RSP;
	nanPairingResponseRsp.fwHeader.msgLen =
		sizeof(struct _NanPairingResponseRspMsg);
	nanPairingResponseRsp.fwHeader.handle =
		pNanPairingResponseRsp->fwHeader.handle;
	nanPairingResponseRsp.fwHeader.transactionId =
		pNanPairingResponseRsp->fwHeader.transactionId;
	nanPairingResponseRsp.status = pNanPairingResponseRsp->status;
	nanPairingResponseRsp.value = 0;

	/*  Fill values of NanSubscribeServiceCancelRspMsg */
	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev,
		sizeof(struct _NanPairingResponseRspMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return WLAN_STATUS_RESOURCES;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct _NanPairingResponseRspMsg),
			     &nanPairingResponseRsp) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return WLAN_STATUS_INVALID_DATA;
	}

	hexdump_nan(&nanPairingResponseRsp,
		sizeof(struct _NanPairingResponseRspMsg));
	cfg80211_vendor_event(skb, GFP_KERNEL);

	/* kfree(pvSetBuffer); */ /* This makes exception */
	return WLAN_STATUS_SUCCESS;
}
#endif

#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE

void nan_wiphy_unlock(struct wiphy *wiphy)
{
	if (!wiphy) {
		log_dbg(NAN, ERROR, "wiphy is null\n");
		return;
	}
	wiphy_unlock(wiphy);
}

void nan_wiphy_lock(struct wiphy *wiphy)
{
	if (!wiphy) {
		log_dbg(NAN, ERROR, "wiphy is null\n");
		return;
	}
	wiphy_lock(wiphy);
}
#endif

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
static
uint32_t security_helper_pairing(struct ADAPTER *prAdapter,
				 struct NanPublishRequest *pNanPublishReq,
				 uint16_t publish_id)
{
	uint8_t ucCipherNum = 0;
	uint8_t ucCipherIdx = 0;

	DBGLOG(NAN, INFO, "cipher type=%u\n", pNanPublishReq->cipher_type);
	if (pNanPublishReq->pairing_enable) {
		DBGLOG(NAN, INFO, "nanCmdAddCsid for cipher_suite_list");
		for (ucCipherIdx = NAN_CIPHER_SUITE_ID_NCS_SK_CCM_128;
			ucCipherIdx < NAN_CIPHER_SUITE_ID_MAX;
			ucCipherIdx++) {
			if (ucCipherIdx > 0 &&
			    pNanPublishReq->cipher_type &
			    BIT(ucCipherIdx - 1)) {
				DBGLOG(NAN, DEBUG, "Idx=%u, Cipher=%u\n",
					ucCipherNum,
					ucCipherIdx);
				pNanPublishReq->cipher_suite_list[ucCipherNum]
				= ucCipherIdx;
				ucCipherNum++;
			}
		}
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
		if (prAdapter->rWifiVar.u4NanGtkCipher !=
			NAN_CIPHER_SUITE_ID_NONE) {
			pNanPublishReq->cipher_suite_list[0] =
				prAdapter->rWifiVar.u4NanGtkCipher;
		}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
		if (prAdapter->rWifiVar.u4NanPairingCipher !=
			NAN_CIPHER_SUITE_ID_NONE){
			pNanPublishReq->cipher_suite_list[1] =
				prAdapter->rWifiVar.u4NanPairingCipher;
			ucCipherNum = 2;
		}
		DBGLOG(NAN, INFO, "CipherNum=%u, cipher=%u,%u\n",
			ucCipherNum,
			pNanPublishReq->cipher_suite_list[0],
			pNanPublishReq->cipher_suite_list[1]);
		nanCmdAddCsid(prAdapter, publish_id, ucCipherNum,
			     pNanPublishReq->cipher_suite_list);
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_NOT_SUPPORTED;
}
#endif

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
static
uint32_t security_helper_r4_group_addr_frame_prot(struct ADAPTER *prAdapter,
				 struct NanPublishRequest *pNanPublishReq,
				 uint16_t publish_id)
{
	uint8_t ucCipherNum = 0;
	uint8_t ucCipherIdx = 0;

	DBGLOG(NAN, INFO, "GTK required = %u\n",
			pNanPublishReq->sdea_params.fgGtkRequired);
	if (pNanPublishReq->sdea_params.fgGtkRequired) {
		for (ucCipherIdx = NAN_CIPHER_SUITE_ID_NCS_SK_CCM_128;
			ucCipherIdx < NAN_CIPHER_SUITE_ID_MAX;
			ucCipherIdx++) {
			if (ucCipherIdx > 0 &&
			    pNanPublishReq->cipher_type &
			    BIT(ucCipherIdx - 1)) {
				pNanPublishReq->cipher_suite_list[ucCipherNum]
				= ucCipherIdx;
				ucCipherNum++;
			}
		}

		if (prAdapter->rWifiVar.u4NanGtkCipher !=
			NAN_CIPHER_SUITE_ID_NONE) {
			pNanPublishReq->cipher_suite_list[1] =
				prAdapter->rWifiVar.u4NanGtkCipher;
			ucCipherNum = 2;
		}

		DBGLOG(NAN, INFO, "CipherNum=%u, cipher=%u,%u\n",
			ucCipherNum,
			pNanPublishReq->cipher_suite_list[0],
			pNanPublishReq->cipher_suite_list[1]);
		nanCmdAddCsid(prAdapter, publish_id, ucCipherNum,
			      pNanPublishReq->cipher_suite_list);
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_NOT_SUPPORTED;
}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

static
void security_helper(struct ADAPTER *prAdapter,
		     struct NanPublishRequest *pNanPublishReq,
		     uint16_t publish_id)
{
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1 ||\
	CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	uint32_t u4Status;
#endif
	uint8_t ucCipherType = 0;

	if (!pNanPublishReq->sdea_params.security_cfg || publish_id == 0)
		return;

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	u4Status = security_helper_pairing(prAdapter,
					   pNanPublishReq, publish_id);
	if (u4Status == WLAN_STATUS_SUCCESS)
		return;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	u4Status = security_helper_r4_group_addr_frame_prot(prAdapter,
					   pNanPublishReq, publish_id);
	if (u4Status == WLAN_STATUS_SUCCESS)
		return;
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	/* Fixme: supply a cipher suite list */
	ucCipherType = pNanPublishReq->cipher_type;
	nanCmdAddCsid(prAdapter, publish_id, 1, &ucCipherType);
	nanSetPublishPmkid(prAdapter, pNanPublishReq);
	if (pNanPublishReq->scid_len) {
		if (pNanPublishReq->scid_len > NAN_SCID_DEFAULT_LEN)
			pNanPublishReq->scid_len = NAN_SCID_DEFAULT_LEN;
		nanCmdManageScid(prAdapter, TRUE,
				 publish_id, pNanPublishReq->scid);
	}
}

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
static
uint32_t security_helper_pairing_sub(struct ADAPTER *prAdapter,
				 struct NanSubscribeRequest *pNanSubscribeReq,
				 uint16_t subscribe_id)
{
	uint8_t ucCipherNum = 0;
	uint8_t ucCipherIdx = 0;

	if (pNanSubscribeReq->pairing_enable) {
		DBGLOG(NAN, INFO, "nanCmdAddCsid for cipher_suite_list");
		for (ucCipherIdx = NAN_CIPHER_SUITE_ID_NCS_SK_CCM_128;
			ucCipherIdx < NAN_CIPHER_SUITE_ID_MAX;
			ucCipherIdx++) {
			if (pNanSubscribeReq->cipher_type &
			    BIT(ucCipherIdx - 1)) {
				DBGLOG(NAN, DEBUG, "Idx=%u, Cipher=%u\n",
					ucCipherNum,
					ucCipherIdx);
				pNanSubscribeReq->cipher_suite_list[ucCipherNum]
				= ucCipherIdx;
				ucCipherNum++;
			}
		}
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
		if (prAdapter->rWifiVar.u4NanGtkCipher !=
		    NAN_CIPHER_SUITE_ID_NONE) {
			if (ucCipherNum == 1) {
				/* CSID 1 -> CSID 1 & 5 */
				if (pNanSubscribeReq->cipher_suite_list[0] <
				    NAN_CIPHER_SUITE_ID_NCS_GTK_CCM_128) {
					pNanSubscribeReq->cipher_suite_list[1] =
					prAdapter->rWifiVar.u4NanGtkCipher;
				} else {
				/* CSID 7 -> CSID 5 & 7 */
					pNanSubscribeReq->cipher_suite_list[1] =
					pNanSubscribeReq->cipher_suite_list[0];
					pNanSubscribeReq->cipher_suite_list[0] =
					prAdapter->rWifiVar.u4NanGtkCipher;
				}
				ucCipherNum++;
			} else if (ucCipherNum == 2) {
				/* CSID 1 & 7 -> CSID 5 & 7 */
				pNanSubscribeReq->cipher_suite_list[0] =
					prAdapter->rWifiVar.u4NanGtkCipher;
			}
		}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
		if (prAdapter->rWifiVar.u4NanPairingCipher !=
		    NAN_CIPHER_SUITE_ID_NONE){
			if (ucCipherNum == 1 &&
			    pNanSubscribeReq->cipher_suite_list[0] <
			    NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128) {
				/* CSID 1 -> CSID 1 & 7 */
				pNanSubscribeReq->cipher_suite_list[1] =
					prAdapter->rWifiVar.u4NanPairingCipher;
				ucCipherNum++;
			} else if (ucCipherNum == 2 &&
				   pNanSubscribeReq->cipher_suite_list[1] ==
				   NAN_CIPHER_SUITE_ID_NCS_GTK_CCM_128) {
				/* CSID 1 & 5 -> CSID 5 & 7 */
				pNanSubscribeReq->cipher_suite_list[0] =
					NAN_CIPHER_SUITE_ID_NCS_GTK_CCM_128;
				pNanSubscribeReq->cipher_suite_list[1] =
					prAdapter->rWifiVar.u4NanPairingCipher;
			}
		}
		DBGLOG(NAN, INFO, "CipherNum=%u, cipher=%u,%u\n",
			ucCipherNum,
			pNanSubscribeReq->cipher_suite_list[0],
			pNanSubscribeReq->cipher_suite_list[1]);
		nanCmdAddCsid(prAdapter, subscribe_id, ucCipherNum,
			     pNanSubscribeReq->cipher_suite_list);
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_NOT_SUPPORTED;
}
#endif

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
static
uint32_t security_helper_r4_group_addr_frame_prot_sub(struct ADAPTER *prAdapter,
				 struct NanSubscribeRequest *pNanSubscribeReq,
				 uint16_t subscribe_id)
{
	uint8_t ucCipherNum = 0;
	uint8_t ucCipherIdx = 0;

	DBGLOG(NAN, INFO, "GTK required = %u\n",
			pNanSubscribeReq->sdea_params.fgGtkRequired);
	if (pNanSubscribeReq->sdea_params.fgGtkRequired) {
		for (ucCipherIdx = NAN_CIPHER_SUITE_ID_NCS_SK_CCM_128;
			ucCipherIdx < NAN_CIPHER_SUITE_ID_MAX;
			ucCipherIdx++) {
			if (pNanSubscribeReq->cipher_type &
			    BIT(ucCipherIdx - 1)) {
				pNanSubscribeReq->cipher_suite_list[ucCipherNum]
				= ucCipherIdx;
				ucCipherNum++;
			}
		}

		if (prAdapter->rWifiVar.u4NanGtkCipher !=
		    NAN_CIPHER_SUITE_ID_NONE) {
			pNanSubscribeReq->cipher_suite_list[1] =
				prAdapter->rWifiVar.u4NanGtkCipher;
			ucCipherNum = 2;
		}

		DBGLOG(NAN, INFO, "CipherNum=%u, cipher=%u,%u\n",
			ucCipherNum,
			pNanSubscribeReq->cipher_suite_list[0],
			pNanSubscribeReq->cipher_suite_list[1]);
		nanCmdAddCsid(prAdapter, subscribe_id, ucCipherNum,
			      pNanSubscribeReq->cipher_suite_list);
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_NOT_SUPPORTED;
}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

static
void security_helper_sub(struct ADAPTER *prAdapter,
		     struct NanSubscribeRequest *pNanSubscribReq,
		     uint16_t subscribe_id)
{
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1 ||\
	CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	uint32_t u4Status;
#endif
	uint8_t ucCipherType = 0;

	DBGLOG(NAN, DEBUG, "security_cfg=%u, sub_id=%u, cipher=%u\n",
		pNanSubscribReq->sdea_params.security_cfg,
		subscribe_id,
		pNanSubscribReq->cipher_type);
	if (subscribe_id == 0 ||
	    (pNanSubscribReq->sdea_params.security_cfg == 0 &&
	    pNanSubscribReq->cipher_type == 0))
		return;

#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
	u4Status = security_helper_pairing_sub(prAdapter,
					   pNanSubscribReq, subscribe_id);
	if (u4Status == WLAN_STATUS_SUCCESS)
		return;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	u4Status = security_helper_r4_group_addr_frame_prot_sub(prAdapter,
					   pNanSubscribReq, subscribe_id);
	if (u4Status == WLAN_STATUS_SUCCESS)
		return;
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	/* Fixme: supply a cipher suite list */
	ucCipherType = pNanSubscribReq->cipher_type;
	nanCmdAddCsid(prAdapter, subscribe_id, 1, &ucCipherType);
}


struct NanDataPathInitiatorNDPE g_ndpReqNDPE;
int mtk_cfg80211_vendor_nan(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct sk_buff *skb = NULL;
	struct ADAPTER *prAdapter;
	struct WIFI_VAR *prWifiVar = NULL;
	struct _NAN_SCHEDULER_T *prNanScheduler;

	struct _NanMsgHeader nanMsgHdr;
	struct _NanTlv outputTlv;
	u16 readLen = 0;
	u32 u4BufLen;
	u32 i4Status = -EINVAL;
	u32 u4DelayIdx;
	int ret = 0;
	int remainingLen;
	u32 waitRet = 0;

	if (data_len < sizeof(struct _NanMsgHeader)) {
		DBGLOG(NAN, ERROR, "data_len error!\n");
		return -EINVAL;
	}
	remainingLen = (data_len - (sizeof(struct _NanMsgHeader)));

	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EINVAL;
	}
	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev error!\n");
		return -EINVAL;
	}

	if (data == NULL || data_len <= 0) {
		DBGLOG(NAN, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}
	WIPHY_PRIV(wiphy, prGlueInfo);

	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "prGlueInfo error!\n");
		return -EINVAL;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(NAN, WARN, "driver is not ready\n");
		return -EINVAL;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return -EINVAL;
	}

	prWifiVar = &prAdapter->rWifiVar;
	prAdapter->fgIsNANfromHAL = TRUE;
	DBGLOG(NAN, LOUD, "NAN fgIsNANfromHAL set %u\n",
		prAdapter->fgIsNANfromHAL);

	DBGDUMP_HEX(NAN, INFO, "data", data, data_len);
	DBGLOG(NAN, TRACE, "DATA len from user %d, lock(%d)\n",
		data_len,
		rtnl_is_locked());

	memcpy(&nanMsgHdr, (struct _NanMsgHeader *)data,
		sizeof(struct _NanMsgHeader));
	data += sizeof(struct _NanMsgHeader);

	DBGDUMP_HEX(NAN, INFO, "remaining", data, remainingLen);
	DBGLOG(NAN, INFO, "nanMsgHdr.length %u, nanMsgHdr.msgId %d\n",
		nanMsgHdr.msgLen, nanMsgHdr.msgId);

	switch (nanMsgHdr.msgId) {
	case NAN_MSG_ID_ENABLE_REQ: {
		struct NanEnableRequest nanEnableReq;
		struct NanEnableRspMsg nanEnableRsp;
#if KERNEL_VERSION(5, 12, 0) > CFG80211_VERSION_CODE
		uint8_t fgRollbackRtnlLock = FALSE;
#endif

		nanNdpAbortScan(prAdapter);

		kalMemZero(&nanEnableReq, sizeof(struct NanEnableRequest));
		kalMemZero(&nanEnableRsp, sizeof(struct NanEnableRspMsg));

		memcpy(&nanEnableRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanEnableRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb, sizeof(struct NanEnableRspMsg),
					   &nanEnableRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		ret = cfg80211_vendor_cmd_reply(skb);

		if (prAdapter->fgIsNANRegistered) {
			DBGLOG(NAN, WARN, "NAN is already enabled\n");
			goto skip_enable;
		}

#if KERNEL_VERSION(3, 13, 0) <= CFG80211_VERSION_CODE
		kal_reinit_completion(
			&prAdapter->prGlueInfo->rNanHaltComp);
#if (CFG_SUPPORT_MLO_STA_NAN_FALLBACK == 1)
		kal_reinit_completion(
			&prAdapter->prGlueInfo->rNanAisComp);
#endif
#else
		prAdapter->prGlueInfo->rNanHaltComp.done = 0;
#if (CFG_SUPPORT_MLO_STA_NAN_FALLBACK == 1)
		prAdapter->prGlueInfo->rNanAisComp.done = 0;
#endif
#endif

		for (u4DelayIdx = 0; u4DelayIdx < 5; u4DelayIdx++) {
			if (g_enableNAN == TRUE) {
				g_enableNAN = FALSE;
				break;
			}
			msleep(1000);
		}
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_unlock(wiphy);
#else
		/* to avoid re-enter rtnl lock during
		 * register_netdev/unregister_netdev NAN/P2P
		 * we take away lock first and return later
		 */
		if (rtnl_is_locked()) {
			fgRollbackRtnlLock = TRUE;
			rtnl_unlock();
		}
#endif

#if (CFG_SUPPORT_MLO_STA_NAN_FALLBACK == 1)
		if (aisGetLinkNum(
			aisGetDefaultAisInfo(prAdapter)) > 1 &&
			nanIsSapOrP2pActive(prAdapter)) {
			prAdapter->fgIsNANStartWaiting = TRUE;
			aisBssBeaconTimeout_impl(prAdapter,
			BEACON_TIMEOUT_REASON_NUM,
			FALSE,
			aisGetDefaultLinkBssIndex(prAdapter));
			waitRet = wait_for_completion_timeout(
				&prAdapter->prGlueInfo->rNanAisComp,
				MSEC_TO_JIFFIES(2*1000));
			prAdapter->fgIsNANStartWaiting = FALSE;
		}
#endif

		DBGLOG(NAN, TRACE,
			"[DBG] NAN enable enter set_nan_handler, lock(%d)\n",
			rtnl_is_locked());
		set_nan_handler(wdev->netdev, 1, FALSE);
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_lock(wiphy);
#else
		if (fgRollbackRtnlLock)
			rtnl_lock();
#endif

		g_deEvent = 0;

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_CONFIG_DISCOVERY_INDICATIONS:
					nanEnableReq.discovery_indication_cfg =
						*outputTlv.value;
				break;
			case NAN_TLV_TYPE_CLUSTER_ID_LOW:
				if (outputTlv.length >
					sizeof(nanEnableReq.cluster_low)) {
					DBGLOG(NAN, ERROR,
					"type%d outputTlv.length is invalid!\n",
					outputTlv.type);
					return -EFAULT;
				}
				memcpy(&nanEnableReq.cluster_low,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_CLUSTER_ID_HIGH:
				if (outputTlv.length >
					sizeof(nanEnableReq.cluster_high)) {
					DBGLOG(NAN, ERROR,
					"type%d outputTlv.length is invalid!\n",
					outputTlv.type);
					return -EFAULT;
				}
				memcpy(&nanEnableReq.cluster_high,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_5G_CHANNEL:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
						"type%d outputTlv.length %u is invalid!\n",
						outputTlv.type,
						outputTlv.length);
				} else if (prWifiVar->ucNanVendorIoctl) {
					prWifiVar->ucConfig5gChannel =
						*outputTlv.value;
					DBGLOG(NAN, INFO,
					       "Set ucConfig5gChannel=%u\n",
					       prWifiVar->ucConfig5gChannel);
				}
				break;
			case NAN_TLV_TYPE_2G_COMMITTED_DW:
			case NAN_TLV_TYPE_5G_COMMITTED_DW:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
						"type%d outputTlv.length %u is invalid!\n",
						outputTlv.type,
						outputTlv.length);
				} else if (*outputTlv.value &&
					prWifiVar->ucNanVendorIoctl) {
					prWifiVar->ucNanCommittedDw =
						*outputTlv.value;
					DBGLOG(NAN, INFO,
						"Set ucNanCommittedDw=%u\n",
						prWifiVar->ucNanCommittedDw);
				}
				break;
			case NAN_TLV_TYPE_DB_INTERVAL:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
						"type%d outputTlv.length %u is invalid!\n",
						outputTlv.type,
						outputTlv.length);
				} else if (*outputTlv.value &&
					prWifiVar->ucNanVendorIoctl) {
					prWifiVar->u2NanDiscBcnInterval =
						*outputTlv.value;
					DBGLOG(NAN, INFO,
					       "Set u2NanDiscBcnInterval=%u\n",
					       prWifiVar->u2NanDiscBcnInterval);
				}
				break;
			case NAN_TLV_TYPE_MASTER_PREFERENCE:
				if (outputTlv.length >
					sizeof(nanEnableReq.master_pref)) {
					DBGLOG(NAN, ERROR,
					"type%d outputTlv.length is invalid!\n",
					outputTlv.type);
					return -EFAULT;
				}
				memcpy(&nanEnableReq.master_pref,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_ENABLE_INSTANT_MODE:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
						"type%d outputTlv.length %u is invalid!\n",
						outputTlv.type,
						outputTlv.length);
					continue;
				}
				nanEnableReq.fgNanInstantMode =
					!!(*(uint32_t *)outputTlv.value);
				DBGLOG(NAN, INFO,
				       "Set fgNanInstantMode=%u\n",
				       nanEnableReq.fgNanInstantMode);
				break;
			case NAN_TLV_TYPE_ENABLE_INSTANT_MODE_CHANNEL:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
						"type%d outputTlv.length %u is invalid!\n",
						outputTlv.type,
						outputTlv.length);
				}
				memcpy(&nanEnableReq.u4NanInstantModeChannel,
				       outputTlv.value, outputTlv.length);
				DBGLOG(NAN, INFO,
				       "Set u4NanInstantModeChannel=%u\n",
				       nanEnableReq.u4NanInstantModeChannel);
				break;
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		nanEnableReq.enable_log_slot_statistics =
			prAdapter->rWifiVar.ucNanLogSlotStatistics;

		nanEnableReq.master_pref = prAdapter->rWifiVar.ucMasterPref;
		nanEnableReq.config_random_factor_force = 0;
		nanEnableReq.random_factor_force_val = 0;
		nanEnableReq.config_hop_count_force = 0;
		nanEnableReq.hop_count_force_val = 0;
		nanEnableReq.config_5g_channel =
			prAdapter->rWifiVar.ucConfig5gChannel;
		if (rlmDomainIsLegalChannel(prAdapter,
					    BAND_5G,
					    NAN_5G_LOW_DISC_CHANNEL))
			nanEnableReq.channel_5g_val |= BIT(0);
		if (rlmDomainIsLegalChannel(prAdapter,
					    BAND_5G,
					    NAN_5G_HIGH_DISC_CHANNEL))
			nanEnableReq.channel_5g_val |= BIT(1);

		prAdapter->fgIsNANCSAWaiting = TRUE;
		/* Wait DBDC enable here, then send Nan neable request */
		waitRet = wait_for_completion_timeout(
			&prAdapter->prGlueInfo->rNanHaltComp,
			MSEC_TO_JIFFIES(4*1000));

		if (waitRet == 0) {
			DBGLOG(NAN, ERROR,
				"wait event timeout!\n");
			return FALSE;
		}

		prAdapter->fgIsNANCSAWaiting = FALSE;
		nanEnableRsp.status = nanDevEnableRequest(prAdapter,
							  &nanEnableReq);

		for (u4DelayIdx = 0; u4DelayIdx < 50; u4DelayIdx++) {
			if (g_deEvent == NAN_BSS_INDEX_NUM) {
				g_deEvent = 0;
				break;
			}
			msleep(100);
		}

		prNanScheduler = nanGetScheduler(prAdapter);
		prNanScheduler->fgNanInstantMode = FALSE;
		prNanScheduler->u4NanInstantModeChannel = 0;
		prNanScheduler->u4NanInstantModeBitmap = NAN_ICM_DEFAULT_BITMAP;

		prNanScheduler->fgNanInstantMode =
			nanEnableReq.fgNanInstantMode;
		prNanScheduler->u4NanInstantModeChannel =
			nanEnableReq.u4NanInstantModeChannel;
		/* TODO: set bitmap according to the value in request */
skip_enable:
		i4Status = kalIoctl(prGlueInfo, wlanoidNANEnableRsp,
				    (void *)&nanEnableRsp,
				    sizeof(struct NanEnableRequest), &u4BufLen);

		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			return -EFAULT;
		}
		break;
	}
	case NAN_MSG_ID_DISABLE_REQ: {
		struct NanDisableRspMsg nanDisableRsp;
#if KERNEL_VERSION(5, 12, 0) > CFG80211_VERSION_CODE
		uint8_t fgRollbackRtnlLock = FALSE;
#endif

		kalMemZero(&nanDisableRsp, sizeof(struct NanDisableRspMsg));

		memcpy(&nanDisableRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanDisableRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb, sizeof(struct NanDisableRspMsg),
					   &nanDisableRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		ret = cfg80211_vendor_cmd_reply(skb);

		if (!prAdapter->fgIsNANRegistered) {
			DBGLOG(NAN, WARN, "NAN is already disabled\n");
			goto skip;
		}

		for (u4DelayIdx = 0; u4DelayIdx < 5; u4DelayIdx++) {
			/* Do not block to disable if not enable */
			if (g_disableNAN == TRUE || g_enableNAN == TRUE) {
				g_disableNAN = FALSE;
				break;
			}
			msleep(1000);
		}

		if (!wlanIsDriverReady(prGlueInfo,
			WLAN_DRV_READY_CHECK_WLAN_ON |
			WLAN_DRV_READY_CHECK_HIF_SUSPEND)) {
			DBGLOG(NAN, WARN, "driver is not ready\n");
			return -EFAULT;
		}

		if (prAdapter->rWifiVar.ucNanMaxNdpDissolve)
			nanNdpDissolve(prAdapter,
				prAdapter->rWifiVar.u4NanDissolveTimeout);
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
		nanDiscClearAllInstance();
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)

		if (nanSecIsDevSupportGroupSecurity(prAdapter)) {
			struct WIFI_VAR *prWifiVar = NULL;
			struct BSS_INFO *prnanBssInfo = NULL;
			struct _NAN_SPECIFIC_BSS_INFO_T *prNanSpecInfo =
				(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
			uint8_t aucBmcMacAddr[] = BC_MAC_ADDR;
			uint8_t i = 0;

			prWifiVar = &prAdapter->rWifiVar;

			for (i = 0; i < NAN_BSS_INDEX_NUM; i++) {
				prNanSpecInfo =
					nanGetSpecificBssInfo(prAdapter, i);

				if (prNanSpecInfo == NULL)
					continue;

				prnanBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
					prNanSpecInfo->ucBssIndex);
				if (prnanBssInfo == NULL)
					continue;

				DBGLOG(NAN, INFO, "Remove TX IGTK, Wtbl:%d\n",
					prnanBssInfo->ucBMCWlanIndex);
				nan_sec_wpas_setkey_glue(FALSE,
					prnanBssInfo->ucBssIndex,
					WPA_ALG_NONE,
					aucBmcMacAddr,
					4,
					NULL,
					0);

				if (prWifiVar->ucNanGroupSecCap ==
					CSIA_CAP_GTKSA_IGTKSA_SUP_BIGTKSA_UNSUP)
					continue;

				DBGLOG(NAN, INFO, "Remove TX BIGTK, Wtbl:%d\n",
					prnanBssInfo->ucBMCWlanIndex);
				nan_sec_wpas_setkey_glue(FALSE,
					prnanBssInfo->ucBssIndex,
					WPA_ALG_NONE,
					aucBmcMacAddr,
					6,
					NULL,
					0);
			}
		}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

		nanDisableRsp.status =
			nanDevDisableRequest(prGlueInfo->prAdapter);

#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_unlock(wiphy);
#else
		/* to avoid re-enter rtnl lock during
		 * register_netdev/unregister_netdev NAN/P2P
		 * we take away lock first and return later
		 */
		if (rtnl_is_locked()) {
			fgRollbackRtnlLock = TRUE;
			rtnl_unlock();
		}
#endif
		DBGLOG(NAN, TRACE,
			"[DBG] NAN disable, enter set_nan_handler, lock(%d)\n",
			rtnl_is_locked());
		set_nan_handler(wdev->netdev, 0, FALSE);
#if KERNEL_VERSION(5, 12, 0) <= CFG80211_VERSION_CODE
		nan_wiphy_lock(wiphy);
#else
		if (fgRollbackRtnlLock)
			rtnl_lock();
#endif

skip:

		i4Status = kalIoctl(prGlueInfo, wlanoidNANDisableRsp,
				    (void *)&nanDisableRsp,
				    sizeof(struct NanDisableRspMsg), &u4BufLen);

		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			return -EFAULT;
		}

		break;
	}
	case NAN_MSG_ID_CONFIGURATION_REQ: {
		struct NanConfigRequest nanConfigReq;
		struct NanConfigRspMsg nanConfigRsp;

		kalMemZero(&nanConfigReq, sizeof(struct NanConfigRequest));
		kalMemZero(&nanConfigRsp, sizeof(struct NanConfigRspMsg));

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_MASTER_PREFERENCE:
				if (outputTlv.length >
					sizeof(nanConfigReq.master_pref)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					return -EFAULT;
				}
				memcpy(&nanConfigReq.master_pref,
				       outputTlv.value, outputTlv.length);
				nanDevSetMasterPreference(
					prGlueInfo->prAdapter,
					nanConfigReq.master_pref);
				break;
			case NAN_TLV_TYPE_2G_COMMITTED_DW:
			case NAN_TLV_TYPE_5G_COMMITTED_DW:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
				} else if (*outputTlv.value &&
					prWifiVar->ucNanVendorIoctl) {
					prWifiVar->ucNanCommittedDw =
						*outputTlv.value;
					DBGLOG(NAN, INFO,
						"Set ucNanCommittedDw=%u\n",
						prWifiVar->ucNanCommittedDw);
				}
				break;
			case NAN_TLV_TYPE_DB_INTERVAL:
				if (outputTlv.length != sizeof(uint32_t)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
				} else if (*outputTlv.value &&
					prWifiVar->ucNanVendorIoctl) {
					prWifiVar->u2NanDiscBcnInterval =
						*outputTlv.value;
					DBGLOG(NAN, INFO,
					       "Set u2NanDiscBcnInterval=%u\n",
					       prWifiVar->u2NanDiscBcnInterval);
				}
				break;
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		nanConfigRsp.status = 0;

		memcpy(&nanConfigRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanConfigRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb, sizeof(struct NanConfigRspMsg),
					   &nanConfigRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidNANConfigRsp,
			(void *)&nanConfigRsp, sizeof(struct NanConfigRspMsg),
			&u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);
		break;
	}
	case NAN_MSG_ID_CAPABILITIES_REQ: {
		struct NanCapabilitiesRspMsg nanCapabilitiesRsp;

		memcpy(&nanCapabilitiesRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanCapabilitiesRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
					   sizeof(struct NanCapabilitiesRspMsg),
					   &nanCapabilitiesRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidGetNANCapabilitiesRsp,
				    (void *)&nanCapabilitiesRsp,
				    sizeof(struct NanCapabilitiesRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
		DBGLOG(NAN, DEBUG, "i4Status = %u\n", i4Status);
		ret = cfg80211_vendor_cmd_reply(skb);

		break;
	}
	case NAN_MSG_ID_PUBLISH_SERVICE_REQ: {
		struct NanPublishRequest *pNanPublishReq = NULL;
		struct NanPublishServiceRspMsg *pNanPublishRsp = NULL;
		uint16_t publish_id = 0;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
		uint8_t *pu2CipherSuiteList;

		_Static_assert(sizeof(g_aucNanGtkCipherSuiteList[0]) ==
			       sizeof(uint8_t),
			       "g_aucNanGtkCipherSuiteList shall be uint_8");
		_Static_assert(sizeof(pNanPublishReq->cipher_suite_list[0]) ==
			       sizeof(uint8_t),
			       "NanPublishRequest->cipher_suite_list shall be uint_8");
#endif

		DBGLOG(NAN, INFO, "IN case NAN_MSG_ID_PUBLISH_SERVICE_REQ\n");

		pNanPublishReq =
			kmalloc(sizeof(struct NanPublishRequest), GFP_ATOMIC);

		if (!pNanPublishReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanPublishRsp = kmalloc(sizeof(struct NanPublishServiceRspMsg),
					 GFP_ATOMIC);

		if (!pNanPublishRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanPublishReq);
			return -ENOMEM;
		}

		kalMemZero(pNanPublishReq, sizeof(struct NanPublishRequest));
		kalMemZero(pNanPublishRsp,
			   sizeof(struct NanPublishServiceRspMsg));

		/* Mapping publish req related parameters */
		readLen = nanMapPublishReqParams((u16 *)data, pNanPublishReq);
		remainingLen -= readLen;
		data += readLen;

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_SERVICE_NAME:
				if (outputTlv.length >
					NAN_MAX_SERVICE_NAME_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memset(g_aucNanServiceName, 0,
					NAN_MAX_SERVICE_NAME_LEN);
				memcpy(pNanPublishReq->service_name,
				       outputTlv.value, outputTlv.length);
				memcpy(g_aucNanServiceName,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->service_name_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"type:SERVICE_NAME:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"type:SERVICE_SPECIFIC_INFO:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_RX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->rx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->rx_match_filter_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"type:RX_MATCH_FILTER:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				DBGDUMP_HEX(NAN, DEBUG, "RX match filter",
					   pNanPublishReq->rx_match_filter,
					   pNanPublishReq->rx_match_filter_len);
				break;
			case NAN_TLV_TYPE_TX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->tx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->tx_match_filter_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"type:TX_MATCH_FILTER:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				DBGDUMP_HEX(NAN, DEBUG, "TX match filter",
					   pNanPublishReq->tx_match_filter,
					   pNanPublishReq->tx_match_filter_len);
				break;
			case NAN_TLV_TYPE_NAN_SERVICE_ACCEPT_POLICY:
				pNanPublishReq->service_responder_policy =
					*(outputTlv.value);
				DBGLOG(NAN, DEBUG,
					"type:SERVICE_ACCEPT_POLICY:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN_CSID:
				pNanPublishReq->cipher_type =
					*(outputTlv.value);
				DBGLOG(NAN, DEBUG, "cipher=%u\n",
					pNanPublishReq->cipher_type);
				break;
			case NAN_TLV_TYPE_NAN_PMK:
				if (outputTlv.length >
					NAN_PMK_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->key_info.body.pmk_info
					       .pmk,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->key_info.body.pmk_info.pmk_len =
					outputTlv.length;
				break;
			case NAN_TLV_TYPE_NAN_PASSPHRASE:
				if (outputTlv.length >
					NAN_SECURITY_MAX_PASSPHRASE_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanPublishRsp);
					kfree(pNanPublishReq);
					return -EFAULT;
				}
				memcpy(pNanPublishReq->key_info.body
					       .passphrase_info.passphrase,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->key_info.body.passphrase_info
					.passphrase_len = outputTlv.length;
				break;
			case NAN_TLV_TYPE_SDEA_CTRL_PARAMS:
				nanMapSdeaCtrlParams(
					(u32 *)outputTlv.value,
					&pNanPublishReq->sdea_params,
					prAdapter);
				DBGLOG(NAN, DEBUG,
					"type:_SDEA_CTRL_PARAMS:%u Len:%u\n",
					outputTlv.type, outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN_RANGING_CFG:
				nanMapRangingConfigParams(
					(u32 *)outputTlv.value,
					&pNanPublishReq->ranging_cfg);
				break;
			case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SDEA_LEN) {
					DBGLOG(NAN, ERROR,
						"outputTlv.length is invalid!\n");
					kfree(pNanPublishReq);
					kfree(pNanPublishRsp);
					return -EFAULT;
				}
				memcpy(pNanPublishReq
					->sdea_service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanPublishReq->sdea_service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"SDEA_SERVICE_SPECIFIC_INFO type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN20_RANGING_REQUEST:
				nanMapNan20RangingReqParams(
					prAdapter,
					(u32 *)outputTlv.value,
					&pNanPublishReq->range_response_cfg);
				break;
#if CFG_SUPPORT_NAN_R4_PAIRING /*NAN_PAIRING*/
			case NAN_TLV_TYPE_NAN40_PAIRING_CAPABILITY:
				nanMapPublishPairingReqParams(
					(u32 *)outputTlv.value,
					pNanPublishReq);
				break;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

#if CFG_SUPPORT_NAN_R4_PAIRING /*NAN_PAIRING*/
		if (prAdapter->fgIsNANfromHAL == TRUE) {
			DBGLOG(NAN, INFO,
				"[pairing-disc][pairing_enable=%u]\n",
				pNanPublishReq->pairing_enable);
		} else {
			pNanPublishReq->pairing_enable =
				g_pairingPubReq.ucPairingSetupEnabled;
			if (prAdapter->rWifiVar.ucNanEnablePairing == 0)
				pNanPublishReq->pairing_enable = 0;

			if (pNanPublishReq->pairing_enable) {
				pNanPublishReq->sdea_params.security_cfg =
					NAN_DP_CONFIG_SECURITY;
				pNanPublishReq->key_caching_enable =
					g_pairingPubReq.ucNPKNIKCache;
				pNanPublishReq->bootstrap_method =
					g_pairingPubReq.ucBootstrapMethod;
				pNanPublishReq->nira_enable =
					g_pairingPubReq.ucNiraEnabled;
				kalMemCpyS(pNanPublishReq->cipher_suite_list,
					sizeof(g_pairingPubReq.
					aucCipherSuiteList),
					g_pairingPubReq.aucCipherSuiteList, 2);
			}
			nanClearPairingGlobalSettings(prAdapter);
		}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
		pu2CipherSuiteList = pNanPublishReq->cipher_suite_list;
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

		/* Publish response message */
		memcpy(&pNanPublishRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanPublishServiceRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR,
				"Allocate skb failed\n");
			kfree(pNanPublishRsp);
			kfree(pNanPublishReq);
			return -ENOMEM;
		}

		if (unlikely(nla_put_nohdr(
			skb,
			sizeof(struct NanPublishServiceRspMsg),
			pNanPublishRsp) < 0)) {
			kfree_skb(skb);
			kfree(pNanPublishRsp);
			kfree(pNanPublishReq);
			DBGLOG(NAN, ERROR, "Fail send reply\n");
			return -EFAULT;
		}
		/* WIFI HAL will set nanMsgHdr.handle to 0xFFFF
		 * if publish id is 0. (means new publish) Otherwise set
		 * to previous publish id.
		 */
		if (nanMsgHdr.handle != 0xFFFF)
			pNanPublishReq->publish_id = nanMsgHdr.handle;

		/* return publish ID */
		publish_id = (uint16_t)nanPublishRequest(prGlueInfo->prAdapter,
							pNanPublishReq);
		/* NAN_CHK_PNT log message */
		if (nanMsgHdr.handle == 0xFFFF)
			nanLogPublish(publish_id);

		pNanPublishRsp->fwHeader.handle = publish_id;
		DBGLOG(NAN, INFO,
			"pNanPublishRsp->fwHeader.handle %u, publish_id : %u\n",
			pNanPublishRsp->fwHeader.handle, publish_id);

		security_helper(prGlueInfo->prAdapter,
				pNanPublishReq, publish_id);

#ifdef REMOVE_ME /* REMOVE THIS PART, refactored by calling security_helper */
		if (pNanPublishReq->sdea_params.security_cfg &&
			publish_id != 0) {
#if CFG_SUPPORT_NAN_R4_PAIRING
			if (pNanPublishReq->pairing_enable) {

				DBGLOG(NAN, INFO,
				"nanCmdAddCsid for cipher_suite_list\n");
				nanCmdAddCsid(
					prGlueInfo->prAdapter,
					publish_id,
					2,
					pNanPublishReq->cipher_suite_list);
			} else
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
			if (pNanPublishReq->sdea_params.fgGtkRequired) {
				kalMemCopy(pu2CipherSuiteList,
					&pNanPublishReq->cipher_type,
					sizeof(pNanPublishReq->cipher_type));
				nanCmdAddCsid(prGlueInfo->prAdapter,
					     publish_id,
					     2,
					     pu2CipherSuiteList);
			} else
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
			{
				/* Fixme: supply a cipher suite list */
				ucCipherType = pNanPublishReq->cipher_type;
				nanCmdAddCsid(prGlueInfo->prAdapter,
					      publish_id,
					      1,
					      &ucCipherType);
				nanSetPublishPmkid(prGlueInfo->prAdapter,
						   pNanPublishReq);
				if (pNanPublishReq->scid_len) {
					pNanPublishReq->scid_len =
						kal_min_t(uint32_t,
						       pNanPublishReq->scid_len,
						       NAN_SCID_DEFAULT_LEN);
					nanCmdManageScid(prGlueInfo->prAdapter,
							 TRUE,
							 publish_id,
							 pNanPublishReq->scid);
				}
			}
		}
#endif /* REMOVE_ME */
		nanExtTerminateApNanEndLegacy(prAdapter);

		i4Status = kalIoctl(prGlueInfo, wlanoidNanPublishRsp,
				    (void *)pNanPublishRsp,
				    sizeof(struct NanPublishServiceRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			kfree(pNanPublishReq);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanPublishReq);
		break;
	}
	case NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_REQ: {
		uint32_t rStatus;
		struct NanPublishCancelRequest *pNanPublishCancelReq = NULL;
		struct NanPublishServiceCancelRspMsg *pNanPublishCancelRsp =
			NULL;

		pNanPublishCancelReq = kmalloc(
			sizeof(struct NanPublishCancelRequest), GFP_ATOMIC);

		if (!pNanPublishCancelReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanPublishCancelRsp =
			kmalloc(sizeof(struct NanPublishServiceCancelRspMsg),
				GFP_ATOMIC);

		if (!pNanPublishCancelRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanPublishCancelReq);
			return -ENOMEM;
		}

		DBGLOG(NAN, DEBUG, "Enter CANCEL Publish Request\n");
		pNanPublishCancelReq->publish_id = nanMsgHdr.handle;

		DBGLOG(NAN, DEBUG,
		       "PID %d\n", pNanPublishCancelReq->publish_id);
		rStatus = nanCancelPublishRequest(prGlueInfo->prAdapter,
						  pNanPublishCancelReq);

		/* Prepare for command reply */
		memcpy(&pNanPublishCancelRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanPublishServiceCancelRspMsg));
		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanPublishCancelReq);
			kfree(pNanPublishCancelRsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb, sizeof(struct
						 NanPublishServiceCancelRspMsg),
				     pNanPublishCancelRsp) < 0)) {
			kfree_skb(skb);
			kfree(pNanPublishCancelReq);
			kfree(pNanPublishCancelRsp);
			return -EFAULT;
		}

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, DEBUG,
			       "CANCEL Publish Error %x\n", rStatus);
			pNanPublishCancelRsp->status = NAN_I_STATUS_DE_FAILURE;
		} else {
			DBGLOG(NAN, DEBUG, "CANCEL Publish Success %x\n",
			       rStatus);
			pNanPublishCancelRsp->status = NAN_I_STATUS_SUCCESS;
		}

		i4Status =
			kalIoctl(prGlueInfo, wlanoidNANCancelPublishRsp,
				 (void *)pNanPublishCancelRsp,
				 sizeof(struct NanPublishServiceCancelRspMsg),
				 &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			kfree(pNanPublishCancelReq);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanPublishCancelReq);
		break;
	}
	case NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ: {
		struct NanSubscribeRequest *pNanSubscribeReq = NULL;
		struct NanSubscribeServiceRspMsg *pNanSubscribeRsp = NULL;
		bool fgRangingCFG = FALSE;
		bool fgRangingREQ = FALSE;
		uint16_t Subscribe_id = 0;
		int i = 0;

		DBGLOG(NAN, DEBUG, "In NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ\n");

		pNanSubscribeReq =
			kmalloc(sizeof(struct NanSubscribeRequest), GFP_ATOMIC);

		if (!pNanSubscribeReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		pNanSubscribeRsp = kmalloc(
			sizeof(struct NanSubscribeServiceRspMsg), GFP_ATOMIC);

		if (!pNanSubscribeRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanSubscribeReq);
			return -ENOMEM;
		}
		kalMemZero(pNanSubscribeReq,
			   sizeof(struct NanSubscribeRequest));
		kalMemZero(pNanSubscribeRsp,
			   sizeof(struct NanSubscribeServiceRspMsg));

		/* WIFI HAL will set nanMsgHdr.handle to 0xFFFF
		 * if subscribe_id is 0. (means new subscribe)
		 */
		if (nanMsgHdr.handle != 0xFFFF)
			pNanSubscribeReq->subscribe_id = nanMsgHdr.handle;

		/* Mapping subscribe req related parameters */
		readLen =
			nanMapSubscribeReqParams((u16 *)data, pNanSubscribeReq);
		remainingLen -= readLen;
		data += readLen;
		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_SERVICE_NAME:
				if (outputTlv.length >
					NAN_MAX_SERVICE_NAME_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memset(g_aucNanServiceName, 0,
					NAN_MAX_SERVICE_NAME_LEN);
				memcpy(pNanSubscribeReq->service_name,
				       outputTlv.value, outputTlv.length);
				memcpy(g_aucNanServiceName,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->service_name_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"SERVICE_NAME type:%u len:%u SRV_name:%s\n",
					outputTlv.type,
					outputTlv.length,
					pNanSubscribeReq->service_name);
				break;
			case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"SERVICE_SPECIFIC_INFO type:%u len:%u SRV_spec_info:%s\n",
					outputTlv.type,
					outputTlv.length,
					pNanSubscribeReq
					->service_specific_info);
				break;
			case NAN_TLV_TYPE_RX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->rx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->rx_match_filter_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"RX_MATCH_FILTER type:%u len:%u rx_match_filter:%s\n",
					outputTlv.type,
					outputTlv.length,
					pNanSubscribeReq->rx_match_filter);
				DBGDUMP_HEX(NAN, DEBUG, "RX match filter",
					    pNanSubscribeReq->rx_match_filter,
					    outputTlv.length);
				break;
			case NAN_TLV_TYPE_TX_MATCH_FILTER:
				if (outputTlv.length >
					NAN_FW_MAX_MATCH_FILTER_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length %d is invalid!\n",
					outputTlv.length);
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->tx_match_filter,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->tx_match_filter_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"TX_MATCH_FILTERtype:%u len:%u tx_match_filter:%s\n",
					outputTlv.type,
					outputTlv.length,
					pNanSubscribeReq->tx_match_filter);
				DBGDUMP_HEX(NAN, DEBUG, "TX match filter",
					    pNanSubscribeReq->tx_match_filter,
					    outputTlv.length);
				break;
			case NAN_TLV_TYPE_MAC_ADDRESS:
				if (outputTlv.length >
					sizeof(uint8_t)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				if (i < NAN_MAX_SUBSCRIBE_MAX_ADDRESS) {
					/* Get column neumbers */
					memcpy(pNanSubscribeReq->intf_addr[i],
					     outputTlv.value, outputTlv.length);
					i++;
				}
				break;
			case NAN_TLV_TYPE_NAN_CSID:
				pNanSubscribeReq->cipher_type =
					*(outputTlv.value);
				DBGLOG(NAN, DEBUG, "NAN_CSID type:%u len:%u\n",
				       outputTlv.type, outputTlv.length);
				DBGLOG(NAN, DEBUG, "cipher=%u\n",
					pNanSubscribeReq->cipher_type);
				break;
			case NAN_TLV_TYPE_NAN_PMK:
				if (outputTlv.length >
					NAN_PMK_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->key_info.body.pmk_info
					       .pmk,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->key_info.body.pmk_info
					.pmk_len = outputTlv.length;
				break;
			case NAN_TLV_TYPE_NAN_PASSPHRASE:
				if (outputTlv.length >
					NAN_SECURITY_MAX_PASSPHRASE_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq->key_info.body
					       .passphrase_info.passphrase,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq->key_info.body.passphrase_info
					.passphrase_len = outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"NAN_PASSPHRASE type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);
				break;
			case NAN_TLV_TYPE_SDEA_CTRL_PARAMS:
				nanMapSdeaCtrlParams(
					(u32 *)outputTlv.value,
					&pNanSubscribeReq->sdea_params,
					prAdapter);
				DBGLOG(NAN, DEBUG,
					"SDEA_CTRL_PARAMS type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN_RANGING_CFG:
				fgRangingCFG = TRUE;
				DBGLOG(NAN, DEBUG, "fgRangingCFG %d\n",
					fgRangingCFG);
				nanMapRangingConfigParams(
					(u32 *)outputTlv.value,
					&pNanSubscribeReq->ranging_cfg);
				pNanSubscribeReq->ranging_enabled = TRUE;
				break;
			case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SDEA_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanSubscribeReq);
					kfree(pNanSubscribeRsp);
					return -EFAULT;
				}
				memcpy(pNanSubscribeReq
					->sdea_service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanSubscribeReq
					->sdea_service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"SDEA_SERVICE_SPECIFIC_INFO type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);

				break;
			case NAN_TLV_TYPE_NAN20_RANGING_REQUEST:
				fgRangingREQ = TRUE;
				DBGLOG(NAN, DEBUG, "fgRangingREQ %d\n",
					fgRangingREQ);
				nanMapNan20RangingReqParams(
					prAdapter,
					(u32 *)outputTlv.value,
					&pNanSubscribeReq->range_response_cfg);
				break;
#if CFG_SUPPORT_NAN_R4_PAIRING /* NAN_PAIRING */
			case NAN_TLV_TYPE_NAN40_PAIRING_CAPABILITY:
				nanMapSubscribePairingReqParams(
				(u32 *)outputTlv.value, pNanSubscribeReq);
				break;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

#if CFG_SUPPORT_NAN_R4_PAIRING /* NAN_PAIRING */
		if (prAdapter->fgIsNANfromHAL == TRUE) {
			DBGLOG(NAN, INFO,
				"[pairing-disc][pairing_enable=%u]\n",
				pNanSubscribeReq->pairing_enable);
		} else {
			pNanSubscribeReq->pairing_enable =
				g_pairingPubReq.ucPairingSetupEnabled;
			pNanSubscribeReq->bootstrap_method =
				g_bootstrapMethod.ucBootstrapMethod;
			nanClearPairingGlobalSettings(prAdapter);
		}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

		/* Prepare command reply of Subscriabe response */
		memcpy(&pNanSubscribeRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanSubscribeServiceRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanSubscribeReq);
			kfree(pNanSubscribeRsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb,
				     sizeof(struct NanSubscribeServiceRspMsg),
				     pNanSubscribeRsp) < 0)) {
			kfree(pNanSubscribeReq);
			kfree(pNanSubscribeRsp);
			kfree_skb(skb);
			return -EFAULT;
		}

		/* Do Ranging request under sigma mode */
		if (nanGetFeatureIsSigma(prAdapter) &&
			fgRangingCFG && fgRangingREQ) {

			struct NanRangeRequest *rgreq = NULL;
			uint16_t rgId = 0;
			uint32_t rStatus;

			rgreq = kmalloc(sizeof(struct NanRangeRequest),
				GFP_ATOMIC);

			if (!rgreq) {
				DBGLOG(NAN, ERROR, "Allocate failed\n");
				kfree(pNanSubscribeReq);
				kfree(pNanSubscribeRsp);
				kfree_skb(skb);
				return -ENOMEM;
			}

			kalMemZero(rgreq, sizeof(struct NanRangeRequest));

			memcpy(&rgreq->peer_addr,
				&pNanSubscribeReq->range_response_cfg.peer_addr,
				NAN_MAC_ADDR_LEN);
			memcpy(&rgreq->ranging_cfg,
				&pNanSubscribeReq->ranging_cfg,
				sizeof(struct NanRangingCfg));
			rgreq->range_id =
			pNanSubscribeReq->range_response_cfg
				.requestor_instance_id;
			DBGLOG(NAN, DEBUG, MACSTR
				" id %d reso %d intev %d indicate %d ING CM %d ENG CM %d\n",
				MAC2STR(rgreq->peer_addr),
				rgreq->range_id,
				rgreq->ranging_cfg.ranging_resolution,
				rgreq->ranging_cfg.ranging_interval_msec,
				rgreq->ranging_cfg.config_ranging_indications,
				rgreq->ranging_cfg.distance_ingress_cm,
				rgreq->ranging_cfg.distance_egress_cm);
			rStatus =
			nanRangingRequest(prGlueInfo->prAdapter, &rgId, rgreq);

			nanExtTerminateApNanEndLegacy(prAdapter);

			pNanSubscribeRsp->fwHeader.handle = rgId;
			i4Status = kalIoctl(prGlueInfo, wlanoidNanSubscribeRsp,
				    (void *)pNanSubscribeRsp,
				    sizeof(struct NanSubscribeServiceRspMsg),
				    &u4BufLen);
			if (i4Status != WLAN_STATUS_SUCCESS) {
				DBGLOG(NAN, ERROR, "kalIoctl failed\n");
				kfree(pNanSubscribeReq);
				kfree(rgreq);
				kfree_skb(skb);
				return -EFAULT;
			}
			kfree(rgreq);
			kfree(pNanSubscribeReq);
			break;

		}

		prAdapter->fgIsNANfromHAL = TRUE;

		/* return subscribe ID */
		Subscribe_id = (uint16_t)nanSubscribeRequest(
			prGlueInfo->prAdapter, pNanSubscribeReq);
		/* NAN_CHK_PNT log message */
		if (nanMsgHdr.handle == 0xFFFF)
			nanLogSubscribe(Subscribe_id);

		pNanSubscribeRsp->fwHeader.handle = Subscribe_id;

		security_helper_sub(prGlueInfo->prAdapter,
					pNanSubscribeReq, Subscribe_id);

		DBGLOG(NAN, INFO,
		       "Subscribe_id:%u, pNanSubscribeRsp->fwHeader.handle:%u\n",
		       Subscribe_id, pNanSubscribeRsp->fwHeader.handle);
		i4Status = kalIoctl(prGlueInfo, wlanoidNanSubscribeRsp,
				    (void *)pNanSubscribeRsp,
				    sizeof(struct NanSubscribeServiceRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree(pNanSubscribeReq);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanSubscribeReq);
		break;
	}
	case NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_REQ: {
		uint32_t rStatus;
		struct NanSubscribeCancelRequest *pNanSubscribeCancelReq = NULL;
		struct NanSubscribeServiceCancelRspMsg *pNanSubscribeCancelRsp =
			NULL;

		pNanSubscribeCancelReq = kmalloc(
			sizeof(struct NanSubscribeCancelRequest), GFP_ATOMIC);
		if (!pNanSubscribeCancelReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanSubscribeCancelRsp =
			kmalloc(sizeof(struct NanSubscribeServiceCancelRspMsg),
				GFP_ATOMIC);
		if (!pNanSubscribeCancelRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanSubscribeCancelReq);
			return -ENOMEM;
		}
		kalMemZero(pNanSubscribeCancelReq,
			   sizeof(struct NanSubscribeCancelRequest));
		kalMemZero(pNanSubscribeCancelRsp,
			   sizeof(struct NanSubscribeServiceCancelRspMsg));

		DBGLOG(NAN, DEBUG, "Enter CANCEL Subscribe Request\n");
		pNanSubscribeCancelReq->subscribe_id = nanMsgHdr.handle;

		DBGLOG(NAN, DEBUG, "PID %d\n",
		       pNanSubscribeCancelReq->subscribe_id);
		rStatus = nanCancelSubscribeRequest(prGlueInfo->prAdapter,
						    pNanSubscribeCancelReq);

		/* Prepare Cancel Subscribe command reply message */
		memcpy(&pNanSubscribeCancelRsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanSubscribeServiceCancelRspMsg));
		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanSubscribeCancelReq);
			kfree(pNanSubscribeCancelRsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb,
				     sizeof(struct
					    NanSubscribeServiceCancelRspMsg),
				     pNanSubscribeCancelRsp) < 0)) {
			kfree(pNanSubscribeCancelReq);
			kfree(pNanSubscribeCancelRsp);
			kfree_skb(skb);
			return -EFAULT;
		}

		if (rStatus != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "CANCEL Subscribe Error %X\n",
			       rStatus);
			pNanSubscribeCancelRsp->status =
				NAN_I_STATUS_DE_FAILURE;
		} else {
			DBGLOG(NAN, DEBUG, "CANCEL Subscribe Success %X\n",
			       rStatus);
			pNanSubscribeCancelRsp->status = NAN_I_STATUS_SUCCESS;
		}

		i4Status =
			kalIoctl(prGlueInfo, wlanoidNANCancelSubscribeRsp,
				 (void *)pNanSubscribeCancelRsp,
				 sizeof(struct NanSubscribeServiceCancelRspMsg),
				 &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree(pNanSubscribeCancelReq);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanSubscribeCancelReq);
		break;
	}

	case NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ: {
		uint32_t rStatus;
		struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;
		struct NanTransmitFollowupRspMsg *pNanXmitFollowupRsp = NULL;
#if CFG_SUPPORT_NAN_R4_PAIRING
		struct NanPairingBootStrapMsg bootstrap_msg = {0};
		struct NanTransmitFollowupRspMsg_p NanXmitFollowupRsp_p = {0};
#endif

		pNanXmitFollowupReq = kmalloc(
			sizeof(struct NanTransmitFollowupRequest), GFP_ATOMIC);

		if (!pNanXmitFollowupReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		pNanXmitFollowupRsp = kmalloc(
			sizeof(struct NanTransmitFollowupRspMsg), GFP_ATOMIC);
		if (!pNanXmitFollowupRsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanXmitFollowupReq);
			return -ENOMEM;
		}
		kalMemZero(pNanXmitFollowupReq,
			   sizeof(struct NanTransmitFollowupRequest));
		kalMemZero(pNanXmitFollowupRsp,
			   sizeof(struct NanTransmitFollowupRspMsg));

		DBGLOG(NAN, INFO, "Enter Transmit follow up Request\n");

		/* Mapping publish req related parameters */
		readLen = nanMapFollowupReqParams((u32 *)data,
						  pNanXmitFollowupReq);
		remainingLen -= readLen;
		data += readLen;
		pNanXmitFollowupReq->publish_subscribe_id = nanMsgHdr.handle;

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_MAC_ADDRESS:
				if (outputTlv.length >
					NAN_MAC_ADDR_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(pNanXmitFollowupReq->addr,
				       outputTlv.value, outputTlv.length);
				break;
			case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(pNanXmitFollowupReq
					       ->service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanXmitFollowupReq->service_specific_info_len =
					outputTlv.length;
				break;
			case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
				if (outputTlv.length >
					NAN_FW_MAX_TX_FOLLOW_UP_SDEA_LEN) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(pNanXmitFollowupReq
					       ->sdea_service_specific_info,
				       outputTlv.value, outputTlv.length);
				pNanXmitFollowupReq
					->sdea_service_specific_info_len =
					outputTlv.length;
				DBGLOG(NAN, DEBUG,
					"SDEA_SERVICE_SPECIFIC_INFO type:%u len:%u\n",
					outputTlv.type,
					outputTlv.length);
				break;
#if CFG_SUPPORT_NAN_R4_PAIRING
			case NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING:
				if (outputTlv.length >
					sizeof(struct NanPairingBootStrapMsg)) {
					DBGLOG(NAN, ERROR,
					"outputTlv.length is invalid!\n");
					kfree(pNanXmitFollowupReq);
					kfree(pNanXmitFollowupRsp);
					return -EFAULT;
				}
				memcpy(&bootstrap_msg, outputTlv.value,
					sizeof(struct NanPairingBootStrapMsg));
				DBGLOG(NAN, INFO,
					"[pairing-bootstrap]FollowupReq: [boot_type=%d],[boot_method=0x%02x],[boot_status=0x%02x],[u2ComebackAfter=0x%02x]\n",
					bootstrap_msg.bootstrap_type,
					bootstrap_msg.bootstrap_method,
					bootstrap_msg.bootstrap_status,
					bootstrap_msg.u2ComebackAfter);

				pNanXmitFollowupReq->bootstrap_type =
					bootstrap_msg.bootstrap_type;
				pNanXmitFollowupReq->bootstrap_method =
					bootstrap_msg.bootstrap_method;
				pNanXmitFollowupReq->bootstrap_status =
					bootstrap_msg.bootstrap_status;
				if (bootstrap_msg.u2ComebackAfter > 0) {
					pNanXmitFollowupReq->comeback_after
					= bootstrap_msg.u2ComebackAfter;
					pNanXmitFollowupReq->comeback_enable
					= TRUE;
				}
				break;
#endif
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}



#if CFG_SUPPORT_NAN_R4_PAIRING
		/* FIXME: Refactor follow up of pairing command to
		 * bootstramping req/resp command, WiFi HAL should be
		 updated for pairing.
		 */
		DBGLOG(NAN, INFO,
		   "[%s]: pNanXmitFollowupReq->addr=>"MACSTR_A"\n",
		   __func__,
		   pNanXmitFollowupReq->addr[0], pNanXmitFollowupReq->addr[1],
		   pNanXmitFollowupReq->addr[2], pNanXmitFollowupReq->addr[3],
		   pNanXmitFollowupReq->addr[4], pNanXmitFollowupReq->addr[5]);

		if (g_nikExchange.ucNanIdKey)
			pNanXmitFollowupReq->nan_id_key = TRUE;

		if (g_pairingSetupCmd.ucPairingSetup ||
			g_pairingSetupCmd.ucPairingVerification) {
			pNanXmitFollowupReq->pairing_verification =
				g_pairingSetupCmd.ucPairingVerification;
			pNanXmitFollowupReq->bootstrap_method =
				g_pairingSetupCmd.ucBootstrapMethod;
			pNanXmitFollowupReq->pairing_type =
				g_pairingSetupCmd.ucPairingType;
			nanCmdPairingSetupReq(prAdapter, pNanXmitFollowupReq);
			nanClearPairingGlobalSettings(prAdapter);
			kfree(pNanXmitFollowupReq);
			return WLAN_STATUS_SUCCESS;
		}
#endif
#if CFG_SUPPORT_NAN_R4_PAIRING
		if (prAdapter->fgIsNANfromHAL == TRUE) {
			DBGLOG(NAN, INFO,
				"[pairing-bootstrap] Method=%u, Type=%u, Status=%u, comeback_after=%u\n]",
				pNanXmitFollowupReq->bootstrap_method,
				pNanXmitFollowupReq->bootstrap_type,
				pNanXmitFollowupReq->bootstrap_status,
				pNanXmitFollowupReq->comeback_after);
		} else {
			if (g_bootstrapMethod.ucBootstrapMethod) {
				pNanXmitFollowupReq->bootstrap_method =
				g_bootstrapMethod.ucBootstrapMethod;
				pNanXmitFollowupReq->bootstrap_type =
				g_bootstrapMethod.ucBootstrapType;
				pNanXmitFollowupReq->bootstrap_status =
				g_bootstrapMethod.ucBootstrapStatus;
				pNanXmitFollowupReq->comeback_enable =
				g_bootstrapMethod.ucComebackEnabled;
				pNanXmitFollowupReq->comeback_after =
				g_bootstrapMethod.u2ComebackAfter;
			}
			nanClearPairingGlobalSettings(prAdapter);
		}
#endif

		/* Follow up Command reply message */
		memcpy(&pNanXmitFollowupRsp->fwHeader, &nanMsgHdr,
				sizeof(struct _NanMsgHeader));

		pNanXmitFollowupReq->transaction_id =
			pNanXmitFollowupRsp->fwHeader.transactionId;

		skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			sizeof(struct NanTransmitFollowupRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanXmitFollowupReq);
			kfree(pNanXmitFollowupRsp);
			return -ENOMEM;
		}

		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct NanTransmitFollowupRspMsg),
			pNanXmitFollowupRsp) < 0)) {
			kfree(pNanXmitFollowupReq);
			kfree(pNanXmitFollowupRsp);
			kfree_skb(skb);
			DBGLOG(NAN, ERROR, "Fail send reply\n");
			return -EFAULT;
		}

		rStatus = nanTransmitRequest(prGlueInfo->prAdapter,
				pNanXmitFollowupReq);
		if (rStatus != WLAN_STATUS_SUCCESS)
			pNanXmitFollowupRsp->status =
				NAN_I_STATUS_DE_FAILURE;
		else
			pNanXmitFollowupRsp->status =
				NAN_I_STATUS_SUCCESS;

		nanExtTerminateApNanEndLegacy(prAdapter);

#if CFG_SUPPORT_NAN_R4_PAIRING
		NanXmitFollowupRsp_p.pfollowRsp = pNanXmitFollowupRsp;
		NanXmitFollowupRsp_p.bootstrap_type =
			pNanXmitFollowupReq->bootstrap_type;
		NanXmitFollowupRsp_p.bootstrap_status =
			pNanXmitFollowupReq->bootstrap_status;
		i4Status = kalIoctl(prGlueInfo, wlanoidNANFollowupRsp,
				    (void *)&NanXmitFollowupRsp_p,
				    sizeof(struct NanTransmitFollowupRspMsg_p),
				    &u4BufLen);
#else
		i4Status = kalIoctl(prGlueInfo, wlanoidNANFollowupRsp,
				    (void *)pNanXmitFollowupRsp,
				    sizeof(struct NanTransmitFollowupRspMsg),
				    &u4BufLen);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree(pNanXmitFollowupReq);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanXmitFollowupReq);
		break;
	} /* NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ */

	case NAN_MSG_ID_BEACON_SDF_REQ: {
		u16 vsa_length = 0;
		u32 *pXmitVSAparms = NULL;
		struct NanTransmitVendorSpecificAttribute *pNanXmitVSAttrReq =
			NULL;
		struct NanBeaconSdfPayloadRspMsg *pNanBcnSdfVSARsp = NULL;

		DBGLOG(NAN, DEBUG, "Enter Beacon SDF Request.\n");

		pNanXmitVSAttrReq = kmalloc(
			sizeof(struct NanTransmitVendorSpecificAttribute),
			GFP_ATOMIC);

		if (!pNanXmitVSAttrReq) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		pNanBcnSdfVSARsp = kmalloc(
			sizeof(struct NanBeaconSdfPayloadRspMsg), GFP_ATOMIC);

		if (!pNanBcnSdfVSARsp) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			kfree(pNanXmitVSAttrReq);
			return -ENOMEM;
		}

		while ((remainingLen >= 4) &&
		       (0 !=
			(readLen = nan_read_tlv((u8 *)data, &outputTlv)))) {
			switch (outputTlv.type) {
			case NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_TRANSMIT:
				pXmitVSAparms = (u32 *)outputTlv.value;
				pNanXmitVSAttrReq->payload_transmit_flag =
					(u8)(*pXmitVSAparms & BIT(0));
				pNanXmitVSAttrReq->tx_in_discovery_beacon =
					(u8)(*pXmitVSAparms & BIT(1));
				pNanXmitVSAttrReq->tx_in_sync_beacon =
					(u8)(*pXmitVSAparms & BIT(2));
				pNanXmitVSAttrReq->tx_in_service_discovery =
					(u8)(*pXmitVSAparms & BIT(3));
				pNanXmitVSAttrReq->vendor_oui =
					*pXmitVSAparms & BITS(8, 31);
				outputTlv.value += 4;

				vsa_length = outputTlv.length - sizeof(u32);
				if (vsa_length >
					NAN_MAX_VSA_DATA_LEN) {
					DBGLOG(NAN, ERROR,
						"outputTlv.length is invalid!\n");
					kfree(pNanXmitVSAttrReq);
					kfree(pNanBcnSdfVSARsp);
					return -EFAULT;
				}
				memcpy(pNanXmitVSAttrReq->vsa, outputTlv.value,
				       vsa_length);
				pNanXmitVSAttrReq->vsa_len = vsa_length;
				break;
			default:
				break;
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		/* To be implement
		 * Beacon SDF VSA request.................................
		 * rStatus = ;
		 */

		/* Prepare Beacon Sdf Payload Response */
		memcpy(&pNanBcnSdfVSARsp->fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		pNanBcnSdfVSARsp->fwHeader.msgId = NAN_MSG_ID_BEACON_SDF_RSP;
		pNanBcnSdfVSARsp->fwHeader.msgLen =
			sizeof(struct NanBeaconSdfPayloadRspMsg);

		pNanBcnSdfVSARsp->status = NAN_I_STATUS_SUCCESS;

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanBeaconSdfPayloadRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			kfree(pNanXmitVSAttrReq);
			kfree(pNanBcnSdfVSARsp);
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(
				     skb,
				     sizeof(struct NanBeaconSdfPayloadRspMsg),
				     pNanBcnSdfVSARsp) < 0)) {
			kfree(pNanXmitVSAttrReq);
			kfree(pNanBcnSdfVSARsp);
			kfree_skb(skb);
			return -EFAULT;
		}

		ret = cfg80211_vendor_cmd_reply(skb);

		kfree(pNanXmitVSAttrReq);
		kfree(pNanBcnSdfVSARsp);

		break;
	}
#if CFG_SUPPORT_NAN_R4_PAIRING
	case NAN_MSG_ID_PAIRING_REQUEST: {
		struct _NanPairingRequestParams pairingRequest;
		struct _NanPairingRequestRspMsg nanPairingRequestRsp;
		uint32_t rRetStatus = WLAN_STATUS_SUCCESS;


		DBGLOG(NAN, INFO,
			"sizeof(NanPairingRequestParams)=%zu, sizeof(_NanMsgHeader)=%zu, data_len=%d\n",
			sizeof(struct _NanPairingRequestParams),
			sizeof(struct _NanMsgHeader),
			data_len);
		kalMemZero(&pairingRequest,
			sizeof(struct _NanPairingRequestParams));
		readLen = nanMapPairingRequestParams((u8 *)data,
				&pairingRequest);

		switch (pairingRequest.nan_pairing_request_type) {
		case NAN_PAIRING_SETUP_REQ_T: {
			rRetStatus = nanCommandPairingRequest_Pairing(prAdapter,
					&pairingRequest);
			break;
		}
		case NAN_PAIRING_VERIFICATION_REQ_T: {
			uint16_t publish_subscribe_id = nanMsgHdr.handle;

			rRetStatus = nanCommandPairingRequest_Verify(prAdapter,
					&pairingRequest, publish_subscribe_id);
			break;
		}
		default: {
			break;
		}
		}
		if (rRetStatus == WLAN_STATUS_SUCCESS)
			nanPairingRequestRsp.status = NAN_I_STATUS_SUCCESS;
		else
			nanPairingRequestRsp.status = NAN_I_STATUS_DE_FAILURE;

		memcpy(&nanPairingRequestRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct _NanPairingRequestRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct _NanPairingRequestRspMsg),
			&nanPairingRequestRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidNANPairingRequestRsp,
			(void *)&nanPairingRequestRsp,
			sizeof(struct _NanPairingRequestRspMsg),
			&u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			kfree_skb(skb);
			return -EFAULT;
		}
		DBGLOG(NAN, ERROR, "i4Status = %u\n", i4Status);

		ret = cfg80211_vendor_cmd_reply(skb);
		break;
	}
	case NAN_MSG_ID_PAIRING_RESPONSE: {
		struct _NanPairingResponseParams pairingResponse;
		struct _NanPairingResponseRspMsg nanPairingResponseRsp;
		uint32_t rRetStatus = WLAN_STATUS_SUCCESS;

		DBGLOG(NAN, INFO, "data_len=%d\n", data_len);
		kalMemZero(&pairingResponse,
			sizeof(struct _NanPairingResponseParams));
		readLen = nanMapPairingResponseParams((u8 *)data,
				&pairingResponse);
		data += readLen;
		remainingLen -= readLen;

		switch (pairingResponse.nan_pairing_request_type) {
		case NAN_PAIRING_SETUP_REQ_T: {
			rRetStatus = nanCommandPairingRespond_Pairing(prAdapter,
					&pairingResponse, &data, remainingLen);
			break;
		}
		case NAN_PAIRING_VERIFICATION_REQ_T: {
			uint64_t publish_subscribe_id = nanMsgHdr.handle;

			rRetStatus = nanCommandPairingRespond_Verify(prAdapter,
					&pairingResponse, &data, remainingLen,
					publish_subscribe_id);
			break;
		}
		default: {
			break;
		}
		}
		if (rRetStatus == WLAN_STATUS_SUCCESS)
			nanPairingResponseRsp.status = NAN_I_STATUS_SUCCESS;
		else
			nanPairingResponseRsp.status = NAN_I_STATUS_DE_FAILURE;

		memcpy(&nanPairingResponseRsp.fwHeader, &nanMsgHdr,
		       sizeof(struct _NanMsgHeader));
		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct _NanPairingRequestRspMsg));

		if (!skb) {
			DBGLOG(NAN, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct _NanPairingResponseRspMsg),
			&nanPairingResponseRsp) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		i4Status = kalIoctl(prGlueInfo, wlanoidNANPairingResponseRsp,
				    (void *)&nanPairingResponseRsp,
				    sizeof(struct _NanPairingResponseRspMsg),
				    &u4BufLen);
		if (i4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR, "kalIoctl failed\n");
			return -EFAULT;
		}
		DBGLOG(NAN, INFO, "i4Status = %u\n", i4Status);
		ret = cfg80211_vendor_cmd_reply(skb);

		break;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	case NAN_MSG_ID_TESTMODE_REQ:
	{
		struct NanDebugParams *pNanDebug = NULL;

		pNanDebug = kmalloc(sizeof(struct NanDebugParams), GFP_ATOMIC);
		if (!pNanDebug) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}
		kalMemZero(pNanDebug, sizeof(struct NanDebugParams));
		DBGLOG(NAN, INFO, "NAN_MSG_ID_TESTMODE_REQ\n");

		while ((remainingLen >= 4) &&
			(0 != (readLen = nan_read_tlv((u8 *)data,
			&outputTlv)))) {
			DBGLOG(NAN, INFO, "outputTlv.type= %d\n",
				outputTlv.type);
			if (outputTlv.type ==
				NAN_TLV_TYPE_TESTMODE_GENERIC_CMD) {
				if (outputTlv.length >
					sizeof(
					struct NanDebugParams
					)) {
					DBGLOG(NAN, ERROR,
						"outputTlv.length is invalid!\n");
					kfree(pNanDebug);
					return -EFAULT;
				}
				memcpy(pNanDebug, outputTlv.value,
					outputTlv.length);
				switch (pNanDebug->cmd) {
				case NAN_TEST_MODE_CMD_DISABLE_NDPE:
					g_ndpReqNDPE.fgEnNDPE = TRUE;
					g_ndpReqNDPE.ucNDPEAttrPresent =
						pNanDebug->
						debug_cmd_data[0];
					DBGLOG(NAN, DEBUG,
						"NAN_TEST_MODE_CMD_DISABLE_NDPE: fgEnNDPE = %d\n",
						g_ndpReqNDPE.fgEnNDPE);
					break;
#if CFG_SUPPORT_NAN_R4_PAIRING
				case NAN_TEST_MODE_CMD_ENABLE_PAIRING:
					g_pairingPubReq.ucPairingSetupEnabled =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Pairing Enabled = %u\n",
					g_pairingPubReq.ucPairingSetupEnabled);
					break;
				case NAN_TEST_MODE_CMD_CIPHERSUITEI_ID:
					g_bootstrapMethod.ucCipherSuiteId =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Cipher suite id = %u\n",
					g_bootstrapMethod.ucCipherSuiteId);
					break;
				case NAN_TEST_MODE_CMD_CIPHERSUITEI_ID_LIST:
					g_pairingPubReq.aucCipherSuiteList[0] =
						pNanDebug->debug_cmd_data[0];
					g_pairingPubReq.aucCipherSuiteList[1] =
						pNanDebug->debug_cmd_data[1];
					DBGLOG(NAN, INFO,
					"NAN Cipher Suite Id List = %u, %u\n",
					g_pairingPubReq.aucCipherSuiteList[0],
					g_pairingPubReq.aucCipherSuiteList[1]);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_METHOD:
					g_pairingPubReq.ucBootstrapMethod =
						pNanDebug->debug_cmd_data[0];
					g_bootstrapMethod.ucBootstrapMethod =
						pNanDebug->debug_cmd_data[0];
					g_pairingSetupCmd.ucBootstrapMethod =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Method = %u\n",
					g_bootstrapMethod.ucBootstrapMethod);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_TYPE:
					g_bootstrapMethod.ucBootstrapType =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Type = %u\n",
					g_bootstrapMethod.ucBootstrapType);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_STATUS:
					g_bootstrapMethod.ucBootstrapStatus =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Status = %u\n",
					g_bootstrapMethod.ucBootstrapStatus);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_COMEBACK:
					g_bootstrapMethod.ucComebackEnabled =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Bootstrap Comeback = %u\n",
					g_bootstrapMethod.ucComebackEnabled);
					break;
				case NAN_TEST_MODE_CMD_BOOTSTRAP_COMEBACK_AFTER:
					g_bootstrapMethod.u2ComebackAfter =
					pNanDebug->debug_cmd_data[0] +
					pNanDebug->debug_cmd_data[1] * 256;
					DBGLOG(NAN, INFO,
					"NAN Bootstrap ComebackAfter = %u\n",
					g_bootstrapMethod.u2ComebackAfter);
					DBGLOG(NAN, INFO,
					"[NAN] check comeback input = %u, %u\n",
					pNanDebug->debug_cmd_data[0],
					pNanDebug->debug_cmd_data[1]);
					break;
				case NAN_TEST_MODE_CMD_NAN_ID_KEY:
					g_nikExchange.ucNanIdKey =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN NIK Id Key = %u\n",
						g_nikExchange.ucNanIdKey);
					break;
				case NAN_TEST_MODE_CMD_PAIRING_SETUP:
					g_pairingSetupCmd.ucPairingSetup =
						TRUE;
					g_pairingSetupCmd.ucPairingType =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Pairing Setup Type = %u\n",
					g_pairingSetupCmd.ucPairingType);
					break;
				case NAN_TEST_MODE_CMD_PAIRING_VERIFICATION:
					g_pairingSetupCmd.
						ucPairingVerification = TRUE;
					g_pairingSetupCmd.ucPairingType =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
					"NAN Pairing Verification Type = %u\n",
					g_pairingSetupCmd.ucPairingType);
					break;
				case NAN_TEST_MODE_CMD_NIRA_PRESENCE:
					g_pairingPubReq.ucNiraEnabled =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN NIRA Presense = %u\n",
						g_pairingPubReq.ucNiraEnabled);
					break;
				case NAN_TEST_MODE_CMD_NPK_NIK_CACHE:
					g_pairingPubReq.ucNPKNIKCache =
						pNanDebug->debug_cmd_data[0];
					DBGLOG(NAN, INFO,
						"NAN NPK/NIK Cache = %u\n",
						g_pairingPubReq.ucNPKNIKCache);
					break;
				case NAN_TEST_MODE_CMD_FOLLOWUP_TYPE:
					/* from vendor command, req:0, rsp:1 */
					/* for NPBA,0:advertise, 1:req, 2:rsp */
					g_bootstrapMethod.ucBootstrapType =
					pNanDebug->debug_cmd_data[0] + 1;
					DBGLOG(NAN, INFO,
					"NAN Bootstrap type = %u\n",
					g_bootstrapMethod.ucBootstrapType);
					break;
				case NAN_TEST_MODE_CMD_PASSWORD_PINCODE:
				case NAN_TEST_MODE_CMD_PASSWORD_PASSPHRASE:
					kalMemCpyS(g_bootstrapPassword.password,
						NAN_PAIRING_MAX_PASSWORD_SIZE,
						pNanDebug->debug_cmd_data,
						NAN_PAIRING_MAX_PASSWORD_SIZE);
					g_bootstrapPassword.password_len =
						NAN_PAIRING_MAX_PASSWORD_SIZE;
					DBGLOG(NAN, INFO,
						"NAN Bootstrap Password = %s\n",
						pNanDebug->debug_cmd_data);
					nanCmdBootstrapPwdSetup(prAdapter,
						&g_bootstrapPassword);
					break;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
				default:
					break;
				}
			} else {
				DBGLOG(NAN, ERROR,
					"Testmode invalid TLV type\n");
			}
			remainingLen -= readLen;
			data += readLen;
			memset(&outputTlv, 0, sizeof(outputTlv));
		}

		kfree(pNanDebug);
		return 0;
	}
	case NAN_MSG_ID_GET_COUNTRY_CODE:
	{
		struct NanGetCountryCodeRspMsg rCountryCode;
		uint32_t u4CountryCode = 0;
		char acCountryStr[MAX_COUNTRY_CODE_LEN + 1] = {0};

		kalMemZero(&rCountryCode,
			sizeof(struct NanGetCountryCodeRspMsg));
		kalMemCopy(&rCountryCode.fwHeader, &nanMsgHdr,
			sizeof(struct _NanMsgHeader));

		u4CountryCode = rlmDomainGetCountryCode(prAdapter);
		rlmDomainU32ToAlpha(u4CountryCode, acCountryStr);

		rCountryCode.countryCode[0] = acCountryStr[0];
		rCountryCode.countryCode[1] = acCountryStr[1];

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanGetCountryCodeRspMsg));
		if (!skb) {
			DBGLOG(REQ, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct NanGetCountryCodeRspMsg),
					&rCountryCode) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		break;
	}
	case NAN_MSG_ID_GET_INFRA_BSSID:
	{
		struct BSS_INFO *prAisBssInfo = (struct BSS_INFO *) NULL;
		struct NanGetInfraBssidRspMsg rInfraBssid;

		prAisBssInfo = aisGetAisBssInfo(prAdapter, AIS_DEFAULT_INDEX);
		if (prAisBssInfo == NULL) {
			DBGLOG(REQ, ERROR, "prAisBssInfo is null\n");
			return -EFAULT;
		}

		kalMemCopy(&rInfraBssid.fwHeader, &nanMsgHdr,
			sizeof(struct _NanMsgHeader));
		if (prAisBssInfo->eConnectionState ==
			MEDIA_STATE_CONNECTED) {
			COPY_MAC_ADDR(rInfraBssid.MacAddr,
				prAisBssInfo->aucBSSID);
		} else {
			kalMemSet(rInfraBssid.MacAddr, 0, MAC_ADDR_LEN);
		}

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanGetInfraBssidRspMsg));
		if (!skb) {
			DBGLOG(REQ, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct NanGetInfraBssidRspMsg),
					   &rInfraBssid) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		break;
	}
	case NAN_MSG_ID_GET_INFRA_CHANNEL:
	{
		struct BSS_INFO *prAisBssInfo = (struct BSS_INFO *) NULL;
		struct NanGetInfraChannelRspMsg rInfraCh;

		prAisBssInfo = aisGetAisBssInfo(prAdapter, AIS_DEFAULT_INDEX);
		if (prAisBssInfo == NULL) {
			DBGLOG(REQ, ERROR, "prAisBssInfo is null\n");
			return -EFAULT;
		}

		kalMemCopy(&rInfraCh.fwHeader, &nanMsgHdr,
			sizeof(struct _NanMsgHeader));
		if (prAisBssInfo->eConnectionState ==
			MEDIA_STATE_CONNECTED) {
			rInfraCh.Channel = prAisBssInfo->ucPrimaryChannel;
			rInfraCh.Flag = 0;
			/* Bandwidth */
			if (prAisBssInfo->ucVhtChannelWidth ==
				VHT_OP_CHANNEL_WIDTH_20_40) {
				if (prAisBssInfo->eBssSCO ==
					CHNL_EXT_SCN) {
					rInfraCh.Flag |=
						NAN_C_FLAG_20MHZ;
				} else if (prAisBssInfo->eBssSCO ==
				CHNL_EXT_SCA) {
					rInfraCh.Flag |=
						NAN_C_FLAG_40MHZ;
					rInfraCh.Flag |=
						NAN_C_FLAG_EXTENSION_ABOVE;
				} else if (prAisBssInfo->eBssSCO ==
				CHNL_EXT_SCB) {
					rInfraCh.Flag |=
						NAN_C_FLAG_40MHZ;
				}
			} else if (prAisBssInfo->ucVhtChannelWidth ==
				VHT_OP_CHANNEL_WIDTH_80) {
				rInfraCh.Flag |=
					NAN_C_FLAG_80MHZ;
			} else if (prAisBssInfo->ucVhtChannelWidth ==
				VHT_OP_CHANNEL_WIDTH_160) {
				rInfraCh.Flag |=
					NAN_C_FLAG_160MHZ;
			}
			/* Band */
			if (prAisBssInfo->eBand == BAND_2G4)
				rInfraCh.Flag |= NAN_C_FLAG_2GHZ;
			else if (prAisBssInfo->eBand == BAND_5G)
				rInfraCh.Flag |= NAN_C_FLAG_5GHZ;
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prAisBssInfo->eBand == BAND_6G)
				rInfraCh.Flag |= NAN_C_FLAG_6GHZ;
#endif /* CFG_SUPPORT_WIFI_6G */
		} else {
			rInfraCh.Channel = 0;
			rInfraCh.Flag = 0;
		}

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanGetInfraChannelRspMsg));
		if (!skb) {
			DBGLOG(REQ, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct NanGetInfraChannelRspMsg),
					   &rInfraCh) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		break;
	}
	case NAN_MSG_ID_GET_DRIVER_CAPABILITIES:
	{
		struct NanDriverCapabilitiesRspMsg rDriverCapabilities;

		kalMemZero(&rDriverCapabilities,
			sizeof(struct NanDriverCapabilitiesRspMsg));
		kalMemCopy(&rDriverCapabilities.fwHeader, &nanMsgHdr,
			sizeof(struct _NanMsgHeader));

		rDriverCapabilities.capabilities =
			WFPAL_WIFI_DRIVER_SUPPORTS_NAN |
			WFPAL_WIFI_DRIVER_SUPPORTS_DUAL_BAND;
		/* nonDBDC case shall be 0 */
		rDriverCapabilities.capabilities |=
			(prAdapter->rWifiVar.eDbdcMode > 0) ?
			WFPAL_WIFI_DRIVER_SUPPORTS_SIMULTANEOUS_DUAL_BAND : 0;

		skb = cfg80211_vendor_cmd_alloc_reply_skb(
			wiphy, sizeof(struct NanDriverCapabilitiesRspMsg));
		if (!skb) {
			DBGLOG(REQ, ERROR, "Allocate skb failed\n");
			return -ENOMEM;
		}
		if (unlikely(nla_put_nohdr(skb,
			sizeof(struct NanDriverCapabilitiesRspMsg),
					&rDriverCapabilities) < 0)) {
			kfree_skb(skb);
			return -EFAULT;
		}
		break;
	}

	case NAN_MSG_ID_SET_COMMITTED_AVAILABILITY:
	{
		struct _NanCommittedAvailability *prCommittedAvailability;
		struct _NanChannelAvailabilityEntry *prChnlEntry;
		union _NAN_BAND_CHNL_CTRL rChnlInfo = {0};
		union _NAN_AVAIL_ENTRY_CTRL rEntryCtrl = {0};
		uint8_t ucMapId, ucNumMaps, ucNumChnlEntries;
		uint8_t ucMapIdx, ucChnlEntryIdx;
		//size_t i, j;
		size_t message_len = 0;
		unsigned char fgNonContinuousBw = FALSE;
		uint16_t u2TimeBitmapControl = 0;
		uint32_t au4AvailMap[NAN_TOTAL_DW] = {0};

		DBGLOG(REQ, INFO,
			"GET NAN_MSG_ID_SET_COMMITTED_AVAILABILITY\n");

		message_len = sizeof(struct _NanCommittedAvailability);
		prCommittedAvailability = kalMemAlloc(message_len,
			VIR_MEM_TYPE);

		if (!prCommittedAvailability) {
			DBGLOG(REQ, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		kalMemZero(prCommittedAvailability, message_len);
		kalMemCopy(prCommittedAvailability, (u8 *)data, message_len);

#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
		nanSchedResetCommitedAvailability(prAdapter);
#endif

		ucNumMaps = prCommittedAvailability->num_maps_ids;

		for (ucMapIdx = 0; ucMapIdx < ucNumMaps; ucMapIdx++) {
			ucMapId =
				prCommittedAvailability
				->schedule[ucMapIdx].map_id;
			ucNumChnlEntries =
				prCommittedAvailability
				->schedule[ucMapIdx].num_entries;

			DBGLOG(NAN, INFO, "MapId:%d, MapNum:%d, EntryNumb:%d\n",
					ucMapId, ucNumMaps, ucNumChnlEntries);

			if ((ucMapIdx >= NAN_MAX_MAP_IDS) ||
				(ucMapIdx >= NAN_TIMELINE_MGMT_SIZE)) {
				DBGLOG(NAN, ERROR,
					"Map number (%d, %d) exceed cap\n",
					ucMapIdx, ucNumMaps);
				kalMemFree(
					prCommittedAvailability,
					VIR_MEM_TYPE,
					message_len);
				return -EINVAL;
			}

			for (ucChnlEntryIdx = 0;
			     ucChnlEntryIdx < ucNumChnlEntries;
			     ucChnlEntryIdx++) {
				if (ucChnlEntryIdx >=
				    NAN_MAX_AVAILABILITY_CHANNEL_ENTRIES ||
				    ucChnlEntryIdx >=
				    NAN_TIMELINE_MGMT_CHNL_LIST_NUM) {
					DBGLOG(NAN, ERROR,
					  "Chnl number (%d, %d) exceed cap\n",
					  ucChnlEntryIdx, ucNumChnlEntries);
					kalMemFree(
						prCommittedAvailability,
						VIR_MEM_TYPE,
						message_len);
					return -EINVAL;
				}

				prChnlEntry =
					&(prCommittedAvailability
					->schedule[ucMapIdx]
					.channel_entries[ucChnlEntryIdx]);

				/* convert to _NAN_BAND_CHNL_CTRL */
				if (prChnlEntry->auxiliary_channel_bitmap !=
					0) {
					fgNonContinuousBw = TRUE;
					rChnlInfo.u4AuxCenterChnl =
					  nanRegGetChannelByOrder(
					    prChnlEntry->op_class,
					    &prChnlEntry
					    ->auxiliary_channel_bitmap);
				}
				rChnlInfo.u4Type =
					NAN_BAND_CH_ENTRY_LIST_TYPE_CHNL;
				rChnlInfo.u4OperatingClass =
					prChnlEntry->op_class;
				rChnlInfo.u4PrimaryChnl =
					nanRegGetPrimaryChannelByOrder(
						prChnlEntry->op_class,
						&prChnlEntry->op_class_bitmap,
						fgNonContinuousBw,
						prChnlEntry
						->primary_channel_bitmap);

				/* convert to _NAN_AVAIL_ENTRY_CTRL */
				rEntryCtrl.b3Type =
				NAN_AVAIL_ENTRY_CTRL_AVAIL_TYPE_COMMIT;
				rEntryCtrl.b2Preference =
					prChnlEntry->usage_preference;
				rEntryCtrl.b3Util =
					prChnlEntry->utilization;
				rEntryCtrl.b4RxNss =
					prChnlEntry->rx_nss;
				rEntryCtrl.b1TimeMapAvail =
					((prChnlEntry
					->time_bitmap.time_bitmap_length
					== 0) ? 0 : 1);

				/* convert to au4AvailMap */
				kalMemZero(au4AvailMap, sizeof(au4AvailMap));
				if (rEntryCtrl.b1TimeMapAvail == 0) {
					kalMemSet(au4AvailMap, 0xFF,
						sizeof(au4AvailMap));
				} else {
					u2TimeBitmapControl =
					((prChnlEntry->time_bitmap.bitDuration
					<< NAN_TIME_BITMAP_CTRL_DURATION_OFFSET)
					& NAN_TIME_BITMAP_CTRL_DURATION) |
					((prChnlEntry->time_bitmap.period
					<< NAN_TIME_BITMAP_CTRL_PERIOD_OFFSET)
					& NAN_TIME_BITMAP_CTRL_PERIOD) |
					((prChnlEntry->time_bitmap.offset
					<<
					NAN_TIME_BITMAP_CTRL_STARTOFFSET_OFFSET)
					& NAN_TIME_BITMAP_CTRL_STARTOFFSET);

					nanParserInterpretTimeBitmapField(
						prAdapter, u2TimeBitmapControl,
						prChnlEntry
						->time_bitmap
						.time_bitmap_length,
						prChnlEntry
						->time_bitmap.time_bitmap,
						au4AvailMap);
				}

#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
				nanSchedConfigCommitedAvailability(
					prAdapter,
					ucMapId,
					rChnlInfo,
					rEntryCtrl,
					au4AvailMap);
#endif
			}
		}
#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
		/* Sync setting to firmware */
		nanSchedNegoSyncSchUpdateFsmStep(
			prAdapter, ENUM_NAN_SYNC_SCH_UPDATE_STATE_IDLE);
#endif
		kalMemFree(prCommittedAvailability, VIR_MEM_TYPE, message_len);

		return 0;
	}
	case NAN_MSG_ID_SET_POTENTIAL_AVAILABILITY:
	{
		struct _NanPotentialAvailability *
			prPotentialAvailability = NULL;
		struct _NanChannelAvailabilityEntry *
			prChnlEntry = NULL;
		union _NAN_BAND_CHNL_CTRL rChnlInfo = {0};
		union _NAN_AVAIL_ENTRY_CTRL rEntryCtrl = {0};
		uint8_t ucNumMaps = 0, ucNumChnlEntries = 0;
		uint8_t ucMapIdx = 0, ucNumBandEntries = 0;
		uint8_t ucChnlEntryIdx = 0, ucBandEntryIdx = 0;
		unsigned char fgNonContinuousBw = FALSE;
		uint16_t u2TimeBitmapControl = 0;
		uint32_t au4AvailMap[NAN_TOTAL_DW] = {0};
		uint8_t ucBandId = 0, ucMapId = 0;
		size_t message_len = 0;
		uint32_t u4DurOf = NAN_TIME_BITMAP_CTRL_DURATION_OFFSET;
		uint32_t u4Dur = NAN_TIME_BITMAP_CTRL_DURATION;
		uint32_t u4PeOf = NAN_TIME_BITMAP_CTRL_PERIOD_OFFSET;
		uint32_t u4CtPe = NAN_TIME_BITMAP_CTRL_PERIOD;
		uint32_t u4StaOf = NAN_TIME_BITMAP_CTRL_STARTOFFSET_OFFSET;
		uint32_t u4Sta = NAN_TIME_BITMAP_CTRL_STARTOFFSET;

		DBGLOG(REQ, INFO,
			"GET NAN_MSG_ID_SET_POTENTIAL_AVAILABILITY\n");

		message_len = sizeof(struct _NanPotentialAvailability);
		prPotentialAvailability =
			kalMemAlloc(message_len, VIR_MEM_TYPE);

		if (!prPotentialAvailability) {
			DBGLOG(REQ, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		kalMemZero(prPotentialAvailability, message_len);
		kalMemCopy(prPotentialAvailability, (u8 *)data, message_len);

#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
		nanSchedResetPotentialAvailability(prAdapter);
#endif

		ucNumMaps = prPotentialAvailability->num_maps_ids;

		for (ucMapIdx = 0; ucMapIdx < ucNumMaps; ucMapIdx++) {
			ucNumBandEntries =
				prPotentialAvailability
				->potential[ucMapIdx]
				.num_band_entries;
			ucNumChnlEntries =
				prPotentialAvailability
				->potential[ucMapIdx]
				.num_entries;
			ucMapId =
				prPotentialAvailability
				->potential[ucMapIdx]
				.map_id;

			if ((ucMapIdx >= NAN_MAX_MAP_IDS) ||
				(ucMapIdx >= NAN_TIMELINE_MGMT_SIZE)) {
				DBGLOG(NAN, ERROR,
					"Map number (%d, %d) exceed cap\n",
					ucMapIdx, ucNumMaps);
				kalMemFree(prPotentialAvailability,
					VIR_MEM_TYPE,
					message_len);
				return -EINVAL;
			}

			DBGLOG(NAN, INFO,
				"MapId: %u Band number: %d, chnl entry number: %d\n",
				ucMapId, ucNumBandEntries, ucNumChnlEntries);

			if (ucNumBandEntries) {
				for (ucBandEntryIdx = 0;
					ucBandEntryIdx < ucNumBandEntries;
					ucBandEntryIdx++) {
					ucBandId =
					  prPotentialAvailability
					  ->potential[ucMapIdx]
					  .band_ids[ucBandEntryIdx];

					rChnlInfo.u4Type =
					  NAN_BAND_CH_ENTRY_LIST_TYPE_BAND;
					rChnlInfo.u4BandIdMask |=
					  BIT(ucBandId);
				}
#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
				nanSchedConfigPotentialAvailability(
					prAdapter,
					ucMapId,
					rChnlInfo,
					rEntryCtrl,
					au4AvailMap);
#endif
				if (ucBandEntryIdx > NAN_MAX_BAND_IDS) {
					DBGLOG(NAN, ERROR,
					  "Band number (%d, %d) exceed cap\n",
					  ucBandEntryIdx,
					  ucNumBandEntries);
					kalMemFree(prPotentialAvailability,
					  VIR_MEM_TYPE,
					  message_len);
					return -EINVAL;
				}

			} else if (ucNumChnlEntries) {
				for (ucChnlEntryIdx = 0;
					ucChnlEntryIdx < ucNumChnlEntries;
					ucChnlEntryIdx++) {

					if ((ucChnlEntryIdx >=
					  NAN_MAX_AVAILABILITY_CHANNEL_ENTRIES)
					  || (ucChnlEntryIdx
					  >= NAN_MAX_POTENTIAL_CHNL_LIST)) {
						DBGLOG(NAN, ERROR,
						  "Chnl number (%d, %d) exceed cap\n",
						  ucChnlEntryIdx,
						  ucNumChnlEntries);
						kalMemFree(
						  prPotentialAvailability,
						  VIR_MEM_TYPE,
						  message_len);
						return -EINVAL;
					}

					prChnlEntry =
					  &(prPotentialAvailability
					  ->potential[ucMapIdx]
					  .channel_entries[ucChnlEntryIdx]);
					/* convert to _NAN_BAND_CHNL_CTRL */
					if (prChnlEntry
					->auxiliary_channel_bitmap !=
					0) {
						fgNonContinuousBw = TRUE;
						rChnlInfo
						.u4AuxCenterChnl =
						  nanRegGetChannelByOrder(
						    prChnlEntry->op_class,
						    &prChnlEntry
						    ->auxiliary_channel_bitmap);
					}
					rChnlInfo.u4Type =
					  NAN_BAND_CH_ENTRY_LIST_TYPE_CHNL;
					rChnlInfo.u4OperatingClass =
						prChnlEntry->op_class;
					rChnlInfo.u4PrimaryChnl =
						nanRegGetPrimaryChannelByOrder(
						  prChnlEntry->op_class,
						  &prChnlEntry
						  ->op_class_bitmap,
						  fgNonContinuousBw,
						  prChnlEntry
						  ->primary_channel_bitmap);

					/* convert to _NAN_AVAIL_ENTRY_CTRL*/
					rEntryCtrl.b3Type =
					  NAN_AVAIL_ENTRY_CTRL_AVAIL_TYPE_POTN;
					rEntryCtrl.b2Preference =
					  prChnlEntry->usage_preference;
					rEntryCtrl.b3Util =
					  prChnlEntry->utilization;
					rEntryCtrl.b4RxNss =
					  prChnlEntry->rx_nss;
					rEntryCtrl.b1TimeMapAvail =
					  ((prChnlEntry
					  ->time_bitmap
					  .time_bitmap_length ==
						0) ? 0 : 1);

					/* convert to au4AvailMap */
					if (rEntryCtrl
					.b1TimeMapAvail == 1) {
						u2TimeBitmapControl =
						((prChnlEntry
						->time_bitmap
						.bitDuration
						<< u4DurOf)
						& u4Dur) |
						((prChnlEntry
						->time_bitmap
						.period
						<< u4PeOf)
						& u4CtPe) |
						((prChnlEntry
						->time_bitmap
						.offset
						<< u4StaOf)
						& u4Sta);

					  nanParserInterpretTimeBitmapField
					  (prAdapter,
					  u2TimeBitmapControl,
					  prChnlEntry
					  ->time_bitmap
					  .time_bitmap_length,
					  prChnlEntry
					  ->time_bitmap
					  .time_bitmap,
					  au4AvailMap);
					}
#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
					nanSchedConfigPotentialAvailability(
						prAdapter, ucMapId, rChnlInfo,
						rEntryCtrl, au4AvailMap);
#endif
				}
			}
		}

		/* Sync setting to firmware */
#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
		nanSchedCmdUpdatePotentialChnlAvail(prAdapter);
#endif
		kalMemFree(prPotentialAvailability, VIR_MEM_TYPE,
			message_len);

		return 0;
	}
	case NAN_MSG_ID_SET_DATA_CLUSTER_AVAILABILITY:
	{
		struct _NanDataClusterAvailability *prNdcAvailability = NULL;
		struct _NanDataClusterAvailabilityParams *prNdcParam = NULL;
		uint8_t ucMapId = 0, ucNumMaps = 0;
		uint8_t ucMapIdx = 0;
		uint16_t u2TimeBitmapControl = 0;
		uint32_t au4AvailMap[NAN_TOTAL_DW] = {0};
		size_t message_len = 0;

		DBGLOG(REQ, INFO,
			"GET NAN_MSG_ID_SET_DATA_CLUSTER_AVAILABILITY\n");

		message_len = sizeof(struct _NanDataClusterAvailability);
		prNdcAvailability = kalMemAlloc(message_len, VIR_MEM_TYPE);

		if (!prNdcAvailability) {
			DBGLOG(REQ, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		kalMemZero(prNdcAvailability, message_len);
		kalMemCopy(prNdcAvailability, (u8 *)data, message_len);

		ucNumMaps = prNdcAvailability->num_maps_ids;

		for (ucMapIdx = 0; ucMapIdx < ucNumMaps; ucMapIdx++) {
			prNdcParam = &(prNdcAvailability->ndc[ucMapIdx]);
			ucMapId = prNdcParam->map_id;

			DBGLOG(NAN, INFO,
				"MapNum:%d, MapId:%d, sel:%d\n",
				ucNumMaps, ucMapId, prNdcParam->selected);
			if ((ucMapIdx >= NAN_MAX_MAP_IDS) ||
				(ucMapIdx >= NAN_TIMELINE_MGMT_SIZE)) {
				DBGLOG(NAN, ERROR,
					"Map number (%d, %d) exceed cap\n",
					ucMapIdx, ucNumMaps);
				kalMemFree(prNdcAvailability, VIR_MEM_TYPE,
					message_len);
				return -EINVAL;
			}

			if (prNdcParam->selected) {
				/* convert to au4AvailMap */
				u2TimeBitmapControl =
				((prNdcParam->time_bitmap.bitDuration
				<< NAN_TIME_BITMAP_CTRL_DURATION_OFFSET)
				& NAN_TIME_BITMAP_CTRL_DURATION) |
				((prNdcParam->time_bitmap.period
				<< NAN_TIME_BITMAP_CTRL_PERIOD_OFFSET)
				& NAN_TIME_BITMAP_CTRL_PERIOD) |
				((prNdcParam->time_bitmap.offset
				<< NAN_TIME_BITMAP_CTRL_STARTOFFSET_OFFSET)
				& NAN_TIME_BITMAP_CTRL_STARTOFFSET);

				kalMemZero(au4AvailMap, sizeof(au4AvailMap));
				nanParserInterpretTimeBitmapField(
					prAdapter, u2TimeBitmapControl,
					prNdcParam->time_bitmap
					.time_bitmap_length,
					prNdcParam->time_bitmap
					.time_bitmap,
					au4AvailMap);
#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
				nanSchedConfigNdcAvailability(
					prAdapter, ucMapId, au4AvailMap,
					prNdcParam->ndc_id.octet);
#endif
			}
			DBGLOG(NAN, ERROR,
				"sel: %d, MapId:%d, BitmapCtrl:0x%x\n",
				prNdcParam->selected,
				ucMapId,
				u2TimeBitmapControl);
			DBGDUMP_HEX(NAN, INFO, "NDC ID",
				    prNdcParam->ndc_id.octet, MAC_ADDR_LEN);
		}

		kalMemFree(prNdcAvailability, VIR_MEM_TYPE,
			message_len);

		return 0;
	}

	case NAN_MSG_ID_FORCED_BEACON_TRANSMISSION:
	{
		struct _NanForcedDiscBeaconTransmission
					*prNanDiscBcnTrans = NULL;
		struct _NanForcedDiscBeaconTxAvailability *prAvail = NULL;
		struct _NanForcedDiscBeaconTxAvailabilityParams
					*prParams = NULL;
		struct _NAN_CMD_EVENT_SET_DISC_BCN_T rNanSetDiscBcn = {};
		uint32_t rStatus = 0;
		uint16_t u2TimeBitmapControl = 0;
		size_t message_len = 0, i = 0;
		uint32_t au4AvailMap[NAN_TOTAL_DW] = {0};

		DBGLOG(REQ, INFO,
			"GET NAN_MSG_ID_SET_DATA_CLUSTER_AVAILABILITY\n");

		message_len = sizeof(struct _NanForcedDiscBeaconTransmission);
		prNanDiscBcnTrans = kalMemAlloc(message_len, VIR_MEM_TYPE);

		if (!prNanDiscBcnTrans) {
			DBGLOG(REQ, ERROR, "Allocate failed\n");
			return -ENOMEM;
		}

		kalMemZero(prNanDiscBcnTrans, message_len);
		kalMemCopy(prNanDiscBcnTrans, (u8 *)data, message_len);

		prAvail = &prNanDiscBcnTrans->availability;

		if (!prNanDiscBcnTrans->enable)
			return 0;

		DBGLOG(NAN, INFO,
			"[FastDisc] Enable: %d, num_maps_ids: %d, BcnItv: %d\n",
			prNanDiscBcnTrans->enable,
			prAvail->num_maps_ids,
			prNanDiscBcnTrans->beacon_interval);

		if ((prAvail->num_maps_ids == 0 &&
		     prNanDiscBcnTrans->beacon_interval == 0) ||
		    (prAvail->num_maps_ids != 0 &&
		     prNanDiscBcnTrans->beacon_interval != 0)) {
			DBGLOG(NAN, ERROR, "[FastDisc] Wrong Input\n");
			return -EINVAL;
		}

		kalMemZero(&rNanSetDiscBcn,
			   sizeof(struct _NAN_CMD_EVENT_SET_DISC_BCN_T));
		if (prNanDiscBcnTrans->beacon_interval != 0) {
			DBGLOG(NAN, INFO, "[FastDisc] Periodic based\n");

			rNanSetDiscBcn.ucDiscBcnType = ENUM_DISC_BCN_PERIOD;
			rNanSetDiscBcn.ucDiscBcnPeriod =
					prNanDiscBcnTrans->beacon_interval;
		} else if (prAvail->num_maps_ids != 0) {
			DBGLOG(NAN, INFO, "[FastDisc] Slot based\n");

			rNanSetDiscBcn.ucDiscBcnType = ENUM_DISC_BCN_SLOT;
			rNanSetDiscBcn.ucDiscBcnPeriod = 0;

			for (i = 0; i < NAN_TIMELINE_MGMT_SIZE; i++) {
				prParams = &prAvail->slots[i];

				if (i >= prAvail->num_maps_ids) {
					DBGLOG(NAN, WARN,
						"Skip Tid, %d, NumMap, %d\n",
						i, prAvail->num_maps_ids);
					rNanSetDiscBcn.rDiscBcnTimeline[i]
						.ucMapId = NAN_INVALID_MAP_ID;
					break;
				}

				rNanSetDiscBcn.rDiscBcnTimeline[i].ucMapId =
							prParams->map_id;
				/* convert to au4AvailMap */
				u2TimeBitmapControl =
				    ((prParams->time_bitmap.bitDuration
				     << NAN_TIME_BITMAP_CTRL_DURATION_OFFSET)
				     & NAN_TIME_BITMAP_CTRL_DURATION) |
				    ((prParams->time_bitmap.period
				     << NAN_TIME_BITMAP_CTRL_PERIOD_OFFSET)
				     & NAN_TIME_BITMAP_CTRL_PERIOD) |
				    ((prParams->time_bitmap.offset
				     << NAN_TIME_BITMAP_CTRL_STARTOFFSET_OFFSET)
				     & NAN_TIME_BITMAP_CTRL_STARTOFFSET);

				kalMemZero(au4AvailMap, sizeof(au4AvailMap));
				nanParserInterpretTimeBitmapField(
				       prAdapter, u2TimeBitmapControl,
				       prParams->time_bitmap.time_bitmap_length,
				       prParams->time_bitmap.time_bitmap,
				       au4AvailMap);

				kalMemCopy(rNanSetDiscBcn.rDiscBcnTimeline[i]
						.au4AvailMap,
						au4AvailMap,
						sizeof(au4AvailMap));
			}
		}
		rStatus = nanDevSetDiscBcn(prAdapter, &rNanSetDiscBcn);

		if (rStatus != NAN_STATUS_SUCCESS) {
			DBGLOG(NAN, ERROR,
				"[FastDisc] Set Disc Bcn Period Error !!\n");
			return -EFAULT;
		}

		return 0;
	}
	case NAN_MSG_ID_UPDATE_DFSP_CONFIG:
		{
			struct _NanDfspConfig *prNanDfspCfg = NULL;

			prNanDfspCfg = (struct _NanDfspConfig *)data;
			nanUpdateDfspConfig(prAdapter,
				(struct _NAN_CMD_DFSP_CONFIG *)prNanDfspCfg);
			return 0;
		}
	case NAN_MSG_ID_UPDATE_CUSTOM_ATTRIBUTE:
	{
		struct NanCustomAttribute *prNanCustomAttr = NULL;

		prNanCustomAttr = (struct NanCustomAttribute *)data;

		prAdapter->rNanCustomAttr.length =
			prNanCustomAttr->length;
#ifdef NAN_TODO /* T.B.D Unify NAN-Display */
		kalMemCpyS(prAdapter->rNanCustomAttr.data,
			sizeof(prAdapter->rNanCustomAttr.data),
			prNanCustomAttr->data,
			prNanCustomAttr->length);
#endif

		/* Update custom vendor specific attribute to FW */
		nanDiscSetCustomAttribute(prAdapter, prNanCustomAttr);

		return 0;
	}
	case NAN_MSG_ID_PRIV_CMD:
	{
		struct NanDrvPrivCmd *prNanPrivCmd = NULL;
		struct NanDrvPrivCmdWork *prWork = NULL;

		prNanPrivCmd = (struct NanDrvPrivCmd *)data;

		prWork = kalMemAlloc(sizeof(struct NanDrvPrivCmdWork),
				PHY_MEM_TYPE);
		if (!prWork) {
			DBGLOG(REQ, ERROR, "Cannot allocate prWork\n");
			return -ENOMEM;
		}

		kalMemZero(prWork, sizeof(struct NanDrvPrivCmdWork));
		kalMemCopy(prWork->cmd,
			prNanPrivCmd->cmd,
			NAN_PRIV_CMD_MAX_SIZE - 1);
		prWork->prGlueInfo = prGlueInfo;

		INIT_WORK(&prWork->work, kalNanPrivWork);

		if (!prGlueInfo->prNANPrivCmdWorkQueue) {
			DBGLOG(REQ, ERROR, "prNANPrivCmdWorkQueue is NULL\n");
			kalMemFree(prWork, PHY_MEM_TYPE,
				sizeof(struct NanDrvPrivCmdWork));
			return -EFAULT;
		}

		queue_work(prGlueInfo->prNANPrivCmdWorkQueue, &prWork->work);

		return 0;
	}

	default:
		return -EOPNOTSUPP;
	}

	return ret;
}

/* Indication part */
int
mtk_cfg80211_vendor_event_nan_event_indication(struct ADAPTER *prAdapter,
					       uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NanEventIndMsg *prNanEventInd;
	struct NAN_DE_EVENT *prDeEvt;
	uint16_t u2EventType;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;

	prDeEvt = (struct NAN_DE_EVENT *) pcuEvtBuf;

	if (prDeEvt == NULL) {
		DBGLOG(NAN, ERROR, "pcuEvtBuf is null\n");
		return -EFAULT;
	}

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	/*Final length includes all TLVs*/
	message_len = sizeof(struct _NanMsgHeader) +
		SIZEOF_TLV_HDR + MAC_ADDR_LEN;

	prNanEventInd = kalMemAlloc(message_len, VIR_MEM_TYPE);
	if (!prNanEventInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	prNanEventInd->fwHeader.msgVersion = 1;
	prNanEventInd->fwHeader.msgId = NAN_MSG_ID_DE_EVENT_IND;
	prNanEventInd->fwHeader.msgLen = message_len;
	prNanEventInd->fwHeader.handle = 0;
	prNanEventInd->fwHeader.transactionId = 0;

	tlvs = prNanEventInd->ptlv;


	if (prDeEvt->ucEventType != NAN_EVENT_ID_DISC_MAC_ADDR) {
		DBGLOG(NAN, DEBUG, "ClusterId=%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->ucClusterId[0], prDeEvt->ucClusterId[1],
		       prDeEvt->ucClusterId[2], prDeEvt->ucClusterId[3],
		       prDeEvt->ucClusterId[4], prDeEvt->ucClusterId[5]);
		/* NAN_CHK_PNT log message */
		if (prDeEvt->ucEventType == NAN_EVENT_ID_STARTED_CLUSTER) {
			nanLogClusterMac(prDeEvt->ucOwnNmi);
			nanLogClusterId(prDeEvt->ucClusterId);
		} else if (prDeEvt->ucEventType ==
			   NAN_EVENT_ID_JOINED_CLUSTER)
			nanLogJoinCluster(prDeEvt->ucClusterId);
		DBGLOG(NAN, DEBUG,
		       "AnchorMasterRank=%02x%02x%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->aucAnchorMasterRank[0],
		       prDeEvt->aucAnchorMasterRank[1],
		       prDeEvt->aucAnchorMasterRank[2],
		       prDeEvt->aucAnchorMasterRank[3],
		       prDeEvt->aucAnchorMasterRank[4],
		       prDeEvt->aucAnchorMasterRank[5],
		       prDeEvt->aucAnchorMasterRank[6],
		       prDeEvt->aucAnchorMasterRank[7]);
		DBGLOG(NAN, DEBUG, "MyNMI=%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->ucOwnNmi[0], prDeEvt->ucOwnNmi[1],
		       prDeEvt->ucOwnNmi[2], prDeEvt->ucOwnNmi[3],
		       prDeEvt->ucOwnNmi[4], prDeEvt->ucOwnNmi[5]);
		DBGLOG(NAN, DEBUG, "MasterNMI=%02x%02x%02x%02x%02x%02x\n",
		       prDeEvt->ucMasterNmi[0], prDeEvt->ucMasterNmi[1],
		       prDeEvt->ucMasterNmi[2], prDeEvt->ucMasterNmi[3],
		       prDeEvt->ucMasterNmi[4], prDeEvt->ucMasterNmi[5]);
	}

	if (prDeEvt->ucEventType == NAN_EVENT_ID_DISC_MAC_ADDR)
		u2EventType = NAN_TLV_TYPE_EVENT_SELF_STATION_MAC_ADDRESS;
	else if (prDeEvt->ucEventType == NAN_EVENT_ID_STARTED_CLUSTER)
		u2EventType = NAN_TLV_TYPE_EVENT_STARTED_CLUSTER;
	else if (prDeEvt->ucEventType == NAN_EVENT_ID_JOINED_CLUSTER)
		u2EventType = NAN_TLV_TYPE_EVENT_JOINED_CLUSTER;
	else {
		kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);
		return WLAN_STATUS_SUCCESS;
	}

	nanExtComposeClusterEvent(prAdapter, prDeEvt);

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	if (nanSecIsDevSupportGroupSecurity(prAdapter) &&
		(prDeEvt->ucEventType == NAN_EVENT_ID_JOINED_CLUSTER)) {
		struct WIFI_VAR *prWifiVar = NULL;
		struct BSS_INFO *prnanBssInfo = NULL;
		struct _NAN_SPECIFIC_BSS_INFO_T *prNanSpecInfo =
			(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
		uint8_t aucBmcMacAddr[] = BC_MAC_ADDR;
		uint8_t aucIgtk[32] = {0};
		size_t szIgtkLen = 0;
		uint32_t u4IgtkCipher = 0;
		uint8_t ucIgtkAlgo = 0;
		uint8_t aucBigtk[32] = {0};
		size_t szBigtkLen = 0;
		uint32_t u4BigtkCipher = 0;
		uint8_t ucBigtkAlgo = 0;
		uint8_t i = 0;

		prWifiVar = &prAdapter->rWifiVar;

		nanSecGetTxIgtkBigtk(
			&u4IgtkCipher,
			aucIgtk,
			&szIgtkLen,
			&u4BigtkCipher,
			aucBigtk,
			&szBigtkLen);

		ucIgtkAlgo = (uint8_t) wpa_cipher_to_alg((int)u4IgtkCipher);
		ucBigtkAlgo = (uint8_t) wpa_cipher_to_alg((int)u4BigtkCipher);

		for (i = 0; i < NAN_BSS_INDEX_NUM; i++) {
			prNanSpecInfo = nanGetSpecificBssInfo(prAdapter, i);
			if (prNanSpecInfo == NULL) {
				DBGLOG(NAN, ERROR, "prNanSpecInfo is NULL\n");
				continue;
			}
			prnanBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
				prNanSpecInfo->ucBssIndex);

			DBGLOG(NAN, INFO,
				"Install TX IGTK, Wtbl:%d\n",
				prnanBssInfo->ucBMCWlanIndex);
			nan_sec_wpas_setkey_glue(FALSE,
				prnanBssInfo->ucBssIndex,
				ucIgtkAlgo,
				aucBmcMacAddr,
				4,
				aucIgtk,
				szIgtkLen);

			if (prWifiVar->ucNanGroupSecCap ==
			    CSIA_CAP_GTKSA_IGTKSA_SUP_BIGTKSA_UNSUP)
				continue;

			/**
			 * prWifiVar->ucNanGroupSecCap !=
			 * CSIA_CAP_GTKSA_IGTKSA_SUP_BIGTKSA_UNSUP
			 */
			DBGLOG(NAN, INFO, "Install TX BIGTK, Wtbl:%d\n",
			       prnanBssInfo->ucBMCWlanIndex);
			nan_sec_wpas_setkey_glue(FALSE,
				prnanBssInfo->ucBssIndex,
				ucBigtkAlgo,
				aucBmcMacAddr,
				6,
				aucBigtk,
				szBigtkLen);
		}
	}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	/* Add TLV datas */
	tlvs = nanAddTlv(u2EventType, MAC_ADDR_LEN, prDeEvt->ucClusterId, tlvs);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanEventInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kalMemFree(prNanEventInd, VIR_MEM_TYPE, message_len);

	return WLAN_STATUS_SUCCESS;
}

int mtk_cfg80211_vendor_event_nan_disable_indication(
		struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NanDisableIndMsg *prNanDisableInd;
	struct NAN_DISABLE_EVENT *prDisableEvt;
	size_t message_len = 0;

	prDisableEvt = (struct NAN_DISABLE_EVENT *) pcuEvtBuf;

	if (prDisableEvt == NULL) {
		DBGLOG(NAN, ERROR, "pcuEvtBuf is null\n");
		return -EFAULT;
	}

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	/*Final length includes all TLVs*/
	message_len = sizeof(struct _NanMsgHeader) +
			sizeof(u16) +
			sizeof(u16);

	prNanDisableInd = kalMemAlloc(message_len, VIR_MEM_TYPE);
	if (!prNanDisableInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	prNanDisableInd->fwHeader.msgVersion = 1;
	prNanDisableInd->fwHeader.msgId = NAN_MSG_ID_DISABLE_IND;
	prNanDisableInd->fwHeader.msgLen = message_len;
	prNanDisableInd->fwHeader.handle = 0;
	prNanDisableInd->fwHeader.transactionId = 0;

	prNanDisableInd->reason = 0;

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kalMemFree(prNanDisableInd, VIR_MEM_TYPE, message_len);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
		message_len, prNanDisableInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kalMemFree(prNanDisableInd, VIR_MEM_TYPE, message_len);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kalMemFree(prNanDisableInd, VIR_MEM_TYPE, message_len);

	g_enableNAN = TRUE;

	return WLAN_STATUS_SUCCESS;
}

/* Indication part */
int
mtk_cfg80211_vendor_event_nan_replied_indication(struct ADAPTER *prAdapter,
						 uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NAN_REPLIED_EVENT *prRepliedEvt = NULL;
	struct NanPublishRepliedIndMsg *prNanPubRepliedInd;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	prRepliedEvt = (struct NAN_REPLIED_EVENT *)pcuEvtBuf;

	/* Final length includes all TLVs */
	message_len = sizeof(struct _NanMsgHeader) +
		      sizeof(struct _NanPublishRepliedIndParams) +
		      ((SIZEOF_TLV_HDR) + MAC_ADDR_LEN) +
		      ((SIZEOF_TLV_HDR) + sizeof(prRepliedEvt->ucRssi_value));

	prNanPubRepliedInd = kmalloc(message_len, GFP_KERNEL);
	if (prNanPubRepliedInd == NULL)
		return -ENOMEM;

	kalMemZero(prNanPubRepliedInd, message_len);

	DBGLOG(NAN, DEBUG, "[%s] message_len : %lu\n", __func__, message_len);
	prNanPubRepliedInd->fwHeader.msgVersion = 1;
	prNanPubRepliedInd->fwHeader.msgId = NAN_MSG_ID_PUBLISH_REPLIED_IND;
	prNanPubRepliedInd->fwHeader.msgLen = message_len;
	prNanPubRepliedInd->fwHeader.handle = prRepliedEvt->u2Pubid;
	prNanPubRepliedInd->fwHeader.transactionId = 0;

	prNanPubRepliedInd->publishRepliedIndParams.matchHandle =
		prRepliedEvt->u2Subid;

	tlvs = prNanPubRepliedInd->ptlv;
	/* Add TLV datas */
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 &prRepliedEvt->auAddr[0], tlvs);

	tlvs = nanAddTlv(NAN_TLV_TYPE_RECEIVED_RSSI_VALUE,
			 sizeof(prRepliedEvt->ucRssi_value),
			 &prRepliedEvt->ucRssi_value, tlvs);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPubRepliedInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPubRepliedInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree(prNanPubRepliedInd);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);

	kfree(prNanPubRepliedInd);
	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_match_indication(struct ADAPTER *prAdapter,
					       uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NAN_DISCOVERY_EVENT *prDiscEvt;
	struct NanMatchIndMsg *prNanMatchInd;
	struct NanSdeaCtrlParams peer_sdea_params;
	struct NanFWSdeaCtrlParams nanPeerSdeaCtrlarms;
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct NanPairingCapabilityMsg PairingCapMsg;
	struct NanPairingNiraMsg  PairingNiraMsg;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	struct _NAN_INSTANCE_T *prInstance = NULL;
	uint8_t aucCipherSuiteList[4];
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	size_t message_len = 0;
	uint8_t *tlvs = NULL;
#if CFG_SUPPORT_RTT
	uint16_t u2RangingId = 0;
	struct _NAN_RANGING_INSTANCE_T *prRanging = NULL;
	struct NanRangeRequest *prRangingReq = NULL;
	struct NanRangeInfo rNanRangeInfo;
	struct RTT_RESULT *prRttResult = NULL;
#endif
	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	kalMemZero(&nanPeerSdeaCtrlarms, sizeof(struct NanFWSdeaCtrlParams));
	kalMemZero(&peer_sdea_params, sizeof(struct NanSdeaCtrlParams));

	prDiscEvt = (struct NAN_DISCOVERY_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct _NanMsgHeader) +
		      sizeof(struct _NanMatchIndParams) +
		      (SIZEOF_TLV_HDR + MAC_ADDR_LEN) +
		      (SIZEOF_TLV_HDR + prDiscEvt->u2Service_info_len) +
		      (SIZEOF_TLV_HDR + prDiscEvt->ucSdf_match_filter_len) +
		      (SIZEOF_TLV_HDR + sizeof(struct NanFWSdeaCtrlParams));
#if CFG_SUPPORT_RTT
	prRanging = nanRangingInstanceSearchByMac(prAdapter,
						prDiscEvt->aucNanAddress);
	if (prRanging) {
		DBGLOG(NAN, INFO, "Ranging of DiscSubId=%u, Addr="MACSTR"\n",
			prDiscEvt->u2SubscribeID,
			MAC2STR(prDiscEvt->aucNanAddress));
		message_len += (SIZEOF_TLV_HDR + sizeof(struct NanRangeInfo));
	}
#endif

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingCapabilityMsg));
		message_len +=
		(SIZEOF_TLV_HDR + sizeof(struct NanPairingNiraMsg));
		memset(&PairingCapMsg, 0,
			sizeof(struct NanPairingCapabilityMsg));
		memset(&PairingNiraMsg, 0,
			sizeof(struct NanPairingNiraMsg));
		g_last_matched_report_npba_dialogTok =
			prDiscEvt->ucPairingNPBA_DialogToken;
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	prNanMatchInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanMatchInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	kalMemZero(prNanMatchInd, message_len);

	prNanMatchInd->fwHeader.msgVersion = 1;
	prNanMatchInd->fwHeader.msgId = NAN_MSG_ID_MATCH_IND;
	prNanMatchInd->fwHeader.msgLen = message_len;
	prNanMatchInd->fwHeader.handle = prDiscEvt->u2SubscribeID;
	prNanMatchInd->fwHeader.transactionId = 0;

	prNanMatchInd->matchIndParams.matchHandle = prDiscEvt->u2PublishID;
	prNanMatchInd->matchIndParams.matchOccuredFlag =
		0; /* means match in SDF */
	prNanMatchInd->matchIndParams.outOfResourceFlag =
		0; /* doesn't outof resource. */
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		PairingCapMsg.enable_pairing_setup
			= prDiscEvt->ucPairingEnable;
		PairingCapMsg.enable_pairing_cache
			= prDiscEvt->ucPairingCacheEnabled;
		PairingCapMsg.enable_pairing_verification
			= prDiscEvt->ucPairingVerificationEnabled;
		PairingCapMsg.supported_bootstrapping_methods
			= prDiscEvt->u2PairingBootstrapMethod;

		kalMemCpyS(&PairingNiraMsg.NiraTag,
		NAN_PAIRING_NIRA_CIPHERVERSION1_TAG_SIZE,
		&prDiscEvt->u8NiraTag, sizeof(uint8_t)*8);
		kalMemCpyS(&PairingNiraMsg.NiraNonce,
		NAN_PAIRING_NIRA_CIPHERVERSION1_NONCE_SIZE,
		&prDiscEvt->u8NiraNonce, sizeof(uint8_t)*8);
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	tlvs = prNanMatchInd->ptlv;
	/* Add TLV datas */
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 &prDiscEvt->aucNanAddress[0], tlvs);
	DBGLOG(NAN, DEBUG, "[%s] :NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO %u\n",
	       __func__, NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO);

	tlvs = nanAddTlv(NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
			 prDiscEvt->u2Service_info_len,
			 &prDiscEvt->aucSerive_specificy_info[0], tlvs);

	tlvs = nanAddTlv(NAN_TLV_TYPE_SDF_MATCH_FILTER,
			 prDiscEvt->ucSdf_match_filter_len,
			 prDiscEvt->aucSdf_match_filter,
			 tlvs);
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prAdapter->rWifiVar.ucNanEnablePairing == 1) {
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_CAPABILITY,
			sizeof(struct NanPairingCapabilityMsg),
			(u8 *)&PairingCapMsg, tlvs);

		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_NIRA,
			sizeof(struct NanPairingNiraMsg),
			(u8 *)&PairingNiraMsg, tlvs);
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	nanPeerSdeaCtrlarms.data_path_required =
		(prDiscEvt->ucDataPathParm != 0) ? 1 : 0;
	nanPeerSdeaCtrlarms.security_required =
		(prDiscEvt->aucSecurityInfo[0] != 0) ? 1 : 0;
	nanPeerSdeaCtrlarms.ranging_required =
		(prDiscEvt->ucRange_measurement != 0) ? 1 : 0;
#ifdef NAN_TODO /* (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1) */
	nanPeerSdeaCtrlarms.gtk_required =
		(u2SdeaServiceControl & NAN_SDEA_CTRL_GTK_REQUIRED) ? 1 : 0;
#endif

	DBGLOG(NAN, LOUD,
	       "data_path_required : %d, security_required:%d, ranging_required:%d\n",
	       nanPeerSdeaCtrlarms.data_path_required,
	       nanPeerSdeaCtrlarms.security_required,
	       nanPeerSdeaCtrlarms.ranging_required);

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	DBGLOG(NAN, INFO, "gtk_required:%d", nanPeerSdeaCtrlarms.gtk_required);
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */

	/* NAN_CHK_PNT log message */
	nanLogMatch(prDiscEvt->aucNanAddress);

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (prDiscEvt->ucPairingEnable)
		pairingFsmPairedOrNot(prAdapter, prDiscEvt);

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	prInstance = nanDiscGetInstanceByServId(prDiscEvt->u2SubscribeID);
	if (prInstance) {
		kalMemCopy(aucCipherSuiteList,
			&prDiscEvt->u4CipherType,
			sizeof(prDiscEvt->u4CipherType));
		if (aucCipherSuiteList[0] ==
		    NAN_CIPHER_SUITE_ID_NCS_GTK_CCM_128 ||
		    aucCipherSuiteList[0] ==
		    NAN_CIPHER_SUITE_ID_NCS_GTK_GCM_256)
			prInstance->ucGtkCipher = aucCipherSuiteList[0];
		else
			prInstance->ucGtkCipher = aucCipherSuiteList[2];
		prInstance->u2PubId = prDiscEvt->u2PublishID;
		DBGLOG(NAN, INFO,
			"Set GTK cipher %u and PubId %u, cipher_type 0x%x\n",
			prInstance->ucGtkCipher,
			prInstance->u2PubId,
			prDiscEvt->u4CipherType);
	}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	tlvs = nanAddTlv(NAN_TLV_TYPE_SDEA_CTRL_PARAMS,
			 sizeof(struct NanFWSdeaCtrlParams),
			 (u8 *)&nanPeerSdeaCtrlarms, tlvs);
#if CFG_SUPPORT_RTT
	prRangingReq = nanGetRangingReq(prAdapter, prDiscEvt->u2SubscribeID);
	if (!prRangingReq) {
		DBGLOG(NAN, ERROR,
			"RangingReq Null, id: %d\n",
			prDiscEvt->u2SubscribeID);
		goto SKIP_RTT;
	}

	if (prRanging) {
		DBGLOG(NAN, INFO, "write range_info to tlv\n");
		prRttResult = &prRanging->ranging_ctrl.rRangingRttResult;
		rNanRangeInfo.range_measurement_cm =
			prRttResult->i4DistanceMM / 1000;
		rNanRangeInfo.ranging_event_type =
			prRangingReq->ranging_cfg.config_ranging_indications;
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN20_RANGING_RESULT,
				sizeof(struct NanRangeInfo),
				(u8 *)&rNanRangeInfo, tlvs);
	} else if (nanIsSubEnableRanging(prAdapter, prDiscEvt->u2SubscribeID)) {
		DBGLOG(NAN, INFO, "store DiscEvt and trigger ranging\n");
		nanSubStoreDiscEvtForRanging(prAdapter, prDiscEvt);
		COPY_MAC_ADDR(prRangingReq->peer_addr,
			      prDiscEvt->aucNanAddress);
		nanRangingRequest(prAdapter, &u2RangingId, prRangingReq);
		return	WLAN_STATUS_PENDING;
	}

SKIP_RTT:

#endif /* CFG_SUPPORT_RTT */

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanMatchInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanMatchInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanMatchInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanMatchInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_publish_terminate(struct ADAPTER *prAdapter,
						uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NAN_PUBLISH_TERMINATE_EVENT *prPubTerEvt;
	struct NanPublishTerminatedIndMsg nanPubTerInd;
	struct _NAN_PUBLISH_SPECIFIC_INFO_T *prPubSpecificInfo = NULL;
	size_t message_len = 0;
	uint8_t i;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	kalMemZero(&nanPubTerInd, sizeof(struct NanPublishTerminatedIndMsg));
	prPubTerEvt = (struct NAN_PUBLISH_TERMINATE_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct NanPublishTerminatedIndMsg);

	nanPubTerInd.fwHeader.msgVersion = 1;
	nanPubTerInd.fwHeader.msgId = NAN_MSG_ID_PUBLISH_TERMINATED_IND;
	nanPubTerInd.fwHeader.msgLen = message_len;
	nanPubTerInd.fwHeader.handle = prPubTerEvt->u2Pubid;
	/* Indication doesn't have transactionId, don't care */
	nanPubTerInd.fwHeader.transactionId = 0;
	/* For all user should be success. */
	nanPubTerInd.reason = prPubTerEvt->ucReasonCode;
	prAdapter->rPublishInfo.ucNanPubNum--;

	DBGLOG(NAN, DEBUG, "Cancel Pub ID = %d, PubNum = %d\n",
	       nanPubTerInd.fwHeader.handle,
	       prAdapter->rPublishInfo.ucNanPubNum);

	for (i = 0; i < NAN_MAX_PUBLISH_NUM; i++) {
		prPubSpecificInfo =
			&prAdapter->rPublishInfo.rPubSpecificInfo[i];
		if (prPubSpecificInfo->ucPublishId == prPubTerEvt->u2Pubid) {
			prPubSpecificInfo->ucUsed = FALSE;
			if (prPubSpecificInfo->ucReportTerminate) {
				/* Fill skb and send to kernel by nl80211 */
				skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN,
					GFP_KERNEL);
				if (!skb) {
					DBGLOG(NAN, ERROR,
						"Allocate skb failed\n");
					return -ENOMEM;
				}
				if (unlikely(nla_put(skb,
					MTK_WLAN_VENDOR_ATTR_NAN,
					message_len,
					&nanPubTerInd) < 0)) {
					DBGLOG(NAN, ERROR,
						"nla_put_nohdr failed\n");
					kfree_skb(skb);
					return -EFAULT;
				}
				cfg80211_vendor_event(skb, GFP_KERNEL);
			}
		}
	}
	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_subscribe_terminate(struct ADAPTER *prAdapter,
						  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NAN_SUBSCRIBE_TERMINATE_EVENT *prSubTerEvt;
	struct NanSubscribeTerminatedIndMsg nanSubTerInd;
	struct _NAN_SUBSCRIBE_SPECIFIC_INFO_T *prSubSpecificInfo = NULL;
	size_t message_len = 0;
	uint8_t i;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	kalMemZero(&nanSubTerInd, sizeof(struct NanSubscribeTerminatedIndMsg));
	prSubTerEvt = (struct NAN_SUBSCRIBE_TERMINATE_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct NanSubscribeTerminatedIndMsg);

	nanSubTerInd.fwHeader.msgVersion = 1;
	nanSubTerInd.fwHeader.msgId = NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND;
	nanSubTerInd.fwHeader.msgLen = message_len;
	nanSubTerInd.fwHeader.handle = prSubTerEvt->u2Subid;
	/* Indication doesn't have transactionId, don't care */
	nanSubTerInd.fwHeader.transactionId = 0;
	/* For all user should be success. */
	nanSubTerInd.reason = prSubTerEvt->ucReasonCode;
	prAdapter->rSubscribeInfo.ucNanSubNum--;

	DBGLOG(NAN, DEBUG, "Cancel Sub ID = %d, SubNum = %d\n",
		nanSubTerInd.fwHeader.handle,
		prAdapter->rSubscribeInfo.ucNanSubNum);

	for (i = 0; i < NAN_MAX_SUBSCRIBE_NUM; i++) {
		prSubSpecificInfo =
			&prAdapter->rSubscribeInfo.rSubSpecificInfo[i];
		if (prSubSpecificInfo->ucSubscribeId == prSubTerEvt->u2Subid) {
			prSubSpecificInfo->ucUsed = FALSE;
			if (prSubSpecificInfo->ucReportTerminate) {
				/*  Fill skb and send to kernel by nl80211*/
				skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN,
					GFP_KERNEL);
				if (!skb) {
					DBGLOG(NAN, ERROR,
						"Allocate skb failed\n");
					return -ENOMEM;
				}
				if (unlikely(nla_put(skb,
					MTK_WLAN_VENDOR_ATTR_NAN,
					message_len,
					&nanSubTerInd) < 0)) {
					DBGLOG(NAN, ERROR,
						"nla_put_nohdr failed\n");
					kfree_skb(skb);
					return -EFAULT;
				}
				cfg80211_vendor_event(skb, GFP_KERNEL);
			}
		}
	}
	return WLAN_STATUS_SUCCESS;
}
#if CFG_SUPPORT_NAN_R4_PAIRING
int indicatePairingSuccess_resp(struct ADAPTER *prAdapter,
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt,
	struct PAIRING_FSM_INFO *prPairingFsm)
{
	struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;

	DBGLOG(NAN, INFO, "[pairing-keyEx] enter\n");
	pNanXmitFollowupReq =
		kmalloc(sizeof(struct NanTransmitFollowupRequest), GFP_ATOMIC);
	if (!pNanXmitFollowupReq) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return  -ENOMEM;
	}
	DBGLOG(NAN, INFO, "[pairing-keyEx] update ucSubscribeID(%u)\n",
		prFollowupEvt->requestor_instance_id);

	prPairingFsm->ucSubscribeID = prFollowupEvt->requestor_instance_id;
	/*indicate Pairing success to WifiHal for responder*/
	nanPairingPeerNikReceived(prAdapter, (u8 *)prFollowupEvt, prPairingFsm);
	kalMemZero(pNanXmitFollowupReq,
		sizeof(struct NanTransmitFollowupRequest));
	pNanXmitFollowupReq->publish_subscribe_id = prPairingFsm->ucPublishID;
	pNanXmitFollowupReq->requestor_instance_id =
		prPairingFsm->ucSubscribeID;
	memcpy(pNanXmitFollowupReq->addr,
		prPairingFsm->prStaRec->aucMacAddr, NAN_MAC_ADDR_LEN);
	nanTransmitRequest_host(prAdapter, pNanXmitFollowupReq);
	kfree(pNanXmitFollowupReq);

	DBGLOG(NAN, INFO, "[pairing-keyEx] leave\n");
	return WLAN_STATUS_SUCCESS;
}
void indicatePairingSuccess_init(struct ADAPTER *prAdapter,
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt,
	struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "[pairing-keyEx] enter\n");
	/* indicate Pairing success to WifiHal for initiator */
	nanPairingPeerNikReceived(prAdapter, (u8 *)prFollowupEvt, prPairingFsm);
	DBGLOG(NAN, INFO, "[pairing-keyEx] leave\n");
}

static uint32_t
sendBootstrappingResponseWithComeback(struct ADAPTER *prAdapter,
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt, uint16_t comeback) {

	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;

	DBGLOG(NAN, INFO, "Enter\n");

	if (prFollowupEvt == NULL)
		return WLAN_STATUS_INVALID_DATA;

	pNanXmitFollowupReq = kmalloc(sizeof(struct NanTransmitFollowupRequest),
				GFP_ATOMIC);
	if (!pNanXmitFollowupReq) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return WLAN_STATUS_RESOURCES;
	}
	kalMemZero(pNanXmitFollowupReq,
		sizeof(struct NanTransmitFollowupRequest));

	/* FW Follow up instance ID */
	pNanXmitFollowupReq->publish_subscribe_id =
		prFollowupEvt->publish_subscribe_id;
	pNanXmitFollowupReq->requestor_instance_id =
		prFollowupEvt->requestor_instance_id;

	/* PEER MAC */
	memcpy(pNanXmitFollowupReq->addr, prFollowupEvt->addr,
		NAN_MAC_ADDR_LEN);

	/* SDA */
	pNanXmitFollowupReq->service_specific_info_len =
		prFollowupEvt->service_specific_info_len;
	memcpy(pNanXmitFollowupReq->service_specific_info,
		prFollowupEvt->service_specific_info,
		prFollowupEvt->service_specific_info_len);

	/* SDEA */
	pNanXmitFollowupReq->sdea_service_specific_info_len =
		prFollowupEvt->sdea_service_specific_info_len;
	memcpy(pNanXmitFollowupReq->sdea_service_specific_info,
		prFollowupEvt->sdea_service_specific_info,
		prFollowupEvt->sdea_service_specific_info_len);

	/* NPBA */
	pNanXmitFollowupReq->bootstrap_type =
		NAN_BOOTSTRAPPING_TYPE_RESPONSE;
	pNanXmitFollowupReq->bootstrap_method =
		0x00;
	pNanXmitFollowupReq->bootstrap_status =
		NAN_BOOTSTRAPPING_STATUS_COMEBACK;
	pNanXmitFollowupReq->comeback_enable = TRUE;
	pNanXmitFollowupReq->comeback_after = comeback;


	rStatus = nanTransmitRequest(prAdapter,	pNanXmitFollowupReq);
	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(NAN, ERROR,
			"Failed (rStatus=%u)\n", rStatus);
	else
		DBGLOG(NAN, INFO, "Leave\n");

	kfree(pNanXmitFollowupReq);
	return rStatus;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
int
mtk_cfg80211_vendor_event_nan_followup_indication(struct ADAPTER *prAdapter,
						  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NanFollowupIndMsg *prNanFollowupInd = NULL;
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;
#if CFG_SUPPORT_NAN_R4_PAIRING
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct NanPairingBootStrapMsg bootstrap_msg = {0};
	u_int8_t fgIsNanPairingEn = prAdapter->rWifiVar.ucNanEnablePairing;
	u_int16_t comeback_time = prAdapter->rWifiVar.u2NanPairingComeback;
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	prFollowupEvt = (struct NAN_FOLLOW_UP_EVENT *)pcuEvtBuf;

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (fgIsNanPairingEn &&
	prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST) {
		g_last_bootstrapReq_npba_dialogTok =
			prFollowupEvt->bootstrap_dialog;
	}

	if (fgIsNanPairingEn &&
	prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST
	&& (prFollowupEvt->bootstrap_status
	!= NAN_BOOTSTRAPPING_STATUS_COMEBACK)) {
		if (comeback_time > 0) {
			sendBootstrappingResponseWithComeback(prAdapter,
				prFollowupEvt, comeback_time);
		}
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	if (prFollowupEvt->service_specific_info_len <
		NAN_FW_MAX_SERVICE_SPECIFIC_INFO_LEN &&
		prFollowupEvt->sdea_service_specific_info_len <
		NAN_FW_MAX_FOLLOW_UP_SDEA_LEN) {

		message_len =
			sizeof(struct _NanMsgHeader) +
			sizeof(struct _NanFollowupIndParams) +
			(SIZEOF_TLV_HDR + MAC_ADDR_LEN);

		if (prFollowupEvt->service_specific_info_len > 0)
			message_len += (SIZEOF_TLV_HDR +
			prFollowupEvt->service_specific_info_len);

		if (prFollowupEvt->sdea_service_specific_info_len > 0)
			message_len += (SIZEOF_TLV_HDR +
			prFollowupEvt->sdea_service_specific_info_len);

#if CFG_SUPPORT_NAN_R4_PAIRING
		if (fgIsNanPairingEn &&
			(prFollowupEvt->bootstrap_type ==
			NAN_BOOTSTRAPPING_TYPE_REQUEST
			|| prFollowupEvt->bootstrap_type ==
			NAN_BOOTSTRAPPING_TYPE_RESPONSE)) {
			message_len += (SIZEOF_TLV_HDR +
				sizeof(struct NanPairingBootStrapMsg));
		}
#endif
	}

	if (message_len > 0)
		prNanFollowupInd = kmalloc(message_len, GFP_KERNEL);

	if (!prNanFollowupInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanFollowupInd, message_len);
	if (!prNanFollowupInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	prNanFollowupInd->fwHeader.msgVersion = 1;
#if CFG_SUPPORT_NAN_R4_PAIRING
	if (fgIsNanPairingEn && prFollowupEvt->bootstrap_type ==
		NAN_BOOTSTRAPPING_TYPE_REQUEST)
		prNanFollowupInd->fwHeader.msgId =
			NAN_MSG_ID_BOOTSTRAPPING_REQ_IND;
	else if (fgIsNanPairingEn && prFollowupEvt->bootstrap_type ==
		NAN_BOOTSTRAPPING_TYPE_RESPONSE)
		prNanFollowupInd->fwHeader.msgId =
			NAN_MSG_ID_BOOTSTRAPPING_RSP_IND;
	else
		prNanFollowupInd->fwHeader.msgId = NAN_MSG_ID_FOLLOWUP_IND;

	if (prNanFollowupInd->fwHeader.msgId ==
	    NAN_MSG_ID_BOOTSTRAPPING_REQ_IND)
		DBGLOG(NAN, INFO, "send bootstrap indication to WifiHAL\n");
#else
	prNanFollowupInd->fwHeader.msgId = NAN_MSG_ID_FOLLOWUP_IND;
#endif
	prNanFollowupInd->fwHeader.msgLen = message_len;
	prNanFollowupInd->fwHeader.handle = prFollowupEvt->publish_subscribe_id;

	/* Indication doesn't have transition ID */
	prNanFollowupInd->fwHeader.transactionId = 0;

	/* Mapping datas */
	prNanFollowupInd->followupIndParams.matchHandle =
		prFollowupEvt->requestor_instance_id;
	prNanFollowupInd->followupIndParams.window = prFollowupEvt->dw_or_faw;

	DBGLOG(NAN, INFO, "matchHandle: %d, window:%d, ServiceLen(%d,%d)\n",
	       prNanFollowupInd->followupIndParams.matchHandle,
	       prNanFollowupInd->followupIndParams.window,
	       prFollowupEvt->service_specific_info_len,
	       prFollowupEvt->sdea_service_specific_info_len);

#if CFG_SUPPORT_NAN_R4_PAIRING
	DBGLOG(NAN, INFO,
		"bootstrap_type=%u, bootstrap_status=%u, u2ComebackAfter=%u, key_len=%u\n",
		prFollowupEvt->bootstrap_type,
		prFollowupEvt->bootstrap_status,
		prFollowupEvt->u2ComebackAfter,
		prFollowupEvt->key_length);

	if (prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_RESPONSE &&
	    prFollowupEvt->bootstrap_status == NAN_BOOTSTRAPPING_STATUS_ACCEPTED
	    && prFollowupEvt->u2ComebackAfter == 0) {

		/* Rx Bootstrap response and status accepted */
		prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);

		if (prPairingFsm) {
			DBGLOG(NAN, INFO,
				"state=%u, go to BOOTSTRAP_DONE\n",
				prPairingFsm->ePairingState);
		} else {
			DBGLOG(NAN, INFO, "FSM NULL(addr:"MACSTR_A"\n",
				prFollowupEvt->addr[0],
				prFollowupEvt->addr[1],
				prFollowupEvt->addr[2],
				prFollowupEvt->addr[3],
				prFollowupEvt->addr[4],
				prFollowupEvt->addr[5]);
		}
		if (prPairingFsm) {
			if (prPairingFsm->ePairingState ==
				NAN_PAIRING_BOOTSTRAPPING) {
				pairingFsmSteps(prAdapter,
				prPairingFsm, NAN_PAIRING_BOOTSTRAPPING_DONE);
			}
		}
	} else if (prFollowupEvt->key_length > 0) {

		/* Get PairingFSM for SKDA */
		prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);
		if (prPairingFsm) {
			DBGLOG(NAN, INFO,
				"state=%u, go to DECRYPT_SKDA\n",
				prPairingFsm->ePairingState);
		}
		DBGLOG(NAN, INFO,
			"FSM %p, SKDA, addr="MACSTR_A"\n",
			prPairingFsm,
			prFollowupEvt->addr[0],
			prFollowupEvt->addr[1],
			prFollowupEvt->addr[2],
			prFollowupEvt->addr[3],
			prFollowupEvt->addr[4],
			prFollowupEvt->addr[5]);
		/* Print PairingFsm for debug */
		pairingDbgInfo(prAdapter);

		if (prPairingFsm &&
			prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			prFollowupEvt->key_length > 0) {
			/* Copy key data for decryption */
			DBGLOG(NAN, INFO, "[pairing-dbg] go decrypt SKDA\n");
			prPairingFsm->u2Skda2KeyLength =
				prFollowupEvt->key_length;

			pairingDecryptSKDA(prAdapter,
				prPairingFsm, prFollowupEvt->key_data,
				prFollowupEvt->key_length);

			/* NIK sending at responder */
			if (prPairingFsm->ucPairingType ==
			NAN_PAIRING_RESPONDER &&
			prPairingFsm->fgCachingEnable == TRUE &&
			prPairingFsm->fgPeerNIK_received == FALSE) {
				int ret = WLAN_STATUS_SUCCESS;

				ret = indicatePairingSuccess_resp(
					prAdapter, prFollowupEvt,
					prPairingFsm);
				if (ret != WLAN_STATUS_SUCCESS) {
					kfree(prNanFollowupInd);
					return ret;
				}
			} else if (prPairingFsm->ucPairingType ==
			 NAN_PAIRING_REQUESTOR &&
			prPairingFsm->fgCachingEnable == TRUE &&
			prPairingFsm->fgPeerNIK_received == FALSE){
				indicatePairingSuccess_init(
				prAdapter, prFollowupEvt, prPairingFsm);
			}

		}
		DBGDUMP_HEX(NAN, DEBUG, "NAN SKDA",
			prFollowupEvt->key_data, NAN_KDE_ATTR_BUF_SIZE);
	}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
	tlvs = prNanFollowupInd->ptlv;
	/* Add TLV datas */

#if (!defined(CFG_SUPPORT_NAN_R4_PAIRING) || (CFG_SUPPORT_NAN_R4_PAIRING == 0))
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 prFollowupEvt->addr, tlvs);
#endif

	if (prFollowupEvt->service_specific_info_len > 0)
		tlvs = nanAddTlv(NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
			 prFollowupEvt->service_specific_info_len,
			 prFollowupEvt->service_specific_info, tlvs);
	if (prFollowupEvt->sdea_service_specific_info_len > 0)
		tlvs = nanAddTlv(NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO,
			 prFollowupEvt->sdea_service_specific_info_len,
			 prFollowupEvt->sdea_service_specific_info, tlvs);

#if CFG_SUPPORT_NAN_R4_PAIRING
	if (fgIsNanPairingEn &&
	(prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_REQUEST
	|| prFollowupEvt->bootstrap_type == NAN_BOOTSTRAPPING_TYPE_RESPONSE)) {
		bootstrap_msg.bootstrap_type = prFollowupEvt->bootstrap_type;
		bootstrap_msg.bootstrap_status =
			prFollowupEvt->bootstrap_status;
		bootstrap_msg.bootstrap_method =
			prFollowupEvt->bootstrap_method;
		bootstrap_msg.u2ComebackAfter = prFollowupEvt->u2ComebackAfter;
		tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING,
				sizeof(struct NanPairingBootStrapMsg),
				(u8 *)&bootstrap_msg, tlvs);
	}
	/* need to add NAN_TLV_TYPE_MAC_ADDRESS after
	 * NAN_TLV_TYPE_NAN40_PAIRING_BOOTSTRAPPING
	 */
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 prFollowupEvt->addr, tlvs);
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

	DBGLOG(NAN, INFO,
		"pub/subid: %d, addr: "MACSTR_A", specific_info[0]: %02x\n",
		prNanFollowupInd->fwHeader.handle,
		((uint8_t *)prFollowupEvt->addr)[0],
		((uint8_t *)prFollowupEvt->addr)[1],
		((uint8_t *)prFollowupEvt->addr)[2],
		((uint8_t *)prFollowupEvt->addr)[3],
		((uint8_t *)prFollowupEvt->addr)[4],
		((uint8_t *)prFollowupEvt->addr)[5],
		prFollowupEvt->service_specific_info[0]);

	/* NAN_CHK_PNT log message */
		nanLogRx("Follow_Up", prFollowupEvt->addr);

	/* Ranging report
	 * To be implement. NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO
	 */

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanFollowupInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanFollowupInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanFollowupInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanFollowupInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_selfflwup_indication(
	struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NanSelfFollowupIndMsg *prNanSelfFollowupInd;
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt;
	size_t message_len = 0;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	prFollowupEvt = (struct NAN_FOLLOW_UP_EVENT *) pcuEvtBuf;

	message_len = sizeof(*prNanSelfFollowupInd);

	prNanSelfFollowupInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanSelfFollowupInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	kalMemZero(prNanSelfFollowupInd, message_len);

	prNanSelfFollowupInd->fwHeader.msgVersion = 1;
	prNanSelfFollowupInd->fwHeader.msgId =
			NAN_MSG_ID_SELF_TRANSMIT_FOLLOWUP_IND;
	prNanSelfFollowupInd->fwHeader.msgLen = message_len;
	prNanSelfFollowupInd->fwHeader.handle =
		prFollowupEvt->publish_subscribe_id;
	/* Indication doesn't have transition ID */
	prNanSelfFollowupInd->fwHeader.transactionId =
		prFollowupEvt->transaction_id;

	if (prFollowupEvt->tx_status == WLAN_STATUS_SUCCESS)
		prNanSelfFollowupInd->reason = NAN_I_STATUS_SUCCESS;
	else
		prNanSelfFollowupInd->reason = NAN_I_STATUS_DE_FAILURE;

	nanLogTxAndTxDoneFollowup("Follow_Up", prFollowupEvt);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					message_len + NLMSG_HDRLEN,
					WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanSelfFollowupInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
		message_len, prNanSelfFollowupInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanSelfFollowupInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanSelfFollowupInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_match_expire(struct ADAPTER *prAdapter,
					       uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct NAN_MATCH_EXPIRE_EVENT *prMatchExpireEvt;
	struct NanMatchExpiredIndMsg *prNanMatchExpiredInd;
	size_t message_len = 0;

	wiphy = GLUE_GET_WIPHY(prAdapter->prGlueInfo);
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev is NULL\n");
		return -ENODEV;
	}

	prMatchExpireEvt = (struct NAN_MATCH_EXPIRE_EVENT *)pcuEvtBuf;

	message_len = sizeof(struct NanMatchExpiredIndMsg);

	prNanMatchExpiredInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanMatchExpiredInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	kalMemZero(prNanMatchExpiredInd, message_len);

	prNanMatchExpiredInd->fwHeader.msgVersion = 1;
	prNanMatchExpiredInd->fwHeader.msgId = NAN_MSG_ID_MATCH_EXPIRED_IND;
	prNanMatchExpiredInd->fwHeader.msgLen = message_len;
	prNanMatchExpiredInd->fwHeader.handle =
			prMatchExpireEvt->u2PublishSubscribeID;
	prNanMatchExpiredInd->fwHeader.transactionId = 0;

	prNanMatchExpiredInd->matchExpiredIndParams.matchHandle =
		prMatchExpireEvt->u4RequestorInstanceID;

	DBGLOG(NAN, DEBUG, "[%s] Handle:%d, matchHandle:%d\n", __func__,
		prNanMatchExpiredInd->fwHeader.handle,
		prNanMatchExpiredInd->matchExpiredIndParams.matchHandle);

	/* Fill skb and send to kernel by nl80211 */
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
		message_len + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN,
		GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanMatchExpiredInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb,
		MTK_WLAN_VENDOR_ATTR_NAN,
		message_len,
		prNanMatchExpiredInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree(prNanMatchExpiredInd);
		kfree_skb(skb);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanMatchExpiredInd);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_report_beacon(
	struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf)
{
	struct _NAN_EVENT_REPORT_BEACON *prFwEvt;
	struct WLAN_BEACON_FRAME *prWlanBeaconFrame
		= (struct WLAN_BEACON_FRAME *) NULL;

	prFwEvt = (struct _NAN_EVENT_REPORT_BEACON *) pcuEvtBuf;
	prWlanBeaconFrame = (struct WLAN_BEACON_FRAME *)
		prFwEvt->aucBeaconFrame;

	DBGLOG(NAN, DEBUG,
		"Cl:" MACSTR ",Src:" MACSTR ",rssi:%d,chnl:%d,TsfL:0x%x\n",
		MAC2STR(prWlanBeaconFrame->aucBSSID),
		MAC2STR(prWlanBeaconFrame->aucSrcAddr),
		prFwEvt->i4Rssi,
		prFwEvt->ucHwChnl,
		prFwEvt->au4LocalTsf[0]);

	nanExtComposeBeaconTrack(prAdapter, prFwEvt);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_schedule_config(
	struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf)
{
	g_deEvent++;

	nanUpdateAisBitmap(prAdapter, TRUE);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_lowpower_ctrl(
	struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf)
{
#define NAN_SET_TX_ALLOWED 0
	struct _NAN_EVENT_LOWPOWER_CTRL *prFwEvt;
#if (NAN_SET_TX_ALLOWED == 1)
	struct _NAN_NDL_INSTANCE_T *prNDL = NULL;
	struct _NAN_NDP_CONTEXT_T *prNdpCxt;
	struct _NAN_DATA_PATH_INFO_T *prDataPathInfo;
	uint8_t ucNdlIndex;
	uint8_t ucNdpCxtIdx;
	uint32_t i = 0;
#endif

	if (!prAdapter)
		return -EFAULT;

	prFwEvt = (struct _NAN_EVENT_LOWPOWER_CTRL *) pcuEvtBuf;

	DBGLOG(NAN, DEBUG,
		"Map: 0x%x\n",
		prFwEvt->ucPeerSchRecordTxMap);

#if (NAN_SET_TX_ALLOWED == 1)
	KAL_SPIN_LOCK_DECLARATION();

	prDataPathInfo = &(prAdapter->rDataPathInfo);

	for (ucNdlIndex = 0;
		ucNdlIndex < NAN_MAX_SUPPORT_NDL_NUM;
		ucNdlIndex++) {
		prNDL = &prDataPathInfo->arNDL[ucNdlIndex];
		if (prNDL->fgNDLValid == FALSE)
			continue;

		for (ucNdpCxtIdx = 0;
			ucNdpCxtIdx < NAN_MAX_SUPPORT_NDP_CXT_NUM;
			ucNdpCxtIdx++) {
			prNdpCxt = &prNDL->arNdpCxt[ucNdpCxtIdx];
			if (prNdpCxt->fgValid == FALSE)
				continue;

			KAL_ACQUIRE_SPIN_LOCK(prAdapter,
				SPIN_LOCK_NAN_NDL_FLOW_CTRL);

			for (i = 0; i < NAN_LINK_NUM; i++) {
				if (!prNdpCxt->prNanStaRec[i])
					continue;

				qmSetStaRecTxAllowed(
					prAdapter,
					prNdpCxt->prNanStaRec[i],
					FALSE);
			}

			KAL_RELEASE_SPIN_LOCK(prAdapter,
					SPIN_LOCK_NAN_NDL_FLOW_CTRL);
		}
	}
#endif

	g_ucNanLowPowerMode = TRUE;
	return WLAN_STATUS_SUCCESS;
}

#if CFG_SUPPORT_NAN_R4_PAIRING
int
mtk_cfg80211_vendor_event_nan_pairing_indication(struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf, struct SW_RFB *prSwRfb)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct _NanPairingRequestIndMsg *prNanPairingRequestInd = NULL;
	struct _NanPairingRequestIndParams *pairingParam = NULL;
	size_t message_len = 0;
	struct NanPairingPASNMsg pasnframe = {0};
	uint8_t *tlvs = NULL;
	enum NanPairingRequestType nan_pairing_request_type =
		NAN_PAIRING_SETUP_REQ_T;
	struct net_device *prDev;

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_INDICATION[0]!!\n");

	wiphy = wlanGetWiphy();
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EFAULT;
	}
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	pairingParam = (struct _NanPairingRequestIndParams *)pcuEvtBuf;

	pasnframe.framesize = prSwRfb->u2PacketLen;
	DBGLOG(NAN, INFO, "[pairing-pasn] prSwRfb->u2PacketLen = %u\n",
		prSwRfb->u2PacketLen);
	if (sizeof(pasnframe.PASN_FRAME) >= pasnframe.framesize)
		memcpy(pasnframe.PASN_FRAME,
			prSwRfb->pvHeader, pasnframe.framesize);
	else {
		DBGLOG(NAN, ERROR,
			"FATAL !!! failed to cache full PASN-M1(size:%u)\n",
			pasnframe.framesize);
		return -ENOMEM;
	}
	message_len = sizeof(struct _NanPairingRequestIndMsg);
	message_len += (SIZEOF_TLV_HDR + sizeof(struct NanPairingPASNMsg));
	DBGLOG(NAN, INFO, "message_len=%zu\n", message_len);

	prNanPairingRequestInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanPairingRequestInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanPairingRequestInd, message_len);
	if (!prNanPairingRequestInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}

	prNanPairingRequestInd->fwHeader.msgVersion = 1;
	prNanPairingRequestInd->fwHeader.msgId = NAN_MSG_ID_PAIRING_INDICATION;
	prNanPairingRequestInd->fwHeader.msgLen = message_len;

	/* Indication doesn't have transition ID */
	prNanPairingRequestInd->fwHeader.transactionId = 0;

	/* copy MAC, pairing_request_type, enable_pairing_cache,
	 * nira.nonce, nira.tag
	 */
	memcpy(prNanPairingRequestInd->pairingRequestIndParams.
		peer_disc_mac_addr, pairingParam->peer_disc_mac_addr,
		MAC_ADDR_LEN);
	nan_pairing_request_type = pairingParam->nan_pairing_request_type;
	prNanPairingRequestInd->pairingRequestIndParams.
		nan_pairing_request_type = nan_pairing_request_type;
	prNanPairingRequestInd->pairingRequestIndParams.
		enable_pairing_cache = pairingParam->enable_pairing_cache;
	if (nan_pairing_request_type == NAN_PAIRING_VERIFICATION_REQ_T) {
		memcpy(prNanPairingRequestInd->pairingRequestIndParams.
			nira.nonce, pairingParam->nira.nonce,
			NAN_IDENTITY_NONCE_LEN);
		memcpy(prNanPairingRequestInd->pairingRequestIndParams.nira.tag,
			pairingParam->nira.tag, NAN_IDENTITY_TAG_LEN);
	}

	tlvs = prNanPairingRequestInd->ptlv;
	tlvs = nanAddTlv(NAN_TLV_TYPE_NAN40_PAIRING_RAWFRAME,
			sizeof(struct NanPairingPASNMsg),
			(u8 *)&pasnframe,
			tlvs);

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_INDICATION[1]!!\n");
	hexdump_nan((void *)prNanPairingRequestInd, message_len);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPairingRequestInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPairingRequestInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanPairingRequestInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanPairingRequestInd);

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_INDICATION[2]!!\n");

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_event_nan_pairing_confirm(struct ADAPTER *prAdapter,
						  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct _NanPairingConfirmIndMsg *prNanPairingConfirmInd = NULL;
	struct NAN_FOLLOW_UP_EVENT *prFollowupEvt;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	u32 mask = NAN_CIPHER_SUITE_SHARED_KEY_NONE;
	uint8_t direct_confirm = FALSE;
	struct net_device *prDev;

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_CONFIRM[0]!!\n");

	wiphy = wlanGetWiphy();
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EFAULT;
	}
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	prFollowupEvt = (struct NAN_FOLLOW_UP_EVENT *)pcuEvtBuf;

	message_len =
		sizeof(struct _NanMsgHeader) +
		sizeof(struct _NanPairingConfirmIndParams) +
		(SIZEOF_TLV_HDR + MAC_ADDR_LEN);

	prPairingFsm = pairingFsmSearch(prAdapter, prFollowupEvt->addr);
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "prPairingFsm error\n");
		return -EINVAL;
	}

	direct_confirm =
		((prAdapter->rWifiVar.ucNanPairingNikExAtVerify == FALSE) &&
		prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION);

	DBGLOG(NAN, INFO, "[pairing-pasn] direct confirm\n");

	if (prPairingFsm->ePairingState != NAN_PAIRING_PAIRED)
		return -EINVAL;

	prNanPairingConfirmInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanPairingConfirmInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanPairingConfirmInd, message_len);

	prNanPairingConfirmInd->fwHeader.msgVersion = 1;
	prNanPairingConfirmInd->fwHeader.msgId = NAN_MSG_ID_PAIRING_CONFIRM;
	prNanPairingConfirmInd->fwHeader.msgLen = message_len;
	prNanPairingConfirmInd->fwHeader.handle =
		prFollowupEvt->publish_subscribe_id;


	if (prPairingFsm->fgPeerNIK_received == TRUE ||
		direct_confirm == TRUE) {

		DBGLOG(NAN, INFO,
			"[pairing-pasn] composing NanPairingConfirmIndMsg !!\n");
		/* initial value. it will be filled by Hal */
		prNanPairingConfirmInd->pairingConfirmIndParams.
			pairing_instance_id = 0;
		prNanPairingConfirmInd->pairingConfirmIndParams.
			rsp_code = NAN_PAIRING_REQUEST_ACCEPT;
		prNanPairingConfirmInd->pairingConfirmIndParams.
			reason_code = NAN_STATUS_SUCCESS;
		if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING)
			prNanPairingConfirmInd->pairingConfirmIndParams.
				nan_pairing_request_type =
					NAN_PAIRING_SETUP_REQ_T;
		else if (prPairingFsm->FsmMode ==
			NAN_PAIRING_FSM_MODE_VERIFICATION)
			prNanPairingConfirmInd->pairingConfirmIndParams.
				nan_pairing_request_type =
					NAN_PAIRING_VERIFICATION_REQ_T;


		prNanPairingConfirmInd->pairingConfirmIndParams.
			enable_pairing_cache = prPairingFsm->fgCachingEnable;
		memcpy(prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.peer_nan_identity_key,
			prPairingFsm->aucPeerNik, NAN_NIK_LEN);
		memcpy(prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.local_nan_identity_key,
			prPairingFsm->aucNik, NAN_NIK_LEN);
		memcpy(prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.npk.pmk,
			prPairingFsm->npk, prPairingFsm->npk_len);
		prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.npk.pmk_len =
			prPairingFsm->npk_len;
		prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.akm =
			prPairingFsm->akm;


		switch (prPairingFsm->u4SelCipherType) {
		case NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128:
			mask = NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;
			break;
		case NAN_CIPHER_SUITE_ID_NCS_PK_PASN_256:
			mask = NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_256_MASK;
			break;
		default:
			mask = NAN_CIPHER_SUITE_SHARED_KEY_NONE;
			break;

		}
		prNanPairingConfirmInd->pairingConfirmIndParams.
			npk_security_association.cipher_type = mask;

		/* Add TLV datas */
		tlvs = prNanPairingConfirmInd->ptlv;
		tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
				 prFollowupEvt->addr, tlvs);
	}





	/* Indication doesn't have transition ID */
	prNanPairingConfirmInd->fwHeader.transactionId = 0;

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_CONFIRM[1]!!\n");
	hexdump_nan((void *)prNanPairingConfirmInd, message_len);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPairingConfirmInd);
		return -ENOMEM;
	}
	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPairingConfirmInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanPairingConfirmInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanPairingConfirmInd);

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_CONFIRM[2]!!\n");

	return WLAN_STATUS_SUCCESS;
}


int
mtk_cfg80211_vendor_event_nan_pairing_ndl_disconnect(struct ADAPTER *prAdapter,
	  uint8_t *pcuEvtBuf)
{
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;
	struct _NanPairingNdlDisconnectIndMsg *prNanPairingNdlDisconnectInd;
	struct _NanPairingNdlDisconnectIndParams
		*prPairingNdlDisconnectIndParams;
	uint8_t *tlvs = NULL;
	size_t message_len = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_NDLDISCONNECT[1]!!\n");
	wiphy = wlanGetWiphy();
	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;
	prPairingFsm = (struct PAIRING_FSM_INFO *)pcuEvtBuf;
	message_len =
		sizeof(struct _NanMsgHeader) +
		sizeof(struct _NanPairingNdlDisconnectIndParams) +
		(SIZEOF_TLV_HDR + MAC_ADDR_LEN);
	prNanPairingNdlDisconnectInd = kmalloc(message_len, GFP_KERNEL);
	if (!prNanPairingNdlDisconnectInd) {
		DBGLOG(NAN, ERROR, "Allocate failed\n");
		return -ENOMEM;
	}
	kalMemZero(prNanPairingNdlDisconnectInd, message_len);
	prNanPairingNdlDisconnectInd->fwHeader.msgVersion = 1;
	prNanPairingNdlDisconnectInd->fwHeader.msgId
		= NAN_MSG_ID_PAIRING_NDLDISCONNECT;
	prNanPairingNdlDisconnectInd->fwHeader.msgLen = message_len;

	prPairingNdlDisconnectIndParams =
		&prNanPairingNdlDisconnectInd->pairingNdlDisconnectIndParams;

	prPairingNdlDisconnectIndParams->pairing_instance_id = 0;
	prPairingNdlDisconnectIndParams->reason_code = 0;

	tlvs = prNanPairingNdlDisconnectInd->ptlv;
	tlvs = nanAddTlv(NAN_TLV_TYPE_MAC_ADDRESS, MAC_ADDR_LEN,
			 prPairingFsm->prStaRec->aucMacAddr, tlvs);

	/*  Fill skb and send to kernel by nl80211*/
	skb = kalCfg80211VendorEventAlloc(wiphy, wdev,
					  message_len + NLMSG_HDRLEN,
					  WIFI_EVENT_SUBCMD_NAN, GFP_KERNEL);

	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		kfree(prNanPairingNdlDisconnectInd);
		return -ENOMEM;
	}

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN, message_len,
			     prNanPairingNdlDisconnectInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		kfree(prNanPairingNdlDisconnectInd);
		return -EFAULT;
	}
	cfg80211_vendor_event(skb, GFP_KERNEL);
	kfree(prNanPairingNdlDisconnectInd);

	DBGLOG(NAN, INFO, "NAN_MSG_ID_PAIRING_NDLDISCONNECT[2]!!\n");

	return WLAN_STATUS_SUCCESS;
}
int mtk_cfg80211_vendor_nan_pasn_rsp(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *data,
	int data_len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct nlattr *tb
		[MTK_WLAN_VENDOR_ATTR_MAX + 1] = {};
	struct MSDU_INFO *prMgmtFrame = (struct MSDU_INFO *) NULL;
	struct MSG_MGMT_TX_REQUEST *prMsgTxReq = NULL;
	uint32_t frame_size;

	if (!wiphy || !wdev || !data || !data_len) {
		DBGLOG(NAN, ERROR, "%s():input data null.\n", __func__);
		rStatus = -EINVAL;
		goto exit;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "get glue structure fail.\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(NAN, WARN, "driver is not ready\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->prAdapter->rWifiVar.ucNanEnablePairing == 0) {
		DBGLOG(NAN, WARN, "Pairing is not enabled\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_MAX,
		data,
		data_len,
		nla_pasn_rsp_policy)) {
		DBGLOG(NAN, ERROR, "NLA_PARSE failed\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (!tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_RAW_FRAME]) {
		DBGLOG(NAN, ERROR, "Failed to get raw PASN frame\n");
		rStatus = -EINVAL;
		goto exit;
	}

	prMsgTxReq = cnmMemAlloc(prGlueInfo->prAdapter,
			RAM_TYPE_MSG, sizeof(struct MSG_MGMT_TX_REQUEST));
	if (prMsgTxReq == NULL) {
		DBGLOG(REQ, ERROR, "allocate msg req. fail.\n");
		rStatus = -ENOMEM;
		goto exit;
	}

	prMsgTxReq->rMsgHdr.eMsgId = MID_MNY_NAN_MGMT_TX;
	prMsgTxReq->ucBssIdx = NAN_DEFAULT_INDEX;
	frame_size = nla_len(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_RAW_FRAME]);
	prMgmtFrame = cnmMgtPktAlloc(prGlueInfo->prAdapter,
				(int32_t) (frame_size + sizeof(uint64_t)
				+ MAC_TX_RESERVED_FIELD));
	if (prMgmtFrame == NULL) {
		rStatus = -ENOMEM;
		goto exit;
	}
	prMsgTxReq->prMgmtMsduInfo = prMgmtFrame;
	prMsgTxReq->prMgmtMsduInfo->u2FrameLength = frame_size;
	kalMemCopy((uint8_t *)((unsigned long) prMgmtFrame->prPacket +
		MAC_TX_RESERVED_FIELD),
		nla_data(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_RAW_FRAME]),
		frame_size);

	mboxSendMsg(prGlueInfo->prAdapter,
			MBOX_ID_0,
			(struct MSG_HDR *) prMsgTxReq,
			MSG_SEND_METHOD_BUF);
	return rStatus;
exit:
	if (prMsgTxReq && prMsgTxReq->prMgmtMsduInfo)
		cnmMgtPktFree(prGlueInfo->prAdapter,
			prMsgTxReq->prMgmtMsduInfo);
	if (prMsgTxReq)
		cnmMemFree(prGlueInfo->prAdapter, prMsgTxReq);

	return rStatus;
}


int mtk_cfg80211_vendor_nan_pasn_setkey(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *data,
	int data_len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct nlattr *tb
		[MTK_WLAN_VENDOR_ATTR_MAX + 1] = {};
	struct nan_pairing_keys_t key_data = {0};
	struct NanTransmitFollowupRequest *pNanXmitFollowupReq = NULL;
	uint8_t direct_confirm = FALSE;

	if (!wiphy || !wdev || !data || !data_len) {
		DBGLOG(NAN, ERROR, "%s():input data null.\n", __func__);
		rStatus = -EINVAL;
		goto exit;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "get glue structure fail.\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(NAN, WARN, "driver is not ready\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_MAX,
		data,
		data_len,
		nla_pasn_setkey_policy)) {
		DBGLOG(NAN, ERROR, "NLA_PARSE failed\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (!tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]) {
		DBGLOG(NAN, ERROR, "Failed to get nan_pairing_keys\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (nla_len(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]) !=
			sizeof(struct nan_pairing_keys_t)) {
		DBGLOG(NAN, ERROR,
			"[pairing-key1] NAN Pairing Key Data size mismatch !\n");
		rStatus = -EINVAL;
		goto exit;
	}
	kalMemCopy(&key_data,
		nla_data(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]),
		sizeof(struct nan_pairing_keys_t));

	DBGLOG(NAN, INFO,
		"[pairing-key1] enter mtk_cfg80211_vendor_nan_pasn_setkey()\n");

	if (nanPasnSetKey(prGlueInfo->prAdapter, &key_data) ==
		WLAN_STATUS_SUCCESS) {
		struct PAIRING_FSM_INFO *prPairingFsm = NULL;

		prPairingFsm = pairingFsmSearch(prGlueInfo->prAdapter,
			key_data.peer_mac_addr);
		if (!prPairingFsm) {
			DBGLOG(NAN, ERROR, "prPairingFsm error\n");
			rStatus = -EINVAL;
			goto exit;
		}
		direct_confirm =
		(prGlueInfo->prAdapter->rWifiVar.ucNanPairingNikExAtVerify
		== FALSE &&
		prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION);

		/* for responder , only key save happens here and
		 * key install happens at reception of PASN-M3 confirm
		 * for initiator , both key save and key install happens here.
		 */
		if (prPairingFsm->ucPairingType == NAN_PAIRING_REQUESTOR) {
			pairingFsmSteps(prGlueInfo->prAdapter, prPairingFsm,
				NAN_PAIRING_PAIRED);
		}

		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			prPairingFsm->ucPairingType == NAN_PAIRING_REQUESTOR &&
			prPairingFsm->fgCachingEnable == TRUE &&
			(prPairingFsm->fgPeerNIK_received == FALSE &&
			prPairingFsm->fgLocalNIK_sent == FALSE)) {
			int remainingtime = 0;

			if (prPairingFsm->fgM3Acked == FALSE) {
				DBGLOG(NAN, INFO,
					"[pairing-keyEx] wait M3 acked by responder(max:3s)->\n");
				/* coverity[double_unlock]
				 * no wait queue unclock other than below.
				 */
				remainingtime =
				wait_event_timeout(prPairingFsm->confirm_ack_wq,
				prPairingFsm->fgM3Acked == TRUE, 3 * HZ);
				if (remainingtime > 0) {
					DBGLOG(NAN, INFO,
						"[pairing-keyEx] wait responder to install TK\n");
					msleep(512);
				} else if (remainingtime == 0) {
					DBGLOG(NAN, ERROR,
						"[pairing-keyEx] wait M3 ack timeout!!\n");
					rStatus =  -ETIMEDOUT;
					goto exit;
				}
			}

			if (nanGetFeatureIsSigma(prGlueInfo->prAdapter) &&
			prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING) {
				sigma_nik_exchange_run = false;
				do {
					DBGLOG(NAN, INFO,
						"[pairing-keyEx] waiting UCC command\n");
					msleep(1000);
				} while (sigma_nik_exchange_run == false);
				sigma_nik_exchange_run = false;
			}

			if  (direct_confirm) {
				struct NAN_FOLLOW_UP_EVENT
				*prFollowupEvent = NULL;

				DBGLOG(NAN, INFO,
					"[pairing-pasn] NOT trigger NIK exchange!!\n");
				prFollowupEvent =
				kmalloc(sizeof(struct NAN_FOLLOW_UP_EVENT),
					GFP_ATOMIC);
				if (!prFollowupEvent) {
					DBGLOG(NAN, ERROR, "Allocate failed\n");
					rStatus =  -ENOMEM;
					goto exit;
				}
				kalMemZero(prFollowupEvent,
				sizeof(struct NAN_FOLLOW_UP_EVENT));
				memcpy(prFollowupEvent->addr,
					prPairingFsm->prStaRec->aucMacAddr,
					NAN_MAC_ADDR_LEN);
				mtk_cfg80211_vendor_event_nan_pairing_confirm(
					prGlueInfo->prAdapter,
					(uint8_t *)prFollowupEvent);
				kfree(prFollowupEvent);

			} else {
				DBGLOG(NAN, INFO,
					"[pairing-keyEx] trigger NIK exchange at initiator!!\n");
				pNanXmitFollowupReq =
				kmalloc(
				sizeof(struct NanTransmitFollowupRequest),
				GFP_ATOMIC);
				if (!pNanXmitFollowupReq) {
					DBGLOG(NAN, ERROR, "Allocate failed\n");
					rStatus =  -ENOMEM;
					goto exit;
				}
				kalMemZero(pNanXmitFollowupReq,
				sizeof(struct NanTransmitFollowupRequest));
				pNanXmitFollowupReq->publish_subscribe_id =
					prPairingFsm->ucSubscribeID;
				pNanXmitFollowupReq->requestor_instance_id =
					prPairingFsm->ucPublishID;
				memcpy(pNanXmitFollowupReq->addr,
					prPairingFsm->prStaRec->aucMacAddr,
					NAN_MAC_ADDR_LEN);
				nanTransmitRequest_host(prGlueInfo->prAdapter,
					pNanXmitFollowupReq);
				kfree(pNanXmitFollowupReq);
			}
		}
	}

	return rStatus;
exit:
	return rStatus;
}

int mtk_cfg80211_vendor_nan_pasn_confirm(
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	const void *data,
	int data_len)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo;
	struct nlattr *tb
		[MTK_WLAN_VENDOR_ATTR_MAX + 1] = {};
	struct nan_pairing_keys_t key_data = {0};
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint8_t direct_confirm = FALSE;

	if (!wiphy || !wdev || !data || !data_len) {
		DBGLOG(NAN, ERROR, "input data null.\n");
		rStatus = -EINVAL;
		goto exit;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "get glue structure fail.\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (prGlueInfo->u4ReadyFlag == 0) {
		DBGLOG(NAN, WARN, "driver is not ready\n");
		rStatus = -EFAULT;
		goto exit;
	}

	if (NLA_PARSE(tb, MTK_WLAN_VENDOR_ATTR_MAX,
		data,
		data_len,
		nla_pasn_setkey_policy)) {
		DBGLOG(NAN, ERROR, "NLA_PARSE failed\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (!tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]) {
		DBGLOG(NAN, ERROR, "Failed to get nan_pairing_keys\n");
		rStatus = -EINVAL;
		goto exit;
	}

	if (nla_len(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]) !=
			sizeof(struct nan_pairing_keys_t)) {
		DBGLOG(NAN, ERROR,
			"[pairing-pasn] NAN Pairing Key Data size mismatch !\n");
		rStatus = -EINVAL;
		goto exit;
	}
	kalMemCopy(&key_data,
		nla_data(tb[MTK_WLAN_VENDOR_ATTR_NAN_PASN_SET_KEY]),
		sizeof(struct nan_pairing_keys_t));

	prPairingFsm = pairingFsmSearch(prGlueInfo->prAdapter,
			key_data.peer_mac_addr);
	if (prPairingFsm) {
		direct_confirm =
		((prGlueInfo->prAdapter->rWifiVar.ucNanPairingNikExAtVerify
		== FALSE) &&
		prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION);
	}
	if  (prPairingFsm && prPairingFsm->prStaRec && direct_confirm) {
		struct NAN_FOLLOW_UP_EVENT *prFollowupEvent = NULL;

		DBGLOG(NAN, INFO,
			"[pairing-pasn] verifiction stage confirm upload directly()\n");
		prFollowupEvent =
			kmalloc(sizeof(struct NAN_FOLLOW_UP_EVENT),
					GFP_ATOMIC);
		if (!prFollowupEvent) {
			DBGLOG(NAN, ERROR, "Allocate failed\n");
			rStatus =  -ENOMEM;
			goto exit;
		}
		kalMemZero(prFollowupEvent, sizeof(struct NAN_FOLLOW_UP_EVENT));
		memcpy(prFollowupEvent->addr,
			prPairingFsm->prStaRec->aucMacAddr,
			NAN_MAC_ADDR_LEN);
		mtk_cfg80211_vendor_event_nan_pairing_confirm(
			prGlueInfo->prAdapter,
			(uint8_t *)prFollowupEvent);
		kfree(prFollowupEvent);
	}

	return rStatus;
exit:
	return rStatus;
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */

#if CFG_SUPPORT_NAN_EXT
int mtk_cfg80211_vendor_nan_ext_indication(struct ADAPTER *prAdapter,
					   u8 *data, uint16_t u2Size)
{
	struct NanExtIndMsg nanExtInd = {0};
	struct sk_buff *skb = NULL;
	struct wiphy *wiphy;
	struct wireless_dev *wdev;
	struct net_device *prDev;

	wiphy = wlanGetWiphy();
	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EFAULT;
	}

	prDev = wlanGetNetDev(prAdapter->prGlueInfo, NAN_DEFAULT_INDEX);

	if (!prDev) {
		DBGLOG(NAN, ERROR, "prDev for Bss0 not exist.\n");
		return -ENODEV;
	}
	wdev = prDev->ieee80211_ptr;

	nanExtInd.fwHeader.msgVersion = 1;
	nanExtInd.fwHeader.msgId = NAN_MSG_ID_EXT_IND;
	nanExtInd.fwHeader.msgLen = u2Size;
	nanExtInd.fwHeader.transactionId = 0;

	skb = kalCfg80211VendorEventAlloc(
		wiphy, wdev, sizeof(struct NanExtIndMsg) + NLMSG_HDRLEN,
		WIFI_EVENT_SUBCMD_NAN_EXT, GFP_KERNEL);
	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb failed\n");
		return -ENOMEM;
	}

	kalMemCopy(nanExtInd.data, data, u2Size);
	DBGLOG(NAN, DEBUG, "NAN Ext Ind:\n");
	DBGLOG_HEX(NAN, DEBUG, nanExtInd.data, u2Size)

	if (unlikely(nla_put(skb, MTK_WLAN_VENDOR_ATTR_NAN,
			     sizeof(struct NanExtIndMsg),
			     &nanExtInd) < 0)) {
		DBGLOG(NAN, ERROR, "nla_put_nohdr failed\n");
		kfree_skb(skb);
		return -EFAULT;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);

	return WLAN_STATUS_SUCCESS;
}

int
mtk_cfg80211_vendor_nan_ext(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct sk_buff *skb = NULL;
	struct ADAPTER *prAdapter;

	struct NanExtCmdMsg extCmd = {0};
	struct NanExtResponseMsg extRsp = {0};
	u32 u4BufLen;
	u32 i4Status = -EINVAL;

	if (!wiphy) {
		DBGLOG(NAN, ERROR, "wiphy error!\n");
		return -EINVAL;
	}

	WIPHY_PRIV(wiphy, prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(NAN, ERROR, "prGlueInfo error!\n");
		return -EINVAL;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error!\n");
		return -EINVAL;
	}

	if (!wdev) {
		DBGLOG(NAN, ERROR, "wdev error!\n");
		return -EINVAL;
	}

	if (data == NULL || data_len < sizeof(struct NanExtCmdMsg)) {
		DBGLOG(NAN, ERROR, "data error(len=%d)\n", data_len);
		return -EINVAL;
	}

	/* read ext cmd */
	kalMemCopy(&extCmd, data, sizeof(struct NanExtCmdMsg));
	extRsp.fwHeader = extCmd.fwHeader;

	/* execute ext cmd */
	i4Status = kalIoctl(prGlueInfo, wlanoidNANExtCmd, &extCmd,
				sizeof(struct NanExtCmdMsg), &u4BufLen);
	if (i4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, ERROR, "kalIoctl NAN Ext Cmd failed\n");
		return -EFAULT;
	}
	kalMemCopy(extRsp.data, extCmd.data, extCmd.fwHeader.msgLen);
	extRsp.fwHeader.msgLen = extCmd.fwHeader.msgLen;

	DBGLOG(NAN, TRACE, "Resp data:");
	DBGLOG_HEX(NAN, TRACE, extRsp.data, extCmd.fwHeader.msgLen)
	/* reply to framework */
	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
				sizeof(struct NanExtResponseMsg));

	if (!skb) {
		DBGLOG(NAN, ERROR, "Allocate skb %zu bytes failed\n",
		       sizeof(struct NanExtResponseMsg));
		return -ENOMEM;
	}

	if (unlikely(
		nla_put_nohdr(skb, sizeof(struct NanExtResponseMsg),
			(void *)&extRsp) < 0)) {
		DBGLOG(NAN, ERROR, "Fail send reply\n");
		goto failure;
	}

	i4Status = kalIoctl(prGlueInfo, wlanoidNANExtCmdRsp, (void *)&extRsp,
				sizeof(struct NanExtResponseMsg), &u4BufLen);

	if (i4Status != WLAN_STATUS_SUCCESS) {
		DBGLOG(NAN, ERROR, "kalIoctl NAN Ext Cmd Rsp failed\n");
		goto failure;
	}

	return cfg80211_vendor_cmd_reply(skb);

failure:
	kfree_skb(skb);
	return -EFAULT;
}
#endif /* CFG_SUPPORT_NAN_EXT */
#endif /* CFG_SUPPORT_NAN */
