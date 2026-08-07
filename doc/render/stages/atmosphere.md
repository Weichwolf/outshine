# Atmosphere — the LUT chain

**Passes:** `TransmittanceStage` · `MultiScatterStage` · `SkyViewStage` · `SkyStage` ·
`IrradianceStage` (`sim/src/render/stages/`). **One document because it is one chain** — each LUT is the
next one's input, and none of them is judgeable on its own.

```
Transmittance ──▶ MultiScatter ──▶ SkyView ──┬──▶ Sky   (the dome the camera sees)
                                             └──▶ Irradiance (the two numbers every lit surface uses)
```

Neighbours: [`../renderer.md`](../renderer.md) §4 (which today carries the implementation detail — the
resource table, `AtmoBuf`'s layout, the constants, the day factor, the init-order contract),
[`../clouds.md`](../clouds.md) (the deck that shares the same air), [`celestial.md`](celestial.md) (sun
and moon, drawn additively right after the dome), [`terrain.md`](terrain.md) and
[`tonemap.md`](tonemap.md) (the two consumers of the irradiance).

## Spec

| Contract | Why |
|---|---|
| **Hillaire 2020** is the technique, LUT for LUT | the algorithms in this field are solved; value is in integration ([`../../conventions.md`](../../conventions.md)) |
| the chain is **compute for the LUTs, one fullscreen draw for the dome** | the LUTs are camera-independent or nearly so, and recomputing them per pixel is the thing the LUT exists to avoid |
| **ONE scene-referred scale.** Radiance is computed in the units the LUTs are in — top-of-atmosphere solar irradiance = 1 — and exactly one exposure is applied on the way to ACES | two independently fitted scales is what put zenith sky and sunlit ground **2.5–3.6 EV** apart. There is now one number to move ([`tonemap.md`](tonemap.md)) |
| the ground's ambient **IS** the integral of the sky the sky pass draws | not a second constant fitted against it — that is what `IrradianceStage` exists for |
| the multiple-scattering LUT is **not optional** | without it the sky-view march is single-scatter only, which measures **1.4 EV too dark at the zenith** and leaves ground and sky on two different scales however the ground is fitted |
| **ONE atmosphere** for dome, terrain and cloud deck | one σ₀ from one weather sample, one inscatter colour, in one shared function both shaders splice ([`../clouds.md`](../clouds.md)) |
| the sky-view LUT **wraps in azimuth** and its seam sits at the sun's own azimuth | so it is sampled with the LUT sampler (`Repeat` in U), never the tile sampler — a filtered seam through the brightest part of the far field is the failure this prevents |
| stages are configured in **dependency order** | WebGPU bind groups pin a concrete `TextureView` at creation; there is no „rebind later" |

## State

**Built, and it is the furthest advanced part of the renderer.** Transmittance, sky-view and sky landed
with the stage split (`c9206eb`…`2099cb0`, see [`../renderer.md`](../renderer.md) `## State`).

`MultiScatterStage` and `IrradianceStage` exist in the working tree and are **uncommitted** (`git status`
reports both untracked at the time of this split; the round that builds them is running concurrently).
Their own sources state what they do:

| Stage | What it produces | Numbers it carries |
|---|---|---|
| `MultiScatterStage` | LUT 2 of 3, Hillaire 2020 eq. 5–7, parametrised by height × sun cos θ like the transmittance LUT it reads | `kSide = 32` |
| `IrradianceStage` | the two irradiances every lit surface needs — direct normal and diffuse on horizontal — in the LUT's own units | compute-only, one workgroup, once per frame; rides in the existing sky-view compute pass, so the per-frame render-pass count is unchanged |

**The commit anchors and the frame measurements for the two new stages are owed by the concurrent
round** and are deliberately not guessed here.

## Gaps

- **[`../renderer.md`](../renderer.md) §4 says „two compute LUTs plus one fullscreen sky pass" and the
  code now has three LUTs plus an irradiance reduction.** That section is the implementation detail this
  file should own; it was left in place by the split that created this file (the renderer document was
  out of scope for it). **Migrating §4 here, and the stage catalogue's atmosphere rows with it, is
  outstanding work** — until then the detail is there and the contract is here.
- **The transmittance LUT is recomputed every frame** although it is parametrised only by height and sun
  cos θ. Its own source carries the note as a TODO: cache it while the sun is static. Unmeasured, so the
  saving is unknown.
- **No aerial-perspective LUT.** Hillaire's fourth LUT is absent; terrain haze is evaluated analytically
  in `AtmoHaze.h` instead. Whether that costs accuracy at range is unmeasured, and the two formulations
  have never been differenced against each other.
- **Nothing measures the chain against a reference.** [`../visual-target.md`](../visual-target.md) §1.3's
  matched-pair harness would settle sky brightness, colour and the zenith/ground ratio in one number, and
  it does not exist. The 1.4 EV and 2.5–3.6 EV figures above are what wrong looks like when it is finally
  measured — there is no standing gate that would catch the next one.
- **The two scale heights owe their citation.** [`../clouds.md`](../clouds.md) states the requirement:
  the molecular (~8 km) and aerosol (~1.2 km) terms must be **published and cited**, and the rule that
  divides σ₀ between them argued rather than tuned. That is that file's Spec and this chain's dependency.

## Knowledge

The derivations, the resource table, `AtmoBuf`'s layout, the Hillaire constants, the day factor and the
init-order contract are in [`../renderer.md`](../renderer.md) §4 today and are stated **once**, there.
This file states the contract; the measured errors that motivate two of its rows — 1.4 EV single-scatter
zenith deficit, 2.5–3.6 EV sky-versus-ground split — are in `## Spec` with the decision they force.

The exposure that converts this chain's units to a displayed image is derived in
[`tonemap.md`](tonemap.md) `## Knowledge`.
