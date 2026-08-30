#!/bin/sh
# Prints every object in an archive reachable from a seed prefix by UNDEFINED SYMBOL, one per
# line -- the closure the linker itself computes when it pulls members out of a `.a`.
#
# This walk is SOUND where a relocation walk is not. A virtual call carries no symbol at the call
# site, but the vtable that names its overrides is a relocation in the referencing object, so the
# override's symbol is undefined there and the closure crosses it. What the linker must resolve is
# exactly what this prints.
set -e
archive=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
work=$2
seed=$3
rm -rf "$work"
mkdir -p "$work"
( cd "$work" && ar x "$archive" )
cd "$work"
nm -g *.o 2>/dev/null |
  awk '/\.o:$/ { o = $1 } / [TDSB] / { if (o != "") print $3 "\t" o }' | sort -u > defines
nm -u *.o 2>/dev/null |
  awk '/\.o:$/ { o = $1 } !/\.o:$/ { if (o != "" && NF > 0 && $NF ~ /^_/) print o "\t" $NF }' \
  > wants
awk -v seed="$seed" '
  BEGIN { FS = "\t" }
  NR == FNR { where[$1] = $2; next }
  { need[$1] = need[$1] "\t" $2 }
  END {
    n = 0
    for (o in need) { if (index(o, seed) == 1) { if (!(o in seen)) { seen[o] = 1; queue[n++] = o } } }
    for (i = 0; i < n; i++) {
      split(need[queue[i]], symbols, "\t")
      for (k in symbols) {
        s = symbols[k]
        if (s == "") { continue }
        d = where[s]
        if (d != "" && !(d in seen)) { seen[d] = 1; queue[n++] = d }
      }
    }
    for (i = 0; i < n; i++) { print queue[i] }
  }' defines wants | sort -u
