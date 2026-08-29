Type: bug
State: open
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

- [ ] A Khronos case reads a file, places a camera, renders, and compares -- through `include/`
      alone, and the driving part of it fits on a screen
- [ ] The scoring stays as long as it needs to be: an EXR oracle, a p99 delta and an acceptance
      class are the corpus's own work and are not the door's business
- [ ] `Parity.cpp` names no header outside `include/`, and a claim holds that count at zero
      proof: the negative control is one internal include, which turns the claim RED
