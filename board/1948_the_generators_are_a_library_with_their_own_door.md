Type: feature
State: open
Progress: streaming
Area: generators, door
Tags: benchmark, target, owner

# The generators are a library with their own door, a registry, and glTF on the way out

Owner's target, three parts, and the third is the one that binds:

1. outshine SHIPS a generator registry -- forest, buildings, water, infrastructure.
2. A CLIENT adds its own to it.
3. **Another project uses the generators alone, without the engine.**

Part 3 is the constraint the other two follow from. A tier that must stand up in a foreign
program cannot name the renderer, the scenario, the sim or the engine; its input must be a value
a foreign caller can fill; and its output must be a format a foreign caller already has a reader
for. That format is glTF, which CLAUDE.md already names as the tree's only content surface.

## What the tree has today, measured

**No generator produces glTF.** `Generator::Occupy` places BODIES -- `Generators::Body` carries
`Em`, `Nm`, `BaseAslM`, `RadiusM`, `HeightM`, `MassKg`, `YawRad`, `ContactMaterial` -- and a
separate `DrawSource::Draw(ground, placed, sink)` meshes them into a `DrawSink`, which is a
private sink in `src/world/generators/draw/`. Nothing between a generator and a glTF document
exists.

**There is no registry a client can reach.** `GeneratorSet::Add(rank, generator)` is the registry
and it is an `src/` type behind `Clients::Sim`, which one file includes.

**But the input is already a value, and that half is nearly done.** `Generators::Ground::Snapshot`
is four shared pointers -- `GroundPatch`, `ClassStructure`, `FeatureField`, `GroundTable` -- and
three of the four have public factories that take plain arrays: `GroundPatch::Complete(region,
side, postings)`, `GroundTable::Of(rows)`, and `FeatureField` from features, rings and vertices.
`SnapshotOver` composes them outside the engine as of this session, proven by a case in a suite
that links no `src/engine` source.

## What will be true

- [ ] A second public header declares the generator library: the input value, the `Generator`
      interface, the registry and the glTF output. It names no type from `src/render`,
      `src/scenario`, `src/sim` or `src/engine`.
- [ ] A generator's output is a glTF document, so a foreign caller needs no outshine reader to
      use it and the engine consumes it through the same path as any declared asset -- content
      store, hash for a name, handle for a reference.
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
      generator of its own beside a shipped one, and reads a glTF document back with both
      contributions in it. Negative control: the same program with the engine's objects removed
      from the link line today, and it does not link.
