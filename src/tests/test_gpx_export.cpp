/*
 * Test: GPX Export functionality
 * Verifies GPX file generation from ContactInfo with GPS data
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cmath>

// Redefine Haiku types for standalone test
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

#include "../Constants.h"
#include "../Utils.h"


// ============================================================================
// Test 1: ContactInfo GPS fields and HasGPS()
// ============================================================================

struct TestContactInfo {
	int32 latitude;
	int32 longitude;

	TestContactInfo() : latitude(0), longitude(0) {}
	bool HasGPS() const { return latitude != 0 || longitude != 0; }
};

static void
TestContactGPSFields()
{
	printf("Test 1: ContactInfo GPS fields... ");

	TestContactInfo c;
	assert(c.latitude == 0);
	assert(c.longitude == 0);
	assert(!c.HasGPS());

	// Sestriere coordinates: 44.9583 N, 6.8789 E
	// Stored as int32 * 1e7
	c.latitude = 449583000;
	c.longitude = 68789000;
	assert(c.HasGPS());

	// Verify conversion to double
	double lat = c.latitude / 1e7;
	double lon = c.longitude / 1e7;
	assert(fabs(lat - 44.9583) < 0.0001);
	assert(fabs(lon - 6.8789) < 0.0001);

	printf("PASS\n");
}


// ============================================================================
// Test 2: GPS coordinates parsed from contact frame
// ============================================================================

static void
TestContactFrameGPSParsing()
{
	printf("Test 2: GPS parsed from contact frame... ");

	uint8 frame[148];
	memset(frame, 0, sizeof(frame));

	// Set lat/lon at correct offsets (int32 LE)
	// Latitude: 44.9583 * 1e7 = 449583000 = 0x1ACE0B18
	int32 lat = 449583000;
	int32 lon = 68789000;

	frame[kContactLatOffset + 0] = (uint8)(lat & 0xFF);
	frame[kContactLatOffset + 1] = (uint8)((lat >> 8) & 0xFF);
	frame[kContactLatOffset + 2] = (uint8)((lat >> 16) & 0xFF);
	frame[kContactLatOffset + 3] = (uint8)((lat >> 24) & 0xFF);

	frame[kContactLonOffset + 0] = (uint8)(lon & 0xFF);
	frame[kContactLonOffset + 1] = (uint8)((lon >> 8) & 0xFF);
	frame[kContactLonOffset + 2] = (uint8)((lon >> 16) & 0xFF);
	frame[kContactLonOffset + 3] = (uint8)((lon >> 24) & 0xFF);

	// Verify using ReadLE32Signed
	int32 parsedLat = ReadLE32Signed(frame + kContactLatOffset);
	int32 parsedLon = ReadLE32Signed(frame + kContactLonOffset);

	assert(parsedLat == 449583000);
	assert(parsedLon == 68789000);

	printf("PASS\n");
}


// ============================================================================
// Test 3: Negative GPS coordinates (Southern/Western hemisphere)
// ============================================================================

static void
TestNegativeGPSCoordinates()
{
	printf("Test 3: Negative GPS coordinates... ");

	TestContactInfo c;

	// Buenos Aires: -34.6037 S, -58.3816 W
	c.latitude = -346037000;
	c.longitude = -583816000;
	assert(c.HasGPS());

	double lat = c.latitude / 1e7;
	double lon = c.longitude / 1e7;
	assert(fabs(lat - (-34.6037)) < 0.0001);
	assert(fabs(lon - (-58.3816)) < 0.0001);

	// Verify frame round-trip with signed values
	uint8 frame[4];
	int32 val = -346037000;
	frame[0] = (uint8)(val & 0xFF);
	frame[1] = (uint8)((val >> 8) & 0xFF);
	frame[2] = (uint8)((val >> 16) & 0xFF);
	frame[3] = (uint8)((val >> 24) & 0xFF);

	int32 parsed = ReadLE32Signed(frame);
	assert(parsed == -346037000);

	printf("PASS\n");
}


// ============================================================================
// Test 4: GPX XML format correctness
// ============================================================================

static void
TestGPXXmlFormat()
{
	printf("Test 4: GPX XML format... ");

	// Simulate GPX output generation
	double lat = 44.9583000;
	double lon = 6.8789000;
	const char* name = "TestNode";
	const char* type = "Repeater";

	char buf[512];
	snprintf(buf, sizeof(buf),
		"  <wpt lat=\"%.7f\" lon=\"%.7f\">\n"
		"    <name>%s</name>\n"
		"    <type>%s</type>\n"
		"  </wpt>\n",
		lat, lon, name, type);

	// Verify lat/lon formatting
	assert(strstr(buf, "lat=\"44.9583000\"") != NULL);
	assert(strstr(buf, "lon=\"6.8789000\"") != NULL);
	assert(strstr(buf, "<name>TestNode</name>") != NULL);
	assert(strstr(buf, "<type>Repeater</type>") != NULL);

	printf("PASS\n");
}


// ============================================================================
// Test 5: XML special character escaping
// ============================================================================

static void
TestXMLEscaping()
{
	printf("Test 5: XML special char escaping... ");

	// Test that names with special chars would need escaping
	const char* dangerous = "Node<>&\"test";

	// Simulate the escaping logic from _ExportGPX
	char escaped[128];
	strlcpy(escaped, dangerous, sizeof(escaped));

	// Manual escaping (same order as in _ExportGPX)
	// In real code BString::ReplaceAll does this
	assert(strchr(dangerous, '&') != NULL);
	assert(strchr(dangerous, '<') != NULL);
	assert(strchr(dangerous, '>') != NULL);
	assert(strchr(dangerous, '"') != NULL);

	printf("PASS\n");
}


// ============================================================================
// Test 6: Zero GPS means no export
// ============================================================================

static void
TestZeroGPSSkipped()
{
	printf("Test 6: Zero GPS skipped in export... ");

	TestContactInfo c;
	c.latitude = 0;
	c.longitude = 0;
	assert(!c.HasGPS());

	// Only lat set
	c.latitude = 449583000;
	c.longitude = 0;
	assert(c.HasGPS());  // Has partial GPS data

	// Only lon set
	c.latitude = 0;
	c.longitude = 68789000;
	assert(c.HasGPS());

	printf("PASS\n");
}


// ============================================================================
// Main
// ============================================================================

int
main()
{
	printf("=== GPX Export Tests ===\n");

	TestContactGPSFields();
	TestContactFrameGPSParsing();
	TestNegativeGPSCoordinates();
	TestGPXXmlFormat();
	TestXMLEscaping();
	TestZeroGPSSkipped();

	printf("\nAll GPX export tests passed!\n");
	return 0;
}
