# Complete-cell far products and inverse-depth planning (#12)

This supplies a complete-cell control for [master's set/compositor discussion](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6261461347).
It compares existing native Fine/Shell/Massed products before another image
renderer is attempted. No native code, cache/profile or device gate changes.

## Preserved input and observable ownership

`subset_native_cell_input` selects **every** original record belonging to a
native cell, in original order, with its matching summary. The shared coordinate,
hole and height arrays remain byte-identical. The complete original schema3 cell
form block is copied without parsing/rebuilding individual forms or hip plans.
Its source count must match. Anonymous `(kind=0,id=0)` records remain distinct
through original cohort plus source ordinal. Non-first-in-cell sources are now
testable; the earlier one-source adapter and its restrictions are unchanged.

The unchanged frozen x86-64/libc++ exporter has SHA256
`60679166753fd2cc6856524329a1eba13f59b61fa6cfdb13b63d5f5a0a6d9a33`.
`gdb_read_cell_mesh` observes all append calls and reads six vectors, with no
inferior calls/writes/patches. Each call preserves all previous stream bytes.
Final whole/cell stream hashes and call ranges match; independent native JSON
matches instrumented JSON after excluding elapsed time. Counts/bytes/indices,
accepted calls, unsupported/projected paths and original source mappings are checked.
At most 32 sources and the frozen vector byte limits are accepted by this control.

Normal calls retain actual source-local stream ranges. Massed calls have native
`source:null`: the complete set of lumped cell sources is known, **not** which
members belong to each emitted mass. Per-mass membership, per-part/face ranges
and projected-emission ownership are still missing contracts; they are not
invented by matching bounds or nearest geometry. In Flensburg five sources
become four masses, making this distinction concrete.

Follow-up: the frozen reader now also observes `StructurePlanSelection::EmitProjected`
at its verified SysV argument-six `BakedTile&` (`r9`), then `Raised` at byte 16.
Independent final counts and whole/cell hashes validate this ABI. Projected
members retain their observed collective source list, not invented per-face IDs.
Normal triangle ranges still bind sources; optional colour/metre vectors may
backfill previous vertices, so physical append ranges alone are not semantic
ownership of those attributes. A 32-source control and the new image decision
are documented in [the depth-cluster comparison](depth_cluster.md).

## Existing native products, not new speedups

Two complete original cells preserve nine sources, including every generated part:

| Cell | Fine | Shell control | Massed control |
| --- | --- | --- | --- |
| Vienna bbd9aab74311… / 39, four sources | 467 triangles / 34,756 B | 127 / 9,172 B | 112 / 8,096 B |
| Flensburg a169a8d0d6fb… / 49, five sources | 3,761 / 222,684 B | 237 / 13,980 B | 48 / 2,880 B |

Vienna source ordinals are 106/107/109/110. Private generation errors 0/4/512
produce three controls; **observed detail arrays**, not those numbers, define
the labels. Vienna's Shell control is `[Shell,Fine,Shell,Shell]`; its Massed
control is `[Massed,Shell,Massed,Massed]` with three masses. Flensburg selects
five Shells or five Massed sources grouped into four masses. No protected source
is forced into a different type. Runtime projection tolerances are unchanged.
Bytes are original `Raised::UsedBytes`, not GPU/cache residency or total assets.

Four pairs (two cells × two candidates) render 36 identical poses each: six
azimuths, 0/32 m translation, distances 8/32/128 source extents, original focal
1044.33155 px and 1280×720. Four regular samples per pixel use the corrected
triangle/R-tree reference. The eye is 1.7 m above the lowest source-vertex plane;
this is a low-eye isolated control, **not DEM-verified ground or a terrain/world
view**. Panels enlarge identical ROIs; the underlying arrays keep native pixels.
Two diagnostic colour-factor/Lambert lights are shared. Native PBR/UV/shadow,
weather, interaction and native device costs are not evaluated.

Both Shell controls have zero alpha disagreement with Fine in these 72 poses.
They still have material/normal differences. Massed controls change form and
appearance, including gaps and roofs. Alpha L1 sums below cover 12 poses per row,
not unique world pixels. They do not establish a positional/coverage certificate.

| Distance in source extents | Vienna Fine alpha / Massed alpha L1 | Flensburg Fine alpha / Massed alpha L1 |
| --- | ---: | ---: |
| 8 | 6,344.25 / 499.00 px | 8,468.25 / 3,376.50 px |
| 32 | 381.25 / 19.50 px | 486.75 / 195.50 px |
| 128 | 28.75 / 0.75 px | 40.75 / 20.25 px |

The final row is only a few pixel-equivalents per pose, with visible relative
losses in Flensburg; small absolute counts do not grant admission. Close,
translated, opposite-side and distant panels were actually inspected. Process
peak RSS across the four runs is 145,140–284,596 KiB; three final comparisons ran
concurrently. Python query/preparation times include R-tree/library overhead and
are not GPU timing. Per-call native ranges, triangle/index/byte counts and ray
counts are available; allocations and library-internal memory traffic remain
incompletely measured. Fine fallback remains for every unvalidated selection.

## How many planes, under precisely which motion?

For parallel camera axes, focal length `f`, source depth `z`, plane depth `zp`
and purely lateral eye displacement `D`, horizontal screen error is exactly
`f * |D| * |1/z - 1/zp|`. Divide `[1/zmax,1/zmin]` into `L` equal bins and put
each plane at the inverse-depth bin centre. In exact real arithmetic the bound is
`f * Dmax * (1/zmin - 1/zmax) / (2*L)`. Thus a sufficient uniform-bin count is:

`L = max(1, ceil(f * Dmax * (1/zmin - 1/zmax) / (2 * epsilon)))`.

`plane_depth_budget` checks this conditional construction with an independent
point-projection test. It is **not** a general 64 m movement cube bound: forward
translation changes denominators; camera rotation, projected sampling, uncovered
surfaces, normal/material/shadow error and depth order need separate treatment.
True-depth reprojection can avoid this plane quantisation but still needs enough
layers to store newly visible surfaces. The interval formula does not determine
that visibility-layer count or make a single front-depth image sufficient.

For the same complete-cell poses, `Dmax=64 m`, `epsilon=0.5 px`, and original
focal length, the planning results are:

| Distance | Vienna planes / dense field bytes | Flensburg planes / dense field bytes |
| --- | ---: | ---: |
| 8 | 3–11 / 539,460–1,761,870 B | 4–6 / 544,320–756,360 B |
| 32 | 1 / 17,040–20,640 B | 1 / 13,500–16,740 B |
| 128 | 1 / 3,150–3,750 B | 1 / 2,880–3,420 B |

This is one explicit **dense layout estimate**, not an optimal representation:
30 B/slot = depth32 + packed normal32 + colour-factor RGBA8 + material-code16 +
UV Float32×2 + wall metres Float32×2. The rectangle includes ±64 m horizontal
image displacement at nearest depth and the common two-pixel border. Counts omit
mesh/state/mips, row offsets, source bindings, visibility-layer multiplicity and
roundabout coverage. They estimate storage, not bake work or measured cache traffic.
They do not prove PBR sufficiency or shader filtering quality. No plane/LDI bake
or renderer runs in this planning step. The Fine geometry is an offline reference;
a runtime selector would consume cached product metadata before Fine emission.

The 64 m case is a challenge, not a universal reuse requirement. Following
[master's comparison request](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6261595758),
the default planning matrix now also includes 1 m and 6 m. At 6 m/s and 60 fps
these represent 10 and 60 motion-only frames (64 m: 640 frames). They are test
loads, not engine limits or justified appearance refresh intervals. The source
pixel span, padded storage rectangle and actual focal length are recorded separately.

| Reuse / distance | Vienna planes / dense field bytes | Flensburg planes / dense field bytes |
| --- | ---: | ---: |
| 1 m / 8 | 1 / 58,320–93,960 B | 1 / 63,630–97,020 B |
| 6 m / 8 | 1–2 / 86,940–143,640 B | 1 / 68,040–101,430 B |
| 1 m / 32 | 1 / 7,440–11,280 B | 1 / 7,830–11,340 B |
| 6 m / 32 | 1 / 8,160–12,000 B | 1 / 8,370–11,610 B |
| 1 m / 128 | 1 / 1,650–2,250 B | 1 / 1,980–2,520 B |
| 6 m / 128 | 1 / 1,800–2,400 B | 1 / 1,980–2,520 B |

Short reuse changes the conclusion about plane count: most such intervals need
one plane, and their intermediate-distance field estimates can fall below Shell
bytes before missing costs. It must not be rejected just because 64 m needed more
planes. These are geometric/storage estimates, not new 1/6 m rendered-image
evidence. A useful refresh choice also needs amortized bake/update/transfer and
draw/shading costs versus direct geometry; none of those GPU costs is measured
by this planner. Visibility layers, source bindings and fallback can erase a
storage advantage. The complete small-cluster depth-image comparison remains open.

## Procedure choice and remaining counterprobe

Use the set analogy as a spatial hierarchy of native products with a common
compositor, not a rule to replace scenery with planes. Preserve direct geometry
for the near/interacting world and as the measured baseline. At distance 8 in
these cells, dense field estimates exceed Shell bytes even with short reuse;
their complete rendering/update costs remain unmeasured. Far projected clusters remain
candidates. Their unseen surfaces, source membership and complete costs must be
tested before choosing between depth-cluster images, layered samples and geometry.

Reuse static form/material fields, then shade under current light/weather and
pose. Specular response, local/shadow changes and moving traffic/wind are not
made static by image caching. Geometry-only reuse time follows allowable eye
displacement and motion; it is not a lighting update interval. Rotation needs
fresh validation of the reuse domain. For maximum inverse-depth residual `drho`,
the lateral displacement limit is `Dallow = epsilon / (f * drho)`; at constant
lateral speed `v`, geometry reuse time is at most `Dallow / v`. Zero residual
removes this particular parallax limit, not atlas/visibility/update constraints.
Source changes and unsupported disocclusion can require an earlier update.
Rotation needs resident roundabout products; a previously unseen direction is not reconstructed
by TAA. Near/far transitions still need common shading and explicit overlap tests.
The next useful small image counterprobe is a source-complete, small projected
cluster with true-depth/unlit fields versus these Shell/Massed baselines, with
raw disocclusion and fallback measured separately. No resolution sweep of the
rejected individual-house LDI or large new native pipeline follows this result.

SIGGRAPH-first sources: [Hierarchical Image Caching (1996)](https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/shade96.pdf),
pages 3–4 including equations and figures optically checked, and the local
[LDI paper (1998)](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf).
The former makes caching conditional on reuse benefit and its page-4 corner
approximation is explicitly nonconservative; it is not adopted as an admission
proof. [Talisman](https://www.microsoft.com/en-us/research/publication/talisman-commodity-realtime-3d-graphics-for-the-pc/)
supplies a historical layer/coherence architecture, not an Outshine PBR/device
result. Its publication abstract was checked; full PDF analysis remains open.
The supplied Pixar Deep Compositing endpoint redirects to a library page; its
full PDF/optical review and a deep-compositor implementation are not claimed here.

## Reproduction

Use `.venv` with NumPy, Trimesh/Rtree and Pillow; original native inputs, pinned
exporter, its original runtime environment and GDB are required. No native build.
All outputs below must be fresh explicit system-temp paths.

```sh
.venv/bin/python test/experiments/render/subset_native_cell_input.py \
  --input "$ORIGINAL/input.json" --cell "$CELL" --error-px 0 --output "$FINE"
# Repeat errors 4/512 into separate controls; inspect actual selected details.
# Run the unchanged exporter; source gdb_read_cell_mesh.py at main with
# OUTSHINE_SOURCE_MESH_OUTPUT, OUTSHINE_SOURCE_NATIVE_JSON and independent
# OUTSHINE_SOURCE_ORIGINAL_JSON. Retain original native/index/form inputs.
.venv/bin/python -W error test/experiments/render/compare_native_cell_views.py \
  --fine "$FINE" --candidate "$CANDIDATE" --output "$TEMP_VIEWS"
.venv/bin/python -W error test/experiments/render/plane_depth_budget.py \
  --fine "$FINE" --views "$TEMP_VIEWS/report.json" --output "$TEMP_PLANES"
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p test_native_cell_inputs.py
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p test_plane_depth_budget.py
```

Reports record input/code hashes, original members, native types, cameras, fields,
cost scope and raw image errors. The receipt linked in #12 preserves exact local
commands, terminal outcomes and viewed diagnostics. Images remain local only.
