Type: task
Parent: 1498
Area: generators
Tags: scope

**The terrain is cut and filled to meet the road**

**A road is not laid on the ground; the ground was moved to carry it.** Once `board:1500` has given
every node an elevation, the road and the DEM disagree almost everywhere -- by five metres on an
embankment, by ten in a cutting -- and **that disagreement is what makes a world look wrong**: a
motorway floating over a valley, an approach ramp buried in a hill, a bridge whose deck clears the river
while its abutment sticks out of the water.

## What must be true

- [ ] **The terrain is deformed to meet the road along a corridor**, and outside the corridor it is the
      DEM's untouched
- [ ] **A fill slopes away at the angle of repose** and a cut rises at it, both declared per material
      rather than constant -- *this is ordinary earthworks and the numbers are declared data*
- [ ] **A retaining wall stands where the slope would not fit**, which is what a scenario declares a
      budget for rather than the generator deciding
- [ ] **The corridor blends back into the DEM with continuity**, or the seam is a cliff and the car
      finds it
- [ ] **Two roads whose corridors overlap produce ONE surface**, and the rule that decides it is
      declared -- a junction on an embankment is where this bites
- [ ] **A bridge and a tunnel do NOT cut**: the terrain passes beneath the deck untouched, and a tunnel
      cuts only its portal
- [ ] **What was moved is published per tile** -- cubic metres cut, cubic metres filled -- because a
      tile that moved a mountain is a defect in the solve above it

## What this may not do

**It may not make the terrain a function of the road alone.** A river under a bridge, a building beside
a cutting and a field above a tunnel all sit on ground the corridor must not eat. *The corridor is
bounded and what it may not touch is declared.*
