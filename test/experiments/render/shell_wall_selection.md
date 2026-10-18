# Shell base-wall selection before target emission

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12), continuing the
[joined roof comparison](roof_fusion.md). The first bounded wall comparison
removes 6/26 source envelopes across cache-cell boundaries, but only 21/56
triangles in the incomplete roof/base-wall reference. This is another measured
step, **not a native adoption or runtime-budget recommendation**.

## Concrete admitted geometry

The native owner's [narrow Shell contract](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6256008442)
confirms opaque, backface-culled Wall/MetreWall triangles for successful selected
Shell calls, without Fine recesses. The shader changes surface appearance, not
coverage. This is not inferred from ShellError, a ring, or Whole acceptance.

`building_shell_walls.py` requires the original successful **source-specific**
Shell call (`detail=1`, openings false) and explicit retention. It reconstructs
only base-wall panels for Flat, Shed and Hip roofs. Other roof break recipes,
gables, floors, roofs and Fine openings remain outside this wall emitter.
Original outer/hole ring directions and edges remain separate. Native edges
shorter than 0.05 m stay absent. Bottom/deck heights come from original native
`BuildingBottomM`/`RoofDeck` parameters; no foundation height is invented.

The Hip case reproduces `HipBreaksAlong` from the original facet rings and its
precision/edge-length tolerance. Each actual panel uses the native quad diagonal,
millimetre ties-away position snapping, duplicate/cross-product filtering and
ordered Float32 placement. Degenerate local triangles never become coverage.
`building_positions.py` supplies the shared position recipe, also consumed by
the unchanged roof geometry recipe. Float64 roof bit patterns and snapped roof
batch consistency remain checked. Actual shader transforms are a separate bound.

For each group, the existing [exact inner-kernel/slab construction](roof_fusion.md)
works in a chart chosen from the actual triangle normal; vertical walls need no
new visibility theorem. The SIGGRAPH-first extended-projections reference and
independently checked polygon-kernel basis there still apply. No outer hull,
courtyard fill, edge-length inflation or tolerance repair is introduced.

## Bounded comparison

`building_shell_selection.py` chooses at most 32 nearest successful Shell sources
by original envelope metadata. All 32 stay retained, including unsupported wall
recipes. Seeds are emitted first, then up to 256 useful certificates; remaining
unproved work is retained. Their 4×4×4 m observer box uses the original camera.
Opacity defaults to **unknown**, yielding no certificates. The explicit
`--certified-opaque-shell` experiment declaration uses the reviewed material
contract; the source export's `opaque_for_coverage=null` is unchanged.

The unculled reference (named `whole` in the output), independently restricted
per-cell selection and shared hierarchy consume identical source bounds and
certificates. **This unculled reference is not the existing native Whole/Project
algorithm**; its separate #800 measurements remain unchanged. Frozen successful
per-cell calls supply the chosen representation for this offline experiment.
That does not establish a production planner obtaining all required state before
meshing; actual integration must admit bounded seeds without first building all
targets. No complete Fine geometry or
cached native triangle stream is read to choose seeds, build certificates or
decide masks. Only after every mask is fixed are SHA-bound original native
cached wall streams opened as controls. Oriented triangle multisets must contain
every reconstructed seed triangle exactly, preserving position and winding.
These are cell-stream subset controls, not a full attribute/source-ownership or
universal mesher equivalence proof. The earlier singleton source 141 control
also contains its 16 expected base-wall triangles exactly.

| Original parent | Sources | Supported seed sources / wall triangles | Certificates | Hidden whole / cell / shared sources | Reference triangles whole → shared |
| --- | ---: | --- | ---: | --- | --- |
| bbd9aab743 | 457 | 18 / 258 | 59 | 0 / 0 / 0 | 6,801 → 6,801 |
| 6d41e63e73 | 1,754 | 22 / 592 | 137 | 0 / 0 / 6 | 18,720 → 18,699 |
| 035e2150a3 | 4,141 | 29 / 1,028 | 256 | 0 / 0 / 26 | 27,134 → 27,078 |

