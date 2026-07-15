#!/usr/bin/env bash
# build_terrain.sh — build Terrarium-RGB terrain PMTiles for the Hameln
# region from Copernicus GLO-30 DEM tiles.
#
# Output: ../wasm/data/hameln_terrain.pmtiles (gitignored)
# Idempotent: skips the full pipeline if the output exists and is newer
# than every source DEM tile.
#
# Prerequisites (auto-discovered, not installed by this script):
#   - curl (for anonymous S3 HTTPS fetch)
#   - gdal (>= 3.4) for gdalwarp / gdal_translate
#   - rio-rgbify (pip install rio-rgbify)  — `rio` on PATH or as RIO env var
#   - go-pmtiles (https://github.com/protomaps/go-pmtiles) on PATH as `pmtiles`
#     or picked up from tools/.cache/pmtiles.exe automatically.
#
# Do NOT run this from the Claude harness — the DEM fetch + gdalwarp +
# rio-rgbify tiling pipeline takes 10+ minutes. Run manually from a
# plain shell.
set -euo pipefail

# --- Tool discovery helpers ---------------------------------------------
# On Windows, rio-rgbify installs into the user's Python Scripts dir which
# is rarely on MSYS2's PATH. Fall back to the standard user install dir.
find_rio() {
    if command -v rio >/dev/null 2>&1; then
        command -v rio; return 0
    fi
    local candidates=(
        "$APPDATA/Python/Python310/Scripts/rio.exe"
        "$APPDATA/Python/Python311/Scripts/rio.exe"
        "$APPDATA/Python/Python312/Scripts/rio.exe"
        "$USERPROFILE/AppData/Roaming/Python/Python310/Scripts/rio.exe"
        "$USERPROFILE/AppData/Roaming/Python/Python311/Scripts/rio.exe"
        "$USERPROFILE/AppData/Roaming/Python/Python312/Scripts/rio.exe"
    )
    for c in "${candidates[@]}"; do
        if [ -n "${c:-}" ] && [ -x "$c" ]; then echo "$c"; return 0; fi
    done
    return 1
}
find_pmtiles() {
    if command -v pmtiles >/dev/null 2>&1; then
        command -v pmtiles; return 0
    fi
    if [ -x "$SCRIPT_DIR/.cache/pmtiles.exe" ]; then
        echo "$SCRIPT_DIR/.cache/pmtiles.exe"; return 0
    fi
    if [ -x "$SCRIPT_DIR/.cache/pmtiles" ]; then
        echo "$SCRIPT_DIR/.cache/pmtiles"; return 0
    fi
    return 1
}

# --- Resolve paths -------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CACHE_DIR="$SCRIPT_DIR/.cache/terrain"
DATA_DIR="$REPO_ROOT/wasm/data"
mkdir -p "$CACHE_DIR" "$DATA_DIR"

# --- Config --------------------------------------------------------------
# Copernicus GLO-30 DEM (AWS Open Data Registry).
# 1x1 degree tiles, named by SW corner. The Hameln bbox
# [9.25, 51.94] .. [9.52, 52.15] spans two tiles.
S3_BUCKET="s3://copernicus-dem-30m"
TILES=(
    "Copernicus_DSM_COG_10_N51_00_E009_00_DEM"
    "Copernicus_DSM_COG_10_N52_00_E009_00_DEM"
)
# Output is Terrarium-RGB. rio-rgbify handles the reprojection to the
# Web-Mercator slippy-tile grid internally; we feed it an EPSG:4326
# raster cropped to the Hameln BBox. (Pre-reprojecting to EPSG:3857 on
# Windows triggers a PROJ 'densify_pts' abort inside rio-rgbify 0.4.0.)
BBOX_MIN_LON="9.25"
BBOX_MIN_LAT="51.94"
BBOX_MAX_LON="9.52"
BBOX_MAX_LAT="52.15"
ZOOM_MIN="0"
ZOOM_MAX="13"

MERGED_TIF="$CACHE_DIR/hameln_dem_3857.tif"
MBTILES="$CACHE_DIR/hameln_terrarium.mbtiles"
OUTPUT="$DATA_DIR/hameln_terrain.pmtiles"

# --- Tool resolution -----------------------------------------------------
RIO_BIN="$(find_rio || true)"
PMTILES_BIN="$(find_pmtiles || true)"
if [ -z "${RIO_BIN:-}" ]; then
    echo "[terrain] ERROR: rio (rio-rgbify) not found. Install via: pip install --user rio-rgbify" >&2
    exit 1
