# Correlated two-sided leaf reflection and transmission

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
Uses the installed [#121 diffuse leaf contract](https://github.com/Weichwolf/outshine/issues/121)
and follows the [opaque source/camera study](joint_light_cohort.md).
This adds a finite Python thin-leaf reference and actual source-loaded near
images. No native shader, shared material ABI or complete plant acceptance.

## Physical scope and sources

The previous source consists of **closed needles/short wood**. Its opaque surface
integrator does not define light propagation through needle tissue. Our existing
reciprocal thin-sheet R/T model applies here to actual **open Blade surfaces**.
No thin-sheet coefficients are silently assigned to closed needles.

For the authored normal oriented toward the outgoing camera direction, retain
the installed response `R*max(n·L,0)/pi + T*max(-n·L,0)/pi`. R/T are prepared
linear RGB hemispherical coefficients, not metallic-roughness base colour and
not a measured directional BSDF. Local transmission is diffuse scattering at
the visible leaf. A different opaque geometric neighbour blocks the direct beam;
hemispherical T is not straight-through beam transparency. The existing diffuse
neighbour operator remains the route to additional scattering orders.

First filtering source: local SIGGRAPH2007
[Frequency Domain Normal Map Filtering](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf),
already inspected optically in the preceding source studies. Local EGSR2007
[Physically Based Real-Time Translucency for Leaves](../../../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf),
PDFpage2/§3.1 also opened here, separates local R/T, indirect/environment light
and the fuller BSSRDF. We reuse the existing Lambertian baseline, not its full
thin-slab BSSRDF/convolution. Prepared PROSPECT-D/gamut provenance remains the
existing library asset; no spectral solver, vendor data/code or web research.

## Shared integration procedure

The geometry kernels are now `surface_area_cells.py`/`surface_area_light.py`;
all current Python callers and affected documentation are migrated from the
old needle-specific names. No duplicate integrator or compatibility framework.
Opaque one-sided mode remains default, preserving its actual old source fields.

Explicit `two_sided=True` selects both nonzero projected facet orientations,
using their absolute projected areas. Camera visibility orients geometric
normals toward the view before the same existing ray query. Original face IDs
survive GEOS clipping/quadrature and are passed to the optional contact collector.
Shared barycentric reconstruction preserves authored corner normals and
interpolates prepared vertex R/T as a convex combination. Neither geometry nor
the projected-area Jacobian is invented or multiplied twice.

`SheetMeasure` requires an explicit thin-sheet source, source/BVH identity,
matching camera/grid/bias and valid per-vertex optics. Only already visible
contacts arrive. At each native pixel it accumulates the area-weighted **optical
response times actual light visibility**, before any footprint filtering.
For light rays use the existing signed-direction nearest-hit/Float64 fallback,
with2µm origin displacement toward the light; grass's upper-only visibility
helper is not used for lower-hemisphere transmission. Exactly co-axial light
reuses camera visibility. Zero optical response skips light queries; geometric
coverage/opaque caster geometry remain. Zero mass skips optical and light work.

Retained diagnostics are coverage W, premultiplied R/T sums, unshadowed and
joint RGB, reflected RGB and positive/negative unshadowed cosine sums. Raw joint
RGB and these measures coarsen with the existing nonnegative box filter. A
deliberately separated alternative uses mean R/T times mean cosine; it is
compared with **unshadowed** response. It does not pretend to know visibility
of skipped black contacts or to be an exact shadowed product.

## Independent controls

Analytic flat sheets match R/pi and T/pi on both camera sides at unchanged
area coverage. Reversing winding/authored normals preserves the symmetric model.
An actual opaque plane behind the receiver blocks its transmission for either
winding; front reflection is unchanged. Exactly edge-on sheets have zero
projected mass and no camera/light queries.

Two actual source vertices have weights.5/.5, view cosines.2/sqrt(.96),
R=.8/.2 and T=.1/.7. Correct reflected value is
`(.8*.2 + .2*sqrt(.96))/(2*pi)`; multiplying mean R by mean cosine differs by
more than.03. The separate transmitted expression also differs. Source corner
normals and optical vertices carry those actual values; this is a defined
finite-measure counterexample, not a fit to a rendered image.

Black material retains positive coverage but issues no extra light rays. Work
exhaustion, wrong surface semantics/owner and invalid packet IDs are checked
before optical/ray work. Zero-area fan samples can still reach the inherited
primary integrator; the light collector skips their zero mass, and counters
distinguish sampled candidates from positive visible contacts.

## Actual leaves, prepared source and images

Use the existing fixed near-tree plan
`59fbc581bf126400b60223be7202cec8d231745e589f1a25b7a28ad05d012f73`.
Select owner axis1's original six leaf IDs0–5/variants1–6. All original poses,
frames and Blade recipes survive, with one recorded local translation. There
are996vertices/1560triangles, without regenerating the whole571-leaf plant.
These remain generic leaves, not a new botanical species.

Existing physical12-wave pigment fields at chlorophyll45/anthocyanin.2µg/cm²
sample the prepared33²R/T table once per source vertex. Vein ridge is zero here;
no new vein, relief, wax/specular or species-calibrated optical model. Table
SHA256 `55be125973ee8c2f6558b4c1ff33aff97db723d7dca051f1f2108305aa00ea79`.
Prepared geometry/normal/optical/face-owner arrays use215,232B; source NPZ SHA256
`764aa72e8e98d8a8e72219112817978f9b386b5c69fbd51d22aded3f1bb40032`.

`selected-thin-leaves-1` descriptors explicitly declare surface semantics and
source hash. The numerical portrait checks those before decode, reads each
source once, prepares one existing BVH and evaluates current light. It never
rebuilds pigment lookup or leaf meshes. Actual full runs forbid both `Blade.mesh`
and `LeafOpticsTable.sample`; they succeed. This is a source-loaded diagnostic,
not the native world AssetCache or a no-BVH-work claim.

Four actual2560×1440portraits use the same camera and two primary samples/pixel:
front and opposite light, each full R/T and reflection-only. Existing Philox
camera packets, ScatterScene and authored-normal interpolation are reused.
Beam RGB(3,2.7,2.4), black environment, fixed linear→sRGB encoding, no exposure
or density fit. No second raytracer, indirect light or unreported ambient term.
Full front/back and reflection-only images were opened; the counterpart with
transmission removed visibly loses the backlit leaves. Thin leaves are visible
from both sides; opaque-neighbour correctness comes from independent geometry
controls, not the previously rejected EEVEE transmission-shadow oracle.

## Finite footprint evidence and costs

Two opposite source views/three signed lights at native64², then seven box MIPs.
Order4uses166,528candidate samples/view. Positive visible contact×light work is
499,506/498,768; camera/light rays666,034/665,296. The existing two-million
sample,262,144facet/pixel-pair and8,000,000contact×light caps remain unchanged.
Six held diagnostic arrays occupy1,310,720B/view, excluding source/BVH/scratch.
These are test responses; current illumination is not baked into the unlit source.

On this real source, independent optical/cosine averaging has max error.0002264/
.0004221 at32²; at1pixel it is.00000312/.00000816. The broad analytic counterexample
is not representative of this small actual pigment variation. There is no
justification here for a costly new optical×normal-moment bank. Actual direct
self-shadow changes native64²RGB by up to.009731/.008944. Observed order2→4joint
differences are max.001329/.000965 at64², falling to.000000808/.000000537 at1px.
These observations are not continuous-source convergence/error certificates.

The portrait renders two light states from7,372,800primary rays and334,435visible
contacts;668,870light-contact products,8,041,670total rays/225,166,760copied ray
input bytes. Full-frame primary work still visits empty background. It is a
bounded numerical reference, not an optimized real-time renderer.
Controlled output arrays191,692,800B are diagnostics, not a product budget.
Final portrait takes11.268s export-inclusive, peak486,260,736B; the installed
counterpart9.567s/485,523,456B. All three linear arrays and four PNG hashes match
exactly. Both real full runs forbid meshing/optical lookup on the prepared input.
One Linux/WSL4CPU numerical worker, concurrent source studies/checks; no isolated
speedup, GPU/RTX/A18, total-allocation or native/world-budget claim.

110source/110installed controls pass, including existing optical energy,
reciprocity, source table and partial-coverage material MIPs.68actually imported
reference-module paths/bytes match the unchanged#667wheel SHA256
`e73361c9b8df238432fb00e2c97eadf6bddaac60e5aa883fcafbdd7b038a642e`.
Fresh installed source preparation reproduces all28sheet fields/source arrays.
Opaque replay preserves all21previous fields and30reductions exactly after
the generic kernel migration. No library code/data changes.

## Reproduction and remaining work

All private inputs, figures, logs and environments stay under/tmp; current root:
`/tmp/outshine-vegetation-sheet-contacts-proof-438802bd3/`.
Order2/initial source snapshotf23b9c87f; full source-loaded portraitc343c51fd;
explicit prepared-source semantics/final portrait8e98e0b5a. Later controls/docs
do not change those valid-source response paths. No server or gallery.

```sh
TMPDIR=/tmp OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/sheet_area_study.py \
  --order 4 --output /tmp/outshine-sheet-source-new
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/sheet_leaf_render.py \
  --inputs /tmp/outshine-sheet-source-new \
  --output /tmp/outshine-sheet-portrait-new --samples 2
```

Retain this explicit two-sided baseline and current unlit optical fields.
Closed-needle transmission needs its own volume/transport definition. Specular
energy allocation must be coordinated with Material before adding a lobe to
hemispherical R/T. Existing roughness/normal methods, indirect neighbour
transport, moving LODs/wind, mature trees and complete forests remain open.
