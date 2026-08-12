# Now

| | |
|---|---|
| **In the tree** | Phase 1's five blockers — the container's Mach-O binary, nine `_GNU_SOURCE` lines that break the Linux build, `make` missing from the image, `FieldTooSmall` as a terminal hole, one `default:` over a house enumeration |
| **Then** | The renderer stage, which leads from here |
| **Blocked** | The harness — four demonstrated defects, three of them green over a failing test. Fixed by one change: the verdict is the reporter's printed trailer, not an eight-bit exit status |

**Requirements are pulled, not marched.** `doc/requirements.md` is ordered by band — engine, world,
vegetation, buildings, vehicles — and that is a **taxonomy, not a schedule**. A line is built when the
stage that needs it reaches for it. Band 0 holds 78 lines about residency that nothing currently reaches
for, while the renderer needs a linear readback sitting unticked in a band nobody was working on. A stage
is done when it renders, not when a band is exhausted.

**1 406 lines open, 227 ticked.** A tick names the file that implements it **and the test that holds it**
— today it names at most the first, and twice this week it named a file that had been deleted.

---

## The renderer stage — what it reaches for

Ordered by what unblocks what, not by band.

| | Pulls | Why |
|---|---|---|
| **A studio scenario — a stage with no world** | § I.25 | Nothing else can be rendered in isolation. A studio stage declares a `Ground` directly, and `Ground` is already the whole interface between world and generator, so no second code path exists. It also moves every generator test off the network |
| **A glTF reader** | § I.26 | The shared input for geometry and camera. Light and material are declared beside it, never through it — Blender's importer applies a 683 lm/W factor in its default mode |
| **Ten scenes, one new thing per rung** | § I.26 | Triangle · quad · cube · sphere · lit cube · lit sphere · albedo and emissive · point light · shadow and plane · texture at 1:1. **Rung *n* waits for *n−1*** — ten reds is one finding |
| **Coverage parity** | § I.26 | Needs no light model, no linear tap, no material mapping. IoU ≥ 0.999, boundary p95 ≤ 0.5 px, instrument floor **0.005 px** because Cycles' box filter is constant and the integer raster coordinate is the pixel centre |
| **`render/` → SDL_GPU** | § I.19, § I.24 | 9 340 lines in 36 files, 2 739 of shader, HLSL through `SDL_shadercross`. **Accepted against coverage parity** — the only acceptance criterion the port has. The still becomes a *new* single sha, because a pixel identity cannot survive an API change; the geometry counters must not move |
| **A punctual light** | § II.8 | Rung 8 is Blender's factory light and we have none — `Gpu.h` binds one sun irradiance and one sky term, no light list, no second shadow kind. Already scope we owed, unticked since before the oracle existed |
| **A scene-referred linear readback** | § I.26 | 7.37 MB copy of a texture that already exists, one method shaped like `ReadDepth`, zero cost when unasked. **Settles on day one whether `kSceneExposure = 11.0` is an exposure or physics** |
| **A material that can decline the soil constants** | § II.8 | `kGroundBounce = 0.12` and `kSelfShelter = 0.35` are statements about soil spliced into every surface at seven call sites — **+17.5 % over Lambert at albedo 0.5**, on water and glass alike. Radiance parity has no zero point until a Lambertian surface is spellable |
| **Radiance parity** | § I.26 | Median relative difference ≤ 1 % against the closed form, **with Blender's own residual against that same closed form published beside ours** — the oracle states its error before it judges ours |

## Then foliage, then buildings

Each pulls its own, the same way. Bands III and IV hold **458 and 346 lines** with no gate among them
today; the studio scenario is what makes any of them testable without a world and without a network.

## Standing

- **Coverage has no baseline** — no coverage instrument exists. *Not yet measured.*
- **Nothing tests that a counter survives a 32-bit target.** A `static_assert` over the ledger's field
  widths is stronger than the gate that left with the browser, and costs nothing.
- **`/bake` has no consumer.** A live container at 40 h and 10 218 prefetches reports `baked=0`;
  **3.1 GB of a 7 GB cache serves nothing**, including 177 MB whose producer is not in the source at all.
- **`TerrainSource` returns bare bytes** — absent, still fetching, transport refused and an empty 200 are
  all `{}`, with the reason on a thread-local. The provider contract would be built from that shape, so
  it is a named follow-up rather than a note.
