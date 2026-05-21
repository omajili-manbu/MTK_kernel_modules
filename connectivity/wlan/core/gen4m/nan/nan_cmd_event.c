/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#if (CFG_SUPPORT_NAN == 1)

#include "precomp.h"
#include "typedef.h"

#include "nic_uni_cmd_event.h"

const char *nanCmdTagString(uint32_t tag)
{
	const char * const nanTagStr[] = {
		[NAN_CMD_TEST] = "Test",
		[NAN_TXM_TEST] = "TXM Test",
		[NAN_CMD_MASTER_PREFERENCE] = "Master Preference",
		[NAN_CMD_HOP_COUNT] = "Hop Count",
		[NAN_CMD_PUBLISH] = "Publish",
		[NAN_CMD_CANCEL_PUBLISH] = "Cancel Publish", /* 5 */
		[NAN_CMD_UPDATE_PUBLISH] = "Update Publish",
		[NAN_CMD_SUBSCRIBE] = "Subscribe",
		[NAN_CMD_CANCEL_SUBSCRIBE] = "Cancel Subscribe",
		[NAN_CMD_TRANSMIT] = "Transmit",
		[NAN_CMD_ENABLE_REQUEST] = "Enable Request", /* 10 */
		[NAN_CMD_DISABLE_REQUEST] = "Disable Request",
		[NAN_CMD_UPDATE_AVAILABILITY] = "Update Availability",
		[NAN_CMD_UPDATE_CRB] = "Update CRB",
		[NAN_CMD_CRB_HANDSHAKE_TOKEN] = "CRB Handshake Token",
		[NAN_CMD_MANAGE_PEER_SCH_RECORD] =
			"Manage Peer Schedule Record", /* 15 */
		[NAN_CMD_MAP_STA_RECORD] = "Map STA Record",
		[NAN_CMD_RANGING_REPORT_DISC] = "Ranging Reposrt Discovery",
		[NAN_CMD_FTM_PARAM] = "FTM Parameters",
		[NAN_CMD_UPDATE_PEER_ULW] = "Update Peer ULW",
		[NAN_CMD_UPDATE_ATTR] = "Update Attribute", /* 20 */
		[NAN_CMD_UPDATE_PHY_SETTING] = "Update Phy Setting",
		[NAN_CMD_UPDATE_POTENTIAL_CHNL_LIST] =
			"Update Potential Channel List",
		[NAN_CMD_UPDATE_AVAILABILITY_CTRL] =
			"Update Availability Control",
		[NAN_CMD_UPDATE_PEER_CAPABILITY] = "Update Peer Capability",
		[NAN_CMD_ADD_CSID] = "Add CSID", /* 25 */
		[NAN_CMD_MANAGE_SCID] = "Manage CSID",
		[NAN_CMD_CHANGE_ADDRESS] = "Change Address",
		[NAN_CMD_SET_SCHED_VERSION] = "Set Scheduling Version",
		[NAN_CMD_SET_NAN_CONFIG] = "Set NAN Config",
		[NAN_CMD_SET_DISC_BCN] = "Set Discovery Beacon", /* 30 */
		[NAN_CMD_UPDATE_POTENTIAL_AVAILABILITY] = "Update Potential",
		[NAN_CMD_UPDATE_CUSTOM_ATTR] = "Update Custom Attribute",
		[NAN_CMD_DFSP_CONFIG] = "DFSP Config",
		[NAN_CMD_GET_DEVICE_INFO] = "Get Device Info",
		[NAN_CMD_VENDOR_PAYLOAD] = "Vendor Payload", /* 35 */
		[NAN_CMD_LOWPOWER_CTRL] = "Low Power Control",

		[NAN_CMD_SET_HOST_ELECTION] = "Set Host Election",
		[NAN_CMD_SET_ELECTION_ROLE] = "Set Election Role",
		[NAN_CMD_INSTANT_COMM_MODE] = "Instant Comm Mode",

		[NAN_CMD_PUBLISH_EXT] = "Publish Ext", /* 50 */
		[NAN_CMD_SUBSCRIBE_EXT] = "Subscribe Ext",
		[NAN_CMD_TRANSMIT_EXT] = "Transmit Ext",
		[NAN_CMD_KEY_MGMT] = "Key Mgmt",
		[NAN_CMD_NDC_MGMT] = "Ndc Mgmt",
		[NAN_CMD_SET_NDI] = "Set NDI", /* 55 */
		[NAN_CMD_DUMP_SEC_INFO] = "Dump Sec Info",

		[NAN_CMD_SET_DW_INTERVAL] = "Set DW interval", /* 60 */
		[NAN_CMD_ENABLE_UNSYNC] = "Enable UnSync",
	};

	/* Reserve for vendor s, 200 ~ 299 */
	const char * const nanTagStr2[] = {
		[NAN_CMD_EXT_CLUSTER - NAN_CMD_EXT_CLUSTER] = "Ext Cluster",
		[NAN_CMD_EXT_P2P - NAN_CMD_EXT_CLUSTER] = "Ext P2P",
		[NAN_CMD_EXT_MERGING_DIRECTION - NAN_CMD_EXT_CLUSTER] =
			"Ext Merging Direction",
		[NAN_CMD_EXT_SYNC - NAN_CMD_EXT_CLUSTER] = "Ext Sync",
		[NAN_CMD_EXT_MERGING - NAN_CMD_EXT_CLUSTER] = "Ext merging",
		[NAN_CMD_EXT_SCHEDULING - NAN_CMD_EXT_CLUSTER] =
			"Ext Scheduling",
		[NAN_CMD_EXT_USD - NAN_CMD_EXT_CLUSTER] = "Ext USD",
		[NAN_CMD_EXT_ASC - NAN_CMD_EXT_CLUSTER] = "Ext ASC",
	};

	if (tag == NAN_CMD_EXT_CUSTOM_CMD) {
		return "Ext Custom Command";
	} else if (tag < ARRAY_SIZE(nanTagStr)) {
		if (nanTagStr[tag])
			return nanTagStr[tag];
		else
			return "";
	} else if (tag >= NAN_CMD_EXT_CLUSTER &&
		   tag < NAN_CMD_EXT_CLUSTER + ARRAY_SIZE(nanTagStr2)) {
		if (nanTagStr2[tag - NAN_CMD_EXT_CLUSTER])
			return nanTagStr2[tag - NAN_CMD_EXT_CLUSTER];
		else
			return "";
	}

	return "";
}

