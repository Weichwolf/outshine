Type: feature
State: open
Area: render
Tags: backend, wasm
Depends: 1582

# A software rasteriser is a second executor table

**Benchmark** — Unreal: Nanite HAS a software rasteriser, in compute, for triangles smaller than the hardware path is efficient at — so a second rasteriser is not a fallback but a chosen executor. RAGE: none. **Taking Unreal** — the evidence says a second table is legitimate where the first is inefficient, not where it is absent.

Retro games on the modern engine, and a wasm build without WebGPU, are a wanted target — served
by a GL 1.5 fixed-function software rasteriser as a SECOND render backend behind the ONE plan.

The deeper point: **one backend proves no abstraction.** The second executor table is the PROOF
of the registry seam — that the plan is genuinely declarative and nothing above the renderer
smuggles SDL_GPU assumptions. It is also a deterministic CPU reference that gives pictures
without a GPU.

## What will be true

- [ ] The stage registry IS the seam: the same declared plan, a second table of executors.
- [ ] Capability is an EXISTENCE question — a stage the backend cannot execute refuses loudly
      and publishes `-> neutral`. Degrade on detail, refuse on existence; no silent discard.
- [ ] The medium LUTs come from the C++ side on the CPU (board:1580), the sky becomes a baked
      dome, subjects draw Gouraud/DOT3, and SceneHdr collapses to LDR with a CPU tonemap.
- [ ] The rasteriser is a sibling dependency of ITS BACKEND ONLY: a build without the backend
      never sees it, and the core takes no new dependency.
