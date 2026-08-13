# Now

**19 Khronos criteria of 23 scored.** 106 tests, 88 PASS / 18 FAIL — the eighteen are **nine render
cases counted twice**, plain and `~sanitised`. The `scenario` suite has **zero members**.

| | |
|---|---|
| **In the tree** | the emissive oracle reduction — three assets carry an `emissiveTexture` and Cycles samples emissive geometry through the light tree, so at 1 spp the reference is salt-and-pepper. The acceptance is two seeds bit-identical |
| **Then** | the picture bound of § I.26.15 — whole image, RGBA, on the declared transfer unquantised. It will turn some green cases red and that is the point |
| **Then** | § I.28's three artefacts, none of which exists |

## The four constraints

**SDL3 · SDL_GPU · modern C++ · this device at 720p60.** The port met the first three for the first time
at `0161f88`. **The fourth is undemonstrated** — no camera path, no frame clock, no scene worth
measuring. `CLAUDE.md`'s *distribution over a moving camera, p50/p95/p99, never a mean* has **no
instrument, no subject and no case**, and until one exists no line anywhere may claim the target is met.

That is not three separate tasks. A declared camera path, a per-frame clock with its floor published,
and a scene are useless apart, and together they are the `scenario` suite's first member.

## § I.28 — what it names that does not exist

| | |
|---|---|
| **the emit path** | `Subject → Document → bytes`. The reader answers to every legal file; the writer answers to **one producer** — no sparse accessors, no stride variety, one buffer. Acceptance is `Subject(Emit(S)) == S`, a fixed point of the **flatten**, and it needs neither Blender nor a GPU |
| **the compositor** | four implementations — terrain, forest, city, traffic. Terrain's composition **exists and is fused to the streamer** in `src/world/World.cpp`, so that quarter is moving code rather than writing it |
| **the CPU frame instrument** | wall-clock distribution with the compositor's own span beside the GPU spans, and `frameMs − Σ(GPU) − Σ(compositor)` as its own column |

**Why the emit path is worth it before the rest:** it is what gives a generated part a number. Cycles
renders a grown beech and the picture bound decides it — otherwise vegetation is judged by eye against
SpeedTree for as long as it exists.

## Standing

- **17 ticked requirement lines cite a file that is not in the tree** — 23 of 50 cited paths do not
  exist. Six merely moved into `test/unit/` and are one prefix edit; sixteen are genuinely gone.
  **The class ends when the harness checks it**: every backticked path in `doc/` resolves, or the run is
  red. One `grep -oE` and a loop.
- **`PointLightIntensityTest` is red and unattributed.** 0.18 % on region means, cross-talk and
  non-determinism both ruled out, offsets confirmed exact. An unattributed residual keeps the case red
  and the number published — it is not a `doc/bugs.md` entry, because that file needs a file and a site.
- **Four cases moved across the port** in `reported` metrics with no threshold, ≤ 2e-4 relative, no
  criterion changing side. The BRDF is ruled out by direct measurement. Proving the rest needs the
  1.4 GB Dawn tree the port deleted; **we priced it and declined**, which is a different claim from
  *could not measure*. `doc/bugs.md` carries the conditions that would reopen it.
- **`ContentStore` enforces its cap once, in the constructor**; `Keep` never sweeps. Its header says so,
  so it is *not built* rather than *built wrong* — and a city compositor at one unique part per footprint
  is unbounded growth against a declared cap.