enum ENUM_UNI_CMD_NAN_TAG nanUniCommandTag[NAN_CMD_NUM] = {
	[NAN_CMD_MASTER_PREFERENCE] = UNI_CMD_NAN_TAG_SET_MASTER_PREFERENCE,
	[NAN_CMD_PUBLISH] = UNI_CMD_NAN_TAG_PUBLISH,
	[NAN_CMD_CANCEL_PUBLISH] = UNI_CMD_NAN_TAG_CANCEL_PUBLISH,
	[NAN_CMD_UPDATE_PUBLISH] = UNI_CMD_NAN_TAG_UPDATE_PUBLISH,
	[NAN_CMD_SUBSCRIBE] = UNI_CMD_NAN_TAG_SUBSCRIBE,
	[NAN_CMD_CANCEL_SUBSCRIBE] = UNI_CMD_NAN_TAG_CANCEL_SUBSCRIBE,
	[NAN_CMD_TRANSMIT] = UNI_CMD_NAN_TAG_TRANSMIT,
	[NAN_CMD_ENABLE_REQUEST] = UNI_CMD_NAN_TAG_ENABLE_REQUEST,
	[NAN_CMD_DISABLE_REQUEST] = UNI_CMD_NAN_TAG_DISABLE_REQUEST,
	[NAN_CMD_UPDATE_AVAILABILITY] = UNI_CMD_NAN_TAG_UPDATE_AVAILABILITY,
	[NAN_CMD_UPDATE_CRB] = UNI_CMD_NAN_TAG_UPDATE_CRB,
	[NAN_CMD_MANAGE_PEER_SCH_RECORD] =
		UNI_CMD_NAN_TAG_MANAGE_PEER_SCH_RECORD,
	[NAN_CMD_MAP_STA_RECORD] = UNI_CMD_NAN_TAG_MAP_STA_RECORD,
	[NAN_CMD_RANGING_REPORT_DISC] = UNI_CMD_NAN_TAG_RANGING_REPORT_DISC,
	[NAN_CMD_FTM_PARAM] = UNI_CMD_NAN_TAG_FTM_PARAM,
	[NAN_CMD_UPDATE_PEER_ULW] = UNI_CMD_NAN_TAG_UPDATE_PEER_ULW,
	[NAN_CMD_UPDATE_ATTR] = UNI_CMD_NAN_TAG_UPDATE_ATTR,
	[NAN_CMD_UPDATE_PHY_SETTING] = UNI_CMD_NAN_TAG_UPDATE_PHY_SETTING,
	[NAN_CMD_UPDATE_POTENTIAL_CHNL_LIST] =
		UNI_CMD_NAN_TAG_UPDATE_POTENTIAL_CHNL_LIST,
	[NAN_CMD_UPDATE_AVAILABILITY_CTRL] =
		UNI_CMD_NAN_TAG_UPDATE_AVAILABILITY_CTRL,
	[NAN_CMD_UPDATE_PEER_CAPABILITY] =
		UNI_CMD_NAN_TAG_UPDATE_PEER_CAPABILITY,
	[NAN_CMD_ADD_CSID] = UNI_CMD_NAN_TAG_ADD_CSID,
	[NAN_CMD_MANAGE_SCID] = UNI_CMD_NAN_TAG_MANAGE_SCID,
	[NAN_CMD_SET_SCHED_VERSION] = UNI_CMD_NAN_TAG_SET_SCHED_VERSION,
	[NAN_CMD_SET_DW_INTERVAL] = UNI_CMD_NAN_TAG_SET_DW_INTERVAL,
	[NAN_CMD_ENABLE_UNSYNC] = UNI_CMD_NAN_TAG_ENABLE_UNSYNC,
	[NAN_CMD_GET_DEVICE_INFO] = UNI_CMD_NAN_TAG_GET_DEVICE_INFO,
	[NAN_CMD_VENDOR_PAYLOAD] = UNI_CMD_NAN_TAG_VENDOR_PAYLOAD,
	[NAN_CMD_LOWPOWER_CTRL] = UNI_CMD_NAN_TAG_LOWPOWER_CTRL,
	[NAN_CMD_SET_HOST_ELECTION] = UNI_CMD_NAN_TAG_SET_HOST_ELECTION,
	[NAN_CMD_SET_ELECTION_ROLE] = UNI_CMD_NAN_TAG_SET_ELECTION_ROLE,
	[NAN_CMD_INSTANT_COMM_MODE] = UNI_CMD_NAN_TAG_INSTANT_COMM_MODE,
	[NAN_CMD_KEY_MGMT] = UNI_CMD_NAN_TAG_KEY_MGMT,
	[NAN_CMD_NDC_MGMT] = UNI_CMD_NAN_TAG_NDC_MGMT,

	[NAN_CMD_EXT_CUSTOM_CMD] = UNI_CMD_NAN_TAG_EXT_CUSTOM_CMD,
	[NAN_CMD_EXT_CLUSTER] = NAN_CMD_EXT_TAG_CLUSTER,
	[NAN_CMD_EXT_P2P] = NAN_CMD_EXT_TAG_P2P,
	[NAN_CMD_EXT_MERGING_DIRECTION] = NAN_CMD_EXT_TAG_MERGING_DIRECTION,
	[NAN_CMD_EXT_SYNC] = NAN_CMD_EXT_TAG_SYNC,
	[NAN_CMD_EXT_MERGING] = NAN_CMD_EXT_TAG_MERGING,
	[NAN_CMD_EXT_SCHEDULING] = NAN_CMD_EXT_TAG_SCHEDULING,
	[NAN_CMD_EXT_USD] = NAN_CMD_EXT_TAG_USD,
	[NAN_CMD_EXT_ASC] = NAN_CMD_EXT_TAG_ASC,
};

int32_t nanGetSubCmdId(uint32_t tag, uint16_t *u2CmdTag)
{
	enum ENUM_UNI_CMD_NAN_TAG eCmdTag = -1;

	if (tag >= NAN_CMD_NUM)
		return -1;

	eCmdTag = nanUniCommandTag[tag];
	if (unlikely(eCmdTag == 0) && tag != NAN_CMD_MASTER_PREFERENCE)
		return -1;

	if (u2CmdTag)
		*u2CmdTag = eCmdTag;
	return eCmdTag;
}

void nanGetSubCmdIdString(uint32_t tag, char subcmd[], size_t szBufSize)
{
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	uint16_t u2CmdTag = -1;

	if (nanGetSubCmdId(tag, &u2CmdTag) >= 0)
		kalSnprintf(subcmd, szBufSize, "(Subcmd:%d)", u2CmdTag);
#else
	kalSnprintf(subcmd, szBufSize, "");
#endif
}

/**
 * nicNanGetTargetTlvElement() - Get the n-th TLV element by u2TargetTlvElement
 * @u2TargetTlvElement: id of the target TLV element, 1-based indexing
 * @prTlvCommon: TLV structure to retrieve the element
 *
 * Context:
 *   When getting an existing element, the id shall be less than or equal to
 *   the current element number in the TLV.
 *   When calling from nicNanAddNewTlvElement(), the id will be
 *   the current element number in the TLV added by 1 to return the position
 *   to append a new entry.
 *
 * Return: pointer to the queried n-th target TLV element
 */
struct _CMD_EVENT_TLV_ELEMENT_T *nicNanGetTargetTlvElement(
		   uint16_t u2TargetTlvElement,
		   struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon)
{
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement = NULL;
	uint16_t u2ElementNum;
	void *pvCurrPtr;

	if (u2TargetTlvElement - prTlvCommon->u2TotalElementNum > 1) {
		/* element is not exist */
		DBGLOG(TX, ERROR, "Target element is not exist\n");
		return NULL;
	}

	for (u2ElementNum = 1; u2ElementNum <= u2TargetTlvElement;
	     u2ElementNum++) {
		if (u2ElementNum == 1) {
			pvCurrPtr = prTlvCommon->aucBuffer;
		} else {
			pvCurrPtr =
				&prTlvElement->aucbody[prTlvElement->body_len];
		}
		prTlvElement = pvCurrPtr;
	}

	return prTlvElement;
}

/**
 * nicNanAddNewTlvElement() - Add an element to NAN TLV structure
 * @u4Tag: Tag of new TLV
 * @u4BodyLen: Length of new TLV
 * @u4CmdBufferLen: Total buffer size to hold concatenated TLV structures
 * @prCmdBuffer: Buffer to hold concatenated TLV structures
 *
 * Context:
 *   The u4BodyLen is only used to check the remaining size against
 *   u4CmdBufferLen, and return the position of new TLV.
 *   The value in TLV is still empty.
 *   The caller calls nicNanGetTargetTlvElement() later to get the position to
 *   new TLV to copied the values to the buffer.
 *
 *   The u2TotalElementNum has incremented in this function.
 *
 * Return: WLAN_STATUS_SUCCESS: check pass and Tag and Length are updated
 *»        WLAN_STATUS_NOT_ACCEPTED: no sufficient buffer
 *»        WLAN_STATUS_FAILURE: unexpected error
 */
