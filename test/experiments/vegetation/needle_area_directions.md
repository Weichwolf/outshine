# Held-out needle directions and explicit partial demand

Owner: `outshine_vegetation`; [#643](https://github.com/Weichwolf/outshine/issues/643).
Extends [projected-area integration](surface_area_light.md). No native/material
binding, plant recipe or stored plant changes. The default full-query numerical
fields replay exactly against the prior frozen actual tail product.

## Independent cases

`data/needle-area-heldout.json` fixes three new views and four world lights;
each view additionally has exactly coaxial/front and opposite/back lights.
None of the three views is the original #655view. Native16×16, unit opaque
diffuse, unchanged source geometry/normals and sample budgets. Both actual
256/35-needle intervals are reused, without loading/building a whole plant.

All three views compare orders2/4. The view with the largest discrepancy over
coverage and every direct/unshadowed field is then checked at order6 and
ray biases0.2/2/20µm. This selects a countercase, not a fitted recipe or proof
that every other view has a smaller order4→6error. Exactly coaxial response
remains bitwise equal to the unshadowed field. All computed values are finite
and nonnegative. The default bias remains2µm; the new keyword is explicitly
validated even when the projection is empty.

New analytic controls cover diagonal illumination behind a vertical half-blocker
and zero backlight on a flat opaque sheet. The expected half-shadow follows the
existing frame convention (worldX maps to imageY for view+Z), not an assumed
image-axis convention. That initial new-test assumption was corrected after
inspecting the unchanged frame; no implementation tolerance was relaxed.

| Source/view | Order2→4 max, all fields | Direct-radiance max |
|---|---:|---:|
|256/0|.005111|.004072|
|256/1|.003197|.002395|
|256/2|.009994|.007154|
|35/0|.013188|.013188|
|35/1|.017546|.014562|
|35/2|.022678|.016722|

View2 is selected in both sources. Order4→6max is.004345full/.011507tail;
direct-radiance maxima.002905/.007039. Thus the original single-view stability
cannot be generalized. These are observed differences between finite quadrature
rules, not certified errors against the continuous integral.

At that view, lowering2→0.2µm changes a field by at most5.87e-6full/1.76e-5tail;
raising2→20µm changes at most.000555/.001025. Bias contributes less than the
observed quadrature discrepancy here; this is not a universal bias guarantee.
The opposite-light residual at central order4is≤1.28e-6full/4.42e-6tail across
the views, not asserted exactly zero for curved facets with shading normals.
The local SIGGRAPH2019 paper [Taming the Shadow Terminator](../../../doc/references/vegetation/siggraph/2019-taming-shadow-terminator.pdf),
page1/figures1–2, was inspected optically: geometric/shading-normal disagreement
is a distinct issue. No terminator fit, normal replacement or extra shadowing
function was applied to change this reference model.

Both2560×1440 held-out comparison figures were opened at the same linear range.
They show actual filtered source responses in three views/two world lights,
not whole-tree/PBR/species/stand acceptance. All six lights remain in raw fields.

## Correct partial operator, rejected quality selector

`integrate(..., pixel_mask=...)` accepts only an explicit same-grid NumPy boolean
mask. Hold that mask unchanged throughout the synchronous query. Candidate
enumeration remains bounded; unrequested pairs are removed **before** GEOS
intersection, source quadrature and rays. Empty demand avoids projection and
all ray work. Sample/pair/packet caps are unchanged, not increased to fit results.

Unrequested field entries are unevaluated zeros, not a free/clear source result.
The returned work explicitly records `requested_pixels` and `complete_footprint`.
A caller must preserve its mask and combine with an existing complete parent;
it must not publish a partial query as a complete cached product. Nonboolean
or wrong-grid masks are rejected before expansion. The operator is a research
query, not a new world cache or residency framework.

Independent partial/full analytic controls agree exactly. On the actual selected
views, partial order4 agrees with stored full order4 within7.78e-16(full) and
5.56e-16(tail); batching changes floating summation grouping. The partial query
returns explicit incompleteness while retaining the original source/BVH identity.

A **diagnostic heuristic** selects pixels where the order2/4difference exceeds
.002, then replaces those with order8. The fixed trigger is not a device or
image-quality gate. The hybrid retains all unrequested parent pixels.

| Source | Requested pixels/256 | Partial8rays | Uniform6rays | Unselected4/6max |
|---|---:|---:|---:|---:|
|256|35|2,725,162|2,711,873|.002703|
|35|49|652,132|535,070|.001601|

The partial8query alone already costs more rays than uniform6, before counting
the required2/4prepasses. Selected pixels contain much of the source surface;
pixel fraction is not proportional to source/ray work. Full-source selection
also misses one pixel above its own trigger. Hybrid/uniform6discrepancies remain
.002703/.006133(all fields), .001731/.003798(direct radiance). Neither field is
an exact oracle, so these differences are not evidence of guaranteed accuracy.
**Reject this selector as a cost/quality solution.** Retain the bounded partial
operator and the experiment/counterexamples; no heuristic changes the default.

`needle_area_demand_checks` constructs a stronger geometric counterexample:
a narrow vertical blocker lies between every order2/4ray but intersects a
known order6ray. Its true shadow is an analytic rectangle. Orders2/4agree to
13decimal places while their error against that rectangle exceeds.002.
Thus small quadrature disagreement cannot certify visibility accuracy, even
without interpolation noise. The control uses the real geometry/ray pipeline,
not arbitrary fabricated response arrays.

## Evidence, work and next decision

Frozen held-out algorithm `d82d60a38`; bounded partial operator/probe `d72f2f372`.
Source inputs/owners/implementation/scenario hashes and actual ray/conversion/
sample counters are in the reports under
`/tmp/outshine-vegetation-area-directions-proof-138974979/`.
One selected immutable source/BVH per run;≤8192sample points and unchanged
2,000,000source-sample cap. Partial8uses1,403,776full/315,328tail source points.
All candidate bounds are still enumerated; the mask saves later work only.
GEOS/BVH allocation internals, process peaks and device times remain unmeasured.
No memory/frame/native/GPU/RTX/A18performance claim follows from these CPU trials.

27source/27installed controls pass. Fresh installed held-out tail and partial
products match **every** saved array exactly.39actual reference modules load
inside the verified installed wheel, with source files equal to the topic;
no library changed or new wheel needed. Format/Ruff/function lengths/diff and
`make lint-docs` pass. No native gate or published image is changed.

From `.venv`, `TMPDIR=/tmp`, one NumPy thread and explicit fresh `/tmp` outputs:

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  -m unittest needle_area_demand_checks needle_area_checks \
  needle_joint_checks needle_response_checks -v
```

Run `needle_area_heldout.py --source .../physical.npz --output /tmp/new-heldout`,
then `needle_area_refine_probe.py --source .../physical.npz --parent /tmp/new-heldout
--output /tmp/new-partial`. Both preserve sources; neither fetches/builds a plant.

#643 stays active. Compare a nonnegative directional representation against
these measured finite references and preserve their uncertainty; do not build
a general adaptive-certification system first. R/T/specular/roughness, wind,
multiple scattering and actual whole-tree/stand image improvement remain open.
