/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#if CFG_SUPPORT_NAN
#ifndef INCLUDE_FROM_NAN
#define INCLUDE_FROM_NAN
#include "precomp.h"
#undef INCLUDE_FROM_NAN
#endif
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
#include "wpa_supp/FourWayHandShake.h"
#include "wpa_supp/src/ap/wpa_auth_glue.h"
#include "precomp.h"
#include "nan_pairing.h"
#include "nan_sec.h"
struct wpa_ptk g_ptk[NAN_MAX_NDP_SESSIONS];
struct wpa_sm_ctx g_rNanWpaPairingSmCtx;
uint64_t g_publisherNonce;
uint8_t g_last_matched_report_npba_dialogTok;
uint8_t g_last_bootstrapReq_npba_dialogTok;
static const char * const
		apucDebugParingMgmtState[NAN_PAIRING_MGMT_STATE_NUM] = {
	"NAN_PAIRING_INIT",
	"NAN_PAIRING_BOOTSTRAPPING",
	"NAN_PAIRING_BOOTSTRAPPING_DONE",
	"NAN_PAIRING_SETUP",
	"NAN_PAIRING_PAIRED",
	"NAN_PAIRING_PAIRED_VERIFICATION",
	"NAN_PAIRING_INVALID",
	"NAN_PAIRING_CANCEL",
};
uint32_t nanPairingPeerNikReceived(struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf, struct PAIRING_FSM_INFO *prPairingFsm)
{
	prPairingFsm->fgPeerNIK_received = TRUE;
	DBGLOG(NAN, INFO, "Enter!\n");
	if (mtk_cfg80211_vendor_event_nan_pairing_confirm(prAdapter,
		pcuEvtBuf) != 0) {
		DBGLOG(NAN, ERROR, "[pairing-Ex] failed\n");
		return WLAN_STATUS_FAILURE;
	}
	return WLAN_STATUS_SUCCESS;
}
uint32_t nanPasnRxAuthFrame(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct NanPairingPASNMsg pasnframe;
	uint32_t nan_pairing_request_type = NAN_PAIRING_SETUP_REQ_T;
	uint8_t enable_pairing_cache = 1;
	struct NanIdentityResolutionAttribute nira = {0};

	prAuthFrame = (struct WLAN_AUTH_FRAME *)prSwRfb->pvHeader;
	if (prAuthFrame == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] prAuthFrame=%p\n",
			prAuthFrame);
		rRetStatus = WLAN_STATUS_FAILURE;
		goto out;
	} else {
		uint32_t ret = WLAN_STATUS_SUCCESS;
		/* only M1/M2 include NAN IE */
		if (prAuthFrame->u2AuthTransSeqNo == PASN_M1_AUTH_SEQ ||
			prAuthFrame->u2AuthTransSeqNo == PASN_M2_AUTH_SEQ) {
			ret = nanPairingProcessAuthFrame(prAdapter,
				prSwRfb,
				&nan_pairing_request_type,
				&enable_pairing_cache,
				nira.nonce,
				nira.tag);
		}
		if (ret != WLAN_STATUS_SUCCESS) {
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			DBGLOG(NAN, INFO,
				"[pairing-pasn] request_type =%u\n",
				nan_pairing_request_type);
		}
	}
	DBGLOG(NAN, INFO, "pump-up PasnRx AuthFrame(PASN-M%u)\n",
		prAuthFrame->u2AuthTransSeqNo);
	if (prAuthFrame->u2AuthTransSeqNo == PASN_M1_AUTH_SEQ) {
		prPairingFsm =
			pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
		if (prPairingFsm == NULL &&
			nan_pairing_request_type == NAN_PAIRING_SETUP_REQ_T) {
			DBGLOG(NAN, ERROR, "ERROR prPairingFsm is NULL\n");
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			kalIndicateNanPairingRequest(prAdapter,
				prAdapter->prGlueInfo, prSwRfb, ucBssIdx);
		}
	} else if (prAuthFrame->u2AuthTransSeqNo == PASN_M2_AUTH_SEQ) {
		prPairingFsm =
			pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
		if (prPairingFsm == NULL ||
			prPairingFsm->ucPairingType != NAN_PAIRING_REQUESTOR) {
			DBGLOG(NAN, ERROR, "PasnRx AuthFrame\n");
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
			pasnframe.framesize = prSwRfb->u2PacketLen;
			if (sizeof(pasnframe.PASN_FRAME) <
			    pasnframe.framesize) {
				DBGLOG(NAN, ERROR,
					"FATAL!! fail to cache full PASN-M2(size:%u)\n",
					pasnframe.framesize);
				rRetStatus = WLAN_STATUS_RESOURCES;
				goto out;
			} else {
				memcpy(pasnframe.PASN_FRAME, prSwRfb->pvHeader,
					pasnframe.framesize);
			}
			/* TODO : from Standard, M2 is not indicated to Host.
			 * (M3 tx with Confirm is indicated instead)
			 * directly report it to wpa_supplicant.
			 */
			nanPasnRequestM2_Rx(prAdapter, &pasnframe);
		}
	} else if (prAuthFrame->u2AuthTransSeqNo == PASN_M3_AUTH_SEQ) {
		prPairingFsm =
			pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
		if (prPairingFsm == NULL ||
			prPairingFsm->ucPairingType != NAN_PAIRING_RESPONDER) {
			DBGLOG(NAN, ERROR,
				"prPairingFsm or ucPairingType ERROR\n");
			rRetStatus = WLAN_STATUS_FAILURE;
			goto out;
		} else {
			memset(&pasnframe, 0, sizeof(struct NanPairingPASNMsg));
			pasnframe.framesize = prSwRfb->u2PacketLen;
			if (sizeof(pasnframe.PASN_FRAME) <
				pasnframe.framesize) {
				DBGLOG(NAN, ERROR,
					"FATAL!! fail to cache full PASN-M3size:%u)\n",
					pasnframe.framesize);
				rRetStatus = WLAN_STATUS_RESOURCES;
				goto out;
			} else {
				memcpy(pasnframe.PASN_FRAME, prSwRfb->pvHeader,
					pasnframe.framesize);
			}
			/* TODO : from Standard, M3 rx with Confirm is
			* indicated to Host
			* directly report it to wpa_supplicant.
			*/
			nanPasnRequestM3_Rx(prAdapter, &pasnframe);
		}
	} else {
		DBGLOG(NAN, ERROR, "unexpected Auth SeqNo\n");
		rRetStatus = WLAN_STATUS_FAILURE;
		goto out;
	}
	DBGLOG(NAN, INFO, "[pairing-pasn] rRetStatus=%u\n", rRetStatus);
out:
	return rRetStatus;
}
uint32_t nanPasnRequestM1_Tx(struct ADAPTER *prAdapter,
	struct NAN_PASN_START_PARAM *prPasnStartParam)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	uint8_t *pucLocalNMI = NULL;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint8_t *pucPeerAddr = prPasnStartParam->peer_mac_addr;

	DBGLOG(NAN, INFO, "RequestType=%u, Cipher=%u\n",
		prPasnStartParam->nan_pairing_request_type,
		prPasnStartParam->cipher_type);
	if (prPasnStartParam->nan_pairing_request_type ==
	    NAN_PAIRING_VERIFICATION_REQ_T) {
		DBGLOG(NAN, DEBUG,
			"[pairing-verification] debug peer NMI="MACSTR_A"\n",
			pucPeerAddr[0], pucPeerAddr[1], pucPeerAddr[2],
			pucPeerAddr[3], pucPeerAddr[4], pucPeerAddr[5]);
	}

	prPairingFsm =
		pairingFsmSearch(prAdapter, prPasnStartParam->peer_mac_addr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR,
			"[pairing-pasn] (initiator) cannot find PairingFsm\n");
		return WLAN_STATUS_FAILURE;
	}
	prPairingFsm->ucPairingType = NAN_PAIRING_REQUESTOR;

	pucLocalNMI = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	memcpy(prPasnStartParam->own_mac_addr, pucLocalNMI, MAC_ADDR_LEN);
	rRetStatus = nanDevGetClusterId(prAdapter, prPasnStartParam->bssid);
	DBGLOG(NAN, INFO,
		"rRetStatus=%d prPairingFsm->ePairingState=%d prPasnStartParam->nan_pairing_request_type=%d\n",
		rRetStatus, prPairingFsm->ePairingState,
		prPasnStartParam->nan_pairing_request_type);
	if (rRetStatus == WLAN_STATUS_SUCCESS &&
	    prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE &&
	    prPasnStartParam->nan_pairing_request_type ==
	    NAN_PAIRING_SETUP_REQ_T){
		DBGLOG(NAN, INFO,
			"[pairing-pasn] (initiator)Trigger pairing setup\n");
		pairingFsmSteps(prAdapter, prPairingFsm, NAN_PAIRING_SETUP);
		prPasnStartParam->nan_ie_len = prPairingFsm->nan_ie_len;
		memcpy(prPasnStartParam->nan_ie,
			prPairingFsm->nan_ie,
			prPairingFsm->nan_ie_len);

		kalRequestNanPasnStart(prAdapter,
			(void *)prPasnStartParam, ucBssIdx);
	} else if (rRetStatus == WLAN_STATUS_SUCCESS &&
		prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE &&
		prPasnStartParam->nan_pairing_request_type ==
		NAN_PAIRING_VERIFICATION_REQ_T) {
		DBGLOG(NAN, INFO,
			"[pairing-verification] (initiator)Trigger verification\n");
		pairingFsmSteps(prAdapter, prPairingFsm,
			NAN_PAIRING_PAIRED_VERIFICATION);
		prPasnStartParam->nan_ie_len = prPairingFsm->nan_ie_len;
		memcpy(prPasnStartParam->nan_ie, prPairingFsm->nan_ie,
			prPairingFsm->nan_ie_len);

		nanPairingCalCustomPMKID(
			*(uint64_t *)prPairingFsm->aucNonce,
			prPairingFsm->u8Tag,
			(uint8_t *)prPasnStartParam->pmkid);

		kalRequestNanPasnStart(prAdapter,
			(void *)prPasnStartParam, ucBssIdx);
	} else {
		DBGLOG(NAN, ERROR,
			"Status=%u PairingState=%u, PairingType=%u\n",
			rRetStatus,
			prPairingFsm->ePairingState,
			prPasnStartParam->nan_pairing_request_type);
	}

	return rRetStatus;
}


uint32_t nanPasnRequestM1_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *prPasnframe)
{
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;

	if (prPasnframe->framesize == 0) {
		DBGLOG(NAN, ERROR, "Invalid packet!\n");
		return WLAN_STATUS_INVALID_PACKET;
	}
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prPasnframe->PASN_FRAME;

	prPairingFsm = pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] prPairingFsm is NULL!\n");
		return WLAN_STATUS_FAILURE;
	}

	prPairingFsm->ucPairingType = NAN_PAIRING_RESPONDER;
	if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING &&
		prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE) {
		DBGLOG(NAN, INFO, "Handle pairing-setup");
		pairingFsmSteps(prAdapter, prPairingFsm, NAN_PAIRING_SETUP);

		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
				prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}

		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);
	} else if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION &&
		prPairingFsm->ePairingState == NAN_PAIRING_BOOTSTRAPPING_DONE) {
		DBGLOG(NAN, INFO, "Handle pairing-verification");
		pairingFsmSteps(prAdapter, prPairingFsm,
		NAN_PAIRING_PAIRED_VERIFICATION);

		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
				prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}
		nanPairingCalCustomPMKID(*(uint64_t *)prPairingFsm->aucNonce,
			prPairingFsm->u8Tag,
			(uint8_t *)prPasnframe->localhostconfig.
			hostM1param.pmkid);

		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);
	}
	return rRetStatus;
}

