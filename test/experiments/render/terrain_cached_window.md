# Existing terrain neighborhood export — #1248

`terrain_cached_window.py` extends the existing `terrain_cached_page.py` offline
reader with an explicit, complete rectangle of at most sixteen native pages.
This supplies missing original neighborhood inputs for a contact experiment;
it does not generate terrain, acquire sources or update the asset cache.

```sh
.venv/bin/python -B test/experiments/render/terrain_cached_window.py \
  --cache "$existing_assets_sqlite" --key "$existing_ground_region_key" \
  --window 20 308878 308880 393922 393924 \
  --output "${TMPDIR:-/tmp}/outshine-pool-terrain-window"
```

The window above is the requested original pool neighborhood, not product data.
Pass the existing cache path and full ground-region asset key explicitly.
The output directory must be new. A missing, duplicated, unsupported or nonfinite
requested page fails before creating it. An ancestor page never silently replaces
a missing requested page. A later filesystem failure can leave partial diagnostic
files; only a successfully returned archive is a delivery.

The existing decoder accepts one complete version-5 ground-region package, with
at most 64 MiB each of stored and native bytes, and verifies stored size/CRC.
It uses SQLite read-only mode and system `libzstd` for compressed input. Its scope
remains the versioned page prefix: trailing classes, network and geometry are not
independently validated. The complete package is decoded once; page records are
scanned once and at most sixteen 35×35 Float32 height arrays are copied. This
offline diagnostic is not a runtime selective-IO or memory-budget result.

Each page keeps its original index, tile identity, halo/layout, source zoom,
Float32 height words and native posting fractions. `input.json` binds the cache
key, whole stored/native hashes and individual height/fraction hashes. The
standard `pages.tar.xz` archive contains only that manifest and its named files.
No native format or dependency is added. Existing one-page selection retains
its maximum-zoom/nearest-anchor/index ordering and original words.

```sh
PYTHONPATH=test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  .venv/bin/python -B -W error -m unittest test_terrain_cached_window -v
```

The tests use the already pinned real page words plus a small declared binary
prefix/SQLite control. An optional existing-cache integration check is separate;
ordinary tests never depend on a private cache or create new reference pins.
