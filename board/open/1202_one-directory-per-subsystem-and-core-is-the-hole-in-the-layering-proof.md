Type: feature
Area: core
Tags: scope, instrument

**One directory per subsystem, and `core/` is the hole in the layering proof**

Owner's ruling: *one `src/(subsystem)/` folder for each Outshine module, no generic things like `world`,
and there is a lot of old stuff.* `board:1201` is the same instruction applied to one directory; this is
the whole tree, and `1201` becomes a step of it rather than a separate answer.

## The argument is `CLAUDE.md`'s own, and it condemns the current shape twice

**Once by name:** *a layer that cannot be named on this diagram does not belong in the engine.*
`src/core/`, `src/clients/`, `src/scenario/` and `src/assets/` are not on the diagram.

**Once by measurement, and this is the harder one:** the table lists **`src/generators` under `Ground`
AND under `compositor`**, with `src/generators/draw` under `generator`. **One directory, two layers.** So
the claim that *layering is the build, never a checker* is already false where it is asserted — the
include set cannot separate what one directory contains. **A per-subsystem include set is finer than five
layer buckets, not coarser**, which is why the owner's instruction strengthens the proof rather than
trading it away.

## `core/` measured — 45 files, seven subsystems, and everything includes it

| what is in there | what it actually is |
|---|---|
| `AlpineLimit` | a **treeline model**, reached from `Forest.h` and `VegetationTemplates.h` |
| `ClassStructure` · `StructureMesher` · `FacadeUv` | buildings |
| `TreeLook` · `GroundSample` · `SurfaceState` · `WaterDepth` | the field a generator reads |
| `Geodesy` · `Mercator` · `CivilTime` · `Ephemeris` | earth frames and the sun |
| `CalmWeather` · `ConstantWindWeather` · `WeatherProvider` | a provider family |
| `Material` · `PunctualLight` · `TangentFrame` · `UvTransform` · `ChunkVtx` · `Keyframes` | the representation's vocabulary |
| `Span` · `Json` · `Sha256` · `Mat4` · `Units` · `Capacity` · `io/` · `CatmullRom` · `TriangleBvh` | genuinely depth-only |

**Because every layer includes `core/`, the renderer can spell `AlpineLimit` and a provider can spell
`FacadeUv`.** That is the hole: one directory that turns the compile-error rule back into a convention.

## The old stuff, enumerated over every header in `src/core/` and not sampled

```
CloudDensity.h                                  included by NOTHING -- dead
State.h -> AvionicsBlocks.h -> BlockStatus.h    reached only from src/clients/Sim.h
```

The chain is the flight-sim ancestry this repository is named after, sitting in engine core and reached
by one client. **Dead is a deletion; client vocabulary in `core/` is a move.** Neither is a rename.

## The shape

| | |
|---|---|
| `src/base/` | the only universally-includable directory — `Span` `Json` `Sha256` `Mat4` `Units` `Capacity` `io/` `CatmullRom` `ClusterDag` `TriangleBvh`. **Named `base` and not `core`**: *core* is an invitation and this tree accepted it |
| `src/scene/` | the representation (`board:1201`) and its vocabulary |
| `src/gltf/` | the format alone, a serialisation of `scene` |
| `src/providers/` | today's `src/data`, under the name `CLAUDE.md` already uses for the layer |
| `src/earth/` · `src/weather/` | frames and sun · the provider family |
| `src/terrain/` · `src/vegetation/` · `src/buildings/` · `src/streets/` · `src/water/` · `src/osm/` | what `src/world` and `src/generators` share today |
| `src/render/` · `src/scenario/` · `src/clients/` | unchanged |

**The layer is then the allowed include set per subsystem**, declared once in the `Makefile` and
`test/run.sh` — the same mechanism at a granularity that can be true.

## Done when

- [ ] `src/core/` and `src/world/` do not exist
- [ ] Every directory under `src/` is one subsystem, and **`CLAUDE.md`'s layer table names each of them
  exactly once** — the check that the current table fails
- [ ] One compile group per subsystem with its own include set, in the `Makefile` and `test/run.sh`
- [ ] `test/outshine/unit/` mirrors the new tree, which is what makes each include set a continuous proof
- [ ] `Area:` in the board vocabulary is regenerated from the directories, since **that vocabulary is
  defined as the tree's own layering**
- [ ] `CloudDensity.h` is deleted rather than rehomed, and the `State`/`AvionicsBlocks`/`BlockStatus`
  chain is placed with its one client or deleted with a reason

## Comments

**The cost is not the moving, it is that `Area:` and the layer table are derived from the directory
names.** A rename therefore touches every board item's header and one table in a binding document, so it
is a single commit or it is a tree that disagrees with itself in the middle. *That is an argument for
doing it in one pass, not for doing it later.*

**And it should be sequenced against `board:0079`'s corpus work rather than interleaved.** The corpus
sequence is the goal's critical path; this is a refactor with no picture behind it, and a round that
does both will attribute neither.