uint32_t nanPasnRequestM2_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *prPasnframe) {
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;

	if (prPasnframe->framesize == 0) {
		DBGLOG(NAN, ERROR, "Invalid packet!\n");
		return WLAN_STATUS_INVALID_PACKET;
	}
	prAuthFrame = (struct WLAN_AUTH_FRAME *)prPasnframe->PASN_FRAME;

	prPairingFsm = pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] prPairingFsm is NULL!\n");
		return WLAN_STATUS_FAILURE;
	}

/*	prPairingFsm->ucPairingType = NAN_PAIRING_REQUESTOR;
*	--> it should already iniaiated to NAN_PAIRING_REQUESTOR
*	before sending M1.
*/
	DBGLOG(NAN, INFO,
		"[pairing-pasn] ucPairingType=%u ePairingState=%u\n",
		prPairingFsm->ucPairingType, prPairingFsm->ePairingState);
	if (prPairingFsm->ePairingState == NAN_PAIRING_SETUP ||
		(prPairingFsm->ePairingState ==
		NAN_PAIRING_PAIRED_VERIFICATION)) {
		DBGLOG(NAN, INFO, "(initiator)continue pairing setup\n");
		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
				prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}
		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);
	}

	return rRetStatus;
}

uint32_t nanPasnRequestM3_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *prPasnframe) {
	uint32_t rRetStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIdx = NAN_DEFAULT_INDEX;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct WLAN_AUTH_FRAME *prAuthFrame;

	if (prPasnframe->framesize == 0) {
		DBGLOG(NAN, INFO, "Invalid Packet!\n");
		return WLAN_STATUS_INVALID_PACKET;
	}
	prAuthFrame = (struct WLAN_AUTH_FRAME *) prPasnframe->PASN_FRAME;

	prPairingFsm = pairingFsmSearch(prAdapter, prAuthFrame->aucSrcAddr);
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "[pairing-pasn] prPairingFsm is NULL !\n");
		return WLAN_STATUS_FAILURE;
	}

	DBGLOG(NAN, INFO, "PairingType=%u PairingState=%u",
		prPairingFsm->ucPairingType, prPairingFsm->ePairingState);
	if (prPairingFsm->ePairingState == NAN_PAIRING_SETUP ||
	    prPairingFsm->ePairingState == NAN_PAIRING_PAIRED_VERIFICATION) {
		DBGLOG(NAN, INFO, "(responder)[%s] continue pairing setup\n");
		if (prPairingFsm->nan_ie_len > 0) {
			prPasnframe->nan_ie_len = prPairingFsm->nan_ie_len;
			memcpy(prPasnframe->nan_ie, prPairingFsm->nan_ie,
			prPairingFsm->nan_ie_len);
		} else {
			prPasnframe->nan_ie_len = 0;
		}
		kalReportNanPasnFrame(prAdapter, (void *)prPasnframe, ucBssIdx);

		/* for responder , key install happens
		 * at reception of PASN-M3 confirm
		 */
		pairingFsmSteps(prAdapter, prPairingFsm, NAN_PAIRING_PAIRED);

	}
	DBGLOG(NAN, INFO, "[pairing-pasn] ret=%u\n", rRetStatus);

	return rRetStatus;
}

uint32_t nanPasnSetKey(struct ADAPTER *prAdapter,
	struct nan_pairing_keys_t *key_data) {
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return NAN_PAIRING_INVALID;
	}
	prPairingFsm = pairingFsmSearch(prAdapter, key_data->peer_mac_addr);
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "prPairingFsm error\n");
		return NAN_PAIRING_INVALID;
	}
	if (prPairingFsm->ePairingState != NAN_PAIRING_SETUP &&
	prPairingFsm->ePairingState != NAN_PAIRING_PAIRED_VERIFICATION) {
		DBGLOG(NAN, ERROR, "PairingState error\n");
		return NAN_PAIRING_INVALID;
	}

	switch (key_data->selected_pairwise_cipher) {
	case WPA_CIPHER_CCMP:
		prPairingFsm->u4SelCipherType =
			NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
		break;
	case WPA_CIPHER_GCMP_256:
		prPairingFsm->u4SelCipherType =
			NAN_CIPHER_SUITE_ID_NCS_PK_PASN_256;
		break;
	default:
		DBGLOG(NAN, ERROR, "selected_pairwise_cipher error\n");
		return NAN_PAIRING_INVALID;
	}
	kalMemCopy(prPairingFsm->prPtk->tk, key_data->nm_tk, NAN_NM_TK_MAX_LEN);
	prPairingFsm->prPtk->tk_len = key_data->nm_tk_len;
	/* NM-TK */
	DBGLOG(NAN, DEBUG, "prPairingFsm->prPtk->tk_len=%zu\n",
		prPairingFsm->prPtk->tk_len);
	DBGDUMP_HEX(NAN, DEBUG, "prPairingFsm->prPtk->tk",
		prPairingFsm->prPtk->tk, prPairingFsm->prPtk->tk_len);

	/* Set NM-TK at PASN M2 first for pairing verification */
	if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED_VERIFICATION) {
		nanPairingInstallTk(prPairingFsm->prPtk,
			prPairingFsm->u4SelCipherType, prPairingFsm->prStaRec);
		nanPairingInstallTk(prPairingFsm->prPtk,
			prPairingFsm->u4SelCipherType,
			prPairingFsm->prStaRec5G);
	}

	kalMemCopy(prPairingFsm->prPtk->kek, key_data->nm_kek,
		NAN_NM_KEK_MAX_LEN);
	prPairingFsm->prPtk->kek_len = key_data->nm_kek_len;
	/* NM-KEK */
	DBGLOG(NAN, DEBUG, "prPairingFsm->prPtk->kek_len=%zu\n",
		prPairingFsm->prPtk->kek_len);
	DBGDUMP_HEX(NAN, DEBUG, "prPairingFsm->prPtk->kek",
		prPairingFsm->prPtk->kek, prPairingFsm->prPtk->kek_len);

	kalMemCopy(prPairingFsm->prPtk->kck, key_data->nm_kck,
		NAN_NM_KCK_MAX_LEN);
	prPairingFsm->prPtk->kck_len = key_data->nm_kck_len;
	/* NM-KCK */
	DBGLOG(NAN, DEBUG, "prPairingFsm->prPtk->kck_len=%zu\n",
		prPairingFsm->prPtk->kck_len);
	DBGDUMP_HEX(NAN, DEBUG, "prPairingFsm->prPtk->kck",
		prPairingFsm->prPtk->kck, prPairingFsm->prPtk->kck_len);

	kalMemCopy(prPairingFsm->prPtk->kdk, key_data->nm_kdk,
		NAN_NM_KDK_MAX_LEN);
	prPairingFsm->prPtk->kdk_len = key_data->nm_kdk_len;
	/* NM-KDK */
	DBGLOG(NAN, DEBUG, "prPairingFsm->prPtk->kdk_len=%zu\n",
		prPairingFsm->prPtk->kdk_len);
	DBGDUMP_HEX(NAN, DEBUG, "prPairingFsm->prPtk->kdk",
		prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);

	kalMemCopy(prPairingFsm->npk, key_data->npk,
		key_data->npk_len);
	prPairingFsm->npk_len = key_data->npk_len;
	/* NPK */
	DBGLOG(NAN, DEBUG, "prPairingFsm->npk_len=%zu\n",
		prPairingFsm->npk_len);
	DBGDUMP_HEX(NAN, DEBUG, "prPairingFsm->npk",
		prPairingFsm->npk, prPairingFsm->npk_len);

	return WLAN_STATUS_SUCCESS;
}

struct PAIRING_FSM_INFO *
pairingFsmAlloc(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "prAdapter NULL\n");
		return NULL;
	}

	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm == NULL) {
			DBGLOG(NAN, ERROR, "Alloc failed\n");
			return NULL;
		}
		if (!prPairingFsm->fgIsInUse) {
			DBGLOG(NAN, INFO, "Alloc PairingFSM %u\n", szIdx);
			prPairingFsm->ucIndex = szIdx;
			kalMemZero(prPairingFsm,
				sizeof(struct PAIRING_FSM_INFO));
			pairingFsmInit(prAdapter, prPairingFsm);

			return prPairingFsm;
		}
	}
	DBGLOG(NAN, ERROR, "No available PairingFSM\n");
	return NULL;
}
void
pairingFsmInit(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	DBGLOG(NAN, INFO, "Enter\n");
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "FSM NULL and return\n");
		return;
	}
	prPairingFsm->fgIsInUse = TRUE;
	prPairingFsm->ePairingState = NAN_PAIRING_INIT;
	prPairingFsm->prPtk = NULL;
	pairingDeriveNirNonce(prPairingFsm);
	pairingDeriveNik(prAdapter, prPairingFsm);
	DBGDUMP_HEX(NAN, DEBUG, "Paring nik",
		prPairingFsm->aucNik, NAN_NIK_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "Paring nmi",
		prAdapter->rDataPathInfo.aucLocalNMIAddr, MAC_ADDR_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "Paring nonce",
		prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
	pairingDeriveNirTag(prAdapter, prPairingFsm->aucNik, NAN_NIK_LEN,
		prAdapter->rDataPathInfo.aucLocalNMIAddr,
		prPairingFsm->aucNonce,
		&prPairingFsm->u8Tag);
	prPairingFsm->fgPeerNIK_received = FALSE;
	prPairingFsm->fgLocalNIK_sent = FALSE;
	prPairingFsm->fgM3Acked = FALSE;
	init_waitqueue_head(&prPairingFsm->confirm_ack_wq);
	prPairingFsm->ucTxKeyRetryCounter = 0;
}
struct PAIRING_FSM_INFO *
pairingFsmSearch(struct ADAPTER *prAdapter, uint8_t *pucPeerAddr)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return NULL;
	}
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm->fgIsInUse &&
			(prPairingFsm->prStaRec != NULL)) {
			if (EQUAL_MAC_ADDR(prPairingFsm->prStaRec->aucMacAddr,
				pucPeerAddr)) {
				DBGLOG(NAN, DEBUG,
					"idx=%zu,addr="MACSTR_A"\n",
					szIdx,
					pucPeerAddr[0], pucPeerAddr[1],
					pucPeerAddr[2], pucPeerAddr[3],
					pucPeerAddr[4], pucPeerAddr[5]);
				return prPairingFsm;
			}
		}
	}
	DBGLOG(NAN, WARN, "Cannot find "MACSTR_A"\n",
		pucPeerAddr[0], pucPeerAddr[1], pucPeerAddr[2],
		pucPeerAddr[3], pucPeerAddr[4], pucPeerAddr[5]);
	return NULL;
}

