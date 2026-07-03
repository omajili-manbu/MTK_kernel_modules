// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*
 * SHA-256 hash implementation and interface functions
 * Copyright (c) 2003-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "wpa_supp/FourWayHandShake.h"

#include "wpa_supp/src/crypto/crypto.h"
#include "wpa_supp/src/crypto/sha256.h"
#include "wpa_supp/src/utils/common.h"

/**
 * hmac_sha256_vector - HMAC-SHA256 over data vector (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @num_elem: Number of elements in the data vector
 * @addr: Pointers to the data areas
 * @len: Lengths of the data blocks
 * @mac: Buffer for the hash (32 bytes)
 * Returns: 0 on success, -1 on failure
 */
int
hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
		   const u8 *addr[], const size_t *len, u8 *mac) {
	/*unsigned char k_pad[64];*/
	/* padding - key XORd with ipad/opad zalloc for deadbeef*/
	/*unsigned char tk[32];*/
	const u8 *_addr[6];
	size_t _len[6], i;

	unsigned char *k_pad;
	unsigned char *tk;

	int ret = 0;

	k_pad = os_zalloc(64);

	if (!k_pad) {
		DBGLOG(NAN, ERROR, "k_pad is null!\n");
		return -1;
	}
	tk = os_zalloc(32);

	if (!tk) {
		DBGLOG(NAN, ERROR, "tk is null!\n");
		return -1;
	}

	if (num_elem > 5) {
		/*
		 * Fixed limit on the number of fragments to avoid having to
		 * allocate memory (which could fail).
		 */
		os_free(k_pad);
		os_free(tk);
		return -1;
	}

	/* if key is longer than 64 bytes reset it to key = SHA256(key) */
	if (key_len > 64) {
		if (sha256_vector(1, &key, &key_len, tk) < 0) {
			os_free(k_pad);
			os_free(tk);
			return -1;
		}
		key = tk;
		key_len = 32;
	}

	/* the HMAC_SHA256 transform looks like:
	 *
	 * SHA256(K XOR opad, SHA256(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in ipad */
	/*os_memset(k_pad, 0, sizeof(k_pad));*/
	os_memset(k_pad, 0, 64);
	os_memcpy(k_pad, key, key_len);
	/* XOR key with ipad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x36;

	/* perform inner SHA256 */
	_addr[0] = k_pad;
	_len[0] = 64;
	for (i = 0; i < num_elem; i++) {
		_addr[i + 1] = addr[i];
		_len[i + 1] = len[i];
	}
	if (sha256_vector(1 + num_elem, _addr, _len, mac) < 0) {
		os_free(k_pad);
		os_free(tk);
		return -1;
	}
	/*os_memset(k_pad, 0, sizeof(k_pad));*/
	os_memset(k_pad, 0, 64);
	os_memcpy(k_pad, key, key_len);
	/* XOR key with opad values */
	for (i = 0; i < 64; i++)
		k_pad[i] ^= 0x5c;

	/* perform outer SHA256 */
	_addr[0] = k_pad;
	_len[0] = 64;
	_addr[1] = mac;
	_len[1] = SHA256_MAC_LEN;

	ret = sha256_vector(2, _addr, _len, mac);
	os_free(k_pad);
	os_free(tk);

	return ret;

	/*return sha256_vector(2, _addr, _len, mac);*/
}

/**
 * hmac_sha256 - HMAC-SHA256 over data buffer (RFC 2104)
 * @key: Key for HMAC operations
 * @key_len: Length of the key in bytes
 * @data: Pointers to the data area
 * @data_len: Length of the data area
 * @mac: Buffer for the hash (32 bytes)
 * Returns: 0 on success, -1 on failure
 */
int
hmac_sha256(const u8 *key, size_t key_len, const u8 *data, size_t data_len,
	    u8 *mac) {
	return hmac_sha256_vector(key, key_len, 1, &data, &data_len, mac);
}

/**
 * calculate_pmkid - calculate pmkid by HMAC-SHA256
 * @key: PMK Key
 * @IMAC: I-NDI
 * @RMAC: R-NDI
 * @serviceName: service name
 * @serviceHash: service hash
 * @pmkid: Buffer for the pmkid (32 bytes)
 * Returns: void
 */
void calculate_pmkid(u8 *key, u8 *IMAC, u8 *RMAC,
	u8 *serviceName, u8 *serviceHash, u8 *pmkid)
{
	const char pmkName[] = "NAN PMK Name";
	struct nan_rdf_sha256_state r_SHA_256_state = {0};
	u8  auc_tk[32] = {0};
	u8 aucServiceID[6] = {0};
	int pmkIdSrcLen = (int)(kalStrnLen(pmkName, sizeof(pmkName))+6+6+6);
	u8 *pmkIdSrc = os_malloc((size_t)pmkIdSrcLen);
	size_t i = 0;
	ptrdiff_t post = 0;

	os_memset(auc_tk, 0, sizeof(auc_tk));

	if (!pmkIdSrc) {
		DBGLOG(NAN, ERROR, "pmkIdSrc is null!\n");
		return;
	}

	/* use provided serviceHash or calculate hash by serviceName */
	if (serviceHash != NULL) {
		kalMemCpyS(aucServiceID, sizeof(aucServiceID), serviceHash, 6);
	} else {
		for (i = 0; i < NAN_MAX_SERVICE_NAME_LEN; i++) {
			if (serviceName[i] == '\0')
				break;
			if ((serviceName[i] >= 'A') && (serviceName[i] <= 'Z'))
				serviceName[i] = (u8)(serviceName[i] + 32);
		}
		nan_rdf_sha256_init(&r_SHA_256_state);
		sha256_process(&r_SHA_256_state, serviceName, i);
		sha256_done(&r_SHA_256_state, auc_tk);
		kalMemCpyS(aucServiceID, sizeof(aucServiceID), auc_tk, 6);
	}

	kalMemCpyS(pmkIdSrc,
		(size_t)pmkIdSrcLen,
		pmkName,
		kalStrnLen(pmkName, sizeof(pmkName)));
	post += (ptrdiff_t)kalStrnLen(pmkName, sizeof(pmkName));
	kalMemCpyS(pmkIdSrc+post,
		(size_t)pmkIdSrcLen - (size_t)post,
		IMAC, 6);
	post += 6;
	kalMemCpyS(pmkIdSrc+post,
		(size_t)pmkIdSrcLen - (size_t)post,
		RMAC, 6);
	post += 6;
	kalMemCpyS(pmkIdSrc+post,
		(size_t)pmkIdSrcLen - (size_t)post,
		aucServiceID, 6);
	hmac_sha256(key, 32, pmkIdSrc, (size_t)pmkIdSrcLen, pmkid);
	os_free(pmkIdSrc);
}

