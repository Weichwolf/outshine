#!/bin/sh
# `make lint` -- the format, the static analysis, and this tree's own repository rules.
#
# THE BASELINE MAY ONLY SHRINK. A strict analysis over 57 000 lines finds thousands on the first
# day, and a gate that is red on the first day is a gate that is switched off in the first week. So
# the count is recorded and a commit may LOWER it and never raise it: new code is held to zero
# because every finding it adds shows up in the total, and old code is repaired at whatever pace it
# is touched. That is the only version of "very strict" that survives contact.
set -eu
cd "$(dirname "$0")/.."

LLVM=${LLVM_BIN:-/opt/homebrew/opt/llvm/bin}
# run-clang-tidy spawns clang-tidy by NAME, so naming the directory is not enough.
PATH="$LLVM:$PATH"
export PATH
BASELINE=test/lint-baseline
REPORT=build/lint

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
  exit 1
fi

printf '\n== analysis ==\n'
"$LLVM/run-clang-tidy" -p . -quiet -j "$(sysctl -n hw.ncpu)" \
  '/src/.*\.cpp$' > "$REPORT/tidy.log" 2>&1 || true
grep 'warning:' "$REPORT/tidy.log" | sed 's/ \[/\t[/' | sort -u > "$REPORT/tidy.unique"
found=$(wc -l < "$REPORT/tidy.unique" | tr -d ' ')
# A LINT THAT FINDS NOTHING HAS BROKEN, NOT PASSED. clang-tidy writes its diagnostics to STDERR and
# this redirected them to /dev/null for one round, which reported zero and recorded a baseline of
# zero -- a gate blind to its own path, which is the first trap on CLAUDE.md's list.
if [ "$found" -eq 0 ]; then
  printf 'lint: the analysis found NOTHING over 172 units, which means it did not run.\n' >&2
  printf 'lint: %s\n' "$REPORT/tidy.log" >&2
  exit 2
fi
grep -o '\[[a-z-]*\]$' "$REPORT/tidy.unique" | sort | uniq -c | sort -rn > "$REPORT/tidy.checks"
head -12 "$REPORT/tidy.checks"

allowed=$(cat "$BASELINE" 2>/dev/null || echo 0)
printf '\nlint: %s finding(s), the baseline allows %s\n' "$found" "$allowed"
if [ "$found" -gt "$allowed" ]; then
  printf 'lint: THE BASELINE GREW by %s. A commit lowers it or leaves it; it never raises it.\n' \
    "$((found - allowed))" >&2
  printf 'lint: what is new is in %s\n' "$REPORT/tidy.unique" >&2
  exit 1
fi
if [ "$found" -lt "$allowed" ]; then
  printf '%s\n' "$found" > "$BASELINE"
  printf 'lint: the baseline SHRANK to %s -- recorded. Commit %s with the repair.\n' \
    "$found" "$BASELINE"
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
  allowedDoc=$(cat test/doc-baseline 2>/dev/null || echo 0)
  printf 'lint: %s undocumented public entit(ies), the baseline allows %s\n' \
    "$undocumented" "$allowedDoc"
  if [ "$undocumented" -gt "$allowedDoc" ]; then
    printf 'lint: THE DOCUMENTATION BASELINE GREW by %s -- a new public name arrived undocumented.\n' \
      "$((undocumented - allowedDoc))" >&2
    printf 'lint: they are named in build/doc/warnings.txt\n' >&2
    exit 1
  fi
  if [ "$undocumented" -lt "$allowedDoc" ]; then
    printf '%s\n' "$undocumented" > test/doc-baseline
    printf 'lint: the documentation baseline SHRANK to %s -- recorded.\n' "$undocumented"
  fi
else
  printf 'lint: doxygen is not installed, so the door is not checked. `brew install doxygen`\n' >&2
fi

# WHAT NOTHING CALLS, and clang cannot answer it. clang-tidy works one translation unit at a time,
# so a public member in a header could be reached from any other unit and no single-unit pass may
# call it dead. The LINKER resolves the whole archive, and this tree already reads exactly what it
# resolves. The count is a SUSPICION rather than a verdict -- a symbol may be reached through a
# table this graph cannot see -- so it carries a baseline that may fall and never rise.
if [ -f build/liboutshine.a ]; then
  unreached=$(python3 test/scripts/unreached.py | sed -n 's/.* \([0-9][0-9]*\) that nothing in the archive calls.*/\1/p')
  allowedUnreached=$(cat test/unreached-baseline 2>/dev/null || echo 0)
  printf '\nlint: %s symbol(s) nothing in the archive calls, the baseline allows %s\n' \
    "$unreached" "$allowedUnreached"
  if [ "$unreached" -gt "$allowedUnreached" ]; then
    printf 'lint: THE UNREACHED BASELINE GREW by %s -- something was written that nothing calls.\n' \
      "$((unreached - allowedUnreached))" >&2
    printf 'lint: they are named by `python3 test/scripts/unreached.py`\n' >&2
    exit 1
  fi
  if [ "$unreached" -lt "$allowedUnreached" ]; then
    printf '%s\n' "$unreached" > test/unreached-baseline
    printf 'lint: the unreached baseline SHRANK to %s -- recorded.\n' "$unreached"
  fi
fi

# THE SCENARIO'S GRAMMAR AGAINST ITS OWN READER. The two are kept by hand beside each other and they
# drift; eight drifts have been found this way, each of them a capability no declaration could
# reach. board:2052 removes the guard by removing the second copy -- derive the grammar from the
# declaration types -- and until it lands this is what holds them together. It goes RED rather than
# carrying a baseline, because a child the reader reads and the grammar refuses is a defect with no
# legitimate population.
if ! python3 test/scripts/grammar_vs_reader.py; then
  printf 'lint: the scenario reader reads a child its grammar refuses -- a capability no\n' >&2
  printf 'lint: declaration can reach. Add the row, or stop reading it.\n' >&2
  exit 1
fi

# THE SHADER VARIANTS AGAINST THE NAMES THAT ASK FOR THEM. The subject's variant set is written
# twice -- as `fragment SFrag` entries in the .msl files and as string literals the renderer hands
# the driver -- and the two agree by hand. A name asked for and not defined is a pipeline that fails
# at RUN time with a driver's message rather than a compiler's; a name defined and never asked for
# is a variant nothing can reach. board:2060 removes the second list; this holds them together until
# it does.
if ! python3 test/scripts/entries_vs_shaders.py; then
  printf 'lint: the renderer names a shader entry no .msl defines, or defines one nobody asks\n' >&2
  exit 1
fi
