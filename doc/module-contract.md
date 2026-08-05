# The Outshine module contract — state in, geometry out

> Owner, 2026-08-05: *„jedes Outshine-Core-Modul erfüllt denselben festen Vertrag … ich möchte schon vom
> Directory-Tree und den Dateinamen die komplette Struktur lesen können und neue Funktionen/Module müssen
> vollkommen eindeutig zu integrieren sein."* · *„letztendlich haben wir Weltzustand und Geometrie, also
> müsste sich eine passende Abstraktionsebene finden."*

Spec-first. Nothing built.

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

## State

Nothing built under this name. What exists that already obeys it: the layer order and its gate, `doc/`
mirroring `src/`, and `FBCapability.h`'s declaration list (20 rows, four expansions, runtime-readable).

## Gaps

- **The closed vocabulary does not exist.** §0 rests on there being a fixed, small set of things a module
  may declare it contributes. Naming it is the design work, and it is cheap to get wrong.
- **`verify-trees` does not know what a module directory is**, so §2 is an intention.
- **No module owns its state explicitly today** — state is spread across `core/`, and who may write it is
  a compile-time argument rather than a structural one.
