# Mods — what a title may declare, and what it may not

> Owner, 2026-08-05: *„daraus folgt auch, dass die `mods/` komplett deklarativ sind."* ·
> *„Szenarien sind `mods/` auf die Outshine-Engine."*

Spec-first. **`mods/` holds two mods, `demo` and `webcams`, and each holds one `mod.json`.** See
`## State`.

## Spec

### 0. The boundary is epistemic, and that is what makes it checkable

> *„Outshine ist Gott und weiß alles. In den `mods/` ist, was nur kennt, was es kennt."* — owner,
> 2026-08-05

One question decides where a thing belongs: **does this need knowledge no participant could have?**
Yes → engine. No → mod. The engine builds and simulates the world and is omniscient in it; a mod brings
actors with a limited view.

That draws a line the current goal makes concrete:

| Owns | What |
|---|---|
| **Outshine** | **Earth.** Terrain, land cover, vegetation, water · infrastructure — roads, rails, bridges, power · buildings · the epoch and decay parameters. All of it derived from DEM, OSM and the classification chain |
| **a mod** | actors, entities, usable objects, and its **scenes**: position, direction, field of view, time, wind, cloud cover, exposure — and, where nobody is watching, the recording itself |

### 0.0.1 A mod is a set of scenes, and a scene is one of two kinds

`mods/<name>/mod.json` is `{"schema": "outshine/mod/1", "name": …, "scenes": [ … ]}`. A client is told
two words — **which mod, which scene** — and that is the entire command line. Everything else that used
to be a switch is a property here; what is left is where this machine keeps things, and that list with
its justification is in [`build-and-ops.md`](build-and-ops.md).

| Scene kind | What it is |
|---|---|
| `interactive` | the viewer stands in it and can walk. `demo/walk` |
| `run` | it executes without a viewer and delivers products: `capture` (frame size, warm ceiling, settle length) plus an ordered `runs` array |

**The run kinds are `motion`, `classDump`, `classCompare`, `windProbe` and `subject`,** and `motion` is
the one that records: `frames`, `fps`, `world` (`frozen`|`streaming`), `give` (`stills`|`profile`),
`path`, an optional `depth`, and an `animation`. Movement is glTF's channel/sampler shape with the
keyframes measured in **frames**; the targets, the interpolations and the accumulated-angle rule are in
[`clients/clients.md`](clients/clients.md) § *The run language*. **A still is the run with one frame and
no channel** — not a separate concept.

A scene may carry blocks the engine does not read; `webcams` carries a `pose` block with the resection's
provenance (`fitted`, `residPx`, `fitImage`, the reported bearing and focal length). The engine reads
what it knows and the mod's own tooling reads the rest.

**A mod never declares a tree and never declares a turbine** ([`goal.md`](goal.md)). A road is not
scenario, it is Earth. If a mod could place a plant, the claim that the engine generates the world would
be untestable, and so would the epoch claim below it.

### 0.1 The language is JSON, and that is a decision with a reason

The previous era shipped four bespoke line formats — mission, campaign, catalogue and HUD — each with a
hand-written parser and its own error messages. They are dead, and nothing is migrated: the titles that
used them are gone, so there is nothing to migrate.

> **A bespoke format is a parser nobody ordered.**

An engine cut for a machine to author into declares in **JSON**:

| Property | What it buys |
|---|---|
| schema-checkable | a wrong declaration is caught by a schema, not by a parser's bespoke diagnostics |
| diffable | a review sees what changed, and a regression names the line |
| generatable | a machine emits it without learning a grammar first |
| one reader | `render/Json.*` already parses JSON in this tree; a second syntax is a second thing to keep correct |

The cost is stated too: JSON has no comments, and the line formats used the header comment to carry the
reading rule of a run. **A reading rule is data, not a comment** — it becomes a field, which is a better
place for it anyway, because a machine can then read it.

**Shaders are the one exception, and it is narrow.** A mod may ship a shader for the appearance of its
own entities. Appearance is not knowledge; a shader cannot query the world and cannot decide anything.
A mod ships **no `.cpp` and no world**.

### 1. The undeclarables list IS the plan

