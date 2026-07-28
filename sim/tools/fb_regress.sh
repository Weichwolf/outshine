#!/bin/bash
# Run every sim/missions/*.fbm into a snapshot directory, one subdirectory per mission.
# Usage: tools/fb_regress.sh <outRoot> [threads]
# The snapshot is what the byte-identity gate diffs; `wallS`/`speedup` and absolute paths are the
# only fields allowed to move, so they are normalised out here rather than in the diff.
set -u
cd "$(dirname "$0")/.."
OUT="$1"
THREADS="${2:-1}"
mkdir -p "$OUT"
for m in missions/*.fbm; do
  n=$(basename "$m" .fbm)
  mkdir -p "$OUT/$n"
  ./build/fb-gym --mission "$m" --out "$OUT/$n" --threads "$THREADS" >/dev/null 2>&1
  echo "$? $n" >> "$OUT/exit.txt"
  # normalise: drop the two wall-clock fields and any absolute path
  if [ -f "$OUT/$n/events.log" ]; then
    sed -e 's/wallS=[^ ]*//' -e 's/speedup=[^ ]*//' -e "s#$OUT/$n/##g" \
        "$OUT/$n/events.log" > "$OUT/$n/events.norm"
  fi
done
sort -o "$OUT/exit.txt" "$OUT/exit.txt"
echo "snapshot -> $OUT"
