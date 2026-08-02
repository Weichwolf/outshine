# HUD — the backend

**Origin:** moved out of `rendering.md` §7 (state `793e1fe`), taken over unchanged. The **symbology**
(what is drawn) is not here but in `systems/FBDisplaySystem` and its F-16 override — see
[`../sim/systems.md`](../systems.md) and [`../modules/f16/module.md`](../modules/f16/module.md). This file
describes the **backend** (what it is drawn with).

## Spec

The HUD **backend**: how the picture is drawn, never what is drawn.

| Contract | Acceptance / measurement anchor |
|---|---|
| Airframe-agnostic | the geometry buffer, the font and the stage know no aircraft type; symbology lives in `systems/FBDisplaySystem` and its module override |
| Geometry is CPU-side and WebGPU-free | `systems/FBHudGeometry.cpp` is the documented core-lib exception |
| Real antialiasing from area coverage | 8-bit coverage atlas + sharp-bilinear reconstruction, no hard alpha test — coverage-agnostic, unchanged since the 1 bpp era |
| Font is baked, not a build dependency | `sim/tools/bake_hud_font.py` runs only on font/charset change; `FBHudFontRom.h` is generated, `FBHudFont.h` is hand-kept |
| Chip-specific artefacts are NOT here | MAX7456 behaviour belongs in a module hook (`FBF16Max7456`) |
| Public screen units stay stable | `kFontAdvance` / `kFontQuadSize` unchanged across the 8×8 → 16×16 raster growth |

## State

Built; three cleanly separated layers (geometry → backend → font).

| Piece | Status | Anchor |
|---|---|---|
| Generic default HUD in the displays slot | built | `4cb92e8` |
| Coverage AA instead of alpha test; generic font system split from the MAX7456 hook; 16×16 glyphs from B612 Mono | built | `2f3c277`, `8997eec`, `6f160af` |
| F-16 main HUD in the real combiner aperture, legible at 720p | built | `6802a6d`, `d31b1a9` |

The symbology implementation is documented in [`../modules/f16/module.md`](../modules/f16/module.md).

## Gaps

### Open work (from the retired `TODO.md` §4)

| # | Thing |
|---|---|
| 4.5 | the 8-tap HUD glow is missing (`TODO` in `FBHudStage.cpp`) — it is what gives a real combiner its luminance feel. **Load-bearing since the symbology fills the whole windscreen (2026-08-03):** green ink over the white SVS ground is now measurably marginal in the lower half of the window, where the bands and the steering line sit. The MFD bays solved the same problem with a veil; the HUD cannot use one (it must not tint the world) and needs the glow instead |
| 5.4 | no lock / TD-box symbology, because `doc/modules/f16/hud-symbology.md` documents none. It will not be invented (see `../aircraft/f16.md`). |

### Inventory (from the previous `Open points` section)

(see [`renderer.md`](renderer.md) — the collected list of the renderer round is preserved there in
full; the points that belong here are above under Gaps.)


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### The three layers

Three layers, cleanly separated: **geometry** (CPU, airframe-agnostic) → **backend** (WebGPU) →
**font** (generic bitmap system). The **symbology** itself lives outside, in
`systems/FBDisplaySystem::BuildHud` and its F-16 override.

#### 7.1 `FBHudGeometry` (`systems/FBHudGeometry.h/.cpp`)

The reused per-frame geometry buffer in **2D pixel coordinates**; it replaces the old GL-shim globals
(`w3_hud`/`w3_hudT`/`mx_v`) with an owned, resettable buffer. `Reset()` empties without freeing
capacity — after the first frame the (bounded, deterministic) symbology allocates nothing more.

Two kinds of primitive, matching the two pipelines:

| Kind | Layout | Producer |
|---|---|---|
| **Strokes** | `(x, y, d, hw, r, g, b, a)` × 6 per segment | `Line()` (hairline, hw = 0.5 px), `QLine()` (explicit half-width), `Circle()` (n chords), `Box()`, **`Fill()`** |
| **Glyphs** | `(x, y, u, v, r, g, b)` × 6 per character | `Text()`, `Printf()` |

The stroke vertex carries an **opacity** channel since the MFD bays became translucent: the fragment
multiplies its coverage by it, every primitive but `Fill()` passes 1, and `Fill()` is one wide stroke
along its own centre line — flat inside, antialiased only at the edge. It is also why the stroke
stride is 32 B and not 28.

`d` is the signed normal distance of the vertex from the centreline in pixels, `hw` the nominal
half-width of the segment — from those the fragment shader computes analytical coverage. There is **no**
hard `LineList` pipeline any more; every straight run (rails, ticks, carets, boxes, the conformal
horizon) goes through the same AA quad path. Caps are plain butt caps.

Upper bounds on which `FBHudStage` sizes its GPU buffers: `kHudMaxStrokeFloats = 168480`
(the same 3510 quads the 7-float vertex held, re-expressed for the 8-float one — not a new budget),
`kHudMaxTextFloats = 32768`. `Encode()` CLAMPS to both: a fixed vertex buffer plus a `WriteBuffer`
past its end is a device error, and symbology that outgrows the bound must lose its tail, not the
frame.