uint32_t nicNanAddNewTlvElement(uint32_t u4Tag, uint32_t u4BodyLen,
				uint32_t u4CmdBufferLen,
				struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon)
{
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement;
	uint32_t u4TotalLen;
	char subcmd[20] = {0};

	/* Get pointer to new element (the one right after current last) */
	prTlvElement = nicNanGetTargetTlvElement(
		prTlvCommon->u2TotalElementNum + 1, prTlvCommon);
	if (prTlvElement == NULL) {
		DBGLOG(NAN, ERROR, "Get new TLV element fail\n");
		return WLAN_STATUS_FAILURE;
	}

	/* Check tatol len is overflow or not,
	 * u4TotalLen = len(end of the TLV - start of the TLV common
	 */
	u4TotalLen = ((size_t)prTlvElement->aucbody + u4BodyLen) -
		     (size_t)prTlvCommon;

	if (u4TotalLen > u4CmdBufferLen) { /* Length overflow */
		DBGLOG(NAN, ERROR,
		       "Length overflow: Total len:%d, CMD buffer len:%d\n",
		       u4TotalLen, u4CmdBufferLen);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	/* Update total element count */
	prTlvCommon->u2TotalElementNum++;

	/* Fill TLV constant */
	prTlvElement->tag_type = u4Tag;
	/* Unlinke the UNI_CMD structure as set in nicUniCmdNanGenEntry(),
	 * body_length here only counts the following data field
	 */
	prTlvElement->body_len = u4BodyLen;
	nanGetSubCmdIdString(prTlvElement->tag_type, subcmd, sizeof(subcmd));
	DBGLOG(NAN, INFO, "Add cmd to firmware:%u(%s)%s, len:%u\n",
	       prTlvElement->tag_type,
	       nanCmdTagString(prTlvElement->tag_type),
	       subcmd, prTlvElement->body_len);

	return WLAN_STATUS_SUCCESS;
}

static const char *nan_subevent_str(uint32_t u4SubEvent)
{
	static const char * const subevent_string[NAN_EVENT_NUM] = {
	[NAN_EVENT_TEST] = "Test", /* 0 */
	[NAN_EVENT_DISCOVERY_RESULT] = "Discovery Result",
	[NAN_EVENT_FOLLOW_EVENT] = "Follow",
	[NAN_EVENT_MASTER_IND_ATTR] = "Master Ind",
	[NAN_EVENT_CLUSTER_ID_UPDATE] = "Cluster ID Update",
	[NAN_EVENT_REPLIED_EVENT] = "Replied",
	[NAN_EVENT_PUBLISH_TERMINATE_EVENT] = "Publish Terminate",
	[NAN_EVENT_SUBSCRIBE_TERMINATE_EVENT] = "Subscribe Terminate",
	[NAN_EVENT_ID_SCHEDULE_CONFIG] = "Schedule Config",
	[NAN_EVENT_ID_PEER_AVAILABILITY] = "Peer Availability",
	[NAN_EVENT_ID_PEER_CAPABILITY] = "Peer Capability",
	[NAN_EVENT_ID_CRB_HANDSHAKE_TOKEN] = "CRB Handshake Token",
	[NAN_EVENT_ID_DATA_NOTIFY] = "Data Notify",
	[NAN_EVENT_FTM_DONE] = "FTM Done",
	[NAN_EVENT_RANGING_BY_DISC] = "Ranging by Disc",
	[NAN_EVENT_NDL_FLOW_CTRL] = "NDL Flow Ctrl",
	[NAN_EVENT_DW_INTERVAL] = "DW Interval",
	[NAN_EVENT_NDL_DISCONNECT] = "NDL Disconnect",
	[NAN_EVENT_ID_PEER_CIPHER_SUITE_INFO] = "Peer Cipher Suite Info (CSIA)",
	[NAN_EVENT_ID_PEER_SEC_CONTEXT_INFO] =
		"Peer Security Context Info (SCIA)",
	[NAN_EVENT_ID_DE_EVENT_IND] = "DE Event",
	[NAN_EVENT_SELF_FOLLOW_EVENT] = "Self Follow",
	[NAN_EVENT_DISABLE_IND] = "Disable",
	[NAN_EVENT_NDL_FLOW_CTRL_V2] = "NDL Flow Ctrl v2",
	[NAN_EVENT_ID_DEVICE_CAPABILITY] = "Device Capability",
	[NAN_EVENT_DISC_BCN_PERIOD] = "Discovery Beacon",
	[NAN_EVENT_DFSP_CSA] = "DFSP CSA",
	[NAN_EVENT_DFSP_CSA_COMPLETE] = "DFSP CSA Complete",
	[NAN_EVENT_DFSP_SUSPEND_RESUME] = "DFSP Suspend Resume",
	[NAN_EVENT_REPORT_DW_START] = "DW Start",
	[NAN_EVENT_REPORT_DW_END] = "DW End",
	[NAN_EVENT_DEVICE_ROLE] = "Device Role",
	[NAN_EVENT_SERVICE_DISC_CAPABILITY] =  "Service Discovery Capability",
	[NAN_EVENT_DEVICE_INFO] = "Device Info",
	[NAN_EVENT_REPORT_BEACON] = "Report Beacon",
	[NAN_EVENT_SLOT_STATISTICS] = "Slot Statistics",
	[NAN_EVENT_MATCH_EXPIRE] = "Match Expire",
	[NAN_EVENT_LOWPOWER_CTRL] =  "Low Power Ctrl",
	[NAN_EVENT_RANGING_CTRL] =  "Ranging Ctrl",
	[NAN_EVENT_UPDATE_LOCAL_ULW] = "Update Local ULW",
	[NAN_EVENT_REPORT_PACKET_NUM] = "Update SDF/BCN PN",
	[NAN_EVENT_VENDOR_DISCOVERY_RESULT] = "Vendor Discovery Result",
	[NAN_EVENT_VENDOR_PUBLISH_REPLIED_EVENT] = "Vendor Publish Replied",
	[NAN_EVENT_VENDOR_FOLLOW_UP_RX_EVENT] = "Vendor Follow up RX",
	[NAN_EVENT_VENDOR_FOLLOW_UP_TX_EVENT] =  "Vendor Follow up TX",
	};

	if (u4SubEvent < NAN_EVENT_NUM)
		return subevent_string[u4SubEvent];
	else
		return "";
}

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
static const char *nan_unisubevent_str(uint32_t u4SubEvent)
{
	static const char * const subevent_string[UNI_EVENT_NAN_TAG_NUM] = {
		[UNI_EVENT_NAN_TAG_DISCOVERY_RESULT] = "Discovery Result",
		[UNI_EVENT_NAN_TAG_FOLLOW_EVENT] = "Follow",
		[UNI_EVENT_NAN_TAG_MASTER_IND_ATTR] = "Master Ind",
		[UNI_EVENT_NAN_TAG_CLUSTER_ID_UPDATE] = "Cluster ID Update",
		[UNI_EVENT_NAN_TAG_REPLIED_EVENT] = "Replied",
		[UNI_EVENT_NAN_TAG_PUBLISH_TERMINATE_EVENT] =
			"Publish Terminate",
		[UNI_EVENT_NAN_TAG_SUBSCRIBE_TERMINATE_EVENT] =
			"Subscribe Terminate",
		[UNI_EVENT_NAN_TAG_ID_SCHEDULE_CONFIG] = "Schedule Config",
		[UNI_EVENT_NAN_TAG_ID_PEER_AVAILABILITY] = "Peer Availability",
		[UNI_EVENT_NAN_TAG_ID_PEER_CAPABILITY] = "Peer Capability",
		[UNI_EVENT_NAN_TAG_ID_CRB_HANDSHAKE_TOKEN] =
			"CRB Handshake Token",
		[UNI_EVENT_NAN_TAG_ID_DATA_NOTIFY] = "Data Notify",
		[UNI_EVENT_NAN_TAG_FTM_DONE] = "FTM Done",
		[UNI_EVENT_NAN_TAG_RANGING_BY_DISC] = "Ranging by Disc",
		[UNI_EVENT_NAN_TAG_NDL_FLOW_CTRL] = "NDL Flow Ctrl",
		[UNI_EVENT_NAN_TAG_DW_INTERVAL] = "DW Interval",
		[UNI_EVENT_NAN_TAG_NDL_DISCONNECT] = "NDL Disconnect",
		[UNI_EVENT_NAN_TAG_ID_PEER_CIPHER_SUITE_INFO] =
			"Peer Cipher Suite Info (CSIA)",
		[UNI_EVENT_NAN_TAG_ID_PEER_SEC_CONTEXT_INFO] =
			"Peer Security Context Info (SCIA)",
		[UNI_EVENT_NAN_TAG_ID_DE_EVENT_IND] = "DE Event",
		[UNI_EVENT_NAN_TAG_SELF_FOLLOW_EVENT] = "Self Follow",
		[UNI_EVENT_NAN_TAG_DISABLE_IND] = "Disable",
		[UNI_EVENT_NAN_TAG_NDL_FLOW_CTRL_V2] = "NDL Flow Ctrl v2",
		[UNI_EVENT_NAN_TAG_ID_DEVICE_CAPABILITY] = "Device Capability",
		[UNI_EVENT_NAN_ID_MATCH_EXPIRE] = "Match Expire",
		[UNI_EVENT_NAN_TAG_DISC_BCN_PERIOD] = "Discovery Beacon",
		[UNI_EVENT_NAN_TAG_DFSP_CSA] = "DFSP CSA",
		[UNI_EVENT_NAN_TAG_DFSP_CSA_COMPLETE] = "DFSP CSA Complete",
		[UNI_EVENT_NAN_TAG_DFSP_SUSPEND_RESUME] = "DFSP Suspend Resume",
		[UNI_EVENT_NAN_TAG_REPORT_DW_START] = "DW Start",
		[UNI_EVENT_NAN_TAG_REPORT_DW_END] = "DW End",
		[UNI_EVENT_NAN_TAG_DEVICE_ROLE] = "Device Role",
		[UNI_EVENT_NAN_DEVICE_INFO] = "Device Info",
		[UNI_EVENT_NAN_TAG_REPORT_BEACON] = "Report Beacon",
		[UNI_EVENT_NAN_TAG_SLOT_STATISTICS] = "Slot Statistics",
		[UNI_EVENT_NAN_TAG_LOWPOWER_CTRL] = "Low Power Ctrl",
		[UNI_EVENT_NAN_TAG_RANGING_CTRL] = "Ranging Ctrl",
		[UNI_EVENT_NAN_REPORT_PACKET_NUM] = "Update SDF/BCN PN"
	};

	if (u4SubEvent < UNI_EVENT_NAN_TAG_NUM)
		return subevent_string[u4SubEvent];
	else
		return "";
}

uint16_t nanGetLegacyEventId(uint32_t u4SubEvent)
{
	const uint32_t au4NanEventMapping[] = {
		[UNI_EVENT_NAN_TAG_DISCOVERY_RESULT] =
			NAN_EVENT_DISCOVERY_RESULT,
		[UNI_EVENT_NAN_TAG_FOLLOW_EVENT] = NAN_EVENT_FOLLOW_EVENT,
		[UNI_EVENT_NAN_TAG_MASTER_IND_ATTR] = NAN_EVENT_MASTER_IND_ATTR,
		[UNI_EVENT_NAN_TAG_CLUSTER_ID_UPDATE] =
			NAN_EVENT_CLUSTER_ID_UPDATE,
		[UNI_EVENT_NAN_TAG_REPLIED_EVENT] = NAN_EVENT_REPLIED_EVENT,

		/* 5 */
		[UNI_EVENT_NAN_TAG_PUBLISH_TERMINATE_EVENT] =
			NAN_EVENT_PUBLISH_TERMINATE_EVENT,
		[UNI_EVENT_NAN_TAG_SUBSCRIBE_TERMINATE_EVENT] =
			NAN_EVENT_SUBSCRIBE_TERMINATE_EVENT,
		[UNI_EVENT_NAN_TAG_ID_SCHEDULE_CONFIG] =
			NAN_EVENT_ID_SCHEDULE_CONFIG,
		[UNI_EVENT_NAN_TAG_ID_PEER_AVAILABILITY] =
			NAN_EVENT_ID_PEER_AVAILABILITY,
		[UNI_EVENT_NAN_TAG_ID_PEER_CAPABILITY] =
			NAN_EVENT_ID_PEER_CAPABILITY,

		/* 10 */
		[UNI_EVENT_NAN_TAG_ID_CRB_HANDSHAKE_TOKEN] =
			NAN_EVENT_ID_CRB_HANDSHAKE_TOKEN,
		[UNI_EVENT_NAN_TAG_ID_DATA_NOTIFY] = NAN_EVENT_ID_DATA_NOTIFY,
		[UNI_EVENT_NAN_TAG_FTM_DONE] = NAN_EVENT_FTM_DONE,
		[UNI_EVENT_NAN_TAG_RANGING_BY_DISC] = NAN_EVENT_RANGING_BY_DISC,
		[UNI_EVENT_NAN_TAG_NDL_FLOW_CTRL] = NAN_EVENT_NDL_FLOW_CTRL,

		/* 15 */
		[UNI_EVENT_NAN_TAG_DW_INTERVAL] = NAN_EVENT_DW_INTERVAL,
		[UNI_EVENT_NAN_TAG_NDL_DISCONNECT] = NAN_EVENT_NDL_DISCONNECT,
		[UNI_EVENT_NAN_TAG_ID_PEER_CIPHER_SUITE_INFO] =
			NAN_EVENT_ID_PEER_CIPHER_SUITE_INFO,
		[UNI_EVENT_NAN_TAG_ID_PEER_SEC_CONTEXT_INFO] =
			NAN_EVENT_ID_PEER_SEC_CONTEXT_INFO,
		[UNI_EVENT_NAN_TAG_ID_DE_EVENT_IND] = NAN_EVENT_ID_DE_EVENT_IND,

		/* 20 */
		[UNI_EVENT_NAN_TAG_SELF_FOLLOW_EVENT] =
			NAN_EVENT_SELF_FOLLOW_EVENT,
		[UNI_EVENT_NAN_TAG_DISABLE_IND] = NAN_EVENT_DISABLE_IND,
		[UNI_EVENT_NAN_TAG_NDL_FLOW_CTRL_V2] =
			NAN_EVENT_NDL_FLOW_CTRL_V2,
		[UNI_EVENT_NAN_TAG_ID_DEVICE_CAPABILITY] =
			NAN_EVENT_ID_DEVICE_CAPABILITY,
		[UNI_EVENT_NAN_ID_MATCH_EXPIRE] = NAN_EVENT_MATCH_EXPIRE,

		/* 25 */
		[UNI_EVENT_NAN_TAG_DISC_BCN_PERIOD] = NAN_EVENT_DISC_BCN_PERIOD,
		/* NOTE 26 is empty */
		/* no handler */
		[UNI_EVENT_NAN_DEVICE_INFO] = NAN_EVENT_DEVICE_INFO,
		[UNI_EVENT_NAN_TAG_REPORT_BEACON] = NAN_EVENT_REPORT_BEACON,
		[UNI_EVENT_NAN_TAG_SLOT_STATISTICS] = NAN_EVENT_SLOT_STATISTICS,

		/* NOTE 30~36 are empty */

		/* 37 */
		[UNI_EVENT_NAN_TAG_LOWPOWER_CTRL] = NAN_EVENT_LOWPOWER_CTRL,
		[UNI_EVENT_NAN_TAG_RANGING_CTRL] = NAN_EVENT_RANGING_CTRL,
		[UNI_EVENT_NAN_TAG_UPDATE_LOCAL_ULW] =
			NAN_EVENT_UPDATE_LOCAL_ULW,
		[UNI_EVENT_NAN_REPORT_PACKET_NUM] = NAN_EVENT_REPORT_PACKET_NUM,

		/* NOTE 41~55 are empty */

		/* 56 */
		[UNI_EVENT_NAN_TAG_DFSP_CSA] = NAN_EVENT_DFSP_CSA,
		[UNI_EVENT_NAN_TAG_DFSP_CSA_COMPLETE] =
			NAN_EVENT_DFSP_CSA_COMPLETE,
		[UNI_EVENT_NAN_TAG_DFSP_SUSPEND_RESUME] =
			NAN_EVENT_DFSP_SUSPEND_RESUME,
		[UNI_EVENT_NAN_TAG_REPORT_DW_START] = NAN_EVENT_REPORT_DW_START,

		/* 60 */
		[UNI_EVENT_NAN_TAG_REPORT_DW_END] = NAN_EVENT_REPORT_DW_END,
		[UNI_EVENT_NAN_TAG_DEVICE_ROLE] = NAN_EVENT_DEVICE_ROLE,
	};

	if (u4SubEvent >= ARRAY_SIZE(au4NanEventMapping)) {
		DBGLOG(NAN, WARN, "Event %u too large", u4SubEvent);
		return NAN_EVENT_NUM;
	}

	/* not defined */
	if (au4NanEventMapping[u4SubEvent] == 0 &&
	    u4SubEvent != UNI_EVENT_NAN_TAG_DISCOVERY_RESULT) {
		DBGLOG(NAN, WARN, "Table destination entry not defined %u",
		       u4SubEvent);
		return NAN_EVENT_NUM;
	}


	DBGLOG(NAN, TRACE, "NAN UNI event %u(%s) -> %u(%s)",
	       u4SubEvent, nan_unisubevent_str(u4SubEvent),
	       au4NanEventMapping[u4SubEvent],
	       nan_subevent_str(au4NanEventMapping[u4SubEvent]));
	return au4NanEventMapping[u4SubEvent];
}
#endif

uint32_t nanGetEventTag(uint8_t *pucBody)
{
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	struct UNI_CMD_EVENT_TLV_ELEMENT_T *prEvent;
	uint32_t u4SubEvent;

	prEvent = CONTAINER_OF((uint8_t (*)[])pucBody,
			       struct UNI_CMD_EVENT_TLV_ELEMENT_T, aucbody);
	u4SubEvent = prEvent->u2Tag;

	u4SubEvent = nanGetLegacyEventId(u4SubEvent);

	return u4SubEvent;
#else
	struct _CMD_EVENT_TLV_ELEMENT_T *prEvent;

	prEvent = CONTAINER_OF((uint8_t (*)[])pucBody,
			       struct _CMD_EVENT_TLV_ELEMENT_T, aucbody);
	return prEvent->tag_type;
#endif
}

/* Get the Event body length excluding the header  */
uint32_t nanGetEventBodyLength(uint8_t *pucBody)
{
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	struct UNI_CMD_EVENT_TLV_ELEMENT_T *prEvent;

	prEvent = CONTAINER_OF((uint8_t (*)[])pucBody,
			       struct UNI_CMD_EVENT_TLV_ELEMENT_T, aucbody);
	return prEvent->u2Length - sizeof(struct UNI_CMD_EVENT_TLV_ELEMENT_T);
#else
	struct _CMD_EVENT_TLV_ELEMENT_T *prEvent;

	prEvent = CONTAINER_OF((uint8_t (*)[])pucBody,
			       struct _CMD_EVENT_TLV_ELEMENT_T, aucbody);
	return prEvent->body_len;
#endif
}

uint8_t *nanGetEventBody(struct WIFI_EVENT *prEvent)
{
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	struct UNI_CMD_EVENT_TLV_ELEMENT_T *prTlvElement;

	prTlvElement = (struct UNI_CMD_EVENT_TLV_ELEMENT_T *)prEvent->aucBuffer;

	return prTlvElement->aucbody;
#else
	struct _CMD_EVENT_TLV_COMMOM_T *prTlvCommon;
	struct _CMD_EVENT_TLV_ELEMENT_T *prTlvElement;

	prTlvCommon = (struct _CMD_EVENT_TLV_COMMOM_T *)prEvent->aucBuffer;
	prTlvElement =
		(struct _CMD_EVENT_TLV_ELEMENT_T *)prTlvCommon->aucBuffer;

	return prTlvElement->aucbody;
#endif
}

uint8_t g_u2IndPubId;

struct NanMatchInd g_rDiscMatchInd;
void nicNanEventDiscoveryResult(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NAN_DISCOVERY_EVENT *prDiscEvt;

	prDiscEvt = (struct NAN_DISCOVERY_EVENT *)pcuEvtBuf;
	g_u2IndPubId = prDiscEvt->u2PublishID; /* for sigma test */

	DBGLOG(NAN, DEBUG, "generate discovery event\n");
	dumpMemory8((uint8_t *)prDiscEvt->aucNanAddress, MAC_ADDR_LEN);

	kalMemSet(&g_rDiscMatchInd, 0, sizeof(struct NanMatchInd));
	g_rDiscMatchInd.eventID = ENUM_NAN_SD_RESULT;
	g_rDiscMatchInd.peer_sdea_params.config_nan_data_path =
		prDiscEvt->ucDataPathParm;
	g_rDiscMatchInd.publish_subscribe_id = prDiscEvt->u2SubscribeID;
	g_rDiscMatchInd.requestor_instance_id = prDiscEvt->u2PublishID;
	g_rDiscMatchInd.peer_sdea_params.security_cfg = 0;
	g_rDiscMatchInd.peer_cipher_type = 1;
	g_rDiscMatchInd.peer_sdea_params.ndp_type = NAN_DATA_PATH_UNICAST_MSG;
	g_rDiscMatchInd.service_specific_info_len =
		prDiscEvt->u2Service_info_len;
	kalMemCopy(g_rDiscMatchInd.service_specific_info,
		   prDiscEvt->aucSerive_specificy_info,
		   NAN_MAX_SERVICE_SPECIFIC_INFO_LEN);
	COPY_MAC_ADDR(g_rDiscMatchInd.addr, prDiscEvt->aucNanAddress);
	g_rDiscMatchInd.sdf_match_filter_len =
		prDiscEvt->ucSdf_match_filter_len;
	kalMemCopy(g_rDiscMatchInd.sdf_match_filter,
			prDiscEvt->aucSdf_match_filter,
			NAN_FW_MAX_MATCH_FILTER_LEN);

	kalIndicateNetlink2User(prAdapter->prGlueInfo, &g_rDiscMatchInd,
				sizeof(struct NanMatchInd));
}

struct NanFollowupInd rFollowInd;
void nicNanReceiveEvent(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NAN_FOLLOW_UP_EVENT *prDiscEvt;

	_Static_assert(sizeof(rFollowInd.service_specific_info) ==
		       sizeof(prDiscEvt->service_specific_info),
		       "service_specific_info len not match");
	prDiscEvt = (struct NAN_FOLLOW_UP_EVENT *)pcuEvtBuf;
	dumpMemory8((uint8_t *)pcuEvtBuf, 32);
	DBGLOG(NAN, LOUD, "receive followup event\n");
	kalMemSet(&rFollowInd, 0, sizeof(struct NanFollowupInd));
	rFollowInd.eventID = ENUM_NAN_RECEIVE;
	rFollowInd.publish_subscribe_id = prDiscEvt->publish_subscribe_id;
	rFollowInd.requestor_instance_id = prDiscEvt->requestor_instance_id;
	COPY_MAC_ADDR(rFollowInd.addr, prDiscEvt->addr);
	if (unlikely(prDiscEvt->service_specific_info_len >
		sizeof(rFollowInd.service_specific_info))) {
		DBGLOG(NAN, WARN,
			"service_specific_info len too large: %u > %zu\n",
			prDiscEvt->service_specific_info_len,
			sizeof(rFollowInd.service_specific_info));
		prDiscEvt->service_specific_info_len =
			sizeof(rFollowInd.service_specific_info);
	}
	rFollowInd.service_specific_info_len =
		prDiscEvt->service_specific_info_len;
	kalMemCopy(rFollowInd.service_specific_info,
		   prDiscEvt->service_specific_info,
		   prDiscEvt->service_specific_info_len);
	kalIndicateNetlink2User(prAdapter->prGlueInfo, &rFollowInd,
				sizeof(struct NanFollowupInd));
}

void nicNanRepliedEvnt(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NanPublishRepliedInd rRepliedInd;
	struct NAN_REPLIED_EVENT *prRepliedEvt;

	prRepliedEvt = (struct NAN_REPLIED_EVENT *)pcuEvtBuf;
	kalMemZero(&rRepliedInd, sizeof(struct NanPublishRepliedInd));
	rRepliedInd.eventID = ENUM_NAN_REPLIED;
	rRepliedInd.pubid = prRepliedEvt->u2Pubid;
	rRepliedInd.subid = prRepliedEvt->u2Subid;
	COPY_MAC_ADDR(rRepliedInd.addr, prRepliedEvt->auAddr);
	kalIndicateNetlink2User(prAdapter->prGlueInfo, &rRepliedInd,
				sizeof(struct NanPublishRepliedInd));
}

void nicNanPublishTerminateEvt(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NanPublishTerminatedInd rPubTerminatEvt;
	struct NAN_PUBLISH_TERMINATE_EVENT *prPubTerEvt;

	prPubTerEvt = (struct NAN_PUBLISH_TERMINATE_EVENT *)pcuEvtBuf;
	kalMemZero(&rPubTerminatEvt, sizeof(struct NanPublishTerminatedInd));
	rPubTerminatEvt.eventID = ENUM_NAN_PUB_TERMINATE;
	rPubTerminatEvt.publish_id = prPubTerEvt->u2Pubid;
	kalIndicateNetlink2User(prAdapter->prGlueInfo, &rPubTerminatEvt,
				sizeof(struct NanPublishTerminatedInd));
}

void nicNanEventSTATxCTL(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct EVENT_UPDATE_NAN_TX_STATUS *prUpdateTxStatus;

	prUpdateTxStatus = (struct EVENT_UPDATE_NAN_TX_STATUS *)pcuEvtBuf;
	qmUpdateFreeNANQouta(prAdapter, prUpdateTxStatus->aucFlowCtrl);
}

void nicNanSubscribeTerminateEvt(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NanSubscribeTerminatedInd rSubTerminatEvt;
	struct NAN_SUBSCRIBE_TERMINATE_EVENT *pSubTerEvt;

	pSubTerEvt = (struct NAN_SUBSCRIBE_TERMINATE_EVENT *)pcuEvtBuf;
	kalMemZero(&rSubTerminatEvt, sizeof(struct NanPublishTerminatedInd));
	rSubTerminatEvt.eventID = ENUM_NAN_SUB_TERMINATE;
	rSubTerminatEvt.subscribe_id = pSubTerEvt->u2Subid;
	kalIndicateNetlink2User(prAdapter->prGlueInfo, &rSubTerminatEvt,
				sizeof(struct NanSubscribeTerminatedInd));
}

#if CFG_SUPPORT_NAN_ADVANCE_DATA_CONTROL
void nicNanNdlFlowCtrlEvt(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NAN_EVT_NDL_FLOW_CTRL *prFlowCtrlEvt;
	struct STA_RECORD *prStaRec;
	uint16_t u2SchId = 0;
	uint32_t u4Idx;
	unsigned char fgNeedToSendPkt = FALSE;
	OS_SYSTIME rCurrentTime;
	OS_SYSTIME rExpiryTime;

	prFlowCtrlEvt = (struct NAN_EVT_NDL_FLOW_CTRL *)pcuEvtBuf;
	for (u2SchId = 0; u2SchId < NAN_MAX_CONN_CFG; u2SchId++) {
		uint8_t ucStaIdx;
		uint16_t u2SlotTime;

		if (nanSchedPeerSchRecordIsValid(prAdapter, u2SchId) == FALSE)
			continue;

		rCurrentTime = kalGetTimeTick();
		u2SlotTime = prFlowCtrlEvt->au2FlowCtrl[u2SchId];
		rExpiryTime =
			rCurrentTime + u2SlotTime * NAN_SEND_PKT_TIME_SLOT;

		DBGLOG(NAN, LOUD,
		       "[NDL flow control] Sch:%u, Expiry:%u, Slot:%u\n",
		       u2SchId, rExpiryTime, u2SlotTime);

		if (u2SlotTime == 0)
			continue;

		rExpiryTime -= NAN_SEND_PKT_TIME_GUARD_TIME;
		for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
			ucStaIdx = nanSchedQueryStaRecIdx(prAdapter, u2SchId,
				u4Idx, NAN_MAIN_LINK_INDEX);
			if (ucStaIdx == STA_REC_INDEX_NOT_FOUND)
				continue;

			prStaRec = &prAdapter->arStaRec[ucStaIdx];
			prStaRec->rNanExpiredSendTime = rExpiryTime;

			if (prStaRec->fgNanSendTimeExpired)
				fgNeedToSendPkt = TRUE;
		}
	}

	if (fgNeedToSendPkt == TRUE &&
	    wlanGetTxPendingFrameCount(prAdapter) > 0) {
		DBGLOG(NAN, LOUD, "Trigger NAN tx request\n");
		kalSetEvent(prAdapter->prGlueInfo);
	}
}

