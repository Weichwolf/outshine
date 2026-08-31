Type: feature
State: active
Area: generators, world
Tags: terrain, door, determinism, benchmark

# A generator may return a STAMP, and the ground is what applies it

**Benchmark** — Unreal: *Deform Landscape to Splines* moves the Landscape to meet a spline, with a
Half-Width, a Side Falloff and an End Falloff, and the blend is COSINE-weighted; the same operation
by hand is the Landscape's Flatten brush. RAGE: the map ships already cut and filled, because an
artist did it. **Neither DERIVES it**, so the mechanism is ours -- and the one published
formalisation is Galin et al. (Eurographics 2010), with GRASS `r.carve` the only open
implementation. There is no published algorithm for a building pad on a slope at all.

## The owner's rule

> Every building must be able to return, OPTIONALLY, a stamp that plateaus the terrain.

**And "optionally" is the load-bearing word.** A building on flat ground returns nothing and the
ground is untouched. A building on a slope returns a stamp and the ground becomes flat under it --
which is what a site does in reality: it balances cut against fill and then builds.

## Why the GENERATOR may not cut it itself

The generator does not own the ground. `CLAUDE.md`: content is data, the engine has the verbs, and a
generator hands back a FINISHED result rather than reaching into somebody else's state. A generator
that wrote terrain would be a second writer of one field -- exactly the "second holder" the
invariants forbid, and the thing that makes two subsystems disagree about the same place.

So the generator DECLARES and the ground APPLIES. That also makes the operation orderable, which it
has to be: two stamps that overlap disagree, and if the ground applied them in completion order the
same declaration would render different bytes twice.

## The shape

    struct Stamp {
      Span<const double> RingEastNorthM;   // the footprint, in world metres
      double PlateauAslM;                  // what the ground becomes inside it
      double FalloffM;                     // how far out it blends back
    };

    class Generates {
      virtual bool make(const Ask &, Geometry &) const = 0;
      virtual bool stamps(const Ask &, Stamps &) const { return false; }   // OPTIONAL
    };

A default that returns `false` is what makes it optional without touching a single existing
generator: `Structures` keeps compiling and keeps stamping nothing.

**`PlateauAslM` is already decided and already computed.** board:2074 settled it -- the seat is the
MEAN over the footprint's corners and its interior grid, not the highest point, because a site
balances cut against fill. `BuildingField::RingBase` returns exactly that number today and then
throws away the half nobody could use, because nothing could move the ground.

**`FalloffM` carries Unreal's shape and its reason**: cosine-weighted, so the ground leaves the pad
with zero slope and rejoins the terrain with zero slope, which is what stops the seam reading as a
crease. It is a DECLARED number with an origin, in the table, never a constant in a generator body.

## MEASURED FIRST, and the number decides the order of the work

`include/Generate.h` now carries `Stamp` and the optional `stamps` verb, and nothing else changed --
the default answers `false`, so `Structures` compiles untouched and stamps nothing. Before writing a
ground that applies them, the question is how many stamps there would BE and whether the ground can
express them. Kaiserberg, 95 907 footprints:

    a stamp would fill, p50                        0.101 m
    a stamp would fill, p95                        0.461 m
    a stamp would fill, worst                      9.538 m
    footprints worth a stamp (fill > 0.25 m)      15 105      15.7 per cent
    footprint across, p50                         13.388 m
    footprints narrower than a ground cell        87 127      90.8 per cent

The fill is the MEAN seat minus the lowest ground under the footprint, which `RingBase` already
computes and threw away because nothing could move the ground. The median building needs ten
centimetres and does not care; the worst needs nine and a half metres, which is a house standing in
mid-air or buried, and one in six is worth a stamp at all.

**And ninety-one per cent of footprints are narrower than one ground cell.** The median is 13.4 m
across against a ground whose vertices stand about 25 m apart. **A stamp applied by moving existing
vertices cannot express nine buildings in ten** -- it would flatten a whole cell to seat a house
across half of it, which is a worse picture than the tilt it was meant to fix.

**So the ground gaining vertices where a stamp asks for them is not a later refinement, it is the
PREREQUISITE**, and the item below that says so has the number now. Applying stamps to today's grid
would produce a measurable improvement in one building in ten and a visible defect in the rest.

## THE TESSELLATION IS THE POINT, AND IT IS NOT A STAMPING DETAIL

The owner corrected the reading above, and the correction is larger than the item was.

