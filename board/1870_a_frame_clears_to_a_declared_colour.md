Type: bug
State: open
Area: render, scenario
Tags: driver, picture, measured

# A frame clears to a colour the scenario declares, and `fill` is not that colour

**This item named the wrong knob for three rounds and the correction is the whole of it.**
`fill="0.9"` in `apps/driver/src/f31.scenario:4` is not a background. It is the FRAMING fraction
-- how much of the frame a derived camera gives the subject:

    src/engine/Live.cpp:94    double Live::Framing() const {
    src/engine/Live.cpp:95      return Declared_.Fill > 0.0 ? Declared_.Fill : Gltf::kFramingFill;
    src/content/gltf/Framing.h:12   constexpr double kFramingFill = 0.6;

So "the declared fill never reaches the picture" was false: it reaches the camera, which is what
it names. **Nothing in `include/Scenario.h` declares what a frame clears to at all**, and the
colour is nailed into the renderer:

    src/render/Renderer.cpp:669   : SDL_FColor{0, 0, 0, carriesCoverage ? 0.0f : 1.0f};

A literal black with a literal alpha, in the one place a declaration should reach. That is the
defect.

## Measured at a32c4919, the nine stills of the 302 m drive

Decoded channel by channel, not composited by a viewer:

| | |
|---|---|
| alpha values present in the whole frame | exactly two: **255** on 570 941 px and **102** on 350 659 px |
| pixels with `alpha == 0` | **0** -- the transparency this item was filed on is GONE |
| mean of `max(R,G,B)` over 921 600 px | **0.45 / 255** (along01), 1.72 at the brightest (along08) |
| pixels with `max(R,G,B) > 32` | **7 131 of 921 600 = 0.77 %** |

The frame is BLACK. What earlier rounds read as "a silver flank" and scored at mean luminance
15.1 was an image viewer compositing `alpha = 0.4` over its own white page: the colour channel
carried nothing then either. Every luminance figure in this tree taken from a still before
a32c4919 measured a composite and not the picture (board:1893 carries the same correction).

The 38 % at `alpha = 102` is what the windscreen looks out onto -- a transparent black where a
world should be, unaffected by any declaration.

## What will be true

- [x] A drive that cannot be laid leaves the scenario STANDING: the frame renders, the still
      is written, `Engine::Drove()` answers false and no message carries a prefix twice.
- [ ] `<render>` declares the colour a frame clears to, the door carries it as a value, and the
      picture shows it where nothing is drawn. *Something is always drawn* means something with
      a colour.
- [ ] Proving case: a scenario declaring a clear colour renders a frame whose untouched pixels
      ARE that colour, through `include/` alone. Negative control: the same scenario with the
      declaration removed clears to the engine's own default and not to the zeroes of a struct
      nobody filled in (board:1901's rule).