void
pairingFsmSteps(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			enum NAN_PAIRING_STATE eNextState)
{
	uint8_t ucBssIndex = 0;
	uint8_t *pucLocalNMI = NULL;
	uint8_t *pucPeerNMI = NULL;
	uint8_t pmk[NAN_NPK_LEN] = {0};
	u8 pmkid[16] = {0};
	u8 tag[8] = {0};
	u8 nonce[8] = {0};
	size_t szPairingFsmIdx = 0;
	size_t szIdx = 0;
	enum NAN_PAIRING_STATE eState = 0;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return;
	}
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "FSM NULL and return\n");
		return;
	}
	DBGLOG(NAN, INFO, "Pairing mgmt STATE_%d: [%s] -> [%s]\n",
	prPairingFsm->ucIndex,
	apucDebugParingMgmtState[(uint8_t)prPairingFsm->ePairingState],
	apucDebugParingMgmtState[(uint8_t)eNextState]);

	if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_PAIRING)
		eState = pairingFsmNextState(prPairingFsm);
	else if (prPairingFsm->FsmMode == NAN_PAIRING_FSM_MODE_VERIFICATION)
		eState = pairingFsmNextStateForVerification(prPairingFsm);

	if (eNextState != eState) {
		DBGLOG(NAN, DEBUG, "Steps Error, Go to NAN_PAIRING_INIT\n");
		/* GO TO INIT for error handling */
		eNextState = NAN_PAIRING_INIT;
	}
	pucPeerNMI = prPairingFsm->prStaRec->aucMacAddr;
	DBGLOG(NAN, INFO, "Peer="MACSTR_A"\n",
		pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
		pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);
	switch (eNextState) {
	/* error handling and go init */
	case NAN_PAIRING_INIT:
		/* TODO: reset pairing setting */
		pairingFsmFree(prAdapter, prPairingFsm);
		break;
	/* 1. recevie followup command with bootstrapping info */
	/* 2. receive followup with bootstrapping request */
	case NAN_PAIRING_BOOTSTRAPPING:
	/* TODO: set Followup to carry NPBA and set Bootstrapping_request */
#if 0
	if (prPairingFsm->eBootStrapMethod == NAN_BOOTSTRAPPING_HANDSHAKE_SKIP)
		prPairingFsm->fgIsBootStrapNeedHandShake = FALSE;
	else
		prPairingFsm->fgIsBootStrapNeedHandShake = TRUE;
#endif
		break;
	/* 1. receive bootstrapping password */
	/* 2. receive bootstrapping response */
	case NAN_PAIRING_BOOTSTRAPPING_DONE:
		/* TODO: idle to wait pairing_start commmand */
		/* TODO: derive NIK and NIR-TAG */

		/* station record creation for PMF on 2.4G */
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, BAND_2G4);
		pucLocalNMI = prAdapter->rDataPathInfo.aucLocalNMIAddr;
		/* 2G station record could be created at bootstrapping
		 * while 5G station record needs to be created here
		 */
		if (prPairingFsm->prStaRec == NULL) {
		prPairingFsm->prStaRec =
			cnmStaRecAlloc(prAdapter,
			STA_TYPE_NAN, ucBssIndex, pucPeerNMI);
		}
		if (prPairingFsm->prStaRec) {
			atomic_set(
			&prPairingFsm->prStaRec->NanRefCount,
			1);
#if (CFG_SUPPORT_NAN_11BE_MLO == 1)
			nanMldStaRecRegister(prAdapter,
				prPairingFsm->prStaRec,
				BAND_2G4);
#endif
		}

		/* station record creation for PMF on 5G */
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, BAND_5G);
		prPairingFsm->prStaRec5G =
			cnmStaRecAlloc(prAdapter,
			STA_TYPE_NAN, ucBssIndex, pucPeerNMI);
		if (prPairingFsm->prStaRec5G) {
			atomic_set(
			&prPairingFsm->prStaRec5G->NanRefCount,
			1);
#if (CFG_SUPPORT_NAN_11BE_MLO == 1)
			nanMldStaRecRegister(prAdapter,
				prPairingFsm->prStaRec5G,
				BAND_5G);
#endif
		}

		DBGLOG(NAN, DEBUG, "Local=> "MACSTR_A"\n",
			pucLocalNMI[0], pucLocalNMI[1], pucLocalNMI[2],
			pucLocalNMI[3], pucLocalNMI[4], pucLocalNMI[5]);
		DBGLOG(NAN, DEBUG, "Peer=> "MACSTR_A"\n",
			pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
			pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);
		cnmStaRecChangeState(prAdapter,
			prPairingFsm->prStaRec, STA_STATE_1);
		cnmStaRecChangeState(prAdapter,
			prPairingFsm->prStaRec5G, STA_STATE_1);
		szPairingFsmIdx = 0;
		for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
			if (prPairingFsm ==
				&prAdapter->rWifiVar.arPairingFsmInfo[szIdx]) {
				DBGLOG(NAN, INFO,
					"[pairging-key1] found szPairingFsmIdx=%zu\n",
					szPairingFsmIdx);
				szPairingFsmIdx = szIdx;
				break;
			}
		}
		prPairingFsm->prPtk = &g_ptk[szPairingFsmIdx];
		break;
	/* 1. receive pairing_start command to trigger NAN pairing */
	/* 2. receive PASN_Auth1 */
	case NAN_PAIRING_SETUP:
		/* TODO: trigger to start PASN TRX */
		/* Pairing Initiator: start TX PARN Auth1 */
		/* Pairing Responder: wait for PASN Auth1 */
		prPairingFsm->u4SelCipherType =
			NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
		pairingComposeNanIeForSetup(prAdapter, prPairingFsm);
		break;
	/* 1. get PASN results */
	case NAN_PAIRING_PAIRED:
		/* TODO: key cache */
		prAdapter->rWifiVar.fgNoPmf = FALSE;
		pairingDeriveNdPmk(prAdapter, prPairingFsm);

		kalMemCpyS(tag, NAN_NIR_TAG_LEN,
			&prPairingFsm->u8Tag, NAN_NIR_TAG_LEN);
		DBGDUMP_HEX(NAN, DEBUG, "Paring tag",
			tag, NAN_NIR_TAG_LEN);
		kalMemCpyS(nonce, NAN_NIR_NONCE_LEN,
			prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
		DBGDUMP_HEX(NAN, DEBUG, "Paring nonce",
			nonce, NAN_NIR_NONCE_LEN);
		kalMemCpyS(pmkid, NAN_NIR_NONCE_LEN,
			nonce, NAN_NIR_NONCE_LEN);
		kalMemCpyS(pmkid + NAN_NIR_NONCE_LEN,
			NAN_NIR_TAG_LEN, tag, NAN_NIR_TAG_LEN);
		DBGDUMP_HEX(NAN, DEBUG, "Paring pmkid",
			pmkid, NAN_NPKID_LEN);

		DBGDUMP_HEX(NAN, DEBUG, "[pairing-keyEx] local NIK:",
			prPairingFsm->aucNik, NAN_NIK_LEN);

		DBGLOG(NAN, DEBUG,
			"[pairging-key1] Install NM-TK for peer["MACSTR_A"]\n",
			pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
			pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);
		nanPairingInstallTk(prPairingFsm->prPtk,
			prPairingFsm->u4SelCipherType, prPairingFsm->prStaRec);
		nanPairingInstallTk(prPairingFsm->prPtk,
			prPairingFsm->u4SelCipherType,
			prPairingFsm->prStaRec5G);

		if (prPairingFsm->u2BootstrapMethod !=
		    NAN_BOOTSTRAPPING_OPPORTUNISTIC) {
			prPairingFsm->rNpksaCache.isPmkCached = TRUE;
			kalMemCpyS(prPairingFsm->rNpksaCache.pmkid,
			NAN_NPKID_LEN, pmkid, NAN_NPKID_LEN);
			kalMemCpyS(prPairingFsm->rNpksaCache.pmk,
			NAN_NPK_LEN, pmk, NAN_NPK_LEN);
			prPairingFsm->rNpksaCache.pmk_len = NAN_NPK_LEN;
		}
		pairingPrintKeyInfo(prAdapter, prPairingFsm);
		break;
	/* 1. receive verification command */
	/* 2. receive PASN_Auth1 */
	case NAN_PAIRING_PAIRED_VERIFICATION:
		/* TODO: trigger to start PASN TRX */
		/* Pairing Initiator: start TX PASN Auth1 */
		/* Pairing Responder: wait for PASN Auth1 */
		pairingComposeNanIeForVerification(prAdapter, prPairingFsm);
		break;
	default:
		break;
	}
	prPairingFsm->ePairingState = eNextState;
}
uint8_t
pairingFsmCurrState(struct ADAPTER *prAdapter, uint8_t *pucPeerAddr)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return NAN_PAIRING_INVALID;
	}
	prPairingFsm = pairingFsmSearch(prAdapter, pucPeerAddr);
	if (prPairingFsm)
		return prPairingFsm->ePairingState;
	else
		return NAN_PAIRING_INVALID;
}
uint8_t
pairingFsmNextState(struct PAIRING_FSM_INFO *prPairingFsm)
{
	enum NAN_PAIRING_STATE eNextState = NAN_PAIRING_INIT;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return NAN_PAIRING_INVALID;
	}
	switch (prPairingFsm->ePairingState) {
	case NAN_PAIRING_INIT:
		eNextState = NAN_PAIRING_BOOTSTRAPPING;
		break;
	case NAN_PAIRING_BOOTSTRAPPING:
		eNextState = NAN_PAIRING_BOOTSTRAPPING_DONE;
		break;
	case NAN_PAIRING_BOOTSTRAPPING_DONE:
		eNextState = NAN_PAIRING_SETUP;
		break;
	case NAN_PAIRING_SETUP:
		eNextState = NAN_PAIRING_PAIRED;
		break;
#if 0
	case NAN_PAIRING_PAIRED:
		eNextState = NAN_PAIRING_PAIRED_VERIFICATION;
		break;
	case NAN_PAIRING_PAIRED_VERIFICATION:
		eNextState = NAN_PAIRING_PAIRED;
		break;
#endif
	default:
		eNextState = NAN_PAIRING_INIT;
		break;
	}
	return eNextState;
}

