#!/bin/bash
#
# Run the terminadventure test suite.
#
# Every test_*.py in this directory runs in its own app session: the test
# calls harness.launch() at the start and harness.quit() at the end, and
# exits 0 on success. Scripts ending in PASS/FAIL are reported here.
#
# Usage:
#   ./runtests.sh                 run every test
#   ./runtests.sh links           run only tests whose name contains 'links'
#   TERMINADVENTURE_BIN=... ./runtests.sh
#
# Set TERMINADVENTURE_BIN to point at the app binary if it is not the default.

set -u

cd "$(dirname "$0")"

FILTER="${1:-}"

PASS=0
FAIL=0
FAILED=()
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

for t in test_*.py; do
    [ -e "$t" ] || continue
    if [ -n "$FILTER" ] && [[ "$t" != *"$FILTER"* ]]; then
        continue
    fi

    log="$LOG_DIR/${t%.py}.log"
    printf '%-34s ' "$t"
    if timeout 90 python3 "$t" >"$log" 2>&1; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL"
        FAIL=$((FAIL + 1))
        FAILED+=("$t")
    fi
done

echo
echo "passed: $PASS   failed: $FAIL"

if [ "$FAIL" -gt 0 ]; then
    echo "failed tests: ${FAILED[*]}"
    for t in "${FAILED[@]}"; do
        echo "--- tail of ${LOG_DIR}/${t%.py}.log ---"
        tail -n 25 "${LOG_DIR}/${t%.py}.log"
        echo
    done
    exit 1
fi
exit 0
