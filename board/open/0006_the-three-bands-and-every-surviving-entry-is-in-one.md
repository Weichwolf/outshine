Type: bug
Area: render
Tags: khronos, instrument

**The three bands, and every surviving entry is in one**

**Band 1 — blocks the Khronos work.** Repaired before the draw-list round, because the assets it must
load are what would hit them.

| entry | where |
|---|---|
| `core/ChunkVtx.h` carries one UV and no `COLOR_0`, while `gltf/Types.h:163` already carries `TEXCOORD_1` for `MultiUVTest` | *Constants, names and units* |
| The glTF reader resolves a URI with no scheme, authority or traversal check | *Declaration and build* |
| `Artifacts` is an interface with **zero** implementations since `FileArtifacts` was deleted | *Declaration and build* |
| The winding is hard-coded at seven sites | *Declaration and build* |
| Three node-transform cases measure an ambient-occlusion estimator at one sample | its own section |
| **Eighteen of twenty catalogue rows cannot execute**, and a row is read as a capability | its own section |
| Frame alpha derived from depth, so a translucent body over nothing is absent from our picture | its own section |
| The preparer and the runner hold two closed sets over one manifest schema, disagreeing on 8 of 26 | its own section |

*Checked and **not** in this band, against the coordinator's reading:* **`SurfaceState` carries
`SurfaceKind::Blended` and `Blends()` at `core/SurfaceState.h:8,23`, and `CoverageCut_` is per-material
at `:44` (`s.CoverageCut_ = material.CoverageCut`) with `AlphaBlendModeTest` named in the comment above
it.** Nothing there blocks that asset. **`extensionsRequired` is read and refused** at
`gltf/Document.cpp:156,295-299`, `kHonouredExtensions = {nullptr}`, so anything named is a refusal —
that entry is deleted below.

**Band 2 — cheap and just undone.** No excuse, no dependency; one round, batched.

| entry | measure |
|---|---|
| Stale pointers naming two deleted documents | **7** sites, not nine |
| German in an English-only repository | 5 sites, all live |
| Two headers guarded by reserved identifiers (`_EPHEMERIS_H`, `_FBSTATE_H`) | [lex.name]/3, undefined behaviour |
| `GpuTimer` takes no slot names and `TakeGpuTimes` has no caller | 2 sites |
| The browser is gone from the code and still in the prose | **30** hits, not 38 |
| `core/Mat4.h` is dead and its defending comment names a test that never existed | 2 files |
| `FacadeUv.h` has **0** `static_assert`s against 11 enumerators and a stride of 16 | 1 file |
| The language standard has two values — `-std=c++17` at `Makefile:24` and `test/run.sh:44`, `-std=c++20` on every shipping line | 4 sites |
| The unit-height check accepts 168 ulps where it measures 1 | `test/outshine/unit/generators/draw/GrownBarkIsAClosedMesh.cpp:225` |
| The harness's build cache is keyed by path and not by root | `test/run.sh:41-42` |
| `BenchGround` is a catalogue row named for a deleted harness, no implementation | its own section |
| Five camera manifests aim 0.4357 px off their stated derivation, origin unknown | `test/khronos/glTF/coverage/*/manifest.json` |
| Six environment variables change the picture and ride no column | `FB_TAU` · `FB_TAA` · `FB_GEOM` · `FB_TILEWORKERS` · `FB_GROUND_CLASS_VIZ` · `FB_DAGLOG` — `FB_MOON_SCALE` and `FB_TONE_PROBE` are gone |

**Band 3 — waits for the round that needs it, and the round is named.** *"Later" is a named event here
or it is a hope.*

| entry | waits for |
|---|---|
| `Node`'s *matrix XOR TRS* invariant enforced 250 lines from its type | **the round that adds a second node consumer** — one consumer cannot show the leak |
| `Document::ReadJson` is 228 lines | **the round that adds the next extension**, which is when the length becomes a cost rather than a shape |
| `Renderer`'s sixteen unconditional stage objects | **the SDL_GPU port**, which rewrites every one of them |
| `View()` creates a texture view per attachment per frame | **the SDL_GPU port**, same reason |
| Everything under *World and streaming* | **the round that restores a world consumer** — the walk client is deleted and nothing drives that path today |
| Everything under *Buildings*, *Vegetation*, *Light and shadow*, *Picture* | **the round that renders a scene against KCD** — all are picture judgements with no picture to judge |
| The data ledgers have no reader | **the round that restores a telemetry consumer** |
| The trailer is authenticated by shape, and a hard error stops the run | **the round that adds a test the harness cannot already judge** |

---