uint8_t
pairingFsmNextStateForVerification(struct PAIRING_FSM_INFO *prPairingFsm)
{
	enum NAN_PAIRING_STATE eNextState = NAN_PAIRING_INIT;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, INFO, "FSM NULL and return\n");
		return NAN_PAIRING_INVALID;
	}
	switch (prPairingFsm->ePairingState) {
	case NAN_PAIRING_INIT:
		eNextState = NAN_PAIRING_BOOTSTRAPPING_DONE;
		break;
	case NAN_PAIRING_BOOTSTRAPPING_DONE:
		eNextState = NAN_PAIRING_PAIRED_VERIFICATION;
		break;
	case NAN_PAIRING_PAIRED_VERIFICATION:
		eNextState = NAN_PAIRING_PAIRED;
		break;
	default:
		eNextState = NAN_PAIRING_INIT;
		break;
	}
	return eNextState;
}
void
pairingFsmSetBootStrappingMethod(struct PAIRING_FSM_INFO *prPairingFsm,
	uint16_t u2BootstrapMethod)
{
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
	prPairingFsm->u2BootstrapMethod = u2BootstrapMethod;
	if (prPairingFsm->u2BootstrapMethod)
		prPairingFsm->fgIsBootStrapNeedHandShake = TRUE;
	else
		prPairingFsm->fgIsBootStrapNeedHandShake = FALSE;
}
void
pairingFsmSetup(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			uint16_t u2BootstrapMethod,
			uint8_t ucCacheEnable, uint8_t ucPubID, uint8_t ucSubID,
			uint8_t ucIsPub, uint16_t u2Ttl)
{
	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return;
	}
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "NULL and return\n");
		return;
	}
	DBGLOG(NAN, INFO, "IsPub=%d, PubId=%d, SubId=%d\n",
		ucIsPub, ucPubID, ucSubID);
	prPairingFsm->fgCachingEnable = ucCacheEnable;
	prPairingFsm->ucPublishID = ucPubID;
	prPairingFsm->ucSubscribeID = ucSubID;
	prPairingFsm->ucIsPub = ucIsPub;
	pairingFsmSetBootStrappingMethod(prPairingFsm, u2BootstrapMethod);
	prPairingFsm->u2Ttl = u2Ttl;
	if (prPairingFsm->u2Ttl > 0) {
		cnmTimerStopTimer(prAdapter,
			&prPairingFsm->rTtlTimer);
		/*ToDo:Init Timer to check get
		 * Auth Txdone avoid sta_rec not clear
		 */
		cnmTimerInitTimer(prAdapter,
			&prPairingFsm->rTtlTimer,
			(PFN_MGMT_TIMEOUT_FUNC)
			pairingServiceLifetimeout,
			(unsigned long) prPairingFsm);
		cnmTimerStartTimer(prAdapter,
			&prPairingFsm->rTtlTimer,
			prPairingFsm->u2Ttl);
	}
}
void
pairingFsmFree(struct ADAPTER *prAdapter,
	       struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prAdapter == NULL) {
		DBGLOG(NAN, ERROR, "Adapter NULL\n");
		return;
	}
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "NULL and return\n");
		return;
	}
	DBGLOG(NAN, INFO, "Setup, IDX_%d\n", prPairingFsm->ucIndex);
	if (prPairingFsm->fgIsInUse) {
		prPairingFsm->fgIsInUse = FALSE;
		cnmStaRecFree(prAdapter, prPairingFsm->prStaRec);
		cnmStaRecFree(prAdapter, prPairingFsm->prStaRec5G);
		prPairingFsm->prStaRec = NULL;
		prPairingFsm->prStaRec5G = NULL;
		prPairingFsm->ucPublishID = 0;
		prPairingFsm->ucSubscribeID = 0;
		prPairingFsm->ePairingState = NAN_PAIRING_INIT;
		prPairingFsm->fgCachingEnable = FALSE;
		prPairingFsm->prPtk = NULL;
		prPairingFsm->wpa_passphrase = NULL;
		cnmTimerStopTimer(prAdapter, &prPairingFsm->rTtlTimer);
		cnmTimerStopTimer(prAdapter, &prPairingFsm->rNikTimer);
	}
}
void
pairingFsmUninit(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	DBGLOG(NAN, INFO, "Enter\n");
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm->fgIsInUse)
			pairingFsmFree(prAdapter, prPairingFsm);
	}
}
void
pairingFsmCancelRequest(struct ADAPTER *prAdapter,
			uint8_t ucInstanceId, uint8_t ucIsPub)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return;
	}
	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm->fgIsInUse) {
			if (ucIsPub) {
				if (prPairingFsm->ucIsPub &&
					prPairingFsm->ucPublishID ==
					ucInstanceId) {
					DBGLOG(NAN, INFO,
						"PairingFSM %zu cancel, Pub=1, InsId=%d\n",
						szIdx,
						ucInstanceId);
					pairingFsmFree(prAdapter, prPairingFsm);
				}
			} else {
				if (!prPairingFsm->ucIsPub &&
					prPairingFsm->ucSubscribeID ==
					ucInstanceId) {
					DBGLOG(NAN, INFO,
						"PairingFSM %zu cancel, Sub=1, InsId=%d\n",
						szIdx,
						ucInstanceId);
					pairingFsmFree(prAdapter, prPairingFsm);
				}
			}
		}
	}
}
uint32_t
nanCmdPairingSetupReq(struct ADAPTER *prAdapter,
			struct NanTransmitFollowupRequest *msg)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	if (!prAdapter) {
		DBGLOG(NAN, ERROR, "prAdapter error\n");
		return WLAN_STATUS_INVALID_DATA;
	}
	prPairingFsm = pairingFsmSearch(prAdapter, msg->addr);
	DBGDUMP_HEX(NAN, DEBUG, "NAN_PAIRING_ADDR", msg->addr, 6);
	if (prPairingFsm) {
		DBGLOG(NAN, INFO,
			"pairing_state = %u, verification=%u, pairing_type=%u\n",
			prPairingFsm->ePairingState,
			msg->pairing_verification,
			msg->pairing_type);
		/*very important*/
		prPairingFsm->ucPairingType = msg->pairing_type;
		if (prPairingFsm->ePairingState ==
		    NAN_PAIRING_BOOTSTRAPPING_DONE) {
			/* Trigger pairing setup */
			DBGLOG(NAN, INFO, "Trigger pairing setup\n");
			pairingFsmSteps(prAdapter,
			prPairingFsm, NAN_PAIRING_SETUP);
			pairingNotifyPasn(prAdapter, prPairingFsm);
		} else if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED &&
			msg->pairing_verification == TRUE &&
			(msg->pairing_type == NAN_PAIRING_RESPONDER ||
			prPairingFsm->fgIsPaired)) {
			/* Trigger pairing verification */
			DBGLOG(NAN, INFO, "Trigger pairing verification\n");
			pairingFsmSteps(prAdapter,
			prPairingFsm, NAN_PAIRING_PAIRED_VERIFICATION);
			pairingNotifyPasn(prAdapter, prPairingFsm);
		} else {
			DBGLOG(NAN, ERROR, "INVALID state\n");
			pairingFsmSteps(prAdapter,
				prPairingFsm, NAN_PAIRING_INVALID);
		}
	}
	return WLAN_STATUS_SUCCESS;
}
uint32_t
nanCmdBootstrapPwdSetup(struct ADAPTER *prAdapter,
			struct NanBootstrapPassword *msg)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

#if 0
	prPairingFsm = pairingFsmSearchByServiceName(prAdapter,
		msg->service_name, msg->service_name_len);
#else
	/* FIXME: Should find PairingFSM by service name */
	prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[0];
#endif
	if (prPairingFsm) {
		if (prPairingFsm->ePairingState ==
		    NAN_PAIRING_BOOTSTRAPPING_DONE) {
			/* TODO:
			* forward password and bootstrap method to
			* PASN for SAE
			*/
			prPairingFsm->wpa_passphrase = (char *)msg->password;
			DBGDUMP_HEX(NAN, DEBUG,
				"NAN BOOTSTRAP PASSEWORD",
				msg->password, msg->password_len);
			DBGLOG(NAN, DEBUG, "wpa_passphrase=%s\n",
				prPairingFsm->wpa_passphrase);
		}
	}
	return WLAN_STATUS_SUCCESS;
}

void nanPairingInstallLocalNik(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	u8 *nan_identity_key)
{
	memcpy(prPairingFsm->aucNik, nan_identity_key,
		NAN_NIK_LEN);
}

void nanPairingInstallLocalIGtk(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm, u8 *igtk)
{
	memcpy(prPairingFsm->aucIGtk, igtk,
		NAN_IGTK_LEN);
}

void nanPairingInstallLocalBIGtk(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm, u8 *bigtk)
{
	memcpy(prPairingFsm->aucBIGtk, bigtk,
		NAN_BIGTK_LEN);
}

void
nanPairingInstallTk(struct wpa_ptk *prPtk,
		uint32_t u4SelCipherType,
		struct STA_RECORD *prStaRec)
{
	uint8_t *pu1Tk = NULL;
	size_t szTkLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Cipher = 0;
	enum wpa_alg alg = WPA_ALG_NONE;

	DBGLOG(NAN, INFO,
		"Enter, StaIdx:%d, BssIdx:%d, Cipher=%x, WlanIdx=%d\n",
		prStaRec->ucIndex, prStaRec->ucBssIndex,
		u4SelCipherType, prStaRec->ucWlanIndex);
	prStaRec->rPmfCfg.fgApplyPmf = TRUE;
	pu1Tk = prPtk->tk;
	szTkLen = prPtk->tk_len;
	u4Cipher = u4SelCipherType;
	dumpMemory8(pu1Tk, szTkLen);
	/* TODO_CJ: dynamic chiper, dynamic keyID */
	if (u4Cipher == NAN_CIPHER_SUITE_ID_NCS_PK_PASN_256)
		alg = WPA_ALG_GCMP_256;
	else
		alg = WPA_ALG_CCMP;
	rStatus = nan_sec_wpas_setkey_glue(FALSE, prStaRec->ucBssIndex, alg,
					   prStaRec->aucMacAddr, 0, pu1Tk,
					   szTkLen);
	prPtk->installed = 1;
}
void
pairingComposeNanIeForSetup(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;
	struct _NAN_IE_HDR_T *prNanIeHdr = NULL;
	struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *prAttrDCEA = NULL;
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T *prAttrCSIA = NULL;
	struct _NAN_ATTR_NPBA_T *prAttrNPBA = NULL;
	struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCsid = NULL;
	struct _NAN_ATTR_EXT_CAPABILITIES_T *prExtCap;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "NULL and return\n");
		return;
	}
	/* TODO: compose DCEA, CSIA, NPBA for Pairing Setup */
	/* NAN IE Hdr(len=6) */
	prPairingFsm->nan_ie_len = 0;
	kalMemZero(prPairingFsm->nan_ie, NAN_PASN_IE_LEN);
	prNanIeHdr = (struct _NAN_IE_HDR_T *)(prPairingFsm->nan_ie);
	prNanIeHdr->ucElementId = 0xDD;
	if (prPairingFsm->ucPairingType == NAN_PAIRING_RESPONDER)
		prNanIeHdr->ucTagLen = 25; /* 21(5+8+8) + 4*/
	else
		prNanIeHdr->ucTagLen = 23; /* 19(5+6+8) + 4*/

	kalMemCpyS(prNanIeHdr->aucOUI, VENDOR_OUI_LEN, aucOui, VENDOR_OUI_LEN);
	prNanIeHdr->ucOUItype = VENDOR_OUI_TYPE_NAN_SDF;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_IE_HDR_T);
	/* DCEA(len=5): Device Capability Extension Attr */
	prAttrDCEA = (struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrDCEA->ucAttrId = NAN_ATTR_ID_DEVICE_CAPABILITY_EXT;
	prAttrDCEA->u2Length = 2;
	prExtCap =
	(struct _NAN_ATTR_EXT_CAPABILITIES_T *)prAttrDCEA->aucExtCapabilities;
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_PARING_SETUP_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_NPK_NIK_CACHE_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);

	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T);
	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_EXT_CAPABILITIES_T);

	/* CSIA(len=8): Cipher Suite Info Attr */
	prAttrCSIA = (struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrCSIA->ucAttrId = NAN_ATTR_ID_CIPHER_SUITE_INFO;
	prAttrCSIA->u2Length = 3;
	prAttrCSIA->ucCapabilities = 0;
	if (prPairingFsm->ucPairingType == NAN_PAIRING_RESPONDER) {
		prAttrCSIA->u2Length += 2;
		prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
					(prAttrCSIA->aucCipherSuiteList);
		prCsid->ucPublishID = prPairingFsm->ucPublishID;
		prCsid->ucCipherSuiteID = 1;
		prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
			(prAttrCSIA->aucCipherSuiteList +
			sizeof(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T));
	} else
		prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
					(prAttrCSIA->aucCipherSuiteList);

	prCsid->ucPublishID = prPairingFsm->ucPublishID;
	prCsid->ucCipherSuiteID = NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
	/* 1+2+1+ 2x1(id:7) */
	prPairingFsm->nan_ie_len += (NAN_ATTR_HDR_LEN + prAttrCSIA->u2Length);

	/* NPBA(len=8): Nan Pairing Bootstrapping Attr */
	prAttrNPBA = (struct _NAN_ATTR_NPBA_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrNPBA->ucAttribID = NAN_ATTR_ID_NAN_PAIRING_BOOTSTRAPPING;
	prAttrNPBA->u2Len = 5;
	prAttrNPBA->ucDialogTok = prPairingFsm->ucDialogToken;
	if (prPairingFsm->ucPairingType == NAN_PAIRING_RESPONDER)
		prAttrNPBA->ucTypeStatus = NAN_BOOTSTRAPPING_TYPE_RESPONSE;
	else
		prAttrNPBA->ucTypeStatus = NAN_BOOTSTRAPPING_TYPE_REQUEST;
	prAttrNPBA->ucReasonCode = 0;
	prAttrNPBA->u2BootstapMethod = prPairingFsm->u2BootstrapMethod;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_ATTR_NPBA_T);

	DBGLOG(NAN, INFO, "NanIeLen=%zu\n", prPairingFsm->nan_ie_len);
	DBGDUMP_HEX(NAN, DEBUG, "NAN IE Setup",
		prPairingFsm->nan_ie, prPairingFsm->nan_ie_len);
}
void
pairingComposeNanIeForVerification(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	const uint8_t aucOui[VENDOR_OUI_LEN] = NAN_OUI;
	struct _NAN_IE_HDR_T *prNanIeHdr = NULL;
	struct _NAN_ATTR_NIRA_T *prAttrNIRA = NULL;
	struct _NAN_ATTR_CIPHER_SUITE_INFO_T *prAttrCSIA = NULL;
	struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCsid = NULL;
	struct _NAN_ATTR_EXT_CAPABILITIES_T *prExtCap;
	struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *prAttrDCEA = NULL;

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "NULL and return\n");
		return;
	}
	/* TODO: compose CSIA, NIRA for verification */
	/* NAN IE Hdr(len=6) */
	prPairingFsm->nan_ie_len = 0;
	kalMemZero(prPairingFsm->nan_ie, NAN_PASN_IE_LEN);
	prNanIeHdr = (struct _NAN_IE_HDR_T *)(prPairingFsm->nan_ie);
	prNanIeHdr->ucElementId = 0xDD;
	if (prPairingFsm->ucPairingType == NAN_PAIRING_RESPONDER)
		prNanIeHdr->ucTagLen = 37; /* 33(5+8+20) + 4*/
	else
		prNanIeHdr->ucTagLen = 35; /* 31(5+6+20) + 4*/

	kalMemCpyS(prNanIeHdr->aucOUI, VENDOR_OUI_LEN, aucOui, VENDOR_OUI_LEN);
	prNanIeHdr->ucOUItype = VENDOR_OUI_TYPE_NAN_SDF;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_IE_HDR_T);

	/* DCEA(len=5): Device Capability Extension Attr */
	prAttrDCEA = (struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrDCEA->ucAttrId = NAN_ATTR_ID_DEVICE_CAPABILITY_EXT;
	prAttrDCEA->u2Length = 2;
	prExtCap =
	(struct _NAN_ATTR_EXT_CAPABILITIES_T *)prAttrDCEA->aucExtCapabilities;
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_PARING_SETUP_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);
	prExtCap->ucSettings |=
	(NAN_DEVICE_CAPABILITY_EXT_NPK_NIK_CACHE_EN >>
	NAN_DEVICE_CAPABILITY_EXT_SETTING_OFFSET);

	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T);
	prPairingFsm->nan_ie_len +=
	sizeof(struct _NAN_ATTR_EXT_CAPABILITIES_T);

	/* CSIA(len=8) */
	prAttrCSIA = (struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrCSIA->ucAttrId = NAN_ATTR_ID_CIPHER_SUITE_INFO;
	prAttrCSIA->u2Length = 3;
	prAttrCSIA->ucCapabilities = 0;
	if (prPairingFsm->ucPairingType == NAN_PAIRING_RESPONDER) {
		prAttrCSIA->u2Length += 2;
		prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
					(prAttrCSIA->aucCipherSuiteList);
		prCsid->ucPublishID = prPairingFsm->ucPublishID;
		prCsid->ucCipherSuiteID = 1;
		prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
			(prAttrCSIA->aucCipherSuiteList +
			sizeof(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T));
	} else
		prCsid = (struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
					(prAttrCSIA->aucCipherSuiteList);

	prCsid->ucPublishID = prPairingFsm->ucPublishID;
	prCsid->ucCipherSuiteID = NAN_CIPHER_SUITE_ID_NCS_PK_PASN_128;
	/* 1+2+1+ 2x1(id:7) */
	prPairingFsm->nan_ie_len += (NAN_ATTR_HDR_LEN + prAttrCSIA->u2Length);

	/* NIRA(len=20) */
	prAttrNIRA = (struct _NAN_ATTR_NIRA_T *)
		(prPairingFsm->nan_ie + prPairingFsm->nan_ie_len);
	prAttrNIRA->ucAttribID = NAN_ATTR_ID_NAN_IDENTITY_RESOLUTION;
	prAttrNIRA->u2Len = 17;
	prAttrNIRA->ucCipherVer = 0;
	kalMemCpyS(&prAttrNIRA->u8Nonce,
		NAN_NIR_NONCE_LEN,
		prPairingFsm->aucNonce,
		NAN_NIR_NONCE_LEN);
	prAttrNIRA->u8Tag = prPairingFsm->u8Tag;
	prPairingFsm->nan_ie_len += sizeof(struct _NAN_ATTR_NIRA_T);

	DBGLOG(NAN, INFO, "NanIeLen=%zu\n", prPairingFsm->nan_ie_len);
	DBGDUMP_HEX(NAN, DEBUG, "NAN IE Verification",
		prPairingFsm->nan_ie, prPairingFsm->nan_ie_len);
}
void
pairingNotifyPasn(struct ADAPTER *prAdapter,
		struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "NULL and return\n");
		return;
	}
	DBGLOG(NAN, INFO, "go to PASN\n");
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief PASN Auth frame TX Wrapper Function
 *
 * \param[in]
 *
 * \return Status
 */
