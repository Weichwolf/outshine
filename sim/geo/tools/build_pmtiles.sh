#!/usr/bin/env bash
# build_pmtiles.sh — build the Hameln Shortbread vector PMTiles from
# Geofabrik's Niedersachsen OSM extract via Planetiler.
#
# Output: ../wasm/data/hameln.pmtiles (gitignored)
# Idempotent: skips the Planetiler run if the output exists and is newer
# than both the planetiler jar and the input .pbf.
#
# Prerequisites (NOT auto-installed):
#   - java >= 21 (Planetiler requirement)
#   - curl
#
# Do NOT run this from the Claude harness — the JVM + osmium + tile
# encode takes minutes and produces hundreds of MB. Run manually from a
# plain shell.
set -euo pipefail

# --- Resolve paths relative to the script, not $PWD ----------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE_DIR="$SCRIPT_DIR/.cache"
DATA_DIR="$REPO_ROOT/wasm/data"
mkdir -p "$CACHE_DIR" "$DATA_DIR"

# --- Tool discovery ------------------------------------------------------
# Windows users often have Java installed outside MSYS2's PATH. Fall back
# to well-known JDK install locations if `java` is not already reachable.
find_java() {
    if command -v java >/dev/null 2>&1; then
        command -v java; return 0
    fi
    local candidates=(
        "/c/Program Files/Microsoft/jdk-21.0.8.9-hotspot/bin/java.exe"
        "/c/Program Files/Microsoft/jdk-21/bin/java.exe"
        "/c/Program Files/Java/jdk-21/bin/java.exe"
    )
    # Also try any Microsoft/jdk-21* install.
    for d in /c/Program\ Files/Microsoft/jdk-21*; do
        if [ -x "$d/bin/java.exe" ]; then candidates+=("$d/bin/java.exe"); fi
    done
    for c in "${candidates[@]}"; do
        if [ -x "$c" ]; then echo "$c"; return 0; fi
    done
    return 1
}
JAVA_BIN="$(find_java || true)"
if [ -z "${JAVA_BIN:-}" ]; then
    echo "[pmtiles] ERROR: java not found. Install JDK >= 21." >&2
    exit 1
fi
echo "[pmtiles] java = $JAVA_BIN"

# --- Config --------------------------------------------------------------
PLANETILER_VERSION="0.8.3"
PLANETILER_URL="https://github.com/onthegomap/planetiler/releases/download/v${PLANETILER_VERSION}/planetiler.jar"
PLANETILER_JAR="$CACHE_DIR/planetiler-${PLANETILER_VERSION}.jar"

GEOFABRIK_URL="https://download.geofabrik.de/europe/germany/niedersachsen-latest.osm.pbf"
PBF="$CACHE_DIR/niedersachsen-latest.osm.pbf"

# Hameln / Emmerthal / Grohnde bbox (min_lon, min_lat, max_lon, max_lat).
BBOX="9.25,51.94,9.52,52.15"
OUTPUT="$DATA_DIR/hameln.pmtiles"

# --- Fetch planetiler ----------------------------------------------------
if [ ! -f "$PLANETILER_JAR" ]; then
    echo "[pmtiles] downloading planetiler ${PLANETILER_VERSION}..."
    curl -L --fail -o "$PLANETILER_JAR.tmp" "$PLANETILER_URL"
    mv "$PLANETILER_JAR.tmp" "$PLANETILER_JAR"
fi

# --- Fetch OSM extract ---------------------------------------------------
if [ ! -f "$PBF" ]; then
    echo "[pmtiles] downloading Niedersachsen OSM extract..."
    curl -L --fail -o "$PBF.tmp" "$GEOFABRIK_URL"
    mv "$PBF.tmp" "$PBF"
fi

# --- Idempotency check ---------------------------------------------------
# Consider the output up-to-date only if it is newer than both the jar and
# the input PBF AND clearly bigger than a failed-early stub (Planetiler
# truncated outputs from a previous crashed run can be a few KB).
MIN_OUTPUT_BYTES=$((1024 * 1024))
OUTPUT_SIZE=0
if [ -f "$OUTPUT" ]; then
    OUTPUT_SIZE=$(stat -c '%s' "$OUTPUT" 2>/dev/null \
                || wc -c < "$OUTPUT")
fi
if [ -f "$OUTPUT" ] \
   && [ "$OUTPUT_SIZE" -ge "$MIN_OUTPUT_BYTES" ] \
   && [ "$OUTPUT" -nt "$PBF" ] \
   && [ "$OUTPUT" -nt "$PLANETILER_JAR" ]; then
    echo "[pmtiles] $OUTPUT is up to date, skipping."
    exit 0
fi

# --- Run planetiler ------------------------------------------------------
# `generate-shortbread` is the built-in Shortbread-schema subcommand of
# planetiler.jar (uses shortbread.yml internally). The top-level entry
# point defaults to OpenMapTiles, which is NOT what our decoder expects.
#
# --compress=none:       raw MVT payloads — the browser fetches without
#                        needing a gzip polyfill.
# --bounds:              crop during tile generation (much faster than
#                        downloading the full-planet build and slicing).
# NOTE: Planetiler detects the archive format from the output file
# extension, so we write directly to "$OUTPUT" (any ".tmp" suffix would
# be interpreted as format=tmp and rejected).
echo "[pmtiles] building $OUTPUT ..."
rm -f "$OUTPUT"
# Planetiler parses the output argument as a URI; on Windows a bare
# "D:/..." path is misread as scheme="D". Pass a POSIX-style relative
# path by chdir'ing to the repo root first.
OUTPUT_REL="wasm/data/hameln.pmtiles"
PBF_REL="$(realpath --relative-to="$REPO_ROOT" "$PBF")"
cd "$REPO_ROOT"

