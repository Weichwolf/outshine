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
REPORT=build/lint
# EVERY GUARD REPORTS, AND THE VERDICT COMES AT THE END. An `exit 1` at the first red made every
# check below it unreachable for as long as the tree was over its baseline -- the repository rules,
# the door's documentation, the unreached count, the grammar against its own reader and writer, the
# shader entry points. Five guards asleep because the first one was awake. A gate that stops at its
# first finding reports one thing and hides five.
red=0

for tool in clang-format clang-tidy run-clang-tidy; do
  [ -x "$LLVM/$tool" ] || {
    printf 'lint: %s/%s is missing. `brew install llvm` puts it there.\n' "$LLVM" "$tool" >&2
    exit 2
  }
done
[ -f compile_commands.json ] || { printf 'lint: no compile_commands.json -- run `make db`\n' >&2; exit 2; }

mkdir -p "$REPORT"
ours=$(find src include test -name '*.cpp' -o -name '*.h' | grep -v '/shaders/' | sort)

printf '== format ==\n'
if "$LLVM/clang-format" --dry-run --Werror $ours 2>"$REPORT/format.log"; then
  printf 'lint: every file is formatted\n'
else
  printf 'lint: %s file(s) are not formatted -- `clang-format -i` on them, or see %s\n' \
    "$(cut -d: -f1 "$REPORT/format.log" | sort -u | wc -l | tr -d ' ')" "$REPORT/format.log" >&2
  red=$((red + 1))
fi

printf '\n== analysis ==\n'
"$LLVM/run-clang-tidy" -p . -quiet -j "$(sysctl -n hw.ncpu)" \
  '/src/.*\.cpp$' > "$REPORT/tidy.log" 2>&1 || true
# THE COUNT IS ABOUT CODE THIS TREE OWNS. clang-tidy reports a diagnostic at the location it is
# ABOUT, and for a replaceable global operator that location is the standard library's own header:
# `readability-inconsistent-declaration-parameter-name` compares src/base/io/Heap.cpp's twelve
# replacements against libc++'s declarations, which name their parameters `__sz` and `__p`. This
# tree may not match those -- `bugprone-reserved-identifier` refuses them, and it is right, a
# double underscore is reserved to the implementation -- and it may not change libc++. Measured:
# our names give 16 findings, libc++'s give 20. So a diagnostic located outside src/, include/ and
# test/ is not counted, because there is no commit that could lower it. No check is switched off
# and every finding about a line this tree wrote still counts.
grep 'warning:' "$REPORT/tidy.log" | grep -vE '^/(Library|usr|opt|System|Applications)/' |
  sed 's/ \[/\t[/' | sort -u > "$REPORT/tidy.unique"
found=$(wc -l < "$REPORT/tidy.unique" | tr -d ' ')
# A LINT THAT FINDS NOTHING HAS BROKEN, NOT PASSED. clang-tidy writes its diagnostics to STDERR and
# this redirected them to /dev/null for one round, which reported zero and was believed -- a gate
# blind to its own path, which is the first trap on CLAUDE.md's list.
if [ "$found" -eq 0 ]; then
  printf 'lint: the analysis found NOTHING over 172 units, which means it did not run.\n' >&2
  printf 'lint: %s\n' "$REPORT/tidy.log" >&2
  exit 2
fi
grep -o '\[[a-z-]*\]$' "$REPORT/tidy.unique" | sort | uniq -c | sort -rn > "$REPORT/tidy.checks"
head -12 "$REPORT/tidy.checks"

printf '\nlint: %s finding(s), the target is 0\n' "$found"
if [ "$found" -gt 0 ]; then
  printf 'lint: %s to go. They are in %s\n' "$found" "$REPORT/tidy.unique" >&2
  red=$((red + 1))
fi

