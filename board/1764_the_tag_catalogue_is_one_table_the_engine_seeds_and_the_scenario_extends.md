Type: issue
State: open
Parent: 1583
Area: core, scene
Tags: component-model, tags, declarative

# The tag catalogue is one table the engine seeds and the scenario extends

**Benchmark** — Unreal: `FGameplayTag` — one registry, seeded by the engine, extended by the project through data. RAGE: hard-coded flags. **Taking Unreal** — a scenario must be able to say a word the engine has never heard, and a typo in it must still be caught.

`TagCatalogue` (include/Scene.h:34) freezes the whole tag vocabulary in a header,
and the encoding bounds it: a `Tag` is one byte per level in a `uint32_t`, so four levels deep
and 255 siblings per node — both overflow SILENTLY, a full byte carrying into the parent so a
child becomes its own uncle. GAS ships five levels routinely.

The declarative defect is the larger one: content-side vocabulary (`Faction.Raider`,
`Material.Concrete`, `Damage.Fire`) numbers 10^3 to 10^4 in a world of this size and changes
with every scenario. Frozen in a header, a new world is an engine recompile — the direct
negation of *content = data, engine = verbs*.

## What will be true

- [ ] The engine SEEDS the catalogue and a scenario EXTENDS it; the engine's own tags stay a
      `constexpr` catalogue so a misspelling in C++ is a compile error.
- [ ] Depth and breadth are bounded by construction and an overflow REFUSES by name.
- [ ] A tag a scenario did not declare refuses at load, naming what the catalogue carries.
