Type: bug
Area: clients
Tags: khronos, perf, instrument

**Declaration and build**

- **A derived constant whose derivation no longer exists.** `world/ChunkSurface.h:58`
  `kSurfaceAgreementM = 9.17e-4f` is the ceiling on how far the two evaluators of the terrain surface
  may disagree, and it is the sum of seven float32 terms. The instrument that summed them and checked
  the sum against plumb runs was the deleted surface-budget tool, deleted with `tools/` on 2026-08-12. The
  number is unchanged and may well be right; what is gone is any way to recompute it, so it is a
  measured value with no reproducible origin — against `CLAUDE.md`'s *every number carries its
  origin*. Right: a test under `test/unit/world/` that reconstructs the seven terms and asserts the
  constant bounds them, which is the same arithmetic in the language the tree is written in.

- **The browser is gone from the code and still in the prose, 30 times — Band 2.** The same grep over
  `src/**.{h,cpp,wgsl}` returns **30 hits** at `9f4ba9e` (down from 38 as the browser-era clients were
  deleted) — `clients/Artifacts.h:11`, `clients/Sim.h:42`, `core/io/Log.cpp:18`,
  `core/GroundSample.h:2`, `core/Camera.h:82` among them. A comment that
  explains a decision by a platform no target compiles for is a reason the reader cannot check, which
  is `NL.2` failing in the direction that costs most. One of them is not a comment: `clients/RunIdentity.h:22`
  carries an `Agent` field that is *"the browser's own version string and empty natively"* and is
  published as a telemetry column that is now always empty. Right: each site either states the reason
  that still holds or goes, and the `agent` column goes with the browser that filled it.

- **`Artifacts` is an interface over one implementation, and two of its states cannot occur.**
  `clients/Artifacts.h` exists because *"a directory natively, an HTTP endpoint in the browser"*. **At
  `9f4ba9e` it has zero implementations** — `git grep -l ': Artifacts'` over `src/` and `test/` is
  empty; `FileArtifacts` and `SceneRunner` were deleted with the clients, and nothing replaced them.
  It still declares `enum class Delivery { InFlight, Complete, Refused }` (`:19`), two of whose three
  states no code can now produce. `C.121`/`I.25`: an abstract interface over nothing is not an
  abstraction, it is a shape waiting to be re-derived wrongly. **Band 1** — the Khronos runner is the
  next thing that writes artefacts, and it will either implement this or replace it, so the decision
  is due before it, not during it. Right: the runner writes to a directory, `Delivery` is `Complete` or
  `Refused`, and the wait has no subject to wait for.



- **The language standard has two values — Band 2.** `Makefile:24` and `test/run.sh:44` both set
  `-std=c++17`; `Makefile:87` and `test/run.sh:129` hard-code `-std=c++20` for anything touching
  Dawn. `CLAUDE.md` no longer names a dialect at all —
  it says *modern C++* — and `board/` § I.19 declares C++20 as the one value, unticked. The program is C++20 — forced by
  `vendor/dawn`, whose `webgpu_cpp.h` needs `std::type_identity` and `std::span` — while the C++17 arm
  judges a dialect the program is not built in, so a C++20 construct outside the render layer is
  compiled by nothing that would catch it. Right: one variable, one value, and the reason (Dawn) beside it.

- **No gate reads the log levels of the run it just declared green.** `verify-walk-asan` now asserts
  the run's own motion verdict — `frames=10800 impostorStands=9565 treeTris=19130` — but a line at
  `ERROR` in the same 10 800-frame run still passes it, and the run emits exactly one:
  `render device_lost reason=2 msg="Device was destroyed."` (`Makefile` `verify-walk-asan`,
  `render/Renderer.cpp:163`). Two defects, and the second is the reason the first cannot simply be
  closed. `2` is `wgpu::DeviceLostReason::Destroyed` (the deleted Dawn header)
  — the device the client destroyed on purpose at teardown, reported at the level reserved for a run
  that failed, and reported as an integer rather than as the enumerator (`Enum.3`). So an `ERROR`
  assertion added today would go red on a healthy run. Right, in this order: the callback answers
  `Destroyed` at `Info` and every other reason at `Error`; then the gate asserts that the run it
  declares silent logged nothing at `ERROR`.

