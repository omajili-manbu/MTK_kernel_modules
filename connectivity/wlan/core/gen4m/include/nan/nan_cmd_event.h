/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*! \file   "nan_cmd_event.h"
 *  \brief This file contains the declairation file of the NAN event handlers
 */

#ifndef _NAN_CMD_EVENT_H
#define _NAN_CMD_EVENT_H

struct _CMD_EVENT_TLV_COMMOM_T {
	uint16_t u2TotalElementNum;
	uint8_t aucReserved[2];
	uint8_t aucBuffer[];
};

struct _CMD_EVENT_TLV_ELEMENT_T {
	uint32_t tag_type;
	uint32_t body_len; /* size of the following aucbody[] */
	uint8_t aucbody[];
};

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_MASTER_PREFERENCE_T {
	uint8_t ucMasterPreference;
	uint8_t aucReserved[3];
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

struct EVENT_UPDATE_NAN_TX_STATUS {
	uint8_t aucFlowCtrl[CFG_STA_REC_NUM];
};

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_UPDATE_ATTR_STRUCT {
	uint8_t ucAttrId;
	uint16_t u2AttrLen;
	uint8_t aucAttrBuf[1024];
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_DW_INTERVAL_T {
	uint8_t ucDWInterval;
	uint8_t ucNanVendorIoctl;
	uint16_t u2NanDiscBcnInterval;
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_GET_DEVICE_INFO {
	uint8_t ucVersion;
	uint8_t aucReserved[3];
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

enum ENUM_NAN_DISC_BCN_TYPE {
	ENUM_DISC_BCN_PERIOD = 0,
	ENUM_DISC_BCN_SLOT
};

/* Due to the firmware size limitation, we have temporarily set it to 256.
 * The data sent from wifip2pd is often 35, and the recommended setting
 * is 512 or 1024.
 */
#define NAN_CUSTOM_ATTRIBUTE_MAX_SIZE 256
struct NanCustomAttribute {
	u16 length;
	u8 data[NAN_CUSTOM_ATTRIBUTE_MAX_SIZE];
};

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_DFSP_CONFIG {
	uint16_t version;
	uint16_t length;
	uint16_t flags;
	/* bit 0 = enable;no other defined */
	/* duration of no beacon for suspension */
	uint16_t max_bcn_miss_duration;
	uint8_t mcsp_ttl;
	uint8_t bcsa_cnt;
	uint8_t max_empty_aw;
	uint16_t mon_chan;
	/* passive monitor channel */
	uint8_t mon_bssid[MAC_ADDR_LEN];
	/* bssid of the AP */
	uint16_t max_bcn_miss_af_duration;
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_UPDATE_CUSTOM_ATTR_T {
	uint16_t u2Length;
	uint8_t aucReserved[2];
	uint8_t aucData[256];
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

struct NAN_EVENT_REPORT_DW_T {
	uint32_t expected_tsf_h;
	uint32_t expected_tsf_l;
	uint32_t actual_tsf_h;
	uint32_t actual_tsf_l;
	uint16_t channel;
	uint16_t dw_num;
};

struct NAN_EVENT_DEVICE_ROLE_T {
	uint8_t ucNanDeviceRole;
	uint8_t ucHopCount;
	uint8_t aucReserved[2];
};

__KAL_ATTRIB_PACKED_FRONT__ __KAL_ATTRIB_ALIGNED_FRONT__(4)
struct _NAN_CMD_LOWPOWER_CTRL_T {
	uint8_t ucEnabled;
	uint8_t aucReserved[3];
} __KAL_ATTRIB_PACKED__ __KAL_ATTRIB_ALIGNED__(4);

struct _NAN_EVENT_DEVICE_INFO {
	uint8_t ucIsEnabled;
	uint8_t aucSelfMacAddr[MAC_ADDR_LEN];
	uint8_t ucFwElectionEnable;
	uint32_t u4NanDeviceRole;
	uint32_t u4NanDeviceState;
	uint8_t ucMasterPreference;
	uint8_t ucRandomFactor;
	uint8_t ucHopCount;
	uint8_t aucClusterID[MAC_ADDR_LEN];
	uint8_t aucAnchorMasterMacAddr[MAC_ADDR_LEN];
	uint8_t ucAmMasterPreference;
	uint8_t ucAmRandomFactor;
	uint8_t aucParentMacAddr[MAC_ADDR_LEN];
	uint8_t ucParentMasterPreference;
	uint8_t ucParentRandomFactor;
	uint32_t u4AMBTT;
	uint32_t au4Tsf[2];
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	uint8_t aucIgtkPn[MAC_ADDR_LEN];
	uint8_t aucBigtkPn[MAC_ADDR_LEN];
#endif /* CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT */
};

struct _NAN_EVENT_REPORT_BEACON {
	enum ENUM_BAND eRfBand;
	int32_t i4Rssi;
	uint32_t au4LocalTsf[2];
	uint16_t u2BeaconLength;
	uint16_t u2TxMode;
	uint8_t ucRate;
	uint8_t ucHwChnl;
	uint8_t ucBw;
	uint8_t aucAnchorMasterRank[ANCHOR_MASTER_RANK_NUM];
	uint8_t aucReserved[5];
	uint8_t aucBeaconFrame[];
};

struct _NAN_EVENT_LOWPOWER_CTRL {
	uint8_t ucPeerSchRecordTxMap;
	uint8_t aucRsvd[7];
};

enum _ENUM_NAN_SUB_CMD {
	NAN_CMD_TEST = 0,       /* 0 */
	NAN_TXM_TEST = 1,
	NAN_CMD_MASTER_PREFERENCE = 2,
	NAN_CMD_HOP_COUNT = 3,
	NAN_CMD_PUBLISH = 4,
	NAN_CMD_CANCEL_PUBLISH = 5,     /* 5 */
	NAN_CMD_UPDATE_PUBLISH = 6,
	NAN_CMD_SUBSCRIBE = 7,
	NAN_CMD_CANCEL_SUBSCRIBE = 8,
	NAN_CMD_TRANSMIT = 9,
	NAN_CMD_ENABLE_REQUEST = 10,     /* 10 */
	NAN_CMD_DISABLE_REQUEST = 11,
	NAN_CMD_UPDATE_AVAILABILITY = 12,
	NAN_CMD_UPDATE_CRB = 13,
	NAN_CMD_CRB_HANDSHAKE_TOKEN = 14,
	NAN_CMD_MANAGE_PEER_SCH_RECORD = 15, /* 15 */
	NAN_CMD_MAP_STA_RECORD = 16,
	NAN_CMD_RANGING_REPORT_DISC = 17,
	NAN_CMD_FTM_PARAM = 18,
	NAN_CMD_UPDATE_PEER_ULW = 19,
	NAN_CMD_UPDATE_ATTR = 20,    /* 20 */
	NAN_CMD_UPDATE_PHY_SETTING = 21,
	NAN_CMD_UPDATE_POTENTIAL_CHNL_LIST = 22,
	NAN_CMD_UPDATE_AVAILABILITY_CTRL = 23,
	NAN_CMD_UPDATE_PEER_CAPABILITY = 24,
	NAN_CMD_ADD_CSID = 25,   /* 25 */
	NAN_CMD_MANAGE_SCID = 26,
	NAN_CMD_CHANGE_ADDRESS = 27,
	NAN_CMD_SET_SCHED_VERSION = 28,
	NAN_CMD_SET_NAN_CONFIG = 29,
	NAN_CMD_SET_DISC_BCN = 30, /* 30 */
	NAN_CMD_UPDATE_POTENTIAL_AVAILABILITY = 31,
	NAN_CMD_UPDATE_CUSTOM_ATTR = 32,
	NAN_CMD_DFSP_CONFIG = 33,
	NAN_CMD_GET_DEVICE_INFO = 34,
	NAN_CMD_VENDOR_PAYLOAD = 35,
	NAN_CMD_LOWPOWER_CTRL = 37,
	NAN_CMD_SET_HOST_ELECTION = 42,
	NAN_CMD_SET_ELECTION_ROLE = 43,
	NAN_CMD_INSTANT_COMM_MODE = 44,
	NAN_CMD_PUBLISH_EXT = 50,
	NAN_CMD_SUBSCRIBE_EXT = 51,
	NAN_CMD_TRANSMIT_EXT = 52,
	NAN_CMD_KEY_MGMT = 53,
	NAN_CMD_NDC_MGMT = 54,
	NAN_CMD_SET_NDI = 55,
	NAN_CMD_DUMP_SEC_INFO = 56,
	NAN_CMD_SET_DW_INTERVAL = 60,
	NAN_CMD_ENABLE_UNSYNC = 61,

	/* EXT_CMD Part */
	/* Reserve for vendor r, 100 ~ 199 */

	/* Reserve for vendor s, 200 ~ 299 */
	NAN_CMD_EXT_CUSTOM_CMD = 200,
	NAN_CMD_EXT_CLUSTER = 252,
	NAN_CMD_EXT_P2P = 255,
	NAN_CMD_EXT_MERGING_DIRECTION = 256,
	NAN_CMD_EXT_SYNC = 257,
	NAN_CMD_EXT_MERGING = 258,
	NAN_CMD_EXT_SCHEDULING = 259,
	NAN_CMD_EXT_USD = 264,
	NAN_CMD_EXT_ASC = 265,

	NAN_CMD_NUM
};

/* NAN set command Tag */
enum ENUM_UNI_CMD_NAN_TAG {
	UNI_CMD_NAN_TAG_SET_MASTER_PREFERENCE = 0,
	UNI_CMD_NAN_TAG_PUBLISH = 1,
	UNI_CMD_NAN_TAG_CANCEL_PUBLISH = 2,
	UNI_CMD_NAN_TAG_UPDATE_PUBLISH = 3,
	UNI_CMD_NAN_TAG_SUBSCRIBE = 4,
	UNI_CMD_NAN_TAG_CANCEL_SUBSCRIBE = 5,
	UNI_CMD_NAN_TAG_TRANSMIT = 6,
	UNI_CMD_NAN_TAG_ENABLE_REQUEST = 7,
	UNI_CMD_NAN_TAG_DISABLE_REQUEST = 8,
	UNI_CMD_NAN_TAG_UPDATE_AVAILABILITY = 9,
	UNI_CMD_NAN_TAG_UPDATE_CRB = 10,
	UNI_CMD_NAN_TAG_CRB_HANDSHAKE_TOKEN = 11,
	UNI_CMD_NAN_TAG_MANAGE_PEER_SCH_RECORD = 12,
	UNI_CMD_NAN_TAG_MAP_STA_RECORD = 13,
	UNI_CMD_NAN_TAG_RANGING_REPORT_DISC = 14,
	UNI_CMD_NAN_TAG_FTM_PARAM = 15,
	UNI_CMD_NAN_TAG_UPDATE_PEER_ULW = 16,
	UNI_CMD_NAN_TAG_UPDATE_ATTR = 17,
	UNI_CMD_NAN_TAG_UPDATE_PHY_SETTING = 18,
	UNI_CMD_NAN_TAG_UPDATE_POTENTIAL_CHNL_LIST = 19,
	UNI_CMD_NAN_TAG_UPDATE_AVAILABILITY_CTRL = 20,
	UNI_CMD_NAN_TAG_UPDATE_PEER_CAPABILITY = 21,
	UNI_CMD_NAN_TAG_ADD_CSID = 22,
	UNI_CMD_NAN_TAG_MANAGE_SCID = 23,
	UNI_CMD_NAN_TAG_CHANGE_ADDRESS = 24,
	UNI_CMD_NAN_TAG_SET_SCHED_VERSION = 25,
	UNI_CMD_NAN_TAG_SET_DW_INTERVAL = 26,
	UNI_CMD_NAN_TAG_ENABLE_UNSYNC = 30,
	UNI_CMD_NAN_TAG_GET_DEVICE_INFO = 33,
	UNI_CMD_NAN_TAG_VENDOR_PAYLOAD = 35,
	UNI_CMD_NAN_TAG_LOWPOWER_CTRL = 37,
	UNI_CMD_NAN_TAG_SET_HOST_ELECTION = 42,
	UNI_CMD_NAN_TAG_SET_ELECTION_ROLE = 43,
	UNI_CMD_NAN_TAG_INSTANT_COMM_MODE = 44,
	UNI_CMD_NAN_TAG_KEY_MGMT = 53,
	UNI_CMD_NAN_TAG_NDC_MGMT = 54,

	/* EXT_CMD Part */
	/* Reserve for vendor r, 100 ~ 199 */

	/* Reserve for vendor s, 200 ~ 299 */
	UNI_CMD_NAN_TAG_EXT_CUSTOM_CMD = 200,
	NAN_CMD_EXT_TAG_CLUSTER = 252,
	NAN_CMD_EXT_TAG_P2P = 255,
	NAN_CMD_EXT_TAG_MERGING_DIRECTION = 256,
	NAN_CMD_EXT_TAG_SYNC = 257,
	NAN_CMD_EXT_TAG_MERGING = 258,
	NAN_CMD_EXT_TAG_SCHEDULING = 259,
	NAN_CMD_EXT_TAG_USD = 264,
	NAN_CMD_EXT_TAG_ASC = 265,
	UNI_CMD_NAN_TAG_MAX_NUM
};

enum _ENUM_NAN_SUB_EVENT {
	NAN_EVENT_TEST = 0, /* 0 */
	NAN_EVENT_DISCOVERY_RESULT = 1,
	NAN_EVENT_FOLLOW_EVENT = 2,
	NAN_EVENT_MASTER_IND_ATTR = 3,
	NAN_EVENT_CLUSTER_ID_UPDATE = 4,
	NAN_EVENT_REPLIED_EVENT = 5,    /* 5 */
	NAN_EVENT_PUBLISH_TERMINATE_EVENT = 6,
	NAN_EVENT_SUBSCRIBE_TERMINATE_EVENT = 7,
	NAN_EVENT_ID_SCHEDULE_CONFIG = 8,
	NAN_EVENT_ID_PEER_AVAILABILITY = 9,
	NAN_EVENT_ID_PEER_CAPABILITY = 10,   /* 10 */
	NAN_EVENT_ID_CRB_HANDSHAKE_TOKEN = 11,
	NAN_EVENT_ID_DATA_NOTIFY = 12,
	NAN_EVENT_FTM_DONE = 13,
	NAN_EVENT_RANGING_BY_DISC = 14,
	NAN_EVENT_NDL_FLOW_CTRL = 15,    /* 15 */
	NAN_EVENT_DW_INTERVAL = 16,
	NAN_EVENT_NDL_DISCONNECT = 17,
	NAN_EVENT_ID_PEER_CIPHER_SUITE_INFO = 18,
	NAN_EVENT_ID_PEER_SEC_CONTEXT_INFO = 19,
	NAN_EVENT_ID_DE_EVENT_IND = 20,  /* 20 */
	NAN_EVENT_SELF_FOLLOW_EVENT = 21,
	NAN_EVENT_DISABLE_IND = 22,
	NAN_EVENT_NDL_FLOW_CTRL_V2 = 23,
	NAN_EVENT_ID_DEVICE_CAPABILITY = 24,
	NAN_EVENT_DISC_BCN_PERIOD = 25,  /* 25 */
	NAN_EVENT_DFSP_CSA = 26,
	NAN_EVENT_DFSP_CSA_COMPLETE = 27,
	NAN_EVENT_DFSP_SUSPEND_RESUME = 28,
	NAN_EVENT_REPORT_DW_START = 29,
	NAN_EVENT_REPORT_DW_END = 30, /* 30 */
	NAN_EVENT_DEVICE_ROLE = 31,
	NAN_EVENT_REPORT_BEACON = 32,
	NAN_EVENT_DEVICE_INFO = 33,
	NAN_EVENT_SERVICE_DISC_CAPABILITY = 34,
	NAN_EVENT_MATCH_EXPIRE = 35,
	NAN_EVENT_SLOT_STATISTICS = 36,
	NAN_EVENT_LOWPOWER_CTRL = 37,
	NAN_EVENT_RANGING_CTRL = 38,
	NAN_EVENT_UPDATE_LOCAL_ULW = 39,
	NAN_EVENT_REPORT_PACKET_NUM = 40,
	NAN_EVENT_VENDOR_DISCOVERY_RESULT = 50, /* 50 */
	NAN_EVENT_VENDOR_PUBLISH_REPLIED_EVENT = 51,
	NAN_EVENT_VENDOR_FOLLOW_UP_RX_EVENT = 52,
	NAN_EVENT_VENDOR_FOLLOW_UP_TX_EVENT = 53,

	NAN_EVENT_NUM
};

uint32_t nicNanAddNewTlvElement(uint32_t u4Tag, uint32_t u4BodyLen,
				uint32_t prCmdBufferLen,
				struct _CMD_EVENT_TLV_COMMOM_T *prCmdBuffer);

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
uint16_t nanGetLegacyEventId(uint32_t u4SubEvent);
#endif

uint32_t nanGetEventTag(uint8_t *pucBody);
uint32_t nanGetEventBodyLength(uint8_t *pucBody);
uint8_t *nanGetEventBody(struct WIFI_EVENT *prEvent);

void nicNanIOEventHandler(struct ADAPTER *prAdapter,
			  struct WIFI_EVENT *prEvent);

struct _CMD_EVENT_TLV_ELEMENT_T *
nicNanGetTargetTlvElement(uint16_t u2TargetTlvElement,
			  struct _CMD_EVENT_TLV_COMMOM_T *prCmdBuffer);

void nicNanVendorEventHandler(struct ADAPTER *prAdapter,
			      struct WIFI_EVENT *prEvent);

#if CFG_SUPPORT_NAN_ADVANCE_DATA_CONTROL
void nicNanNdlFlowCtrlEvt(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf);
void nicNanNdlFlowCtrlEvtV2(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf);
#endif

int32_t nanGetSubCmdId(uint32_t tag, uint16_t *u2CmdTag);

#endif /* _NAN_CMD_EVENT_H */
