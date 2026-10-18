# Exterior window shades on the existing plan (#734)

The coupled bodies still have flat, repetitive opening frames. Two explicit
component programmes now add real depth and shadows: 0.40m horizontal eyebrows
and 0.60m hoods with short side returns. These are additive exterior components,
not two complete architecture families or a solar-performance simulation.
Existing body, roof, openings, source wall materials and simple rooms remain.

## Plan, contact and early work selection

`window_shade_plan.plan_shades` consumes the existing prepared core-local
facade, before floor/Boolean/detail expansion. Frozen descriptors preserve
window/storey identities and dimensions; each component ID includes its
programme. Placement transforms once into the source frame. No camera input,
random state, new room layout or independent opening reconstruction.

The cap is 0.08m thick, 0.18m above the existing window head. Width adds 0.15m
at each side; returns are 0.07m thick. Their bottom is at least 2.10m above the
actual storey floor, including its slab. Caps stay at least 0.06m below the
storey top. The original 0.10m cap gap left insufficient return height on the
actual roof-coupled 2.6402m storeys; moving the component preserves headroom
and source height. This counterexample changed placement, not the acceptance
boundary. Short returns must still have at least 0.12m clear height.

The full projection may enter its host wall only through the declared 0.015m
back attachment. All vertically overlapping source storeys are checked;
opposing walls and insufficient attachment reject with reasons. A spatial
index also rejects both overlapping component bounds at a concave corner,
independent of input order. This is conservative box rejection, not exact
contact/load solving. Existing same-owner facade trim and wall attachment may
overlap intentionally; neighbouring buildings/transport reservations are not
supplied or admitted by this experiment.

Positive integer budgets reject before planning. `window_shade_mesh.expand`
validates explicit requested IDs and selects slots before creating templates.
An empty selection builds no template or mesh. A per-call dimension cache
reuses templates without a global unbounded cache. One concave profile
extrusion gives a closed hood; the comparator uses three closed bars with the
same geometric union. Plain eyebrows use one box in either method.

This adds no selection policy to `FacadeProducts`: its existing 0.35m exterior
radius does **not** bound the new 0.40/0.60m projections. The complete diagnostic
draws are deliberately selected explicitly. Runtime visibility, shadows,
reflections, radiance, cache residency and native LOD require their own bounds
and integration; primary-camera visibility alone cannot remove a shade.

## Independent controls and actual images

Four existing courtyard/concave provider footprints and three existing compact
admission controls provide seven source identities. Original and roof-coupled
heights give 22 prepared masses and 44 programme plans: 12664 accepted component
instances, zero omissions in this cohort. Instance counts are not recipe counts.
Fourteen focused test methods pass, including the existing seven facade-product
controls. The new controls independently check actual placed vertices,
attachment, full headroom, unchanged source preparation, stable/reordered IDs,
early selection and exact reuse, closed positive solids and analytic volume.
For three components per plan, independent compiled 3D bar union matches the
profile solid in both subtraction directions within the unchanged 1e-8m³ gate.
Low/high/overwide/opposing-wall and concave-pair negatives must reject; an empty
pair set is valid. Float64 geometry here is not a Float32/controller proof.

The model/renderer freezes `3f17d020e`. Two existing coupled controls use
`stepped-profiles.json` indices 0/cascade and 3/paired-wings. Each has Before,
eyebrow and returned-hood variants at the same Near/Whole/Opposite cameras:
**18 actual 1920×1080 images, 16 samples, all optically opened**. Both 2D/profile
and 2.5D component sheets were also opened. The worker ends 0 for every image;
image/report/mesh hashes, dimensions, full triangle import, actual camera
matrices, daylight and source-glass settings match. No known shadow/allocation/
shader error appears. Source glass remains visibly pale; this does not repeat
or replace the optional [thin-glass/night controls](open_floor_night.md).

| Source control | Original triangles | Eyebrows | Hoods | Slots per variant |
|---|---:|---:|---:|---:|
| 5335130/11/72, cascade |41454|+3000|+7000|250|
| 3953080/19/766, paired wings |74738|+5568|+12992|464|

