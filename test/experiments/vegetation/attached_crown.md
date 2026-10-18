# Attached leaf shoots on the mature subcrown

Owner [#739](https://github.com/Weichwolf/outshine/issues/739), child of the
unfinished [mature oak study](oak_scaffold.md). The [subcrown source](subcrowns.md)
still carries27/28 large blades on each35cm carrier. Reuse the existing
[TwigClusters procedure](twig_clusters.md), rather than adding more independent
roll noise. The isolated and whole views show less fan repetition and a more
connected crown with the same physical blades. This is a structural improvement,
not a mature-oak/default/forest/LOD admission.

## Source, hierarchy and unchanged inventory

Use the reproducible `limb` product from `subcrown_probe.py` and
`subcrown_products.py`, originating in the fixed own `data/crown682/` inputs.
Plan identity0e9819d59d105c236be898111214c0c6d7c20cf4fbbb80646ffb122de36fc7d6,
catalog identity680b9d908b2fdb370420e6a6a4d33bf676058c88b651c15dce4b8cceb37d8021.
It has3895 growth nodes,3224 carriers,90000 blade IDs and eight variants with
11250 each. Source C1 wood, carriers, blade geometry,392.735145m² analytic area
and procedural study material remain unchanged. New twig/leaf poses have new
identities; old world-space leaf locations deliberately change.

TwigClusters partitions each27/28-leaf host into five attached shoots with
5/6 leaves each:16120 children, within its existing16384-child capacity.
Child IDs remain hostID×32+ordinal. Its seeded stratified parent attachments,
golden-angle azimuth, inclination and rigid-frame formulas are unchanged.
Carrier radius profile is explicitly .5→.05mm, matching the retained source
carriers. Child bases use .65 of the interpolated parent radius and tips .3 of
their base. Intersections are centreline attachments, not newly welded collars.

Existing Shoot recipes give child lengths17–28.111cm,34–46mm physical
internodes and .35rad bend; original carrier internodes are about11.45–11.89mm.
These are authored controls, not measured Quercus allometry or a bud-growth
simulation. Blade petioles, source group/variant assignments and per-leaf area
are retained. Only the additional supporting hierarchy and placement change.

## Bounded compilation, demand and replay

AttachedCrown preflights the complete child count before CompactCrown/RNG work.
Reuse existing scalar TwigClusters to compile child origins, frames, counts,
IDs, radii and parameters. Exactly32 local Shoot prototypes are required,
representing176 local leaf rows; no90000-row near crown or leaf mesh is built
during compilation. A catalog prototype contains at most6 leaves here; the
general existing prototype limit remains16×leaf_limit, at most96 in this study.

The complete camera-independent product contains source carrier data and child
coefficients/prototypes. Hashes bind all arrays and recipe/source identities.
This trusted experimental NPZ is not a native or general-purpose cache format.
Store prototype-node/wood-end boxes, transform them by each child's rigid frame,
add a rotation-independent analytic blade reach and union by original host.
The retained carrier bounds are included, with outward Float64 guards.
Bounds precede local pose output and refusal; Float32 remains unadmitted.

A selected-host query preflights total leaf capacity, groups children by the
32 existing prototypes and composes at most256 children per batch. Source leaf
IDs/variants are scattered back to canonical order. Empty/partial/reversed
host requests do not change product identity or placement. A fresh reader
forbids source growth, TwigClusters regeneration, Shoot detail and RNG.
The diagnostic consumer can read its small demand sidecar before any detail
file; a tested zero-demand case succeeds with every detail file absent.
The Blender form reader loads the complete coefficient product once; its
scene build is not the same path as that sidecar-first cost experiment.

## Independent evidence

The complete scalar TwigClusters result matches the compiled whole/alternating/
empty queries bit-for-bit in all six fields, including nodes and twig owners.
All90000 IDs and variants survive. Pose SHA256 across those six fields:
43c7e3fdc05799fd5adc61393b73ed9e35515e07bcffc7f2c10dd11afccbdd33.
Compiled product identity:
c0682648fb9204fc14009a0e73d6b963dcb685107af278f79f3b5faa2792eaeb.

Independent circular sin/cos equations check all16120 attachment positions:
maximum3.55e-15m; orthogonality/determinant errors at most1.12e-15.
Petioles meet their actual leaf nodes within3e-14m. All2,810,160 transformed
coarse blade/child-tube vertices fit their host bounds; maximum signed violation
is -.0557058m. Unit tests additionally use fine blades. No claim that every
fine vertex in the whole crown was independently expanded for this check.

Eight new controls cover scalar whole/partial agreement, finite prototypes
without mesh construction, early refusal, fresh parameter replay without RNG,
actual fine geometry/anchors/frames, zero inventory, missing detail files and
corrupt-product rejection. Existing TwigClusters, growth/wood/spine and current
pose/material tests run alongside them; terminal scope belongs to the PR.

Local SIGGRAPH2009 *Self-organizing Tree Models for Image Synthesis*, PDFpage3,
Figures3–4 is optically checked: explicit metamers/bud fate and environment
competition are distinct structural processes. This study does not implement
that simulation or establish new botanical traits from it. SpeedTree's supplied
Oak_Hero_Field preview is reopened: mature forks, rich bark and real leaf
appearance still exceed this source. Its scientific-name metadata is not an
independent living-tree reference.

Actual Ghidra12.1.2 read-only/no-analysis runs complete GetGenerationSeed
0x20a1bb0(size12ahex) and ComputeNodes0x20a4220(size1680hex). The first reads a
generator property and clamps its result; the second sorts node work, invokes
node computation, gathers linked generators and updates spatial bounds.
They do not establish SpeedTree's branch morphology or its exact seed-ID rule.
Our geometry/seeding remains the earlier own implementation. Binary SHA256 is
9bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c;
diagnostic C hashes dcb80a24445d84bb1bfc3fda21702ad3888a3022a031d3ac6c7fd14bb1cc1772
and64d68b04ab2c96223cf119cccbdb308d1269ace72a898f82e681ad333df764d5.
No vendor code/assets enter generated products or the repository.

## Cost decision: compact is not the fastest near cache

Complete coefficient arrays use3,003,704B, including the retained source.
Uncompressed NPZ is3,054,364B, versus12,330,000B of complete pose rows and
12,331,488B in full NPZ. The compact payload is24.36% of full rows. A small
common demand sidecar and file/container headers are additional costs.
One normal-priority initial compile took2.090s and an in-memory whole query
.03906s. The later instrumented preparation took15.544s at nice10 beside
rendering/other work; its scalar reference took9.032s. These unlike load states
do not establish a compile or scalar-query speed ratio.

Compare three fresh consumers with equal output, warm OS file cache, normal
priority and three observations per case. Guard setup is timed separately;
diagnostic SHA consumes every output byte without a whole-buffer copy and
forces mmap pages, with its time separate from load/query. All three methods
return identical digests for all90000, near1904 and empty0 leaves.

| Demand / method | Median load/query ms | Median byte consumption ms |
|---|---:|---:|
| All / compact | 112.118 | 16.926 |
| All / full NPZ | 31.809 | 21.390 |
| All / full mmap | 10.049 | 17.352 |
| Near / compact | 60.843 | .400 |
| Near / full NPZ | 26.465 | .447 |
| Near / full mmap | 23.099 | .446 |

Zero demand skips all detail IO, about3.8–5.2ms for the common fresh metadata
reader here. Do not rank tiny differences or claim isolated/device timings:
Blender and transport validation share this four-logical-CPU WSL host.
These are product read/composition observations, not frame times. Metadata,
hundreds of NPZ members and validation are part of the compact cost.

DHAT3.24 compares the same all-demand processes, including common Python
startup/imports, guards and final byte consumption. Profiling wall time is not
used. No origin/source reconstruction or Blender is in these processes.

| DHAT measured heap work | Compact | Full NPZ | Full mmap |
|---|---:|---:|---:|
| Cumulative allocated bytes | 147347892 | 107979442 | 81967741 |
| Allocation blocks | 44230 | 38050 | 38016 |
| Peak live heap bytes | 25615841 | 21961717 | 9143820 |
| Instrumented heap reads B | 437761304 | 299213013 | 260203404 |
| Instrumented heap writes B | 188182756 | 128773771 | 102471516 |

Separate traced allocation peaks for all demand:17,532,880/13,494,787/681,666B;
near demand:7,558,963/13,495,496/1,234,780B. Traced live block counts are not
cumulative allocations. mmap pages and OS/driver memory are not heap; actual
post-consumption process high-water RSS is roughly49–67MB across these cases.
No memory metric is substituted for another.

Retain the compact hierarchy for bounded construction, smaller stored plans and
deferred detail. **Choose full mmap in this measured comparison when CPU read
cost is the criterion.** Compact composition is not admitted as its speed or
allocation replacement. Nearby poses can be retained after their first demand;
neither approach implies regeneration per frame. A production format, eviction
policy or device-specific split is not selected by these trusted NPZ controls.

## Image judgement and remaining work

All views use original procedural leaf colour/height and source normals, generic
smooth wood, the same neutral studio light and2560×1440/32 samples. Shadows are
disabled; camera-only selection is therefore allowed. The isolated pair has
the same28 physical blades and parent carrier, without other crown context.
The near pair uses an identical2654-ID fine-detail union, hash
e9da56be4b7029cda6cf8e0544e8a74db0793394d7b58bde96d9932735e0ff1b:
each ID has the same blade resolution on both sides. Whole views retain all
90000 blades at8×1 resolution,3.06M leaf triangles,357840000 expanded array bytes.

Added coarse child tubes cost515840 triangles and56484480B diagnostic arrays
for the whole tree. Parent/C1 geometry is unchanged. These costs are real;
affine radius-basis reuse avoids16120 separate mesh-kernel calls during drawing,
but does not remove GPU geometry. The full Blender meshes are diagnostics,
not a runtime draw/instancing recommendation.

The isolated view spreads leaves along real child axes rather than one fan.
The near view has fewer repetitive radial clumps and a more connected leafy
volume. Whole-canopy coverage changes because overlap changes, not because
blades were added or inflated. Some leaf grouping still repeats. The central
leader, bare lower seed limbs, thin child junctions, generic serrated lamina,
flat colour and smooth bark remain far from the SpeedTree reference. This is
not an accepted Quercus/default or a forest/wind/shadow/temporal/device result.

Actual software rendering costs, terminal states, image identities/hashes and
final source/installed check invocation are supplied in the linked PR and local
reports. Mesa llvmpipe/LLVM19.1.7 is not an RTX3060 device result. Unavailable
geometry-velocity-buffer warnings make motion unverified; static images finish.

Six new images finish with exit0 and are actually opened, alongside the retained
whole controls. Scene expansion and actual rendering, on the shared host:

| View / mode | Build s | Render s | Peak process RSS B |
|---|---:|---:|---:|
| Isolated dense | .234 | 27.532 | 2321444864 |
| Isolated attached | .252 | 24.639 | 2321031168 |
| Near dense | 20.912 | 350.484 | 3859972096 |
| Near attached | 23.668 | 274.854 | 4057518080 |
| Whole attached | 26.802 | 185.873 | 3918761984 |
| Oblique attached | 42.778 | 155.717 | 3906351104 |

Source loading and matched-detail mask preparation precede these scene-build
timers. Builds/tests, Ghidra, profiles and transport checks overlap portions of
the work; times are not an isolated geometry or rendering speed ranking.
EGL_BAD_MATCH fallback messages and all velocity warnings remain in the logs.
PNG hashes in table order:
06b43b5aa8cbca5425fe0b8b58ec8e83298c163c733f4317e2b68424414d3211,
5f5fdb24de1cadc9f718c755c8ddb9e0e5aad4611e47b71a0a1a9fed68eedbda,
949124cba4ac22f0c00fe2b4c532c4f106ae8e0363734fba68a0078bab549b10,
782f62b330d058d13b75266b183f8b0d3653f4cb965d3a7c37a0324d8cf82a44,
30af0d6a3fd16d391aac8c66e17144039a3116ea597c20847c79323c11acf278,
15ba593f69165224367f701981d8b8b80c12a5b37aed0ad576b93b0e200f3d42.
The reports identify ab36fd4c1 plus the working experiment delivered by this PR;
later formatting/verification additions do not change the studied geometry.

## Reproduction

Use `.venv` with NumPy/SciPy/Matplotlib/Pillow/bpy5.2.2. TMPDIR=/tmp,
PYTHONDONTWRITEBYTECODE=1, OPENBLAS_NUM_THREADS=1, OMP_NUM_THREADS=1;
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation:test/experiments/render.
All caches/config/output directories belong under/tmp. First reproduce the
`limb` source using [subcrowns.md](subcrowns.md); no private input is required.

```sh
.venv/bin/python test/experiments/vegetation/attached_crown_probe.py --inputs /tmp/outshine-vegetation-subcrown-replay/products/limb --output /tmp/outshine-vegetation-attached-replay
LIBGL_ALWAYS_SOFTWARE=1 .venv/bin/python test/experiments/vegetation/render_attached_crown.py --inputs /tmp/outshine-vegetation-subcrown-replay/products/limb --study /tmp/outshine-vegetation-attached-replay --output /tmp/outshine-vegetation-attached-replay/near-attached --mode attached --view near
.venv/bin/python test/experiments/vegetation/attached_crown_cost.py --study /tmp/outshine-vegetation-attached-replay --method mmap --demand near --output /tmp/outshine-vegetation-attached-replay/mmap-near.json
.venv/bin/python -m unittest attached_crown_checks twig_cluster_checks
```

Render dense/attached in isolated/near/whole/oblique views into separate empty
directories. The matched old whole control is also retained from #736 with the
identical source, camera, material and old body/leaf path. For allocation work,
add `--trace`, or use `valgrind --tool=dhat --dhat-out-file=/tmp/<name>.json
--log-file=/tmp/<name>.log` before the Python cost invocation.

Current private proof, original inputs, read profiles and opened PNGs remain
at `/tmp/outshine-vegetation-attached-ab36fd4c1/` and its referenced #736 source.
No server/gallery or diagnostic images are committed. No lib/native/default
change is needed for this experiment; shared library promotion needs its own
consumer/module migration rather than an import from experiments.
