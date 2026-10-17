# Joint needle light: angular and spatial reduction

Owner: `outshine_vegetation`; [#643](https://github.com/Weichwolf/outshine/issues/643).
Continues [isolated filtered views](isolated_normal_views.md) and the existing
[same-contact light model](needle_joint_light.md). This is a bounded Python
counterstudy, **not an admitted compact shader or a new whole-tree image gain**.
No library/native/material layout, recipe, geometry, density or albedo changes.

## Shared finite source and model

The existing canonical-seed718/capacity32 procedural prototype supplies positions,
original corner normals and faces. Reuse its three needle meshes/catalog; emit
this bounded prototype once, then prepare one existing ScatterScene/Embree for
all cameras. No whole plant/axis expansion or new raytracer. Physical source SHA256:
`6bfb131f27c09d9bb0fafa0d9695500cb047882954785fcbc57d181442b1c537`.
Source JSON retains the catalog/pattern/seed and original input hashes.

Views are normalized(1,0,.35),(.2,.9,-.39),(-.62,.41,.67). Existing GEOS clipping
and area quadrature produce native16×16fields. Camera visibility and authored
normal interpolation run once; `JointMeasure` streams the same visible contacts
through lights, keeping the2µm bias. At each pixel, camera visibility included:

`W=sum(w)`, `U(L)=sum(w*max(n·L,0))`, `F(L)=sum(w*max(n·L,0)*V_light)`.

Only positive cosines request light rays; other flags remain unevaluated.
Exactly co-axial light reuses known camera visibility, with exact identity rather
than tolerance merging. Default integrator outputs remain unchanged. The optional
collector checks source/scene/view/grid/bias before quadrature or rays.

An actual wall/two-contact control has weights.5/.5, cosines1/.2, visibility0/1:
joint=.1, unshadowed=.6, conditional-positive marginal product=.3. The latter
uses the same actual rays; it is neither exact joint transfer nor a cheap runtime
alternative. Model: unit opaque direct diffuse, common1/pi omitted, no external
occluders, R/T or multiple scattering.

Local SIGGRAPH2002
[Precomputed Radiance Transfer](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf),
PDFpage2/Figure2 also inspected optically, motivates separating geometry transfer
from changing illumination. This is not its full SH/glossy implementation. Reuse
existing contacts/rays/normals; no second transport operator. The SIGGRAPH2007
filtering source/limits remain in [normal_lobes.md](normal_lobes.md).

## Angular and rank alternatives

Compare32object-space Fibonacci knots with32/64view-relative knots using existing
source projection frames.32knots use poles and polar rings(degrees,count):
(2,4),(10,4),(40,8),(90,8),(140,6).64knots use poles and(1,4),(3,8),(10,8),
(25,10),(50,10),(85,12),(130,10). These are candidate grids, not optimal ones.

Object-space evaluation has eight independent lights. View-relative evaluation
has **six independent lights**, polar1/5/22/65/110/165degrees, plus two training
pole controls. All three cameras participate in the fit; no held-out-camera claim.
The initial table includes both controls; the footprint table excludes them.

ConvexHull selects triangular cones. Nonnegative weights solve
`sum(lambda_i*L_i)=L`; they do **not** sum to1. They preserve homogeneous linear
response inside a cone, not shadow discontinuities or the clipped-cosine horizon.
Invalid/non-enclosing unit knots are refused. Greedy positive reduction chooses
actual response columns and fits others with SciPy NNLS, ranks1/2/4/8. Compare
truncated signed SVD at1/2/4/8/12. Rank12matches four3-vector normal lobes'
12per-pixel scalars; it does not raise a world/runtime budget. SVD training
Frobenius optimality does not imply best held-out, maximum or image error.

Signed factors filter linearly; only final response is projected to[0,W]. Since
finite true F lies there, projection cannot increase pointwise absolute error,
also after consistent nonnegative filtering of raw response and W. It does not
remove approximation error. Report raw negatives, clipped error and quantization.
Separately filtering a carrier/normal and visibility then multiplying them would
lose spatial covariance again; this representation does not do that.

Order4, fixed native16², absolute unit-diffuse response:

| Knots / representation | Maximum query error | RMSE |
|---|---:|---:|
|32object, angular only|.166063|.011903|
|32view-relative, angular only|.121353|.008374|
|32view-relative, positive rank8|.172045|.010798|
|64view-relative, angular only|.095812|.006338|
|64view-relative, positive rank8|.142086|.010364|
|64view-relative, signed rank8/clipped|.106088|.007560|
|64view-relative, signed rank12/clipped|.106988|.007342|

Worst64-light angular case: camera1/pixel119/light5°, source.193730 versus
interpolated.289542, W=.578067. More knots reduce mean error but still miss a
narrow source-shadow feature; rank12does not repair it. Signed rank8has1841
negative training and269negative evaluation entries before projection, zero
afterward. No general near-detail/default admission.

Order2→4 on identical64-light fields differs by max.014706/.026752/.020155 across
cameras; corresponding evaluation-query maxima.011469/.018593/.010005. These
observed quadrature differences are not continuous-source bounds. Keep angular,
rank and source uncertainty separate; this is not an exact continuous oracle.

## Footprints: retain correlation, then simplify

Reuse `normal_lobe_study.coarsen` nonnegative2×2box filtering on **joint response**,
W and raw signed factors. Never average conditional responses or mix view frames.
Compare filtered original rank8factors with per-footprint refits; no extra rays.
At1pixel the refit has at most three spatial ranks for these three cameras; do
not report requested8/12as eight/twelve actually stored ranks.

Six independent lights, pooled same finite16²source, not a fresh coarse bake:

| Footprint | Angular max | Filter rank8 max | Refit rank8 max | Refit RMSE / RMS W |
|---|---:|---:|---:|---:|
|16×16|.095812|.106088|.106088|4.386%|
|8×8|.033924|.047757|.044354|2.978%|
|4×4|.010436|.012619|.012598|2.071%|
|2×2|.005891|.006265|.005816|1.707%|
|1×1|.003641|.003961|.003641|1.392%|

Smaller footprints tolerate simpler transfer here. Per-level refitting does not
always improve held-out RMSE (8²counterexample). This motivates footprint-driven
detail, not a fixed distance threshold, continuous-angle guarantee, view blend,
temporal stability or forest/species acceptance.

## Costs, checks and images

Same8,000,000contact×light cap/camera, sample demand bounded before quadrature.
64knots+8queries allow111,111candidates; actual order4counts96,752/99,072/93,328
fit without relaxing it. Visible contact×light work6,272,424/5,857,920/6,022,368.
Actual rays4,320,183/4,128,188/4,121,488; copied ray-input bytes120,965,124/
115,589,264/115,401,664. Substantial offline work even for32needles.

Collector fields591,872B/camera. Rank8Float32maps24,576B plus2,048B angular
coefficients; rank12maps36,864B plus3,072B. Coverage, geometry/BVH, fitting scratch
and containers are extra. Float16rank12payload19,968B changes clipped prediction
by≤.0002572; Float32rounding≤2.81e-8. Both are far below angular/model error;
tighter packing is not the next quality fix.

Local Linux/WSL4CPU, one numerical thread, overlapping order2/checks: order4
per-camera2.875/2.588/2.599s; SVD fit.00302s. Not an isolated speedup, GPU/A18/RTX
or runtime-budget proof. Allocation count, process peak/residency and driver
costs are unmeasured; array/copy counters must not be presented as those totals.

85source/85installed controls pass.64actually loaded outshine_ref paths/bytes
match the unchanged#667wheel SHA256
`e73361c9b8df238432fb00e2c97eadf6bddaac60e5aa883fcafbdd7b038a642e`.
Fresh installed64-light/order4replay preserves source bytes and all18source/
30reduced arrays exactly. Controls cover real opaque covariance, unchanged old
outputs, ownership/budget refusal before rays, zero work, enclosing cones,
positive fitting, final projection and filterable raw factors/coarse actual rank.

Two actual2560×1440plots were opened: native16²fields at5° and six-independent-
light error versus footprint. Camera1's shadow error remains visible. These are
magnified measured fields, not detailed whole-tree beauty renders. Prior#672
whole images/adapter are unchanged; no new species/whole-plant art gain claimed.

## Reproduction and decision

Private results/logs/images exclusively
`/tmp/outshine-vegetation-joint-basis-proof-770646256/`. Original models remain
`/tmp/outshine-vegetation-whorl-balance-proof-6df639bf9/frozen-source/models` and
catalog `/tmp/outshine-vegetation-window-fidelity-proof-813112870/catalog`.
Preserve these actual inputs. Source-field snapshot3b8a2c3ba; later footprint
analysis reuses those fields, without modifying imported live code.

```sh
TMPDIR=/tmp OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/joint_basis_study.py \
  --models /tmp/existing-models --catalog /tmp/existing-catalog \
  --output /tmp/outshine-vegetation-joint-new --capacity 32 \
  --order 4 --angles view --knots 64
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/joint_basis_mips.py \
  --source /tmp/outshine-vegetation-joint-new \
  --output /tmp/outshine-vegetation-joint-new-mips.json
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/joint_basis_plot.py \
  --source /tmp/outshine-vegetation-joint-new \
  --mips /tmp/outshine-vegetation-joint-new-mips.json \
  --output /tmp/outshine-vegetation-joint-new.png
```

Keep the bounded collector/negative comparisons as research controls. Reject
unconditional low-rank adoption or full-bank rebaking from this pilot. Next
compare footprint-specific response on additional sources/views with the existing
normal/shadow representation before choosing a stored product. R/T, roughness/
specular, wind, external light/shadow and near/far/whole-stand acceptance remain
open in#643/#1.
