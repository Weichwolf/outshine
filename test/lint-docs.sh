#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

log=$(mktemp "${TMPDIR:-/tmp}/outshine-lint-docs.XXXXXX")
status=0
sh test/run.sh \
  harness/claims/ABoardIdIsIssuedOnce.cpp \
  harness/claims/ACommitSayingClosedRemovedTheItem.cpp \
  harness/claims/EveryBoardEdgePointsAtAnItem.cpp \
  harness/claims/EveryWorkItemHasABoundedSize.cpp \
  harness/claims/TheMapCitesLinesThatSayWhatItClaims.cpp \
  > "$log" 2>&1 || status=$?

if [ "$status" -eq 0 ]; then
  printf 'lint-docs: PASS (board and AGENTS only); details: %s\n' "$log"
else
  printf 'lint-docs: FAIL (exit %s); details: %s\n' "$status" "$log" >&2
  grep -E '^(FAIL|BUILD|UNPREP|TIMEOUT|SIGNAL|run.sh:.*(RED|no declared))' "$log" | head -12 >&2 || true
fi
exit "$status"
