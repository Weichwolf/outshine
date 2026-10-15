# Native facade masonry composition

Owner: #306, material composition #6, goal #189. The candidate comparison and
sources remain in [masonry_filter.md](masonry_filter.md). This experiment connects
its selected sixteen joint subcells to the existing nine-sample coating and the
actual native wall metre stream delivered by #335 / PR #351.

## Material and input contract

`SurfacePattern::FacadeMetres` is the explicit value2. None0 and Facade1 retain
their previous interpretation. UV0 carries facade metadata; independently
interpolated UV1 carries source-edge U and base-relative V in metres. The public
material block remains384bytes; no new bindings, varyings or vertex formats.
Performance explicitly confirmed the value/ownership contract in #306,
comment6201900393. The full native world renderer remains #328.

Building surfaces register an additional metre-aware wall row. Actual nonempty
`WallMaterialM` selects it; absent data selects legacy walls. A partial/nonfinite
stream remains an error. Piece admission refuses a required but missing UV1
before allocation. Authored UV1 image selection remains independent. Geometry
asset encoding already stores both streams and the enum; no new wire schema.
Impostor preparation copies the physical stream for mode2 only; a resolved card
clears the pattern and consumes captured channels once.

Only procedural wall variant6 activates masonry. Explicit source colour uses
variant0; glass styles/fields and negative roof/plinth/trim codes retain their
existing paths. The physical two-course repeat is250mm by160mm, with80mm courses
and10mm joints. Bounded complementary colour/roughness deltas preserve the brick
area mean; no relief or aging is implemented in this delivery.

The shader computes metre derivatives before branching. Sixteen joint cells
filter the brick mask and opening/band mask together. Existing coating coverage
bounds the final delta, avoiding a second global opening integral. Its four
periodic brick-band terms use flat-region guards. FMA with binary-exact repeats4
and6.25 reduces phase before subcell offsets; it cannot recover lost input bits.

## Reproduction

Build the normal production archives and shaders, then link the harnesses against
that same header/archive commit using the repository compiler/include/link flags.
Required tools: Clang/libc++23, SDL3 and shadercross, Vulkan validation, glslang,
SPIRV-Cross, Python with NumPy, Shapely, Matplotlib and installed `outshine_ref`.
Use explicit system-temp outputs; no images are published under `doc/`.

```sh
export PYTHONPATH=lib/src
python test/experiments/render/masonry_facade_raster.py \
  --source-binary /tmp/native-wall-metres \
  --binary /tmp/native-facade --output /tmp/outshine-material-masonry-raster
python test/experiments/render/masonry_facade_pixel_reference.py \
  --binary /tmp/native-facade --input /tmp/outshine-material-masonry-raster \
  --output /tmp/outshine-material-masonry-reference --legacy /tmp/predecessor-checkout
python test/experiments/render/masonry_facade_cost.py \
  --binary /tmp/native-material --output /tmp/outshine-material-masonry-cost
/tmp/native-masonry-scene /tmp/outshine-material-masonry-raster/wall-0.bin \
  /tmp/outshine-material-masonry-before 1
/tmp/native-masonry-scene /tmp/outshine-material-masonry-raster/wall-0.bin \
  /tmp/outshine-material-masonry-after 2
```

Harness sources are `NativeWallMetresProbe.cpp`, `NativeFacadeProbe.cpp`,
`NativeGroundLitterProbe.cpp` and `NativeMasonrySceneProbe.cpp` under
`test/harness/render/`. The common compute probe's optional `--gpu-timing`
enables existing public timestamps for32 sequential dispatches; default operation
still performs one dispatch with timing disabled. Unavailable/truncated timing
is reported as unavailable and cannot pass the cost experiment.

## Observations and limits

Linux/RTX3060, regular production build a56f3e3be,15October2026:

-24 paired native cases: actual source/cache wall and bounded traced gable,
  two tile anchors, orthographic/perspective and shifted cameras. Wall crops
  show0.6m and4m; gable crops show0.6m. Pure window panes and eight excluded
  category controls remain byte-identical between modes1/2. Source probe:
  3061checks, zero failures. Material raster PDF opened optically.
-The emitted wall includes a deep foundation. The occupied wall view targets
  V=3.85m above its base rather than centring the whole foundation/body quad.
  Gable corners are individually traced onto the same real gable before native
  patch emission/cache; no fake wall or substituted UV encoding.
-The independent reference clips actual screen-space pixel polygons against
  UV0 openings/bands and UV1 brick rectangles separately. Three analytic controls
  precede24 sampled raster cases, including mixed window/band pixels. The fixed
  additional-RGB error limit remains0.015; maximum0.0133591. Diagnostic-field
  parity with production stays below2e-4. This bounds the sampled additional
  albedo delta with local affine GPU derivatives, not full radiance/BRDF error.
-Fourteen unflagged controls also remain byte-identical to the actual c8507e906
  predecessor fragment: near/regular/perspective walls, gables and all eight
  exclusions. This compares mode1 with the old shader, separately from comparing
  the two new material modes.
-The public Engine API renders the same native/cache wall through both modes at
  1280×720 with common directional light and MR shading. Both captures pass23
  checks and their PDF is optically opened. This exercises the current world
  renderer during #328 migration; it is not a full Place or native-world claim.
-115200 matched probes,32 dispatches: baseline sums7.061184/7.053632ms;
  coating+masonry14.055872/13.992352ms, reverse order included. Approximately
  0.2205 versus0.4383ms per dispatch under the recorded concurrent training/load.
  Output is byte-identical with timing on/off. Copies and inter-dispatch barriers
  are outside these timestamp ranges; no frame-budget extrapolation.512 plain
  integer-period far controls converge within the unchanged2e-6 field tolerance.
  Arbitrary finite rotated far footprints have edge residuals and are not asserted
  to have exactly zero mean.
-Work adds16 coupled region/brick subcells to the existing nine noise evaluations,
  with no additional texture/hash samples. No additional frame geometry or cached
  coordinates beyond PR351. UV1 remains8bytes per covered vertex; bake preparation
  adds one temporary8N-byte owned copy. One extra material row per building material
  registration, no larger384-byte GPU row. Private compute buffers total33180160
  requested bytes and16590080 copied bytes for this batch; these are not driver
  peaks or a whole-world memory bound.
-Five focused programs pass103 checks: material packing, owned geometry cache,
  piece UV1/rejection/restoration, proxy layout, and actual tile material selection.
  All272 lit shader artifacts cross-compile to MSL2.3; that is not Metal execution.
-The existing `ComposedMaterialsReachTheCard` test aborts in system SDL3 during
  `SubjectDraw::ConfigureVariant`, before new masonry assertions. The unchanged
  c8507e906 predecessor/header/archive closure reproduces SIGABRT at the first
  textured control. Extended atlas coverage remains unproven on this host; retain
  the test for Metal/native-world counterchecking. No SDL backend patch or relaxed
  checks are included to conceal it. Its pure-pane control accounts for the actual
  binary16 roughness attachment and checks the interior of a3×3 pane neighbourhood.

Relief/bump, visible aging, full Place/device/frame/peak-memory counterchecks and
the final atlas result remain open under #306/#328 and the owning integration
issues. Master owns Metal/A18 and publication of integrated Place images.
