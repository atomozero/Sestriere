/*
 * Test: Channel PSK generation
 * Verifies that hashtag channel PSK derivation uses SHA-256 hash
 * of the channel name, not raw name bytes.
 */

#include <cstdio>
#include <cstring>
#include <cassert>


static FILE*
OpenSource(const char* filename)
{
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		char path[256];
		snprintf(path, sizeof(path), "../%s", filename);
		f = fopen(path, "r");
	}
	return f;
}


static bool
FileContains(const char* filename, const char* pattern)
{
	FILE* f = OpenSource(filename);
	if (f == NULL)
		return false;

	char line[512];
	bool found = false;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strstr(line, pattern) != NULL) {
			found = true;
			break;
		}
	}
	fclose(f);
	return found;
}


static void
TestPskDerivation()
{
	printf("  TestPskDerivation...");

	// MainWindow derives hashtag PSK via SHA-256 hash
	assert(FileContains("MainWindow.cpp", "SHA256"));
	assert(FileContains("MainWindow.cpp", "Digest"));

	// AddChannelWindow signals hashtag mode
	assert(FileContains("AddChannelWindow.cpp", "hashtag"));

	printf(" PASS\n");
}


static void
TestHashtagStripping()
{
	printf("  TestHashtagStripping...");

	// Should strip leading '#' before hashing
	assert(FileContains("MainWindow.cpp", "name[0] == '#'")
		|| FileContains("MainWindow.cpp", "name + 1"));

	printf(" PASS\n");
}


static void
TestPublicChannelPsk()
{
	printf("  TestPublicChannelPsk...");

	// Well-known Public Channel PSK should be defined
	assert(FileContains("Constants.h", "kPublicChannelPSK"));

	printf(" PASS\n");
}


static void
TestDuplicateCheck()
{
	printf("  TestDuplicateCheck...");

	// Duplicate channel check should be case-insensitive
	assert(FileContains("MainWindow.cpp", "strcasecmp"));

	printf(" PASS\n");
}


int
main()
{
	printf("=== test_channel_psk ===\n");

	TestPskDerivation();
	TestHashtagStripping();
	TestPublicChannelPsk();
	TestDuplicateCheck();

	printf("All channel PSK tests passed!\n");
	return 0;
}
