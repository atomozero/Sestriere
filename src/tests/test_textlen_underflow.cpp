/*
 * test_textlen_underflow.cpp — Verify textLen underflow protection
 *
 * Tests that (length > textOffset) ? (length - textOffset) : 0
 * correctly prevents unsigned integer underflow when length < textOffset.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

static int sFailures = 0;

#define CHECK(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		sFailures++; \
	} else { \
		printf("  OK: %s\n", msg); \
	} \
} while(0)

// Simulate the old (buggy) pattern
static size_t OldTextLen(size_t length, size_t textOffset)
{
	return length - textOffset;  // underflows if length < textOffset
}

// Simulate the new (fixed) pattern
static size_t NewTextLen(size_t length, size_t textOffset)
{
	return (length > textOffset) ? (length - textOffset) : 0;
}

int main()
{
	printf("=== textLen underflow protection tests ===\n\n");

	// Test 1: Normal case — length > textOffset
	printf("Test 1: Normal case (length=100, textOffset=16)\n");
	CHECK(NewTextLen(100, 16) == 84, "textLen should be 84");

	// Test 2: Equal — length == textOffset (empty text)
	printf("Test 2: Equal (length=16, textOffset=16)\n");
	CHECK(NewTextLen(16, 16) == 0, "textLen should be 0 (empty text)");

	// Test 3: Underflow — length < textOffset (the critical bug)
	printf("Test 3: Underflow (length=10, textOffset=16)\n");
	size_t oldResult = OldTextLen(10, 16);
	size_t newResult = NewTextLen(10, 16);
	CHECK(oldResult > 1000000, "old code wraps to huge value (confirms bug)");
	CHECK(newResult == 0, "new code returns 0 (bug fixed)");

	// Test 4: Zero length
	printf("Test 4: Zero length (length=0, textOffset=16)\n");
	CHECK(NewTextLen(0, 16) == 0, "textLen should be 0");

	// Test 5: textOffset=0
	printf("Test 5: textOffset=0 (length=50, textOffset=0)\n");
	CHECK(NewTextLen(50, 0) == 50, "textLen should be 50");

	// Test 6: V3 DM typical (length=20, textOffset=16)
	printf("Test 6: V3 DM typical (length=20, textOffset=16)\n");
	CHECK(NewTextLen(20, 16) == 4, "textLen should be 4");

	// Test 7: V2 DM minimal (length=13, textOffset=13)
	printf("Test 7: V2 DM minimal (length=13, textOffset=13)\n");
	CHECK(NewTextLen(13, 13) == 0, "textLen should be 0");

	// Test 8: Simulate memcpy safety with fixed code
	printf("Test 8: memcpy safety simulation\n");
	{
		uint8_t data[32];
		memset(data, 0x41, sizeof(data));  // fill with 'A'
		size_t length = 10;
		size_t textOffset = 16;
		size_t textLen = (length > textOffset) ? (length - textOffset) : 0;
		char text[256];
		if (textLen > 255) textLen = 255;
		if (textLen > 0) memcpy(text, data + textOffset, textLen);
		text[textLen] = '\0';
		CHECK(textLen == 0, "no memcpy executed for short frame");
		CHECK(text[0] == '\0', "text is empty string");
	}

	printf("\n%s: %d failures\n",
		sFailures == 0 ? "ALL PASSED" : "FAILED", sFailures);
	return sFailures > 0 ? 1 : 0;
}
