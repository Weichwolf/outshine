Type: task
State: open
Parent: 1498
Area: generators
Tags: scope

**A bridge looks like a bridge, and a tunnel has a portal**

`board:1500` gives the deck a height and `board:1505` makes the ground meet it. **What is still missing
is everything that makes the thing recognisable**: a raised ribbon of tarmac with nothing under it is
geometrically correct and looks like a bug.

**The owner's requirement is two words and both count: they must WORK and they must LOOK RIGHT.** The
drive suite decides the first; the eye decides the second, and `CLAUDE.md` says so -- *appearance is
judged by eye and in motion*.

## What the structures are, and each is a profile swept along a reference line

| | |
|---|---|
| **deck** | the carriageway plus its edge beams, thicker than the road surface |
| **parapet** | what stops a car leaving the deck, and what a driver sees at speed |
| **pier** | at declared intervals down to the ground `board:1505` did not raise |
| **abutment** | where the deck meets the embankment, which is where the crack would otherwise be |
| **portal** | the face a tunnel presents, cut into the slope |
| **bore** | the tunnel's own surface -- and the car drives on it rather than on terrain |
| **wall** | where `board:1505` could not fit a slope |

**Every one is the same mechanism**: a cross-section profile swept along `board:1499`'s reference line,
which is a generator taking `(kind, params, seed, budget)` like any other -- so a bridge reduces on the
LOD ladder like a tree does, and a distant viaduct is a ribbon with piers rather than nothing.

## What must be true

- [ ] **A bridge carries a deck, a parapet and piers**, and the pier spacing is derived from the span
      rather than chosen
- [ ] **A tunnel carries a portal and a bore**, and the bore is what the car's wheels contact --
      *a tunnel where the car drives on terrain is a tunnel that works by accident*
- [ ] **An abutment closes the seam between deck and embankment**, in geometry and not only in the
      elevation solve
- [ ] **Each is a generator kind behind the one interface**, with a capability answering what it
      achieved, so a budget too coarse for piers says so rather than dropping them
- [ ] **They are LOOKED AT.** A render case per structure at a declared camera, judged by eye and in
      motion, and the answer is yes or no

## Comments

**This is the item where the drive suite and the render suite meet.** A structure that is wrong will
usually crash the car OR look wrong, rarely both -- a missing pier is invisible to the physics and
obvious to the eye; a deck 3 cm above its abutment is invisible to the eye and stops the car dead.
**Two instruments, one subject, and neither alone would find both.**
