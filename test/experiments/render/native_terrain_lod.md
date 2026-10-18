# Native local terrain LOD prototype

Owner: [#515](https://github.com/Weichwolf/outshine/issues/515), native port [#328](https://github.com/Weichwolf/outshine/issues/328).
The implementation is on `outshine-metal-native-integration`; master retains its
integrated SDL renderer until the port meets the device/image requirements.

## Representation and selection

The existing eight-cell patches use four shared index ranges, with strides
1/2/4/8. Boundary fans keep every original perimeter edge at every level.
Original outer skirts and shadow grids remain complete. No internal skirts,
additional source downloads, reduced coverage or per-patch CPU draw calls.

GEOS builds each uniform common-refinement stencil once per process, through the
existing installed library. Height errors are reduced once per uploaded page;
world bounds/errors are reused with the page revision and tile transform.
For bilinear horizontal coordinates A + Bu + Cv + Duv, precomputed monomial
residuals bound the XY deviation and spherical sag. Height, sag and XY bounds
are transformed conservatively into world displacement. Native tests include
nonaffine corners, sag and a nonorthogonal affine model transform.

GPU culling reuses the existing projected box and current opaque depth. The
projection displacement bound uses matrix-row norms, maximum projected extent
and minimum clip W. Nonpositive denominators, stale bounds and invalid data
retain the original mesh. The prototype allows at most 0.5 pixels of additional
Euclidean raster displacement relative to its resident fine surface; it makes
no smaller claim about the source terrain's existing error.

Only uniform sample fractions can select coarse meshes in this prototype.
Arbitrary existing fractions retain the exact fine representation. The four
LOD streams reuse the block-local scan and block-total scan, with disjoint
buffer regions and four indexed indirect arguments. Stable order remains
inside each level. Layout size/alignment/offsets are asserted at CPU/GPU boundaries.

The coarse fragment path reconstructs the original fine, normalized vertex
normals and triangular interpolation from the height atlas. This preserves
normal detail independently of geometric stride. Fine patches retain their
existing interpolated normal path. Material families and shadow representation
are unchanged. Halo samples and all atlas array layers remain available.

## Evidence and limits

- 1,024 real Körbersee height patches, 3,072 comparisons against Python:
  maximum absolute height-bound difference 4.6811e-11 m.
- Independent dense interpolation/topology checks cover winding, complete area,
  every fine boundary segment, omitted peaks and invalid heights.
- Native GPU checks verify four argument ranges, stable compaction, current
  depth, page revision failure and a flat surface selecting the coarsest level.
- An orthographic nonaffine-height image compares coarse geometry with the
  independently drawn original grid: identical coverage and normal-target
  differences at most 1e-6. This does not establish arbitrary-camera equivalence.

Initial uninstrumented fresh offline processes, 1280x720/30, exactly 30 frames:

| Place | GPU mean / max ms | Complete process s |
|---|---:|---:|
| Koerbersee | 22.420 / 41.912 | 5.023 |
| Tokyo | 19.757 / 30.879 | 4.033 |
| CentralPark | 19.818 / 28.209 | 3.192 |
| Wien | 20.286 / 30.877 | 3.399 |

These runs do not establish a uniform net speedup or device acceptance.
The instrumented first Körbersee terrain pass submits 5,152,696 triangles
(including skirts), down from 6,550,016: 21.33% less. Its vertex/fragment
intervals still overlap at about 32 ms. Those intervals cannot by themselves
identify a vertex bottleneck. Isolate terrain fragment cost before expanding
geometry selection again. Process time includes preload, render and PNG writing;
it is not preload-only time. Source/cache acquisition remains separately measured.

The extra metadata is explicit: 256 bytes per height page and per source tile,
plus 16 bytes per active patch bound. GPU flag/slot/selected streams now hold
four uint regions, not one; `HeldBytes` includes those buffers and staging.
This is additional memory, not a saving inferred from fewer triangles.
Full CPU/GPU/device budgets and repeated image/cost comparisons remain open.

## Follow-up isolation

All twelve native Places were rendered at the same profile on f94fadd23. Eight
GPU maxima remain below 33.333 ms; Koerbersee 43.410, Rosenheim 36.372,
Feldkirch 34.015 and Malcesine 34.122 ms remain above it. This is a measured
integration trend, not uniform improvement or completed device acceptance.

Temporary shader-only diagnostics preserve geometry and the full camera turn.
They change material/normal output and can change downstream shading; they are
not admissible quality or budget runs. Original shader hashes and normal PNGs
were restored after every diagnostic.

| Koerbersee diagnostic | GPU mean / max ms |
|---|---:|
| Constant terrain fragment | 19.147 / 34.971 |
| Fine-normal reconstruction bypass | 19.967 / 42.755 |
| Rock detail bypass | 23.978 / 39.425 |
| Substrate detail bypass | 22.368 / 43.085 |

A separate exact factorization of lattice hash multiplications kept the PNG
byte-identical but measured 23.017 / 43.134 ms. No clear speedup: not adopted.
Only timestamp counters are exposed by this A18 Pro's Metal counter API;
triangle references must not be called actual vertex invocations.
The terrain classifier also walks variable seed/edge lists per pixel. Measure
that work before choosing a bounded cached field representation. No cached-field
architecture or classification simplification is admitted by these diagnostics.

## Tighter projection, unchanged positional allowance

`1a45473a8` replaces the row-sum projection estimate with a conservative spectral
norm over the projected rectangle. The existing 0.5 px Euclidean allowance,
source errors, expanded bounds, depth bias, normal detail, geometry patterns,
draw count and metadata layout remain. Nonfinite/underflow cases retain fine
geometry. [Derivation and independent checks](terrain_projection_bound.md).

The same native binary, cached inputs and 30-frame 1280×720 turn submit on average
4,722,099.73 terrain triangle references before and 3,943,008.27 after, including
skirts: (1 - 3,943,008.27 / 4,722,099.73) * 100 = 16.50% less indexed work.
These are submitted references, not unique vertex invocations. The diagnostic
Metal interposer measures work separately from normal timing runs.

Two old/new/new/old timing sequences on A18 Pro include the new culler work.
The first sequence's mean improves 11.82%; the final source's second sequence
improves 7.98%. GPU maxima remain 41.57–42.00 ms after versus 43.32–43.67 ms
before; all remain above the 33.333 ms target. A frozen world gate runs on the
same host, so these are paired development trends, not isolated device acceptance.

Both new captures have identical hashes within their variant. Old and new are
different: 16.53% of pixels differ, mean RGB-channel error 0.400/255, maximum
119/255 at a changed shoreline pixel. Opened full images preserve mountain shapes;
terrain/depth-dependent materials and boundary coverage change with LOD. This is
not a byte-equivalence claim. The foreground vegetation defects are also present
in the integrated master shot; this projection change does not repair them.

All eight existing terrain tests passed with the spectral shader. The final
underflow guard and orthographic/perspective cases pass 566 native checks under
Metal validation. The unchanged shader package contract passes 517 artifacts and
12 compute contracts; four Python checks include an independent double SVD oracle.
All twelve final native Places and remaining budget failures are recorded in #515.

## Command attribution and repeated captures

The opt-in Metal probe now joins stage intervals to commandbuffer GPU timestamps.
Its parser verifies that the captured sky-pass submissions match GPUROW's count,
sum and maximum, within printed-nanosecond rounding. Preparation submissions stay
outside that set. Vertex/fragment intervals and spans overlap; they are not additive.

Körbersee's first measured frame takes 41.301 ms with a 29.695 ms terrain-pass span.
Changing the initial bearing by 180° moves the first-frame maximum to 36.878 ms;
the original first pose later takes 21.413 ms with almost identical submitted work.
Rosenheim's original first frame takes 35.932 ms; the rotated run peaks at frame 11,
29.262 ms. First-use cost matters, but neither orientation nor warm-up alone explains
all variation. Driver residency/compiler/clock causes remain unproved.

A private client repeats three complete turns in one engine. This exposed a separate
reporting error: the final 30-frame shot inherited 90 GPU frames and the old maximum.
The public `Renderer::resetMeasurements()` now resets CPU/GPU frame aggregates and
starts a device/submission boundary without waiting or drawing. Older completions
are excluded even when collected later. The Place client invokes it before measuring.
Both repeated real-Place controls now report 30/30; their GPU sum and maximum match
exactly the final 30 commandbuffer intervals, while all 90 frames actually render.
The native pending-completion test passes 19 checks; removing only its boundary filter
in a private control causes two failures. Geometry, history and images remain owned
by the existing renderer; no warm-up or omitted first frame is added to the gate.

The full f94fadd23 lint completed all 451 C++ units without findings, all 29
repository guards and format. Its final shader audit rejected 21 new native
fragment variants solely because it still required SDL fragment resources in
set 2. The native backends explicitly support shared/sparse graphics bindings
and reject uncontracted storage writes. The audit is migrated to that actual
contract, retaining duplicate/write/array negatives and all compute conventions.
The focused package audit passes all 517 artifacts, 12 compute contracts and
11 positive/negative audit tests. Full integration lint on frozen 672201cc4 subsequently passed: 451 C++ units,
29 repository guards, 2104 formatted C++ files, 517 shaders and 12 compute contracts.
Production source is unchanged from f94fadd23. The broad world gate is running
on that same frozen source; no terminal result is claimed yet.

An additional inverse-depth perspective oracle was rejected: its original fine-grid
control already produced 252 unlocated curtain pixels and a maximum normal delta
8.12e-6, versus 6.67e-6 for the coarse rendering. It incorrectly treated all visible
skirts as top-heightfield samples and assumed CPU inverse projection reproduced
GPU-interpolated varyings exactly. The negative source/logs are retained privately;
the existing exact orthographic normal/coverage test remains unchanged. A stronger
perspective test needs captured interpolated UVs or a proven raster reconstruction
error model. No perspective guarantee is inferred from this failed oracle.
