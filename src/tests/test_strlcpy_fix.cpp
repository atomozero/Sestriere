/*
 * test_strlcpy_fix.cpp — Verify strncpy replaced with strlcpy
 * in MissionControlWindow topo node label truncation.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int gPassed = 0;
static int gFailed = 0;

static void Check(bool cond, const char* name) {
	if (cond) {
		printf("  PASS: %s\n", name);
		gPassed++;
	} else {
		printf("  FAIL: %s\n", name);
		gFailed++;
	}
}

static bool FileContains(const char* path, const char* needle) {
	FILE* f = fopen(path, "r");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);
	bool found = strstr(buf, needle) != NULL;
	free(buf);
	return found;
}

static int CountOccurrences(const char* path, const char* needle) {
	FILE* f = fopen(path, "r");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc(sz + 1);
	fread(buf, 1, sz, f);
	buf[sz] = 0;
	fclose(f);

	int count = 0;
	const char* p = buf;
	size_t needleLen = strlen(needle);
	while ((p = strstr(p, needle)) != NULL) {
		count++;
		p += needleLen;
	}
	free(buf);
	return count;
}


int main() {
	printf("=== strncpy → strlcpy Fix Test ===\n\n");

	// No strncpy remaining in MissionControlWindow
	Check(CountOccurrences(
		"../MissionControlWindow.cpp", "strncpy") == 0,
		"No strncpy calls remaining in MissionControlWindow.cpp");

	// strlcpy is used instead
	Check(FileContains(
		"../MissionControlWindow.cpp",
		"strlcpy(shortName, fNodes[i].name, sizeof(shortName))"),
		"strlcpy used with sizeof(shortName) for node label");

	// Manual null-termination no longer needed
	Check(!FileContains(
		"../MissionControlWindow.cpp",
		"shortName[11] = '\\0'"),
		"Manual null-termination removed (strlcpy handles it)");

	// No strncpy in entire src/ (ensure codebase-wide)
	printf("\n--- Codebase-wide strncpy check ---\n");

	const char* files[] = {
		"../MainWindow.cpp",
		"../ChatView.cpp",
		"../ProtocolHandler.cpp",
		"../MissionControlWindow.cpp",
		"../NetworkMapWindow.cpp",
		"../PacketAnalyzerWindow.cpp",
		NULL
	};

	bool anyStrncpy = false;
	for (int i = 0; files[i] != NULL; i++) {
		if (CountOccurrences(files[i], "strncpy") > 0) {
			printf("  WARNING: strncpy found in %s\n", files[i]);
			anyStrncpy = true;
		}
	}
	Check(!anyStrncpy,
		"No strncpy in key source files");

	printf("\n=== Results: %d passed, %d failed ===\n",
		gPassed, gFailed);
	return gFailed > 0 ? 1 : 0;
}
