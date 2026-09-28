/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*
 * SHA256 hash implementation and interface functions
 * Copyright (c) 2003-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef SHA256_H
/* rodin(4-6): wpa_supp 本地 crypto 实现与内核 vmlinux 导出符号（hmac_sha256 等
 * 9 个）单镜像撞名，全族加 wpa_ 前缀改名（调用面全在本驱动内，语义不变） */
#define SHA256_H

#define SHA256_MAC_LEN 32

int hmac_sha256_vector(const u8 *key, size_t key_len, size_t num_elem,
		       const u8 *addr[], const size_t *len, u8 *mac);
int wpa_hmac_sha256(const u8 *key, size_t key_len, const u8 *data,
		size_t data_len, u8 *mac);
int sha256_prf(const u8 *key, size_t key_len, const char *label,
	       const u8 *data, size_t data_len, u8 *buf, size_t buf_len);
int sha256_prf_bits(const u8 *key, size_t key_len, const char *label,
		    const u8 *data, size_t data_len, u8 *buf,
		    size_t buf_len_bits);
int tls_prf_sha256(const u8 *secret, size_t secret_len,
		   const char *label, const u8 *seed, size_t seed_len,
		   u8 *out, size_t outlen);
int hmac_sha256_kdf(const u8 *secret, size_t secret_len,
		    const char *label, const u8 *seed, size_t seed_len,
		    u8 *out, size_t outlen);

#endif /* SHA256_H */
