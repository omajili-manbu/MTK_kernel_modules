/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef _NAN_PAIRING_H_
#define _NAN_PAIRING_H_
#if CFG_SUPPORT_NAN
#include "nan/nan_sec.h"
#if (CFG_SUPPORT_NAN_R4_PAIRING == 1)
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#define NAN_NIK_LEN			16 /* 128 bits */
#define NAN_NIR_TAG_LEN		8 /* 64 bits */
#define NAN_NIR_NONCE_LEN	8 /* 64 bits */
#define NAN_GMK_LEN			32
#define NAN_NPKID_LEN		16
#define NAN_NPK_LEN			32
#define NAN_NPK_LEN_MAX		64
#define NAN_GTK_LEN			16 /* NCS-GTK-CCMP-128 */
#define NAN_IGTK_LEN			16 /* NCS-BIP-128 */
#define NAN_BIGTK_LEN			16 /* NCS-BIP-128 */
#define KEY_EXCHANGE_RETRY_LIMIT 10
#define TEST_FIXED_NIK		0
#define TEST_FIXED_IGTK_BIGTK	1
/* NAN pairing state */
enum NAN_PAIRING_STATE {
	/* NAN Pairing Init */
	NAN_PAIRING_INIT = 0,
	/* NAN Pairing Bootstrpping */
	NAN_PAIRING_BOOTSTRAPPING,
	/* NAN Pairing Bootstrapping Done and wait for pairing start command */
	NAN_PAIRING_BOOTSTRAPPING_DONE,
	/* NAN Pairing start setup sequence  */
	NAN_PAIRING_SETUP,
	/* NAN Pairing setup done and TK is generated */
	NAN_PAIRING_PAIRED,
	/* NAN Pairing start verification */
	NAN_PAIRING_PAIRED_VERIFICATION,
	/* NAN Pairing cancel */
	NAN_PAIRING_CANCEL,
	/* NAN Pairing state when get no pairing fsm*/
	NAN_PAIRING_INVALID,
	/* NAN Pairing state number*/
	NAN_PAIRING_MGMT_STATE_NUM
};
/* NAN pairing fsm mode */
enum NAN_PAIRING_FSM_MODE {
	/* Fsm mode pairing */
	NAN_PAIRING_FSM_MODE_PAIRING = 0,
	/* Fsm mode verification */
	NAN_PAIRING_FSM_MODE_VERIFICATION = 1,
};
enum NAN_BOOTSTRAPPING_METHOD {
	NAN_BOOTSTRAPPING_OPPORTUNISTIC = 1,
	NAN_BOOTSTRAPPING_PIN_CODE_DISPLAY,
	NAN_BOOTSTRAPPING_PASSPHRASE_DISPLAY,
	NAN_BOOTSTRAPPING_QR_CODE_DISPLAY,
	NAN_BOOTSTRAPPING_NFC_TAG,
	NAN_BOOTSTRAPPING_KEYPAD_PIN_CODE = 6,
	NAN_BOOTSTRAPPING_KEYPAD_PASSPHRASE,
	NAN_BOOTSTRAPPING_QR_CODE_SCAN,
	NAN_BOOTSTRAPPING_NFC_READER,
	NAN_BOOTSTRAPPING_SERVICE_MANAGE,
	NAN_BOOTSTRAPPING_HANDSHAKE_SKIP,
	NAN_BOOTSTRAPPING_METHOD_NUM
};
enum NAN_BOOTSTRAPPING_TYPE {
	NAN_BOOTSTRAPPING_TYPE_ADVERTISE,
	NAN_BOOTSTRAPPING_TYPE_REQUEST,
	NAN_BOOTSTRAPPING_TYPE_RESPONSE
};
enum NAN_BOOTSTRAPPING_STATUS {
	NAN_BOOTSTRAPPING_STATUS_ACCEPTED,
	NAN_BOOTSTRAPPING_STATUS_REJECTED,
	NAN_BOOTSTRAPPING_STATUS_COMEBACK
};
enum NAN_PAIRING_ROLE {
	NAN_PAIRING_REQUESTOR,
	NAN_PAIRING_RESPONDER
};
struct wpa_ptk;
struct npksa_cache {
	/* Tag *2^64 + Nonce */
	u8 pmkid[NAN_NPKID_LEN];
	u8 isPmkCached;
	u8 pmk[NAN_NPK_LEN_MAX];
	size_t pmk_len;
};
/* NAN Pairing FSM */
struct PAIRING_FSM_INFO {
	/* Index of Pairing FSM */
	uint8_t ucIndex;
	uint8_t ucPublishID;
	uint8_t ucSubscribeID;
	uint8_t ucIsPub;
	uint16_t u2Ttl;
	struct TIMER rTtlTimer;
	/* 0:Requestor,Initiator, 1:Responder */
	uint8_t ucPairingType;
	/* Bootsatrapping method for Pairing Setup */
	uint16_t u2BootstrapMethod;
	/* Current Pairing state to indicate pairing setup progress */
	enum NAN_PAIRING_STATE ePairingState;
	/* StaRec and Peer Address(NMI) for recored per peer info */
	struct STA_RECORD *prStaRec;
	/* StaRec and Peer Address(NMI) for recored per peer info */
	struct STA_RECORD *prStaRec5G;
	/* NPK/NIK caching enable or not */
	uint8_t fgCachingEnable;
	/* Pairing FSM is in use or not */
	uint8_t fgIsInUse;
	/* Bootstrapping method need handshake or not */
	uint8_t fgIsBootStrapNeedHandShake;
	/* PTK info */
	struct wpa_ptk *prPtk;
	uint32_t u4SelCipherType;
	/*
	 * NPKSA Cache : Pairing FSM fill this struct after NIRA resolved
	 * for Pairing verification
	*/
	struct npksa_cache rNpksaCache;
	/* NIK, to generate NIRA TAG for verification */
	uint8_t aucNik[NAN_NIK_LEN];
	uint8_t aucPeerNik[NAN_NIK_LEN];
	struct TIMER rNikTimer;
	/* GMK, for generate NIK */
	uint8_t aucGmk[NAN_GMK_LEN];
	/* Nonce for generate NIR-TAG */
	uint8_t aucNonce[NAN_NIR_NONCE_LEN];
	uint8_t aucPeerNonce[NAN_NIR_NONCE_LEN];
	/* TAG, derived from NIK */
	uint64_t u8Tag;
	uint64_t u8PeerTag;
	/* Paired or not, trigger verification only paired == TRUE */
	uint8_t fgIsPaired;
	/* RX SKDA key data */
	uint16_t u2Skda2KeyLength;
	uint8_t aucSkdaKeyData[NAN_KDE_ATTR_BUF_SIZE];
	/* Store PMK */
	struct NanSecurityKeyInfo key_info;
	/* NAN IE for PASN */
	uint8_t nan_ie[NAN_PASN_IE_LEN];
	size_t nan_ie_len;
	/* NAN passphrase for SAE */
	char *wpa_passphrase;
	uint8_t aucBssid[MAC_ADDR_LEN]; /* NAN cluster ID */

