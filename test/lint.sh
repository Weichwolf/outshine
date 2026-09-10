#!/bin/sh
# `make lint` -- the format, the static analysis, and this tree's own repository rules.
#
# EVERY TARGET IS ZERO. This carried a ratchet once -- a recorded count a commit could lower and
# never raise -- and the argument for it was that a gate red on the first day is a gate switched
# off in the first week. That argument is about a tired human, and it bought a real cost: a
# baseline standing at its own current value is GREEN, so it says "fine" about 714 undocumented
# names, and the number stops being read. The target is 0 and every trailer says how far that is.
#
# What the ratchet did protect is a REGRESSION -- a count that grew. That is now read where it
# belongs, in `git log`: every commit names its measured number, so two commits name two numbers.
set -eu
cd "$(dirname "$0")/.."

LLVM=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
# run-clang-tidy spawns clang-tidy by NAME, so naming the directory is not enough.
PATH="$LLVM:$PATH"
export PATH
REPORT=$(mktemp -d "${TMPDIR:-/tmp}/outshine-lint.XXXXXX")
printf 'lint: reports in %s\n' "$REPORT"
# EVERY GUARD REPORTS, AND THE VERDICT COMES AT THE END. An `exit 1` at the first red made every
# check below it unreachable for as long as the tree was over its baseline -- the repository rules,
# the door's documentation, the unreached count, the grammar against its own reader and writer, the
# shader entry points. Five guards asleep because the first one was awake. A gate that stops at its
# first finding reports one thing and hides five.
red=0

for tool in clang-format clang-tidy; do
  [ -x "$LLVM/$tool" ] || {
    printf 'lint: %s/%s is missing. `brew install llvm` puts it there.\n' "$LLVM" "$tool" >&2
    exit 2
  }
done
[ -f compile_commands.json ] || { printf 'lint: no compile_commands.json -- run `make db`\n' >&2; exit 2; }

mkdir -p "$REPORT"
printf '== format ==\n'
if python3 test/scripts/format_sources.py --tool "$LLVM/clang-format" --check 2>"$REPORT/format.log"; then
  printf 'lint: every file is formatted\n'
else
  printf 'lint: formatting failed -- run `make format`, or see %s\n' "$REPORT/format.log" >&2
  red=$((red + 1))
fi

printf '\n== analysis ==\n'
analysis_status=0
python3 test/scripts/tidy_analysis.py --root "$PWD" --report "$REPORT" \
  --tool "$LLVM/clang-tidy" --jobs "$(sysctl -n hw.ncpu)" || analysis_status=$?
found=$(wc -l < "$REPORT/tidy.unique" | tr -d ' ')
# Execution status is independent of finding count: zero is valid only after every unit ran.
grep -o '\[[a-z-]*\]$' "$REPORT/tidy.unique" | sort | uniq -c | sort -rn > "$REPORT/tidy.checks"
head -12 "$REPORT/tidy.checks"

printf '\nlint: %s finding(s), the target is 0\n' "$found"
if [ "$analysis_status" -ne 0 ]; then
  printf 'lint: %s to go. They are in %s\n' "$found" "$REPORT/tidy.unique" >&2
  red=$((red + 1))
fi

printf '\n== the repository rules ==\n'
# THE CLAIMS ARE A LINTER AND NOT A PROOF. Twelve of them check the BOARD -- an id issued once, an
# item naming its benchmark, every edge pointing at an item -- and about ten check the HARNESS. No
# off-the-shelf tool knows what `board/NNNN_*.md` is, so they stay; what stops is their standing in
# a TEST runner, where a red about a citation reads like a red about the engine.
if ! sh test/run.sh harness/claims > "$REPORT/claims.log" 2>&1; then
  red=$((red + 1))
fi
grep -E '^(FAIL|BUILD|UNPREP|PASS)' "$REPORT/claims.log" | grep -v '^PASS' | sed 's|/var/folders.*||' || true
grep 'tests:' "$REPORT/claims.log" | sed 's/^/lint: /' || true

printf '\n== documentation ==\n'
if ! python3 test/scripts/documentation_analysis.py --report "$REPORT"; then
  printf 'lint: public documentation is not clean or coverage is incomplete; see %s/doxygen.json and doxygen.log\n' "$REPORT" >&2
  red=$((red + 1))
fi

# WHAT NOTHING CALLS, and clang cannot answer it. clang-tidy works one translation unit at a time,
# so a public member in a header could be reached from any other unit and no single-unit pass may
# call it dead. The LINKER resolves the whole archive, and this tree already reads exactly what it
# resolves. The count is a SUSPICION rather than a verdict -- a symbol may be reached through a
# table this graph cannot see -- so a name here is read before it is deleted, never after.
if [ -f build/liboutshine.a ]; then
  if python3 test/scripts/unreached.py > "$REPORT/unreached.log" 2>&1; then
    printf '\nlint: symbol reachability candidates (advisory, not proof of dead code)\n'
    cat "$REPORT/unreached.log"
  else
    cat "$REPORT/unreached.log" >&2
    printf 'lint: symbol reachability analysis failed\n' >&2
    red=$((red + 1))
  fi
fi

# THE SCENARIO'S GRAMMAR AGAINST ITS OWN READER. The two are kept by hand beside each other and they
# drift; eight drifts have been found this way, each of them a capability no declaration could
# reach. board:2052 removes the guard by removing the second copy -- derive the grammar from the
# declaration types -- and until it lands this is what holds them together. It goes RED rather than
# carrying a count, because a child the reader reads and the grammar refuses is a defect with no
# legitimate population.
if ! python3 test/scripts/grammar_vs_reader.py; then
  printf 'lint: the scenario reader reads a child its grammar refuses -- a capability no\n' >&2
  printf 'lint: declaration can reach. Add the row, or stop reading it.\n' >&2
  red=$((red + 1))
fi

# Literal coverage is an inventory, not a proof of lossless serialization.
if ! make test-writer-inventory > "$REPORT/writer-inventory-tests.log" 2>&1; then
  cat "$REPORT/writer-inventory-tests.log" >&2
  red=$((red + 1))
elif ! python3 test/scripts/grammar_vs_writer.py; then
  printf 'lint: writer source coverage is incomplete; inspect unmatched names and analysis errors.\n' >&2
  printf 'lint: independent declaration roundtrip tests remain the preservation oracle.\n' >&2
  red=$((red + 1))
fi

# Audit every artifact Make declares, including descriptor-set ordering from SDL's specification.
# Selector/Shape coverage and backend execution are additional contracts in WI 2152.
if ! python3 test/scripts/test_shader_artifacts.py > "$REPORT/shader-check-tests.log" 2>&1; then
  cat "$REPORT/shader-check-tests.log" >&2
  red=$((red + 1))
fi
if ! python3 test/scripts/shader_artifacts.py --report "$REPORT/shaders"; then
  printf 'lint: shader artifact coverage or SDL binding verification failed\n' >&2
  red=$((red + 1))
fi

printf '\n'
if [ "$red" -gt 0 ]; then
  printf 'lint: %s guard(s) are RED. Each one printed its own number above.\n' "$red" >&2
  exit 1
fi
printf 'lint: every guard is green.\n' 
