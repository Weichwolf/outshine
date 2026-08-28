Type: feature
State: open
Parent: 1953
Progress: streaming
Area: generators, door
Tags: benchmark, target, owner

# The generators are a library with their own door, a registry, and a representation on the way out

**Benchmark** — Unreal: PCG is a plugin with its own registry, outside the engine module. RAGE: none. **Taking Unreal** — a generator library that links without the engine is the only shape that lets another project take it.

## What is measured, and it is worse than "incomplete"

**There are TWO generator doors and the tree reaches one of them.**

| door | what it hands back | implementers | consumers outside `src/generators/` |
|---|---|---|---|
| `Generates` (`include/Generate.h`) | a `Geometry` | **1** -- `Structures` | the engine's registry |
| `Generators::Making` | `Body` PLACEMENTS over a `Ground` | **4** -- Forest, Buildings, Water, Infrastructure | **none** |

So the four generators the owner's target names by name -- forest, buildings, water,
infrastructure -- implement a door nothing consumes, and the door the engine consults holds one
generator that is none of them. `Engine::Offers(const Generates &)` is called by four TEST cases
and by `apps/viewer`, and by no shipped generator at all. 5887 lines under `src/generators/`.

That is CLAUDE.md's own warning made literal: *"counting class names would have scored the world
generators complete while 6528 lines sat in an archive no declaration reached."*

**TWO DOORS IS THE RIGHT ANSWER, and the benchmark says so** -- this is the row the item was
missing. Unreal's PCG graph outputs POINT DATA, and separate spawners turn those points into
static-mesh instances; the point and the mesh are different representations on purpose, because a
forest is not one mesh and instancing dies the moment you flatten it into one. RAGE has no PCG to
compare. **Taking Unreal**: `Generates` hands back a `Geometry` -- a PART -- and `Making` hands
back `Body` placements, which is a point with a radius, a height, a mass, a yaw and a contact
material. `sizeof(Body) == 48` and it is pointer-free, so it is already the value a foreign caller
can hold that part 3 demands.

So the defect is NOT that there are two doors. It is that the second has no consumer, and the
chain it needs already exists end to end:

    World.Stack (heights) + OSM fields
      -> Generators::SnapshotOver  -> Ground::Snapshot
      -> Ground::Of                -> Ground
      -> Making::Occupy            -> Yield -> Body placements
      -> the scene's placements

Every arrow in that chain is written. **Now the first one is walked**:
`outshine/geo/ScoreWhatAPlacementGeneratorYields` builds a `Ground`, leases a sink from a
`RegionPool`, and runs `Forest::Occupy` -- in a suite that links `src/generators` and nothing of
`src/engine`, which is part 3 of this item in miniature. What remains is the ENGINE calling it.

**And a third door under them is dead too.** `DrawSink` (`src/generators/draw/DrawSink.h`) has
ZERO implementers, so `DrawSet::Draw` -- the entry to the 29-file `draw/` subtree -- can never be
called with a sink to write into. Nothing outside `src/generators/draw/` names either.

**A name hid the first row.** `EngineHeld::World.Making` was a `std::vector<const Generates *>`,
one word away from `Generators::Making`, the placement interface it has nothing to do with. It is
`World.Offering` now. A reader who greps `Making` was previously handed the registry and the
interface in one list and could not tell that the four generators reach neither.

Owner's target, three parts, and the third is the one that binds:

1. outshine SHIPS a generator registry -- forest, buildings, water, infrastructure.
2. A CLIENT adds its own to it.
3. **Another project uses the generators alone, without the engine.**

Part 3 is the constraint the other two follow from. A tier that must stand up in a foreign program
cannot name the renderer, the scenario, the sim or the engine; its input must be a value a foreign
caller can fill; and its OUTPUT must be a value a foreign caller can hold.

**THE OUTPUT IS THE REPRESENTATION, NOT A FILE.** A generator does not serialise. Serialising
would force a round trip nobody asked for -- mesh, glTF bytes, parse, mesh -- on the one path
where a round trip is least affordable, and the compositor consumes the representation directly.
So the representation itself is the public value, and a glTF SERIALISER ships beside it as an
optional consumer of that same value: for tooling, for a cache, for inspection, and for a foreign
project that wants a file rather than a buffer. Owner's correction, and it is the better shape.

## What the tree has today, measured

**The tier holds THREE mesh shapes and none of them was the value.** Measured:

- `BuildingMesh::Mesh(plan, soup)` fills a `std::vector<float>` at EIGHT floats a vertex --
  three position, two UV, three normal, the same order `ChunkVtx` uses
- `TreeMesh` holds `BarkVerts` and `LeafVerts` with their own indices
- `DrawSink` is the interface that was to carry them and **NOBODY IMPLEMENTS IT**:
  `grep -rln 'public DrawSink'` over `src/` returns nothing, and `ClusterId` appears in two
  files. The draw half of the tier has never run.

