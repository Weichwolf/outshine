Type: task
Parent: 1498
Area: world
Tags: scope

**Every road node gets an elevation, and the solve is global**

**OSM gives no z to anything.** A bridge says `bridge=yes` and `layer=1`; a tunnel says `tunnel=yes`
and `layer=-1`; a ramp says `highway=motorway_link`. **None of them says a height, and the DEM knows
nothing about either.** The owner's requirement stands anyway: tunnels, bridges, ramps, over- and
underpasses must look right and work.

## A BRIDGE CANNOT BE SOLVED LOCALLY, and that is the whole shape of this item

Raise a deck and its approaches must ramp. **The approaches are OTHER WAYS**, whose far ends are pinned
to the terrain, and if the ramp is too short the constraint propagates into the way beyond that.
**So this is one solve over the whole road graph and never a fix per bridge.**

## THE TAGS ARE EVIDENCE AND NEVER AUTHORITY

**Nothing in OSM can be relied on**, and that includes the topology rule. A bridge may be untagged, a
`layer` may be missing or wrong, two ways may share a node they should not, and a motorway may run
through a hill nobody tagged as a tunnel. **The tags are one source of evidence among several,
weighted, and never a fact the solve trusts.**

| evidence | what it is worth |
|---|---|
| **topology** -- which ways share a node | the strongest, and still not certain |
| **a crossing with no shared node** | OSM's own convention: they do not intersect. Strong, and sometimes an error |
| `bridge` · `tunnel` · `layer` | a hint that a structure is there and which way round |
| `highway=*` | a design speed, a width and a **maximum gradient** |
| **the DEM** | the ground, coarse, and wrong wherever the ground was moved |

## THE GRADIENT LIMIT REVEALS EVERY STRUCTURE NOBODY TAGGED

**This is the derivation that makes the tags optional.** A road of a given class cannot exceed a
gradient; the DEM says what the ground does; so:

- **the ground falls away faster than the class allows** -> the road cannot follow it -> **it is
  elevated**: a bridge, a viaduct or an embankment, and which one follows from how far and how long
- **the ground rises faster than the class allows** -> the road cannot climb it -> **it is below the
  ground**: a cutting, or a tunnel once the cover exceeds a declared depth
- **two ways cross at the same ground height and share no node** -> one is raised and one is not, and
  which follows from class, from what the neighbours are doing, and from the tag if there is one

**So a bridge OSM never tagged is still a bridge, because the alternative is a gradient no road has.**
The tag confirms and refines where it exists; where it is absent the geometry still answers; and where
the tag CONTRADICTS the geometry the geometry wins and the disagreement is published.

## What "correct" means here, and it is three things in a fixed order

**1. FUNCTIONALLY CORRECT, ALWAYS AND UNCONDITIONALLY.** Connected, drivable, continuous, within every
class's gradient, no crack, no gap, no impossible turn. **These are invariants and they are never
traded.** A world that cannot be driven is not a world.

**2. PLAUSIBLE.** What was probably there -- a bridge over the valley, a cutting through the ridge, an
embankment carrying the motorway -- **reconstructed from the height data and the tags together** rather
than from either alone.

**3. TRUE TO REALITY, LAST.** *If the reconstruction is not what is actually there, that is acceptable
and it is not a defect.* What is not acceptable is a world the car cannot drive.

**So the solve never refuses for want of data.** It relaxes what is SOFT -- distance from the DEM,
agreement with the tags -- and **publishes how far it had to go**. A refusal is reserved for a hard
invariant that cannot hold at all, which earthworks make very nearly impossible: ground can be moved.

## The solve

**Every node gets one z. Minimise deviation from the terrain, subject to hard constraints:**

- a way that is neither bridge nor tunnel is pulled to the DEM, **weighted by class** -- a motorway is
  pulled less because it was built on a formation the DEM does not resolve
- **gradient along every edge is at most the class's maximum**
- **a crossing with an ordering clears by a headroom**: `z_over >= z_under + clearance`, and a tunnel
  is below the surface by a cover depth
- **vertical curvature is bounded**, or the profile has a kink no car can take at speed
- and a node is one z, so **every way meeting there agrees by construction** -- which is `board:1499`'s
  continuity in the vertical

**The mechanism is a constrained relaxation and it is deterministic**: each node is pulled toward its
target and pushed by its neighbours' gradient and clearance limits, iterated to a declared tolerance.
O(edges) per pass, no allocation after the first, and the same graph gives the same answer twice.

## What must be true

- [ ] **Every node of the loaded graph has an elevation** and the solve is over the graph rather than
      per way
- [ ] **A crossing without a shared node is treated as a crossing**, and where `layer` is silent the
      inference is DECLARED and published per case -- **and where the topology itself is wrong the
      solve still produces a drivable answer**, because the invariants do not rest on the evidence
- [ ] **A gradient never exceeds the class's maximum**, and where the evidence and the invariants
      conflict **the invariant wins and the deviation is published** -- what is soft is relaxed, and a
      ramp no car climbs is never shipped
- [ ] **A structure nobody tagged is INFERRED from the gradient limit**, and what was inferred says so:
      bridge, viaduct, embankment, cutting or tunnel, with the evidence that produced it
- [ ] **A tag that contradicts the geometry loses, and the disagreement is COUNTED** -- a handful is an
      OSM defect worth reporting upstream, a pattern is a DEM too coarse to resolve what is there, and
      the count is what separates the two
- [ ] **The result is published as an inference**: which nodes were pulled to the DEM, which were
      solved, and what the residual was -- *an inference nobody can see is indistinguishable from data*
- [ ] **It is deterministic to the bit**, or a route driven twice is two worlds
- [ ] **A tile solved alone and the same tile solved with its neighbours agree at the seam**, or the
      solve has created the defect `board:1499` exists to prevent

## Comments

**The verdict is not a comparison with reality, and there is no oracle for what is actually under a
motorway near Kassel.** `board:1498`'s car is the instrument: a world that drives is functionally
correct whether or not the viaduct it invented is the one that is there. *That is why the drive suite
is the right instrument for this and a picture comparison could never be.*

**The DEM is not the road's height and that is the deepest point here.** A motorway on an embankment is
five metres above the ground the DEM reports, and a cutting is ten below. Pulling it to the DEM builds a
road that dives into every hollow. **So the terrain follows the road and not the other way round** --
`board:1505`.
