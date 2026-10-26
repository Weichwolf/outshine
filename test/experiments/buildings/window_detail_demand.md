# Window detail before physical-building expansion

#1052 under #33. The existing `FacadeProducts.physical()` builds all storeys,
stairs, window cuts and details after a building-level near decision. This
separate Python reference accepts explicit window IDs from the existing logical
plan. It expands only entering windows and retains the **complete original compact
exterior**, including its owned trim, courtyard walls, caps and access geometry.
No existing caller, default, native implementation or database format changes.

## Product and ownership

`WindowDetails` owns one existing `PreparedFacade` and `CompactFacade`. Draw spans
reference contiguous triangle ranges in the same immutable coarse arrays.
Requested windows replace their own flat products with the existing closed
frame, pane, mullion and sill recipe. Every other coarse span remains unchanged.
The detail identity is the original window ID; local closed-part numbers are
local to that window, not the old whole-building part-offset namespace.

`update(ids)` validates the entire bounded collection before expansion. Request
order does not affect source-plan order or capacity admission. Default capacity
128 windows, maximum8192; rejected capacity demand remains coarse and is reported
as `fallback_ids`. Duplicate/foreign IDs reject. New products and draw references
commit together; a failed partial build leaves the preceding residency intact.
No unselected window, floor builder or shell Boolean is needed. Repeated demand
reuses every mesh; changes build only entering IDs. The manager drops departed
references, but callers holding previous draw lists still own their meshes.
There is no unbounded history, prefetch policy or serialized cache here.

Function, collision, room state, doors and full physical products retain their
existing independent owners. This reference does not replace them. The compact
trim is retained at its existing quality; this is not the complete fine facade.
Explicit demand must include shadow/reflection/lighting needs when integrated.
The eight nearest windows in the measurement driver are a finite diagnostic
policy, **not** a projection, visibility or radiance error guarantee.

## Independent checks and actual images

Frozen producer `7cfdaedb3b13f5245337262764d70da458e2139d`.
Eight checks pass on five original Paris provider footprints, including a compact
retained control, each with cascade/paired-wing plans. Every generated window,
including every original courtyard window, matches the old **single-batch**
closed recipe exactly by oriented triangle positions/materials and by draw
positions, normals, UVs and facade extents, independent of triangle ordering.
The original complete coarse arrays/owners also match exactly on all ten plans.
Positive component volumes and paired/oppositely directed edges are checked
independently on twenty selected windows. These are not controller-motion gates.

Further checks cover partial/empty demand, stable IDs, unchanged references,
counted entering/released IDs, actual collection of departed arrays after the
last caller releases them, immutable buffers, capacity fallback, foreign/type/
duplicate errors, rollback after one successful new build then failure, and
patched floor/Boolean builders that must never run.

Eight fresh measurement processes produce byte-identical first/last draw arrays
between full and selective retention on all three cases. Actual1920×1080,
16-sample Eevee coarse/detail close views are opened locally. The matched source,
camera/material/light show restored frame recess and sill depth; the building
remains visually simple, glazing plain and interior absent. These images prove
neither physical glass quality nor full-building/world appearance or transitions.
Blender uses llvmpipe software, not Iris GPU admission. The local wrapper flushes
completed reports/images and uses the existing post-render teardown workaround;
ordinary bpy teardown is not claimed fixed. No images are published.

## Measured finite costs

i5-1135G7/Linux shared host, Python3.13.5, NumPy2.5.4, Shapely2.2.0/GEOS3.14.1,
normal priority, one BLAS thread. Three fresh
processes per method in alternating order, plus one separately traced process
per method. Plans and the identical coarse construction are separate phases.
Seven demand states each select eight windows, including repeats and translation.

| Case | All windows / generated selectively | Cold detail ms, all / selective | Seven-state detail ms, all / selective | Final detail bytes, all / selective |
|---|---:|---:|---:|---:|
| Original two-court cascade |252 /14|870.716 /37.080|874.216 /65.851|7846272 /249088|
| Original concave paired-wing |297 /17|928.989 /35.411|932.674 /77.686|8256600 /222400|
| Original compact control |91 /8|318.241 /37.363|319.752 /37.846|2833376 /249088|

The `all` control constructs each identical window patch once and retains all;
it does **not** rebuild all windows per state. Its per-window construction is
slower than the established batch recipe. Separately measured old batch-only
times span166.6–231.8 /117.3–196.2 /57.2–87.6ms respectively, excluding world
placement, draw conversion, partitioning and selection. Do not quote the table
as a speedup against the old batch path or a full physical building.
Cold selective times range26.4–37.4 /27.2–38.3 /36.2–37.8ms. The common complete
coarse build still costs168.3–239.2 /214.7–328.0 /104.0–155.2ms; it is not free.
Unchanged-demand calls cost0.065–0.161ms and generate zero geometry bytes.

Produced immutable detail buffers/freezing copies total7846272→435904,
8256600→472600 and2833376→249088bytes over the sequence. Coarse arrays separately
occupy1507594/1733052/617438bytes. Additional traced detail-phase peaks drop
8417156→309452,8911258→278724 and3067318→309228bytes; common coarse peaks and
caller-held previous draw lists remain separate costs. Whole-process normal RSS
is137.6–138.0MB selective versus144.8–145.9MB full, including imports and later diagnostic
exports/batch controls. This is not application/device memory. Freezing copies
are counted, not all transient allocation/copy traffic or native heaps. Diagnostic
materialization/export copies geometry and is outside the primary timers.
No frame, GPU, IO/upload, database, native memory or universal speed claim.

## Reproduction and sources

Use the existing research `.venv`, NumPy/Shapely and existing recipe dependencies;
no new package. Set `OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1` and
`PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets`.
Run `python -m unittest test_window_detail_demand -q`.
On a clean frozen producer run `compare_window_detail_demand.py --method all`
and `--method selective`, each with `--output /tmp/<explicit-directory>`;
`--trace` is a separate probe. First/last NPZs and Near/Coarse geometry reports
are emitted; `blender_city.py` consumes those reports. All private receipts,
raw measurements, actual image inputs and rendering wrapper remain in
`/tmp/outshine-research-window-demand-ba2976ff4/`; no source depends on this path.

SIGGRAPH-first: [Instant Architecture2003, p3/Fig4](../../../doc/references/buildings/siggraph/2003-instant-architecture.pdf)
and [Procedural Modeling of Buildings2006, p4/Fig5–8](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
visually inspected for component/shape ownership. [F-shade, EGSR2010 p2](../../../doc/references/buildings/egsr/2010-grammar-based-encoding-of-facades.pdf)
was visually inspected for compact random-access facade representation. This is
explicit existing mesh-component selection, not their grammars or GPU algorithm.
