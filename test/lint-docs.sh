#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

scratch=$(mktemp -d "${TMPDIR:-/tmp}/outshine-lint-docs.XXXXXX")
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
log=$(mktemp "${TMPDIR:-/tmp}/outshine-lint-docs.log.XXXXXX")

# Reuse the runner's toolchain policy without preparing engine libraries or corpora.
standard=$(sed -n 's/^CXXSTD=//p' test/run.sh)
warnings=$(sed -n 's/^WARN="\(.*\)"$/\1/p' test/run.sh)
optimization=$(sed -n 's/^OPT="\(.*\)"$/\1/p' test/run.sh)
[ -n "$standard" ] && [ -n "$warnings" ] && [ -n "$optimization" ]
CXX=${CXX:-c++}
. ./test/scripts/native_hardening.sh
ConfigureNativeHardening "$standard"
case "$optimization" in
  *'$NATIVE_COMPILE_FLAGS')
    optimization="${optimization%'$NATIVE_COMPILE_FLAGS'}$NATIVE_COMPILE_FLAGS" ;;
  *) printf '%s\n' 'lint-docs: runner optimization policy needs review' >&2; exit 2 ;;
esac
status=0
$CXX $standard $warnings $optimization $NATIVE_LINK_FLAGS -Itest/harness/shared \
  test/harness/claims/TheMapCitesLinesThatSayWhatItClaims.cpp \
  -o "$scratch/agents-contract" > "$log" 2>&1 || status=$?
if [ "$status" -eq 0 ]; then
  "$scratch/agents-contract" >> "$log" 2>&1 || status=$?
fi
if [ "$status" -eq 0 ]; then
  printf 'lint-docs: PASS (AGENTS only; offline); details: %s\n' "$log"
else
  printf 'lint-docs: FAIL (exit %s); details: %s\n' "$status" "$log" >&2
  tail -12 "$log" >&2
fi
exit "$status"