If a mod is **fully** declarative, then anything a title needs that cannot be declared is not a mod
problem — it is the engine's backlog.

That makes the three epoch mods ([`vision.md`](vision.md)) a measuring instrument for genericity rather
than three content jobs. Known undeclarables today:

| What a title needs | Why it is not declarable yet |
|---|---|
| a person on foot | [`body-format.md`](body-format.md) spans it in principle; nothing implements it |
| a wheeled or tracked vehicle, a rotorcraft, a boat | contacts and drive torque are declared in the format, absent in code |
| **the epoch and decay dial** | no declaration surface exists; `epoch` and `decay` appear nowhere in the code |
| a body, an entity, a manifest as a schema-checked file | only `scene.json` and `mod.json` exist, and neither has a schema |
| a per-title overlay | the previous line format is dead, and `render/` has no text stage at all |

**The list is the deliverable of a round, not the title.** A title that required a patch to the engine
is a title that measured a hole — and the hole is worth more than the content.

### 2. What a mod directory contains

> Owner, 2026-08-05: *„`mods/` haben ihr eigenes `doc/`."* · *„`mods/common/` würde ich lieber nicht.
> Dann lieber Abhängigkeiten auf andere Mods. Das ist ja auch der Mod-Gedanke."*

**No shared bucket.** An asset lives in the mod that needed it first; others declare `depends`. A
`common/` becomes a junk drawer, while an explicit dependency stays honest about who needs what.

```
mods/<title>/
  mod.json          identity, the epoch, its own directory roots, and `depends` on other mods
  doc/              WHAT WE WANT — the scenario, reconstructed and sourced
    place.md          the real bounding box AND why this real place
    sources.md        every claim with URL + page; contradictions kept, not smoothed
  src/              WHAT WE CAN — declarations only, no code
    bodies/           the body declarations (body-format.md)
    entities/         actors, usable objects, their equipment
    scenes/           the scenes, in JSON
```

**And there is no `test/`.**

> Owner, 2026-08-05: *„`mods/` haben keine `test`, da die Missionen auch im Gym laufen."*

If a mod *is* a declaration, a `test/` beside it is the same assertion written twice — and this tree
already knows what a duplicated statement does: the two copies drift, and the one nobody runs is the one
that lies.

| | The subject is | Can it assert about itself? |
|---|---|---|
| `sim/src/` | **code** | No. C++ cannot state its own expected behaviour without becoming the thing that decides whether it passed |
| `mods/<title>/src/` | **data** | **Yes.** A declaration carries what it sets up and what it expects in one file |

Consequences:

1. **`verify-trees` must know one bit** — that a mod is two-tree, not three.
2. **The engine's `doc/` does not document mods**, and a mod's `doc/` does not document the engine. Same
   boundary as `src/`.
3. **The reconstruction has a home.** *Why this real place* is neither engine knowledge nor a verdict —
   `mods/<title>/doc/place.md` is the only place it can live.

### 2.1 The client is title-free

**Several titles in one WASM build is the goal, so the browser cannot be built for one.** A client that
bakes a directory, a file name or a type key has the scenario compiled into it — the same defect §0
removes from the engine, one layer up.

**The manifest is the only place a directory or a type key is named**, and it must reach the browser
instead of being read at build time and forgotten.

| Half of a mod | How the browser gets it | Root |
|---|---|---|
| manifest, bodies, meshes | preloaded into the virtual filesystem by `make wasm` | `/fb/mods/<id>/` |
| scenes | fetched over HTTP from `web/` | `/mods/<id>/` |

Both halves keep **`mod.json`'s own relative directories**, so ONE manifest resolves in both mounts and
no path exists twice.

### 3. Acceptance

