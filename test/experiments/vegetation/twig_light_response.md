# Matched cached needle-window light response

Owner: [#630](https://github.com/Weichwolf/outshine/issues/630), under #182.
The #626 hierarchy restored upper needle mass without producing a convincing
whole spruce. This study separates actual cached organ geometry, statistical
coverage, normal response and camera/light shadow ownership. The whole species,
stand, temporal/LOD and native optical quality remain unapproved.

## Same physical source, bounded detail

Read the unchanged #626product
`741a4403c2f242363921a3ddb5475c55944eceb6faed3e1845cca2b376cdaf3d`.
Select actual terminal axis30258/path(183,13,15),547needles,0.789835m. Request
its first256needles and final35needles from the same existing procedural models.
Global needle starts7303426/7303938; capacity256/64, weights1/0.546875. No
new growth, whole foliage or new needle prototype. Existing1024needle/250,000
triangle request limits precede detail. Actual58,368/7,980triangles,
10,589,184/1,447,740geometry-array bytes and133,468detail bytes per bounded twig
request. Main axis wood is excluded from both; canonical short wood remains.

Use actual saved corner reflectance, geometry normals and leaf membership for
the explicit source. Reuse the same needle/wood BRDF mixture and roughness/
translucence controls; opaque physical faces bypass card-only alpha and normal
maps. The proxy consumes the unchanged32²finite bank, same world frame, UV,
capacity and tail weight. These share physical owner intervals, not individual
statistical geometry. No needle-size, opacity or authored area retuning.

The actual orthographic camera/view and directional light are equal for the
frontal comparison. Full images2560×1440/64samples; physical horizontal spans
384,64or16pixels are measured from Blender's actual camera frame. Enlarged plots
explicitly retain native crop sizes. A384pixel twig magnifies a32²source and
is a diagnostic, not an admitted near LOD. Even16pixels tests a real far window.

## Linear export is measured

A known quarter-covered emitter with binary RGB(0.125,0.25,0.5) establishes that
the actual loaded EXR is premultiplied. Interior mean alpha0.2496855;
meanRGB-minus-alpha×source error6.7817e−8, versus0.375157for straight RGB.
A32-bitEXR does not imply32-bitinternal shading: all sampled interior values
roundtrip exactly throughFloat16 and rare per-pixel deviations reach1.2207e−4.
The initial new diagnostic's1e−5single-pixel assumption was contradicted and
replaced by the intended footprint-mean semantic test1e−6, retaining the original
per-pixel errors. No existing engine/material accuracy gate was changed.

RGB sums below are raw linear premultiplied values, not multiplied by alpha a
second time. PNGs use Standard display; analysis composites over linear0.18gray.
The actual complete frames and magnified plots are opened, not inferred from logs.

## A real coincident camera/shadow failure

| Frontal case | Alpha explicit/proxy | Green linear sum explicit/old proxy/shared proxy |
|---|---|---|
|256needles,384px|3045.810/2882.123|57.8744/0/101.9886|
|35needle tail,64px|437.151/418.224|8.0430/0/14.0159|
|256needles,16px|5.38673/3.88181|0.105223/0/0.183007|

The old separated camera-only/shadow-only cards occupy the same surface for this
view/light query. They make the proxy black although the explicit needles remain
lit. One common card with shadows still enabled restores colour. For384px its
wholeRGBAarray is exactly the unshadowed proxy's array, and alpha exactly equals
the original separated proxy. The tail also preserves alpha exactly. This isolates
representation self-shadowing without a new coverage/material-density factor.
It does not establish why the unrelated whole portrait is thin.

Unshadowed frontal explicit/proxy green sums60.5213/101.9886still differ materially.
Conditional mean green0.0198703/0.0353867shows a separate unresolved normal/BRDF
aggregation error. The symmetric normal example n=(±sqrt(3)/2,0,0.5), light=X,
has true mean clamped cosine sqrt(3)/4 versus0from its normalized mean. Rotating that same pair90°aroundZ keeps exactly the same first moment but
changes its X-light mean to0. The stored mean cannot distinguish those two
normal distributions. The follow-up stays under #182. A blanket normal-length/albedo multiplier therefore
cannot solve all directions. Back-light
64pxgreen is0.0234613/0.000141740with shadows,0.0256677/0.000142038without;
this EEVEE diagnostic is not a physical R/T or transport oracle.

First local source: SIGGRAPH2007 *Frequency Domain Normal Map Filtering*, PDFp.2
opened optically with reflectance equation1 and NDF discussion; p.1's text also
explains nonlinear normal averaging. NDF filtering is not its solution to cast
shadows/occlusion. Existing #601coverage evidence and #451/#462/#468/#477/#481
keep their limits. The later material reductions PR #632/#635 reuse those actual
contacts/operator; no competing grass transport solver is introduced here.

## Share geometry while preserving each light's domain

The whole preview already assigns distinct blocker collections through
`organ_preview.link_shadow_products`. On the actual machine a red/green two-sun
counter proves that this works: no linking blocks both; the old red-only blocker
leaves green0.317382889; no blocker gives red/green0.317382813/0.317382965.
The17,258equal sun/fill view IDs in the #626portrait therefore cannot be treated
as redundant global-domain declarations. Camera/sun and camera/fill have zero
equal view IDs in that portrait; the frontal failure alone does not diagnose it.

`window_portrait_roles.role_packets` now unions geometry by actual(window,view),
while retaining the exact camera and named-light membership set. A camera/shared
surface uses the existing all-role shader; shadow-only geometry retains its
specified owners. New optional `shadow_lights` dictionaries override a shared
surface's default all-light membership. Legacy `shadow_light` and ordinary
all-role wood/ground retain their old behavior. Equal sun/fill geometry can share
one surface assigned to both collections; differing views remain distinct.
Budgets, source paths/IDs, windows, weights and all material parameters remain.

A second actual two-sun counter validates explicit ownership, including an
all-role surface with a conflicting legacy fill tag: explicit sun ownership
still leaves exactly green0.317382889, and explicit sun+fill blocks both. No
native API, per-light binding or material ownership is changed by these Python
preview properties. This fixes duplicate coincident proxy surfaces; it does not
claim general different-angle visibility correlation or a correct filtered NDF.

The complete unchanged #626plant was rendered again and both2560×1440/32sample
portraits actually opened at full resolution. All image pixels are exactly equal;
PNG bytes/hashes differ. This corrects the new coaxial counter and reduces work,
not whole-form quality.17,258surfaces/34,516triangles/3,727,728geometry-array bytes
are removed. Camera surfaces79,085, shared camera/light0, shadow surfaces140,912;
2,800,954total camera/shadow triangles and49,311,744geometry/instance array bytes.
These are emitted NumPy arrays, not total Blender/GPU resource allocations.
Same scene, backend, plant identity, physical inventory and requested view tables.
New PNG SHA256`46fc6ec980491911bf759e44586e1982d9c955e9a0aa4a7ce2682ce6e9c386a9`;
old`2f21104830bf4fce141922c812ec157137e947905cba6ad56f4e596238b366d4`.
The spruce remains thin and tiered; no species default or forest run is admitted.

## Verification, work and reproduction

Five focused binding controls check actual saved corner colour/normals/membership,
actual camera pixel units, the independent normal-mixture counter, real owner
selection and strict coincident-view merging. Four packet controls independently
reconstruct all camera/light membership triples, uniqueness, immutable indices,
bounded refusal and actual Blender blocker collections including legacy owners.
The affected23source and23installed controls pass, with the strengthened same-mean/different-response scalar counter also checked in both environments. Earlier19/
19controls and fresh installed two-case export pass: every physical/window field
and source fact is exact,54actual reference imports are inside the verified
205-module #626wheel. The library is unchanged; this does not reapprove unrelated
package features or native/whole optical quality. Format/Ruff/function limit
and diff checks are separate from the actual renderer witnesses.

Linux x86_64/4logical CPUs/15.44GiBreported physical RAM, Python3.13.5/NumPy2.5.4/bpy5.2.2, llvmpipe.
Individual probes render at normal measurement priority, overlapping other probes;
CPU controls use nice10and one NumPy thread. Do not rank those observed times as
a device/frame speedup. Source bounds/work and all per-frame process/RSS/backend
reports remain with original outputs. Whole render344.641s/process348.619s,
peak2818.016MiB, overlapping the light-linking probe; old328.853swas under another
load. No timing speedup follows. New allocator/internal-copy totals are not
measured. Original axis/site/window and near-detail limits remain unchanged.

Reproduce from`.venv`with`TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1`and
`PYTHONPATH=lib/src:test/experiments/vegetation`. `twig_light_source.py`takes
explicit product/catalog/models/output and freezes the two source intervals.
`twig_light_render.py`takes case/catalog/output, physical/windows, front/back/side,
16/64/384pixels and matched/off/single shadow configuration. Single requires the
same finite camera/light view; it refuses differing views. `twig_light_alpha.py`
measures known emission/coverage and can analyze its existing EXR without rerender.
`twig_light_linking.py`tests the old and explicit blocker domains. Plot programs
read actual linear arrays; none invent missing radiance or promote a crop to a
whole image. The existing portrait CLI consumes the same complete cached plant.

Original cached inputs, full-resolution frames/linear EXRs/NPZs, opened plots,
source/installed replay and terminal logs remain under
`/tmp/outshine-vegetation-twig-light-proof-c5d735e81/`. Shared source/catalog/control
stay at their original manifest paths. No showcase server, repository images or
new default species. #182/full vegetation goal remain active.
