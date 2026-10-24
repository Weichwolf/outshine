#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

if [ "${1:-}" != --verbose ]; then
  transcript=$(mktemp "${TMPDIR:-/tmp}/outshine-lint-changed-output.XXXXXX")
  status=0
  sh test/lint-changed.sh --verbose > "$transcript" 2>&1 || status=$?
  grep -E '^(lint scope:|tidy \(|format:)' "$transcript" || true
  printf 'lint: exit %s; details: %s\n' "$status" "$transcript"
  if [ "$status" -ne 0 ]; then tail -n 8 "$transcript" >&2; fi
  exit "$status"
fi

LLVM=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
REPORT=$(mktemp -d "${TMPDIR:-/tmp}/outshine-lint-changed.XXXXXX")
printf 'lint: reports in %s\n' "$REPORT"
make db
python3 test/scripts/lint_scope.py --base "${LINT_BASE:-HEAD}" --output "$REPORT/scope.json"
if python3 -c 'import json,sys; sys.exit(not json.load(open(sys.argv[1]))["full"])' "$REPORT/scope.json"; then
  exec sh test/lint.sh --verbose
fi

red=0
python3 test/scripts/format_sources.py --tool "$LLVM/clang-format" --check \
  --scope "$REPORT/scope.json" || red=1
python3 test/scripts/tidy_analysis.py --root "$PWD" --report "$REPORT" \
  --tool "$LLVM/clang-tidy" --jobs "${LINT_JOBS:-2}" --scope "$REPORT/scope.json" || red=1
python3 test/scripts/layer-contract.py > "$REPORT/layers.log" 2>&1 || red=1
if python3 -c 'import json,sys; sys.exit("AGENTS.md" not in json.load(open(sys.argv[1]))["changed"])' "$REPORT/scope.json"; then
  make lint-docs || red=1
fi
printf 'lint: changed scope exit %s; full integration remains make lint-all\n' "$red"
exit "$red"
