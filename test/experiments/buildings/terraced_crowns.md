# Highest roof crowns coupled to storeys and exterior demand

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following the
[terraced city](terraced_city.md) and [ground-route correction](terraced_ground.md).
The highest surfaces previously ended as bare uniformly coloured caps. The new
Python experiment reserves a contained highest roof before body/storey/facade
construction and adds vertical parapets, copings, a darker procedural roof deck
and optional sealed technical volumes. It supplies component/shape procedures;
the whole CityEngine-level architecture/diversity and native budgets remain open.

## Camera-independent height and footprint plan

`terraced_crown_plan.plan` retains the delivered source upper height and existing
supplied/derived storey count. Available roof height is bounded by that count
and the existing2.64m minimum storey rise, quantized down to a millimetre. An edge
can be lower than1.10m when height is tight. Under0.06m budget the prior complete
bare-body programme remains explicit. Highest roof access is unserved throughout;
a low edge is never advertised as a safe walkable parapet.

The roof is planned in the actual core-local coordinate frame. The body gets an
explicit `roof_height_m` reserve, so its top is the deck datum; source top remains
in `RoofedPlan.source_top_m`. Window heads, ceilings, stairs, arrivals and terrace
portals use the resulting body/storey plan. No source height/property is rewritten
to hide an added structure. The `building_levels` alias now joins the existing
`building:levels`/`levels` handling. Supplied roof fields retain their previous
programme. Choice depends on geometry/attributes and existing stable source IDs,
never a place/camera name. A failed contained technical placement replans the body
at the actual parapet datum, at most twice, avoiding a floating roof.

Physical roof components use the existing closed `PrismVolume` constructor:
a0.08m slab at the existing opaque ceiling contact, a0.20m wall ring, a0.24m
coping ring with0.045m height, and optionally a technical block and0.08m cap.
Every roof component remains separately closed. Roof-component interiors are disjoint;
the slab deliberately overlaps the body ceiling. Body/roof volumes are not simply
added to infer a physical building mass.
actual section unions and summed volumes are checked independently. Original
highest footprint WKB remains available. The existing bounded contact resolution
from the terrace study handles floating-point whiskers:1e-10m grid and1e-6m
opening/closing, with unchanged1e-7m² source-area boundary. This is a declared
contact approximation, not exact microscopic topology reconstruction.

The render skin omits opaque body/slab contact caps and covered component faces.
It keeps parapet inside/outside faces and the visible coping overhang underside.
Those omissions require the matching body/roof assembly; the draw skin is not an
independent collider. The closed logical components remain available separately.

## Direct and selected components

The far roof is emitted directly from its2D material coverage at source top,
with complete exterior side walls. No closed roof prism or full body is built
for that request. Full/compact body and full/compact roof have independent demand:
the existing0.35m body-position bound and the reserved roof rise use the unchanged
at-most0.5px exterior-position threshold. Bounds include movement and source top;
inside/near-plane and roof-only visibility remain conservative.

`resolve(view)` returns reused mesh parts. A distant body can stay compact while
its visible roof requests full detail. The near cap is prepared once, independently
of the full body's CSG/windows/stairs. `draw(near)` produces cached whole-mesh
full/compact **offline export references**; these joins are not a per-frame path
or an asset-cache implementation. Source/body/roof IDs and plans stay stable
across selection and movement. Local roof material60003 avoids the existing
private accent IDs and source wall bindings. It is a procedural neutral grey
metallic-roughness definition; daylight, glazing and shader code remain unchanged.
General radiance/transmission/shadow/night/temporal LOD acceptance is still open.

## Actual sources, independent checks and images

Frozen code: `b1ffd315818a9c2205d8ca4a3fb2f6cec25adbf7`, Linux x86_64,
Python3.13.5. The original71 proposed programmes remain bound to all988 original
source memberships/properties/footprints and12 protected previous owners.
All71 new roof plans and their221 closed components pass:67 parapets/four with a
technical volume. Maximum component volume/area discrepancies are2.391e-12m³ /
5.713e-12m²; maximum actual section XOR is1.333e-9m² and highest-body footprint XOR
6.806e-9m². Existing1e-7 checking boundaries remain unchanged.

