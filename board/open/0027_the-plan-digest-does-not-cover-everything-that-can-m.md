Type: bug
Area: render
Tags: scope

**The plan digest does not cover everything that can move a pixel, and three things move one behind its back**

`src/render/plan/RenderPlan.cpp:239-254`. The digest's material is the stage set, the derived order, the
passes, the merges, the aliases, the held resources with their formats, the display transfer and the
exposure. § I.27 requires it to cover *everything whose change can move a pixel*, because a baseline is
keyed by it and the alternative is a one-token hash edit that looks like maintenance.

- **The frame extent is not in it.** `Renderer::Init(int width, int height, plan)`
  (`src/render/Renderer.cpp:68`) takes the resolution beside the plan, so the plan never learns it.
  A run at 1280 × 720 and a run at 320 × 180 produce **the same digest**, and 320 × 180 is the rung the
  picture is compared at. This is the same missing field as the residency ledger's: the catalogue has no
  extent, so `Renderer::Create` carries `256, 64` and `192, 108` as literals (`:242-246`), the AO buffer
  is silently half-resolution (`stages/AoStage.h:6`, `:24-25`) and the shadow atlas is 4 × 1024²
  (`stages/ShadowSample.h:16-17`) — four resolution classes, none of them expressible.
- **A picture-changing branch sits at a creation site, which is the one thing § I.27 forbids by name.**
  `Renderer::Create` for `Resource::VegetationTable` reads `if (VegRows.empty()) return;`
  (`src/render/Renderer.cpp:238`), so `Plan_->Holds(Resource::VegetationTable)` is **true while the
  buffer does not exist**. *The terrain shader that branched on it went with the SDL_GPU port, so the
  instance is gone and the shape is not: a `Holds()` that is true while the resource does not exist is
  still spellable, and the next resource with a data-dependent creation will re-create it.* Right: the vegetation table is a declared input of the
  plan or the plan does not hold it; the branch belongs in the declaration, not in `Create`.
- **`FB_TAA=0` retires a declared stage from an environment variable.**
  `src/render/Renderer.h:327` — `const bool TaaOn = [] { const char *e = getenv("FB_TAA"); ... }();` —
  and `Renderer.cpp:723,777` disarm the jitter and the history from it. `TemporalResolve` is now a stage
  a consumer declares; this is a second, undeclared way to turn the same thing off, it changes the
  picture, and it changes neither the digest nor `SettleFrames()`. The tree already states the rule
  against itself in the header that carried the rule until the port deleted it — *"NO ENVIRONMENT GATE.
  An environment variable is not an interface"*. `I.2`, `I.3`.

**Fixed when** two declarations that produce different pixels produce different digests, demonstrated by
the three cases above, and `getenv` appears nowhere under `src/render/`.
