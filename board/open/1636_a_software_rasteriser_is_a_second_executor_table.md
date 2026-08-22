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
- and the deeper point (owner, same day): ONE backend proves no abstraction -- the second
  executor table is the PROOF of the registry seam itself, that the plan is genuinely
  declarative and nothing above the renderer smuggles SDL_GPU assumptions; the softgl build
  is the standing test that the render column is a column

Depends: 1582


---

**Survey + decided seam (2026-08-22, owner steer: the renderer must be generic).** What stands:
the executor table exists (Renderer::kExecutors, Stage -> Configure/Encode) -- the PLAN side is
already table-driven and src/render/plan is pointer-free. What is hard-wired: Gpu and
PassRecording are SDL type bags, and 21 stage files plus the Renderer speak SDL_GPU directly.

The seam is the PLAN, not a wrapper: a BACKEND is an implementation of the plan's four verbs --
create resource, configure stage, encode stage, present -- against its own device. The decided
cut:
1. src/render/plan and src/render/draw stay (and are audited) backend-free -- the shared truth
2. the current stage set and Renderer internals become the SDL_GPU backend (render/sdlgpu/),
   the first row of the backend table; the Renderer facade keeps its API and selects a backend
3. render/soft/ implements the SAME plan against ../softgl -- stages it cannot execute refuse
   and publish -> neutral; the C++ twins fill the LUT resources CPU-side
4. the second row is the PROOF of the first: no shared header may name an SDL type once the
   split lands (the audit that guards it is the same shape as run.sh --audit)