printf '\n== the repository rules ==\n'
# THE CLAIMS ARE A LINTER AND NOT A PROOF. Twelve of them check the BOARD -- an id issued once, an
# item naming its benchmark, every edge pointing at an item -- and about ten check the HARNESS. No
# off-the-shelf tool knows what `board/NNNN_*.md` is, so they stay; what stops is their standing in
# a TEST runner, where a red about a citation reads like a red about the engine.
sh test/run.sh harness/claims > "$REPORT/claims.log" 2>&1 || true
grep -E '^(FAIL|BUILD|UNPREP|PASS)' "$REPORT/claims.log" | grep -v '^PASS' | sed 's|/var/folders.*||' || true
grep 'tests:' "$REPORT/claims.log" | sed 's/^/lint: /' || true

printf '\n== documentation ==\n'
if [ -x "$(command -v doxygen)" ]; then
  mkdir -p build/doc
  doxygen doc/Doxyfile >/dev/null 2>&1 || true
  undocumented=$(wc -l < build/doc/warnings.txt 2>/dev/null | tr -d ' ')
  printf 'lint: %s undocumented public entit(ies), the target is 0\n' "$undocumented"
  if [ "$undocumented" -gt 0 ]; then
    printf 'lint: %s to go. They are named in build/doc/warnings.txt\n' "$undocumented" >&2
    red=$((red + 1))
  fi
else
  printf 'lint: doxygen is not installed, so the door is not checked. `brew install doxygen`\n' >&2
fi

# WHAT NOTHING CALLS, and clang cannot answer it. clang-tidy works one translation unit at a time,
# so a public member in a header could be reached from any other unit and no single-unit pass may
# call it dead. The LINKER resolves the whole archive, and this tree already reads exactly what it
# resolves. The count is a SUSPICION rather than a verdict -- a symbol may be reached through a
# table this graph cannot see -- so a name here is read before it is deleted, never after.
if [ -f build/liboutshine.a ]; then
  unreached=$(python3 test/scripts/unreached.py | sed -n 's/.* \([0-9][0-9]*\) that nothing in the archive calls.*/\1/p')
  printf '\nlint: %s symbol(s) nothing in the archive calls, the target is 0\n' "$unreached"
  if [ "$unreached" -gt 0 ]; then
    printf 'lint: %s to go. They are named by `python3 test/scripts/unreached.py`\n' \
      "$unreached" >&2
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

# AND THE GRAMMAR AGAINST ITS OWN WRITER, which is the half `roundtrip` cannot see. Reading a place,
# writing it, reading that back and writing it again holds even when the writer DROPS a section --
# measured: with `<clock>` removed every place lost 59 bytes and `roundtrip` still said `0 place(s)
# apart`. A child declared and never written is a capability the engine can be told and can never
# hand back. Sixty-four stand today and the target is none of them.
if ! python3 test/scripts/grammar_vs_writer.py; then
  printf 'lint: the scenario grammar declares a child its writer cannot write back, and the\n' >&2
  printf 'lint: count GREW. A declaration that cannot be handed back is not declared.\n' >&2
  red=$((red + 1))
fi

# THE SHADER VARIANTS AGAINST THE NAMES THAT ASK FOR THEM. The subject's variant set is written
# twice -- as `fragment SFrag` entries in the .msl files and as string literals the renderer hands
# the driver -- and the two agree by hand. A name asked for and not defined is a pipeline that fails
# at RUN time with a driver's message rather than a compiler's; a name defined and never asked for
# is a variant nothing can reach. board:2060 removes the second list; this holds them together until
# it does.
if ! python3 test/scripts/entries_vs_shaders.py; then
  printf 'lint: the renderer names a shader entry no .msl defines, or defines one nobody asks\n' >&2
  red=$((red + 1))
fi

printf '\n'
if [ "$red" -gt 0 ]; then
  printf 'lint: %s guard(s) are RED. Each one printed its own number above.\n' "$red" >&2
  exit 1
fi
printf 'lint: every guard is green.\n' 
