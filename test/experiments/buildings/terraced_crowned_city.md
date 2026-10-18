# Coupled highest roofs in the complete source window

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following
[highest roof components](terraced_crowns.md). Roof-only checks do not prove the
changed building body's function: reserving the roof changes storey rise,
stairs, ceilings, windows and terrace portals. `terraced_crown_census.py` now
accepts `--full-bodies` to run the existing independent body checks on each
coupled plan, with a distinct outcome from the closed roof checks.

## Physical scope and composition

The original988 source identities, properties, footprints and12 protected
functional owners remain bound by the existing complete census loader. The
proposed71 programmes and their original storey counts remain fixed. Every
record explicitly distinguishes planning, roof checking and full scoped body
checking; failed geometry keeps the earlier source route with its reason.
An unfinished report cannot supply a complete city composition.

The body checker retains its existing scope and reconstructs closed shell/stair/landing solids,
independently sweeps shell sections/volume, checks closed facade/terrace parts,
samples stairs against relevant facade/terrace collisions, and checks ground
arrival/continuous approach and declared terrace routes. It retains the existing
tolerances. This is the previous bounded physical scope: no continuous stair
controller, arbitrary free roaming or highest-roof access is established.
Complete composed draw arrays must also remain finite, with unit normals and
the original source height interval. Closed roof components retain the independent volume/area/section checks from
the preceding study; the shared ceiling overlap is deliberate.

`terraced_city.py --crowns` requires both explicit audit modes.
`CrownedDetails` rejects references without successful body and roof checks,
keeps prior unselected parts intact, and creates one source-height-coupled plan
per selected source. It consumes separate body/roof demand before expensive
expansion. A mixed selection can build only the roof over a compact body.
Movement reuses that source plan and existing products. The joined whole-window
meshes are offline exports, not per-frame joins or a product cache policy.

The original complete source window,1920×1080 view and10000m flat ground remain
unchanged. Previous source parts, material bindings and functional owners remain
protected; every previous draw fingerprint is rechecked after composition.
The before scene is the earlier source pipeline, not the immediately preceding
bare-terrace city. These images therefore compare complete source treatments,
not isolated roof changes. The previous [three actual roof pairs](terraced_crowns.md)
remain the closer roof/body-height comparison.

Roof role60003 uses the same procedural neutral-grey metallic-roughness values
as those earlier pairs. Source wall bindings and private accent roles60000–60002
remain separate; normals, UVs and shader/daylight configuration are unchanged.
Draw indices are range-checked before32-bit conversion. The shared cut-normal and draw-surface correction is described below. Provider,
material, cache and native interfaces are unchanged.

## Exact null facets and directed sections

The first full coupled run finds70 successful bodies and one real failure:
source10851960/24/0, a26m/9-storey paired-wing programme. Its valid Boolean result
has17434 shell facets, including two exactly collinear topology strips. Both
local and placed cross products are exactly zero. The source data, heights and
body plan remain unchanged; this is not a small nonzero-area face threshold.
The original full-cohort process wrote all71 outcomes but ended143; that run is
counterexample evidence, not a passing terminal gate.

Two bounded backend simplification probes leave these strips. The implemented
handling retains the complete positions, topology roots, materials, UVs and
source/triangle provenance. Only certified null facets receive a finite oriented normal
from the original parent triangle, reversed for a subtractive cutter. A missing
or unresolved parent normal still rejects; nonfinite or underflowing nonzero
areas cannot be treated as null. Normal nondegenerate outputs retain their
existing computation, with no geometry movement or tolerance increase.

A floating-point cross product of zero alone is insufficient: Transport supplied
the counterexample a=2^-27 and points (0,0), (1+a,1), (1+2a,1+a), whose exact
represented-binary determinant is2^-54. `triangle_null.exact_null_candidates`
therefore verifies numerical null candidates using exact dyadic `Fraction`
coordinates, exact differences and all three2×2 minors. A positive exact area
with an unresolved floating normal rejects explicitly. Fraction work is limited
to those numerical candidates; existing resolved normals retain their path.
The deterministic cancellation counterexample runs in all three coordinate
planes for both cut normals and draw construction. Float32 conversion is not
used to decide nullity of the original geometry.

The draw helper omits only certified area-free facets, preserving the closed logical
body separately. Normal inputs retain their buffers until the existing draw
expansion; filtering/copying happens only when null facets exist. Terrace route
normalisation handles null boundary faces without NaNs and keeps the full native
solid in its actual collision/support queries.

The independent section checker now orients crossings by triangle winding and
descending/ascending Z edges, including a vertex exactly on the section plane.
For area-bearing triangles this has the previous normal/tangent orientation.
It also connects zero-length topological section segments whose geometric normal
has no direction. Four extra cuts through the null facets, including their tiny
internal height intervals, recover valid contours within the unchanged1e-7m²
logical-section boundary. Topology, geometry and draw roles remain explicit.

