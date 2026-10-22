# Relit attributed roof prisms and compact ready records (#1094)

This extends the existing #888/#895/#897 comparison rather than rebuilding its
geometry/material algorithms. Complete stored height surfaces preserve lighting
much better than #1088's raised flat roofs. A direct compact layout reduces the
old product's array payload while keeping its outputs unchanged. Known native
coverage/material counterexamples and a visible night discrepancy remain;
**this is not an admitted native replacement or a frame/device winner**.

## Existing method and changed work

The original versioned complete native cell has 576 sources / 14,169 triangles.
The unchanged attributed-prism assembler represents 557 sources as 586 convex
solids / 4,327 planes. Nineteen unsupported/nonconvex/holed sources keep all
887 original triangles and their fields. No hull replaces a rejected footprint.
The existing source-owned normal/gable material-code pairs, continuous coordinates,
metres and fallback RGBA remain. Native Part/Face IDs are unknown and stay unknown;
analytic source/part/plane identifiers are their own representation contract.

`compact_attributed_prisms.py` changes storage and integer lookup, not represented
surfaces. It stores only the 2,353 wall edge/bay rows and their primitive offsets.
Floor/roof rows no longer occupy zero edge records. Material code pairs share a
dictionary, and integer tables use an exactly fitting native NumPy integer width.
Primitive bounds already represented by BVH leaves and unused roof flags are
omitted from the ready product. Fallback source IDs replace unnecessary dense
native-binding columns; known fallback parts and original triangle IDs remain.
The equal-requirement exact opponent applies dictionary sharing and fitting
integer widths too. No bit codec, lossy quantization or resolution sweep is used.

`attributed_prisms.py` keeps one attribute evaluator. It selects dense or indexed
wall/code records without expanding compact tables during load. Edge calculations
address wall hits; explicit material IDs never pass through recomputed packed U.
The fallback and all-unsupported paths retain original fields, including alpha.
The source-relative Gable convention remains the existing own analytic contract,
not recovered native patch identity at an eave. The old dense compiler remains
the default; compact output is explicitly requested by the new experiment.

The ready ray path needs no BSC3/form decoder or GEOS. `roof_prisms.shape_planes`
now imports GEOS only when actually constructing geometry. Master's unchanged
#1092/b2a7d636b package import fix is cherry-picked as e9271366c; there is no second
package-entry implementation. Three upstream import tests pass on this base.

## Complete fair storage controls

`prism_field_storage_control.py` reuses the existing exact post-vertex field
factorization: original Float32 positions/coordinates/metres/RGBA at every corner,
packed flat normals, explicit material codes, triangle order and complete source
identity. All represented bits roundtrip. Normal/code pairs share a dictionary;
source membership uses exact runs. White RGBA is implicit only where all values
are actually white. This control does not charge mandatory native Part/Face tags
that the candidate does not preserve. Original triangle IDs are implicit in its
unchanged order; candidate fallback IDs remain explicit.

| Stored array payload | Bytes |
| --- | ---: |
| Existing attributed product | 594,035 |
| Direct compact attributed product | 428,888 |
| Exact original post-vertex/source-ID control | 450,372 |
| Original supplied native fields/indices | 1,028,316 |
| Same original supplied fields, narrower exact indices | 943,302 |

The compact product is 27.80% smaller than the previous product and 4.77% smaller
than this explicit exact control. These are actual array payloads, **not** a
storage lower bound, GPU layout/residency or an equal-native-quality victory.
NPZ/container compression, CPU derived caches and input/diagnostic memory are
separate. Fully qualified native material/filter/opacity/caster/transition
contracts and device times are still missing. Better exact layouts are possible.

No table is expanded at ready load. The consumer keeps 2,353 edge rows rather
than 4,327 and 176 code-pair rows rather than 4,327; omitted primitive bounds stay
empty. As before, 61,260 extra CPU bytes cover decoded fallback normals,
vertex-stage coordinates/categories and the shared white RGBA value. Rtree/library
acceleration, query arrays and interpreter allocation add memory beyond these.
The assembler still constructs its existing dense arrays transiently before
compaction. This does not prove a cheaper cold generator or native asset pipeline.

## Actual lit images

`prepare_prism_lighting.py` assembles, saves and reloads the ready product **before**
creating diagnostic triangles. `prism_diagnostic_mesh.py` reuses the independent
plane-triple/Qhull polyhedron construction. Its outward faces are associated with
actual represented planes; maximum residual here is 7.85e-10 m. It preserves
fallback vertices/normals/RGBA and all 576 source identities. The resulting
10,125-triangle model is private verification data, not the proposed runtime
product or evidence of hardware raycasting performance. Product queries still
use the existing BVH/plane clipping and material evaluator.

All 21 original/prism pairs use 1280×720, verified matched cameras, shared light,
ground, source colour and neutral roughness/metallic parameters, with 16-sample
Eevee. Poses include the usual static20/moving20/moving21, known failing moving23
and static25, a 500 m near control and a roof-facing far control. Day, backlight
and night are all present. Every resulting sheet was actually opened. Reused
original input geometry/normals/RGBA/camera/ground are checked exactly for four
poses. The upper original controls were rerendered because Double camera inputs
differed at rounding scale. All 29 new matrix renders plus the initial night
render completed with status 0; 42 final images use only matched opponents.

| Pose | Max RGB8 error day / backlight / night |
| --- | ---: |
| static-020 | 1 / 0 / 0 |
| moving-020 | 1 / 0 / 0 |
| moving-021 | 0 / 2 / 0 |
| moving-023, known corner switch | 1 / 3 / 1 |
| static-025, known coverage loss | 3 / 2 / 17 |
| near control | 5 / 7 / 3 |
| roof-facing control | 1 / 4 / 1 |

