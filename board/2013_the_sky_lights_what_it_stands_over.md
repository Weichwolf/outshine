Type: bug
State: open
Progress: gpu-driven
Area: render
Tags: measured, benchmark

# The sky lights what it stands over, and today it only sets the exposure

**Benchmark** — Unreal: `SkyLight` captures the sky and delivers it to every surface as an ambient
term, directional rather than flat; Lumen replaces it with a bounce but the sky term predates it by
a decade and shipped on far weaker hardware. RAGE: an ambient probe from the sky plus a small set
of baked terms, again per-direction. **Both agree that the SKY is the cheapest indirect light there
is and both spend it on surfaces.** Taking that.

## The capability is built, and it reaches the wrong consumer

The catalogue runs the full Bruneton chain -- `MediumTransmittance` -> `MediumMultiScatter` ->
`MediumRadiance` -> `SkyViewLut` -> `Irradiance` -> `IrradianceBuffer`, nineteen rows of it. And:

    {Stage::AutoExposure, Provenance::Content, PassKind::Compute, "autoExposure",
     {Resource::IrradianceBuffer, kNoEdge}, {Resource::Meter, kNoEdge}, {kNoEdge}, kNoFusion},

`IrradianceBuffer` has exactly ONE consumer and it is the exposure meter. What the subject pass
gets instead:

    struct SubjectEnvironment {
      double RadianceLinear[3] = {0, 0, 0};
    };

**Three doubles. One colour, for the whole scene, from any direction.** So a physically derived,
direction-dependent sky irradiance is computed every frame and a flat constant is what lights the
geometry.

This is the tree's commonest defect once more: a complete capability no declaration reaches --
except here it is reached, by the one consumer that needed the least of it.

## Why this is the first rung and not a nice-to-have

`AmbientOcclusion` stands in the catalogue, so the tree can DARKEN a crevice. Nothing FILLS it. An
old-town alley at midday, a canyon's shaded wall, a forest floor -- every one of them is lit almost
entirely by sky, and every one of them is currently lit by a constant. Reflections (board:2012) and
bounced light from surfaces are the rungs above; this one is already paid for.

- [ ] the subject pass reads the sky's irradiance per NORMAL DIRECTION rather than a scene constant
- [ ] `AmbientOcclusion` attenuates that term rather than a flat one, so the crevice darkens
      against what actually lights it
- [ ] the cost is measured: the irradiance is already computed, so the delta is the bind and the
      lookup and nothing else

**The measurement that would show I am wrong**: if the picture does not change, the constant was
already being set from the same irradiance somewhere upstream and this item is withdrawn. The
control is a scene lit only by sky -- no key light -- where a flat term and a directional one
cannot look the same. `khronos/glTF` is the guard: those cases declare their environment, so a
change here that moves them is a change that is wrong.
