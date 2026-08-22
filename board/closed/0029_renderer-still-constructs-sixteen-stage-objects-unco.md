Type: bug
Area: render
Tags: instrument

**`Renderer` still constructs sixteen stage objects unconditionally, and `RenderFrame` is 170 lines**

`src/render/Renderer.h` — sixteen members of the form `std::unique_ptr<T> X = std::make_unique<T>();`.
The plan now decides what is *created on the device* and what is *configured*, which was the expensive
half; the object graph still does not follow the plan, so a renderer that draws a depth buffer for a
coverage mask holds a `TaaStage`, a `StarsStage` and a `MoonStage`. `R.5`, `C.41`. The consequence is not
bytes: it is that `View(Resource::SceneLinear)` → `Taa->Output(FrameNo)` and `Light()` →
`Shadow->AtlasView()` (`Renderer.cpp:298,446`) are **callable on stages the plan does not hold**, and
answer with a null view instead of failing to compile. The catalogue's read edges are what makes every
such call correct today; nothing in the type system does.

`RenderFrame` is 170 lines, down from 310 — camera basis, jitter, ephemeris, atmosphere update,
frame-context assembly, caster collection, the pass loop and the history swap. `F.3`, `F.2`.

**And `View()` still creates a texture view per call**, at eleven sites (`Renderer.cpp:290-300`), so
every colour and depth attachment of every pass allocates one per frame — `Per.14`, `Per.15`. The count
fell sharply when `AttachmentSet` replaced the per-stage loop (the walk-like scene pass went from 24
views a frame to 3), which is why this is now a shape finding and not a cost: **no measurement of it
exists and `Per.6` forbids claiming one.** Right: create each view once, where the plan says the
resource exists, and let `View()` return a handle.

**Band 3 — waits for the SDL_GPU port**, which rewrites every one of these sites; repairing them first
would be repairing code about to be deleted. **Fixed when** a stage object exists because the plan holds
its stage, so a call into an unheld stage does not compile.


---

**Closed by the backlog adjudication (2026-08-22).** The band-3 condition (SDL_GPU port) arrived: no unconditional stage members, RenderFrame is 35 lines, stages configure through the executor table per plan.
