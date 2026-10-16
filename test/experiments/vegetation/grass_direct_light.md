# Direct light at the actual saved grass contact — #468

Ground coverage is not illumination of the visible blade. This bounded Python
reference follows #463/#467's unchanged canonical stand and uses its actual
first contact for every view sample. No public library, native material/shader,
binding or world cache format changes. No SpeedTree shader is copied or claimed.

## Definition and local sources

First: SIGGRAPH2014 [Embree](../../../doc/references/vegetation/siggraph/2014-embree-ray-tracing.pdf),
PDF page2 optically inspected: separate BVH construction and ray traversal;
vectorized secondary queries. Habel etal.
[Leaf Translucency](../../../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf),
PDF page3 optically inspected: directional diffuse/specular response and back
illumination are distinct. We reuse our already installed #121/#126 reciprocal
diffuse thin-sheet `leaf_diffuse_response`, not that paper's complete BSSRDF.

The authored frame stays `grass-study-ground-1`, metres, orthonormal X/Y ground
and Z up; no geographic location or botanical species is asserted. For ground
sample q, unit view v and cached valid height h, the visible point is
p=q+h*v/v.z. Face validity distinguishes ground gaps; gaps use h=0 and normal Z.
Normals and canonical face/root IDs come from the same unlit saved geometry.

Each declared upper light direction l points toward its source. Query opaque
blade occlusion from p toward l, then multiply that binary visibility by the
existing local sheet response and the declared relative linear-RGB beam energy.
The ground uses Lambert albedo*l.z/pi. The five declared directional sources
are one sun and four sky beams, not continuous-sky integration or measured
irradiance. Authored leaf reflection(0.08,0.22,0.025), transmission(0.035,0.12,
0.018); substrate(0.2,0.13,0.055). These are not calibrated species optics.

Opaque geometric blocking plus local thin-sheet R/T is a deliberately limited
game-rendering reference. It omits optical transmission through **other** leaves,
multiple scattering, BSSRDF/specular effects and terrain horizons. It is not
complete energy transport through a canopy or final natural-grass acceptance.

## Bounds, numerical checks and correlation

For maximum blade height H, any positive-height view-then-light path stays in
the XY convex hull of0,H*v.xy/v.z,H*l.xy/l.z, expanded by form radius and offset.
Check complete neighbouring-root support for both directions before BVH work;
missing support or direction.z<0.25 fails. The stored4×4m stand supports the
same1×1m footprint,2368 roots/eight shared forms/284160 triangles.

Embree uses locally normalized Float32 geometry and bounded8192-ray packets.
Double-precision checks inspect hit distance, triangle barycentrics and grazing
cases; ambiguous hits use an independent Float64 plane/Gram triangle query.
Float32 misses are not globally certified: the independent probes are finite.
Normal offset toward the source avoids the source surface. The first20µm case
changes413/65536 classifications between10/40µm and fails the preset1per-thousand
criterion.2µm is selected:1/4µm changes37 front/39 back samples, same unchanged
criterion. This is measured offset sensitivity, not a universal geometry bound.

384 independent-reference Float64 checks cover both sun directions and every sky
beam (320 unique point/direction pairs;64 sky checks repeat in the second state).
All match.128-sun-ray scalar/packet outputs match in each state over three
repeats. Analytic sheet/layer fixtures check empty space, self-shadow refusal,
packet boundaries, normal orientation and the existing front/back R/T response.
25 source and installed-reference controls pass; no new wheel is claimed.

Front sun equals the view direction:99.966% of visible leaf contacts see the sun,
but only14.024% of ground samples do. Replacing conditional visibility by the
ground marginal therefore darkens the visible canopy incorrectly. Back sun:
49.859% leaf visibility versus11.958% ground visibility. The four-point layered
fixture also keeps the same global marginals while conditional light switches
between0 and1. No product of independent coverages recovers this information.

Mean green direct response front: correlated0.13488 versus independent0.02503;
back0.04218 versus0.01549. Disabling local transmission gives0.01463 back green.
These are declared-model results, not measured vegetation or renderer validation.