static void nanSetTxAllowedByFlowCtrl(struct ADAPTER *prAdapter,
				 struct STA_RECORD *prStaRec,
				 OS_SYSTIME rExpiryTime)
{
	struct STA_RECORD *prSta;
#if (CFG_SUPPORT_NAN_11BE_MLO == 1)
	struct MLD_STA_RECORD *prMldStaRec;
	struct LINK *prStarecList = NULL;

	prMldStaRec = mldStarecGetByStarec(prAdapter, prStaRec);
	if (prMldStaRec)
		prStarecList = &prMldStaRec->rStarecList;

	if (!prStarecList) {
#endif
		prSta = prStaRec;
		prSta->rNanExpiredSendTime = rExpiryTime;
		if (prSta->fgNanSendTimeExpired) {
			prSta->fgNanSendTimeExpired = FALSE;
			DBGLOG(NAN, TRACE, "Trigger NAN tx request starec=%u\n",
			       prSta->ucIndex);
			qmSetStaRecTxAllowed(prAdapter, prSta, TRUE);
		}
		return;
#if (CFG_SUPPORT_NAN_11BE_MLO == 1)
	}

	/* prStarecList */
	LINK_FOR_EACH_ENTRY(prSta, prStarecList,
			    rLinkEntryMld, struct STA_RECORD) {
		prSta->rNanExpiredSendTime = rExpiryTime;
		if (prSta->fgNanSendTimeExpired) {
			prSta->fgNanSendTimeExpired = FALSE;
			DBGLOG(NAN, TRACE, "Trigger NAN tx request starec=%u\n",
			       prSta->ucIndex);
			qmSetStaRecTxAllowed(prAdapter, prSta, TRUE);
		}
	}
#endif
}