This cohort verifies **roofs**, not the changed bodies' complete access scope.
Its `full_body_checked` field stays false. Three independent actual controls do
also pass the complete previous body shell/volume/stair/arrival/terrace checker.
They retain five/eight/nine storeys and15/22/26m source tops. Source72 and121 have
technical roofs, source766 a lower parapet. Their near triangle counts change
41318→41454,74574→74738 and46018→46168. Their body plans differ intentionally
through the reserved heights; the six full-model pictures are whole source
comparisons, not isolated roof-cost or equal-geometry render comparisons.

All six actual paired1920×1080 images and the three-panel2D roof plan were opened.
The highest roof edges, court rims, deck surface and technical-volume shadows
are legible gains. Facades/windows remain pale and repetitive, the technical
blocks are simple sealed volumes, and richer roof/facade programmes are still
needed. The renderer is normal Eevee16 under Xvfb/llvmpipe LLVM19.1.7 with the same
camera,10000m flat ground, sunlight/sky and AgX settings per pair; no DEM.
No specialist images/gallery/server are published.

21 affected tests pass from source and again against the existing installed
reference target; both terminal0 and94 actual loaded module paths/bytes checked.
This is no new isolated dependency installation. A separate old/new replay of
all71 **default unreserved** mass plans compares every field exactly. The existing
body route and historical city imagery are therefore not relabelled as new
roofed-body admission. The tests also cover explicit supplied levels/roof fields,
unsafe reserves, missing contact caps, no-budget fallback, failed technical
placement, far/lookaway/inside views and roof-near/body-compact reuse.

## Work and storage context

Each of12 fresh workers measures three unprofiled constructions and a separate
Memray build. Source loading/imports, independent checking and rendering are
excluded; all2D planning, coarse products, requested closed products and retained
draw parts are included. All compared draw indices are32-bit. Referenced NumPy
storage is the union of root byte intervals, including shared/view backing data;
Python, WKB, GEOS/kernel/allocator and OS storage are outside that column.
Memray requests/traffic/heap and worker RSS are separate. Traffic is not an exact
copy count. Concurrent nice10 checks/software rendering were present; wall-time
medians are local diagnostic samples, not device/frame acceptance.

| Case | Request | Draw triangles | NumPy MiB | Median wall s | Requests M | Traffic MiB | Tracked peak MiB |
|---|---|---:|---:|---:|---:|---:|---:|
| 0 | before | 41318 | 17.712 | 1.1750 | 4.057 | 443.795 | 30.502 |
| 0 | full | 41454 | 17.770 | 1.2153 | 4.037 | 456.475 | 30.474 |
| 0 | mixed | 5847 | 3.115 | 0.5304 | 1.544 | 160.155 | 5.587 |
| 0 | compact | 5695 | 3.039 | 0.4332 | 1.428 | 151.271 | 5.587 |
| 1 | before | 74574 | 32.038 | 2.3910 | 6.994 | 793.219 | 54.743 |
| 1 | full | 74738 | 32.110 | 1.9658 | 7.180 | 826.447 | 54.713 |
| 1 | mixed | 10516 | 5.624 | 0.5869 | 2.817 | 292.687 | 10.241 |
| 1 | compact | 10348 | 5.536 | 0.6873 | 2.697 | 283.790 | 10.242 |
| 2 | before | 46018 | 19.900 | 1.4002 | 6.505 | 656.702 | 34.080 |
| 2 | full | 46168 | 19.946 | 1.5997 | 6.772 | 688.469 | 33.990 |
| 2 | mixed | 7044 | 3.772 | 0.6657 | 3.630 | 325.327 | 6.803 |
| 2 | compact | 6834 | 3.665 | 0.5693 | 3.478 | 314.011 | 6.805 |

