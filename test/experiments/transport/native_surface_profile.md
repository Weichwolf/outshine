# Native complete cap profiles (#1189)

`CutSurfaceProfile` intersects a qualified source's native cap triangles with a
vertical plane through its source axis. The caller owns source identity, frame,
station choice and complete candidate-face selection. This is a geometric cut,
not a source join, navigation station or automatic endpoint qualification.

Input cap faces must be finite single-valued height triangles. Tangent direction
defines transverse order. Source vertices use exact XYZ keys; crossing source edges
use sorted endpoint keys and deterministic interpolation. Duplicate render vertices
at normal/UV seams can share an exact geometric edge. No positional welding occurs.
The ordered chain must have one continuous support interval containing the axis.
Gaps, height discontinuities, overlapping/coincident supports and clipped bands fail
locally. The caller must supply unique cap support at an edge-aligned ambiguous cut;
this operator does not select a highest surface or discard an owner.

All transverse points remain. Axis insertion interpolates on its native face;
each output interval retains its original input-face index. The caller can map this
back to page/triangle/source references. Unknown outer-end semantics stay unknown.

Default bounds: 128 candidate triangles, 256 output points. One face pass and at
most three edge tests per face precede O(F log F) segment ordering; no per-edge heap
map. Three owned vector buffers hold segments, output points and face parents.
Reported scratch/output bytes are their actual capacities, excluding caller input,
allocator overhead and stack. Error/cancellation returns no partial profile.

## Reproduction

```sh
make suite SUITE=outshine/src/generators/road/SurfaceProfile JOBS=2
PYTHONPATH=lib/src python3 test/experiments/transport/native_surface_profile.py \
  --output "${TMPDIR:-/tmp}/outshine-surface-profile-<unique-run>"
```

The module test builds only this operator. It checks a known affine height plane,
native face provenance, axis insertion, boundary edges, candidate filtering,
opposite frames, winding/order, gaps, stacked supports, bad inputs and budgets.

Four actual native-cut fixtures compare against unchanged `canonical_profile`:
original, permuted-face/winding and rigid controls (12 cases). The original four
XYZ arrays are word-identical. Rigid input generation transforms each unique source
coordinate once; independently batched rounding must not split shared source edges.
Six further comparisons consume native profiles through `BuildSurfaceRibbon`.

These candidate sets have six faces/eighteen edge tests each. On this build, owned
vector capacities are 1008 scratch bytes plus 220 result bytes: six 168-byte segment
records, eight 24-byte points and seven 4-byte parent indices. This is no claim
about whole-process memory or a device/frame budget.

The prepared original native replay can consume raw faces through profile cut,
ribbon construction, height program/HiGHS, expansion and body. Its topology, landing
masks, sections and caller orchestration are still prepared inputs. Native world
candidate selection, complete local planning, DEM/tunnel/navigation consumers and
cache/runtime admission remain open in #1189. This operator alone changes no Places.
