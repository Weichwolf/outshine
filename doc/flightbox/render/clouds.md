# Wolken — die Renderkette

> Body still in German — translation pass pending (see [roadmap](../roadmap.md)).

**Herkunft:** ausgelagert aus `rendering.md` §5 (Stand `793e1fe`), unverändert übernommen.
Nachbardateien: [`renderer.md`](renderer.md) (Pass-Topologie, in die sich die Kette einhängt),
`../world-and-terrain.md` (Wetterquelle, sobald verdrahtet).

## Spec

**Rebuild, specified by the project owner** (roadmap R5). The six existing `FBCloud*` stages are
**demolition material, not a base.** What the owner asks for: how much and how far you can see,
flying through with consequences, and the fog underneath you.

**Bounded-volumetric, but simple:**

| Element | Specification |
|---|---|
| Density function | ONE separable function per deck: 2D coverage FBM (wind-advected from the weather provider) × analytic vertical profile, optionally a small 3D erosion noise generated at startup (~64³) |
| March | only inside the layer band: ray ∩ spherical shell analytically → at most three short segments, 6–12 steps per segment, blue-noise jitter, 16F accumulation, dither at the output (the banding recipe) |
| Light | no secondary march: Beer over the remaining thickness in closed form from the profile + 2–3 sun taps into the 2D field, ambient from the existing sky LUT |
| Composition | full resolution, ONE pass, no temporal, no bakes, `t1` clamped to scene depth (clouds in front of mountains, fog below the jet over terrain for free) |
| Camera inside the band | the segment starts at the camera — ONE code path for outside/inside/transition, fly-through seamless by construction |
| Explicitly NOT | impostors. Price: broken cumulus reads as a patchy sheet. Accepted. |
| Shared evaluation | the density function must be evaluable in WGSL **and** C++ (shared constants) — "how far can I see" is the same question sensors/IR will ask later |

Acceptance: a frame proof through the deck (fly-through without a seam), and a C++ evaluation of the
same function agreeing with the shader for a given sample set.

### High layer (cirrus) — why 2D is the RIGHT approximation

A sheet a few hundred metres thick at ~9 km has negligible parallax from any normal viewpoint, so a
2D advected field is **physically apt, not a cheat**. The one weak angle — edge-on at layer altitude —
is covered by the same unified shell logic as the other decks: the high layer gets its real thickness
(a few hundred metres), and the bounded march takes **1 analytic tap when far away, 2–4 steps near
layer altitude**. Same shell as low and mid, only thinner. No special case.

The feared "soft blobs" look is a property of **isotropic FBM**, not of the 2D method. Cirrus reads as
cirrus through three cheap properties:

| # | Property | How | Cost |
|---|---|---|---|
| 1 | **Fibres along the wind** | stretch the noise domain 5–10:1 along the `/wx` wind vector at 250 hPa — which *is* cirrus altitude, so the streak orientation comes from the real jet stream instead of an invented direction | zero, only the sample coordinate |
| 2 | **Hooks and fall-streaks** ("mares' tails") | domain-warp the sample position with a second low-frequency noise, and shear the high-frequency octave against the low-frequency one along the wind | +2–3 taps |
| 3 | **Sharp edges** | steep smoothstep remap instead of raw FBM; coverage drives the character — low coverage → hard remap, discrete streaks (cirrus uncinus); high coverage → soft remap, closed veil (cirrostratus). Both are real forms of the same étage. | zero |

Sharpness is **procedural** (3–4 octaves evaluated in-shader), not texture magnification — there is no
resolution ceiling to run into.

Lighting: the layer is thin, so forward scattering dominates; the HG phase term brightens the sun side
to silver — a large part of what the eye accepts as a real high cloud.

**Explicit non-goal:** single dramatic formations (towering Cb, anvil, storm front) are a **different
object** than a layer. If they are ever wanted, they become a fourth thing of their own, never an
extension of the three étages — recorded under Gaps as a deliberate omission.

## State

Six `FBCloud*` stages exist and work (three bake once, two run per frame, one is an init helper); the
whole branch is only built when `FB_CLOUDS=1`.

Owner's verdict: **expensive and ugly** — the chain is demolition material, not a base. Default is off
(`FB_CLOUDS=0`, user decision 2026-07-23), and no weather source is wired: `FBState.Env.Cloud*` stays
zero, and `FBCloudMarchStage` reads "0" as "no weather report" and sets its own default deck.

The distilled description of the existing chain is kept below under Knowledge — it is the record of
what is being replaced, plus the studies in `doc/clouds/01`–`10`.

## Gaps

### Open work (from the retired `TODO.md` §4)

| # | Thing |
|---|---|
| 4.4 | clouds off by default **and no weather source wired** — the rebuild depends on the weather provider of roadmap R4 |
| 4.9 | the rebuild itself: the Spec above is the contract, nothing of it is built |

Order: R2 (server `/wx`) → R4 (`FBWeatherProvider`) → R5 (this rebuild). Building the marcher before
the data source would mean tuning against an invented sky.

### Deliberate omissions of the rebuild

