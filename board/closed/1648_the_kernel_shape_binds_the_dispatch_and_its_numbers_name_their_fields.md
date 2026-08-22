Type: bug
Area: render
Tags: hygiene, tests

# The kernel shape binds the dispatch and its numbers name their fields

The shape statics (660efaf9, board:1634) made the pipeline shape the stage's ONE declaration —
and then two of three dispatch sites decline to read it. At HEAD:

- src/render/stages/MediumRadianceStage.cpp:123 divides `kSkyViewLutHeight` by
  `KernelShape.GroupX`; the declared `GroupY` is never read at this site.
- src/render/stages/MediumMultiScatterStage.cpp:92-93 divides both axes by `GroupX`.
- src/render/stages/MediumTransmittanceStage.cpp:80-81 does it right (width/GroupX,
  height/GroupY) — the correct form stands one file away.

Correct today only because every shape spells 8×8. The moment a stage tunes an asymmetric
group (the 200-wide sky-view LUT is the obvious candidate), the y-grid under- or
over-dispatches rows and nothing refuses — exactly the drift class the shape statics were
built to kill, reintroduced one line below the declaration.

Second face of the same defect: every shape is positional aggregate init —
`ComputeShape KernelShape{2, 0, 1, 1, 8, 8, 1}` (MediumRadianceStage.h:18) and six siblings.
Seven bare numbers against seven fields; a swapped Samplers/UniformBuffers or GroupX/GroupZ
compiles clean and misdeclares the pipeline. C++20 designated initializers exist for exactly
this: `{.Samplers = 2, .ReadWriteTextures = 1, .UniformBuffers = 1, .GroupX = 8, .GroupY = 8}`.

Demanded: the y term divides by `GroupY` at both sites, and every ComputeShape/DrawShape
static takes designated initializers. board:1647's medium slice touches these files — the fix
rides with it or lands first, it does not wait behind it.

---

Closed: the height axes of the multi-scatter and radiance dispatches divide by GroupY (the
declared field was dead -- the 1634 sweep's own defect, caught in one round); all seven
shapes are designated-initializer literals now, so a swapped field no longer compiles
quietly. Proving test: test/unit/render/EveryAssembledKernelCompilesOnTheDevice.cpp -- the
slot-parse (below) would catch a shape whose counts drift from the MSL, and the dispatch
shares the same fields it compiles with. 127/127.
