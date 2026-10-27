# Native-shaped water source binding with selective area evaluation

Owner: [#970](https://github.com/Weichwolf/outshine/issues/970), on #1049/#1046.
The observed consumer is `WaterSurfaceTopology::BindSources` in
`src/generators/water/WaterSurfaceTopology.cpp` (native30ab833a8). It stores
`FaceSourceOffsets`, `FaceSources` and each face's `Surface` representative.
It uses overlap area to choose that representative but does not store area weights.
The Python reference now supplies exactly those metadata products, not a new
water/cache/GPU format. Source coordinates, face order and original IDs remain.

## Selection contract

`bind_sources` returns immutable uint32 source offsets, sorted original source
IDs and representative IDs, plus explicit per-face states and diagnostics.
A representative maximizes exact original overlap area; equal areas choose the
smallest original source ID. This is the mathematical interpretation of the
existing largest-overlap/ascending-original-ID rule. It can correct a floating
numeric choice; it is not a claim of byte equality to the old numeric producer.

`overlap_relation` reuses the previous predicate's work to distinguish disjoint,
partial and complete **single-source** triangle coverage. A valid polygon boundary
inside the open triangle implies positive area on both its interior and exterior
sides. Without such a boundary, the rational interior point determines the whole
open triangle's membership; source closedness extends containment to its boundary.
Thus a complete source has area equal to the triangle and every partial source
has strictly less area. No new numerical tolerance is used.

The optimized method avoids area evaluation when there is one positive owner,
or chooses the smallest complete-cover owner when one exists. Only several
partial owners need the existing rational area calculation. The `areas` control
computes all candidate areas and produces the same three required metadata arrays.
Those computations internally also determine coverage, but no area/coverage
output is part of the binding product in either method.

Binding validity means every face has committed IDs/a representative and every
original selected source appears. It is not proof of geometric coverage, physical
water compatibility, height/terrain correctness or native asset admission. In
particular, the single-owner shortcut may bind a partially covered triangle.
Native integration must retain any separately required coverage checks/residuals.

## Limits and ownership

The source/coordinate/vertex/face/intersection/member limits follow the observed
native defaults:4096/262144/8192/16384/100000/100000. Existing Python source-edge
preparation also caps each source at8192coordinates; this reference is not a proof
for every larger native-accepted polygon. Predicate/area work is bounded by100000
units per face and10000000total, with the previous rational bit/edge limits.
No raised geometry/height tolerance or per-place rule is introduced.

Failed faces commit no partial source list or representative; independently
completed faces remain inspectable and the complete-group validity stays false.
Unused input source IDs are explicit, not deleted. Exhausted work/intersection/
membership budgets skip unnecessary queries. GEOS validation/index traversal,
input/geometry construction and opaque allocation are separate from that counter;
it is not a hard wall-clock/memory cap. The caller owns cancellation/job scheduling.
Original source tables stay with the caller; IDs refer to that unchanged snapshot.

## Native-spacing evidence

The frozen captured union is densified at the observed native default20m and
triangulated:5506faces/5495unique points, below the existing native limits. The
original exported counterexample triangle is present byte-for-byte in its ordered
coordinates exactly once. This is a regeneration consistent with that native
control, not a byte replay of a complete original native mesh.

Both methods produce byte-identical source CSR/representatives, input geometry
and source-ID tables in four normal processes. All48originals remain represented;
there are6398positive memberships. The optimized paths are4649single-owner faces,
240complete-cover winners and617partial-owner area evaluations, instead of5506.
Compared with numeric overlay diagnostics,55source sets differ, one numeric face
has no owner and6other numeric representatives choose a strictly smaller original
area. Independent rational checks confirm all six are strict errors, not a new
tie-policy effect. A further fixed/hash-bound original-input triangle covers this.

On i5-1135G7/Linux/shared host, two normal runs per method measured binding phases
42.367/33.458s (`areas`) versus10.062/10.151s (relations). The first control used the
same frozen binder before diagnostic-only owner-array/image output was added to
the driver; those later operations are outside the binding timer. Counted work
6407857→1748594; metadata arrays69644bytes in both, extra prepared edges79328bytes
in the optimized path. Process RSS was55.37/55.51MB versus52.54/52.56MB. This small
shared-host sample is not a portable speed/memory or native/device/frame claim;
complete allocator/copy traffic is unmeasured. Fixture decode/regeneration and
numeric diagnosis are separately timed, output/plotting excluded.

The unchanged geometry and actual representative assignments were plotted/opened.
The numeric panel is a per-face diagnosis, not an admitted old scene. Twenty-eight
targeted tests include the prior19, all shortcut/ranking/tie/permutation cases,
strict native owner error, genuine uncovered remainder, immutable inputs and
atomic local-budget refusal. Full native roundoff/component/height/cache/Places
and device integration remain with Master; no native code was modified.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/water "$PY" \
  test/experiments/water/compare_water_source_binding.py --method relations --image \
  --output "${TMPDIR:-/tmp}/outshine-source-binding"
```

Use `--method areas` for the same-output control. `--trace` is an optional separate
instrumented run; the reported memory comparison above uses process peaks.