**Clipping** (`SetClip`/`ClearClip`): for the conformal symbology inside the HUD combiner aperture.
Strokes are **cut** by a Liang-Barsky segment clip (a partially visible stroke emits only its visible
part); glyphs are **discarded entirely** when their content box does not intersect the rectangle — a
glyph is an opaque quad and cannot be subdivided further without a shader change. Geometry outside a
`SetClip`/`ClearClip` pair (tapes, text blocks) is untouched.

#### 7.2 `FBHudStage` (`render/stages/FBHudStage.h/.cpp`)

A pure WebGPU backend: pipelines, buffers, atlas. Once per `Encode()` it asks the **borrowed**
`FBDisplaySystem` to fill an `FBHudGeometry`; the logic lives there, not here.
`SetDisplaySystem(nullptr)` = an empty HUD.

| Detail | Value |
|---|---|
| Pipelines | `Stroke` and `Text`, shared pixel→NDC mapping `scale = (2/W, 2/H)` |
| Blend | `SrcAlpha / OneMinusSrcAlpha` (colour), `One / OneMinusSrcAlpha` (alpha) |
| Colour | The fragment shader **linearises** (`pow(col, 2.2)`) so that the sRGB swapchain view re-encodes to the intended display green on write |
| Pass | LoadOp `Load` on `FrameTex` — the tonemapped picture stays |
| Second use | `EncodeLoadingText()` — loading screen, text pipeline only |

Stroke coverage (analytical box filter across the width):
`alpha = clamp(hw + 0.5 − |d|, 0, 1)`, `alpha <= 0 → discard`. A pixel-aligned 1 px line (`hw = 0.5`)
therefore renders **exactly** like the earlier hard `LineList`; every other angle and every other width
gets a soft edge instead of a staircase.

#### 7.3 `FBHudFont` (`systems/FBHudFont.h` + the generated `FBHudFontRom.h`)

The generic, **airframe-agnostic** font system — every module HUD draws text through it.

| Quantity | Value | Meaning |
|---|---|---|
| `kFontTile` | 16 | edge length of the glyph bitmap (grown from 8) |
| Bit depth | 8 bit **area coverage** (0..255) | real box-filtered coverage, not a 1 bpp mask |
| `kFontGlyphs` | 43 | charset `" 0123456789A–Z-.:/+°"` |
| `kFontTilePad` | 18 = `kFontTile + 2` | 1 texel of transparent **gutter** all round |
| Atlas | `43·18 × 18`, format `R8Unorm`, sampler **LINEAR** | gutter texels stay at their zero init |
| `kFontAdvance` | 4.0 | public screen-pixel unit: character advance |
| `kFontQuadSize` | 6.0 | public screen-pixel unit: drawn tile |

**Provenance of the data:** B612 Mono Bold (SIL OFL 1.1 — Airbus' own cockpit typeface), baked by
`sim/tools/bake_hud_font.py` (Pillow: 8× supersampled, box filter down to 16×16) into the GENERATED
`FBHudFontRom.h`. The script is **not a build dependency** — it runs only on a font or charset change.
`FBHudFont.h` itself stays hand-kept (atlas layout + quad builder).

**Why advance and quad size stayed unchanged:** they are the public units of EVERY caller. Only the
internal raster resolution grew 8×8 → 16×16; text size, baseline and advance on screen stay where they
were. `kFontAdvance = 4` with `kFontQuadSize = 6` means: the drawn quad is larger than the advance but
the ink is narrower (the bake script respects that via `SAFE_X_FRACTION`) — content < advance yields a
clear gap.

**Gutter + quad overhang:** the quad is expanded on each side by **one texel in screen pixels**
(`texel = qs / kFontTile`) beyond the content box, and the UVs cover the full padded tile. That way the
coverage reconstruction sees real zeros outside the bitmap instead of a clamped edge-texel echo:
edge-touching ink reaches full coverage, outside it falls cleanly to 0 — and nothing bleeds into the
neighbouring tile.

**"Sharp bilinear" — real antialiasing instead of an alpha test.** The text fragment shader:

```wgsl
let t  = uv * texSize;              // sample location in texel coordinates
let fw = max(fwidth(t), 1e-4);      // screen footprint of ONE texel
let c  = floor(t - 0.5) + 0.5;      // centre of the texel that was hit
let f  = clamp((t - c - 0.5) / fw + 0.5, 0.0, 1.0);
let coverage = textureSampleLevel(atlas, samp, (c + f) / texSize, 0.0).r;
```

The sample location is thus **warped** in texel space (the fractional part scaled onto the `fwidth()`
footprint, then clamped) before a LINEAR sample. Properties:

- At a footprint of 1 texel this is the **identity** (plain bilinear).
- Magnified it snaps flat, except in a ≈ 1 screen-pixel wide ramp **right at every bit edge** — a
  box-filter approximation of the ideal analytical edge, not a blur.
- The result is **straight alpha**, not a hard `alpha < x → discard`. An alpha test knows only on/off
  and produces exactly the staircase it is meant to hide; here the coverage is a real area.

The mechanism is **coverage-agnostic** and unchanged since the 8×8 1 bpp era — the ROM resolution grew
underneath it without the shader ever being touched.

**The boundary to chip-specific idiosyncrasies.** MAX7456 artefacts (interlace jitter, brightness
curve, sync artefacts, …) do **not** belong here. A module that wants them hooks in from ITS own class:
`modules/f16/FBF16Max7456` — today a real NoOp override point held by `FBF16Module`.
`systems/FBHudFont.h` stays generic; otherwise every future type would carry the F-16's chip around
with it.
