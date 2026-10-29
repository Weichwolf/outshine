# Terrain cascade coverage

`terrain_cascade_coverage.py` compares the old disjoint-region union with a fixed
4×4 coverage grid. Input is the existing dyadic terrain cascade, not a metric
raster imposed on transport or terrain generation.

A cell at level L represents 4^L finest-level cells. Each parent sums its four
children. A ready tile replaces partial coverage with the full cell area; a
pending/absent/refused tile retains coverage supplied by finer descendants.
Complete coverage skips the same parent query. Unwrapped coordinates preserve
coverage; only the provider tile address wraps at the date line.

Induction: the initial grid is empty. Summation preserves the disjoint covered
area when moving one level outward. Replacing a cell with its complete area is
exactly the previous region union after deleting its contained descendants.
Thus every subsequent readiness query and fallback decision is preserved.
Each 4×4 child block lies inside the next 4×4 parent block; both axis parities
are exercised. No source-status memoization or invalidation policy is added.

The comparison covers 15,000 combinations: all four-level block alignment
residues, poles/date-line edges, zooms up to 23, randomized positions, the twelve
current camera inputs, all reply kinds, invalid ready meshes, all access modes
and playable-only demand. Source replies may depend on query order. The native
unit test separately unions individual finest-level cells; it does not reuse
this coverage algorithm.

Coverage bookkeeping needs at most 16 parent additions per level transition,
rather than repeated scans/compaction of up to 16*(24-1) regions. Status queries
remain unchanged. Unknown tile states can change the result, so the required
query count is a lower bound under this source interface; this is not a claim
of global engine optimality.

```sh
python3 -B test/experiments/render/terrain_cascade_coverage.py \
  --output "${TMPDIR:-/tmp}/outshine-terrain-coverage.json"
cmake --build build --target outshine_terrain_coverage_probe --parallel
build/outshine_terrain_coverage_probe 20000
```

The native probe uses the actual CMake component/flags. Modes are 0=ready,
1=pending, 2=mixed; it reports process CPU time, wall time, source query count
and output checksum. The probe performs no IO, meshing or GPU work. Use paired
A/B/B/A runs of the same probe linked to each implementation; keep query count
and checksum equal. These timings do not establish whole-world loading speed
or a framebudget pass. Device/cold/warm/visual evidence is in #1670/#15.
