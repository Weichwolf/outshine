# Coherent facade replacement reference

Issue [#774](https://github.com/Weichwolf/outshine/issues/774), following
[whole-window fields](bay_city.md). This is a checked Python reference, handed
to `outshine_research` under [#786](https://github.com/Weichwolf/outshine/issues/786).
It is not the completed issue or a native/runtime admission.

## Products and preserved function

`facade_programme_plan.py` produces two camera-independent wall programmes:
`ribbon-screens` groups existing horizontal opening rows; `vertical-slots`
groups full-height opening columns with dark spandrels and projecting fins.
Both compose plinth, main wall and attic on actual coplanar domains, including
blank walls. Protected openings, portals and route reserves remain unchanged.
Wall admission is atomic within32768 closed triangles; rejected wall IDs and
reasons remain explicit. Original structural walls survive rejection.

`facade_programme_products.py` removes old decorative material9 only from the
owned facade-frame component, retaining original opening frames/panes and their
triangle positions, materials and part IDs. It appends the new closed layers
to physical/collision products as well as their visible outer/front products.
Deleting material9 globally would incorrectly remove other construction parts.
Floors, access core, terraces, roof, heights and source identity are preserved.
Empty plans retain the old source product. Component IDs are validated before
expansion; direct fronts avoid side construction.

This reference still **constructs and then removes old trim**. Early omission
and exact equivalence to this reference remain unimplemented. The new programmes
have not been applied to the complete988-source window. The previous complete
result in `bay_city.md` belongs to the older additive programmes; its code-bound
receipt must not be rewritten to approve the new collision geometry.

## Precision counterexample and checks

An initial full run ended with16 errors: valid local profiles could collapse
after world placement. Source5409740/16/398 had minimum clearance8.88e-16m;
48 local points became46 distinct world points with four null triangles.
The fix reduces only newly constructed decorative U/height profiles on a1e-6m
grid, restores counterclockwise orientation and measures Hausdorff adjustment.
Construction reserves receive an extra2e-6m opening guard. Original OSM,
structural coordinates, protected geometry and check tolerances are unchanged.
Invalid/collapsed profiles are rejected rather than admitted by a relaxed gate.

On modelling commit `d5e4b4182bf6b3fdee3e009e3a9f689d7710d7f9`,11 combined
`verify_facade_programmes`/`verify_bay_facades` methods pass in486.432s, exit0.
Seven real sources at original and roof-coupled heights, each with both
programmes, cover28 plans and3356 closed layers. Maximum measured recipe
adjustment is6.606231614050135e-7m. Checks include actual compound-body function,
positive closed volume, wall attachment, protected openings, continuous scoped
entry/terrace routes, exact retained source openings, front/outer/closed subsets,
selection before work, shifted openings and capacity failures. These are not
general controller, arbitrary circulation, Float32 or device proofs.
Four previous source/programme plans also replay exactly as JSON/WKB after the
default-preserving wall-domain extension. Ruff/format and diff checks pass.

## Actual images and remaining appearance deficit

The frozen producer exports two actual source bodies, each with legacy and two
replacement programmes, at identical Near/Whole/Opposite cameras. Twelve new
1920×1080 images and two elevation sheets were optically opened. Six actual
legacy captures are reused from914073b2d, with exact NPZ, camera, palette,
ground and source matching and original image/render provenance. This gives18
comparisons, not18 newly rendered images. Hashes, imported triangle counts and
camera/light/source-glass settings are checked in the local assessment.

Blender5.2.2 LTS/Eevee,16 samples, Xvfb/llvmpipe LLVM19.1.7, two software-render
workers with two driver threads each: new captures take30.318–38.416s, worker
RSS1957320–2144200KiB including the software driver. These are shared-host
diagnostics, not GPU/runtime performance. No images are published.

The replacement reads more coherently than overlaid short bands and piers;
horizontal screens and vertical fins now have distinct large-scale structure.
Fixed procedural window grids, pale glazing and large plain roofs still dominate
repetition. This does not attain CityEngine architectural quality or worldwide
recipe diversity. Existing window positions were held fixed for fair comparison;
they are procedural interpretations, not source-observed facade measurements.

## Scoped equal-output cost comparison

Cost producer `4d1625457b7ebd492a2753c2224d16206cd2d57a` reuses the existing
benchmark for16 timed warm calls and a separate traced call per case. All six
draw arrays match byte-for-byte for all/three/empty component selections.
Ownership count/range is checked separately.

| Source / programme | Plan median ms | Direct front ms | Outer/filter ms | Template triangles direct/filter |
|---|---:|---:|---:|---:|
|5335130/11/72 ribbon|164.417|131.325|159.893|1792 /4584|
|5335130/11/72 vertical|229.424|93.221|111.792|912 /2872|
|3953080/19/766 ribbon|302.086|195.613|239.600|2852 /7428|
|3953080/19/766 vertical|461.977|127.349|163.837|1638 /5258|

Direct construction avoids work and is faster here; its traced peak is slightly
higher in every case. There is no memory-saving claim. Acquisition, full-body
CSG and discarded old trim are excluded. Tracemalloc excludes native heap;
complete allocation/copy traffic is unmeasured. Descriptor JSON is diagnostic,
not a compact cache format. No cache, automatic LOD or device admission follows.

## Reproduction and retained evidence

Host VIRUS; Python environment `/home/cosmo/Git/outshine_buildings/.venv`:
Python3.13.5, NumPy2.5.4, Shapely2.2.0, Manifold3.5.4, bpy5.2.2.
Run from the matching frozen source with a new explicit output under `/tmp`:

```sh
export TMPDIR=/tmp OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
PY=/home/cosmo/Git/outshine_buildings/.venv/bin/python
nice -n 10 "$PY" -W error -m unittest verify_facade_programmes verify_bay_facades
nice -n 10 "$PY" test/experiments/buildings/facade_programme_probe.py --output /tmp/outshine-research-programme-models
"$PY" test/experiments/buildings/facade_programme_cost.py --output /tmp/outshine-research-programme-cost.json
LP_NUM_THREADS=2 LIBGL_ALWAYS_SOFTWARE=1 nice -n 10 xvfb-run -a "$PY" test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-research-programme-models/Programme0-cascade-ribbon-screensWhole-geometry.json --output /tmp/outshine-research-programme-images --samples 16
```

Current frozen modelling worktree is `/tmp/outshine-buildings-programmes-d5e4b4182`.
Models, plans, images, assessment, legacy provenance and control counts remain
under `/tmp/outshine-buildings-programme-models-d5e4b4182/`.
Cost JSON: `/tmp/outshine-buildings-programmes-cost.json`.
Passing log: `/tmp/outshine-buildings-programmes-controls.log`;
original red log: `/tmp/outshine-buildings-programmes-controls-unrounded.log`.
Legacy image originals remain in
`/tmp/outshine-buildings-facade-city-controls-914073b2d/`.
These inputs/evidence must remain available during handover; they are not runtime
dependencies or repository assets. Acquisition uses the retained provider cache,
not a different OSM API. No owned jobs remain running at handover.

SIGGRAPH2006 [Procedural Modeling of Buildings](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
PDF-page5/Figures9–11, was optically reopened for semantic alignment and shape
replacement. The numerical micrometre grid is a separate construction repair.
Pinned [SDK/Ghidra evidence](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md)
supports split/component/extrude primitives, not supplied Esri architecture
recipes. There was no new decompilation or executable Esri recipe oracle.
