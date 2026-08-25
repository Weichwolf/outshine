Type: bug
State: open
Area: clients, scenario
Tags: viewer, measured, door

# A selected case DRAWS, and the stage is where it draws

Selecting a case in `apps/viewer` loads it -- the title bar reads `render/khronos / Box`, the
status line reads `Box`, the previous subject is gone and the case's own fill is on the stage --
and NOTHING is drawn there. Measured at c2ac6bd9 + the stage work, with
`build/outshine-viewer --show apps/driver/src/f31.scenario --case Box --frames 4`.

Four things were tried and none of them was it:

| tried | result |
|---|---|
| `Scenario::RenderPlan::Picture` reaching `SetPictureRegion` | the region arrives, the stage is dark and empty |
| a declared `Lighting` (40000 lx key, lifted environment) | unchanged |
| `Engine::Assemble()` after the re-`Declare` | unchanged |
| `Live::FrameItself()` | it has NO caller in the tree and never had one; `HaveEye_` is already false on a fresh stand, so it resets nothing |

`Engine::Declare` returns true -- the viewer prints its refusal and there is none. So the
subject is accepted and the frame does not carry it.

## What will be true

- [ ] A case selected in the viewer is DRAWN on the stage, at a size that fills it.
- [ ] The proving run names the case and the frame: `--case Box --frames 4` and frame004.png
      carries the box.
- [ ] Whatever swallows the subject is NAMED. A declaration the door accepts and does not draw
      is the same defect class as board:1862's five accepted-and-never-advanced declarations,
      and it is the FIFTH dead capability this week after PresentFrame, ShowOffscreen,
      AssembleDrive and Declaration::Presents -- so the answer is likely a call nobody makes.
