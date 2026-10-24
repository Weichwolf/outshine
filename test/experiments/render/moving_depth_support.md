# Motion-depth counterexamples and geometric occluder controls

Research [#1081](https://github.com/Weichwolf/outshine/issues/1081), under
[#1078](https://github.com/Weichwolf/outshine/issues/1078). This is a bounded Python
reference and input diagnosis. The complete native current-occluder comparison is
still open: native point depth and cluster spheres do not provide opaque surface
coverage. Engine is supplying the additional original-geometry/material contract.
No product renderer, GPU pass, cache format or LOD bound changes here.

## Actual native depth pairs

Use Engine's unchanged #1078/6328267363 inputs, native frozen core `3d6f5b0e3`
plus its published FrameIO/SPIR-V package. These are diagnostic inputs, not a new
Master world gate. The independent reader checks the binary 60-record camera audit,
JSON matrix equality, actual cull-clip words, original depth byte sizes and hashes.
Only frames 59/60 and their declared 1280×720 D32 fields are consumed; spare GPU
allocation capacity and cluster spheres are not interpreted as filled geometry.

For each nonzero old pixel centre, unproject through the original Float32 matrix
(promoted to binary64), apply the recorded relative-world translation, project with
the new matrix, and splat into its containing target cell using nearest reversed-Z.
NDC Y is `1-2*(y+.5)/height`; depth zero remains clear. Batches have at most 65,536
points. Current full depth is used **afterwards only for diagnosis**, never candidate
repair, mask selection or hole filling. This point control is intentionally unsafe
for final occlusion rejection.

| Native input | Source points | Reprojected occupied pixels | Candidate on current clear | Current without candidate | Maximum reverse-Z overshoot |
|---|---:|---:|---:|---:|---:|
| Feldkirch rotation |611266|476733|119|133659|0.0000826780604|
| Tokyo translation/rotation |593991|491611|195|100472|0.000696723949|

At diagnostic overshoot values 1e-6/1e-5/1e-4, pixel counts are 3273/435/0 and
13859/1546/467 respectively. These values characterize errors, not acceptance
thresholds. A real thin opaque triangle independently shows why a depth point's
containing target cell need not have any covered centre: camera motion changes one
old hit into a current miss, while point splatting still fills the target cell.
Point agreement or unchanged generation cannot establish area coverage/rigidity.

Three untraced processes and a separate trace reproduce identical candidate words,
source hashes and implementation hashes per native pair. Binary64 point transfer
medians are 221.629/225.623 ms; read/hash/schema validation 39.427/41.216 ms. These
are CPU diagnostics, not a shader replay, cold SSD/asset-DB loading or GPU timings.
The output array is 7372800 B; source indices 4890128/4751928 B; largest explicit
batch payload 8881216/8881328 B. Scoped traced peaks are 25081145/24943057 B.
Temporary expression/native allocations and physical copied bytes are incomplete.
Raw process `ru_maxrss` ranges 76.7–2217.6 MB across launch modes, with two direct
launch outliers; attribution of the full process peak is unqualified. No application
or target-device memory-budget claim follows from these diagnostics.

## Geometric support, explicitly stronger input

Compare three controls at the current pose:

1. Select current eligible front-facing original triangles by projected area, then
   rasterize them as a cheap depth seed.
2. Rasterize the old selected **original triangles**, retaining only source/face
   identities whose geometry remains valid and static.
3. Transfer old-view tiles whose entire four-corner footprint lies strictly inside
   an actual eligible original triangle. Intersect old corner rays with that
   triangle's actual plane. At the new pose, rasterize those geometric quads.

The third control needs original edges/planes plus source validity; **it cannot be
recovered from native point depth or bounds alone**. Interior barycentric guards
leave uncertain edge tiles open. A 65,536-tile capacity bounds snapshots; unsupported
or changed source support remains absent. Selected original triangles are a direct
alternative when their geometry is already available. Near-crossing seed triangles
are omitted by the existing CPU rasterizer; near-plane clipping, native sample
rules, Float32 rounding, alpha/transmission and general material eligibility remain
separate contracts. This is finite Double geometric support, not all-input interval
certification or a native raster/PBR proof.

The old complete 576-source/14169-triangle real cell is retained unchanged, under
the explicitly labeled opaque-control assumption. At a controlled 500 m eye, even
**full current depth rejects zero source groups** for rotation/translation/combined
poses. It cannot establish occlusion performance. With 64 seeds, 16-pixel support
tiles yield zero quads; 1-pixel tiles yield 6797 quads/13594 triangles/706888 B and
still no source rejection. Further tuning on this non-discriminating input stops.
Its source SHA256 remains `31e296fdda1e8ffebba3303bf28f35e4de99c29410e19bc9983a6e375696d3da`.

## Discriminating geometry and complete candidate work

A separate analytic control has two opaque wall panels at 8 m, a 0.06 m opening,
and 200 independent rear patches at 24 m: 202 sources/404 triangles. These are
explicit test metres, not reconstructed native content. At 1280×720 and focal
691.5535657096198 px, compare 6° rotation, 6 m translation, both, source removal,
20 m source movement, and a 5% focal change. Modified sources release old support;
the original current geometry remains the full reference. The metadata query uses
the existing four-level conservative CPU hierarchy and its unchanged depth bias.

Every method rerenders retained original geometry. All six poses × three methods
have **zero changed original winning-face pixels and exactly equal final depth**.
No full-reference result participates in candidate selection. This is a fixed
pixel-centre opaque geometry control; it does not establish shader/material/shadow
equivalence, worldwide performance or continuous-motion coverage.

| Pose | Full frustum triangles | Current seeds: prepass+late | Valid old triangles: prepass+late | Geometric tiles: prepass+late |
|---|---:|---:|---:|---:|
| Rotation |404|48|48|6816|
| Translation |404|28|28|6844|
| Both |402|26|26|6854|
| Source removed |402|226|224|3598|
| Source moved |402|226|224|3598|
| Projection |404|48|48|6846|

Four selected old triangles produce 3360 geometric tiles, 6720 prepass triangles
and 349440 B of quad/owner arrays (half are released in the two source-edit cases).
Original geometry arrays for this entire analytic scene occupy 19392 B positions,
9696 B indices and 3232 B source owners. Tile construction median is 6.628 ms;
additional traced peak 1373461 B. Metadata/IDs and Python-object overhead are separate.

Both a depth prepass and the subsequent original colour-geometry submission count.
The CPU candidate interval includes seed selection/rasterization, source-bound and
hierarchy/query work, and retained-original geometry rasterization. Full-reference
rendering, comparisons, hashing, image/file export and diagnostic potential queries
are outside it. Old support acquisition is charged separately, not presumed free.
Source bounds are reconstructed from held geometry in this control; this is not a
proposal to decode all Fine assets before demand selection.

| Pose | Current triangles ms | Valid old triangles ms | Geometric tiles ms |
|---|---:|---:|---:|
| Rotation |601.680|586.096|549.784|
| Translation |386.855|396.165|336.738|
| Both |357.057|345.904|384.644|
| Source removed |392.251|383.097|322.006|
| Source moved |389.054|388.377|320.762|
| Projection |573.635|577.702|642.437|

Three untraced diagnostic processes, fixed method order, plus a separate allocation
trace; Linux/i5-1135G7, Python 3.13.5/NumPy 2.5.4, one BLAS/OMP thread, nice +10 on a
shared host. These diagnostic CPU rates are not native performance admission.
The CPU rasterizer enumerates large triangle bounding rectangles: tiles can reduce
its pixel-pair work and temporary memory despite submitting many more primitives.
For rotation, 4194304 versus 3138552 candidate triangle/pixel pairs are tested.
This CPU artifact does not rank hardware rasterization or prove a global winner.
With query/late rendering included, all methods have maximum scoped traced peaks
around 116.3 MB, despite smaller tile-prepass peaks. Whole diagnostic process RSS is
218.8–224.5 MB. All internal allocation counts/copied bytes remain incomplete.

Decision: point splats are not admitted as conservative occluders. Geometry-backed
support is valid in the tested opaque controls but costs more primitives and stores
more than these original triangles. Original geometry/current seeds are a simple
native candidate; **no overall/native winner is selected** from these CPU timings.
Next: the supplied native opaque geometry/material/placement/validity contract,
then the real new-frame case and device acquisition/query/draw cost. The broader
#1078/#1081 comparison remains open.

## Verification and reproduction

21 focused tests pass (0.196 s), including an independent scalar raster oracle,
analytic world-to-clip transfer, exact rational footprint-edge checks, thin-triangle
failure, rotation/translation, source invalidation, bounded support and near-plane
unknowns. All four analytic total-cost workers terminate 0; 34 saved array fields
repeat bit-for-bit. Eight native point processes terminate 0. Six source-motion
depth comparisons, native current/error maps, both original native final images
and the complete analytic support sheet were opened. No images published.

Point producers `ee8204511`/`69782dc1a` have identical measured three-file hashes.
Real-cell controls use `69782dc1a`; analytic total-cost producer is
`fc00382850bead23bad3c152c29821f36ff13bff`, based on Master `f81132553`.
Early analytic reports time only the prepass and are superseded for total-cost
conclusions; their unchanged arrays remain controls. Reports name exact hashes.

Use existing `.venv`, NumPy/Pillow, no added package. Set
`PYTHONPATH=test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`.
Run `python -B -W error -m unittest test_depth_reprojection verify_current_visibility -q`.
On frozen sources run `compare_depth_reprojection.py --input <native-case> --output <fresh-temp>`
for each #1078 pair. Run `compare_occluder_support.py --control --seeds 4 --output <fresh-temp>`;
repeat untraced and separately `--trace`. `--input <original-cell.json.gz>` selects
the real-cell control; `--block 1` is its separately labeled support-density check.
Saved NPZs hold original face IDs and candidate depths; JSONs hold hashes/work/costs.
Private originals/results: `/tmp/outshine-research-moving-depth-53946a622/`.

SIGGRAPH-first: [GPU-Driven Rendering Pipelines (2015)](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
pages 26/52, visually inspected. Page 26 combines selected current occluders and
reprojection; page 52 separately refreshes current depth and retries deferred work.
The established [two-phase reference](https://github.com/Weichwolf/outshine/blob/8ab3aa077/test/experiments/render/two_phase_occlusion.md) on the research branch
retains its own scope; it is not silently copied or claimed integrated here.
