#!/bin/bash
# The cloud frame-proof set, and the RECIPE that makes it reproducible.
#
# Lighting and weather were always deterministic (pinned --utc, the committed FBWX fixture); the
# TERRAIN was not, because the screenshot venue streams tiles while it renders. So every shot holds the
# camera still for 180 frames and writes only the last one: by then `fbworld` reports pending=0, and
# the converged tile set is a pure function of the camera. Two independent runs then write the same
# bytes -- verified, all 12 frames, in both --albedo osm and --albedo photo.
#
#   sim/tools/capture_cloud_proofs.sh            -> sim/build/r6-cloud-proofs/
#   OUT=build/x BIN=./build/gpu_native.old ...   -> an A/B set against another binary
#
# Needs fb-tiles on :8081 (the terrain) and `make -C sim native`. Analysis, not a build target:
# doc/render/clouds.md carries the numbers this produces.
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-build/r6-cloud-proofs}
BIN=${BIN:-./build/gpu_native}
WX=${WX:-assets/wx-2026-07-27T00Z.wxb}
mkdir -p "$OUT"

shot() {   # name lat lon altM pitch yaw utc [osm|photo]
  local name=$1 lat=$2 lon=$3 alt=$4 pitch=$5 yaw=$6 utc=$7 alb=${8:-photo}
  local d="$OUT/.tmp_$name"
  rm -rf "$d"; mkdir -p "$d"
  $BIN --lat "$lat" --lon "$lon" --ground 450 --agl "$(python3 -c "print($alt-450)")" \
       --pitch "$pitch" --yaw "$yaw" --albedo "$alb" --utc "$utc" --wx "$WX" \
       --seconds 3.0 --interval 3.0 --out "$d" > "$d/log.txt" 2>&1
  mv "$d/frame_0000.png" "$OUT/$name.png"
  echo "$name | $lat/$lon ${alt}m pitch $pitch yaw $yaw | utc $utc $(grep ephemeris "$d/log.txt" | sed 's/.*sunEl=/sun el /;s/ sunAz=/ az /;s/ moon.*//') | $(grep fbworld "$d/log.txt" | tail -1 | sed 's/.*\(pending=[0-9]*\).*/\1/')"
  echo "    $(grep cloud_decks "$d/log.txt" | tail -1 | sed 's/.*camAltM/camAltM/')"
  echo "    sha256 $(shasum -a 256 "$OUT/$name.png" | cut -c1-16)"
  rm -rf "$d"
}

# 1 — the undercast from above: the fixture's 76 % deck over Payerne, terrain through the gaps
shot p1-undercast-from-above 46.84 6.92 8450 -35 90 1785146400

# 2 — the fly-through, as a LADDER through one column (a MOVING camera never converges, so a flown
#     sequence cannot be hashed). Deck at 46.76/6.90 is 2982..3882 m.
shot p2a-above     46.76 6.90 5800 -10 50 1785146400
shot p2b-at-top    46.76 6.90 3950 -10 50 1785146400
shot p2c-inside-hi 46.76 6.90 3500 -10 50 1785146400
shot p2d-inside-lo 46.76 6.90 3060 -10 50 1785146400
shot p2e-at-base   46.76 6.90 2900 -10 50 1785146400
shot p2f-below     46.76 6.90 1900 -10 50 1785146400

# 3 — the underside into the sun. The THICK deck is Beer-dead (slant optical depth ~57, all three
#     multi-scatter octaves zero); the CIRRUS, tau ~3, is where Beer + the silver rim show.
shot p3-underside-into-sun         46.84 6.92 2000 25 277 1785171600
shot p3b-cirrus-underside-into-sun 49.00 14.00 7000 18 270 1785168000

# 4 — cirrus fibres across the 250 hPa wind axis, and the same veil from above
shot p4a-cirrus-across-wind 49.00 14.00  7000  20 47 1785168000
shot p4b-cirrus-from-above  49.00 14.00 10600 -25 47 1785168000

# 5 — the banding check at the worst light: sun 0.8 deg, deck self-shadowed, long tone range
shot p5-dusk-banding 46.84 6.92 8450 -35 90 1785178800
