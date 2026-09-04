Type: feature
State: open
Area: scenario, engine, world, generators
Tags: architecture, owner, ai-first
Depends: 2131, 2126

# A scenario RE-DRESSES a real place: Boston becomes the Commonwealth by declaration alone

**Benchmark** -- Unreal: a level is authored in the editor, and World Partition's data layers
switch content in and out; RAGE: the map is authored content and `ymap` layers toggle
variants. **Both are AUTHORED**, and that is the difference this tree is built on: outshine has
no authoring tool for a person. The world is DATA -- the Earth, fetched -- and everything laid
over it is a DECLARATION an AI wrote. Where the references have a level designer, outshine
has a scenario LAYER over a data-driven place. **The choice is mine**, and it is the whole
product: a Fallout is Boston plus a layer that ruins it; a Cyberpunk is a city plus a layer
that lights it.

## Where it stands, measured 2026-09-04

```
  Scenario::Layer                merges rows by id (ScenarioLayer.cpp: kinds, instances, ...)
  OsmField::Declared             a declared vector feature: layer, key, value, width, height,
                                 bridge, tunnel, level, a ring of lat/lon -- ADDS to the fetched
  SurfaceOverride                a material over a surface by name
  Weather, Clock                 declared datums the sky and the medium obey
  removal                        NONE -- a layer cannot say "this building is gone"
  re-dressing by class or region NONE -- a layer cannot say "every roof in this district is rust"
  scale                          untested beyond a handful of declared rows
```

## The solution

A layer is a DELTA over the data-driven world, and the delta has four verbs, each a row a
reader and a writer both spell (board:2131):

| verb | what it says | over |
|---|---|---|
| **add** | `OsmField::Declared` as it stands: a structure, a way, a water body, a placement | a place |
| **remove** | a declared feature id or a region is GONE from the fetched data | an id, a ring |
| **replace** | a fetched feature keeps its outline and takes the layer's tags -- height, material, ruin | an id, a class, a ring |
| **dress** | a material, a vegetation table, a class palette, a weather, a clock | a class, a region, the world |

The generators already take their inputs from fields (`FeatureField`, `GroundTable`,
`VegetationTemplates`); the delta is applied to the FIELDS after the fetch and before any
generator runs, so no generator knows whether a building came from OSM or from the layer.
That is the same seam board:2110 drew -- the engine reads, the layer edits, the generator
expands -- with the edit added between the first two.

## What will be true

- [ ] The four verbs stand in the grammar, read and written back, each with a case
- [ ] A place carries a layer that removes a district, replaces a class's material and adds a
      declared structure, and the picture shows all three, looked at
- [ ] The layer scales: a delta of ten thousand rows over CentralPark stands inside the
      preload budget (board:2092) and costs its rows, not the world
- [ ] A generator cannot tell a layered feature from a fetched one: a case swaps the source
      and the digest holds
- [ ] Negative control: remove the delta pass and the removed district comes back

## What will show I was wrong

If a re-dressed place needs a generator to know it was re-dressed -- a ruin generator that
reads "ruined" -- then the delta is not a data edit, it is a new subject, and it gets its own
generator area rather than a flag inside another's.