fi
if [ -z "${PMTILES_BIN:-}" ]; then
    echo "[terrain] ERROR: pmtiles (go-pmtiles) not found. Drop binary into tools/.cache/pmtiles.exe" >&2
    exit 1
fi
echo "[terrain] rio     = $RIO_BIN"
echo "[terrain] pmtiles = $PMTILES_BIN"

# --- Fetch DEM tiles -----------------------------------------------------
# Use anonymous HTTPS (AWS CLI is optional; curl is ubiquitous and works
# against the public Copernicus bucket without credentials).
S3_HTTPS_BASE="https://copernicus-dem-30m.s3.amazonaws.com"
downloaded=()
for tile in "${TILES[@]}"; do
    local_tif="$CACHE_DIR/${tile}.tif"
    if [ ! -f "$local_tif" ]; then
        echo "[terrain] fetching $tile..."
        curl -L --fail -o "$local_tif.tmp" \
            "$S3_HTTPS_BASE/$tile/$tile.tif"
        mv "$local_tif.tmp" "$local_tif"
    fi
    downloaded+=("$local_tif")
done

# --- Idempotency check ---------------------------------------------------
# If the final PMTiles is newer than every DEM tile, skip.
skip=1
if [ -f "$OUTPUT" ]; then
    for tif in "${downloaded[@]}"; do
        if [ ! "$OUTPUT" -nt "$tif" ]; then
            skip=0
            break
        fi
    done
else
    skip=0
fi
if [ "$skip" = "1" ]; then
    echo "[terrain] $OUTPUT is up to date, skipping."
    exit 0
fi

# --- Merge + crop in EPSG:4326 ------------------------------------------
# gdalwarp mosaics the two DEM tiles and crops to the Hameln BBox. We
# keep EPSG:4326 here (Copernicus' native CRS) because rio-rgbify will
# do its own reprojection per slippy tile; pre-warping to 3857 upsets
# PROJ on Windows. Copernicus GLO-30 pixel size is 0.000277778 deg
# (= 1/3600, i.e. 1 arcsecond) in both axes — matching that keeps the
# resample clean.
echo "[terrain] warping mosaic in EPSG:4326 ..."
gdalwarp -overwrite \
    -t_srs EPSG:4326 \
    -te "$BBOX_MIN_LON" "$BBOX_MIN_LAT" "$BBOX_MAX_LON" "$BBOX_MAX_LAT" \
    -tr 0.000277778 0.000277778 \
    -r bilinear \
    -co COMPRESS=DEFLATE \
    "${downloaded[@]}" \
    "$MERGED_TIF"

# --- Encode to Terrarium-RGB mbtiles -------------------------------------
# rio-rgbify's --encoding=terrarium produces the RGB format three.js'
# MapboxTerrainProvider / common WebGL viewers expect:
#   height_m = (R * 256 + G + B / 256) - 32768
echo "[terrain] rio-rgbify → mbtiles (zoom $ZOOM_MIN-$ZOOM_MAX) ..."
# rio-rgbify 0.4.0 lacks --encoding; encode Terrarium manually via the
# generic base/interval parameters. Formula used by the decoder:
#   h = R*256 + G + B/256 - 32768
# rio-rgbify encodes: encoded_int = (data - baseval) / interval, splitting
# encoded_int across RGB as (high, mid, low) bytes. Setting
#   baseval  = -32768
#   interval = 1/256 = 0.00390625
# yields encoded_int = (h + 32768) * 256 and therefore
#   h = -32768 + (R*65536 + G*256 + B) / 256
#     = -32768 + R*256 + G + B/256   (bit-for-bit Terrarium).
rm -f "$MBTILES"
"$RIO_BIN" rgbify \
    -b -32768 \
    -i 0.00390625 \
    --min-z "$ZOOM_MIN" --max-z "$ZOOM_MAX" \
    --format png \
    "$MERGED_TIF" "$MBTILES"

# --- Convert to PMTiles --------------------------------------------------
echo "[terrain] pmtiles convert → $OUTPUT ..."
rm -f "$OUTPUT.tmp"
"$PMTILES_BIN" convert "$MBTILES" "$OUTPUT.tmp"

# go-pmtiles defaults to internal_compression=gzip; osmmesh's T2 reader
# only accepts internal_compression=none, so repack the archive in place.
echo "[terrain] repacking internal_compression to none ..."
python "$SCRIPT_DIR/_pmtiles_decompress.py" \
    "$OUTPUT.tmp" "$OUTPUT.repack.tmp"
mv -f "$OUTPUT.repack.tmp" "$OUTPUT"
rm -f "$OUTPUT.tmp"
echo "[terrain] done: $OUTPUT"