All three inputs use the same Vienna camera; they do not establish three
geographic load classes. Known envelopes receive the existing conservative
Float64 interval query and 1e-6 m margin; 1/7/40 unknown source bounds remain.
No query exhausts its fixed budget. Joint box tests: 3,110 / 17,372 / 76,977;
plane tests: 192,518 / 973,820 / 4,239,345. This planning work is substantial
relative to the small reference primitive saving. Python diagnostic time and
these counters do not establish native CPU, GPU, allocation/copy or peak gains.
No native draw-call claim follows from the cell/page counts.

## Post-selection visual control and limits

Only after selection, emit all available original planar roofs and supported
base walls for the originally selected Shell sources. Omitted nonplanar roofs,
invalid original polygons and all unmodelled Fine walls stay explicitly counted.
This reference is incomplete; omitted geometry is not a valid empty world.
Actual emitted reference vertices of hidden sources must remain inside their
original source envelopes.

Eight 1280×720 fixed-metadata views per input cover four quarter-turns and a
bounded translation. Depth is exactly equal before/after. bbd9 has eight
nonempty views, 6d41 six, 035e only two (1,886/1,915 covered pixels). The distant
035e geometry occupies a very small region; this is a weak image load case.
Removed-reference-only rasters separately check whether omitted target triangles
would appear without occluders: 035e reaches only **three pixels in one moved
view**, 6d41 reaches **zero** in all eight views. Thus even the nonempty world
reference comparisons provide weak visual evidence for these removals. Actual
images are opened, not inferred from test status. These discrete images do not
replace native turn/device acceptance.

Two additional original geographic packets use their own cached cameras.
The unchanged native #800/#801 exporters run with the existing isolated SDK/
TIFF runtime path, without a rebuild or system-library change. Koerbersee has
14 sources / 18 shapes and 11 exact-view native cache-cell controls; the current
wall comparison hides no source. Flensburg has 3,043 sources / 4,114 shapes and
64 source-form cells, but native cached products at the **exact same** view and
quality radius exist for only 39 cells. The original independently chosen seeds
need 14 supported source controls in seven missing cells. Other-view cached
products are not substituted and seeds are not changed to fit the cache. Native
control validation therefore fails explicitly; that case is **unproved**.
Its current experimental mask removes no source anyway, so this comparison
does not justify new native work merely to fill those control gaps.
Alternate exact-view cache keys are collapsed only after exact wall vertex/index
stream equality, without claiming full artifact/cluster equality. Neither these
inputs nor the three Vienna packets establish the required park-edge load class.

Logical source records, IDs, contacts and source bytes remain unchanged. A native
consumer must independently retain shadow/reflection/interaction/audio demand,
keep accepted seed geometry resident, and invalidate certificates when geometry,
representation, material or transforms change. The observer box alone does not
freeze LOD: the original camera's quality radius may be zero. The reference
neither authorizes holding Shell after a Fine transition nor bounds GPU transform
rounding. Full-world images, neighbors, real motion/changes, work/allocation/copy/
peak scaling and both device budgets remain open under #12/#567.

## Reproduction

Use `.venv`, the original #800/#801 SHA-bound receipts and frozen schema-10
products. Outputs must be fresh explicit system-temp directories.

```sh
PYTHONPATH=lib/src .venv/bin/python \
  test/experiments/render/building_shell_selection.py \
  --surfaces "$original_surfaces" --trace "$original_selection" \
  --products "$original_native_products" --output "$fresh_temp_output" \
  --certified-opaque-shell --reference-images
PYTHONPATH=lib/src:lib/tests nice -n 10 .venv/bin/python -m unittest discover \
  -s test/experiments/render -p 'test_*roof*.py'
PYTHONPATH=lib/src:lib/tests nice -n 10 .venv/bin/python -m unittest discover \
  -s test/experiments/render -p 'test_building_shell_walls.py'
```

Omitting `--certified-opaque-shell` is the real-input unknown-opacity control:
all sources remain selected. Eight new wall tests cover concrete acceptance,
retention, native winding, short-edge gaps, snapping collapse, courtyard winding,
Hip splits, excluded recipes and explicit opacity/frontside behavior. The
existing 26 roof/fusion/position tests still apply, including independent exact
segment oracles and negative dominant axes.
