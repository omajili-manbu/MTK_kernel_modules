// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*
 * AES Key Wrap Algorithm (RFC3394)
 *
 * Copyright (c) 2003-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "wpa_supp/FourWayHandShake.h"

#include "wpa_supp/src/utils/common.h"
#include "wpa_supp/src/crypto/aes.h"

/**
 * aes_wrap - Wrap keys with AES Key Wrap Algorithm (RFC3394)
 * @kek: Key encryption key (KEK)
 * @kek_len: Length of KEK in octets
 * @n: Length of the plaintext key in 64-bit units; e.g., 2 = 128-bit = 16
 * bytes
 * @plain: Plaintext key to be wrapped, n * 64 bits
 * @cipher: Wrapped key, (n + 1) * 64 bits
 * Returns: 0 on success, -1 on failure
 */
int
aes_wrap(const u8 *kek, size_t kek_len, int n,
	const u8 *plain, u8 *cipher, size_t cipher_len)
{
	u8 *a = NULL, *r = NULL, b[AES_BLOCK_SIZE] = {0};
	int i = 0, j = 0;
	void *ctx = NULL;
	u32 t = 0;
	size_t r_len = 0;

	a = cipher;
	r = cipher + 8;
	r_len = cipher_len - 8;

	/* 1) Initialize variables. */
	os_memset(a, 0xa6, 8);
	kalMemCpyS(r, r_len, plain, (size_t)(8 * n));

	ctx = aes_encrypt_init(kek, kek_len);
	if (ctx == NULL)
		return -1;

	/* 2) Calculate intermediate values.
	 * For j = 0 to 5
	 *     For i=1 to n
	 *         B = AES(K, A | R[i])
	 *         A = MSB(64, B) ^ t where t = (n*j)+i
	 *         R[i] = LSB(64, B)
	 */
	for (j = 0; j <= 5; j++) {
		r = cipher + 8;
		r_len = cipher_len - 8;
		for (i = 1; i <= n; i++) {
			kalMemCpyS(b, AES_BLOCK_SIZE, a, 8);
			kalMemCpyS(b + 8, AES_BLOCK_SIZE - 8, r, 8);
			aes_encrypt(ctx, b, b);
			kalMemCpyS(a, cipher_len, b, 8);
			t = (u32)(n * j + i);
			a[7] ^= (u8)t;
			a[6] ^= (u8)(t >> 8);
			a[5] ^= (u8)(t >> 16);
			a[4] ^= (u8)(t >> 24);
			kalMemCpyS(r, r_len, b + 8, 8);
			r += 8;
			r_len -= 8;
		}
	}
	aes_encrypt_deinit(ctx);

	/* 3) Output the results.
	 *
	 * These are already in @cipher due to the location of temporary
	 * variables.
	 */

	return 0;
}

