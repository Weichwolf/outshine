Type: feature
State: open
Area: render
Tags: sky, generators, optimisation

# The sky is GENERATED and cached; the sun, the moon and the stars are not in it

**Benchmark** — Unreal: `SkyAtmosphere` caches the atmosphere in a sky-view LUT and a distant-sky light probe, and draws the SUN DISC and the stars in the forward pass against that; `VolumetricCloud` renders to its own low-resolution target with reprojection. RAGE: a low-resolution sky target with the celestial bodies composited over it. **Taking Unreal**, and the reason is a number rather than a preference — see below.

## What is proposed and what is taken

A GENERATOR that writes the sky and refreshes it, instead of four raster stages (`sky`, `sun`,
`moon`, `stars`) each drawing every frame. The generator tier is the right home: it has its own
door, a client registers its own beside them, and a sky is exactly the kind of thing a generator
makes -- one concrete output from a parameterisation.

**Taken: the ATMOSPHERE and the HIGH CLOUDS.** Both are smooth, low-frequency and expensive to
integrate, which is what a cache is for.

**Refused: the SUN, the MOON and the STARS.** They are point-to-small features and a cube map
destroys them. A face of N pixels covers 90 degrees, so it holds N/90 pixels per degree, and the
sun and moon are 0.53 degrees across:

    face      sun is        six faces, RGBA16F
    512^2      3 px          12 MB
    1024^2     6 px          50 MB
    4096^2    25 px         805 MB

A six-pixel sun with cube filtering is WORSE than the analytic disc this tree already draws, and
stars are worse still: one- to two-pixel point sources crawl across the texel grid and twinkle as
the camera turns. Unreal keeps both in the forward pass for this reason. They cost a fullscreen
pass each and they are not where a frame goes.

## What ALREADY stands, and must not be built twice

`Resource::SkyViewLut` is a derived resource written by `Stage::MediumRadiance`, and
`MediumTransmittanceStage` carries `Settled_` -- it recomputes only when the medium changes. **The
caching this item asks for already exists for the atmosphere.** What does not exist is a cloud
layer, and that is where the work is.

## THE REFRESH RULE IS A THRESHOLD, NEVER A CLOCK

A period was proposed -- sixty seconds -- and it is the wrong shape. The sun moves 0.25 degrees a
minute, which over a 60-degree field at 1280 px is **5.3 pixels a minute**. A sun five pixels from
where its own shadow says it stands is visible in a still frame, and the whole reason this engine
computes the sky from place and clock is that a sky which disagrees with its shadows is the defect.

The rule is: refresh when the SUN HAS MOVED MORE THAN HALF A PIXEL at the current field of view, or
when the medium changed. That is `Settled_`'s shape, already in the tree, and it is self-correcting
under a fast clock where a period is not.

## THE WORD

CLAUDE.md refuses a SKYBOX and the refusal stands, because it is about a submitted IMAGE: a client
hands in a photograph of a sky and it argues with its own shadows the moment the clock moves. What
this item builds is the opposite -- the engine computes the sky from where and when, and caches its
own answer. Calling that a skybox would make the rule read backwards to a stranger, and a name is a
promise. The cached thing is a SKY, and the refusal keeps its subject.

## What is measured

- [ ] the sky and cloud stages' GPU cost, BEFORE the generator is written -- the CPU encode is
      0.037 ms for `sky`, which says nothing about the device, and a cache that saves under a
      millisecond of a 31 ms frame is not worth a tier
- [ ] the cached sky is refreshed on a THRESHOLD and a case moves the clock fast enough to make a
      period-based rule visibly wrong
- [ ] the sun disc and the stars still come from the forward pass, and a picture at 1280x720 shows
      the sun round rather than square

## What this does NOT cover

Shibuya's frame is 31.45 ms and 9.4 M triangles of buildings. The sky is not where it goes, and
this item does not claim otherwise: board:2041's next stage -- occlusion against a HiZ pyramid --
is what the measurement points at. This one is for when clouds arrive.
