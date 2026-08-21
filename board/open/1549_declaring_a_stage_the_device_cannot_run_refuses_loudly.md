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
