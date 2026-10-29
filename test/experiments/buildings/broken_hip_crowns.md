# Two-slope crowns on unchanged functional buildings

Owner: #1679 under #618. Explicit optional Python exterior programme, reusing the
existing #469 broken-hip construction and #1637 functional source-height crowns.
No measured historical roof, automatic worldwide morphology choice, new attic
rooms or native/asset-cache/Shader/Source admission is claimed.

## Product and construction

`with_broken_hip` creates a small immutable request before skeleton/CSG work. It
retains the original body object, source height, footprint/courtyards, whole
floor/stair/window/terrace products and existing highest-roof access. Defaults
remain unchanged. A far/repeated view keeps the complete compact crown without
expanding a hip or profile; every new vertex obeys the old vertical displacement
radius. This is not a temporal shadow/radiance/LOD qualification.

The existing hip is built in its unplaced core-local frame. Its upper facets are
cut with the existing `broken_hip.surface`; the existing full logical split/emit
is the equal-product comparison. For relative old height `t`, default run `r=0.15`
and height `h=0.65`, the continuous map is `h*t/r` below `r` and
`h+(t-r)*(1-h)/(1-r)` above it. The lower slope steepens and the upper flattens,
retaining the same declared eave/top. Parameters satisfy `0<r<h<1`; an additional
0.25/0.75 control is independently checked. This is not a weighted-skeleton solver.

Both cap sides use the declared eave/top planes within the **existing 1e-9 m**
source-height guard. The largest actual correction is 1.172041e-10 m; old assets
are never changed. Replacing only the upper skin originally left open edges
against the old cap's few-ULP eave deviations. A common XY-unit/Z-height scope
plus a complete cap on the declared planes fixes that contact without an XY
rounding grid, approximate positional weld or relaxed manifold/height gate.
Matching directed edges retain separate point-contact fans.

Source triangle IDs, UV interpolation and material ownership remain explicit;
new geometric normals come from the new facets. The original 128-boundary/
1024-node/4096-Python-event/one-second skeleton limits remain. Total profiled cap
capacity stays at65536 triangles, with `3*top+bottom` checked before cuts. The
existing cooperative deadline is checked around the added construction; this is
not a preemptive hard realtime limit. The original valid hip survives a profile
failure with its owner/reason. If the base hip itself fails, the old flat crown
survives. Previously cached compact draws are corrected to that effective roof.

## Independent geometry and reference correction

Checks retain the complete body/terrace/portal drawing, floor shell, stairs,
landings and frames exactly. Roof validation includes closed native/triangle
volume and area, independent original facet integrals, complete projection/
courtyards, source triangle barycentric inclusion, original UV/material mapping,
whole affine height mapping and rejection of any triangle spanning the kink.
Six full native sections compare against the old actual hip under the inverse
height map, including the exact kink and both sides. Their areas are also
checked against separate sums of the original clipped facets; no section
polygon union is treated as a height or Source authority.

The new height exposes a real old reference-union failure: valid clipped facets
sum to222.284330438722 m², the unchanged original native roof gives
222.284330433041 m², while raw GEOS union reports221.983692766808 m². No product
mesh causes that0.300638 m² loss. `hip_crown_checks.expected_section` now checks
validity and area preservation, then uses the existing Manifold CrossSection
union at1000 scale when GEOS cannot satisfy them. The original1e-7 m² area
requirement remains; failure of both paths returns no reference geometry. The
same correction covers a formerly invalid transformed ring. A native original
roof regression independently checks this actual source.

Twenty-eight focused checks pass, including local cut failure, early triangle
capacity, base skeleton failure, source-height rebinding rejection, deferred
views, custom profile and highest-roof access. All101 historical default fields
(flat/hip/contour crowns plus both complete last-source bodies) remain exact.

## Equal-product cost and actual images

