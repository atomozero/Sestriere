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
SQLITE_TESTS="test_error_handling test_phase1_fixes test_phase2_fixes test_phase3_fixes test_security"

for src in test_*.cpp; do
	name="${src%.cpp}"

	# Optional filter
	if [ -n "$1" ] && echo "$name" | grep -qv "$1"; then
		continue
	fi

	# Determine libraries
	LIBS=""
	case "$SQLITE_TESTS" in
		*"$name"*) LIBS="-lsqlite3" ;;
	esac

	# Build
	if ! g++ -o "$name" "$src" -I../ $LIBS -lm 2>/dev/null; then
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