/*----------------------------------------------------------------------------*/
uint32_t
pairingTxPasnAuthFrame(struct ADAPTER *prAdapter,
	struct MSDU_INFO *prMsduInfo, uint16_t u2FrameLength,
	PFN_TX_DONE_HANDLER pfTxDoneHandler,
	struct STA_RECORD *prSelectStaRec)
{
	/* 4 <3> Update information of MSDU_INFO_T */
	TX_SET_MMPDU(
		prAdapter, prMsduInfo,
		(prSelectStaRec != NULL)
			? (prSelectStaRec->ucBssIndex)
			: nanGetSpecificBssInfo(prAdapter, NAN_BSS_INDEX_MAIN)
				  ->ucBssIndex,
		(prSelectStaRec != NULL) ? (prSelectStaRec->ucIndex)
					 : (STA_REC_INDEX_NOT_FOUND),
		OFFSET_OF(struct _NAN_ACTION_FRAME_T, ucCategory),
		u2FrameLength, pfTxDoneHandler, MSDU_RATE_MODE_AUTO);
	prMsduInfo->ucTxToNafQueFlag = TRUE;
	if (!prAdapter->rWifiVar.fgNoPmf && (prSelectStaRec != NULL) &&
		(prSelectStaRec->rPmfCfg.fgApplyPmf == TRUE)) {
		nicTxConfigPktOption(prMsduInfo, MSDU_OPT_PROTECTED_FRAME,
			TRUE);
		DBGLOG(NAN, DEBUG,
			"StaIdx:%d, MAC=>"MACSTR_A"\n",
			prSelectStaRec->ucIndex,
			prSelectStaRec->aucMacAddr[0],
			prSelectStaRec->aucMacAddr[1],
			prSelectStaRec->aucMacAddr[2],
			prSelectStaRec->aucMacAddr[3],
			prSelectStaRec->aucMacAddr[4],
			prSelectStaRec->aucMacAddr[5]);
	}
	/* 4 <6> Enqueue the frame to send this NAF frame. */
	nicTxEnqueueMsdu(prAdapter, prMsduInfo);
	return WLAN_STATUS_SUCCESS;
}
uint32_t
pairingRxPasnAuthFrame(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb)
{
	/* TODO: add interface for Rx PASN auth frame */
	return WLAN_STATUS_SUCCESS;
}
uint8_t
pairingFsmPairedOrNot(struct ADAPTER *prAdapter,
		      struct NAN_DISCOVERY_EVENT *prDiscEvt)
{
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	uint8_t fgIsPaired = FALSE;

	DBGLOG(NAN, INFO, "tag=%llu, nonce=%llu\n",
		prDiscEvt->u8NiraTag, prDiscEvt->u8NiraNonce);
	prPairingFsm = pairingFsmSearch(prAdapter, prDiscEvt->aucNanAddress);
	if (prPairingFsm) {
		/* Check service paired or not */
		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
			if (prPairingFsm->u8PeerTag == prDiscEvt->u8NiraTag) {
				DBGLOG(NAN, DEBUG,
					"PairingFsm%d is paired,addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
					prPairingFsm->ucIndex,
					prDiscEvt->aucNanAddress[0],
					prDiscEvt->aucNanAddress[1],
					prDiscEvt->aucNanAddress[2],
					prDiscEvt->aucNanAddress[3],
					prDiscEvt->aucNanAddress[4],
					prDiscEvt->aucNanAddress[5]);
				fgIsPaired = TRUE;
			}
		}
		prPairingFsm->fgIsPaired = fgIsPaired;
	}
	return fgIsPaired;
}
/**
 * rsn_pmkid_suite_b - Calculate PMK identifier for Suite B AKM
 * @kck: Key confirmation key
 * @kck_len: Length of kck in bytes
 * @aa: Authenticator address
 * @spa: Supplicant address
 * @pmkid: Buffer for PMKID
 * Returns: 0 on success, -1 on failure
 *
 * IEEE Std 802.11ac-2013 - 11.6.1.3 Pairwise key hierarchy
 * NIR-TAG = Truncate-64(HMAC-SHA-256(NIK, "NIR" || AA(NMI) || SPA(nonce)))
 */
