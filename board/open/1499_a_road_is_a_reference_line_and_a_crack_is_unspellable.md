Type: task
Parent: 1498
Area: generators
Tags: scope

**A corridor is a reference line, and a crack is unspellable**

**THIS IS NOT ONLY ABOUT ROADS.** A road, a railway, a canal, a runway, a pipeline, a power line and a
wall are all the same shape: **a reference line with a cross-section swept along it.** They differ in
the profile and in the limits -- and a railway's limits are far tighter than a road's, which is why the
rails are the harder case and the better test.

**Today a road is a strip of triangles from OSM nodes.** Two ways meeting at a node produce two strips
that agree about a position and about nothing else -- not the tangent, not the curvature, not the
cross-slope -- so a seam is a step, and a step at 30 m/s is a crash. **Detecting those is the wrong
answer. Making them unspellable is the right one.**

## The shipped mechanism, looked up rather than recalled

**ASAM OpenDRIVE**: *the basic element of every road is the road reference line*, and its plan view is a
sequence of `line`, `arc`, `spiral` and `paramPoly3`; elevation along `s` and the roll angle are
**sequences of cubic polynomials**. The sentence that matters is theirs:

> *Spirals may be used to describe the transition from a `line` element to an `arc` element **without
> causing leaps in the curvature**.*

**A spiral is a clothoid -- curvature linear in arc length -- and that is what real roads are built
from**, because it is the shape a driver produces at constant speed and constant steering rate. **Take
the MECHANISM**: a road is a curve plus profiles, and the mesh is what the curve is *evaluated into*.
**The assumption that comes with it**: a road has a design speed, and the curvature and its rate are
bounded by that speed -- which is true of a built road and is exactly what OSM's `highway=*` classes
imply.

## What must be true

- [ ] **A corridor is a reference line with an elevation profile and a cross-slope**, and the mesh is
      evaluated from it rather than being the primary thing. *The PLAN VIEW is built* --
      `src/core/ReferenceLine.{h,cpp}`: straights, arcs and spirals, `At(s)` answering position,
      heading and curvature. The elevation profile and the cross-slope are not
- [ ] **A junction is a BLEND with declared continuity**, so several roads meeting share a tangent
      rather than a point -- G1 at minimum, and where two design speeds meet, the lower one's limits
- [ ] **The continuity class is DECLARED and CHECKED at build**, not discovered by driving: a generator
      that emitted a tangent discontinuity refuses rather than shipping a crack
- [x] **A transition between curvatures is a clothoid**, so the steering rate a driver needs is bounded
      -- and **a transition that would leap has NO SPELLING**: laying a straight against an arc is
      refused with a sentence that names the spiral as the repair
- [ ] **Two tiles that share a road share its reference line**, or the seam returns at every tile
      boundary and this whole item bought nothing
- [ ] **The reference line is what the ROUTER walks**, so navigation and geometry cannot disagree
- [ ] **One mechanism carries every corridor**: `highway=*`, `railway=*`, `waterway=*` and the rest
      differ in their profile and their limits, never in their machinery

## Comments

*Why this comes first of the six*: every other task measures a world, and a world made of independent
triangle strips has defects at every junction by construction. **Building the instrument before the
thing it measures can be right would spend the first hundred findings on one cause.**

## The plan view is built, and the number that says so

[MEASURED] a line -> spiral -> arc -> spiral -> line at a 400 m radius, sampled every 0.1 m:

| | |
|---|---|
| worst curvature step over 0.1 m | **2.08333333e-06 1/m** |
| what the spiral's own linear rate accounts for over 0.1 m | **2.08333333e-06 1/m** |

**The two are the same number**, which is what *there is no leap, only the ramp* means when it is
measured rather than asserted. The heading's worst step is 2.5e-04 rad, which is the curvature times
the step, so there is no kink either.

**And the negative control is the point of the item.** Laying a straight directly against an arc is
REFUSED:

```
segment 1 enters at curvature 0.002500 where segment 0 leaves at 0.000000, and a leap in
curvature is a step in the lateral force -- a spiral is what carries a transition without one
```

*A crack is not found downstream. It has no spelling upstream.*

**A clothoid is integrated rather than tabulated** -- 8-node Gauss-Legendre over the heading, which is
deterministic, allocation-free and needs no Fresnel table. The arc and the straight are closed forms
and only the spiral pays for the quadrature.

## What proves it

**`test/unit/core/AReferenceLineCarriesACurvatureWithoutALeap.cpp`** -- 15 checks: the continuity of
curvature and heading against the spiral's own rate, the refusal and both halves of its sentence, the
curvature at the ends and inside the arc, a station off either end placing nothing, and a line of no
segments refused.