The original static night/prism pair is pixel-identical; the earlier raised flat
profile differed by up to 38 RGB8 levels. Roof-facing day improves from the flat
profile's maximum 19 (normal atlas 12) to 1. These are finite controls. In
particular, static25's night discrepancy remains visible and must not be hidden
by ordinary-view averages. The near control is not an original-identical result.
Changed-pixel counts and full-frame/changed-region errors are retained in raw
reports; the common sky does not turn a whole-frame mean into an image gate.

This common neutral shader does **not** implement the native procedural material
programs/UV filtering, emissive windows, terrain/weather or native opacity/TAA/
MSAA/caster/reflection behavior. Full coordinate/category comparisons are made
independently by the ready ray evaluator, not inferred from white pictures.

## Independent old/new and known-error evidence

`verify_prism_lighting.py` imports the immutable legacy module from f33125f03 and
compares it with the direct compact consumer on the same rays. All 122 supplied/
additional poses are reported, including offscreen cases; 448,936 visible ROI
samples compare exactly in depth, source, part, primitive/plane/fallback identity
and every code/coordinate/metre/normal/RGBA result. Diagnostic work counters are
excluded because their operand model was refined. The first smaller independent
ROI run also checked 216,512 samples. Both are old/new equivalence evidence,
**not native equality**. No raw error or tolerance was removed to pass.

The final original-reference counterchecks reuse the original triangle/Rtree
reader plus exhaustive triangle-ray controls. Moving23 retains one same-source
material switch and 91.285° normal difference, even though coordinate changes
are small. Static25 still loses one quarter-pixel sample. Its original-source
hit count/source/material/field/depth results remain in saved NPZs. Native
Part/Face labels stay unknown. These old counterexamples are explicitly retained
alongside the new real PBR images rather than replaced by a more convenient pose.

Four focused new controls check represented face normals/depth against the
independent mesh, dense/compact exact fields and absence of re-expansion, and
nonwhite RGBA fallback with a fresh reader whose form/GEOS imports are blocked.
Eighteen new/existing prism tests pass; three upstream import tests pass.

## Measured cost and decision

Three fresh ordinary processes per path read ready products and query the same
6,600 rays from static20, moving23 and the roof view. They include clipping, full
fallback and attributes, not only a material/prepass kernel. On this Linux host
under shared development load, median load/query times are:

| Consumer | Load ms | Whole query ms |
| --- | ---: | ---: |
| Original f33125f03 | 13.75 | 2,900.40 |
| Current dense | 14.84 | 3,005.88 |
| Direct compact | 17.11 | 3,060.81 |

These observations show **no CPU speedup**; they do not select a native winner.
Ordinary raw process peak RSS spans approximately 125.5–126.3 MB, including
libraries. A separate compact trace reports 8,034,308 peak traced bytes and
127,541,248 raw peak RSS bytes; tracing changes timing and does not instrument all
native-library allocations. This is not an application memory budget.

The representative query performs 559,636 node-ray tests, 15,532 primitive-ray
tests and 117,679 plane-ray pairs, with 1,554 analytic material hits and 300
fallback field queries. Its output field arrays occupy 607,200 bytes. Logical
operands are 30,628,256 box/plane bytes plus 281,163 material-record bytes for the
compact consumer. These are declared record operands, not physical DRAM traffic
or copied bytes. Cumulative allocation/copy traffic and GPU work are unmeasured.

Retain the direct compact path as an explicit research alternative, not a default
speed upgrade. It reduces ready memory without losing old field outputs and
supplies much better actual lighting than raised flat roofs. Native first-surface,
filter/material/caster/transition quality and native end-to-end work still decide
whether this family is useful. The visible static25 case and existing corner
switch remain red. No view/content/profile/budget condition was weakened.

## Sources and reproduction

Prior source/algorithm assumptions belong to `roof_prisms.md`,
`prepared_surface_recipes.md` and `attributed_prisms.md`. SIGGRAPH2000 *Relief
Texture Mapping* was read, including its displacement/parallax discussion and
page2 diagram/equation; it explains why shading alone cannot recover displaced
visibility. This implementation uses existing convex clipping, not that paper's
texture warp. No third-party code or photo textures were copied.

Use the existing `.venv`, `PYTHONPATH=lib/src:test/experiments/render` and one
BLAS/OpenMP thread. Inputs are the versioned building-full-view fixture. All
outputs use fresh explicit system-temp paths; no private input is required.

```
python -B -W error -m unittest test_prism_lighting test_roof_prisms test_attributed_prisms
python -B -W error test/experiments/render/prepare_prism_lighting.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz --output /tmp/outshine-prism-lighting
python -B test/experiments/render/render_far_height_profile.py \
  --scene /tmp/outshine-prism-lighting/static-025/prisms/scene.json \
  --lighting night --output /tmp/outshine-prism-night
```

Extract the legacy own-source module with
`git show f33125f03:test/experiments/render/attributed_prisms.py` into a temp file,
then run `verify_prism_lighting.py --input <fixture> --product <prepared-dir>
--legacy <temp-source.py> --output <fresh-temp-dir>`. `bench_prism_consumer.py`
accepts a ready NPZ, saved origins/directions NPZ, optional legacy source and an
explicit report output; `--trace` is separate from normal timing. The ready
benchmark imports no form/GEOS implementation. Software headless image runs use
`LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless`, low priority and no native timing
claim. No new dependencies or native code. Evidence:
`/tmp/outshine-research-prism-surfaces-f33125f03/`. Images remain local.