void
pairingDeriveNirTag(struct ADAPTER *prAdapter, const u8 *nik,
	size_t nik_len, const u8 *aa,
	const u8 *spa, u64 *tag)
{
	/* aa = NMI, spa = nonce */
	char *title = "NIR";
	const u8 *addr[3] = {NULL, NULL, NULL};
	const size_t len[3] = { 3, ETH_ALEN, NAN_NIR_TAG_LEN };
	unsigned char hash[SHA256_MAC_LEN] = {0};

	addr[0] = (u8 *)title;
	addr[1] = aa;
	addr[2] = spa;
	if (hmac_sha256_vector(nik, nik_len, 3, addr, len, hash) < 0)
		DBGLOG(NAN, ERROR, "NIR-TAG derivation failed\n");
	/* TAG=64bit, len = 8byte */
	kalMemCpyS(tag, NAN_NIR_TAG_LEN, hash, NAN_NIR_TAG_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "hash", hash, SHA256_MAC_LEN);
}
void
pairingDeriveNdPmk(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	u8 *myaddr = NULL;
	u8 *peer = NULL;
	u8 context[2 * MAC_ADDR_LEN] = {0}, *ptr = context;

	/*
	 * AEK = KDF-Hash-256(PMK, "AEK Derivation", Selected AKM Suite ||
	 *       min(localMAC, peerMAC) || max(localMAC, peerMAC))
	 */
	/* Selected AKM Suite: SAE */
	myaddr = prAdapter->rDataPathInfo.aucLocalNMIAddr;
	peer = prPairingFsm->prStaRec->aucMacAddr;
	DBGLOG(NAN, INFO, "pairing_type=%d\n", prPairingFsm->ucPairingType);
	if (prPairingFsm->ucPairingType == NAN_PAIRING_REQUESTOR) {
		kalMemCpyS(ptr, MAC_ADDR_LEN, myaddr, MAC_ADDR_LEN);
		ptr += MAC_ADDR_LEN;
		kalMemCpyS(ptr, MAC_ADDR_LEN, peer, MAC_ADDR_LEN);
	} else { /* NAN_PAIRING_RESPONDER */
		kalMemCpyS(ptr, MAC_ADDR_LEN, peer, MAC_ADDR_LEN);
		ptr += MAC_ADDR_LEN;
		kalMemCpyS(ptr, MAC_ADDR_LEN, myaddr, MAC_ADDR_LEN);
	}
	if (prPairingFsm->prPtk == NULL) {
		DBGLOG(NAN, ERROR, "null ptk and return\n");
		return;
	}
	if (prPairingFsm->prPtk->kdk_len != WPA_KDK_MAX_LEN) {
		DBGLOG(NAN, ERROR, "NM-KDK should be 256 bits always\n");
		return;
	}
	DBGLOG(NAN, INFO, "pmk size=%zu, context size=%zu, kdk_len=%zu\n",
		sizeof(prPairingFsm->key_info.body.pmk_info.pmk),
		sizeof(context),
		prPairingFsm->prPtk->kdk_len);
	DBGDUMP_HEX(NAN, DEBUG, "context", context, sizeof(context));
	DBGDUMP_HEX(NAN, DEBUG, "NM-KDK",
		prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);
	sha256_prf(prPairingFsm->prPtk->kdk,
		prPairingFsm->prPtk->kdk_len,
		"NDP PMK Derivation",
		context, (MAC_ADDR_LEN + MAC_ADDR_LEN),
		prPairingFsm->key_info.body.pmk_info.pmk,
		sizeof(prPairingFsm->key_info.body.pmk_info.pmk));
	DBGDUMP_HEX(NAN, DEBUG, "ND-PMK:",
		prPairingFsm->key_info.body.pmk_info.pmk,
		sizeof(prPairingFsm->key_info.body.pmk_info.pmk));
}
void
pairingDeriveNik(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
#if 0
	random_get_bytes(prPairingFsm->aucGmk, WPA_GMK_LEN);
#endif
}
void
pairingDeriveNirNonce(struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
	random_get_bytes(prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
}
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
void
pairingComposeIGtkKde(struct IGTK_KDE_INFO *prIGtkKde,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	uint8_t aucIgtk[32] = {0};
	size_t szIgtkLen = 0;
	uint32_t u4IgtkCipher = 0;
	uint8_t aucBigtk[32] = {0};
	size_t szBigtkLen = 0;
	uint32_t u4BigtkCipher = 0;

	nanSecGetTxIgtkBigtk(
		&u4IgtkCipher, aucIgtk, &szIgtkLen,
		&u4BigtkCipher, aucBigtk, &szBigtkLen);

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}

	kalMemZero((void *)prIGtkKde, sizeof(struct IGTK_KDE_INFO));
	prIGtkKde->type = 0xdd;
	prIGtkKde->length = 28;
	prIGtkKde->oui[0] = 0x00;
	prIGtkKde->oui[1] = 0x0F;
	prIGtkKde->oui[2] = 0xAC;
	prIGtkKde->data_type = NAN_KDE_IGTK;
	prIGtkKde->key_id[0] = 4;
	kalMemCpyS(prIGtkKde->IPN, NAN_PACKET_NUMBER_LEN,
		g_aucNanIgtkPn, NAN_PACKET_NUMBER_LEN);
	kalMemCpyS(prIGtkKde->igtk,
		NAN_IGTK_LEN, aucIgtk, NAN_IGTK_LEN);
}
void
pairingComposeBIGtkKde(struct BIGTK_KDE_INFO *prBIGtkKde,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	uint8_t aucIgtk[32] = {0};
	size_t szIgtkLen = 0;
	uint32_t u4IgtkCipher = 0;
	uint8_t aucBigtk[32] = {0};
	size_t szBigtkLen = 0;
	uint32_t u4BigtkCipher = 0;

	nanSecGetTxIgtkBigtk(
		&u4IgtkCipher, aucIgtk, &szIgtkLen,
		&u4BigtkCipher, aucBigtk, &szBigtkLen);

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}

	kalMemZero((void *)prBIGtkKde, sizeof(struct BIGTK_KDE_INFO));
	prBIGtkKde->type = 0xdd;
	prBIGtkKde->length = 28;
	prBIGtkKde->oui[0] = 0x00;
	prBIGtkKde->oui[1] = 0x0F;
	prBIGtkKde->oui[2] = 0xAC;
	prBIGtkKde->data_type = NAN_KDE_BIGTK;
	kalMemCpyS(prBIGtkKde->BIPN, NAN_PACKET_NUMBER_LEN,
		g_aucNanBigtkPn, NAN_PACKET_NUMBER_LEN);
	kalMemCpyS(prBIGtkKde->bigtk,
		NAN_BIGTK_LEN, aucBigtk, NAN_BIGTK_LEN);
}
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
void
pairingComposeNikKde(struct NIK_KDE_INFO *prNikKde,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
	prNikKde->type = 0xdd;
	prNikKde->length = 21;
	prNikKde->oui[0] = 0x50;
	prNikKde->oui[1] = 0x6F;
	prNikKde->oui[2] = 0x9A;
	prNikKde->data_type = NAN_KDE_NIK;
	prNikKde->cipher_ver = 0;
	kalMemCpyS(prNikKde->nik,
		NAN_NIK_LEN, prPairingFsm->aucNik, NAN_NIK_LEN);
}
void
pairingComposeNikLifetimeKde(struct ADAPTER *prAdapter,
			struct NIK_LIFETIME_KDE_INFO *prNikLifetimeKde,
			struct PAIRING_FSM_INFO *prPairingFsm)
{
	if (!prAdapter || !prPairingFsm) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
	prNikLifetimeKde->type = 0xdd;
	prNikLifetimeKde->length = 10;
	prNikLifetimeKde->oui[0] = 0x50;
	prNikLifetimeKde->oui[1] = 0x6F;
	prNikLifetimeKde->oui[2] = 0x9A;
	prNikLifetimeKde->data_type = NAN_KDE_KEY_LIFETIME;
	prNikLifetimeKde->key_bitmap = 0x08;
	/* Lifetime uint: sec, set as 15 min */
	prNikLifetimeKde->lifetime = prAdapter->rWifiVar.u4NanNikLifetime;
}

void pairingParseKDE(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	uint8_t const *key_data, const uint16_t key_data_len) {
	uint16_t offset = 0;

	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "prPairingFsm NULL\n");
		return;
	}

	while (offset + 2 <= key_data_len) {
		uint8_t type = key_data[offset];
		uint8_t length = key_data[offset + 1];
		uint16_t kde_total_len = 2 + length;

		if (offset + kde_total_len > key_data_len) {
			DBGLOG(NAN, ERROR,
				"KDE data truncated or invalid at offset %u\n",
				offset);
			break;
		}

		if (type != 0xdd) {
			if (length == 0) {
				DBGLOG(NAN, ERROR,
					"aes wrap padding buffer found(offset:%u)\n",
					offset);
			} else {
				DBGLOG(NAN, ERROR,
					"unknown KDE Structure offset:%u,key_data_len:%u)\n",
					offset, key_data_len);
			}
			break;
		}

		if (length < 4) {
			DBGLOG(NAN, ERROR,
				"KDE length too short at offset %u\n", offset);
			offset += kde_total_len;
			continue;
		}

		uint8_t data_type = key_data[offset + 5];

		switch (data_type) {
		case NAN_KDE_DATA_TYPE_IGTK: {
			struct IGTK_KDE_INFO *igtk_info =
				(struct IGTK_KDE_INFO *)&key_data[offset];
			DBGLOG(NAN, INFO,
				"IGTK KDE detected at offset %u, key_id %u\n",
				offset,
				igtk_info->key_id[0]);
			DBGDUMP_HEX(NAN, DEBUG, "IGTK",
				(uint8_t *)igtk_info->igtk, NAN_IGTK_LEN);
			prPairingFsm->fgPeerIgtk = TRUE;
			kalMemCpyS(prPairingFsm->aucPeerIGtk, NAN_IGTK_LEN,
				igtk_info->igtk, NAN_IGTK_LEN);
			break;
		}
		case NAN_KDE_DATA_TYPE_BIGTK: {
			struct BIGTK_KDE_INFO *bigtk_info =
				(struct BIGTK_KDE_INFO *)&key_data[offset];
			DBGLOG(NAN, INFO,
				"BIGTK KDE detected at offset %u, key_id: %u\n",
				offset,
				bigtk_info->key_id[0]);
			DBGDUMP_HEX(NAN, DEBUG, "BIGTK",
				(uint8_t *)bigtk_info->bigtk, NAN_BIGTK_LEN);
			prPairingFsm->fgPeerBigtk = TRUE;
			kalMemCpyS(prPairingFsm->aucPeerBIGtk, NAN_BIGTK_LEN,
				bigtk_info->bigtk, NAN_BIGTK_LEN);
			break;
		}
		case NAN_KDE_DATA_TYPE_NIK: {
			struct NIK_KDE_INFO *nik_info =
				(struct NIK_KDE_INFO *)&key_data[offset];
			DBGLOG(NAN, INFO,
				"NIK KDE detected at offset %u, cipher_ver: %u\n",
				offset,
				nik_info->cipher_ver);
			DBGDUMP_HEX(NAN, DEBUG, "NIK",
				(uint8_t *)nik_info->nik, NAN_NIK_LEN);
			kalMemCpyS(prPairingFsm->aucPeerNik, NAN_NIK_LEN,
				nik_info->nik, NAN_NIK_LEN);
			break;
		}
		case NAN_KDE_DATA_TYPE_NIKLIFETIME: {
		struct NIK_LIFETIME_KDE_INFO *niklifetime_info =
			(struct NIK_LIFETIME_KDE_INFO *)&key_data[offset];
			DBGLOG(NAN, INFO,
				"NIK lifetime KDE detected at offset %u, key_bitmap: %hu, lifetime(sec): %u\n",
				offset,
				niklifetime_info->key_bitmap,
				niklifetime_info->lifetime);
			break;
		}
		default:
		DBGLOG(NAN, INFO, "Unknown KDE type: 0x%02X at offset %u\n",
			data_type, offset);
		break;
		}
		offset += kde_total_len;
	}
}

void
pairingDecryptSKDA(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			uint8_t *pucKey, uint16_t u2KeyLen)
{
	struct wpa_sm *sm = NULL;
	struct wpa_eapol_key *key = NULL;
	uint8_t *key_data = NULL;
	uint16_t ver = 0, key_info = 0;
	size_t key_data_len = 0;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	uint8_t aucBCAddr[] = BC_MAC_ADDR;
	uint8_t *pucPeerAddr = NULL;
	uint16_t u2WtblIdx = WTBL_RESERVED_ENTRY;
	uint8_t fgWtblReUsed = FALSE;
	uint8_t ucKeyStatus = NO_KEY_EXIST_IND;
	struct BSS_INFO *prNanBssInfo = NULL;
	struct _NAN_SPECIFIC_BSS_INFO_T *prNanSpecInfo =
		(struct _NAN_SPECIFIC_BSS_INFO_T *)NULL;
	uint32_t i = 0;
#endif /* (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1) */

	if (prPairingFsm == NULL) {
		DBGLOG(NAN, ERROR, "FSM null\n");
		return;
	}
	if (pucKey == NULL) {
		DBGLOG(NAN, ERROR, "Key null\n");
		return;
	}

	prPairingFsm->u2Skda2KeyLength = u2KeyLen;

	kalMemCpyS(prPairingFsm->aucSkdaKeyData,
		NAN_KDE_ATTR_BUF_SIZE, pucKey, u2KeyLen);

	key = (struct wpa_eapol_key *)prPairingFsm->aucSkdaKeyData;
	key_data = (uint8_t *)(key + 1);
	key_info = WPA_GET_BE16(key->key_info);
	ver = key_info & WPA_KEY_INFO_TYPE_MASK;

	key_data_len = WPA_GET_BE16(key->key_data_length);
	if (key_data_len > NAN_KDE_ATTR_BUF_SIZE)
		return;

	DBGDUMP_HEX(NAN, DEBUG, "SKDA",
		(uint8_t *)prPairingFsm->aucSkdaKeyData, NAN_KDE_ATTR_BUF_SIZE);
	DBGLOG(NAN, INFO, "go set responder sm\n");
	sm = nanSecGetPairingResponderSm(prPairingFsm->ucIndex);
	/* sm init */
	sm->renew_snonce = 1;
	sm->ctx = &g_rNanWpaPairingSmCtx;
	sm->dot11RSNAConfigPMKLifetime = 43200;
	sm->dot11RSNAConfigPMKReauthThreshold = 70;
	sm->dot11RSNAConfigSATimeout = 60;
	kalMemCpyS(&sm->ptk,
		sizeof(struct wpa_ptk),
		prPairingFsm->prPtk,
		sizeof(struct wpa_ptk));
	sm->ptk_set = 1;
	DBGLOG(NAN, INFO, "kek_len=%zu, kck_len=%zu, tk_len=%zu\n",
		sm->ptk.kek_len, sm->ptk.kck_len, sm->ptk.tk_len);
	DBGLOG(NAN, INFO, "P_FSM kek_len=%zu, kck_len=%zu, tk_len=%zu\n",
		prPairingFsm->prPtk->kek_len, prPairingFsm->prPtk->kck_len,
		prPairingFsm->prPtk->tk_len);
	DBGLOG(NAN, INFO, "ver=0x%x, key_info=0x%x, key_data_len=%zu\n",
		ver, key_info, key_data_len);

	DBGLOG(NAN, INFO, "go wpa_supplicant_decrypt_key_data\n");
	ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	wpa_supplicant_decrypt_key_data(sm, key, ver, key_data, &key_data_len);
	DBGDUMP_HEX(NAN, DEBUG, "Key_data", key_data, key_data_len);
	pairingParseKDE(prAdapter, prPairingFsm, key_data, key_data_len);

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	if (prPairingFsm->fgPeerIgtk == FALSE)
		return;

	pucPeerAddr = prPairingFsm->prStaRec->aucMacAddr;
	ucKeyStatus =
		prPairingFsm->fgPeerIgtk | (prPairingFsm->fgPeerBigtk << 1);

	for (i = 0; i < NAN_BSS_INDEX_NUM; i++) {
		prNanSpecInfo = nanGetSpecificBssInfo(prAdapter, i);
		if (prNanSpecInfo == NULL)
			continue;
		prNanBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prNanSpecInfo->ucBssIndex);
		if (prNanBssInfo == NULL)
			continue;
		u2WtblIdx = secPrivacySeekForNanEntry(prAdapter,
			aucBCAddr, pucPeerAddr, FALSE,
			prNanBssInfo->ucBssIndex, &fgWtblReUsed);

		if (u2WtblIdx != WTBL_RESERVED_ENTRY) {
			nanRegisterMcRxWtblIdx(prAdapter, pucPeerAddr,
				u2WtblIdx, prNanBssInfo->ucBssIndex,
				fgWtblReUsed);

			nanSetIgtkBigtkInMcRxWtbl(prAdapter,
				pucPeerAddr, ucKeyStatus);
		}

		if (prPairingFsm->fgPeerIgtk) {
			nanSecSetIgtkToSm(prAdapter, pucPeerAddr,
				CSIA_CAP_IGTKSA_BIGTKSA_NCS_BIP_128, 128,
				prPairingFsm->aucPeerIGtk, FALSE, u2WtblIdx);
		}
		if (prPairingFsm->fgPeerBigtk) {
			nanSecSetBigtkToSm(prAdapter, pucPeerAddr,
				CSIA_CAP_IGTKSA_BIGTKSA_NCS_BIP_128, 128,
				prPairingFsm->aucPeerBIGtk, FALSE, u2WtblIdx);
		}
	}