**First: the boundary is COMPUTABLE.** A fill of `h` metres seen from `d` metres subtends `h/d`
radians, and one pixel at 720p over a 60-degree vertical field is `(60 pi / 180) / 720` =
**1.4544e-3 rad**. So a fill vanishes into a pixel beyond `d = h / 1.4544e-3 = 687.6 h`:

    fill p50    0.101 m   ->  invisible beyond      69 m
    fill p95    0.461 m   ->  invisible beyond     317 m
    fill worst  9.538 m   ->  invisible beyond   6 560 m
    a 25 m ground cell subtends one pixel at    17 200 m

That is the same rule board:2035 already states -- a thing is detailed by what covers a pixel -- so
no new principle is needed, only its arithmetic.

**Second, and this is the correction: the answer is NOT "stamp near tiles and skip far ones".** It
is that EVERY rung gets correct geometry, and a far rung is CHEAPER rather than looser -- the
buildings there are simpler, so there is less tessellation to do, not less correctness to keep. The
formula above sizes the WORK at each rung; it never licenses a wrong one.

**Third: tessellating the ground is what makes SNAPPING possible, which is the real prize.** Once
the ground carries vertices where the world needs them, an OSM footprint's corners can be snapped to
the ground mesh instead of hovering over it or sinking into it. The stamp stops being a special
operation and becomes what the shared vertices already say.

**Fourth, and it is the sentence the whole tree can be held to: GEOMETRY MAY NEVER INTERPENETRATE
GEOMETRY.** Not "buildings", not "roads" -- everything, at every rung. board:2082's goal said it for
infrastructure and this generalises it, which is the right direction: a rule with one exception is a
rule nobody can check.

**Fifth, why it PAYS rather than merely being tidy.** A static frame forgives an intersection; a
moving one does not. Two surfaces that pass through each other flicker under temporal accumulation,
and TAA integrates that flicker into a smear that no sharpening removes. Perfect geometry is
therefore not neatness -- it is what lets the temporal filter do its job, and the cost of getting it
wrong arrives later, in motion, where it is hardest to diagnose.

**Sixth: "perfect" is PROGRAMMATICALLY VERIFIABLE, which is what makes it a rule and not a wish.**
Closed and manifold, consistently wound, no self-intersection, no pair of bodies overlapping in
plan and in height, vertices that meet sharing an index rather than a coordinate. Every one of those
is decidable by a walk over the geometry the engine already holds, and the tree already publishes
part of it -- `--audit-access` counts coincident corners, edges on one triangle, needles and
over-long triangles today.

## What will be true

- [x] `include/Generate.h` carries `Stamp` and the optional `stamps` verb, and no existing
      generator changes -- the default answers nothing
- [ ] **The ground gains vertices where a stamp asks for them.** 90.8 per cent of footprints are
      narrower than a cell, so this comes FIRST and stamping the present grid would make the
      picture worse in nine cases out of ten
- [ ] The ground applies stamps in a DECLARED order -- sorted by a key the declaration fixes, never
      by completion -- and the same scenario stamps the same bytes twice
- [ ] A building is seated on the STAMPED ground, not the raw ground, so the seat and the pad agree
      by construction rather than by luck
- [ ] Roads use the same mechanism: board:1505 wants the terrain moved to meet a road, and a
      corridor's stamp is a long thin one with the same three fields. One mechanism, two callers
- [ ] Negative control: a generator on flat ground returns no stamp, and the terrain digest is
      BIT-IDENTICAL to the run without stamping. A control that moves the ground anyway proves the
      falloff is being applied where nothing asked for it
- [ ] Measurement that shows this is wrong: the gap between a body's sole and the ground beneath it.
      It is what board:2074 measured and it must fall
- [ ] **A walk decides "perfect"** and prints it: closed, manifold, consistently wound, no
      self-intersection, no two bodies overlapping in plan AND height, meeting vertices sharing an
      index. It runs at EVERY rung, because a far rung is simpler and not looser
- [ ] The tessellation each rung needs is DERIVED from `d = h / 1.4544e-3`, the same pixel rule
      board:2035 states, rather than set by hand

## What this does NOT cover

The DEM's own resolution. A ground ring's vertices stand about 25 m apart on the places measured
here and a house is 15 m across, so a pad smaller than a cell cannot be expressed by moving
vertices -- the ground has to gain vertices where a stamp asks for them, or the stamp is a lie at
the scale that matters most. That is the harder half and it is its own item.

It also does not cover EXCAVATION. A building genuinely dug into a hillside is not a plateau, and
board:2074 already names it as the exception the mean does not describe.