- **`core/Mat4.h` is entirely dead, and the comment defending it names a test that does not exist.** `Mat4Identity`, `Mat4Mul`, `Mat4Perspective`, `Mat4LookAt`, `Vec3Normalize` and `Vec3Cross` have no caller outside `core/Camera.h`; inside `Camera.h`, `CameraBasisFrom`, `CameraAxes`, `HorizonDipRad`, `MvpTranslate`, `Frustum`, `FrustumFrom` and `AabbVisible` have none either. `CameraBasisEcef` is the only live function in the pair — **re-verified at `9f4ba9e`: one caller, `clients/Sim.cpp:555`**, the bench that was the second having been deleted. `Mat4Perspective` is reached only from `core/Camera.h:71`, itself dead. `Camera.h:76` asserts "CameraBasisFrom above is NOT dead: sky dome and star field are an infinity pass in LOCAL render-ENU"; `SkyStage` and `StarsStage` call nothing in the file. Two comments say "Pinned in `test_camera.c`"; no such file exists anywhere in the tree. Three consequences, worst first: the dead `Mat4Perspective` builds a **GL-style [-1,1] reversed-Z** projection, so anyone reviving it under WebGPU's [0,1] clip volume silently loses everything past the mid-range; `outshine::Frustum` (`Camera.h:132`) and `outshine::Render::Frustum` (`render/Frustum.h`) are two spellings of one statement against "every statement has exactly one place"; and a false comment is worse than no comment. Right: delete `core/Mat4.h` and everything in `core/Camera.h` but `CameraBasisEcef`.
- **Two headers guard themselves with reserved identifiers.** `core/Ephemeris.h:6` `#ifndef _EPHEMERIS_H` and `core/State.h:3` `#ifndef _FBSTATE_H`. A leading underscore followed by a capital is reserved to the implementation **in every scope** ([lex.name]/3) — undefined behaviour, not a style preference, and the rest of the tree already spells it `GEODESY_H`.
- **The log's timestamp is dead** — every `walk key` line carries `t=0.0` — and key repeat events are logged individually, so a held key floods the buffer.
- **`FacadeUv.h` has no `static_assert` anywhere**: 11 enumerators against a stride of 16, `kStyleCount 8` against 7 enumerators. A 17th `Facade` silently aliases identity 1.
- **`TreeGrower::GrowOnce` is ~110 lines** (`F.2`/`F.3`), and `TreeSpecies::Parse` is a 90-line flat key list (`F.3`).

- **A wire decoder reads multi-byte values in the host's byte order.** `world/OsmVector.cpp:111-112`
  takes protobuf `fixed32` and `fixed64` with `std::memcpy(&f, v.P, 4)` and `std::memcpy(&d, v.P, 8)`
  straight off the wire buffer. Protocol Buffers fixes those two wire types as **little-endian**
  (encoding spec, `fixed32`/`fixed64`), so this is correct on every target we build for today —
  wasm32 is little-endian by specification and both native hosts are — and it is wrong on any
  big-endian one. Filed as a bug rather than as a requirement because the code claims to decode
  protobuf and decodes host order instead; the caveat was checked and it is the only reason it has
  never shown: **the tree has zero `reinterpret_cast` and the rest of its decoding is byte-wise**
  (`world/OsmVector.cpp:18-19` assembles varints with `<<` and `0x7f`, `data/TerrariumDem.cpp`
  assembles Terrarium height from bytes), so this is a single site, not a
  habit. The neighbouring `std::memcpy`s of floats into word arrays (`core/ClassStructure.cpp:74-76,124`,
  `world/ClassBuilder.cpp:340`) are **not** this defect — they are an in-memory layout that is never
  serialised, and host order is the right order for them. Right: two byte-wise assemblers beside the
  varint reader, `LittleEndianF32` and `LittleEndianF64`, and no `memcpy` from a wire buffer anywhere.

- **The declared negatives pass on any compile failure of the right shape.** `Makefile`
  `verify-generators` and `verify-world` accept a fixture as refused if the compiler exits non-zero
  **and** its output contains the substring `file not found` — so a fixture that misspells its own
  forbidden header (`#include "Rendererr.h"`) is a green gate that proves nothing, and so is one whose
  body stops compiling for an unrelated reason while the include still resolves. Four fixtures ride
  this: `RendererIsNotReachable`, `WorldIsNotReachable`, `LogIsNotReachable`, `DrawIsNotReachable`,
  plus `test/unit/compile/world/GeneratorIsNotReachable`. The three `-Werror` negatives under
  `test/unit/compile/core/` are
  weaker still — `verify-types` checks only the exit status and matches no diagnostic at all, so any
  error in `HeightIsNotReachableWithoutItsState.cpp`, `AnswerIsNotIgnorable.cpp` or
  `DepthIsNeverNegative.cpp` passes it. Right, and it costs nothing: demand the **exact** expected
  diagnostic text (`fatal error: 'Renderer.h' file not found`, `error: ignoring return value of
  function declared with 'nodiscard' attribute`) **and** that it is the *only* error the compiler
  emitted, which is what makes a typo elsewhere in the fixture fail the gate instead of satisfying it.