All six original draw arrays remain an exact prefix. The first baseline also
exactly replays the previous glass/night model. The later concave-pair guard
(`eaddd5bf3`) replays all four complete plans and all complete draw arrays
exactly; images retain their original render provenance. These are building
controls, not a new complete 988-source city composition or full body re-audit.

The captures show clear projecting caps, short returns and changed wall/window
shadows from both sides. Long repeated window fronts remain monotonous and the
overall material treatment remains plain. This improves a component vocabulary;
CityEngine-level architecture diversity, richer ground floors, balconies and
roof/facade composition remain open at #618/#32/#33.

## Scoped costs

Linux x86_64, Python3.13.5, NumPy2.5.4, four-CPU/16GiB-class shared host. Actual
Eevee/bpy5.2.2LTS runs on llvmpipe LLVM19.1.7 under Xvfb, not RTX/Cycles. Across
18 images: render21.434–36.797s, worker peak1950692–2074632KiB. Tests and the
normal-priority cost worker overlapped nice10 rendering; these are reference
capture costs, not isolated rankings or native frame/device measurements.

`window_shade_cost.py` on `eaddd5bf3` measures all 16 original-height source/
kind/programme combinations, with16 timed calls plus separate traced calls.
The first cascade has252 openings at original height, rather than250 after
roof coupling. Its source/mass preparation is timed separately. Returned-hood
results at identical geometric coverage:

| Expansion | Median ms | Triangles | Retained arrays B | Traced peak B |
|---|---:|---:|---:|---:|
| All252, profile, templates reused |8.611|7056|566496|1281573|
| All252, bars, templates reused |9.433|9072|707616|1562421|
| All252, profile, no template reuse |119.246|7056|566496|1324741|

The profile uses28 instead of36 triangles per hood, with equivalent volume.
Four exact dimension templates replace252 template constructions in this case;
binary height differences are not rounded into one approximate template.
Planning including attachment/corner tests is105.600ms median/p95109.447ms;
serialized descriptors115236B. This planning cost is not a frame budget.
Empty selection creates zero payload/templates; selecting three creates only
84 triangles. Source setup, planning and expansion are distinct measurements.
Retained arrays and tracemalloc peaks are not total allocation churn, GEOS-native
peaks, copied bytes or GPU memory. Worker lifetime peak63612KiB includes imports
and previous cases; no total-cache/async/world/device acceptance follows.

## Sources, reproduction and custody

[CGA Shape, SIGGRAPH2006](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
PDF-page5/Figures9–11, optically reviewed again: aligned component scopes and
bounded occlusion. The pinned [SDK/Ghidra extrude/scope findings](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md)
support primitive correspondence, not an executable ready-made Esri shade recipe.
No new foreign source/model or SDK binary is bundled.

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
.venv/bin/python -W error -m unittest verify_window_shades verify_terraced_facade_products
.venv/bin/python test/experiments/buildings/window_shade_cost.py --output /tmp/outshine-buildings-shade-cost.json
nice -n 10 .venv/bin/python test/experiments/buildings/window_shade_probe.py --output /tmp/outshine-buildings-shades
nice -n 10 xvfb-run -a .venv/bin/python test/experiments/buildings/blender_city.py /tmp/outshine-buildings-shades/Shade0-cascade-returned-hoodWhole-geometry.json --output /tmp/outshine-buildings-shades/images --samples 16
```

Use a clean frozen producer; repeat the last command for the other generated
Near/Whole/Opposite reports. Private originals, six model exports, both sheets,
18 images/logs, source-prefix/corner replay and assessment/cohort remain under
`/tmp/outshine-buildings-shade-models-3f17d020e/`. Final scoped costs are
`/tmp/outshine-buildings-shade-cost-corner-final.json`; the earlier no-pair-guard
costs remain a separate control. Original-height and compact inputs already
reside in the versioned `stepped-profiles.json` and
`terraced-admission-controls.json`, with their existing source receipts.
No `lib/` or native code changes, installed-library claim, new default or public
gallery image. Reusable promotion follows wider composition/selection work.
