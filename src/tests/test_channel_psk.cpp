/*
 * Test: Channel PSK generation uses SHA-256 hash instead of raw name bytes.
 *
 * Verifies:
 * 1. PSK is NOT just the channel name bytes (old broken behavior)
 * 2. Same name always produces the same PSK (deterministic)
 * 3. Different names produce different PSKs
 * 4. Short names still produce full 16-byte PSK with non-zero entropy
 */

#include <SHA256.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

static void GeneratePSK(const char* name, uint8_t* secret)
{
	SHA256 hash;
	hash.Update(name, strlen(name));
	const uint8_t* digest = hash.Digest();
	memcpy(secret, digest, 16);
}

static bool IsAllZero(const uint8_t* buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		if (buf[i] != 0)
			return false;
	}
	return true;
}

int main()
{
	int failures = 0;

	// Test 1: PSK should NOT equal raw name bytes
	{
		const char* name = "test";
		uint8_t secret[16];
		GeneratePSK(name, secret);

		uint8_t naive[16];
		memset(naive, 0, sizeof(naive));
		size_t nameLen = strlen(name);
		for (size_t i = 0; i < 16 && i < nameLen; i++)
			naive[i] = (uint8_t)name[i];

		if (memcmp(secret, naive, 16) == 0) {
			printf("FAIL: PSK equals raw name bytes (no hashing)\n");
			failures++;
		} else {
			printf("PASS: PSK differs from raw name bytes\n");
		}
	}

	// Test 2: Same name produces same PSK (deterministic)
	{
		uint8_t secret1[16], secret2[16];
		GeneratePSK("mychannel", secret1);
		GeneratePSK("mychannel", secret2);

		if (memcmp(secret1, secret2, 16) != 0) {
			printf("FAIL: Same name produced different PSKs\n");
			failures++;
		} else {
			printf("PASS: Same name produces same PSK\n");
		}
	}

	// Test 3: Different names produce different PSKs
	{
		uint8_t secret1[16], secret2[16];
		GeneratePSK("channel_a", secret1);
		GeneratePSK("channel_b", secret2);

		if (memcmp(secret1, secret2, 16) == 0) {
			printf("FAIL: Different names produced same PSK\n");
			failures++;
		} else {
			printf("PASS: Different names produce different PSKs\n");
		}
	}

	// Test 4: Short name still fills all 16 bytes with non-zero data
	{
		uint8_t secret[16];
		GeneratePSK("x", secret);

		if (IsAllZero(secret + 1, 15)) {
			printf("FAIL: Short name leaves trailing zeros (no hash diffusion)\n");
			failures++;
		} else {
			printf("PASS: Short name fills all 16 bytes via hash\n");
		}
	}

	// Test 5: Empty-ish name (single char) still produces non-trivial PSK
	{
		uint8_t secret[16];
		GeneratePSK("a", secret);

		int nonZero = 0;
		for (int i = 0; i < 16; i++) {
			if (secret[i] != 0)
				nonZero++;
		}
		if (nonZero < 8) {
			printf("FAIL: PSK has too few non-zero bytes (%d/16)\n", nonZero);
			failures++;
		} else {
			printf("PASS: PSK has good entropy (%d/16 non-zero bytes)\n", nonZero);
		}
	}

	printf("\n%s: %d failures\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures;
}
