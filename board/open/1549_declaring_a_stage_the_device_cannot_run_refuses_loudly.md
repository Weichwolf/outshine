Type: bug
Area: render
Tags: instrument, bug

**Declaring a stage the device cannot run REFUSES LOUDLY**

**Measured while answering the reviewer's second-biggest lever, the sky.** `Stage::Sky` is in the
catalogue as `Provenance::Content`, so a consumer declares it -- which is exactly right, and
`Live` now can. The plan **compiles without complaint**. The picture does not change: opaque
fraction 49.03 % with the sky declared against 49.04 % without it.

**`src/render/Renderer.cpp:412` is why:**

```
case Stage::Sky:   case Stage::Sun:      case Stage::Moon:
case Stage::Stars: case Stage::Terrain:  case Stage::Buildings:
case Stage::Water: case Stage::Models:   case Stage::AmbientOcclusion:
  error = "this device layer does not execute the stage";
```

**Nine catalogued stages the device layer does not run.** The error is written and **it never reached
the caller** -- the run printed no refusal at all, and a picture that silently omits what it was asked
for is the exact shape `CLAUDE.md` forbids: *a missing producer is a REFUSAL*, and *a failure is loud*.

## THE REFUSAL SPEAKS NOW, AND IT NAMES A DIFFERENT STAGE

`Renderer::Init` already logged the stage by name and then **returned void**, so the caller learnt
nothing and `Live` reported *"the device did not come up"*. It keeps a refusal now -- `WhyNot()` --
and `Live` passes it through. Declaring the sky again gives:

```
REFUSED this device layer does not execute the stage 'mediumTransmittance',
        which the catalogue offers and the consumer declared
```

**Not `sky`.** The compiler pulls the plan backwards, so declaring `Stage::Sky` requires the
atmosphere machinery behind it, and **`MediumTransmittance` is where the device stops**. That is a
sharper statement than this item opened with: the sky is not one missing stage, it is a chain, and the
first link is named.

**Three mute refusals found in one session, all the same shape**: a `Sink` that discarded failing
claims, a `ReadScenario` error nobody printed, and this -- a `void` return over an error that had
already been written down.

## THE SKY'S TECHNIQUE IS ALREADY CHOSEN, AND IT IS PUBLISHED

**The catalogue's own stage names are Hillaire's**, looked up rather than recalled: *A Scalable and
Production Ready Sky and Atmosphere Rendering Technique*, Sebastien Hillaire, Computer Graphics Forum
2020. Its chain is exactly the one this engine's diagram draws:

| the paper's LUT | parameterised by | the catalogue's stage |
|---|---|---|
| Transmittance | sample height and direction, toward the top of the atmosphere | `MediumTransmittance` -> `TransmittanceLut` |
| Multiple Scattering | sample height and sun direction | `MediumMultiScatter` |
| Sky View | latitude and longitude about the camera | `MediumRadiance` -> `SkyViewLut` |

**So nothing about the sky needs designing.** The stage rows, their resources and their order were
written from a published technique whose whole point is that it is cheap, physically based, needs no
high-dimensional LUTs, and scales from mobile to desktop. What is missing is the compute passes
themselves -- `MediumTransmittance` is a `PassKind::Compute` producing one texture, and it is the
first link because the other two read it.

**And the refusal now walks the chain for us.** Implement one, declare the sky again, and the device
names the next stage it cannot run. That is a work list the engine hands out by itself.

## What must be true

- [ ] **Declaring a stage the device cannot execute refuses at plan time**, by name, before a frame is
      drawn -- not silently at encode time
- [ ] **The catalogue and the device layer agree**, and a `static_assert` or a test says so rather than
      a reader noticing
- [ ] **The nine are either built or the catalogue stops offering them** -- *a dead path is worse than
      a missing one*

## Comments

**This turns the reviewer's B3 from a one-liner into honest scope.** *"Kein Himmel"* is not a flag
somebody forgot to set: `Sky`, `Sun`, `Moon` and `Stars` are unimplemented in the device layer, and the
sky chain the architecture diagram draws -- Transmittance, MultiScatter, SkyView, Irradiance -- has
`Provenance::Machinery` rows that would be pulled in behind them.

**And it explains why `Clients::Sim` is dead code**: it stands up terrain, buildings, water, moon and
stars, and every one of those stages is on this list.