| Thing | Why |
|---|---|
| No impostors | accepted price: broken cumulus reads as a patchy sheet |
| **Single dramatic formations** — towering Cb, anvil, storm front | a formation is a different object than a layer. Not an extension of the three étages; if ever wanted, a fourth thing of its own with its own spec. |
| No temporal accumulation, no bakes | one pass, full resolution — the banding is handled by jitter + dither instead |

### Inventory (German, from the previous `Offene Punkte` section)

(siehe [`renderer.md`](renderer.md) — die Sammelliste der Renderer-Runde ist dort
vollständig erhalten; die hierher gehörenden Punkte stehen oben unter Gaps.)


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### Die Kette im Bestand

Sechs Klassen, eine je Shader. **Drei backen einmal, zwei laufen pro Frame, eine ist ein
Init-Helfer.** Der gesamte Zweig wird nur gebaut, wenn `FB_CLOUDS=1` — sonst kostet er weder Bootzeit
noch VRAM.

```
FBCloudMipDownStage  (geteilter Box-Downsample, Init-Zeit)
   ├─ FBCloudBaseBakeStage   128³ Perlin-Worley  ─┐
   ├─ FBCloudDetailBakeStage  32³ Worley         ─┤
   └─ (FBCloudCellBakeStage   512² F1-Zellen)    ─┤
                                                  ▼
                                        FBCloudMarchStage  → CloudLowTex (¼ Auflösung)
                                                  ▼
                                        FBCloudResolveStage → CloudHist/CloudWSum (Ping-Pong)
                                                  ▼
                                        FBTonemapStage (Composite)
```

Bind-Group-Reihenfolge (aus demselben Pin-Grund wie bei der Atmosphäre): erst die Bakes, dann March
(dessen Bind-Group ihre Views pinnt), dann Resolve (pinnt Marchs `CloudLowTex`-View).

**Der March** (`FBCloudMarchStage`):

| Größe | Wert | Herkunft |
|---|---|---|
| Zielauflösung | `Width/4 × Height/4`, rgba16float | Kostenbudget; die Resolve rekonstruiert daraus |
| Schalenradien | absolut: `groundR = |eye| − AltM`, `rBase = groundR + baseAGL`, `rTop = rBase + thick` | bewusst gegen den ECHTEN WGS84-Boden gerechnet, nicht gegen Hillaires vereinfachte 6360 km |
| Default-Basis | 8000 m AGL (hohe, aufgelockerte Zellenschicht) | akzeptierte Setzung 2026-07-23; `FB_CLOUD_BASE_M` sweept |
| Dicke | `2600 + 1400 · CloudHigh` m | Setzung; `FB_CLOUD_THICK_M` |
| Material | Dichte 18, Extinktion 0,06, Sonnenintensität 18, Detail 1,3 | Setzungen, per `SetCloudLab` überschreibbar |
| Deckung | max(CloudCover, Low, Mid, High); 0 → 0,4 in EVS | „kein Wetterbericht" = eine ansehnliche Defaultdecke |
| Schirm-Jitter | exaktes 4×4-Subraster, `FrameNo % 16` | jedes Vollauflösungs-Subpixel bekommt genau einmal je 16 Frames eine Direktprobe |
| Strahl-Dither | `frac(FrameNo · 0.6180339887)` | goldener Schnitt gegen Banding |
| Winddrift | `nowSec · 8` (km-Maßstab) | Setzung |
| Zellfeld | 40 km je Tile (≈ 4 km Zellen), Dome-Subtraktion 0,5 | `FB_CELL_KM` / `FB_CELL_DOME` |

Optionale GPU-Zeitmessung: Ist das Device-Feature `TimestampQuery` da, klammern **beide** Indizes den
March-Pass (nur diesen — einen Index auf `kQuerySetIndexUndefined` zu lassen lässt diesen Dawn-Build
den ganzen Command-Buffer verwerfen). Auflösen vor `Finish`, Pollen nach `Submit`; Mittelwert wird
alle 120 Frames geloggt.

**Der Resolve** (`FBCloudResolveStage`): Reprojektion der Historie über die Kamerabewegung an der
**Schalen-Mitte** (`CloudMidR = (rBase + rTop)/2`), zwei Ping-Pong-Paare (`CloudHist` rgba16float,
`CloudWSum` r32float = akkumuliertes Splat-Gewicht je Vollauflösungs-Pixel). `ResetHistory()` und
`SetAccumMode(true)` (echtes 1/N-Mittel statt exponentiellem Blend) sind Lab-Werkzeuge für die
Parameter-Sweeps.

**Der Composite** liegt im Tonemap, nicht in der Wolke: `scene = scene·(1−cl.a) + cl.rgb`
(premultipliziert) vor dem ACES-Fit. `FBTonemapStage` hält deshalb **zwei Pipelines aus einem
Quelltext** — die Plain-Variante bindet die Wolkentextur überhaupt nicht, sodass der ausgeschaltete
Pfad auch keine veraltete Historie samplen kann. Welche gilt, ist eine Bootzeit-Konstante und wird nie
mitten im Lauf umgeschaltet.

Vertiefte Herleitungen zu Noise, Dichte, Beleuchtung, Marschstrategie, temporaler Reprojektion und
iGPU-Budget stehen in `doc/clouds/01`–`10`.
