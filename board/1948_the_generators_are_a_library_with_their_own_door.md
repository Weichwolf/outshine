Type: feature
State: active
Parent: 1953
Progress: streaming
Area: generators, door
Tags: benchmark, target, owner

# The generators are a library with their own door, a registry, and a representation on the way out

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
      proof: harness/outshine/door/ScoreWhatAClientsGeneratorMakes

What remains is the SHIPPED half: outshine offers none of its own yet, so today every kind must
come from the client. The four internal generators (`Forest`, `Buildings`, `Water`,
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
      proof: harness/outshine/geo/ScoreWhatAGeneratorHandsBack
- [ ] `DrawSink` is deleted or implemented -- it is an interface with no implementation and
      `ClusterId` reaches two files, so the instanced-draw half of the tier is a declaration with
      nothing behind it.
- [ ] A GENERATOR calls it: the meshers need a plan, and a plan needs the snapshot.
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
