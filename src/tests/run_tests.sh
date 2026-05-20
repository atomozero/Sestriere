#!/bin/sh
#
# run_tests.sh — Build and run all Sestriere test programs
#
# Usage: ./run_tests.sh [pattern]
#   pattern: optional filter, e.g. "phase" to run only test_phase* tests
#

cd "$(dirname "$0")"

PASS=0
FAIL=0
SKIP=0
ERRORS=""

# Tests requiring sqlite3
SQLITE_TESTS="test_error_handling test_phase1_fixes test_phase2_fixes test_phase3_fixes test_security test_message_db"

for src in test_*.cpp; do
	name="${src%.cpp}"

	# Optional filter
	if [ -n "$1" ] && echo "$name" | grep -qv "$1"; then
		continue
	fi

	# Determine libraries and extra object files
	LIBS=""
	OBJS=""
	case "$SQLITE_TESTS" in
		*"$name"*) LIBS="-lsqlite3" ;;
	esac
	# Tests requiring -lbe (Haiku API)
	BE_TESTS="test_frame_parser test_media_codec test_compat test_contact_groups test_database_thread_safety test_mqtt_thread_safety test_mute_logic"
	# Tests requiring DatabaseManager.o
	DB_OBJ_TESTS="test_contact_groups test_mute_logic"
	case "$DB_OBJ_TESTS" in
		*"$name"*)
			LIBS="$LIBS -lsqlite3"
			OBJS="$OBJS ../objects.x86_64-cc13-debug/DatabaseManager.o"
			;;
	esac
	case "$BE_TESTS" in
		*"$name"*) LIBS="$LIBS -lbe" ;;
	esac
	case "$name" in
		test_frame_parser)
			OBJS="../objects.x86_64-cc13-debug/FrameParser.o"
			;;
		test_media_codec)
			OBJS="../objects.x86_64-cc13-debug/ImageSession.o ../objects.x86_64-cc13-debug/VoiceSession.o"
			;;
		test_wal_fallback)
			LIBS="$LIBS -lsqlite3"
			;;
		# test_channel_psk requires SHA256.h which doesn't exist
		# in this project — kept as skip
	esac

	# Build
	if ! g++ -o "$name" "$src" $OBJS -I../ $LIBS -lm 2>/dev/null; then
		printf "  SKIP: %-40s (build failed)\n" "$name"
		SKIP=$((SKIP + 1))
		continue
	fi

	# Run
	if ./"$name" > /dev/null 2>&1; then
		printf "  PASS: %s\n" "$name"
		PASS=$((PASS + 1))
	else
		printf "  FAIL: %s\n" "$name"
		FAIL=$((FAIL + 1))
		ERRORS="$ERRORS $name"
	fi
done

echo ""
echo "================================"
printf "Results: %d passed, %d failed, %d skipped\n" "$PASS" "$FAIL" "$SKIP"

if [ -n "$ERRORS" ]; then
	echo "Failed:$ERRORS"
fi

echo "================================"
exit $FAIL
