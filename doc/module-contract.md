# The Outshine module contract — state in, geometry out

> Owner, 2026-08-05: *„jedes Outshine-Core-Modul erfüllt denselben festen Vertrag … ich möchte schon vom
> Directory-Tree und den Dateinamen die komplette Struktur lesen können und neue Funktionen/Module müssen
> vollkommen eindeutig zu integrieren sein."* · *„letztendlich haben wir Weltzustand und Geometrie, also
> müsste sich eine passende Abstraktionsebene finden."*

Spec-first. §4's vocabulary and §5's gate are built; the migration is not.

## Spec

### 0. The contract

> **A module owns a piece of world state and derives geometry from it.**

Two obligations. Everything else is declared, not implemented.

| | State | Geometry |
|---|---|---|
| sky, clouds | weather, time of day | a participating medium, no surfaces |
| foliage, clutter | which instances exist where | meshes |
| OSM buildings | which buildings, in what condition | meshes **and** collision |
| terrain | the elevation field | a surface |

**Not a method set.** Forcing N methods on these produces one of two failures: a contract so wide it says
nothing, or one so narrow that half the modules reach around it — the usual end of „everything is a
plugin". Today's measurement makes the point: `FBModule` went from **28 pure virtuals to 1**, and the
result was more capable, not less. Whoever inherits declares what it can, from a closed vocabulary.

### 1. Why this is the right level: it is the third sighting of the same cut

| Decision | State side | Geometry side |
|---|---|---|
| [`client-server.md`](client-server.md) | the server holds it | the client receives a view |
| [`render/gpu-determinism.md`](render/gpu-determinism.md) | identity is **integer** | position and appearance are **float** |
| this file | a module owns it | a module emits it |

Three separate arguments, arrived at independently, landing on one line. That is the evidence that it is
the abstraction rather than an abstraction.

Two properties then follow without being demanded:

- **State is authoritative, deterministic, persistent. Geometry is derived, float, disposable.** The WGSL
  finding lands exactly on this seam: what decides *existence* must be integer, what decides *appearance*
  need not be.
- **Collision is not a third thing.** It is geometry consumed by physics instead of by the renderer. A
  module emits a shape; the consumer decides what to read it as.

### 2. Reading the structure from the tree

The tree already half does this — `core/ fdm/ units/ sensors/ weapons/ systems/ pilot/ modules/
missions/` with an enforced layer order, `doc/` mirroring it, `verify-layers` measuring it. Extend rather
than invent.

**Make it a gate, not an intention.** `verify-trees` counts directories today; it can equally count
whether every module directory carries its fixed parts. Then „read the structure from the tree" is
checkable, and integrating a new module is unambiguous because the tool says what is missing.

### 3. One caution, measured today

`core/FBAircraft.h` was a „generic" file that knew eighteen aircraft types — **148 mentions**, in the
layer everything reads. It was modular by the code's shape, not the domain's.

**Modularity that mirrors the domain is good; modularity that mirrors the code produces that file.** The
test is the same epistemic one as `CLAUDE.md` principle 3: does this module need to know something no
participant in its domain would know?

### 4. The closed vocabulary — eight words, two sides

`sim/src/modules/FBContribution.h`, the shape of `FBCapability.h`: one table, four expansions,
include-free, runtime-readable. **The gate reads that table, so no second copy of the list exists.**

**The closure rule: a word earns its row by having a reader that no other row's reader can be.** Two
words that always occur together are one word; a word one module needs is not a word.

**Geometry — three, and the distinction is how a consumer READS them:**

| Word | Read as | Key | Carries |
|---|---|---|---|
| `surface` | point → a boundary. The renderer tessellates it, physics stands on it, a sensor is blocked by it | point | terrain, water, floors |
| `volume` | point → density or velocity, **no boundary**. The renderer integrates it, the FDM flies in it, a sensor sees through it | point | cloud, fog, smoke, wind |
| `instances` | region → `(birth address, transform, asset)`, **enumerated**, never a stored list | address | foliage, buildings, furniture — and every unit body |

**State — five, and they are the ROW KINDS OF A WORLD SNAPSHOT.** That is what closes the set:
[`persistent-world.md`](persistent-world.md) §2 makes the snapshot the completeness test, so a state
class that owes the save file no row of its own is not a state class.

