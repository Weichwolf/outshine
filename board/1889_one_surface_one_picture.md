Type: bug
State: open
Area: render
Tags: measured, colour, door

# A window and an offscreen canvas show the SAME picture

The same scenario, the same frame index, the same build, drawn twice:

```
build/outshine-viewer --show apps/driver/src/f31.scenario --frames 3 --into DIR
build/outshine-viewer --show apps/driver/src/f31.scenario --frames 3 --into DIR --headless
```

The windowed still is dark and warm -- the browser chrome reads brown-on-black, the car reads
near-black with a bright shoulder. The headless still of the same declaration is light and cool
-- the chrome reads blue-grey, the selected row is a saturated blue, the car reads mid-grey.
They are not the same picture.

The client hands in ONE surface and reads pixels back from it; that surface is the picture, and
which of the two ways it was obtained may not change what it holds.

The likely seam is the format. `Renderer::SurfaceFormat()` answers
`SDL_GetGPUSwapchainTextureFormat` when a window presents and the plan's declared format when
it does not, and the two differ in whether the hardware applies the sRGB transfer on write. One
of the two pictures has the transfer applied twice or not at all.

## What will be true

- [ ] A scenario drawn to a window and to an offscreen canvas of the same extent yields stills
      that agree within the tolerance the corpus already uses for a render case.
- [ ] Whichever path is wrong is named: the transfer is applied ONCE, at the place the plan
      declares it, and the surface format is not two different answers to one question.
- [ ] Proving case: one scenario, two canvases, compared. Negative control: force the offscreen
      format to the swapchain's and the difference moves rather than vanishing, which is how a
      format mismatch is told from a tonemap that runs twice.