## Geometry lifetime and measured cost

BVH preparation and light state are separate. A fresh installed consumer reads
hashed original geometry/contacts with placement, blade meshing and contact
reconstruction forbidden, prepares one geometry index, reproduces both frames
exactly, then doubles all beam energies and obtains exactly doubled radiance.
No geometry/index rebuild for changed light. Frame NPZs/images are temporary
diagnostics; **finished world illumination is never cached** by this procedure.

Frozen a4d2cc242 comparison:21.68s wall/21.72CPU-s/212.7MiB RSS including all
verification, IO and counterexamples. Numerical libraries use one thread;
Embree's internal worker count is not separately measured. BVH setup0.680s;
two known encoded geometry arrays copy6819840B, internal BVH memory unmeasured.
Front/back full diagnostic lighting4.29/4.65s includes both the correlated model
and the deliberately wrong ground-mean comparison, plus reflection-only output.
Each state traces655360 rays in80 packets;240 explicit converted input arrays
copy18350080B.18/25 ambiguous rays use independent fallback. Other NumPy/native
allocation/traffic is unmeasured; process RSS is separate from array payload.
Each three-map diagnostic state owns4718592B of RGB arrays.

128-ray equal-query medians: scalar12.54/26.59ms, packet0.350/0.621ms front/back,
setup excluded. This limited comparison does not claim world/frame/GPU budgets.
The renderer should choose its own shadow implementation; Embree here is an
offline reference backend, not a proposed mandatory Outshine runtime design.

## Actual renderer qualification

Blender's raw Translucent-node colour is not automatically our linear sheet T.
The isolated vertical-sheet control removes neighbouring plants and toggles
shadows. Default Sphere mode nearly suppresses back response. Slab mode with
raw T gives approximately T times the desired response, independently of shadow
enablement. The selected EEVEE-only adapter encodes sqrt(T) in an explicit
zero-thickness Slab; reference/native optical data keep their original T.
`grass_sheet_probe.py` tests several R/T patches, front/back and shadows on/off,
with a preset0.005 absolute linear-RGB criterion. All16 patch cases pass;
maximum observed error0.000876. This is a backend qualification for this
Blender5.2.2 setup, not permission to square-root native shader coefficients.

Actual1920×1080 physical and CPU comparisons, material qualification outcomes,
their terminal receipts and descriptive common-footprint RGB errors are linked
in the PR. Eight samples, software EEVEE/llvmpipe; no GPU/frame/device admission.
Different shadow discretization and256² first-contact sampling remain visible.
In the fixed common-footprint decoded linear-RGB comparison, mean absolute
error versus the calibrated Blender field is0.01929 front/0.01965 back, compared
with0.04984/0.02840 for the independent-mean approximation. These are descriptive
results without an acceptance threshold. Back green remains0.04219 in our opaque
reference versus0.07194 in Blender; inter-leaf transport and shadow approximations
still need investigation. The field is not admitted as complete grass lighting.
Current physical front/back renders terminate0 at110.36/109.97s wall and
2110.3/1931.3MiB RSS; the six diagnostic-map renders terminate0 at96.06s/1877.9MiB.
The explicit standalone renderer exits only after synchronous renders and its
closed hash receipt; earlier post-export process143 cases remain failure evidence.

## Reproduce and open work

Use `.venv`, existing NumPy/Shapely/SciPy/embreex/bpy dependencies, explicit temp
paths and `PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`:
`grass_light_experiment.py --inputs <463-products> --output <fresh-temp>`;
`grass_light_replay.py --inputs <light-diagnostics> --output <temp-json>`;
`grass_light_preview.py --inputs <light-diagnostics> --output <fresh-temp>
--states front --modes physical`;
`grass_sheet_probe.py --output <fresh-temp>`.
Cold direct coarse-world products, botanical forms, procedural surface detail,
wind/motion, angular transitions, native material/light integration and complete
canopy transport remain open. #451 remains open; paused Material/Light ownership
is untouched, and master owns any later native adoption.
