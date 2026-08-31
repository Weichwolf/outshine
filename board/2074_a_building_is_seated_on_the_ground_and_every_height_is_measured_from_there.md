Type: defect
State: active
Area: world
Tags: buildings, terrain, measured

# A building is SEATED on the ground and every height in it is measured from there

**Benchmark** — Unreal: a landscape-placed actor snaps to the terrain under its bounds and its
components are laid out in the actor's own local frame, so a mesh cannot lose height to a slope.
RAGE: a map object carries a placement matrix and its LODs are authored relative to that matrix.
**They agree**: a body has ONE origin and everything in it is measured from that origin, never from
the world.

## What was measured

`BuildingField::RingBase` sampled the DEM at the ring's CORNERS and returned the LOWEST. The mesher
then fitted a least-squares plane through those corners (`Site2Ground`), walked the ring at two
metres and seated the body on that plane's maximum. Two things follow and both were wrong:

- **the ground inside the outline was never asked.** A plane through the corners cannot see a rise
  in the middle of a footprint, and a DEM post is coarser than a large building
- **the roof was measured from the PLAN'S BASE**, which is the lowest corner:
  `EavesZ(s) = s.FootM + s.EavesM`. The walls ran up from the seat, the roof stayed at the base, so
  on a slope the roof cut into the walls and the building lost exactly the ground's spread in
  storey height

**And the check that was supposed to catch it could not fail.** `buildings: seated BELOW the ground
they stand on` compared five interior samples of the fitted plane against that same plane's maximum
over the ring. A plane's maximum over a convex outline is on the outline, so an interior sample can
never exceed it: **0 buildings on 43 992 footprints**, and the zero was construction rather than
measurement. That is the ninth blind check found this session.

## What was done

- `RingBase` samples the ring AND a grid inside the outline for any footprint wider than 20 m, and
  returns the DEM's true lowest and highest under the whole outline. Under 20 m the corners bound
  it and the grid is skipped, because a footprint smaller than a DEM post learns nothing from it
- `StructurePlan` carries `SeatAslM` and `FootAslM` beside `BaseAslM`, and `Site2Ground` carries
  them as `High()` and `Low()`. The plane is still what a WALL follows; the two extremes are what
  the body STANDS on
- **`EavesZ` is `SeatM + FootM + EavesM`.** `FootM` stacks a part above the one below it and keeps
  its meaning; `SeatM` is what the stack rests on. The facade's own uv starts at the floor
- `PlinthTopZ` and `PlinthFootZ` are called ONCE per part rather than six times. Both walked the
  ring at two-metre steps and both were called from the plinth, the floor, the walls, the pixel
  budget and the pavement, so every footprint paid for six identical walks to learn two numbers

## Measured after

    Heidelberg   0da91522 -> 0c7c65e9
                 982 238 -> 1 048 513 triangle(s)
                 varies by 2.327 -> 2.477 of 255

LOOKED AT: the foreground buildings now carry visible walls with their roofs sitting ON them, where
before the same houses read as flat roofs close to the ground. The triangle count rises because more
of them clear the pixel budget for full architecture once they are their true height.

## What will be true

- [x] the ground UNDER the outline decides the seat, interior included, not a plane through the
      corners
- [x] every height in the massing is measured from the seat, so a roof cannot cut into its walls
- [ ] the blind control is replaced by one that can fail: how far the ground rises ABOVE the corner
      fit, counted and at its worst, because that is the height every building was seated too low by
- [ ] the walls reach DOWN to the lowest ground under the outline with no gap, checked by looking
      at a building on the steepest slope this tree stands

## What this does NOT cover

The DEM's own resolution. Seating on the true maximum of a coarse height field is still seating on a
coarse height field, and a building on a cliff edge will show it.
