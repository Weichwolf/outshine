# Local mature-oak fork groups — #687

The continuous #690 wood still has sharply intersecting side branches. This
experiment rounds two requested junctions of the same saved 18-m oak, using the
existing #429/#443 union and collar methods. It preserves the entire 90000-leaf
inventory, source skeleton and materials. It is Python research, with no new
library/native/cache API, species default, vendor asset/code or published image.

## Sources and decision

SIGGRAPH 1985 [Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf),
PDF pages 5–6, was inspected optically again: outgoing tangent control and the
ramiform saddle describe geometry beyond intersecting tubes. This reuses our
implicit collar construction, not the paper's parametric ramiform. Prior local
SpeedTree/Ghidra parent/start-ring evidence is documented in
[continuous limbs](continuous_limbs.md) and [collars](crown_collars.md); this work
does not claim a new decompilation or executed numeric vendor weld oracle.
The retained whole-union/direct-limb counterexample from
[crown union](crown_union.md) remains valid; local success does not erase it.

The two real forks are source nodes 67 and 249. They share dominant parent axis
40 and use side axes 248/601. Canonical sorted demand merges overlapping axis
groups before kernel work; the parent is solved once. At most eight explicit
unique side forks are accepted. Existing source/field/union/output limits and
normal-orientation refusals remain. Unsupported requests do not export assets.
The prepared continuous-oak format is explicit; radii come from its physical
recipe, and the authored seed-axis continuations remain authoritative.

On each isolated real fork, direct whole limbs and the existing closed loft
cells both pass orientation and connected-output checks. Direct inputs use two
kernel bodies instead of 132/120, 2144/1952 input faces instead of 4224/3840,
and 77280/70368 geometry-copy bytes instead of 158400/144000. Corresponding
union volumes agree within 1.4e-17 m³; triangulations can differ. These are local
admissions, not a universal Boolean or speed ranking. Required internal side-root
and cell caps are explicitly forbidden by source face ID.

The final combined group uses three limbs/2640 faces, then adds two closed local
collar solids. Source corner normals transfer with the existing barycentric
method; invalid orientation still fails. No normal flipping or relaxed cleanup
criterion is introduced. Every unrequested wood face retains exact positions,
corner normals, UVs, material selector and source limb ID after index compaction.
New patch faces retain kind/source-node provenance in the saved patch product;
whole-wood patch IDs identify the replaced group, not a native cache format.

## Field quality and work avoided

Each fork reuses the existing C1 Hermite/PCHIP carrier, eight-sided inscribed
radius shrink and symmetric compact field mixing. Parent windows span 2.5 parent
radii on each side; side windows use the same chord span. Each selected branch
needs eight envelope segments. Source centre-plus-radius chord bounds are
0.1168/0.0528 mm. These bound the carrier, not the mixed surface or whole tree.

The existing maximum 2-mm first-exit ray-probe requirement is unchanged. Each
fork has 304 fixed directions, 256/512 sign scans and 42 bisections; scan
refinement changes results by at most 5.56e-17 m. Finite probes are not a global
Hausdorff or closest-point bound.

| Longest-axis cells | Fork 67 maximum probe error | Fork 249 | Result |
|---|---:|---:|---|
| 16 | 6.569 mm | 2.294 mm | Refused for this quality requirement |
| 32 | 1.465 mm | 0.776 mm | Selected |
| 96 | 0.169 mm | 0.074 mm | More work than needed for this requirement |

Dense and blocked mesh arrays match exactly at every compared resolution.
At selected 32 cells, grid field-point evaluations sum to 60800 → 23530,
plus 1050 interval-block queries. Both fields have 16 segments: these grid
queries account for 972800 → 376480 segment-point pairs; normal evaluation,
Marching Cubes, Boolean work and verifier queries are additional. Grid payload
sums to 243200 B; blocked selection adds 60800 B of masks. Fields run sequentially,
so these sums are not simultaneous peaks. Timings are exploratory shared-host
observations, not a device or isolated speed ranking.

## Complete product, independent checks and image result

Input product `645ac64957aab862c79613516652bd7d8d17805d9af3a2396f41792de1dab1a8`
produces `a96a082555d70a2656d896bd227afe83aed1b3c77ea170a74caeb355b36d4056`.
The new identity binds source product/wood bytes, requested nodes, resolution,
blend/tolerance recipe and Manifold version. These hashes are reproduction and
integrity evidence, not an external trust root for bypassing asset admission.
All 13 non-wood/non-entry source files are byte-identical, including eight Blade
meshes, leaf tables, maps/material data and source skeleton. No growth, placement,
organ construction or texture generation is repeated for this wood change.

