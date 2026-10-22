# Sparse water-face provenance (#760)

`river_membership.py` provides the agreed provenance component for the forthcoming
shared river cooker. It maps each caller-supplied triangle to **all original input
surfaces with positive-area overlap**, retaining original-table indices in CSR.
It does not group hydrological classes or change geometry, heights or native formats.

The original polygon/identity table remains with the caller. Members are unique,
ascending table indices. Offsets start at zero, are monotone and end at the member
count. The representative is the largest-overlap member; equal-area ties choose
the lowest original-table index. Touching an edge/point gives no membership; tiny
positive areas are retained without a threshold. Identical geometries in different
original records remain distinct. GEOS Float64 overlay is used, not an exact-rational
arrangement implementation or a proof for all near-degenerate inputs.

Membership is not a coverage proof. A face overlapping one original surface may
remain mostly uncovered. `uncovered_area_m2` separately measures triangle minus the
union of its clipped originals, preserving holes and overlapping-source semantics.
Summing individual member areas would double-count overlapping sources. A rejected
face has empty CSR, the uint32-max no-source sentinel and an explicit status;
unexamined coverage stays NaN. Other faces may still fit the remaining budget.
The caller must check status/coverage before using a face and must retain original
rings to delimit each member's part. No fabricated ancestry for procedural fill.

## Work and bounds

The implementation queries one triangle at a time against an immutable GEOS STRtree;
`indexed=False` is the complete-pair control. Input limits match the existing water
experiment: 8192 vertices, 16384 faces and at most 4096 original surfaces. An explicit
original-coordinate limit (default 262144, a laboratory guard) is checked before
copying all source coordinates into an array or constructing the index. Requested
polygon intersections and committed references have separate explicit limits.
Exhausted budgets stop further queries. A face that cannot fit gets no partial list;
the next smaller face may still fit. One query can enumerate all sources before the
intersection check, so this is not a bound on tree traversal or all GEOS work. No
dense face×source array is constructed. Worst-case enumeration can still approach
face count times source count; GEOS workspace/allocation totals are unmeasured.

CSR indices cost 4×(F+1+R) bytes and representatives 4×F. Per-member areas, missing
areas and statuses are diagnostics, not an agreed native file/binding change.

## Fixed real-input comparison

`compare_river_membership.py` uses the preserved Ill fixture and unchanged 20 m
conforming mesh: 13 original surfaces, 1487 vertices/faces, one connected body and
one island. The original source table, provider feature IDs as decimal strings,
tile/revision/digest/class/crossing observations and external original indices are
retained. CSR members index its 13-entry surface table, not native global array
positions; `original_surface_indices` supplies that separate mapping.

Indexed and complete-pair offsets, members, representatives, overlap/missing areas
and statuses are array-identical. All originals have positive references: 1602
members in total, 114 multi-source faces, maximum three members per face. The index
requests 1661 versus 19331 polygon intersections (maximum four candidates per face).
Neither approach is faster in this small instrumented run: about 0.60/0.59 seconds;
this is work reduction, not a native speedup. Output arrays occupy 44507 bytes,
including 12360 bytes for CSR and 5948 bytes for representatives. Python-traced peaks are
217368/140308 bytes; GEOS memory is excluded.

Coverage diagnostics retain Float64 overlay residuals: missing area sums to
6.2868e−10 m², maximum 8.0414e−12 m² across 367 faces. The unchanged mesh reports
zero symmetric-difference area and 1.5134e−8 m² overlap residual, within its existing
coverage tolerance; no tolerance is loosened. A whole-process Linux VmHWM reading
including plotting is about 93 MB. RUSAGE reports 2.22 GB in the same launch context;
isolated stage captures show about 42/49/93 MB. Both raw measurements remain in the
evidence; their discrepancy is unresolved and supplies no memory-budget admission.

This remains the **legacy planar frame**. Its original Lon/Lat/transform information
is insufficient for the agreed WGS84/physical-height test; master is recovering the
original export. The component supplies provenance now, not a completed Ill cooker,
geodetic level/direction proof, native integration, runtime image or device gate.

## Reproduction and sources

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error -m unittest test_river_membership test_river_surface
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error test/experiments/water/compare_river_membership.py \
  --input test/experiments/water/fixtures/feldkirch-ill-profile.npz \
  --output /tmp/outshine-research-water-membership-<commit>/capture
```

Dependencies: existing `outshine_ref`, NumPy, Shapely/GEOS and Matplotlib. The runner
pins input/manifest and output hashes, records complete-pair equality and source
usage, and saves its plot only in the explicit private output directory. Analytic
tests independently cover split areas, duplicate records, uncovered faces, holes,
touching boundaries, tiny slivers and local/exhausted budgets.

SIGGRAPH-first review: local [Zhou et al. 2016, *Mesh Arrangements for Solid Geometry*](../../../doc/references/buildings/geometry/2016-mesh-arrangements-for-solid-geometry.pdf),
p.1/Fig.1–2 visually inspected: ownership/classification and extracted coverage are
different tasks. This experiment uses existing planar GEOS overlay, not their exact
3D arrangement/winding-vector algorithm. The installed Shapely 2.2.0 / GEOS 3.14.1
API was inspected; the official [STRtree documentation](https://shapely.readthedocs.io/en/2.1.2/strtree.html)
also distinguishes bounding-box candidates from actual geometric overlap.