| Word | Writer | Snapshot row | Carries |
|---|---|---|---|
| `dataset` | an offline baker | a **content hash** | DEM, OSM, landcover, catalogue, an Ocean snapshot |
| `generated` | the generator, from (address, dataset, ambient) | generator **name + version + seed** | foliage, clutter, interiors, terrain detail |
| `delta` | first touch, on the CPU (GD6) | the **touched things**, keyed by address | a moved chair, a destroyed building |
| `simulated` | the module's own `Run()` | the **projection** taken on leaving | an F-16, a missile, a SAM site |
| `ambient` | the mission clock | the **scalars** | time of day, weather, epoch, decay |

**Four keys, and the column IS `GD1`.** Every `address` is integer and *enumerated* — a generator's loop
variable, a mission line, a `(tile, cell, slot)` — never a computed position
([`render/gpu-determinism.md`](render/gpu-determinism.md) GD2). Every `point` is float and carries no
identity, which is why no state word may use it. `content` names a pinned artefact, `global` names the
one there is.

**The two obligations are one predicate.** `FBContributionsSatisfyContract(mask)`: at least one state
word and at least one geometry word. Two further invariants are `static_assert`s: a row's snapshot is
`None` **iff** it is geometry (geometry is disposable), and no state word is keyed by a float.

#### The five test cases

| | State | Geometry |
|---|---|---|
| sky, clouds | `ambient` | `volume` |
| foliage, clutter | `generated` `delta` | `instances` |
| OSM buildings | `dataset` `delta` | `instances` |
| terrain | `dataset` `generated` | `surface` |
| **interiors, on entry** | `generated` `delta` | `instances` `surface` |

Interiors need no word and no trigger: **the birth address nests.** An instance's address is the domain
of what is generated inside it, so a chair in a room in a building is `(tile, cell, slot)/(cell, slot)`
— enumerated at every level, integer at every level, and addressable in a save before the room exists.

#### Two mergers, and they are the load-bearing part