Independent directed edge pairing, single vertex fans, connected components,
Euler characteristic and centroid-relative signed volume give one genus-0 local
body: 5924 faces, 2964 single fans, volume 0.016511321488064754 m³. Before collars
the selected united limbs have volume 0.016508879350630397 m³. There are no removed
flat fragments. All whole-tree facets/normals pass the existing checks; minimum
double-area is 4.309e-13 m² in Float64. This is not a Float32 admission.

The verifier independently extracts original axes by saved face ownership,
regenerates the selected collar meshes through the dense comparator, and checks
384 solid-angle samples against source-OR-collars; 62 samples observe added wood.
Selected surface offsets and box samples are not a global self-intersection or
material-equivalence proof. The entire tree still contains separate unrequested
limb/short-shoot bodies. Its 330880 untouched wood faces are checked exactly.

Matched 2560×1440 fork-camera images show a rounded outgoing transition instead
of an abruptly intersecting tube. A shading boundary against the octagonal parent
remains visible; arbitrary parent-skin C1 continuity is not proven. The opened
whole-context leaf-near comparison shows little visible gain beneath dense foliage.
The paired whole-tree image is also opened; the overall silhouette remains the
same, with the earlier regular/thin crown and repeated leaf fans still apparent.
No claim of completed bark, botanical oak morphology, forest/LOD/wind or native
runtime/device acceptance follows. #687 remains active for those local skin and
short-shoot connections; #682 retains the full morphology requirement.

Wood faces 333520 → 336804; complete coarse instanced references
3393520 → 3396804 (+0.0968%). Finished wood arrays 49963376 → 51799812 B,
including the new per-face group identifier. The final two union stages copy
known NumPy geometry payloads of 95184 + 322992 B, write 92864 B of face IDs,
and construct 18 known kernel-input arrays. Kernel-internal allocation/traffic
is unmeasured. Whole aggregation writes 51799812 B of concatenated output payload;
compaction masks/remaps, other reads/writes and compression are additional.

Frozen producer 6175c3119: source decode/plan 3.140 s, local construction 1.005 s,
whole aggregation/export 2.778 s, process peak 286359552 B. Current recipe-field
reading is separately replayed against the frozen output. Source/installed cold
controls load **both** original and new complete products (125793460 B of arrays),
with named generation entry points guarded; this is not a single runtime-cache
load benchmark or an alias-proof sandbox. Paired guard/decode 0.578/0.877 s;
separate independent verification 5.775/5.655 s; process peaks
339128320/339197952 B. No whole-tree Boolean is performed on a hit.

69 source and 69 installed-library consumer controls pass, including shared-parent
deduplication, reversed demand, bad demand/ownership, no-work assembly identity,
exact untouched triangles/attributes and unsupported charted-material refusal.
The existing #690 wheel is reused: all 207 installed Python library files match
current source; installed PYTHONPATH excludes lib/src. No new wheel/lib change is
claimed. Linux/WSL x86_64, Python 3.13.5/NumPy 2.5.4, one numerical thread, nice 10
under concurrent host work. Render receipts identify EEVEE/llvmpipe, not RTX.

## Reproduction

Original complete source and prior producer commands are retained in
[oak_continuous_wood.md](oak_continuous_wood.md) and [oak_scaffold.md](oak_scaffold.md).
Private inputs, current PNGs, failed-quality probes and terminal receipts are
under `/tmp/outshine-vegetation-oak-forks-proof-dc4a6fbd1`; do not remove the
original source targets of its symlinks. Exact final commits/commands/receipts
are linked in the PR. No server, gallery or image is added to the repository.

```sh
export TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest oak_fork_checks source_limb_checks limb_checks crown_collar_checks crown_union_checks crown_shoot_checks test_spatial_axes test_plant_plans test_blades oak_scaffold_checks posture_checks
.venv/bin/python test/experiments/vegetation/oak_fork_probe.py \
  --inputs /tmp/oak-continuous-products --output /tmp/oak-fork-comparison --node 67 --cells 32
.venv/bin/python test/experiments/vegetation/oak_fork_product.py \
  --inputs /tmp/oak-continuous-products --output /tmp/oak-local-forks --nodes 67 249
.venv/bin/python test/experiments/vegetation/oak_fork_replay.py \
  --inputs /tmp/oak-continuous-products --products /tmp/oak-local-forks --output /tmp/oak-fork-replay.json
```
