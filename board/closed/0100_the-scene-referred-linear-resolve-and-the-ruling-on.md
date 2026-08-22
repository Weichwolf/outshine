Type: feature
Area: render
Tags: instrument

**The scene-referred linear resolve, and the ruling on the fused pass**

*This is `board/active/`'s long-unbuilt "scene-referred linear readback" under its real name, and it is the
prerequisite the plan snags on: **a plan without `Taa` and `Exposure` has no path from `HdrTex` to
`FrameTex` at all**, because the temporal resolve and the tonemap are one fused fragment and its second
attachment is the only writer of `frameView` (`render/Renderer.cpp:780-805`, `stages/TaaStage.cpp:107-125`).*

- [x] **Ruling: the fusion survives as an implementation and dies as an interface.** Three declared stages — `TemporalResolve` (writes `SceneLinear`), `Tonemap` (reads `SceneLinear`, `AoTex`, `Meter`; writes `FrameTex`) and the existing `ExposureMeter` compute — and the compiler re-fuses the first two under **R2**. A stage plan that has to special-case its only path to the framebuffer is describing a defect; a compiler rule that fuses any qualifying pair is not a special case (`render/plan/RenderCatalogue.h:219-226`, `render/plan/RenderPlan.cpp:223-227`; `test/outshine/unit/render/plan/APlanIsPulledFromWhatItRequests.cpp` — the coverage plan reaches `frameTex` with no resolve at all)
- [x] **No new render work and no new texture.** `SceneLinear` **is** the resolve's attachment 0, the texture that exists today — 1280 × 720 × 8 B = **7.372 800 MB** at `RGBA16Float`, which is what `Renderer.cpp:111-112` unconditionally selects. What is new is a name in the catalogue, a readback shaped like `ReadDepth`, and the split of one fragment into two declared stages (`render/Renderer.cpp:918-959`, `ReadSceneLinear`; held by `test/harness/shared/render/Parity.cpp:315`)
- [x] **What `SceneLinear` contains is written down, because a parity comparison must know which image it got**: resolved linear radiance **before** the screen-space occlusion composite and **before** the metered exposure multiply — `stages/TaaStage.cpp` writes `o.history = scene` and applies both to `o.surface` only (`render/plan/RenderCatalogue.h:156-160`, on the row itself)
- [x] **`SceneLinear` declares `FallsBackTo SceneHdr`**, one field in the `constexpr` catalogue, so a picture plan without `TemporalResolve` still reaches the tonemap without a full-screen blit that exists to copy. It earns its place for one named consumer: the TAA-off run that is the deletion question's measurement instrument (`render/Renderer.cpp:576-579`). **The compiled plan publishes every alias it applied** — that record is the difference between a declared identity and a default nobody sees (`render/plan/RenderCatalogue.h:83,159-160`, `render/plan/RenderPlan.cpp:83-87`; `test/outshine/unit/render/plan/APlanIsPulledFromWhatItRequests.cpp` — *"the plan publishes the one alias it applied"*)
- [ ] *The alternative was considered and is refused with its reason:* a `Role::SceneLinear` slot bound by *last writer wins* reintroduces order-dependence into an order the compiler derives from the graph, and would need a cycle check to be well-founded. One catalogue field plus a published list is cheaper and checkable
- [ ] **Radiance parity gets its zero point from `SceneLinear` and not from a PNG** — § I.26's rung 3 needs a linear tap, and the tap is this resource


---

**Closed by the backlog adjudication, tranche 2 (2026-08-22).** ReadSceneLinear, the SceneLinear resource with its fallback, and the parity tap all stand.
