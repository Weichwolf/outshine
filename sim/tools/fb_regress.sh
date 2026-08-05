#!/bin/bash
# Run every mission of the mod into a snapshot directory, one subdirectory per mission.
# Usage: tools/fb_regress.sh <outRoot> [threads] [--full]   (MOD= to point at another scenario)
#
# The snapshot is what the byte-identity gate diffs; `wallS`/`speedup` and absolute paths are the only
# fields allowed to move, so they are normalised out here rather than in the diff.
#
# WHAT IS KEPT, AND WHY IT IS NOT THE TELEMETRY. A full snapshot of 251 missions is ~6 GB, two of them
# are needed for one comparison, and a session that takes three such comparisons fills a disk — which
# has happened three times in this tree and each time blocked EVERY command, including the one that
# would have cleaned up. So the default keeps, per mission:
#   telemetry.sha    one SHA-256 per telemetry*.csv, sorted by name — the diff of two of these names
#                    exactly the files that moved, which is what the gate asks
#   events.norm      the normalised event log, kept in FULL because it is small and because it is the
#                    only place a reader can see WHAT moved rather than THAT it moved
#   exit.txt         one line per mission
# `--full` keeps the CSVs as well, for the one case the hashes cannot serve: reading a moved column.
# The honest cost is stated rather than hidden — with the default you learn WHICH file moved and must
# re-fly that ONE mission to see the numbers, which is seconds, against a snapshot that costs gigabytes.
set -u
cd "$(dirname "$0")/.."
OUT="$1"
THREADS="${2:-1}"
FULL="${3:-}"
# Zwei Staende vergleichen heisst ZWEI Binaries, nicht zweimal den Baum umschalten: `GYM=` zeigt auf
# eine weggelegte Kopie, damit der Arbeitsbaum waehrend eines 14-Minuten-Laufs unangetastet bleibt.
GYM="${GYM:-./build/fb-gym}"
MOD="${MOD:-../mods/f16}"
MISSIONS="$MOD/$(sed -n 's/.*"missions"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$MOD/mod.json")"
mkdir -p "$OUT"
for m in "$MISSIONS"/*.fbm; do
  n=$(basename "$m" .fbm)
  mkdir -p "$OUT/$n"
  "$GYM" --mission "$m" --mod "$MOD" --out "$OUT/$n" --threads "$THREADS" >/dev/null 2>&1
  echo "$? $n" >> "$OUT/exit.txt"
  # normalise: drop the two wall-clock fields and any absolute path
  if [ -f "$OUT/$n/events.log" ]; then
    sed -e 's/wallS=[^ ]*//' -e 's/speedup=[^ ]*//' -e "s#$OUT/$n/##g" \
        "$OUT/$n/events.log" > "$OUT/$n/events.norm"
  fi
  ( cd "$OUT/$n" && shasum -a 256 telemetry*.csv 2>/dev/null | sort -k2 ) > "$OUT/$n/telemetry.sha"
  if [ "$FULL" != "--full" ]; then
    rm -f "$OUT/$n"/telemetry*.csv "$OUT/$n/events.log"
  fi
done
sort -o "$OUT/exit.txt" "$OUT/exit.txt"
du -sh "$OUT" 2>/dev/null
echo "snapshot -> $OUT"
