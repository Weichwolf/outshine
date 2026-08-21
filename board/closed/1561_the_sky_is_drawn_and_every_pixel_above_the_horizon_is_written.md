Type: task
Parent: 0120
Area: render
Tags: bug

**The sky is drawn, and every pixel above the horizon is written**

The fourth link, and the first VISIBLE one: `Stage::Sky` -- a fullscreen raster pass sampling the
`SkyViewLut` per pixel through the reference's dome mapping (up-projected azimuth against the sun,
the non-linear v with the horizon on the middle row), scaled by the declared sun illuminance,
writing colour and the static velocity marker. The reviewer's finding #1 -- 51.5 % of a driving
frame alpha-zero, "not sky, unwritten framebuffer" -- is what this closes for every scenario that
declares the sky.

| | |
|---|---|
| `src/render/stages/SkyStage.{h,cpp}` | the raster stage; depth test and write off, drawn before the geometry passes |
| `src/render/Renderer.cpp` | **a target is CLEARED by the first pass that touches it each frame and LOADED after** -- without this the subjects pass wiped the sky it followed |
| `src/clients/Live.cpp` | `DrawsSky` declares the whole chain: `SetMedium` + `SetSky`, sun from the key light's elevation and bearing through `EcefFromGltf` into the engine frame, illuminance = `KeyLux` |
| `src/clients/Live.h` | `ReadPixels` -- the screenshot's own readback, published so a test can measure a frame without a PNG round trip |
| `tools/driver/stills/...` | the drive now declares the sky |

Proof: `test/render/outshine/scenario/TheSkyStandsOverTheGroundAndItIsBlue.cpp` -- a two-triangle
ground and a declared sky, one frame: **all 57 600 pixels of the top quarter written**, mean
(R 42.1, G 69.4, B 112.7) of 255 -- blue leading red by 2.7x after the tonemap -- and the ground row
differs from the sky row across the width, so the horizon is in the picture.

## Comments

**The refusal chain worked exactly as built**: the first run refused loudly -- *"the stage 'sky' did
not configure: the sky did not compile"* -- where before board:1549 it would have silently drawn
nothing. The cause: `%.9g` prints -10000 without a decimal point, and the appended `f` made
`-10000f`, an invalid MSL literal. `%e` always carries the exponent form. SDL_GetError was empty;
the Metal validation layer's own log carried the message -- worth remembering, the layer speaks
when SDL does not.

The eye height passed to the LUT is the ground lift itself (10 m): between 0 and 80 m of eye height
the horizon dip changes by under 0.26 deg, below the seam the drawn terrain covers. When a scenario
one day flies, the height comes from the camera and this line moves.
