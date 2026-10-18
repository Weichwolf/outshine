# Ribbon fields and paired bays on stepped walls (#741)

The existing coupled bodies still repeat individual windows. This first
delivery adds two coherent facade treatments without moving their openings:
dark horizontal ribbon fields, and dark fields with projecting frames around
pairs of columns/storeys. The latter groups two to four existing windows,
depending on setbacks and available openings. These are facade programmes,
not complete architecture families. The whole988-source application remains
the next step in #741; this report qualifies the component/source controls.

## Actual wall plan before detail

`bay_wall_domains` recovers the actual core-local storey edges used by existing
windows and terrace portals. It validates their positions, tangents, span and
entry attachment. Coplanar edges with the same outward direction form a wall
domain; plane tolerance1e-8m, no source geometry snapping. Each edge retains
0.26m corner margins. Domain height follows the real storeys, including steps.

Existing window stations select paired columns; integer storey pairs select
vertical groups. Ribbons use each actual row. Candidate rectangles are clipped
to the complete wall domain, then cut by protected openings/routes. A lone
window stays on its original source route with a recorded reason. Layer/whole
closed-reference triangle capacities reject before extrusion. No random or
camera-dependent layout, second window grid or invented room programme.

Fields project0.03m and rims0.20m, with0.005m of declared host-wall attachment.
The two explicit private metallic-roughness bindings leave original source
wall colours, glass, existing bands/piers, frames and roof bindings intact.
Independent layer geometry may overlap original trim intentionally; these are
attached components, not a remeshed structural union or load calculation.
Other buildings/transport reservations are outside this input/control scope.

Every existing opening gets0.12m protection, including frame/sill room and
ground/terrace door height. Existing entry/approach/core/deck routes also
contribute conservative2D reserves:0.10m foot radius to0.40m height,0.30m torso
radius to1.90m. Circumscribed32-segment-quadrant tubes and a0.02m projection pad
reserve their overlap with the actual layer depth. This prevents new low
ornaments from narrowing a previously valid route. It is not arbitrary roaming.

Exterior trace bounds are checked against vertically overlapping own storeys;
wall/corner overlap rejects conservatively with reasons. Both layers of a bay
remain attached to its shared cell identity. Original body/room/roof products
and their source IDs remain; this does not renew their entire physical audit.

`bay_facade_mesh.expand` validates explicit cell IDs before template work and
reuses the existing profile extrusion and world placement. Front, outer and
closed products use the same region. Empty selection creates no template.
The0.20m projection fits within the earlier0.35m exterior displacement radius;
that position bound alone admits no radiance/shadow/reflection/LOD policy.
No native/cache/controller/default renderer integration is delivered here.

## Controls and actual captures

Four versioned courtyard/concave inputs plus three compact admission controls
provide seven real source identities. Original and roof-coupled heights give
14 masses/28 programme plans. Six comprehensive test methods pass, including:

- Every layer as a positive closed compiled solid, volume against area×depth
  within1e-7m³, and actual source opening area plus compiled3D aperture rays.
- Actual back-cap samples inside the source footprint and within0.01m of its
  boundary; attachment, height and original source footprint remain bound.
- Independent closed-mesh sections against continuous existing actor tubes;
  an added blocking solid must fail. Original support is preserved, not retested.
- Full front/outer positions, normals and UVs match their closed-reference
  subsets; direct fronts never call the side builder. Unselected cells build
  nothing; foreign IDs reject before expansion.
- Shifted window/portal/wall/entry and invalid/capacity-zero counterexamples;
  source windows/portals in reversed input order reproduce identical plans.

The first permutation test exposed input-order-sensitive polygon encoding.
Canonical source-ID order before Boolean operations corrects it without
changing the source objects. The passing complete run ends0 in137.538s under
concurrent nice10 rendering/Transport work; no timing/performance inference.

Producer/renderer: `837b8547c`. Both actual1920×1080 elevation sheets were
optically opened before rendering. Two source controls each have Before,
ribbon and framed versions at identical Near/Whole/Opposite poses: **18 actual
1920×1080,16-sample images, all optically opened**. The near view shows a whole
bay rather than the earlier one-window crop. Every worker ends0; complete
report/image/mesh/plan hashes, imported triangle counts, original draw prefixes,
camera matrices, source glass and daylight bindings are checked. Known shadow,
allocation and shader-error strings are absent. No process-success-only claim.

| Source / body | Original triangles | Ribbon outer / cells | Framed outer / cells |
|---|---:|---:|---:|
|5335130/11/72, cascade|41454|+3524 /35|+6038 /81|
|3953080/19/766, paired wings|74738|+6528 /52|+11140 /135|

