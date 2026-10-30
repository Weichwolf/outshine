# Dormers on the actual upper facade axes

Issue #1707 composes the existing box-dormer recipe with its final upper facade
programme. The old edge-spacing default remains available and unchanged. This is
one optional architectural refinement, not a new roof system or a complete family.

## Plan before geometry

`DormerFacades(..., dormer_layout="facade")` first stages the same source facade,
entrance and elevation choices used by the physical facade. For a scoped elevation,
the top scope supplies the bay count; an ordinary facade supplies its own count.
The later mesh binder requires exactly these source frames. The final output also
checks the chosen frame IDs, endpoints, width and column counts against the actual
bound facade. A second guessed window grid is insufficient.

The staged handoff is consumed during this source's construction. A roof failure
discards it before the previous owner runs, including only this source's provisional
entrance records; later records from other sources are preserved. The prepared roof retains its immutable source relationship and logical
frontage coordinates; existing resident asset and near-pocket reuse remains.
No global index, cache manager, native interface or material/shader change is added.

Each original nominal roof site selects its nearest actual facade bay centre.
Exact midpoint ties choose the lower column; positions beyond the end centres
clamp before subtracting distances. Compare exhaustive nearest selection with
binary search of ordered centres and at most two neighbouring distances. Both
produce identical complete plans, rejections and whole source products. New IDs
bind the existing source/facade ID and bay column, independently of the camera.

Original width/depth/inset, height, clearance, courtyard and64-dormer limits apply.
There is one proposal per original site; snapping does not authorize another roof
volume. A moved proposal can fail the original tests and keeps its reason. Missing
or invalid frontage plans fall back to the previous complete dormer product;
contradictory later facade binding rejects admission. Source rows remain unchanged.
At most4096 bay coordinates are accepted, checked before excess materialization.

## Physical products and evidence

Reuse the existing closed-roof/prism union, independent affine added-volume
integral, matching body/skin split, window-pocket subtraction, metric frame/glass
components and selective near expansion. No full attic, access or night programme
is inferred from an opaque-backed window pocket. Scope/entry/projection programmes,
original height and holes remain constraints.

The focused checks cover actual ordinary/scoped facade ownership, whole retained
facade/entrance programmes, closed unions, independent volumes, physical pockets,
body/skin seams, source footprints/heights, exhaustive/binary agreement, immutable
inputs, bounded invalid frames, exact ties, fallback and failed handoff cleanup.
Actual before/after whole, opposite, elevated, far and detail views share cameras,
light and materials. The full pinned source window checks all source/function owners. Its reader uses
explicit complete URI/payload-hash receipts before decode (#1711); an unpinned
survey continues using the current provider. No source refresh or oracle update
is performed. Commits, terminal measurements, original images and decisions belong to
the PR; local diagnostics remain under explicit system-temp paths.

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -m unittest verify_dormer_bays verify_dormers verify_elevations
PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/buildings/dormer_bay_cost.py --output /tmp/outshine-dormer-bay-cost
PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render nice -n 10 .venv/bin/python -B test/experiments/buildings/dormer_probe.py --layout facade --output /tmp/outshine-dormer-bay-models
PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render nice -n 10 .venv/bin/python -B test/experiments/buildings/dormer_city.py --layout facade --output /tmp/outshine-dormer-bay-city
PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render nice -n 10 .venv/bin/python -B test/experiments/buildings/dormer_audit.py --layout facade --geometry /tmp/outshine-dormer-bay-city/ParisDormerAfter-geometry.json --output /tmp/outshine-dormer-bay-physical.json
```

Use fresh output directories and frozen producers. Nine rotating normal-priority
comparisons keep correctness checks outside timing and Memray/native-stack
allocation profiling separate. Equal aligned selection methods are compared on
complete replies and source products. The old edge programme has different
geometry and is only an architectural cost reference. Source loading, plan-only,
whole source/selected near detail and rendering are separate. Guaranteed frame
endpoint-array copying is at least32 bytes per upper frame; validation, arithmetic
and internal copies remain additional. Total internal copying,
RSS and UMA remain unmeasured. Scoped incremental heap peaks exclude held inputs.

## Reference and limits

[SIGGRAPH2006 procedural buildings](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
p.3/Fig.4, was optically rechecked: hierarchical facade splits/repeats supply the
existing component axes. Existing own dormer/union operators and their provenance
remain documented in [the original recipe](dormers.md); no new SDK render oracle
or foreign recipe is claimed. Native World/cache/LOD, shadows/temporal transitions,
both devices and whole-world p99/UMA acceptance remain separate. The identical box
shape, broader roof/facade diversity and full architectural families remain open.
