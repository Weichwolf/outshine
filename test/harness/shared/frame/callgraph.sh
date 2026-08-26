#!/bin/sh
# Prints one edge per line, "caller<TAB>callee", over every object in an archive.
#
# THE LINKER'S OWN VIEW. mach-o objects built without -ffunction-sections carry one text section,
# so objdump labels every function `ltmp0` and its own labels are useless. `nm -n` gives the text
# symbols in ADDRESS order, and every relocation objdump prints carries the address it sits at, so
# the enclosing function is the last symbol at or below that address. No toolchain change, and the
# graph is exactly what the linker resolves rather than what a header suggests.
set -e
archive=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
work=$2
rm -rf "$work"
mkdir -p "$work"
( cd "$work" && ar x "$archive" )
for object in "$work"/*.o; do
  nm -n "$object" 2>/dev/null | awk '$2 == "t" || $2 == "T" { if ($3 !~ /^ltmp/) print $1, $3 }' \
    > "$work/syms"
  objdump -dr "$object" 2>/dev/null |
    awk -v symfile="$work/syms" '
      BEGIN {
        n = 0
        while ((getline line < symfile) > 0) {
          split(line, part, " ")
          at[n] = part[1]; who[n] = part[2]; n++
        }
      }
      /ARM64_RELOC_/ {
        here = $1
        sub(/:$/, "", here)
        if (here !~ /^[0-9a-f]+$/) { next }
        target = $NF
        caller = ""
        for (i = 0; i < n; i++) { if (at[i] <= here) { caller = who[i] } else { break } }
        if (caller != "" && target ~ /^_/) { print caller "\t" target }
      }'
done | sort -u
