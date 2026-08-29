Type: bug
State: active
Area: test, door
Tags: measured

# A corpus case drives the door

**Benchmark** — Unreal: its automation tests drive the ENGINE (`UWorld`, `FSceneView`,
`FAutomationTestBase`) and never a render module's internals, which is why a renderer rewrite does
not touch them. RAGE: the same, one layer down -- a test harness stands on the game layer's public
surface. **They agree**, so the matter is closed: a conformance case exercises what a client can
reach, or it is not measuring the product.

MEASURED:

    test/harness/shared/render/Parity.cpp        2430 lines
    the whole render harness                     6629 lines
    apps/demo/src/main.cpp (a real client)        195 lines

    #include in Parity.cpp                         41
    of those, from include/ -- the door             0

Not one. The tree's ONLY oracle -- the corpus a case fails because the code is wrong -- reaches
`Render::SubjectProxy`, `Render::SceneRenderer`, `Gltf::Subject`, `Gltf::Document`,
`Gltf::ResolveSurfaceTable`, `Render::SurfaceTable` and thirty-five more internal headers, and
never `Engine`, `Scene`, `View`, `Camera` or `Renderer`.

**THIS IS THE FIRST TRAP IN CLAUDE.md's OWN TABLE**: *a gate blind to a path -- vendor cases green
while engine cases are red, because the harness bypasses the engine's own submission*. It was
written down and then not applied to the one harness where it costs most.

THREE CONSEQUENCES, ALL MEASURED TODAY:

- The corpus went RED-BY-BUILD on two commits in a row (d768f13e, 1a1a68b6) that changed nothing a
  client can see -- a type alias and a file move. Neither would have touched a harness that spoke
  the door. Both were committed unnoticed because the runs that followed them were `harness/claims`
  and `outshine/places`, and neither of those compiles this file.
- The line count is the door's own instrument (CLAUDE.md: *a client is almost no code and its LINE
  COUNT measures the door*). 2430 lines to load a glTF, place a camera and render one picture says
  the door cannot do it, and the harness has been carrying the difference.
- The parity oracle therefore states that the RENDER INTERNALS agree with Khronos, not that
  outshine does. Whether the engine's own submission path draws these files at all is untested.

## WHAT THE HARNESS REACHES FOR, AND WHAT THE DOOR ANSWERS

Measured by listing every internal call the file makes. This is the door's shopping list, and each
row is a benchmark question rather than a wish.

| the harness reaches | the door today | the name it should wear |
|---|---|---|
| `ReadSceneLinear` | nothing -- `readPixels` is 8-bit sRGB | Filament's `readPixels` takes a pixel FORMAT, so a float one is the same verb |
| `ReadDepth` | nothing | the same verb, naming the depth attachment |
| `ReadShadingNormal` · `ReadSurfaceIdentity` · `ReadSceneVelocity` | nothing | Unreal's buffer-visualisation targets, named |
| `SubjectDrawCount` · `SubjectBatchCount` | nothing | Unreal keeps them in `FSceneRenderer`'s stats; Filament has none. **Ours**, and the reason is that board:1943's whole claim is a draw count |
| camera: orthographic, near, far | `Camera{Placed, Stands, FovDeg}` | Filament's `Camera::setProjection(Projection, ...)` carries kind, near and far |
| `studio.Around` · `IndirectLight` | `Lighting::IndirectLight` -- **already there** | Filament `Scene::setIndirectLight` |
| `studio.Lit` · `Lights` | `Lighting::Key` -- **already there** | Filament `Scene::addEntity` |
| `studio.Emits` | `Material::Emissive` -- **already there** | |
| `studio.Posed` | a scenario's animation -- **already there** | |
| `renderer.Init` · `DeviceUsable` · `RenderFrame` · `SettleFrames` | `Engine::renderer()`, `render`, `settleFrames` -- **already there** | |

Half of it the door already answers and the harness reaches past anyway, which is its own finding:
the file was written against the internals and never revisited. The other half is real and is what
this item buys.

- [ ] A Khronos case reads a file, places a camera, renders, and compares -- through `include/`
      alone, and the driving part of it fits on a screen
- [ ] The scoring stays as long as it needs to be: an EXR oracle, a p99 delta and an acceptance
      class are the corpus's own work and are not the door's business
- [ ] `Parity.cpp` names no header outside `include/`, and a claim holds that count at zero
      proof: the negative control is one internal include, which turns the claim RED