# The big auxiliary (~1 GB water-polygons-split-3857) is downloaded into
# data/sources/ on the first run and cached there. Idempotent reruns skip
# the download entirely -- Planetiler only refetches when
# --refresh-sources or a newer upstream timestamp is detected.
#
# Pre-populate data/sources/ with the auxiliary Shortbread inputs.
# Planetiler's Shortbread profile expects two shapefiles besides the OSM
# PBF:
#   ocean         (water-polygons-split-3857, openstreetmap.de)
#   admin_points  (admin-points-4326, shortbread-tiles.org)
# The admin_points source returns 404 since the Shortbread-Tiles
# maintainers stopped publishing that shapefile. We stand in a
# header-only dummy shapefile so Planetiler can open it and iterate
# zero features -- admin_points only carries capital/city label points,
# which the Hameln viewer does not render anyway.
SRC_DIR="$REPO_ROOT/data/sources"
mkdir -p "$SRC_DIR"
ADMIN_ZIP="$SRC_DIR/shortbread.geofabrik.de_shapefiles_admin_points_4326.zip"
if [ ! -f "$ADMIN_ZIP" ] || [ ! -s "$ADMIN_ZIP" ]; then
    echo "[pmtiles] generating stub admin_points shapefile ..."
    python "$SCRIPT_DIR/_make_stub_shapefile.py" "$ADMIN_ZIP" \
        --basename admin-points-4326 \
        --shape-type 1
fi

# Planetiler's built-in 100 MB parallel HTTP chunker corrupts the 920 MB
# water-polygons-split-3857.zip on at least some Windows setups (the
# resulting file is byte-length-correct but its ZIP central directory is
# scrambled). Fetch it cleanly with curl, then re-pack it — the upstream
# zip triggers a JDK ZipFileSystem "zip END header not found" regression
# on Windows even when the archive is byte-perfect per `unzip -t`, so we
# ship Planetiler a ZipFS-friendly replacement. Only done once; the
# re-packed copy is cached across invocations.
OCEAN_CURL_CACHE="$CACHE_DIR/shortbread/water-polygons-split-3857_upstream.zip"
OCEAN_SRC_CACHE="$CACHE_DIR/shortbread/water-polygons-split-3857.zip"
OCEAN_DST="$SRC_DIR/osmdata.openstreetmap.de_download_water_polygons_split_3857.zip"
mkdir -p "$(dirname "$OCEAN_SRC_CACHE")"
if [ ! -f "$OCEAN_CURL_CACHE" ] || [ ! -s "$OCEAN_CURL_CACHE" ]; then
    echo "[pmtiles] curl fetching water-polygons zip (~900 MB) ..."
    curl -L --fail -o "$OCEAN_CURL_CACHE.tmp" \
        "https://osmdata.openstreetmap.de/download/water-polygons-split-3857.zip"
    mv "$OCEAN_CURL_CACHE.tmp" "$OCEAN_CURL_CACHE"
fi
if [ ! -f "$OCEAN_SRC_CACHE" ] || [ ! -s "$OCEAN_SRC_CACHE" ] \
   || [ "$OCEAN_CURL_CACHE" -nt "$OCEAN_SRC_CACHE" ]; then
    echo "[pmtiles] re-packing water-polygons zip for JDK ZipFileSystem ..."
    TMP_EXTRACT="$CACHE_DIR/shortbread/_extract"
    rm -rf "$TMP_EXTRACT"
    mkdir -p "$TMP_EXTRACT"
    (cd "$TMP_EXTRACT" && unzip -q "$OCEAN_CURL_CACHE")
    rm -f "$OCEAN_SRC_CACHE.tmp"
    (cd "$TMP_EXTRACT" && zip -r -0 -q "$OCEAN_SRC_CACHE.tmp" water-polygons-split-3857)
    mv "$OCEAN_SRC_CACHE.tmp" "$OCEAN_SRC_CACHE"
    rm -rf "$TMP_EXTRACT"
fi
if [ ! -f "$OCEAN_DST" ] || [ ! "$OCEAN_DST" -nt "$OCEAN_SRC_CACHE" ]; then
    echo "[pmtiles] copying ocean zip into data/sources/ ..."
    cp -f "$OCEAN_SRC_CACHE" "$OCEAN_DST"
fi

"$JAVA_BIN" -Xmx2g -jar "$PLANETILER_JAR" generate-shortbread \
    --area=niedersachsen \
    --osm-path="$PBF_REL" \
    --bounds="$BBOX" \
    --tile_compression=none \
    --output="$OUTPUT_REL" \
    --download \
    --http-retries=5 \
    --force

# Planetiler always writes internal_compression=gzip regardless of
# --tile_compression. osmmesh's T2 reader only accepts
# internal_compression=none (by design -- no zlib dependency), so we
# rewrite the directory + metadata blobs in place.
echo "[pmtiles] repacking internal_compression to none ..."
python "$SCRIPT_DIR/_pmtiles_decompress.py" \
    "$OUTPUT" "$OUTPUT.repack.tmp"
mv -f "$OUTPUT.repack.tmp" "$OUTPUT"

echo "[pmtiles] done: $OUTPUT"
