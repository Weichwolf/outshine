# A generator expands, and the engine holds what was measured

State: open

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

**The measurement that shows I was wrong:** count the cross-area includes after the move. If more
than a couple of files need to reach across, the subjects are not as separable as this claims and
the stage-cut was carrying real weight.
