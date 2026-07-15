#!/usr/bin/env bash
# Ensure the region PMTiles cache under sim/geo/data/ is populated.
#
# These are DERIVED data (~32 MB) and are .gitignored — they are never committed. This script
# is the cache: if the files are already there it does nothing, otherwise it obtains them.
#
# Resolution order (first hit wins):
#   1. cache hit                       -> done, no work
#   2. $FLIGHTBOX_PMTILES_URL          -> download a prebuilt tarball
#   3. a local wasm-osm checkout       -> copy (fast path on a dev box that already built them)
#   4. build from public upstream      -> ./tools/build_pmtiles.sh + build_terrain.sh
#                                         (slow: Planetiler/Java, Geofabrik, Copernicus, gdal)
#
# Long-term this whole cache disappears: worldwide on-demand HTTP tiles remove the need for a
# preloaded region archive entirely (see the geo-mapdata agent).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p data
VEC="data/hameln.pmtiles"; TER="data/hameln_terrain.pmtiles"

if [ -s "$VEC" ] && [ -s "$TER" ]; then
    echo "[geo] PMTiles cache hit ($(du -sh data | cut -f1))"; exit 0
fi
echo "[geo] PMTiles cache miss -> obtaining"

if [ -n "${FLIGHTBOX_PMTILES_URL:-}" ]; then
    echo "[geo] downloading from \$FLIGHTBOX_PMTILES_URL"
    curl -L --fail "$FLIGHTBOX_PMTILES_URL" | tar -xz -C data
elif [ -s "${OSM_DIR:-$HOME/Git/wasm-osm}/wasm/data/hameln.pmtiles" ]; then
    SRC="${OSM_DIR:-$HOME/Git/wasm-osm}/wasm/data"
    echo "[geo] copying from local checkout $SRC (fast path)"
    cp "$SRC/hameln.pmtiles" "$SRC/hameln_terrain.pmtiles" data/
else
    echo "[geo] building from public upstream sources (slow; needs java, gdal, rio-rgbify, curl)"
    ./tools/build_pmtiles.sh
    ./tools/build_terrain.sh
fi

[ -s "$VEC" ] && [ -s "$TER" ] || { echo "[geo] FAILED to obtain PMTiles"; exit 1; }
echo "[geo] PMTiles ready ($(du -sh data | cut -f1))"