/**
 * For Rm: values > 0:
 * 1. set prStaRec->fgNanSendTimeExpired = FALSE
 * 2. qmSetStaRecTxAllowed(TRUE) for the corresponding prStaRec(s)
 * 3. kalSetEvent(prAdapter->prGlueInfo) to Wakeup TX to flush pending packets
 *
 * See also:
 * updateNanStaRecTxAllowed, the allow case
 * called by nicTxDirectStartXmitMain on processing each packet
 * 1. set prStaRec->fgNanSendTimeExpired = FALSE
 *
 * updateNanStaRecTxAllowed, the disallow case
 * called by nicTxDirectStartXmitMain on processing each packet
 * 1. set prStaRec->fgNanSendTimeExpired = TRUE
 * 2. qmSetStaRecTxAllowed(FALSE)
 *
 * nanIsSendTimeExpired
 * called by qmDequeueTxPacketsFromPerStaQueues on processing each packet
 * 1. set prStaRec->fgNanSendTimeExpired = TRUE
 * 1. set prStaRec->fgNanSendTimeExpired = FALSE
 */
void nicNanNdlFlowCtrlEvtV2(struct ADAPTER *prAdapter, uint8_t *pcuEvtBuf)
{
	struct NAN_EVT_NDL_FLOW_CTRL_V2 *prFlowCtrlEvt;
	struct STA_RECORD *prStaRec;
	uint16_t u2SchId = 0;
	uint16_t u2SeqNum;
	uint32_t u4Idx;
	uint32_t u4NanSendPacketGuardTime;
	struct NAN_FLOW_CTRL *prNanFlowCtrlRecord;
	OS_SYSTIME rCurrentTime;
	OS_SYSTIME rExpiryTime;

	KAL_SPIN_LOCK_DECLARATION();

	u4NanSendPacketGuardTime = prAdapter->rWifiVar.u4NanSendPacketGuardTime;
	prFlowCtrlEvt = (struct NAN_EVT_NDL_FLOW_CTRL_V2 *)pcuEvtBuf;
	u2SeqNum = prFlowCtrlEvt->u2SeqNum;

	for (u2SchId = 0; u2SchId < NAN_MAX_CONN_CFG; u2SchId++) {
		uint8_t ucStaIdx;
		uint16_t u2RemainingTime;
		uint32_t u4OpClass = 0;
		uint32_t u4PrimaryChnl = 0;

		if (nanSchedPeerSchRecordIsValid(prAdapter, u2SchId) == FALSE)
			continue;

		u4OpClass =
			prFlowCtrlEvt->arBandChnlInfo[u2SchId].u4OperatingClass;
		u4PrimaryChnl =
			prFlowCtrlEvt->arBandChnlInfo[u2SchId].u4PrimaryChnl;
		if (IS_2G_OP_CLASS(u4OpClass) && !nanLinkNeedMlo(prAdapter) &&
		    nanSchedGetHighestCommonBand(prAdapter, u2SchId, FALSE) !=
		    ENUM_SUPPORTED_BN_2G) {
			DBGLOG(NAN, DEBUG,
				   "Seq:%u, Sch:%u, Rm:%u, Op:%u, ch=%u, 5/6G peer skip 2G flow ctrl\n",
				   u2SeqNum, u2SchId,
				   prFlowCtrlEvt->au2RemainingTime[u2SchId],
				   u4OpClass, u4PrimaryChnl);
			continue;
		}

		prNanFlowCtrlRecord = nanSchedGetPeerSchRecFlowCtrl(prAdapter,
								    u2SchId);
		rCurrentTime = kalGetTimeTick();
		u2RemainingTime = prFlowCtrlEvt->au2RemainingTime[u2SchId];
		rExpiryTime = rCurrentTime + u2RemainingTime;

		DBGLOG(NAN, DEBUG,
		       "Seq:%u, Sch:%u, Stop=%3u, Rm:%u, Op=%u, ch=%u",
		       u2SeqNum, u2SchId,
		       rCurrentTime - prNanFlowCtrlRecord[u2SchId].u4ExpiryTime,
		       u2RemainingTime,
		       u4OpClass, u4PrimaryChnl);

		prNanFlowCtrlRecord[u2SchId].u4ExpiryTime = rExpiryTime;

		if (u2RemainingTime == 0)
			continue;

		rExpiryTime -= u4NanSendPacketGuardTime;
		for (u4Idx = 0; u4Idx < NAN_MAX_SUPPORT_NDP_CXT_NUM; u4Idx++) {
			ucStaIdx = nanSchedQueryStaRecIdx(prAdapter, u2SchId,
				u4Idx, nanGetLinkIndexbyOpClass(prAdapter,
				u4OpClass));
			if (ucStaIdx == STA_REC_INDEX_NOT_FOUND)
				continue;

			KAL_ACQUIRE_SPIN_LOCK(prAdapter,
				SPIN_LOCK_NAN_NDL_FLOW_CTRL);

			prStaRec = &prAdapter->arStaRec[ucStaIdx];

			nanSetTxAllowedByFlowCtrl(prAdapter, prStaRec,
						  rExpiryTime);

			KAL_RELEASE_SPIN_LOCK(prAdapter,
					SPIN_LOCK_NAN_NDL_FLOW_CTRL);
		}

		kalSetEvent(prAdapter->prGlueInfo); /* Wakeup TX */
	}
}
#endif

