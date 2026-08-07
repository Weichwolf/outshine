# TileLights — the night-light field

**Pass:** `TileLightsStage` (`sim/src/render/stages/TileLightsStage.{h,cpp}`).
Neighbours: [`terrain.md`](terrain.md) (drawn after it and depth-tested against it),
[`celestial.md`](celestial.md) (the other night-time source, and the same self-gating pattern),
[`../../world/terrain.md`](../../world/terrain.md) (where the light positions are streamed from).

## Spec

| Contract | Why |
|---|---|
| **instanced additive sprites**, one draw | a town is thousands of point sources and none of them owns a pixel |
| positions are **streamed and placed by `World`**, relative to one ECEF anchor | `render/` may not grow a tile client of its own |
| **depth-tested against the terrain** | a light behind a ridge is occluded; an additive sprite that ignores depth is a lamp shining through a hill |
| **self-gates** like the stars | no lights, no draw, no cost |
| the sprite has a **world radius**, and its screen size is clamped to a floor in pixels | a sub-pixel additive point aliases into a crawling speckle; a floor with an energy-preserving falloff is the standard remedy. **The derivation:** the farthest any point can lie from the nearest pixel centre is √2/2 = 0.70711, so a quad whose inscribed radius reaches that always covers a sample; the energy is divided straight back out (`gain = 1/(gw·gl)`) so the floor changes the SIZE and never the flux |

## State

**Built**, landed with the stage split (`c9206eb`…`2099cb0`, see [`../renderer.md`](../renderer.md)
`## State`). Instance data is `[posRelAnchor.xyz, worldRadiusM, colorPremul.rgb]` — 7 floats per light.
The screen radius is clamped to **1.3…4.0 px** (`[SET]`, in the shader).

Source of the lights: the OSM vector layer `fb-tiles` already serves (`tiles/src/lights.c`).

## Gaps

- **A light has no type.** Sodium, mercury, LED, a lit window and a headlight are five colours and five
  intensities, and the pass carries one premultiplied colour per instance with nothing deciding it. That
  is the same gap [`../classification.md`](../classification.md) names on the ground: the raster is being
  drawn instead of being used as an index.
- **The epoch does not reach it.** A 1900 street is gas-lit and a 2026 street is not; the epoch parameter
  ([`../../goal.md`](../../goal.md)) is exactly the dial that should decide, and it does not exist in the
  code at all.
- **The pixel floor is `[SET]`.** 1.3 px and 4.0 px are asserted; no measurement establishes where the
  aliasing actually starts at 720p, and [`../gpu-determinism.md`](../gpu-determinism.md) warns that
  coverage at that scale is the least specified part of the raster path.
- **Lights do not light anything.** They are emissive sprites; no surface near them is brighter for it.
  Whether that matters at the altitudes this engine draws is unmeasured, and at 1.70 m it plainly does.

## Knowledge

Nothing is derived here yet. The two clamp values are `[SET]` and stated in `## State`; the scene-referred
scale their colour is expressed in is derived in [`tonemap.md`](tonemap.md) `## Knowledge`.