Measured producer **45c2887528f3c48bb762acef5fc6dc4e940cef20**; later changes only
extend checks/documentation. Linux/i5-1135G7, Python3.13.5, NumPy2.5.4, Memray1.20;
BLAS/OMP1, timings normal priority, checks/render nice10. Seven alternating rounds
use the same prepared **unplaced** hip per source. An earlier world-placed-base
cost prototype is discarded; all87 exported geometry fields match it exactly,
but it is not used to rank the core-local procedures.

| Actual control | Two courtyards | Concave body | One courtyard |
|---|---:|---:|---:|
| Original / new closed triangles | 220 / 354 | 198 / 362 | 268 / 450 |
| Direct / logical median CPU ms | 51.092 / 111.948 | 61.355 / 133.640 | 77.863 / 164.046 |
| Direct / logical allocations | 209673 / 391567 | 210641 / 398673 | 265602 / 498394 |
| Direct / logical requested bytes | 19570853 / 41694618 | 19760558 / 42524880 | 24687797 / 52875194 |
| Direct / logical incremental heap peak B | 494142 / 525802 | 442938 / 504058 | 539302 / 622978 |
| Known copy lower bound B, either path | 73188 | 72900 | 92196 |

Both methods have exactly equal complete solid/skin array signatures on every
source, including all18 separately reloaded roof/root/source-binding fields.
The direct path visits220/198/268 source triangles across two slope zones and
avoids two reconstructed logical split results. It is2.1–2.2× faster here, with
lower allocation/peak costs. No cross-device speedup is inferred. Total internal
copies remain unmeasured; the lower bound counts explicit canonical vertex and
immutable-buffer copies only. Referenced NumPy bytes include the shared hip plan.

Profile times/traces exclude source/body/hip preparation, independent checks,
export and render. Single `single_full_crown_construction` observations include
`RoofedProducts` owner initialization and its existing terrace components as
well as the roof; they are not a pure hip/profile ranking or whole-world load
measurement. Full scene generation is not a frame hot path or native budget pass.

Eight matched actual1920×1080 Eevee16-sample images were opened, including a
higher two-courtyard view. Backend: Mesa Intel Iris Xe (TGL GT2), Blender5.2.2.
The steeper eave band and flatter upper roof are visible; no gaps or lost courts
are apparent. Bodies remain bare/repetitive, and this does not establish a full
city/material/night/World/device quality result. Camera, materials, glass,
daylight and terrain-free test setup match. For image export only, both requests
share the proven unchanged prepared functional-body owner outside measurements.
All render calls and the normal process return0. Images stay in system temp.

## Reproduce and hand off

Use the existing `.venv` and existing NumPy/Shapely/Manifold/CGA roof dependencies;
no package is added. Set `PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render`,
`OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1`; use fresh explicit temp outputs.

- `python -m unittest verify_broken_hip_crowns verify_hip_crowns verify_broken_hips`
- `python test/experiments/buildings/replay_broken_hip_crowns.py --output /tmp/outshine-broken-crowns`
- Render each `render-queue.json` entry with `python test/experiments/buildings/blender_city.py <geometry> --output <temp-images> --samples 16`.

Actual inputs, request parameters, source IDs/WKB hashes, producer hashes,
geometry NPZ, cost traces and whole-body render metadata are retained by replay.
Native consumers must retain the unplaced/placed frame boundary, effective
fallback identity, complete source/body products and observed cost scopes.
Native World/cache/radiance/LOD/device integration remains separate. #618/#33
and worldwide architecture diversity remain open; no whole988-source adoption.

SIGGRAPH first: *Procedural Modeling of Buildings* (2006), p.4/Fig.7 (gambrel and
mansard), and Held/Palfrader2016 weighted skeleton roofs p.6/Figs.3–4 were locally
rendered and optically checked. Existing SDK/cut evidence and the prior recipe:
`doc/references/buildings/esri/broken-hip-profiles-sdk-3.4.12206.md`. We reuse that
continuous height composition, not general weighted events or an Esri roof mesh.
