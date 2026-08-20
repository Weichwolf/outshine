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

## What OSM actually carries, which is more than it looks

| | |
|---|---|
| **topology** | which ways share a node -- **exact and reliable**, and it is the primary datum |
| **ordering** | `layer` says WHAT IS OVER WHAT and never how high |
| **class** | `highway=*` implies a design speed, a width and a maximum gradient |
| **ground** | the DEM, everywhere, and coarse |

**AND THE FREE RULE NOBODY TAGS**: two ways that cross in plan and **share no node do not intersect** --
that is OSM's own convention, so *every* over- and underpass is already marked topologically, `layer` or
no `layer`. **Where `layer` is absent the engine still knows there is a crossing** and infers which is
above from what is a bridge, then from class, then from what its neighbours are doing.

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
      inference is DECLARED and published per case
- [ ] **A gradient never exceeds the class's maximum**, and where the constraints cannot all hold the
      solve **refuses naming the ways it could not satisfy** rather than shipping a ramp no car climbs
- [ ] **The result is published as an inference**: which nodes were pulled to the DEM, which were
      solved, and what the residual was -- *an inference nobody can see is indistinguishable from data*
- [ ] **It is deterministic to the bit**, or a route driven twice is two worlds
- [ ] **A tile solved alone and the same tile solved with its neighbours agree at the seam**, or the
      solve has created the defect `board:1499` exists to prevent

## Comments

**The DEM is not the road's height and that is the deepest point here.** A motorway on an embankment is
five metres above the ground the DEM reports, and a cutting is ten below. Pulling it to the DEM builds a
road that dives into every hollow. **So the terrain follows the road and not the other way round** --
`board:1505`.
