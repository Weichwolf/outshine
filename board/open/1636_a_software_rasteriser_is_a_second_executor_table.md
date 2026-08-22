Type: feature
Area: render
Tags: backend, wasm

# A software rasteriser is a second executor table

Owner ruling (2026-08-22): retro games on the modern engine, and a wasm build WITHOUT WebGPU,
are a wanted target -- served by ../softgl (GL 1.5 fixed-function software rasteriser, SIMD,
wasm-proven, Mesa-referenced 1296 tests green) as a SECOND render backend behind the one plan.

The decided shape:
- the TARGET stage registry (the executor table) is the seam: same declared plan, a second
  table of executors; the renderer column is already the only SDL speller in the tree
- capability is an EXISTENCE question: a stage the backend cannot execute refuses loudly and
  publishes -> neutral (degrade on detail, refuse on existence) -- no silent discard
- the medium LUTs come from the C++ twins (ParticipatingMedium.h) on the CPU; the sky becomes
  a baked dome; subjects draw Gouraud/DOT3; SceneHdr collapses to LDR with a CPU tonemap
- softgl stays a SIBLING dependency of the softgl BACKEND only -- the core takes no new
  dependency (house rule); a build without the backend never sees it
- side value: a deterministic CPU reference backend closes the 1634 class of blindness and
  gives CI pictures without a GPU

Depends: 1582