The complete actual provider row is fixed in `data/terraced-crown-contact.json`.
`verify_terraced_crown_contact` runs the full previous body scope, roof checks,
finite complete draw and the independent extra contour cuts. Unit controls also
check original versus inverted cutter normals, underflow/nonfinite rejection,
exact ordinary draw retention and empty all-null draw behaviour. Transport's
existing ramp consumer controls run with the change; shared primitive
coordination and the corrected exact predicate are [explicitly confirmed](https://github.com/Weichwolf/outshine/issues/47#issuecomment-6244116159);
[master confirms the same attribute-only scope](https://github.com/Weichwolf/outshine/issues/47#issuecomment-6244111793).
Finite normals do not admit collision geometry; each consumer retains its own
whole-solid/Double/Float32 requirements.

The local SIGGRAPH2022 [Interactive and Robust Mesh Booleans](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf),
PDF-page2/Figures2–3, was also optically reviewed for finite-precision/exact-
predicate context. The Boolean backend remains Manifold; this predicate is
not an implementation of that complete Boolean solver.

## Verification and limits

Frozen corrected producer: `eb0854d8173f04a51f1ebb31f6ee69de57945ee4`, Linux x86_64,
Python3.13.5. Seventy-five Source controls pass, checking105 actual matching
reference imports. A fresh local wheel installation matches all212 reference
Python files. Its complete installed controls also pass75/75 with105 actual matching imports.
The fresh complete source-bound cohort passes71/71 bodies,221 closed roof
components and71 finite complete draws. Exactly two null topology facets remain
in the closed source10851960/24/0 shell. Maximum independent shell-section XOR is
4.041e-9m² and shell-volume discrepancy1.347e-8m³, below the unchanged1e-7 gates.
The diagnostic loop takes789.105s for planning, independent checks, composed
full draws and intermediate reports; source loading/initial binding precede
that timer. Whole-worker peak423696KiB RSS also includes source preparation.
These are diagnostic costs, not a construction/runtime budget. Other nice10
controls ran concurrently.

After merging the independently verified optional #706 clearance update, model
production freezes `b6b4864fb2e301079519ea61f30915ccbb0e5633`. All Buildings
modelling files remain exactly as the full physical cohort. The only changed
reference-library file is `geometry/clearance.py`, outside that modelling scope;
Buildings has no direct call to that optional API. A fresh matching212-file
installation and focused clearance/cut/body/city/ramp transition checks cover
the shared import/consumer boundary:38 Source and38 installed checks pass, each
with101 actual matching reference imports. The complete
composition and actual static images are checked below.

New actual-source controls reject roof-only/failed-body/missing-draw references,
retain exact unselected parts, keep far requests free of full roof/body expansion,
and exercise a mixed city view plus reuse after motion. Previous city/body/roof,
rooflight surface/lamina, shared prism and transport ramp controls run alongside them.

## Whole-window result, actual images and work

All three scenes contain all988 original sources without omissions:987 ordinary
buildings and the retained mast. The71 new sources use40 cascade/31 paired-wing
programmes,67 parapets/four technical roofs;12 previous functional owners remain
protected. All988 previous draw-part fingerprints remain unchanged. Normal
selection expands60 bodies and71 roofs:11 bodies remain compact while their
roofs alone expand. The full-near reference uses the same71 source plans and
expands those remaining bodies.917 other source draws are reused in both passes.

| Scene | New body / roof near | Draw triangles | Render-array MiB |
|---|---:|---:|---:|
|Previous pipeline|0 /0|550101|142.572|
|Normal coupled|60 /71|2656495|701.357|
|Full-near reference|71 /71|2970798|784.684|

Normal demand emits314303 fewer triangles than full near:10.58% of the whole
scene and12.63% among the new source draws. It remains4.83 times the baseline's
triangle count. These are geometry-work/payload measurements under the stated
position criterion, not a same-quality runtime or device-budget acceptance.
Whole producer runtime748.195s after input/binding/view setup includes the old pipeline,
both new passes, hashing and exports. Before draw assembly282.266s; new normal
assembly68.069s; incremental full near10.416s. These are sequential different-state
phases with reuse, not equivalent cold benchmarks. Whole-worker peak3969112KiB
RSS includes old owners/parts, new blueprints and physical/draw products, combined
arrays, metadata and native allocator state. Full allocation/copy traffic and a
runtime ownership-residency bound are not measured by this composition study.

The reconstructed before NPZ is byte-identical to the previous source pipeline:
`d48fe4dccaa47639fc2a43f06b99cf1f9040e47e61445696254c68f30fba6b37`.
Every actual array, camera, palette/binding and ground matches; eight direct
renderer-chain files and the original geometry/report/image hashes also match.
The actual `4d54994c2` before image is reused with its original render provenance.
It was reopened here; this is not a new before render.

Normal/full-near are freshly rendered and opened at1920×1080, Blender5.2.2 LTS,
Eevee16, Xvfb/llvmpipe LLVM19.1.7, unchanged sunlight/sky/AgX and flat ground.
The two renders run sequentially in one renderer process.
Every declared triangle is imported. Render times63.358/83.338s and process peaks
4660020/7193408KiB are cumulative process peaks of this software offline renderer,
not separate cold/device measurements or native frames.
The renderer reports a velocity-geometry-buffer allocation fallback to object/
camera velocities. These are static views; no geometry-motion or temporal
acceptance follows. It is not a measured native resource limit.

Both current complete images and the matched before image were optically opened.
The steps, wings, courtyard rims and roof edges are legible; several broad bare
caps gain a contained termination. Facades/glazing and the overall palette remain
pale and repetitive. This is bounded architectural/function progress, with a
large remaining quality/diversity gap to CityEngine-level output.
`test/scripts/pixels.py` records92152 changed before/normal pixels (4.4441%),
mean RGB-channel difference1.141993/255. Normal/full differ at2319 pixels
(0.1118%),1505 by more than1/255, mean0.002885/255 and maximum23/255, restricted
to the upper90 image rows. No black/near-black pixels appear. These single-view
colour differences are observations, not universal LOD or image-quality scores.

The standalone public `terraced_crown_contact_probe.py` runs terminal0 at
`bfd96a4295e3353cea5abcbbdbb5528ee70f2344`. Its source-colour-preserving26m/9-storey
before/after draws contain62304/62490 triangles. Every actual array, camera,
material and ground exactly replays the already opened `be2f9133e` images; the
image receipt preserves that original render provenance. The final exact guard
also replays those same arrays; no unnecessary rerender is claimed.

A separate close pane-omission diagnostic confirms real openings, floors and
stairs behind the pale normal panes. Explicit screen-refraction at roughness0.12
and0.02 reveals depth with blur; an explicit straight-through Fresnel blend shows
depth with visible sampling grain. All four close controls were opened, with
omission/approximation recorded. They change no accepted shader/daylight profile
and do not prove native glass correctness. Source/body/facade diversity and a
coherent glass/depth treatment remain the next visible work.

General radiance/transmission/shadow/night/temporal LOD, persisted asset loading,
native integration and device budgets remain separate. The unchanged0.5px
exterior-position criterion alone does not prove those properties. Whole near
body construction remains expensive; the preceding roof-only demand comparison
does not make those whole-city costs acceptable. Facade diversity and glass/
interior legibility remain explicit quality gaps.

The modelling method reuses the optically reviewed SIGGRAPH2006 scope/component
paper and pinned SDK/Ghidra primitive references in the preceding report; this
composition adds no new Esri operator or claim of a ready-made Esri recipe.

## Reproduction and retained local evidence

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
.venv/bin/python test/experiments/buildings/terraced_crown_census.py --census /tmp/outshine-buildings-terrace-census-f747bb5c0.json --physical-roofs --full-bodies --output /tmp/outshine-buildings-crowned-city-audit/cohort.json
.venv/bin/python test/experiments/buildings/terraced_city.py --audit /tmp/outshine-buildings-crowned-city-audit/cohort.json --crowns --full-near-reference --output /tmp/outshine-buildings-crowned-city-models
.venv/bin/python test/experiments/buildings/terraced_crown_contact_probe.py --output /tmp/outshine-buildings-crowned-city-contact-models
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a .venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-crowned-city-models/CrownedAfter-geometry.json --output /tmp/outshine-buildings-crowned-city-models/images --samples 16
```

The complete source census can be rebuilt using the preceding source-demand
study; its local path is an input to this experiment, not a product dependency.
Private results stay in `/tmp`: the original failed and corrected full audits, raw null-facet export, actual
counterexample, model exports, actual images,
test logs/import receipts and a separate explicit pane-omission diagnostic.
No specialist gallery, server or published image accompanies this change.

Retained current roots:

- `/tmp/outshine-buildings-crowned-city-audit-eb0854d81/`: complete71 body/roof/draw
  report, bound original988-source census and independent metrics.
- `/tmp/outshine-buildings-crowned-city-models-b6b4864fb/`: all three current whole
  scenes, actual normal/full images, original-before reuse, payload and pixel logs.
- `/tmp/outshine-buildings-crowned-city-contact-models-bfd96a429/`: official
  counterexample exporter outputs and exact original-image reuse receipt.
- `/tmp/outshine-buildings-crowned-city-contact-models-be2f9133e/`: original paired
  counterexample models/images retained for that explicit image provenance.
- `/tmp/outshine-buildings-crowned-city-body-counterexample-05c911a3d/`: original
  actual input, failed raw cut and the bounded rejected simplification probes.
- `/tmp/outshine-buildings-crowned-city-glass/`: explicit geometry/shader diagnostics,
  actual images and assessment; no replacement product or normal-profile claim.

Source/installed import logs, matching package receipts and terminal statuses
carry their frozen commit suffixes. Original provider/asset inputs and required
references stay protected. Superseded duplicate helper outputs are cleaned up.