| Merged away | Into | Why |
|---|---|---|
| **`collision`** | `surface` / `instances` | it is the same shape read by physics instead of by the renderer. A separate word would be two truths about one shape — the failure [`missions/verdict.md`](missions/verdict.md) exists to forbid |
| **`body`** (a unit's mesh) | `instances` backed by `simulated` | everything that looked different about a jet — it moves per tick, it has an identity — lives on the **state** side. On the geometry side a jet and a tree are one placed asset |

The `body` merger settles an open question in [`render/gpu-determinism.md`](render/gpu-determinism.md)
Gaps (*"a chair a scenario declares has no birth address"*): **a birth address is the enumerated input
that produced the thing**, and a mission line is as enumerated as a generator's loop variable. One
instance path, two sources of address.

#### Scale — the test is a city, not four campaigns

| AAA demand | What carries it |
|---|---|
| **millions** of instances | `instances` is defined as *enumerate over a region*. A list would have broken here; an enumeration does not care |
| **interiors on entry** | the nested address above — a generation problem, not a storage one |
| **populations with dependencies** | `dataset` (Ocean bakes it) whose *observed* members become `simulated`. Deliberately **not** a word: a runtime "population" channel is the omniscience [`persistent-world.md`](persistent-world.md) §5 forbids |
| **epoch, decay** | `ambient` — one writer, so the honest triple (built epoch, observed epoch, maintenance) cannot contradict itself |
| **a save after hundreds of hours** | `delta`, keyed by address: size tracks *distinct things touched*, not hours |

### 5. The fixed parts of a module directory

`verify-trees` gained a third scope. A module directory is `sim/src/modules/<id>/`, and it carries:

| Part | Path | Why it is fixed |
|---|---|---|
| the class | `src/modules/<id>/FB*Module.h` | the `FBModule` subclass |
| the registration | `src/modules/<id>/FB*ModuleRegistration.cpp`, **exactly one** | the single place the registry key is bound |
| the intent | `doc/modules/<id>/module.md` | the contract statement |
| **the declaration** | a `**Contributes:** …` line in it, words from §4 | machine-checked against `FBContribution.h`, one state word and one geometry word |

Only the engine tree has module directories: a mod ships no `.cpp` by rule
([`mods.md`](mods.md) §2.1), so it cannot own one.

**Every orphan names its remedy**, in all three scopes — the reader is a machine as often as a person.
An unknown word prints the legal list; a one-sided declaration prints the words of the side that is
missing; a missing part prints the path to create.

## State

| Piece | State |
|---|---|
| `sim/src/modules/FBContribution.h` — the eight-word table, two `static_assert`ed invariants, the contract as a predicate | **built.** Included by `FBModule.h` so the assertions compile in every build; **no binder yet** — nothing calls `Declare…` because no world module exists |
| `verify-trees` module scope | **built.** 6 module(s), 8-word vocabulary read from the header, 4 parts each, remedies on every orphan in all three scopes |
| the declarations | **4 of 6** — `air` `f16` `ground` `mig29` declare `simulated` `instances`; `missile` and `stores` have no `doc/modules/<id>/module.md` to declare in |

**Measured, and it is the honest limit: all four declarations are the same pair,** because all six
engine modules are entity modules. The vocabulary's discriminating power is **unmeasured** until a world
module exists — the first `surface`/`volume`/`generated` declaration is its first real test.

**Orphan counts, before → after:** engine **20 → 20**, mods **3 → 3**, module scope **0 → 2**, total
**23 → 25**. The two are `missile` and `stores`: neither has `doc/modules/<id>/module.md`, so neither
can carry a declaration. Both holes are the *same* holes the engine scope already reports one level up
(`modules/missile` `doc=-`; `modules/stores` `doc=leaf`) — reported twice because the remedies differ:
create the directory, versus write the declaration in the file.

**Proof that a new module is unambiguous:** a synthetic `sky/` with the two source files and a
`**Contributes:** ambient volume` line takes the module scope to **0 orphans**; removing either source
file names it by path, an unknown word names the legal list, and a one-sided declaration names the side.

## Gaps

| Gap | Detail |
|---|---|
| **Nothing binds the vocabulary in C++** | `FBContribution.h` compiles and is read by the gate; no module calls a `Declare…`. The binder is the migration round, and it is the point at which `doc/` and code could disagree — a cross-check of the `module.md` line against the runtime mask is the gate that closes it |
| **The declaration is uniform today** | see State. Four modules, one pair. Not a defect, but it means the vocabulary is *reasoned*, not yet *measured* |
| **`missile` and `stores` cannot declare** | no `doc/modules/<id>/module.md`. `stores` needs `doc/modules/stores.md` split into a directory; `missile` needs one written |
| **No module owns its state explicitly** | state is spread across `core/`, and who may write it is a compile-time argument rather than a structural one. §4 names the five state classes; nothing sorts today's state into them |
| **The engine scope still has 20 orphans** | unchanged by this round and untouched on purpose: eight `LEAF` splits and nine `MISSING` trees are content work, not structure work |
| **Rejected: `collision` as a word** | §4. It is `surface`/`instances` read by physics — a separate word makes one shape two truths |
| **Rejected: `body` as a word** | §4. It is `instances` backed by `simulated`; the whole difference is on the state side |
| **Rejected: one `field` word covering `surface` and `volume`** | physics can stand on a boundary and cannot stand in a medium, so the reader set differs. One word would force every consumer to ask the module which kind it is — §0's *"a contract so wide it says nothing"* |
| **Rejected: subject words (`terrain`, `sky`, `vegetation`, `buildings`, `interiors`)** | they name subjects, not contributions, and the set is not closable: a city adds bridges, canals, powerlines, furniture, each wanting a row. That is §3's `FBAircraft.h` failure exactly — modular by the code's shape rather than the domain's |
| **Rejected: `population` / `agents`** | a population is `dataset` whose *observed* members become `simulated`. A runtime population channel would hand a module knowledge no participant has (principle 3) |
| **Rejected: `lod` / `extent` / `budget`** | how much geometry a region gets is a renderer decision over any of the three words. Declared per module it would break at city scale, where the budget must be global |
| **Rejected: `spawn` / `event` / `trigger`** | the cast comes from the mission declaration and `missions/` owns it; a behaviour channel is a script language with one keyword ([`mods.md`](mods.md) §2.1) |
| **Rejected: `epoch` and `decay` as their own words** | `ambient` values. [`persistent-world.md`](persistent-world.md) §5.1 already derives decay from (built epoch, observed epoch, maintenance); two globals could contradict each other silently |
| **Rejected: `navmesh` / `traversability`** | derived from `surface` + `instances` by whoever walks. Stored, it is a second truth |
| **Not decided: `audio`, `light`, `decal`** | today they are appearance carried by an `instances`/`surface` asset. They become words the moment a consumer appears that cannot read them that way — and the rule for adding one is §4's closure rule, not taste |