	/* For multicast frame protection */
	uint8_t aucIGtk[NAN_IGTK_LEN];
	uint8_t fgPeerIgtk;
	uint8_t aucPeerIGtk[NAN_IGTK_LEN];
	uint8_t aucBIGtk[NAN_BIGTK_LEN];
	uint8_t fgPeerBigtk;
	uint8_t aucPeerBIGtk[NAN_BIGTK_LEN];

/* For AOSP start */
	/* fgPeerNIK_received */
	uint8_t fgPeerNIK_received;
	/* whether NIK has been sent to peer  */
	uint8_t fgLocalNIK_sent;
	u8 npk[NAN_NPK_LEN_MAX];
	size_t npk_len;
	u8 akm;
	uint8_t fgM3Acked;
	wait_queue_head_t confirm_ack_wq;
	struct TIMER arKeyExchangeTimer;
	uint8_t ucTxKeyRetryCounter;
	/* Current Pairing FSM mode for verification */
	enum NAN_PAIRING_FSM_MODE FsmMode;
	u8 ucDialogToken; /* NPBA Dialog Token(for initiator) */
/* For AOSP end */
};

#define NAN_KDE_DATA_TYPE_GTK		0x01
#define NAN_KDE_DATA_TYPE_IGTK		0x09
#define NAN_KDE_DATA_TYPE_BIGTK		0x0E
#define NAN_KDE_DATA_TYPE_NIK		0x24
#define NAN_KDE_DATA_TYPE_NIKLIFETIME	0x25

/* IGTK_KDE_INFO for NCS-BIP-128
 * OUI(3):00-0F-AC + Data Type(9):0x09 + IGTK Data(variable)
 * IEEE Std 802.11-2024 Figure 12-44 - IGTK KDE format
 */
struct IGTK_KDE_INFO {
	uint8_t type;
	uint8_t length;
	uint8_t oui[3];
	uint8_t data_type;
	uint8_t key_id[2];
	uint8_t IPN[NAN_PACKET_NUMBER_LEN];
	uint8_t igtk[NAN_IGTK_LEN];
} __packed;

/* BIGTK_KDE_INFO for NCS-BIP-128
 * OUI(3):00-0F-AC + Data Type(14):0x0E + BIGTK Data(variable)
 * IEEE Std 802.11-2024 Figure 12-49 - BIGTK KDE format
 */