All six original numeric draw arrays remain byte-exact prefixes. Window/door
holes stay visible; broad dark fields change the facade rhythm and projecting
frames add larger shadow/volume cues. Earlier bands/piers remain visible through
this added vocabulary. Pale glass, repetitive long fronts, large plain roof
areas and the absence of complete architecture families remain quality gaps.
There is no new city picture or CityEngine-level variety claim.

Blender5.2.2LTS/Eevee runs on llvmpipe LLVM19.1.7 under Xvfb,
`LP_NUM_THREADS=2`, Linux four-CPU/16GiB-class shared host. Python3.13.5,
NumPy2.5.4. Render41.080–63.023s; whole-worker peak1950756–2120332KiB, including
imports/software-driver resources. Concurrent agent work varied. This is not
RTX/Cycles, an isolated repeated ranking, GPU memory or native frame time.

## Equal-output costs and remaining work

`bay_facade_cost.py`, frozen `5637f1802`, measures the same two roof-coupled
controls/programmes:16 timed calls plus separate traced calls. The first
source/mass/terrace setup is separate; body CSG/rendering are excluded. Direct
front versus construct-outer/filter gives byte-exact complete vertex/face/
normal/UV/material/facade-extent output for all, three and empty cell selections.

| Source / programme | Direct front ms | Filter outer ms | Template triangles direct / filter |
|---|---:|---:|---:|
|5335130, ribbon|101.830|123.662|1358 /3334|
|5335130, framed|220.921|261.405|1798 /4822|
|3953080, ribbon|197.000|234.443|2610 /6386|
|3953080, framed|427.100|548.391|3446 /9146|

Template triangle counts precede instance placement; output triangle counts
include repeated cells. Complete front payloads plus cell ownership range
402696–1166352B. Direct traced peaks1194423–3602367B; filter1185991–3545551B:
direct fronts save work/time here, **not** traced peak memory. Whole-worker
peak117888KiB includes imports and earlier cases. Native GEOS peaks, allocation
churn, total copied bytes, cache loading and device times remain unknown.

Planning medians119.405–385.151ms remain expensive. Diagnostic serialized plans
are253295–3469674B because protected regions repeat per layer; this is not a
compact runtime-cache format. Full outer expansion costs122.979–534.518ms;
unchanged plans/products must be reused and detail chosen before expansion.
These are reference construction costs, not accepted runtime budgets.

The remaining #741 work applies this vocabulary to the entire unchanged
988-source comparison, preserves the existing functional owners and reports
every applied/rejected source. Broad architecture/roof/material diversity,
efficient compact assets and native/LOD/light/temporal acceptance remain
#618/#32/#33. Existing body/roof gates and original inputs remain frozen.

## Sources, reproduction and retained originals

[Instant Architecture, SIGGRAPH2003](../../../doc/references/buildings/siggraph/2003-instant-architecture.pdf),
PDF-page3/Figure4, optically reviewed: shared attributes and facade/component
hierarchy. [CGA Shape, SIGGRAPH2006](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf)
and pinned [SDK/Ghidra extrude/scope findings](../../../doc/references/buildings/esri/facade-hierarchies-sdk-3.4.12206.md)
support primitive mechanisms; no ready-made Esri facade recipe is asserted.

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 LP_NUM_THREADS=2
export LIBGL_ALWAYS_SOFTWARE=1
.venv/bin/python -W error -m unittest verify_bay_facades
.venv/bin/python test/experiments/buildings/bay_facade_cost.py --output /tmp/outshine-buildings-bay-cost.json
nice -n 10 .venv/bin/python test/experiments/buildings/bay_facade_probe.py --output /tmp/outshine-buildings-bay-models
nice -n 10 xvfb-run -a .venv/bin/python test/experiments/buildings/blender_city.py /tmp/outshine-buildings-bay-models/Bay0-cascade-framed-baysWhole-geometry.json --output /tmp/outshine-buildings-bay-models/images --samples 16
```

Repeat the last command for the other generated poses/variants, using a clean
frozen producer. Private original plans/six mesh exports, both elevations,
18 captures/logs and the assessment remain under
`/tmp/outshine-buildings-bay-field-models-837b8547c/`; costs in
`/tmp/outshine-buildings-bay-fields-cost-5637f1802.json`. Source fixtures and
their receipts remain versioned. No `lib/`/native change, new installed-library
claim or public gallery. Further whole-window work stays active at #741.