`Generators::Meshed` is the crossing, as of this session: it de-interleaves a soup into
`outshine::Geometry`, refuses a soup that is not whole triangles, and carries several parts as
reaches into one vertex array. What is still missing is a generator CALLING it -- the meshers
need a plan, and a plan needs the snapshot board:1805 now composes.

**THE DECLARATION IS BOUND NOW, AND HALF THE ITEM WITH IT.** `include/Generate.h` declares
`Generates` -- a `Kind()` and a `Make(Ask, Geometry &)` -- and `Engine::Offers(const Generates &)`
registers one. `Declare` resolves every `Scenario::Generators[].Kind` against what has been
offered: an unknown kind is REFUSED BY NAME at declaration, and an offered one runs and what it
makes stands in the picture.
      proof: outshine/door/ScoreWhatAClientsGeneratorMakes

**AND A SCENARIO'S ASSET MAY NAME A GENERATOR RATHER THAN A FILE**: `<asset kind="generated"
uri="test-slab"/>` stands what the maker makes. That is how a scenario USES the geometry a client
builds, without a pointer in the declaration -- a pointer cannot be written to XML and read back,
and a name can. One resolution serves both places a name can appear, and both refuse by that name.

**THE SHIPPED HALF LANDED.** `Generators::Structures` offers itself under `structures`, and
`Engine::Declare` registers outshine's own makers before it resolves a declaration -- so a scenario
naming `structures` stands with nothing offered by the client. It builds a footprint, hands it to
`BuildingMesh`, feeds the soup through `Meshed` and hands back a `Geometry` with its material.

Stranded sources fell from 17 to 6: binding one shipped generator to the registry linked
`BuildingMesh`, `BuildingShape` and the rest of `generators/draw/`, which had been complete and
reachable by nobody.

What remains of the shipped half: The four internal generators (`Forest`, `Buildings`, `Water`,
`Infrastructure`) implement a DIFFERENT interface -- `Occupy`/`Proposes`/`At`, which scatters
BODIES over ground rather than making geometry -- and the mesh makers under
`src/world/generators/draw/` are all stranded, reached by no suite. Those two facts are the same
fact: nothing binds them to a declaration, so nothing links them.

**AND THE DECLARATION FOR IT ALREADY STANDS, UNREACHED.** `include/Scenario.h` carries

    struct Generator { std::string Kind; std::vector<Setting> Parameters; };
    std::vector<Generator> Generators;

`ScenarioRead` parses it, `ScenarioLayer` merges it across layers, `Engine` COUNTS it -- and
nothing resolves `Kind` to anything that runs. A declaration read, merged and counted, then
dropped: the reachability defect this tree keeps producing, filed here where it belongs.

The shape is already right and must not be replaced by a pointer. **A scenario is a value that is
written to XML and read back, and a pointer does not survive that; a NAME does.** Unreal
references an asset by object path resolved through the asset registry, RAGE by name or hash
resolved through streaming, and neither puts a raw pointer in a map. So a client's own generator
REGISTERS under a name and the declaration names it -- the client-side capability the owner asked
for, without the field that would make the declaration unserialisable.

**There is no registry a client can reach.** `GeneratorSet::Add(rank, generator)` is the registry
and it is an `src/` type behind `Clients::Sim`, which one file includes.

**But the input is already a value, and that half is nearly done.** `Generators::Ground::Snapshot`
is four shared pointers -- `GroundPatch`, `ClassStructure`, `FeatureField`, `GroundTable` -- and
three of the four have public factories that take plain arrays: `GroundPatch::Complete(region,
side, postings)`, `GroundTable::Of(rows)`, and `FeatureField` from features, rings and vertices.
`SnapshotOver` composes them outside the engine as of this session, proven by a case in a suite
that links no `src/engine` source.

## What will be true

- [x] The tier LINKS without the engine, and `make` writes `build/libgenerators.a` from the
      closure the linker itself computes -- 53 objects, 44 `world` and 9 `base`, no member of
      `engine`, `render`, `scenario`, `sim`, `ui`, `audio` or `host`. A program links that archive
      alone and runs.
      proof: harness/claims/TheGeneratorsLinkWithoutTheEngine
- [ ] A second public header declares the generator library: the input value, the `Generator`
      interface, the registry, and the OUTPUT REPRESENTATION as a value.
- [ ] That representation is pointer-free and one-width, so a foreign caller can hold it, copy it
      and outlive the generator that made it -- CLAUDE.md's layout rule is what makes it usable
      across a library boundary at all, not only what makes it fast.
- [ ] A glTF SERIALISER ships beside the library, taking that representation and writing a
      document. Nothing on the streaming path calls it: the compositor takes the representation,
      and the serialiser is for a caller who wants a file.
- [x] A generator's mesh crosses into the door's value: `Generators::Meshed` de-interleaves an
      eight-float soup into `outshine::Geometry`, field by field and part by part.
      proof: outshine/geo/ScoreWhatAGeneratorHandsBack
- [ ] `DrawSink` is deleted or implemented -- it is an interface with no implementation and
      `ClusterId` reaches two files, so the instanced-draw half of the tier is a declaration with
      nothing behind it.
- [x] the placement door is REACHED, without the engine behind it.
      `outshine/geo/ScoreWhatAPlacementGeneratorYields` walks ground query -> snapshot ->
      `Ground::Of` -> `RegionPool` lease -> `Yield` -> `Forest::Occupy`, and reads 191 bodies at
      4.0e-4 stems/m2 against 0 at zero density, into a sink of 4096 it does not saturate.
      `Ground::Of` had no caller before it; nor did `Occupy` outside `GeneratorSet`.
      negative control: an empty `Forest::Occupy` body makes it read 0 and the case goes RED.
- [x] **the world's vector data has an OWNER**, which was the blocker. `GroundStack` holds the
      shipped `GroundMaterials` and `VegetationTemplates`, an `OsmField` over all five layers, and
      the three derived fields; `Open` loads the tables and hands them to the class field, and
      `Restand` builds the vectors where the camera stands and ingests them. Before this,
      `BuildingField::Build`, `WaterField::Ingest` and `StreetField::Ingest` had NO caller
      anywhere in the tree and the three fields were never populated by anything.
      proof: outshine/door/ScoreWhatAMovingSceneResends reads `1813 street(s), 3 water
      surface(s), 1849 footprint(s)` over the drive; gate GREEN.
      negative control: dropping the street ingest makes it read 0 and the case goes RED.
- [x] the engine SHIPS a placement generator and CALLS it. `Surrounds` carries a placement
      registry beside its `Generates` one, and the forest in it is built from DECLARATION and not
      from code -- species heights from `src/assets/world/species/`, per-template tree density
      from `vegetation.json`'s `trees.perM2`, and the treeline from its `alpineLimit`.
      `Composes` builds a region, a snapshot, a `Ground`, a `RegionPool` lease and a `Yield`, and
      runs `Occupy` over every registered maker.
- [ ] **ONE REGION CANNOT SERVE TWO ZOOMS, and that is the last blocker, measured.** The chain
      above reaches step 40: registry, table and vector fields all stand, and `SnapshotOver`
      returns with no patch, no classes and no features. `SnapshotOver` takes a SINGLE `Tile`, and
      this tree serves ground blocks at zoom 12 (`GroundSurface.Z`) while the vector provider's
      finest is 14 (`VersatilesVector.cpp:17`, against the DEM's 15). A region at the vector zoom
      finds no resident ground block; a region at the ground zoom finds no settled vector tile.
      **Proven by moving it, both ways**: at the vector zoom the patch is MISSING and the vectors
      read 1813 / 3 / 1849; at the ground zoom the patch STANDS -- step 41 -- and the vectors read
      0 / 0 / 0. The line responsible is `GroundStream::BlockAt`'s first:
      `if (z != Surface_.Z) return block;`. And it is not a fetch that has not landed: `Grows`
      runs every frame and the number never moves.
      So the remaining question is about the SNAPSHOT's signature and not about the generators:
      either it takes two regions, or the ground stream serves more than its own zoom.
      The old blocker, kept for the record:
      `SnapshotOver` returns `Taken` only when the patch, the classes AND the features all stand,
      and `FeaturesOver` returns null unless all FOUR vector fields are handed in -- an
      `OsmField`, a `BuildingField`, a `WaterField` and a `StreetField`.
      `Surrounds` (`src/engine/EngineHeld.h:241`) holds a height stack, `Structures` and the
      `Generates` registry, and **no vector field at all**. The only `OsmField` this tree ever
      constructs is a LOCAL VARIABLE in `Sim::DriveAssembly` (`src/sim/DriveAssembly.cpp:107`):
      it is built over the corridor, it lays the road graph, and it is destroyed when the call
      returns. It carries Streets and StreetPolygons only -- no buildings, no water.
      So the arrow is not a call that is missing; it is a HOLDER. The world's vector fields have
      no owner with a lifetime longer than one road-graph build, and until one exists a placement
      generator in the engine reads the same unmapped ground
      `outshine/geo/ScoreWhatAPlacementGeneratorYields` hands in by construction.
- [ ] The registry holds what outshine ships AND what a client registered. The shipped catalogue
      stays closed against a typo; a client's generator enters as a VALUE with a handle, never a
      string. This is the reconciliation with CLAUDE.md's *"the consumer selects from a
      `constexpr` catalogue and cannot add to it"*, and it is Unreal's own shape: built-in
      factories enumerated, plugin factories registered.
- [ ] The shipped generators are CLIENTS of that door -- they use nothing a third party could
      not. A client that compiles against the header proves more than any suite, and the shipped
      ones are the first such client.
- [ ] Proving case: a program that links the generator objects and NOTHING of `src/engine`,
      `src/render`, `src/scenario` or `src/sim`, fills the input value by hand, registers a
      generator of its own beside a shipped one, and reads the REPRESENTATION back with both
      contributions in it -- then, separately, serialises it to a glTF document the tree's own
      reader accepts. Negative control: the same program with the engine's objects removed
      from the link line today, and it does not link.