void nicNanIOEventHandler(struct ADAPTER *prAdapter,
		     struct WIFI_EVENT *prEvent)
{
	uint8_t *pucEvtBody = nanGetEventBody(prEvent);
	uint32_t u4SubEvent;

	u4SubEvent = nanGetEventTag(pucEvtBody);
	if (u4SubEvent >= NAN_EVENT_NUM)
		return;

	DBGLOG(NAN, DEBUG, "subEvent:%d\n", u4SubEvent);

	if (prAdapter->fgIsNANRegistered == FALSE) {
		DBGLOG(NAN, ERROR, "NAN is unregistered\n");
		return;
	}

	switch (u4SubEvent) {
	case NAN_EVENT_DISCOVERY_RESULT:
		nicNanEventDiscoveryResult(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_FOLLOW_EVENT:
		nicNanReceiveEvent(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_REPLIED_EVENT:
		nicNanRepliedEvnt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_PUBLISH_TERMINATE_EVENT:
		nicNanPublishTerminateEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_SUBSCRIBE_TERMINATE_EVENT:
		nicNanSubscribeTerminateEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_MASTER_IND_ATTR:
		nanDevMasterIndEvtHandler(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_CLUSTER_ID_UPDATE:
		nanDevClusterIdEvtHandler(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_SCHEDULE_CONFIG:
	case NAN_EVENT_ID_PEER_AVAILABILITY:
	case NAN_EVENT_ID_PEER_CAPABILITY:
	case NAN_EVENT_ID_CRB_HANDSHAKE_TOKEN:
	case NAN_EVENT_ID_DEVICE_CAPABILITY:
	case NAN_EVENT_UPDATE_LOCAL_ULW:
		nanSchedulerEventDispatch(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_PEER_SEC_CONTEXT_INFO:
		nanDiscUpdateSecContextInfoAttr(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_PEER_CIPHER_SUITE_INFO:
		nanDiscUpdateCipherSuiteInfoAttr(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_DATA_NOTIFY:
		nicNanEventSTATxCTL(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_FTM_DONE:
		nanRangingFtmDoneEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_RANGING_BY_DISC:
		nanRangingInvokedByDiscEvt(prAdapter, pucEvtBody);
		break;
#if CFG_SUPPORT_NAN_ADVANCE_DATA_CONTROL
	case NAN_EVENT_NDL_FLOW_CTRL:
		nicNanNdlFlowCtrlEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_NDL_FLOW_CTRL_V2:
		nicNanNdlFlowCtrlEvtV2(prAdapter, pucEvtBody);
		break;
#endif
	case NAN_EVENT_NDL_DISCONNECT:
		nanDataEngingDisconnectEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_DEVICE_INFO:
		break;
	case NAN_EVENT_LOWPOWER_CTRL:
		mtk_cfg80211_vendor_event_nan_lowpower_ctrl(prAdapter,
							    pucEvtBody);
		break;
	case NAN_EVENT_RANGING_CTRL:
		nanRangingCtrlEvt(prAdapter, pucEvtBody);
		break;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	case NAN_EVENT_REPORT_PACKET_NUM:
		nanSecUpdatePacketNumEvt(prAdapter, pucEvtBody);
		break;
#endif
	}
}

void nicNanVendorEventHandler(struct ADAPTER *prAdapter,
			 struct WIFI_EVENT *prEvent)
{
	uint8_t *pucEvtBody = nanGetEventBody(prEvent);
	uint32_t u4SubEvent;
	int status = 0;

	TRACE_FUNC(NAN, TRACE, "%s IN, Guiding to Vendor event handler\n");

	u4SubEvent = nanGetEventTag(pucEvtBody);
	if (u4SubEvent >= NAN_EVENT_NUM)
		return;

	if (u4SubEvent != NAN_EVENT_NDL_FLOW_CTRL_V2) {
		DBGLOG(NAN, DEBUG, "subEvent:%d (%s)\n", u4SubEvent,
				nan_subevent_str(u4SubEvent));
	}

	if (prAdapter->fgIsNANRegistered == FALSE) {
		DBGLOG(NAN, ERROR,
			"kalNanHandleVendorEvent, NAN is unregistered\n");
		return;
	}

	switch (u4SubEvent) {
	case NAN_EVENT_ID_DE_EVENT_IND:
		status = mtk_cfg80211_vendor_event_nan_event_indication(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_DISCOVERY_RESULT:
		status = mtk_cfg80211_vendor_event_nan_match_indication(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_FOLLOW_EVENT:
		status = mtk_cfg80211_vendor_event_nan_followup_indication(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_REPLIED_EVENT:
		status = mtk_cfg80211_vendor_event_nan_replied_indication(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_PUBLISH_TERMINATE_EVENT:
		status = mtk_cfg80211_vendor_event_nan_publish_terminate(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_SUBSCRIBE_TERMINATE_EVENT:
		status = mtk_cfg80211_vendor_event_nan_subscribe_terminate(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_SELF_FOLLOW_EVENT:
		status = mtk_cfg80211_vendor_event_nan_selfflwup_indication(
			prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_MASTER_IND_ATTR:
		nanDevMasterIndEvtHandler(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_CLUSTER_ID_UPDATE:
		nanDevClusterIdEvtHandler(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_SCHEDULE_CONFIG:
		status = mtk_cfg80211_vendor_event_nan_schedule_config(
			prAdapter, pucEvtBody);
		kal_fallthrough;
	case NAN_EVENT_ID_PEER_AVAILABILITY:
	case NAN_EVENT_ID_PEER_CAPABILITY:
	case NAN_EVENT_ID_CRB_HANDSHAKE_TOKEN:
	case NAN_EVENT_ID_DEVICE_CAPABILITY:
		nanSchedulerEventDispatch(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_PEER_SEC_CONTEXT_INFO:
		nanDiscUpdateSecContextInfoAttr(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_PEER_CIPHER_SUITE_INFO:
		nanDiscUpdateCipherSuiteInfoAttr(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_ID_DATA_NOTIFY:
		nicNanEventSTATxCTL(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_FTM_DONE:
		nanRangingFtmDoneEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_RANGING_BY_DISC:
		nanRangingInvokedByDiscEvt(prAdapter, pucEvtBody);
		break;
#if CFG_SUPPORT_NAN_ADVANCE_DATA_CONTROL
	case NAN_EVENT_NDL_FLOW_CTRL:
		nicNanNdlFlowCtrlEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_NDL_FLOW_CTRL_V2:
		nicNanNdlFlowCtrlEvtV2(prAdapter, pucEvtBody);
		break;
#endif
	case NAN_EVENT_NDL_DISCONNECT:
		nanDataEngingDisconnectEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_DISABLE_IND:
		mtk_cfg80211_vendor_event_nan_disable_indication(prAdapter,
								 pucEvtBody);
		break;
#if CFG_SUPPORT_NAN_SERVICE_EXPIRED
	case NAN_EVENT_MATCH_EXPIRE:
		mtk_cfg80211_vendor_event_nan_match_expire(prAdapter,
							   pucEvtBody);
		break;
#endif

#if CFG_SUPPORT_NAN_FAST_DISC
	case NAN_EVENT_DISC_BCN_PERIOD:
		nanDevDiscBcnPeriodEvtHandler(prAdapter, pucEvtBody);
		break;
#endif
	case NAN_EVENT_DFSP_CSA:
	case NAN_EVENT_DFSP_CSA_COMPLETE:
	case NAN_EVENT_DFSP_SUSPEND_RESUME:
	case NAN_EVENT_REPORT_DW_START:
	case NAN_EVENT_REPORT_DW_END:
	case NAN_EVENT_DEVICE_ROLE:
		status = nanExtEventHandler(prAdapter, pucEvtBody);
		break;

	case NAN_EVENT_UPDATE_LOCAL_ULW:
		nanSchedEventUpdateLocalUlw(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_REPORT_BEACON:
		mtk_cfg80211_vendor_event_nan_report_beacon(prAdapter,
							    pucEvtBody);
		break;
	case NAN_EVENT_SLOT_STATISTICS:
		nicNanSlotStatisticsEvt(prAdapter, pucEvtBody);
		break;
	case NAN_EVENT_LOWPOWER_CTRL:
		mtk_cfg80211_vendor_event_nan_lowpower_ctrl(prAdapter,
							    pucEvtBody);
		break;
	case NAN_EVENT_RANGING_CTRL:
		nanRangingCtrlEvt(prAdapter, pucEvtBody);
		break;
#if (CFG_NAN_SUPPORT_R4_GROUP_ADDR_FRAME_PROT == 1)
	case NAN_EVENT_REPORT_PACKET_NUM:
		nanSecUpdatePacketNumEvt(prAdapter, pucEvtBody);
		break;
#endif
	default:
		DBGLOG(NAN, LOUD, "No match event!!\n");
		break;
	}
}

#endif /* CFG_SUPPORT_NAN */