| Contract | Anchor |
|---|---|
| No engine code per title | `mods/*` contains zero `.cpp`/`.h`; checked by a tool, not by intent |
| Every declaration is JSON | a schema per surface; a file that does not validate is refused at boot with a named reason |
| A mod knows only what it perceives | the engine is omniscient in the world it builds; a mod is a participant |
| **A mod adds no world** | no tree, no turbine, no road, no building. Earth belongs to the engine ([`goal.md`](goal.md)) |
| **The epoch is a mod's declaration, the geometry is not** | two mods on the SAME place must differ only in the dial ([`vision.md`](vision.md)); if a mod can edit geometry, that claim is untestable |
| A mod asks, it does not instruct | no mod names an LOD, a triangle budget or a draw call. It declares what exists and what matters; the engine holds the frame budget ([`render/visual-target.md`](render/visual-target.md)) |
| The client is title-free | no client names a mod, a directory, a mesh file or a type key: `grep` finds them only in `mod.json` |
| The undeclarables are named | each round publishes what the titles could NOT declare — that list is the engine backlog (§1) |

## State

**Built 2026-08-08.** `mods/` holds two mods and both clients read them through `clients/Mod`:

| Mod | Scenes |
|---|---|
| `demo` | **9** — `walk` (interactive) plus `frame`, `spin`, `sequence`, `classes`, `wind`, `sunrise`, `subject-buche`, `subject-wiese`. `frame` reproduces the pre-conversion oracle frame **bit for bit** (md5 `67978b94ddc4a3e0621ad026fdaef1d9`) |
| `webcams` | **26** — one live scene per camera plus a `-fit` scene per camera that has a fitted pose, carrying the resection provenance. `tools/webcams.py` no longer invents a scene: it writes back the one thing that changes per run, the live image's `utc`, and then runs `gpu_walk webcams <slug>` |

`mods/demo/scene.json` and `mods/webcams/cams.json` are **deleted** — the first is `demo/walk` plus
`demo/frame`, the second is the `webcams` mod's scenes and their `pose` blocks.

The five titles that existed on 2026-08-05 are deleted with the era they belonged to. What their
existence proved is kept as knowledge and nothing more:

| Proven then | Still true |
|---|---|
| the engine can be asset-free — no aircraft, no mesh, no scenario in `sim/` | yes; the include roots and the manifest reader are the mechanism |
| a manifest can be the only place a path is named, resolved by three readers (C++, the tool tree, the Makefile) without a path existing twice | yes structurally, but **two of the three readers are deleted** — `sim/tools/mod.py` went with the tool tree |
| several titles can live in one browser build, picked by name at boot, with an unknown id refused instead of silently substituted | yes structurally; nothing does it today |
| a title could not be fully declarative — build recipes stayed C++ | **that was the measurement.** It is why §1's undeclarables list is the plan and not an afterthought |

No number from that era is carried into this file. They measured deleted subjects.

## Gaps

- **There is still no SCHEMA, only a parser.** `mod.json` declares `"schema": "outshine/mod/1"` and
  nothing checks it: an incomplete declaration stops the boot, which is parser behaviour and not
  validation. A body and an entity have no surface at all.
- **A mod may carry keys the engine silently ignores** (`webcams`' `pose`). That is deliberate — the
  provenance of a resection is the mod's business — but it means a typo in a key the engine DOES read
  is indistinguishable from a key it never read. A schema fixes both at once.
- **Three animation targets the language wants and the engine cannot apply per frame:**
  `wind.fromDeg`, `wind.speedMs`, `weather.cloudCover`. They feed `World::SetWeather` and the class and
  albedo bake, so moving them mid-run would have to re-bake tiles; the five that only reach a renderer
  setter (`camera.*`, `sky.clockS`, `wind.clockS`, `exposure.compEv`) are built. They are refused at
  load rather than silently ignored.
- **The epoch has no declaration surface.** [`vision.md`](vision.md)'s central claim — two mods, one
  place, one dial — cannot even be expressed today.
- **The body format is spec-only**, so none of the three epoch mods' bodies can be declared at all.
- **`mod.json` is unchecked by any gate**, and the reader that resolved it is deleted.
- **A mod's `doc/` is not checked for completeness.** A gate can see that `doc/` exists, not that §2's
  two texts are in it.
- **`verify-trees`' runnable-proof check is gone** — it demanded a `.fbm` and that parser was deleted
  with the combat layer. A gate that asks for a capability the engine no longer has proves nothing, so
  a mod's `src/` currently proves nothing either.
- **Rejected, with its reason: a `mods/common/`.** It becomes a junk drawer that nobody can attribute;
  an explicit `depends` stays honest about who needs what (§2).
