Type: feature
State: open
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

## What will be true

- [ ] `include/Generate.h` carries `Stamp` and the optional `stamps` verb, and no existing
      generator changes
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

## What this does NOT cover

The DEM's own resolution. A ground ring's vertices stand about 25 m apart on the places measured
here and a house is 15 m across, so a pad smaller than a cell cannot be expressed by moving
vertices -- the ground has to gain vertices where a stamp asks for them, or the stamp is a lie at
the scale that matters most. That is the harder half and it is its own item.

It also does not cover EXCAVATION. A building genuinely dug into a hillside is not a plateau, and
board:2074 already names it as the exception the mean does not describe.