- **`test/unit/generators/SameRegionSamePlacement.cpp` is 689 lines behind one `main`** (`F.3`), carrying on
  the order of thirty distinct claims — determinism of placement, the class lattice, water depth,
  way half-widths, `sizeof(Body)`, an allocation count — in one process. Three consequences: the
  first hard failure (it dereferences `std::optional` results directly, e.g. `ways.MadeAt(...)->WidthM`)
  hides every claim after it; the run reports `verify-generators: N failed` without a machine-readable
  name per claim; and no claim in it can be run alone. It also holds the tree's only `malloc` outside
  `world/terrain` (`:315`; `clients/SimHost.cpp`'s went with the file in `b83285f`). Right: one translation unit per claim under
  `test/unit/generators/`, each with its own `main`, which is what the suite this file's requirements
  now describe is for.


- **Fifteen preprocessor conditionals in the library compile for a target no build produces.**
  `core/io/HeapProbe.cpp` (6), `clients/HttpPost.cpp` (4), `render/Renderer.cpp` (3) and
  `core/io/StackProbe.cpp` (2), with four `#include <emscripten…>`. Down from twenty: the fetch's own
  `EM_JS` primitive, `fb_take_http_body` and the loader's browser arm left with the HTTP hop
  (2026-08-12).
  No target compiles them since the wasm targets were deleted, so they are unbuilt code inside `src/`
  — the one thing `-Werror` cannot see. `HttpPost.cpp:17` additionally justifies a static's lifetime
  with `-sEXIT_RUNTIME=0`, a flag from a build file deleted in the same commit. Right:
  the branches leave with the round that builds the `host/` layer, and until then nothing may be added to
  them.

- **Nothing tests that a cumulative counter survives a 32-bit target.** `verify-counters` did, in the
  browser, and went with it: last green 2026-08-12 on Chromium 151.0.7922.34, `poolPosts=220
  poolRepeats=2147999561 sizeofLong=4`. `TilePool.h:52` still declares its ledger `long long` for
  exactly this reason and the comment naming it is now the only thing holding it. Right: the check is
  a property of the declarations, not of a platform — a static assertion over the ledger's field
  widths costs nothing and needs no 32-bit host.


- **`HttpPost.cpp`'s abandoned-status vector is dead, and the comment that justifies it cites a
  build file that no longer exists.** With the browser path gone, `Begin` is the curl arm
  (`clients/HttpPost.cpp:62-88`), which sets `*Status_` to a terminal value **before it returns** —
  an HTTP status or `kNoAnswer`. `~HttpPost` (`:107`) pushes into `gAbandoned` only when
  `*Status_ == kInFlight`, which is now unreachable, so the `std::vector<std::unique_ptr<int>>` at
  `:20` is a static that can never gain an element. Its six-line rationale still argues from
  `-sEXIT_RUNTIME=0` in a Makefile deleted in the same commit and from a promise resolving after
  destruction, neither of which any target can produce. This is what a dead `#ifdef` costs beyond the
  lines it occupies: the *live* code around it keeps a shape and a justification that stopped being
  true, and nothing compiles it wrong. Right: the vector, `kInFlight`, the two-state cell and the
  destructor go together, and `Status_` becomes an answer that exists or does not.


- **`src/data` does file I/O with `<cstdio>` and there is no `Host::Storage` to do it through.**
  `data/ContentStore.cpp` opens, writes, renames and sweeps with `<cstdio>` and `<filesystem>`, and
  `data/StarBands.cpp` reads its four band files the same way. `board/` § I.24 rules that
  the library tier is read through a declared host seam and never `fopen`; the transport half of that
  seam exists now (`data/Transport.h`, implemented in `test/host/`) and the storage half does not.
  Three further `fopen` sites in `clients/` and `world/` predate this and are the same line. Right:
  `Host::Storage` beside `Host::Transport`, declared by the library and implemented by the host, and
  the content store becomes a policy over it rather than a user of a libc call.

- **A source's refusal cannot name what it looked for.** `data/StarBands::Collect` answers
  `Meaning::Refused` when a band file will not read, and the path it tried is not in the answer — the
  provider layer may not name `Log` (that is the layering, and it is right), and `Fetched` carries a
  meaning and bytes and nothing else. The caller that knew the directory and logged it went with the clients on 2026-08-12, so today the
  file that failed is in no record at all. § I.22's *a test that must not reach the network declares zero sources
  and gets a refusal by name* wants the name to travel with the refusal. Right: `Fetched` carries a
  short reason string on the refusing arms only, minted by the source and printed by the caller.

- **`clients/HttpPost.cpp` still carries `curl`, so the tree's transport is behind the host seam and
  the telemetry poster is not.** Measured 2026-08-12 with `nm -u` over `build/obj-walk/*.o`: zero
  `curl_` symbols in every `core-`, `data-`, `gen-`, `world-`, `sim-` and `render-` object, eight in
  `host-CurlTransport.o` where they belong, and **seven in `app-HttpPost.o`**. That is the fb-sim log
  and telemetry channel, which `board/active/` already carries as its own item (*the library owns its
  log*); it is recorded here because "no transport library in the library" is now true of the data
  path and not yet of the tree.

- **A Python file survives in the tree, and it is the one that can recompute the star catalogue.**
  `assets/sky/stars/build_stars.py` (170 lines) fetches HYG v41, applies proper motion and IAU-1976
  precession to the run epoch and writes the four magnitude bands the `hyg.bands` source serves. It
  moved with its data out of the deleted `tiles/` rather than being deleted with it, because the bands
  are admissible measured data only for as long as *we* can recompute them and nothing else in the
  tree can. Against `CLAUDE.md`'s **modern C++, and only C++**. Right: it becomes a declared test that
  bakes the bands and checks them against the committed ones, in C++ — at which point advancing the
  epoch is a run of the test suite rather than a script somebody remembers.
