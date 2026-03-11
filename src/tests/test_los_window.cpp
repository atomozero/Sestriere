/*
 * test_los_window.cpp — Pattern tests for LoSWindow
 *
 * Build: g++ -o test_los_window test_los_window.cpp -I../ -lm
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>


static int sTestsPassed = 0;
static int sTestsFailed = 0;

#define TEST(name) \
	printf("  %-50s ", name); \
	fflush(stdout);

#define PASS() \
	do { printf("[PASS]\n"); sTestsPassed++; } while (0)

#define FAIL(msg) \
	do { printf("[FAIL] %s\n", msg); sTestsFailed++; } while (0)


static bool
FileContains(const char* path, const char* needle)
{
	FILE* f = fopen(path, "r");
	if (f == NULL) return false;

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	char* buf = (char*)malloc(size + 1);
	if (buf == NULL) { fclose(f); return false; }

	fread(buf, 1, size, f);
	buf[size] = '\0';
	fclose(f);

	bool found = strstr(buf, needle) != NULL;
	free(buf);
	return found;
}


// ============================================================================
// Header tests
// ============================================================================

static void
test_header_guard()
{
	TEST("LoSWindow.h: header guard");
	if (FileContains("../LoSWindow.h", "#ifndef LOSWINDOW_H"))
		PASS();
	else
		FAIL("missing header guard");
}

static void
test_header_bwindow_parent()
{
	TEST("LoSWindow.h: inherits BWindow");
	if (FileContains("../LoSWindow.h", "class LoSWindow : public BWindow"))
		PASS();
	else
		FAIL("missing BWindow inheritance");
}

static void
test_header_set_endpoints()
{
	TEST("LoSWindow.h: SetEndpoints method");
	if (FileContains("../LoSWindow.h", "SetEndpoints"))
		PASS();
	else
		FAIL("missing SetEndpoints");
}

static void
test_header_set_frequency()
{
	TEST("LoSWindow.h: SetFrequency method");
	if (FileContains("../LoSWindow.h", "SetFrequency"))
		PASS();
	else
		FAIL("missing SetFrequency");
}

static void
test_header_start_analysis()
{
	TEST("LoSWindow.h: StartAnalysis method");
	if (FileContains("../LoSWindow.h", "StartAnalysis"))
		PASS();
	else
		FAIL("missing StartAnalysis");
}

static void
test_header_quit_requested()
{
	TEST("LoSWindow.h: QuitRequested override");
	if (FileContains("../LoSWindow.h", "QuitRequested"))
		PASS();
	else
		FAIL("missing QuitRequested");
}

static void
test_header_parent_ptr()
{
	TEST("LoSWindow.h: fParent member");
	if (FileContains("../LoSWindow.h", "fParent"))
		PASS();
	else
		FAIL("missing fParent");
}


// ============================================================================
// Implementation tests
// ============================================================================

static void
test_impl_quit_hide()
{
	TEST("LoSWindow.cpp: QuitRequested hides");
	if (FileContains("../LoSWindow.cpp", "Hide()"))
		PASS();
	else
		FAIL("QuitRequested should call Hide()");
}

static void
test_impl_quit_return_false()
{
	TEST("LoSWindow.cpp: QuitRequested returns false");
	if (FileContains("../LoSWindow.cpp", "return false"))
		PASS();
	else
		FAIL("QuitRequested should return false");
}

static void
test_impl_profile_view()
{
	TEST("LoSWindow.cpp: ProfileView class");
	if (FileContains("../LoSWindow.cpp", "class ProfileView"))
		PASS();
	else
		FAIL("missing ProfileView class");
}

static void
test_impl_draw_terrain()
{
	TEST("LoSWindow.cpp: _DrawTerrain method");
	if (FileContains("../LoSWindow.cpp", "_DrawTerrain"))
		PASS();
	else
		FAIL("missing _DrawTerrain");
}

static void
test_impl_draw_los_line()
{
	TEST("LoSWindow.cpp: _DrawLoSLine method");
	if (FileContains("../LoSWindow.cpp", "_DrawLoSLine"))
		PASS();
	else
		FAIL("missing _DrawLoSLine");
}

static void
test_impl_draw_fresnel()
{
	TEST("LoSWindow.cpp: _DrawFresnelZone method");
	if (FileContains("../LoSWindow.cpp", "_DrawFresnelZone"))
		PASS();
	else
		FAIL("missing _DrawFresnelZone");
}

static void
test_impl_theme_aware_colors()
{
	TEST("LoSWindow.cpp: theme-aware colors (ui_color)");
	if (FileContains("../LoSWindow.cpp", "ui_color("))
		PASS();
	else
		FAIL("missing ui_color usage");
}

static void
test_impl_tint_color()
{
	TEST("LoSWindow.cpp: tint_color usage");
	if (FileContains("../LoSWindow.cpp", "tint_color("))
		PASS();
	else
		FAIL("missing tint_color usage");
}

static void
test_impl_fetch_thread()
{
	TEST("LoSWindow.cpp: _FetchThread static method");
	if (FileContains("../LoSWindow.cpp", "_FetchThread"))
		PASS();
	else
		FAIL("missing _FetchThread");
}

static void
test_impl_spawn_thread()
{
	TEST("LoSWindow.cpp: spawn_thread for async fetch");
	if (FileContains("../LoSWindow.cpp", "spawn_thread"))
		PASS();
	else
		FAIL("missing spawn_thread");
}

static void
test_impl_elevation_service()
{
	TEST("LoSWindow.cpp: uses ElevationService");
	if (FileContains("../LoSWindow.cpp", "ElevationService"))
		PASS();
	else
		FAIL("missing ElevationService usage");
}

static void
test_impl_analyze_los()
{
	TEST("LoSWindow.cpp: calls AnalyzeLineOfSight");
	if (FileContains("../LoSWindow.cpp", "AnalyzeLineOfSight"))
		PASS();
	else
		FAIL("missing AnalyzeLineOfSight call");
}

static void
test_impl_center_in_parent()
{
	TEST("LoSWindow.cpp: centers in parent window");
	if (FileContains("../LoSWindow.cpp", "parentFrame"))
		PASS();
	else
		FAIL("missing parent centering");
}

static void
test_impl_draw_grid()
{
	TEST("LoSWindow.cpp: _DrawGrid method");
	if (FileContains("../LoSWindow.cpp", "_DrawGrid"))
		PASS();
	else
		FAIL("missing _DrawGrid");
}

static void
test_impl_draw_axis_labels()
{
	TEST("LoSWindow.cpp: _DrawAxisLabels method");
	if (FileContains("../LoSWindow.cpp", "_DrawAxisLabels"))
		PASS();
	else
		FAIL("missing _DrawAxisLabels");
}


// ============================================================================
// Main
// ============================================================================

int main()
{
	printf("=== LoSWindow Pattern Tests ===\n\n");

	// Header
	test_header_guard();
	test_header_bwindow_parent();
	test_header_set_endpoints();
	test_header_set_frequency();
	test_header_start_analysis();
	test_header_quit_requested();
	test_header_parent_ptr();

	// Implementation
	test_impl_quit_hide();
	test_impl_quit_return_false();
	test_impl_profile_view();
	test_impl_draw_terrain();
	test_impl_draw_los_line();
	test_impl_draw_fresnel();
	test_impl_theme_aware_colors();
	test_impl_tint_color();
	test_impl_fetch_thread();
	test_impl_spawn_thread();
	test_impl_elevation_service();
	test_impl_analyze_los();
	test_impl_center_in_parent();
	test_impl_draw_grid();
	test_impl_draw_axis_labels();

	printf("\n=== Results: %d passed, %d failed ===\n",
		sTestsPassed, sTestsFailed);
	return sTestsFailed > 0 ? 1 : 0;
}
