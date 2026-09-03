# A generator expands, and the engine holds what was measured

State: active

## The order the move goes in

Each step builds, `make shots` holds its eight digests, and nothing else is touched in between --
a rename that lands beside an unrelated edit is a rename nobody can bisect.

  1. `flora/`    -- 25 files, the widest spread, so it proves the shape or breaks it first
  2. `building/` -- 13 files
  3. `road/`, `water/` -- the rest of what OSM feeds
  4. `terrain/`, `scatter/`, `cloud/`
  5. a `reaches` per area, which is where the split stops being a convention

`base/` is emptied INTO the areas rather than deleted: what one subject uses moves to it, and what
survives in `base/` is by definition what all of them share. That is the test for the file, applied
one file at a time, rather than a judgement made up front.

`src/generators/` is cut by PROCESSING STAGE -- `base/`, `path/`, `draw/` -- which runs across
every subject at right angles. Measured on 2026-09-03: the tree is 25 files across three
directories, the building 13. Anyone changing how a tree grows opens three of them, and nothing
holds a file in its place, because there is exactly ONE `reaches` file for all of the generators.
The subdirectories carry no meaning in the dependency system. They are sorting, along the axis
that matters least.

## Where the line runs

Not by height above the ground. By a question that can be answered:

> Can the result be checked against a truth OUTSIDE this tree?
> Yes -> the engine. No -> a generator.

  the height of a point       against SRTM               engine
  the outline of a house      against OSM                engine reads it
  that house's HEIGHT, untagged   against nothing        GENERATOR
  the sun at 14:07            against ephemerides        engine
  the shape of a cloud        against nothing            GENERATOR

The same line stated mechanically: a generator EXPANDS. A loader is linear -- N bytes in, N bytes
out. A generator turns eight outline points into four hundred triangles and a seed into twenty
thousand. Where the output stops tracking the input, a generator begins.

## What this changes against RAGE, and it changes everything

For RAGE a generator is a tool IN THE STUDIO. SLOD1..4, collision, lighting -- baked offline, the
map is content, and the run time only picks. outshine cannot do that: the world arrives over the
wire and it is the whole Earth. **What RAGE bakes offline, outshine must do during preload.**

That is why the one-second preload is a bound RAGE never had, and why "baked, not built at frame
time" means something different here: the fine geometry and the proxy are produced in the same
meshing pass, and the frame only chooses. It is also why the generators are a tier with their own
door in this tree and are not one in RAGE.

## What the engine holds

The Earth (georeference, elevation, the tile pyramid), the sky (sun, moon, stars, scattering --
astronomy has exactly one right answer), the weather as a DATUM, the OSM vectors as read, IO, the
scene, the renderer, the physics, the audio, and the declaration reader.

## The generators

One directory per subject, and the subjects are cut so each has exactly ONE expansion mechanic. A
subject needing two mechanics is two subjects.

| directory | in | mechanic | out |
|---|---|---|---|
| `terrain/` | height field + classification | field -> surface | ground mesh with a material index |
| `road/` | OSM lines | curve -> ribbon | roads, bridges, rails |
| `building/` | OSM areas | polygon -> volume | houses, roofs, facades |
| `water/` | OSM areas and lines | polygon -> surface | lakes, rivers, coast |
| `flora/` | class + climate | seed -> skeleton -> foliage | trees, shrubs |
| `scatter/` | class + ground | area -> instances | grass, stones, clutter |
| `cloud/` | weather | noise -> volume | clouds |

`flora/` and `scatter/` both scatter and stay apart: a tree is GROWN and a grass tuft is only
PLACED. Different expansion depth, shared instancing library. `base/` survives narrow -- what all
seven genuinely share. `path/` survives as a cross-cut: fitting a curve is one problem for a road
and a river.

Not `ground/` for the surface: `src/world/ground/` holds the elevation, and a name meaning two
things in one tree is the collision this session walked into twice in one afternoon. Not `cover/`:
`base/Cover.h` is a type. Not `osm/`: that names the SOURCE rather than the result, and a second
vector provider would make it a lie. Fauna gets its own area when it arrives, because it MOVES --
simulation, not generation.

## What follows from the split

**A generator is the most dangerous place in the tree for determinism, because it is the only part
that rolls dice.** A seed taken from a counter depends on the order tiles arrive in, and that order
is not declared. So every generator draws its seed from the PLACE and never from a sequence.
`PlaceHash(LongitudeLatitude)` already does this for storey counts; where a generator does not, it
is a finding.

**And the gain that is not tidiness:** a directory here IS a dependency tier with its own
`reaches`. Make the areas real and each gets one, and `flora/` reaching into `built/` then fails at
the `#include` with a file and a line. The split stops depending on discipline.

**The measurement that shows I was wrong:** count the cross-area includes after the move. NO
generator may reach into another -- they meet at the door and nowhere else -- so the number that
matters is zero, and anything above it is a finding rather than a tolerance.

## What the move cost, measured on flora

Nothing, and the reason is worth writing down: `test/run.sh` derives every include path from
`find src/<tier> -type d`, and not one `#include` in the tree carries a directory prefix. A new
directory under `src/generators/` gets its `-I` for free and every existing include keeps
resolving. The 27 files moved and the build was green on the first try. The guards were renamed to
spell their new folder, which `EveryGuardSpellsItsFolder` requires.

  out of flora/  4 includes, ALL of them shared machinery -- `Making.h`, `ModelLadder.h`,
                 `ClusterId.h`, `DrawSource.h`. Not one names another subject
  into flora/    3 includes, ALL of them from `Shipped`, which is the registry that stands the
                 generators up and is entitled to know each of them

So the cut holds where it has been tested: flora touches no other subject, and no other subject
touches flora. `ClusterId.h` and `DrawSource.h` sitting in `draw/` is the next thing to fix --
`draw/` holds subject code AND shared machinery, which is why it looked load-bearing.

## What the tiers cannot do yet

`LayerReaches` refuses any name with a `/` in it, so a TIER is exactly one level under `src/`. An
area inside `generators/` cannot carry its own `reaches` today, and the include path for every
generator file is all of `src/generators`'s subdirectories at once. The split is therefore a
convention until `LayerReaches` learns nested tiers -- which is the step that turns "generators do
not reach into each other" from a rule into something the compiler refuses.

`Shipping` holds `std::vector<Forest::Stem>`: the registry knows one generator's INSIDES rather
than knowing it through `Making` and `DrawSource`. That is the same rule pointed at the door
itself, and it is where this item goes after the seven directories exist.
