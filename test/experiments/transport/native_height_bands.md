# Native indexed height bands · #1189

`CutHeightBands` consumes a CCW indexed XY projection with continuous XYZ reference
heights, global level values and one level range per input triangle:

| Range count | Result |
|---|---|
| 0 | Original face, continuous height, level index −1 |
| 1 | Original face assigned the explicitly selected tread level |
| ≥2 | Clip against the range's strictly increasing horizontal bands |

Output retains all input points and appends shared original-edge/level crossings.
Each triangle carries its original face index and upper level index. Reference Z
remains continuous; the body consumer selects a constant tread height from the
level index. Exact horizontal fields belong to one ceiling level, including exact
ties. The generic operator clips to the supplied interval; recipe coverage and
flat-landing selection remain caller contracts. No hardcoded rise or thickness.

Parallel half-space clips keep at most five polygon vertices. Crossings are
evaluated from the original edge, independent of previous clips, and reused across
faces/bands by source-point IDs and level identity. Coordinates do not weld
independent height sheets. Cancellation and work/output limits refuse the whole
result; exact zero-area fan faces are skipped without a new numerical tolerance.

Finite levels and a prefix of nonincreasing transitions validate repeated ranges
in O(levels + faces), rather than scanning each range for every face. Full-range
triangles use a direct path. Others use lower/upper bounds before clipping.
Expected hash-table work is O(points + levels + faces·log(levels) + output), not a
worst-case hash bound or a whole-world/frame guarantee.

```sh
make all db JOBS=2
PYTHONPATH=lib/src:test/experiments/transport python3 -B -W error \
  test/experiments/transport/native_height_bands.py --output "$TMPDIR/height-bands"
make suite SUITE=outshine/src/base/spatial/HeightBandMesh JOBS=2
```

The unchanged `stair_indexed_levels.partition` supplies the independent reference.
Every coordinate word, triangle, source mask and tread index must match. The two
declared multi-flight inputs cover whole stair runs: selection saves no bands.
A fragmented affine control has 128 triangles and 64 bands: 128·64 = 8192 original
visits versus 380 intersecting/touching bands, so 7812 visits are avoided with the
same geometry. This is a declared synthetic work comparison, not a worldwide or
runtime speedup. Native timing excludes fixture preparation, build and probe IO.

`native_planar_regions.py` also consumes these cuts between native envelope
remeshing and native band/body construction. Recipe/group selection, fitting and
the consumer driver still use Python. Terrain checks currently verify reservation
data against the reference field; they do not generate or validate deformed DEM.
