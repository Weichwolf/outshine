# Projected needle-area integration

Owner: `outshine_vegetation`; [#643](https://github.com/Weichwolf/outshine/issues/643).
Continues [same-contact direct light](needle_joint_light.md). This is a finite
opaque diffuse prototype reference, not an accepted runtime/angular asset.
No plant recipe, library implementation, native binding or material changes.

## Why change the integration domain

The previous uniform128²/256²/512² source grids miss thin needle coverage and
change native16² response appreciably when refined. A compact representation
must not be fitted against that aliasing as if it were the true source response.
Instead integrate on actual projected source surfaces, including their pixel
boundaries, and avoid sampling empty image space.

For an orthographic view `v`, each outward-facing source facet contributes
`V_camera * V_light * max(dot(n_shading,light),0)` over its projected area inside
the requested pixel. Divide by pixel area. The projected-area Jacobian already
contains `max(dot(n_geometric,v),0)`; do not multiply it a second time. Gaps have
zero contribution. Camera and light visibility belong to the same source point.
The model assumes consistently oriented opaque surfaces (the actual closed
needles/short wood); it does not define two-sided open-sheet transmission.

Normal-response-before-filtering follows Han et al., SIGGRAPH2007,
[Frequency Domain Normal Map Filtering](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf),
page3/equations2–5, also inspected optically. Its normal-map convolution does not
solve three-dimensional visibility. The polygon clipping uses installed GEOS
through Shapely; Gaussian nodes reuse the existing reference-library quadrature.
Existing source normals and Embree/contact routines remain authoritative.

## Procedure and bounds

1. Read the same frozen actual256/35-needle intervals from #630/#652. One immutable
   selected source and one prepared BVH serve all quadrature alternatives/lights.
2. Project source triangles into the requested orthographic frame. Cull only
   zero/back projected area. Determine triangle/pixel candidate count first;
   refuse more than262,144pairs before expanding candidates.
3. In≤8192pair batches, intersect each projected triangle with its pixel using
   Shapely. Discard zero-area intersections, fan-triangulate the convex pieces
   and map their corners back to the original3D facet. Source facet IDs survive.
4. Apply normalized triangle quadrature inside those pixel-local pieces.
   Order1 is the centroid rule. Higher orders use the existing Gauss-Legendre
   nodes through a Duffy map with Jacobian `2*r`, not equal square weights.
5. Stream≤8192sample points. Before expansion, enforce the unchanged two-million
   sample/query bound. Test camera visibility from the source, then interpolate
   authored corner normals only at visible points. Query light only for positive
   diffuse contacts and accumulate nonnegative projected-area contributions.

Empty edge-on projections return zero. Native1/4/16/64 grids are checked before
output allocation; an oversized grid or mismatched source/BVH is rejected.
Sample exhaustion raises instead of publishing an incomplete response.
This is bounded prototype construction; its bounds are not device/frame budgets.

Exactly coaxial light reuses the already established camera visibility at the
same points/bias. It still evaluates the shading-normal cosine. This removes
518,903rays for the full order4case and95,670for the tail while every output
array stays exactly equal to the independently executed non-reused variant.
Different light directions retain separate queries; no approximate sharing.

Independent controls cover mass/first/second moments of the triangle rule,
analytic clipped square/pixel areas, two opaque overlapping layers without
double counting, rotated integrals, zero edge-on area, source/budget refusal
before ray work, and exact coaxial reuse. Eight new controls plus the prior13
pass in both source and installed environments.

## Actual result

Algorithm frozen at `7c3757ea7`, same selected source identities and camera as
#652/view0. Two lights:camera and positiveY. Native16×16 response, unit opaque
diffuse, common omitted Lambertian `1/pi`; no colour/density/needle-size fitting.
Clipping and visibility are repeated independently for each alternative; only
the immutable source/BVH is shared. This includes clipping cost in every order.

| Interval | Area order2→4 max / RMSE | Area order4→6 max / RMSE | Screen256→512 max / RMSE |
|---|---:|---:|---:|
|256needles|.001272 / .000249|.000688 / .000111|.058899 / .009666|
|35needles|.009091 / .001157|.002334 / .000381|.021676 / .002877|

All columns use the same view, two declared lights and native16² output;
their sample/ray work differs. Screen512 differs from area6 by
max.017449/RMSE.002844(full) and
max.009113/RMSE.001020(tail). Neither is an exact continuous oracle.
The decreasing area differences support this integration domain; they are not
a rigorous error bound under visibility discontinuities or arbitrary views.

Both actual2560×1440 comparisons were opened at identical linear range[0,1].
The area sequence is visibly stable at native16²; the tail's order1losses also
show why one centroid is not a universal default. These diagnostic enlargements
are not a whole-tree/species/forest image improvement or PBR validation.

## Work and memory evidence

| Interval | Front facets | Pixel candidates | Integration triangles |
|---|---:|---:|---:|
|256needles|29,178|33,050|36,828|
|35needles|4,015|5,447|6,767|

| Interval/order | Source samples | Camera/light rays after reuse |
|---|---:|---:|
|256/1|36,828|52,687|
|256/2|147,312|211,171|
|256/4|589,248|845,008|
|256/6|1,325,808|1,901,135|
|35/1|6,767|9,605|
|35/2|27,068|38,469|
|35/4|108,272|153,806|
|35/6|243,612|346,079|

The samples do not allocate an equivalent full sample array: streaming remains
≤8192points (order6≤8172). Projected triangle arrays are4,202,496B/full and
574,560B/tail. Source/BVH ownership and known copies remain as in #652.
Each pair batch additionally owns GEOS pieces/fans, original-facet lookup and
quadrature/ray scratch. GEOS/BVH internals and allocator/process peaks are
unmeasured. The small fields are proof products, not accepted world residency.
Ray conversion/copy bytes and fallback work are in each report; do not confuse
these counters with complete memory traffic. Gaussian refinement costs more
rays; there is no claim that area6 is cheaper than screen512 at unequal quality.

Host: Linux/WSL2 x86_64, four allowed CPUs, Python3.13.5, NumPy2.5.4,
Shapely2.2.0/GEOS3.14.1. Overlapping nice10diagnostics measured full order4/6
at4.23/8.05s, tail.34/.57s. These are prototype-build observations, not isolated
benchmarks, GPU/device times or runtime targets. No A18/RTX/frame claim.

## Reproduction and next step

Private inputs/results/logs only under
`/tmp/outshine-vegetation-area-light-proof-3d068baa0/` and the prior frozen inputs.
Reports include source/implementation hashes, source owner, geometry/kernel
versions, work and actual field arrays. Fresh installed tail replay matches all
fields exactly;39actual reference modules load inside the installed wheel and
their source files equal the topic checkout. No library changed or new wheel
needed. Use `.venv`, `TMPDIR=/tmp`, one NumPy thread and fresh explicit outputs:

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  -m unittest needle_area_checks needle_joint_checks needle_response_checks -v
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/needle_area_study.py \
  --source /tmp/selected-needle-interval/physical.npz \
  --screen /tmp/same-source-512-screen-study \
  --output /tmp/outshine-vegetation-area-light-new
```

#643 stays active. Before compact angular products: independent held-out views,
lights and bias/error cases, local refinement where visibility changes, and fair
equal-quality work comparisons. Then nonnegative angular/material representation
and whole-tree rendering. R/T, specular roughness, multiple scattering, wind,
species/stand quality and native integration remain unresolved.