`before` is the old complete bare-roof near programme. `full` requests the new
body and roof near components. They differ in architecture/height distribution,
so their timings do not establish an equal-quality optimization. `mixed` shares
the new logical plan and position criterion, requests a near roof and compact
body, and does not construct the full body. `compact` constructs neither closed
roof nor full body. Whole near owners remain costly; component demand is useful
but does not admit these prototypes to a native world budget.

## Primary references and rejected alternative

The local [CGA Shape / SIGGRAPH2006 paper](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf)
p.4/Figs5–8 was optically inspected again: mass composition, roof vocabulary,
component scopes and context are relevant. Existing pinned SDK/Ghidra offset
and contained-rectangle observations are in the [primitive reference](../../../doc/references/buildings/esri/roof-crowns-sdk-3.4.12206.md).
They establish primitive background, not a shipped Esri building recipe or full
SDK equivalence. The existing bounded service-rectangle search is reused and
makes no largest-inscribed-rectangle claim.

An unconstrained triangular band-heightfield was tested against an intended
constant-offset bevel on the new source121 upper contour. A contour edge vanishes
inside the ribbon; the resulting volume/area differ by0.000569883m³ /0.0467464m²
from that target. It is retained as a rejected alternative. The delivered vertical
wall/coping recipe has explicit prism semantics and its own independent checks;
the previous general crown-heightfield primitive remains unchanged.

## Reproduction and original evidence

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
.venv/bin/python -m unittest -v verify_terraced_crowns verify_terraced_mass verify_terraced_mass_rooms
.venv/bin/python test/experiments/buildings/terraced_crown_probe.py --output /tmp/outshine-buildings-crowns/models
.venv/bin/python test/experiments/buildings/terraced_crown_census.py --census /tmp/outshine-buildings-terrace-census-f747bb5c0.json --physical-roofs --output /tmp/outshine-buildings-crowns/cohort.json
.venv/bin/python test/experiments/buildings/terraced_crown_cost.py --source 0 --mode mixed --trace --output /tmp/outshine-buildings-crowns/cost/0-mixed.json
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a .venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-crowns/models/Crown-0-After-geometry.json --output /tmp/outshine-buildings-crowns/images --samples 16
```

The full source census can be reproduced with the previous source-demand study;
it remains an input to this new independent roof cohort. The new probe has no
private source-input dependency. Existing requirements cover NumPy/Shapely,
manifold3d, matplotlib, bpy and Memray; no new dependency was installed.

Original private evidence, all under `/tmp`:

- `outshine-buildings-crown-models-b1ffd3158/`: nine exact full/compact NPZ+JSON
  models, six actual images/render reports, paired RGB comparisons and2D plans.
- `outshine-buildings-crown-cohort-b1ffd3158/roofs.json`: all71 plans/roof checks,
  original input binding and explicit absence of full changed-body checks.
- `outshine-buildings-crown-tests-b1ffd3158-{source,installed}.json` with
  `outshine-buildings-crown-{source,installed}-b1ffd3158.log`:21+21 checks/94 imports.
- `outshine-buildings-crown-cost-b1ffd3158/`:12 worker records,36 unprofiled samples,
  12 separate raw Memray traces and JSON/log statistics; terminal0 batch log.
- `outshine-buildings-crown-default-replay-b1ffd3158.json`: all71 unchanged default
  mass plans and prior source receipt.
- `outshine-buildings-crown-bevel-counterexample-8e514ef86/`: original rejected
  cap mesh/roots, source and recipe/provenance, with numeric target discrepancy.

Whole-city use needs the changed bodies' complete source-level functional audit
and integration with the existing owner protection/material bindings. Highest
roof entry/roaming, service interiors, native cache/controller/device and general
lighting/LOD admission remain open. No shared library/native contract changed.
