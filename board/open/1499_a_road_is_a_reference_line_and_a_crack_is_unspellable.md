Type: task
Parent: 1498
Area: generators
Tags: scope

**A road is a reference line, and a crack is unspellable**

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

- [ ] **A road is a reference line with an elevation profile and a cross-slope**, and the mesh is
      evaluated from it rather than being the primary thing
- [ ] **A junction is a BLEND with declared continuity**, so several roads meeting share a tangent
      rather than a point -- G1 at minimum, and where two design speeds meet, the lower one's limits
- [ ] **The continuity class is DECLARED and CHECKED at build**, not discovered by driving: a generator
      that emitted a tangent discontinuity refuses rather than shipping a crack
- [ ] **A transition between curvatures is a clothoid**, so the steering rate a driver needs is bounded
- [ ] **Two tiles that share a road share its reference line**, or the seam returns at every tile
      boundary and this whole item bought nothing
- [ ] **The reference line is what the ROUTER walks**, so navigation and geometry cannot disagree

## Comments

*Why this comes first of the six*: every other task measures a world, and a world made of independent
triangle strips has defects at every junction by construction. **Building the instrument before the
thing it measures can be right would spend the first hundred findings on one cause.**