#endif /* (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1) */

#if 0 /* temp disable timer*/
	cnmTimerStopTimer(prAdapter,
		&prPairingFsm->rNikTimer);
	/*ToDo:Init Timer to check get
	 * Auth Txdone avoid sta_rec not clear
	 */
	cnmTimerInitTimer(prAdapter,
		&prPairingFsm->rNikTimer,
		(PFN_MGMT_TIMEOUT_FUNC)
		pairingNikLifetimeout,
		(unsigned long) prPairingFsm);
	cnmTimerStartTimer(prAdapter,
		&prPairingFsm->rNikTimer,
		prNikLifetimeKde->lifetime);
#endif
}
void
pairingNikLifetimeout(struct ADAPTER *prAdapter,
		      unsigned long plParamPtr)
{
	struct PAIRING_FSM_INFO *prPairingFsm =
		(struct PAIRING_FSM_INFO *) plParamPtr;
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
	DBGLOG(NAN, INFO, "PairingFSM %d clear peer nik\n",
		prPairingFsm->ucIndex);
	kalMemZero(prPairingFsm->aucPeerNik, NAN_NIK_LEN);
}
void
pairingServiceLifetimeout(struct ADAPTER *prAdapter,
			  unsigned long plParamPtr)
{
	struct PAIRING_FSM_INFO *prPairingFsm =
		(struct PAIRING_FSM_INFO *) plParamPtr;
	if (!prPairingFsm) {
		DBGLOG(NAN, ERROR, "FSM NULL and return\n");
		return;
	}
	DBGLOG(NAN, INFO, "PairingFSM %d clear peer nik\n",
		prPairingFsm->ucIndex);
	pairingFsmFree(prAdapter, prPairingFsm);
}
void
pairingDbgInfo(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;
	struct STA_RECORD *prStaRec = NULL;
	uint8_t tag[8] = {0}, peer_tag[8] = {0};

	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm && prPairingFsm->fgIsInUse) {
			prStaRec = prPairingFsm->prStaRec;
			DBGLOG(NAN, INFO,
				"--- PairingFSM %zu ---\n", szIdx);
			DBGLOG(NAN, INFO,
				"Bootstrapping method = 0x%x\n",
				prPairingFsm->u2BootstrapMethod);
			DBGLOG(NAN, INFO,
				"Pairing state = %u\n",
				prPairingFsm->ePairingState);
			DBGLOG(NAN, INFO,
				"Pub or not=%u\n",
				prPairingFsm->ucIsPub);
			DBGLOG(NAN, INFO,
				"PubId = %u, SubId = %u\n",
				prPairingFsm->ucPublishID,
				prPairingFsm->ucSubscribeID);
			DBGLOG(NAN, INFO,
				"NPK/NIK Caching enable = %u\n",
				prPairingFsm->fgCachingEnable);
			if (prStaRec) {
				DBGLOG(NAN, INFO,
					"Peer Addr = %x:%x:%x:%x:%x:%x\n",
					prStaRec->aucMacAddr[0],
					prStaRec->aucMacAddr[1],
					prStaRec->aucMacAddr[2],
					prStaRec->aucMacAddr[3],
					prStaRec->aucMacAddr[4],
					prStaRec->aucMacAddr[5]);
				DBGLOG(NAN, INFO,
					"WlanIdx = %u, StaRecIndex=%u\n",
					prStaRec->ucWlanIndex,
					prStaRec->ucIndex);
			} else {
				DBGLOG(NAN, INFO, "StaRec still NULL\n");
			}
			/* Local Info */
			DBGDUMP_HEX(NAN, DEBUG, "NAN_Nonce",
				prPairingFsm->aucNonce,
				NAN_NIR_NONCE_LEN);
			kalMemCpyS(tag, NAN_NIR_TAG_LEN,
				&prPairingFsm->u8Tag, NAN_NIR_TAG_LEN);
			DBGDUMP_HEX(NAN, DEBUG, "NAN_Tag",
				tag, NAN_NIR_TAG_LEN);
			DBGDUMP_HEX(NAN, DEBUG, "NAN_NIK",
				prPairingFsm->aucNik, NAN_NIK_LEN);
			/* Peer Info */
			DBGDUMP_HEX(NAN, DEBUG, "NAN_PEER_Nonce",
				prPairingFsm->aucPeerNonce,
				NAN_NIR_NONCE_LEN);
			kalMemCpyS(peer_tag, NAN_NIR_TAG_LEN,
				&prPairingFsm->u8PeerTag,
				NAN_NIR_TAG_LEN);
			DBGDUMP_HEX(NAN, DEBUG, "NAN_PEER_Tag",
				peer_tag, NAN_NIR_TAG_LEN);
			DBGDUMP_HEX(NAN, DEBUG, "NAN_PEER_NIK",
				prPairingFsm->aucPeerNik, NAN_NIK_LEN);
			/* TK */
			if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED ||
			    prPairingFsm->ePairingState ==
			    NAN_PAIRING_PAIRED_VERIFICATION) {
				DBGDUMP_HEX(NAN, DEBUG, "NAN_TK",
					prPairingFsm->prPtk->tk,
					prPairingFsm->prPtk->tk_len);
			} else {
				DBGLOG(NAN, INFO, "NAN_TK=0\n");
			}
			DBGDUMP_HEX(NAN, DEBUG, "NAN_PMKID",
				prPairingFsm->rNpksaCache.pmkid,
				NAN_NPKID_LEN);
			DBGDUMP_HEX(NAN, DEBUG, "NAN_PMK",
				prPairingFsm->rNpksaCache.pmk,
				NAN_NPK_LEN);
			DBGLOG(NAN, INFO, "---------------------\n");
		}
	}
}
void pairingDbgStepsLog(struct PAIRING_FSM_INFO *prPairingFsm,
	size_t szIdx)
{
	if (prPairingFsm->fgIsInUse) {
		DBGLOG(NAN, INFO, "--- PairingFSM %zu STATE ---\n", szIdx);
		DBGLOG(NAN, INFO, "Current state = %u\n",
		prPairingFsm->ePairingState);
		prPairingFsm->ePairingState = pairingFsmNextState(prPairingFsm);
		DBGLOG(NAN, INFO, "Next state = %u\n",
		prPairingFsm->ePairingState);
		DBGLOG(NAN, INFO, "PubId = %u, SubId = %u\n",
		prPairingFsm->ucPublishID, prPairingFsm->ucSubscribeID);
		DBGLOG(NAN, INFO, "---------------------\n");
		if (prPairingFsm->ePairingState == NAN_PAIRING_PAIRED) {
			if (prPairingFsm->prPtk) {
				prPairingFsm->prPtk->kek_len = WPA_KEK_MAX_LEN;
				prPairingFsm->prPtk->kck_len = WPA_KCK_MAX_LEN;
				prPairingFsm->prPtk->kdk_len = WPA_KDK_MAX_LEN;
				prPairingFsm->prPtk->tk_len = WPA_TK_MAX_LEN;
				random_get_bytes(prPairingFsm->prPtk->kek,
						WPA_KEK_MAX_LEN);
				random_get_bytes(prPairingFsm->prPtk->kdk,
						WPA_KDK_MAX_LEN);
				random_get_bytes(prPairingFsm->prPtk->kck,
						WPA_KCK_MAX_LEN);
				random_get_bytes(prPairingFsm->prPtk->tk,
						WPA_TK_MAX_LEN);
			}
		}
	}
}
void
pairingDbgSteps(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm)
			pairingDbgStepsLog(prPairingFsm, szIdx);
	}
}
void
pairingDbgTwoStepsImpl(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	size_t szIdx)
{
	if (prPairingFsm->fgIsInUse) {
		DBGLOG(NAN, INFO,
			"--- PairingFSM %zu GOOT WRONG STATE ---\n", szIdx);
		DBGLOG(NAN, INFO,
			"Current state = %u\n", prPairingFsm->ePairingState);
		prPairingFsm->ePairingState += 2;
		DBGLOG(NAN, INFO,
			"Next state = %u\n", prPairingFsm->ePairingState);
		DBGLOG(NAN, INFO,
			"PubId = %u, SubId = %u\n",
			prPairingFsm->ucPublishID,
			prPairingFsm->ucSubscribeID);
		pairingFsmSteps(prAdapter,
			prPairingFsm, prPairingFsm->ePairingState);
		DBGLOG(NAN, INFO, "---------------------\n");
	}
}
void
pairingDbgTwoSteps(struct ADAPTER *prAdapter)
{
	size_t szIdx = 0;
	struct PAIRING_FSM_INFO *prPairingFsm = NULL;

	for (szIdx = 0; szIdx < NAN_MAX_NDP_SESSIONS; szIdx++) {
		prPairingFsm = &prAdapter->rWifiVar.arPairingFsmInfo[szIdx];
		if (prPairingFsm)
			pairingDbgTwoStepsImpl(prAdapter, prPairingFsm, szIdx);
	}
}
void
pairingPrintKeyInfo(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm)
{
	u8 pmkid[16] = {0};
	u8 tag[8] = {0};
	u8 nonce[8] = {0};

	kalMemCpyS(tag, NAN_NIR_TAG_LEN, &prPairingFsm->u8Tag, NAN_NIR_TAG_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "tag", tag, NAN_NIR_TAG_LEN);
	kalMemCpyS(nonce, NAN_NIR_NONCE_LEN,
		prPairingFsm->aucNonce, NAN_NIR_NONCE_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "nonce", nonce, NAN_NIR_NONCE_LEN);
	kalMemCpyS(pmkid, NAN_NIR_NONCE_LEN, nonce, NAN_NIR_NONCE_LEN);
	kalMemCpyS(pmkid + NAN_NIR_NONCE_LEN,
		NAN_NIR_TAG_LEN, tag, NAN_NIR_TAG_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "pmkid", pmkid, NAN_NPKID_LEN);
	DBGDUMP_HEX(NAN, DEBUG, "[pairging-key1] Pairing-TK",
		prPairingFsm->prPtk->tk, prPairingFsm->prPtk->tk_len);
	DBGDUMP_HEX(NAN, DEBUG, "[pairging-key1] Pairing-KCK",
		prPairingFsm->prPtk->kck, prPairingFsm->prPtk->kck_len);
	DBGDUMP_HEX(NAN, DEBUG, "[pairging-key1] Pairing-KDK",
		prPairingFsm->prPtk->kdk, prPairingFsm->prPtk->kdk_len);
	DBGDUMP_HEX(NAN, DEBUG, "[pairing-key2] Pairing-KEK",
		prPairingFsm->prPtk->kek, prPairingFsm->prPtk->kek_len);

	if (prPairingFsm->u2BootstrapMethod !=
		NAN_BOOTSTRAPPING_OPPORTUNISTIC) {
		DBGDUMP_HEX(NAN, DEBUG, "pairing-PMK",
			prPairingFsm->key_info.body.pmk_info.pmk,
			prPairingFsm->key_info.body.pmk_info.pmk_len);
		DBGDUMP_HEX(NAN, DEBUG, "cache Pairing-PMK",
			prPairingFsm->rNpksaCache.pmk,
			prPairingFsm->rNpksaCache.pmk_len);
		DBGDUMP_HEX(NAN, DEBUG, "cache Pairing-PMKID",
			prPairingFsm->rNpksaCache.pmkid,
			NAN_NPKID_LEN);
	}
}
uint32_t
nanPairingProcessAuthFrame(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	u32 *pairing_request_type,
	u8 *enable_pairing_cache,
	u8 *nonce,
	u8 *nir_tag
	)
{
	struct WLAN_AUTH_FRAME *prAuthFrame;
	uint8_t *pucAttrList = NULL;
	uint16_t u2AttrListLength;
	uint8_t *pucOffset, *pucEnd;
	struct _NAN_ATTR_HDR_T *prNanAttr;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	if (!prSwRfb) {
		DBGLOG(NAN, ERROR, "prSwRfb error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	*pairing_request_type = NAN_PAIRING_SETUP_REQ_T;
	*enable_pairing_cache = 0;

	prAuthFrame =
		(struct WLAN_AUTH_FRAME *)(prSwRfb->pvHeader);
	if (!prAuthFrame) {
		DBGLOG(NAN, ERROR, "prNaf error\n");
		return WLAN_STATUS_INVALID_DATA;
	}

	u2AttrListLength =
		prSwRfb->u2PacketLen -
		OFFSET_OF(struct WLAN_AUTH_FRAME, aucInfoElem);
	pucAttrList =
		(uint8_t *)(prAuthFrame->aucInfoElem);
	{
		/* move pucAttrList & u2_AttrListLength more
		 * until VendorSpecific IE with OUI Type 19
		 */
		const uint8_t *start = pucAttrList;
		size_t len = u2AttrListLength;
		const struct element *elem;

		pucOffset = NULL;
		pucEnd = NULL;
		for_each_element(elem, start, len) {
			uint8_t id = elem->id;
			const uint8_t *pos = elem->data;
			unsigned int oui = WPA_GET_BE24(pos);

			if ((id == WLAN_EID_VENDOR_SPECIFIC) &&
				(oui == OUI_WFA) &&
				(pos[3] == VENDOR_OUI_TYPE_NAN_SDF)) {
				pucOffset = (uint8_t *)&pos[4];
				pucEnd = (uint8_t *)(pos + elem->datalen);
			}
		}
	}
	if (pucOffset != NULL) {
		DBGLOG(NAN, INFO,
			"parse IE OK[0,1,2,3]=0x%02x|%02x|%02x|%02x\n",
			pucOffset[0], pucOffset[1], pucOffset[2], pucOffset[3]);
	} else {
		DBGLOG(NAN, ERROR,
			"[pairing-pasn] parsing NAN IE failed\n");
		return WLAN_STATUS_FAILURE;
	}


	DBGDUMP_HEX(TX, INFO, "Dump Pairing PASN Frame:",
		pucAttrList, u2AttrListLength);

	while (pucOffset < pucEnd && rStatus == WLAN_STATUS_SUCCESS) {
		if (pucEnd - pucOffset <
			OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody)) {
			/* insufficient length */
			DBGLOG(NAN, ERROR, "[pairing-pasn]insufficient len\n");
			break;
		}
		/* buffer pucAttr for later type-casting purposes */
		prNanAttr = (struct _NAN_ATTR_HDR_T *)pucOffset;
		if (pucEnd - pucOffset <
			OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
				prNanAttr->u2Length) {
			/* insufficient length */
			DBGLOG(NAN, ERROR,
				"[pairing-pasn]prNanAttr->u2Length=%u\n",
				prNanAttr->u2Length);
			break;
		}
		DBGLOG(NAN, INFO,
			"prNanAttr->ucAttrId=%u, prNanAttr->u2Length=%u\n",
			prNanAttr->ucAttrId, prNanAttr->u2Length);
		/* move to next Attr */
		pucOffset += (OFFSET_OF(struct _NAN_ATTR_HDR_T, aucAttrBody) +
				  prNanAttr->u2Length);
		DBGLOG(NAN, INFO,
			"[pairing-pasn]ucAttrId=%u\n",
			prNanAttr->ucAttrId);

		/* Parsing attributes */
		switch (prNanAttr->ucAttrId) {
		case NAN_ATTR_ID_DEVICE_CAPABILITY_EXT:
		{
			/* DCEA */
			struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T
			*prAttrDevCapExt =
			(struct _NAN_ATTR_DEVICE_CAPABILITY_EXT_T *) prNanAttr;
			*enable_pairing_cache =
			((*(u16 *)prAttrDevCapExt->aucExtCapabilities) &
				NAN_ATTR_DCEA_NPK_NIK_CACHING_ENABLE);
			DBGLOG(NAN, INFO,
				"[pairing-pasn] enable_pairing_cache=%d\n",
				*enable_pairing_cache);
			break;
		}
		case NAN_ATTR_ID_NAN_PAIRING_BOOTSTRAPPING:
		{
			/* NPBA */
			struct _NAN_ATTR_NPBA_T *prAttrNPBA =
				(struct _NAN_ATTR_NPBA_T *) prNanAttr;

			DBGLOG(NAN, INFO,
				"ucDialogTok:%d,ucTypeStatus:%d,u2BootstapMethod:%d\n",
				prAttrNPBA->ucDialogTok,
				prAttrNPBA->ucTypeStatus,
				prAttrNPBA->u2BootstapMethod);
			break;
		}
		case NAN_ATTR_ID_CIPHER_SUITE_INFO:
		{
			/* CSIA */
			struct _NAN_ATTR_CIPHER_SUITE_INFO_T
			*prCipherSuiteAttr =
				(struct _NAN_ATTR_CIPHER_SUITE_INFO_T *)
				prNanAttr;
			struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *prCipherSuite =
				(struct _NAN_CIPHER_SUITE_ATTRIBUTE_T *)
				prCipherSuiteAttr->aucCipherSuiteList;

			DBGLOG(NAN, INFO,
				"ucCipherSuiteID:%d, ucPublishID:%d\n",
				prCipherSuite->ucCipherSuiteID,
				prCipherSuite->ucPublishID);

			break;
		}
		case NAN_ATTR_ID_NAN_IDENTITY_RESOLUTION:
		{
			/* NIRA */
			struct _NAN_ATTR_NIRA_T *prNira =
				(struct _NAN_ATTR_NIRA_T *) prNanAttr;

			DBGLOG(NAN, INFO,
				"[pairing-verification] u8Nonce:%llu, u8Tag:%llu\n",
				prNira->u8Nonce, prNira->u8Tag);
			memcpy(nonce, &prNira->u8Nonce, NAN_IDENTITY_NONCE_LEN);
			memcpy(nir_tag, &prNira->u8Tag, NAN_IDENTITY_TAG_LEN);
			*pairing_request_type = NAN_PAIRING_VERIFICATION_REQ_T;
			DBGLOG(NAN, INFO,
				"[pairing-verification] pairing_request_type=%d\n",
				*pairing_request_type);
			break;
		}
		default:
			break;
		}
	}
	return WLAN_STATUS_SUCCESS;
}

uint32_t nanPairingVerification_FsmFF(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm, uint8_t *pucPeerNMI)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint8_t ucBssIndex = 0;

	if (prPairingFsm == NULL || pucPeerNMI == NULL) {
		DBGLOG(NAN, ERROR,
			"[pairing-verification] prPairingFsm NULL !!\n");
		return WLAN_STATUS_FAILURE;
	}
	/* 1. starec alloc for for 2G */
	if (prPairingFsm->prStaRec == NULL) {
		ucBssIndex = nanGetBssIdxbyBand(prAdapter, BAND_2G4);
		DBGLOG(NAN, INFO, "go alloc pairing starec\n");
		prPairingFsm->prStaRec =
			cnmStaRecAlloc(prAdapter, STA_TYPE_NAN,
			ucBssIndex, pucPeerNMI);
		if (prPairingFsm->prStaRec) {
			atomic_set(&prPairingFsm->prStaRec->NanRefCount, 1);
		} else {
			rStatus = WLAN_STATUS_FAILURE;
			goto error;
		}
	}
	DBGLOG(NAN, DEBUG, "Peer=> "MACSTR_A"\n",
			pucPeerNMI[0], pucPeerNMI[1], pucPeerNMI[2],
			pucPeerNMI[3], pucPeerNMI[4], pucPeerNMI[5]);

	/* 2. for 5G, let FSM setup 5gStaRec */
	pairingFsmSteps(prAdapter, prPairingFsm,
		NAN_PAIRING_BOOTSTRAPPING_DONE);

error:
	return rStatus;
}