struct BIGTK_KDE_INFO {
	uint8_t type;
	uint8_t length;
	uint8_t oui[3];
	uint8_t data_type;
	uint8_t key_id[2];
	uint8_t BIPN[NAN_PACKET_NUMBER_LEN];
	uint8_t bigtk[NAN_BIGTK_LEN];
} __packed;

struct NIK_KDE_INFO {
	uint8_t type;
	uint8_t length;
	uint8_t oui[3];
	uint8_t data_type;
	uint8_t cipher_ver;
	/* NIK length: 128 bits */
	uint8_t nik[NAN_NIK_LEN];
} __packed;
struct NIK_LIFETIME_KDE_INFO {
	uint8_t type;
	uint8_t length;
	uint8_t oui[3];
	uint8_t data_type;
	uint16_t key_bitmap;
	uint32_t lifetime;
} __packed;
struct PAIRING_FSM_INFO *
pairingFsmAlloc(struct ADAPTER *prAdapter);
void
pairingFsmInit(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm);
struct PAIRING_FSM_INFO *
pairingFsmSearch(struct ADAPTER *prAdapter, uint8_t *pucPeerAddr);
void
pairingFsmSteps(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	enum NAN_PAIRING_STATE eNextState);
uint8_t
pairingFsmCurrState(struct ADAPTER *prAdapter, uint8_t *pucPeerAddr);
uint8_t
pairingFsmNextState(struct PAIRING_FSM_INFO *prPairingFsm);
uint8_t
pairingFsmNextStateForVerification(struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingFsmSetBootStrappingMethod(struct PAIRING_FSM_INFO *prPairingFsm,
			uint16_t u2BootstrapMethod);
void
pairingFsmSetup(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm, uint16_t u2BootstrapMethod,
	uint8_t ucCacheEnable, uint8_t ucPubID, uint8_t ucSubID,
	uint8_t ucIsPub, uint16_t u2Ttl);
void
pairingFsmFree(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingFsmUninit(struct ADAPTER *prAdapter);
void
pairingFsmCancelRequest(struct ADAPTER *prAdapter,
			uint8_t ucInstanceId, uint8_t ucIsPub);
uint32_t
nanCmdPairingSetupReq(struct ADAPTER *prAdapter,
			struct NanTransmitFollowupRequest *msg);
uint32_t
nanCmdBootstrapPwdSetup(struct ADAPTER *prAdapter,
			struct NanBootstrapPassword *msg);
void
nanPairingInstallTk(struct wpa_ptk *prPtk, uint32_t u4SelCipherType,
			struct STA_RECORD *prStaRec);
void nanPairingInstallLocalNik(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	u8 *nan_identity_key);
void nanPairingInstallLocalIGtk(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	u8 *igtk);
void nanPairingInstallLocalBIGtk(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm,
	u8 *bigtk);

void
pairingComposeNanIeForSetup(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingComposeNanIeForVerification(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingNotifyPasn(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm);
uint32_t
pairingTxPasnAuthFrame(struct ADAPTER *prAdapter,
		       struct MSDU_INFO *prMsduInfo, uint16_t u2FrameLength,
		       PFN_TX_DONE_HANDLER pfTxDoneHandler,
		       struct STA_RECORD *prSelectStaRec);
uint32_t
pairingRxPasnAuthFrame(struct ADAPTER *prAdapter,
		       struct SW_RFB *prSwRfb);
uint8_t
pairingFsmPairedOrNot(struct ADAPTER *prAdapter,
		      struct NAN_DISCOVERY_EVENT *prDiscEvt);
void
pairingDeriveNirTag(struct ADAPTER *prAdapter,
			const u8 *nik, size_t nik_len, const u8 *aa,
			const u8 *spa, u64 *tag);
void
pairingDeriveNdPmk(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingDeriveNik(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingDeriveNirNonce(struct PAIRING_FSM_INFO *prPairingFsm);

#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
void
pairingComposeIGtkKde(struct IGTK_KDE_INFO *prIGtkKde,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingComposeBIGtkKde(struct BIGTK_KDE_INFO *prBIGtkKde,
			struct PAIRING_FSM_INFO *prPairingFsm);
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
void
pairingComposeNikKde(struct NIK_KDE_INFO *prNikKde,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingComposeNikLifetimeKde(struct ADAPTER *prAdapter,
			struct NIK_LIFETIME_KDE_INFO *prNikLifetimeKde,
			struct PAIRING_FSM_INFO *prPairingFsm);
void
pairingDecryptSKDA(struct ADAPTER *prAdapter,
			struct PAIRING_FSM_INFO *prPairingFsm,
			uint8_t *pucKey, uint16_t u2KeyLen);
void
pairingNikLifetimeout(struct ADAPTER *prAdapter,
		      unsigned long plParamPtr);
void
pairingServiceLifetimeout(struct ADAPTER *prAdapter,
			  unsigned long plParamPtr);
void
pairingDbgInfo(struct ADAPTER *prAdapter);
void
pairingDbgSteps(struct ADAPTER *prAdapter);
void
pairingDbgTwoSteps(struct ADAPTER *prAdapter);
void
pairingPrintKeyInfo(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm);
uint32_t
nanPairingProcessAuthFrame(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	u32 *pairing_request_type,
	u8 *enable_pairing_cache,
	u8 *nonce,
	u8 *nir_tag
	);


uint32_t nanPairingVerification_FsmFF(struct ADAPTER *prAdapter,
	struct PAIRING_FSM_INFO *prPairingFsm, uint8_t *pucPeerNMI);

uint32_t
nanHandlePairingCommand_Verify(struct ADAPTER *prAdapter,
	struct _NanPairingRequestParams *pairingRequest,
	uint16_t publish_subscribe_id);

void nanPairingCalCustomPMKID(uint64_t nonce, uint64_t tag, uint8_t *PMKID);



/* @nanPairingSavePublisherNonce()/nanPairingLoadPublisherNonce()
 *  For pairing verification.
 *  When composing PASN for pairing verification,
 *  The second PASN Authentication frame shall include the same NAN IE as those
 *  included in the publish or subscribe messages transmitted by the pairing
 *  responder. to include same NIRA as publisher message.
 *  responder needs to save Nonce at publish time.
 *  TODO : handle multiple publish case.
 */
void nanPairingSavePublisherNonce(uint64_t nonce);
uint64_t nanPairingLoadPublisherNonce(void);

void nanPairingHandleNdlDisconnect(struct ADAPTER *prAdapter,
		struct PAIRING_FSM_INFO *prPairingFsm);

struct NAN_PASN_START_PARAM {
	u8 peer_mac_addr[NAN_MAC_ADDR_LEN];
	u8 own_mac_addr[NAN_MAC_ADDR_LEN];
/* SAE_AKM = 0, PASN_AKM = 1 */
	u32 akm;
/* NAN_PAIRING_SETUP_REQ_T(0) or NAN_PAIRING_VERIFICATION_REQ_T(1) */
	u32 nan_pairing_request_type;
	u32 cipher_type;
	u8 bssid[NAN_MAC_ADDR_LEN];
	u8 nan_ie_len;
	u8 nan_ie[64];
	u8 key_type;
	u32 key_len;
	u8 key_data[NAN_SECURITY_MAX_PASSPHRASE_LEN];
/* for verification */
	u8 pmkid[NAN_NPKID_LEN];
} __packed;


struct NanPairingPASNMsg {
	uint32_t framesize;
	uint8_t PASN_FRAME[NAN_PASN_FRAME_MAX_SIZE];
	u8 nan_ie_len;
	u8 nan_ie[64];
	union {
		/* host's response to M1. */
		struct _NanPairingResponseParams hostM1param;
	} localhostconfig PACKED;
} PACKED;

enum PASN_STATUS {
	PASN_STATUS_SUCCESS = 0,
	PASN_STATUS_FAILURE = 1,
};

#define PASN_M1_AUTH_SEQ 1
#define PASN_M2_AUTH_SEQ 2
#define PASN_M3_AUTH_SEQ 3

uint32_t nanPasnRequestM1_Tx(struct ADAPTER *prAdapter,
	struct NAN_PASN_START_PARAM *prPasnStartParam);
uint32_t nanPasnRxAuthFrame(struct ADAPTER *prAdapter, struct SW_RFB *prSwRfb);
uint32_t nanPasnRequestM1_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *pasnframe);
uint32_t nanPasnRequestM2_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *pasnframe);
uint32_t nanPasnRequestM3_Rx(struct ADAPTER *prAdapter,
	struct NanPairingPASNMsg *pasnframe);

uint32_t nanPasnSetKey(struct ADAPTER *prAdapter,
	struct nan_pairing_keys_t *key_data);
uint32_t nanPairingPeerNikReceived(struct ADAPTER *prAdapter,
	uint8_t *pcuEvtBuf,
	struct PAIRING_FSM_INFO *prPairingFsm);

extern uint8_t g_last_matched_report_npba_dialogTok;
extern uint8_t g_last_bootstrapReq_npba_dialogTok;
extern bool sigma_nik_exchange_run;

#endif /* CFG_SUPPORT_NAN_R4_PAIRING */
#endif /* CFG_SUPPORT_NAN */
#endif /* _NAN_PAIRING_H_ */

