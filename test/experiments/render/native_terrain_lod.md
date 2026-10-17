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

The full f94fadd23 lint completed all 451 C++ units without findings, all 29
repository guards and format. Its final shader audit rejected 21 new native
fragment variants solely because it still required SDL fragment resources in
set 2. The native backends explicitly support shared/sparse graphics bindings
and reject uncontracted storage writes. The audit is migrated to that actual
contract, retaining duplicate/write/array negatives and all compute conventions.
The focused package audit passes all 517 artifacts, 12 compute contracts and
11 positive/negative audit tests. A new complete integration lint remains required.