void nanPairingCalCustomPMKID(uint64_t nonce, uint64_t tag, uint8_t *PMKID)
{
	memcpy((void *)PMKID, &nonce, NAN_NIR_NONCE_LEN);
	memcpy((void *)PMKID+NAN_NIR_NONCE_LEN, &tag, sizeof(uint64_t));
}

void nanPairingSavePublisherNonce(uint64_t nonce)
{
	g_publisherNonce = nonce;
}
uint64_t nanPairingLoadPublisherNonce(void)
{
	return g_publisherNonce;
}

void nanPairingHandleNdlDisconnect(struct ADAPTER *prAdapter,
		struct PAIRING_FSM_INFO *prPairingFsm) {
	if (prPairingFsm == NULL)
		return;

	mtk_cfg80211_vendor_event_nan_pairing_ndl_disconnect(
		prAdapter, (uint8_t *)prPairingFsm);

	if (prPairingFsm->ucIsPub) {
		pairingFsmCancelRequest(prAdapter,
			prPairingFsm->ucPublishID, TRUE);
		nanDiscFreeInstance(prPairingFsm->ucPublishID);
	} else {
		pairingFsmCancelRequest(prAdapter,
			prPairingFsm->ucSubscribeID, FALSE);
		nanDiscFreeInstance(prPairingFsm->ucSubscribeID);
	}
}
#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#endif /* CFG_SUPPORT_NAN */

