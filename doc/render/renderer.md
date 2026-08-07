# Rendering — the WebGPU renderer

**Sources of this file:** `sim/src/render/` — `Renderer.h/.cpp`, `DrawStage.h`, `FrameContext.h`,
`Gpu.h`, `ChunkMesh.h`, `ChunkVtx.h`, `Mips.h`, `Frustum.h`, `TemporalJitter.h`, `OverlayStage.h`, plus
the entity chain `Json`/`Glb`/`UnitModel`/`UnitDraw.h` — and `sim/src/render/stages/`. `Camera.h` lives
in `core/`. Every number below appears verbatim in the source; derivations and settings are marked as
such.

Neighbouring files: [`../architecture.md`](../architecture.md) (client split),
[`../world/terrain.md`](../world/terrain.md) (what feeds the renderer with geometry and albedo),
[`visual-target.md`](visual-target.md) (the bar and the budget). This file describes the **backend**;
one document per pass lives in [`stages/`](stages/).

---

## Spec

One renderer source, two link targets. The renderer is a **bolt-on**: never a dependency of the
physics or the termination logic.

| Contract | Acceptance / measurement anchor |
|---|---|
| WebGPU only, WGSL lives in stage files | `grep -c 'R"(' Renderer.cpp` == 0 |
| A stage is one shader with its pipelines, bind groups and draws, drawing into a BORROWED encoder | no stage opens a render pass |
| The pass topology is a contract — only `Renderer` sets pass boundaries | the encode order is fixed and documented; a stage split must not multiply passes |
| **WORLD-FIXED THINGS MAY NOT HANG ON THE CAMERA — three mechanisms, because discipline failed three times** | see §2.5. Measured failures: cloud-sheet UV built from `camFwd` (clouds stuck to the screen) · grass placement hashed from the instance index, polar about the eye (the field walked along) · the ground texture, same class. **Every one was invisible in a still frame and obvious in motion**, so a PNG oracle cannot catch this class at all |
| Global standard WGS84-ECEF, camera-relative, reversed-Z depth | far terrain does not z-fight; horizon dip from curvature |
| Ground truth from body geometry | eye height at ground comes from the body's own contact geometry, not a fixed number (§8.2) |
| Native and WASM render the same frame | `gpu_walk` is the headless PNG oracle for frame proofs (`../build-and-ops.md`) |
| Feature gates are baked constants | a disabled pass costs nothing |
| **An overlay is a BODY's capability, not renderer equipment** | `Renderer` holds one borrowed `OverlayStage*` (`render/OverlayStage.h`), registered by whoever HAS the capability, and knows no overlay type. **Nothing registers one today** — the avionics group that did was deleted on 2026-08-07 and `render/` has no text stage at all. Pass count with no overlay: 7, verified `passcount passes=7 clouds=1 cloudPass=0 overlay=0` |
| **The vertical FOV is a RUNTIME number out of the scene** | `Scene::FovDeg()` → `Renderer::SetFovDeg` → the projection, the atmosphere uniform and `FrameContext::FovDeg`. There is no `kSceneVerticalFovDeg` any more, so there is no second copy to drift from. Verified: the same scene at `fovDeg` 60 and 30 renders a clean 2× zoom with sky and terrain still agreeing on the horizon |

## State

Built; the stage split is finished (zero inline shaders in `Renderer.cpp`).

| Piece | Status | Anchor |
|---|---|---|
| Stage split in four slices | done | `c9206eb`…`2099cb0` |
| Hillaire atmosphere (transmittance / sky-view / sky), sun, moon, stars | built | `c9206eb`…`2099cb0` |
| Terrain stage with render bundles and two-phase streaming | built | `c9206eb`…`2099cb0` |
| Tonemap, upscale, loading screen | built | `c9206eb`…`2099cb0` |
| Camera basis shared by native and WASM (`CameraBasisEcef`) | built | `705c90a` |
| Text / overlay backend | **deleted 2026-08-07** with the avionics group. `OverlayStage.h` is a seam nothing implements — §7 | — |
| Cloud chain | rebuilt: ONE stage over ONE shared density function — see [`clouds.md`](clouds.md) | `9ca2c0e` + the R5 follow-up round |
| Units | **built** — one indexed draw per entity from the published pose, LOD off the asset sidecar, moving parts off the published articulation. Pass count unchanged. **No mesh ships and no topic file describes it** | — |
| Sprites | **built, and its combat effect catalogue is being removed** — one instanced draw for every effect in the frame, physical size in metres with an energy-preserving sub-pixel floor. The inputs it read (nozzle bit, store burn window, countermeasure age curves, hit count, wreck bit) have no writer since 2026-08-07 | — |

### The present path sRGB-encodes, and until this round the browser did not

`Renderer::ConfigureSurface` prefers an sRGB surface format and Chrome's canvas offers none — measured,
`caps.formats` holds three UNORM formats and the chosen one was `bgra8unorm` (enum 27). The tonemap
writes **display-linear**, so the browser was presenting a linear buffer as if it were encoded and the
whole picture was a gamma too dark: the same frame read **19.8/255** in the browser against **69.4** in
`gpu_walk`. The surface is now configured with the sRGB variant as a **view format** and the present
view is created with it; one function (`ConfigureSwapchain`) owns the configuration so a resize cannot
drop it. Measured after: browser 69.60 / 5.77 EV / 0.01 % / 0.00 % against native 69.41 / 5.77 / 0.01 /
0.00 — the two clients agree to 0.2/255.

## Gaps

### Die Baumstreuung steht bis auf die Hoehe — 2026-08-07

Gebaut und gemessen: `world/TreeField` streut **166 823 Staende** in 900 m Umkreis, `TreeStage` zieht
sie als Instanzen (ein Netz, ein Draw, N Instanzen), das Frame kostet **7,60 ms** von 16,67. Die
Streuung ist eine FUNKTION des Ortes — ein Hash je 3,33-m-Zelle gegen die deklarierte Dichte
(`trees.perM2`: Laubmischwald 0.04, Nadelwald 0.09) —, also haelt keine Seite eine Liste und dieselbe
Rechnung beantwortet spaeter "steht hier ein Stamm" fuer einen Koerper.

**Kein Baum ist im Bild, und ich habe die Ursache NICHT gefunden.** Fuenf Runden blindes Iterieren,
jede mit einer plausiblen Hypothese, keine davon war es: das Selbsttor der Stage (`HeightM <= 0`) ist
geoeffnet, die Baumhoehe kommt jetzt aus der Art (`bpar.z`, vorher 0 — jede Instanz war auf Groesse null
skaliert), der Fuss sitzt auf der Gelaendehoehe (`FromEnu` plus `fb_stream_ground`), die Achsen sind
geprueft (ax Ost, ay Nord, az Hoch). Der Szenenpass wurde dabei nicht teurer (1.77 gegen 2.03 ms), was
gegen jedes Zeichnen spricht.

**Die naechste Handlung ist eine MESSUNG, keine sechste Vermutung**: den Stammshader eine feste Farbe
ausgeben lassen und zaehlen, ob ein einziges Fragment ankommt. Erst wenn feststeht, ob der Draw
ueberhaupt Fragmente erzeugt, ist die Suche wieder gerichtet.

Die Hoehe selbst ist geloest und war ein echter Fund: Ein Stand traegt Ost, Nord, Groesse und
Gierung; seine LAGE in der Vertikalen fehlt, also sitzen alle auf der Augenhoehe der Kamera — an einem
Berghang heisst das im Fels oder in der Luft. Das ist derselbe offene Posten wie den ganzen Tag: das
Hoehenorakel muss die GEZEICHNETE Flaeche beantworten, nicht eine zweite (0,383 m RMS, max 1,89 m
Abweichung gegen das Netz). 166 823 Einzelabfragen je Streuung sind dafuer kein gangbarer Weg; die
Hoehe muss aus derselben Funktion kommen, die das Netz baut.

Owner, 2026-08-07, zur Einordnung: *„der baumgenerator ist nur ein kleiner prototyp. du musst den schon
noch auf speedtree niveau bringen"*, und *„das bild entsteht aus foliage, material, rocks, buildings,
infrastruktur, trees, water — farbkorrektur kommt zuletzt"*. Die Kurvenarbeit an einem leeren Hang war
verlorene Zeit.


### Nach der Rueckkehr der Schatten: die Struktur ist da, die Kurve zerdrueckt sie — 2026-08-07

Ein Weitwinkelblick vom Fahrenbergkopf bei Tageslicht, Binary `b937ab7c…`, 960x360:

| | |
|---|---|
| Luminanz min / p1 / p5 / p50 / max | 1.7 / 2.0 / 2.7 / **6.5** / 148.9 |
| unter Luminanz 10 | **73.6 % des Bildes** |

Das Verhaeltnis beleuchtet zu verschattet ist **23, also 4,5 EV** — und das ist genau das gemessene
Verhaeltnis der Bestrahlungsstaerken (`sunRGB` 0.85/0.67/0.48 gegen `skyRGB` 0.027/0.049/0.096). **Die
Beleuchtung rechnet richtig; die Anzeigekurve zerdrueckt das Ergebnis.**

Damit ist der Fall eingetreten, vor dem das art-director-Urteil gewarnt hat — spiegelbildlich. Es hiess:
eine filmische Kurve OHNE Schattenstruktur gebe Tonwertumfang ohne Tonwertaufbau und mache das Bild
schlechter. Jetzt ist die Struktur da und die Kurve ist an der Reihe: der Fuss (`kToe` = 0.0551 in
`TaaStage.h`) hebt die Tiefen zu wenig, und 74 % einer Landschaft im Schatten eines Grates ist kein
Bild, sondern eine Silhouette.

### Die Standpunkte sind ein MOD, und sie werden von Hand gestellt

`mods/webcams/cams.json`, nicht `sim/assets/`. Owner, 2026-08-07: *„Posen von Hand nachjustieren … kann
man ja in nem json speichern"*, *„mehrere positionen im mods/"*. Die Engine kennt keine Kamera, sie
kennt eine Szene.

`tools/posefit.py` bleibt als Werkzeug liegen und **sein Ergebnis wird nicht uebernommen** — es ist
dreimal gescheitert: Azimut und Bildwinkel sind zusammen unterbestimmt (die Suche lief an den Rand des
Bildwinkelbereichs), und bei festgehaltenem Bildwinkel lieferte es fuer den Kochelsee konsistent einen
Blick steil nach Nordosten, wo die Kamera nach Norden hinabschaut.

Von Hand geht es besser, und die Quelle steht im Bild selbst: die Einblendung der Kochelseekamera lautet
*„Herzogstand / Fahrenbergkopf — Blick ueber den Kochelsee ins Oberland"*. Meine erste Eintragung setzte
sie ans Seeufer auf 600 m mit Blick nach Sueden; sie steht auf 1620 m und schaut nach Norden hinab. Die
Bildunterschrift ist die zuverlaessigste Posenquelle, die diese Seite hat.


### Das art-director-Urteil gegen die Webcams, und warum Schatten zurueckkamen — 2026-08-07

Acht Alpenkameras, Livebild gegen unseren Render, beides zur selben Minute bei 1,2 bis 3,1 Grad
Sonnenhoehe. Das Urteil war **NACHBESSERN**, und es nennt einen einzigen Grund:

| | dunkelster Pixel | unter 0.10 | ueber 0.90 | Spanne p1..p99 |
|---|---|---|---|---|
| unsere Renders (Median) | 0.41 | **0.0 %** | **0.0 %** | **0.90 EV** |
| die Fotos (Median) | 0.00 | 1.5 % | 6 % | **3.15 EV** |

**Aber die Zahl ist das Symptom, nicht die Ursache.** Woertlich: *„Wir haben den Farbstich der
Daemmerung ohne ihre Selektivitaet."* Bei 2 Grad Sonnenhoehe wirft ein 100-m-Huegel **2,9 km**
Schatten; die Grate brennen und die Taeler sind blauschwarz. Wir legten stattdessen einen
gleichmaessigen Sepiafilter ueber eine gleichmaessig beleuchtete Landschaft — **das Licht hatte keine
Richtung.**

Und die Warnung, die den ganzen Tag zusammenfasst: eine filmische Kurve mit echtem Schwarzpunkt haette
die Kennzahl sofort repariert (3 EV statt 0.9) und **das Bild verschlechtert** — Tonwertumfang ohne
Tonwertaufbau.

**Konsequenz: der Schattenpass ist zurueck.** Nicht neu gebaut, sondern aus `900c970^` zurueckgeholt —
Kaskadenschattenkarten sind, was Witcher 3 und GTA 5 2015 hatten, und die Kosten waren schon gemessen.
Gemessen nach der Rueckholung, Kochelsee bei 640x360, Binary `b937ab7c…`: **shadow 1,97 ms, Frame
7,34 ms** von 16,67. Wolken bleiben draussen.

**Die drei „kaputten" Renders waren nicht kaputt, sie waren falsch gezielt.** Gemessen: alle vier
Kameras streamen vollstaendig (Fortschritt 1, 49 000 bis 61 000 Dreiecke, plausible Bodenhoehen), und
Innsbruck steht auf 577 m im Inntal, wo eine geratene Pose von 180 Grad bei −3 Grad Neigung direkt in
die Talflanke blickt. Dieselbe Kamera mit 90 Grad und +2 Grad: Luminanzspanne **0,0 → 35,4**. Das ist
ein Fehler in meinen geratenen Posen, kein Renderfehler — und er entwertet einen Teil des Urteils, weil
mehrere der beurteilten Bilder gegen eine Wand schauten. **Das Feld sagt nichts, solange die Posen nicht
eingepasst sind**, und der Horizonteinpasser braucht dafuer eine zweite Bindung (Passpunkte aus
OSM-Gebaeuden), weil er in der Bildwinkelachse nicht konvergiert.

Weiter offen aus demselben Urteil, unbearbeitet: salzburg mit senkrechten Naehten bei x=161 und x=251; `damuels`
zeigt **senkrechte Kordstreifung** (Spaltenmittel schwanken um 63,5 Stufen, Zeilenmittel um 2,8); die
Saettigung liegt bei 0,48-0,62 gegen 0,28-0,35 im Foto, also doppelt so bunt bei einem Drittel des
Umfangs; und das Gelaende ist eine Steppdecke ohne Grate und Runsen.


### Contradictions between claim and code (from the retired `TODO.md` §1)

| Place | Contradiction |
|---|---|
| `render/Camera` + `sim/up.sh` | **the camera clamp "never below the surface" has no consumer any more.** `FB_GROUND_CLEAR` is produced by `fb-sim` and read by nobody — an invariant promised in `CLAUDE.md` is effectively off. |
| `render/Camera` | reversed-Z numbers disagree: `CLAUDE.md` says near 0.01 m / far 240 km, the code uses an infinite far plane and `zn = 0.05`; the 240 km are the streaming view radius |

### Open work (from the retired `TODO.md` §4)

| # | Thing |
|---|---|
| 4.1 | `UnitsStage` draws an empty cast — built, and nothing publishes an entity |
| 4.3 | transmittance LUT is recomputed every frame although it only depends on altitude and sun cos θ |
| 4.4 | ~~aerial perspective off by default (`FB_AP=0`)~~ **closed** — the switch and the dead Rayleigh/Mie block are gone; the terrain now runs a WEATHER-driven haze out of the same `AtmoHaze.h` the cloud deck uses (§6, [`clouds.md`](clouds.md)). Its scale height was corrected the same day: two summed terms (molecular 8 km, aerosol 1.2 km) instead of one, and the molecular one carries λ⁻⁴, which closes `clouds.md` Gaps 5.7 **and** 5.8 — the per-channel extinction the deleted block had is back, from the physics rather than from a LUT |
| 4.5 | upscale is bilinear only (`TODO bicubic/sharpen`) |
| 4.6 | dead code `w3_frustum_from`, `w3_aabb_visible`; the static terrain path is untested inheritance |

Renderer-adjacent items that belong to the world/tile side (DEM cache per worker instance, time-based
eviction, `kNodeCeil`, the imagery mode not being declarable at all, TLS not wired) live in
[`../world/terrain.md`](../world/terrain.md)'s Gaps.

### Inventory (from the previous `Open points` section)

1. **The entity stage is BUILT and has nothing to draw, and no topic file describes it.** One indexed
   draw per visible entity from the borrowed registry's published pose, with the asset sidecar's LOD
   and part tables. The pass count is unchanged. **No mesh ships**, so it draws an empty cast today.
   The document that carried the mechanism was retired with its combat effect catalogue on 2026-08-07;
   the drawing half needs a `render/entities.md` as soon as a body exists to point it at.


### The sim-critic's verdict on the built layers, 2026-08-07 — eight defects, none closed

First critic judgement of the round; binary pinned `7a1359a1…`, `judge.png` reproduced bit-identical,
plus eleven of the critic's own frames at six azimuths and three standpoints. **The gate stands at 0 of
10.** In the owner's own order — geometry → LOD → lighting → colour — the first two entries are the same
missing thing and they cause the third.

| # | defect | the measurement |
|---|---|---|
| 1 | **Buildings take no directional light** | a silo's cylinder wall is 88.9 % exactly `(114,114,120)`; 14 probes across it, zero counts of variation. Roof (horizontal) 117.6 against wall (vertical) 113.9 — **4 counts**. Terrain on the same frame has std 13.5 |
| 2 | **No cast shadow anywhere, and no contact shadow** | a 15 m silo at an 11.3° sun owes 75 m of shadow. The apron in front of it has blue std 1.70; the ground gets *brighter* toward the wall foot, not darker. `shadowTris` 3023 against `terrainTris` 52992 — **the terrain is not a caster at all** |
| 3 | **No black point** | 0.000 % of the frame under luminance 32, minimum 100. A consequence of 1 and 2: nothing is dark because nothing is in shadow |
| 4 | **Detail falls off toward the viewer** | luminance std 5.00 at 12.4 m, 1.38 at 0.9 m. At 2.3 mm per pixel the ground carries 3.0 % contrast |
| 5 | **Clouds are thresholded SDFs** | the blue channel varies by 5 steps over 72 800 px — less than the clear sky beside it. Monotone 40 px ramp, then 90 px of constant |
| 6 | **No foreground** | 210 m without one vertical. Green rises in saturation with distance (G−B 69 at 3 m, 89 at 212 m) instead of falling. `fovDeg` 60 is vertical, so hFOV is really **91.5°** and the edges stretch 2.05× |
| 7 | **Class edges are 0.91 px** | declared up to 0.90 m; measured 0.91 px ≈ 1.7 cm at 19 mm/px — a factor of ~50. Straight to 0.29 px of residual. Not TAA convergence: `--settle` 64 against 128 moves the mean by 0.0014 |
| 8 | ~~Screen-space black specks~~ **CLOSED 2026-08-07** | it was `AoStage`, and it was a NaN. `aoProject` rounds a tap back to the depth grid, so a tap landing inside p0's own texel gives `toward = 0` and `normalize(0) = NaN` — which then SURVIVES the `cosA <= 0.05` reject, because every comparison with NaN is false. One such tap poisoned the whole sum. Measured at a standpoint where it is severe (eye 40 m, yaw 100): road darkest 65.1 -> **87.9**, ploughed field 58.1 -> **83.8**, outliers 5 -> **0** and 16 -> **0** — exactly the AO-disabled values, so every dark outlier in the frame was this. AO itself is intact: it still changes 64.11 % of a village frame with a peak of 127 codes, and correctly nothing in an open view. Guard is `dist < 1.0e-3` |

**On (1), what the model predicts against what the picture shows.** With `kGroundBounce` 0.12,
`kSelfShelter` 0.35 and concrete albedo 0.29, back-lit so the sun terms drop out:

```
roof (n·up = 1):  0.650·S + 0      + 0.29·0.350·S = 0.752·S
wall (n·up = 0):  0.325·S + 0.06·S + 0.29·0.175·S = 0.436·S      ratio 1.724 = 0.79 EV
```

**`litRadiance` separates them by 0.79 EV and the frame shows 0.05 EV — a factor of 16.** The tone curve
does not explain it: over an 11.7 EV span at contrast 1.09, 0.79 EV is about **17 codes**, and 4 were
measured. The named suspect is `stages/BuildingsStage.cpp:53`, which turns every normal toward the
camera (`dot(in.nrm, in.rel) > 0`) — right for a wall, and for a horizontal cap it flips `n·up` from +1
to −1 and drops the roof into the wall's ambient branch. **Suspected, not proven: the next measurement
is one line, the building normal written out as colour.**

**On (8), one refutation on the way.** The first hypothesis was that the golden-angle spiral's
per-pixel hash rotation left neighbouring estimates uncorrelated, so an unlucky rotation had nothing to
cancel it. Interleaved gradient noise was built and **measured to fail**: road outliers 5 -> 3, field
16 -> 15, and the darkest pixel got WORSE (58.1 -> 53.0). Reverted rather than kept, because its comment
claimed an effect the measurement denied. The silhouette-safe normal reconstruction was already in place
and was never the cause either.

**On (7), what is already excluded.** `kClsFray` forced 0.05 → 0.30 moves 2597 px (max Δ 54) and forcing
`zone` to 0.30 moves 3888 px, so the width does reach `half`; the JSON carries the nine declared values
and `VegetationTemplates.cpp` reads both of them. So the break is between the class row and `zone`, and
the next measurement is `half` and `hit.dist` written out as colour.

## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1 Fundamental decisions

| Decision | Implementation | Derivation / reason |
|---|---|---|
| **API: WebGPU** | ONE source, TWO link targets — emdawnwebgpu (browser) and native Dawn (`gpu_walk`) | The same Dawn header family on both sides: "write once, link twice". Native Dawn really renders; a headless browser with SwiftShader does not provide that proof. |
| **WGS84-ECEF, camera-relative** | vertices carry the offset to the tile origin; the frame subtracts `origin_ecef − cam_ecef`; the camera sits at the origin | No absolute 6.4e6 m coordinate ever reaches a `float`. Precision is best AT the eye everywhere on Earth. |
| **Reversed-Z (Depth32Float)** | clear `0.0`, `CompareFunction::Greater`, infinite far, `zn = 0.05 m` | `[0,1]` clip (native, not GL's `[-1,1]`) → the full mantissa for depth; distant terrain does not z-fight. |
| **HDR + ACES** | scene → `rgba16float` target, one fullscreen tonemap (Narkowicz ACES fit) → sRGB | Light stays linear until then; display encoding happens at EXACTLY ONE place (the tonemap pass, whose sRGB view encodes on store). |
| **Render bundle submission** | terrain draws are recorded once and replayed per frame; re-recording only on a STRUCTURAL change | ~N CPU draw encodes become one `ExecuteBundles`. |
| **Feature gates = baked constants** | env-driven string replace on the shader source before `CreateShaderModule` | A dead path is optimised away by the shader compiler and costs nothing — no runtime branch. |
| **Fixed 720p frame target** | scene + tonemap + overlay land in `FrameTex` (1280×720); an upscale pass puts it onto the swapchain | The sim resolution is stable, the display follows canvas × DPR. |

#### 1.1 Two link targets, one renderer

| | Browser (WASM/emdawnwebgpu) | Native (Dawn, `gpu_walk`) |
|---|---|---|
| Bring-up | `Renderer::Init(canvasSelector, w, h)` — asynchronous, callbacks `AllowSpontaneous`; `Ready()` polls the app | `Renderer::InitOffscreen(w, h)` — blocking via `Instance::WaitAny(future, UINT64_MAX)` |
| Prerequisite | the browser event loop pumps the callbacks | the instance feature `TimedWaitAny` is requested at `CreateInstance` |
| Final target | `wgpu::Surface` on `#gpu`, format = the first sRGB-capable one from `SurfaceCapabilities` (otherwise `BGRA8Unorm`) | `OffscreenTex`: `RGBA8UnormSrgb`, usage `RenderAttachment\|CopySrc` |
| Return | — | `ReadPixels()` → tightly packed W·H·4 RGBA8, already sRGB-encoded → straight into `stb_image_write`; `ReadDepth()` → W·H f32 of the reversed-Z scene depth, so a critic's "at 1–2 km" is a MASK and not a guess about a hillside's row. Range along the ray = `kNearM / depth / cos(off-axis)`, `kNearM` = 0.05 m = `MvpCamRel`'s `zn`. `gpu_walk --depth PATH` writes it beside the PNG |

At the adapter it is logged **what** WebGPU really runs on (`Log::Info("render","adapter")`):
`adapterType == CPU` means a software rasteriser (SwiftShader/lavapipe/WARP) — then high CPU load is
the browser's, not our code's. Also logged: `maxTextureArrayLayers`, `maxBufferSize`,
`maxTextureDimension2D`.

Device loss is not a crash: the `DeviceLostCallback` sets `DeviceLost`, GPU operations are skipped from
then on, the CPU side (tile streaming, counters) keeps running. `DeviceUsable()` =
`DeviceReady && !DeviceLost`.

#### 1.2 Camera-relative ECEF and the projection

`MvpCamRel()` (`Renderer.cpp`, static) builds **projection × view** like this:

- View = a pure rotation from the ECEF camera basis (`right`, `camUp`, `−fwd`) — no translation,
  because the vertices already arrive pre-shifted.
- Projection = infinite reversed-Z: `p = {f/asp, 0,0,0, 0,f,0,0, 0,0,0,−1, 0,0,zn,0}` with
  `f = 1/tan(fov/2)`, `fov = 60°`, `zn = 0.05`. From that `z_clip = zn`, `w = −z_eye`, hence
  **`depth = zn / (−z_eye)`** — monotonically falling with distance, 1 at `zn`, → 0 at infinity.
- No explicit far plane. The "view distance" is a property of the **streaming** (radius `FB_VIEW_KM`,
  default 240 km — see `world-and-terrain.md`), not of the projection.

The 20-float block `Mvp20` in the `FrameContext` is this matrix (index 0..15, column-major) plus the
sun direction (16..18) plus one pad — exactly as the terrain uniform expects it.

#### 1.3 Depth states per stage

| Stage | `depthWriteEnabled` | `depthCompare` | Effect |
|---|---|---|---|
| `SkyStage` | false | `Always` | background; everything draws over it |
| `SunStage`, `MoonStage` | — (additive, in the same pass directly after sky) | as sky | pure addition, order-independent |
| `StarsStage` | false | `Always` | "at infinity"; terrain paints over them |
| `TilesStage` | true | `Greater` | reversed-Z: nearer = greater |
| `TileLightsStage` | false | `GreaterEqual` | hills occlude distant lights, but the sprites write no depth |

#### 1.4 HDR format: why `rgba16float` and not `rg11b10ufloat`

`rg11b10ufloat` was the bandwidth choice and is **rejected**: it has no alpha channel and no guaranteed
blend support — the cloud pass blends premultiplied alpha over it, and that broke. `rgba16float` is the
standard blend-capable HDR format; the 4 extra bytes per pixel at 720p are negligible
(`Renderer::OnAdapter`, comment there).

#### 1.5 The present path

| Resource | Size/format | Role |
|---|---|---|
| `HdrTex` | width×height, `HdrFormat` (rgba16float), `RenderAttachment\|TextureBinding` | scene target, linear radiance |
| `DepthTex` | width×height, `Depth32Float`, `RenderAttachment\|TextureBinding` | reversed-Z; the cloud pass SAMPLES it (hence `TextureBinding`) |
| `FrameTex` | width×height (1280×720), `SurfaceFormat` (sRGB), `+CopySrc` | scene + tonemap land here |
| Swapchain | canvas `clientSize × devicePixelRatio`, capped at 4096 | follows the display |

`SyncSwapSize()` reconfigures the surface only when width OR height changes by **≥ 8 px** (hysteresis
against sub-pixel jitter). The scene is untouched by this — only swapchain and upscale viewport
follow.

#### 1.6 Feature gates and env switches

The mechanism has two levels: either a **constant is baked into the WGSL source** (string replace
before `CreateShaderModule`, after which the shader compiler strips the dead block), or a whole
resource/pipeline group is **not built at all**.

| Switch | Default | Effect | Place |
|---|---|---|---|
| `FB_CLOUDS` | **1 (armed)** | arms the cloud layer pass. Off = no pipeline, no VRAM. Armed is not the same as drawn: the pass only exists when the weather sample has a deck | `Renderer::OnDevice` |
| `--cloudq` (native) / `SetCloudQuality` | 1.0 | scales the NODE count of the march, 0.25…8 | `CloudLayerStage` |
| `FB_MOON_SCALE` | 1.0 | multiplier on the real lunar angular radius (0.0045 rad ≈ 0.5°) | `Renderer::UpdateAtmosphere` |
| `FB_PHOTO_ZMAX` | 11 | from which zoom an aerial-imagery tile counts as "bright enough" (gain calculation) | `TilesStage.cpp` |
| `FB_PHOTO_EMA` / `FB_PHOTO_MAXGAIN` / `FB_PHOTO_LOG` | 0.08 / 2.5 / off | adaptive brightness adjustment of the imagery layers | `TilesStage.cpp` |
| `FB_GEOM` | **0** | GEOMETRY ISOLATION for measuring: disarms the shadow receivers and the AO draw and freezes the tonemap curve at this scene's metered anchors (−8.20303 / 3.51562 / 1.38095). Removes no pass and no `Begin*Pass`, so the frame count is identical with and without — the point is that a geometry change may not be judged through a light change. Owner, 2026-08-06: *„da Nanite Geometrie ist würde ich es ohne Postprocessing und Schatten bauen."* | `render/GeometryIsolation.h` |
| `FB_DAG` | **1 (armed)** | the cluster DAG ([`lod.md`](lod.md)). 0 = one root cluster per mesh, i.e. the exact geometry drawn before the DAG existed — measured byte-identical, which is what makes every LOD number a paired measurement on ONE binary | `render/ClusterDag.h` |
| `FB_TAU` | **1.0** | the screen-space-error threshold τ in pixels. `[SET]`; overridable so the quality/cost curve is a measurement rather than a recompile | `render/ClusterDag.h` |
| `FB_DAGLOG` | off | one line per tile and per building field: levels, triangles per level, error band per level | `world/World.cpp` |
| `FB_TAA` | **1 (armed)** | the temporal pair — sub-pixel jitter in the projection plus the resolve pass. 0 removes both, the history textures and the ground cover's previous-frame vertex work together, so every antialiasing number is a paired measurement on one binary. [`stages/taa.md`](stages/taa.md) |
| `FB_JITTER` | off | `x,y` PINS the sub-pixel phase in pixels. Two frames at two pinned phases must differ by a rigid image translation of exactly the phase difference, the same in every distance band — the measurement that proves the jitter is a camera property (§1.9) |
| `FB_TAA_GAMMA` / `FB_TAA_VELRAMP` / `FB_TAA_FEEDMAX` | 1.5 / 0.015 / 0.85 | the resolve's three numbers, each with its measured curve in [`stages/taa.md`](stages/taa.md) |
| `FB_TONE_PROBE` | off | `black,white` — the display curve as a RULER: exponent 1, no toe, so a PNG read back through the sRGB decode IS the scene's HDR histogram in `(log2 L − black)/(white − black)`. The only way to measure the population BELOW the black anchor, where the shipped curve has no inverse | `TonemapStage.cpp` |
| `FB_GPU_NOOP` / `FB_GPU_BISECT` | compile defines | bisect levels: inert frames and acquire-only respectively — separates init-side from frame-side device death | `Renderer::RenderFrame` |

---

### 1.9 Three mechanisms against one failure class: world-fixed vs. camera-fixed

> Owner, 2026-08-06, after finding the third instance in the browser: *„wir haben ja die
> Schutzmechanismen im Klassendesign besprochen."*

**They were discussed and not built, which is why the same bug happened three times.** A rule that lives
only in a conversation is not a rule. What went wrong each time was the same move: a quantity that must
be fixed in the WORLD was derived from a quantity that is fixed to the CAMERA.

| # | Where | What it hung on | Symptom |
|---|---|---|---|
| 1 | `SkyStage` cloud sheet | `cross(up, camFwd)` | clouds stood still on the screen while the head turned |
| 2 | the stand's placement | `hash(instance_index)`, polar about the eye | the whole grass field walked along with the walker. **Fixed**: the stand is hashed on the graticule, a place (`render/Graticule.h`, [`stages/terrain.md`](stages/terrain.md)) |
| 3 | ground texture | same class | ground detail did not parallax |
| 4 | `CloudLayerStage` field anchor | the FIRST FRAME'S EYE | the cloud field, and with it every shadow it throws, was pinned to wherever a session happened to start; two standpoints saw two skies. **Fixed**: the anchor is `CloudSky::AnchorLat/LonDeg`, a place, resolved in `Renderer::WriteCloudSky`. Proven by the two tests below rather than by inspection — [`clouds.md`](clouds.md) `## State` |

**All three were invisible in a still frame.** That is the load-bearing fact: the PNG oracle, which
catches almost everything else in this tree, is structurally blind here. Only motion reveals it, and
until the walker existed nobody was in motion.

**The three mechanisms, each catching a different half:**

**(a) Split the uniform — access control, not naming discipline.** Today `AtmoCommon.h` declares ONE
`Atmo` block holding world quantities (`up`, `sunDir`, `sunTan`, `side`) and camera quantities
(`camFwd`, `camRight`, `camUp`) side by side, so `A.camFwd` is exactly as easy to reach for as `A.up`.
Split into `World { … }` and `View { … }`: a pass that binds only `World` **cannot** reach the camera
basis. It does not solve every case — the cloud shader legitimately needs both, for the view ray *and*
for the sheet's frame — but where a pass needs only world quantities, the class becomes unreachable
rather than merely discouraged.

**(b) `verify-uniforms` — invert the burden of proof.** WGSL warns about nothing: an unread uniform
field is perfectly legal and produces no diagnostic, and Tint and Naga both stay silent. So the check
lives outside the compiler, in the shape this tree already uses for `verify-layers`.
Every pass declares, for **every** field of the block it binds, either that it uses it or why it does
not:

```
/* USES:   camPosMm sunDir up camFwd camRight camUp params skyExtra view
 * UNUSED: sunTan side  — the cloud sheet needs a world-fixed frame but takes ECEF +Z rather than
 *                        the sun basis, which drifts with the sun over the day
 * UNUSED: moonDir      — the moon is drawn by MoonStage */
```

The gate checks three things: every field appears in exactly one list · everything under `USES` is
genuinely read in the source · everything under `UNUSED` carries a reason. **This is the mechanism that
would have caught defect 1**: `SkyStage` does not read `sunTan` or `side` — a world-fixed basis that was
sitting in the same block, unused, while the shader built its own from the camera.

**(c) The frame-pair gate — catches what (a) and (b) let through.** Two renders and an image
comparison, automatic, in every round:

| Test | Setup | What must hold |
|---|---|---|
| **rotation** | two frames, same position, yaw offset by Δ | everything world-fixed moves in the image by Δ. What stands still is camera-fixed — **defect** |
| **translation** | two frames, same view direction, position offset by s | everything world-fixed parallaxes, by `f·s/z`. What moves along is camera-fixed — **defect** |

**The image test dies at high instance counts, and the replacement is better than what it replaces.**
Band-passed image correlation measures *texture*, and texture collapses once a frame carries 1.39 M
grass blades: the correlation peak fell to 0.19–0.41 and the anchor could no longer be stated at all.
The fix is to stop asking how it *looks* and ask **where it is** — in the depth buffer, which is what
was being inferred from the image anyway:

1. Two frames, same yaw and pitch, `--stepE s` apart, both with `--depth`. Pitch steeply down so the
   subject fills the frame.
2. Unproject every pixel: `range = 0.05/d/cos(off-axis)`, ray `(x−cx, cy−y, f)` normalised, rotated by
   pitch → a point in camera coordinates `(right, forward, up)`.
3. Bin onto a 3 cm lattice in `(right, forward)`, keep the **maximum** `up` per cell. That is the canopy
   top as a scalar field.
4. Register field B against A: search `(dr, df)` minimising median `|Δz|`.
5. **The argmin against the geometrically predicted shift `+s·(sin Δ, −cos Δ)` is the statement.**

Measured, yaw 280, pitch −55, eastward step:

| step | predicted (dr, df) | best fit | median \|Δz\| | the same under "camera-fixed" |
|---|---|---|---|---|
| 0.0985 m | (+0.0171, −0.0970) | **(+0.017, −0.097)** | **3.8 mm** | 60.5 mm |
| 0.50 m | (+0.0868, −0.4924) | **(+0.087, −0.492)** | **17.3 mm** | 62.2 mm |

References in the same unit: A against itself shifted by ONE cell (3 cm) = 33.3 mm; by four cells
(12 cm) = 70.4 mm, i.e. decorrelated. The null hypothesis sits at 60–62 mm, essentially at the
decorrelation floor. Signal to floor **8.8 : 1**, residual 0.5 % of the canopy's own 0.017–0.788 m
height span.

**Why it holds at any density:** it never computes an image correlation, it fits a rigid transform.
More instances mean more support points per cell, so density makes the measurement *better*. It also
returns millimetres instead of a correlation coefficient nobody can hold against a threshold — and step
3 hands out the canopy-top statistic for free (sd 0.162 m), which is what quantified the missing
self-shadow.

Instance 4 was proved a third way, and it is the cheapest of the three where a field is evaluable on
both sides: **print the quantity itself**. `render/cloud_shadow` logs the local cloud transmittance at
the camera's own ground point, from the same expression the fragment shader runs; four yaws print one
value to every digit and three standpoints print three. An image test could not have decided it — the
shadow's contribution to the far field is 1.4 % of display luminance, below the correlation's floor
([`lighting.md`](lighting.md) `## Gaps`).

The measurement is a normalised cross-correlation of the high-passed image over distance bands; the
grass fix was proved exactly this way (predicted `2.4 · 5.0 · 9.4 · 15.9 · 20.0 px`, measured
`2 · 6 · 10 · 17 · 22`, and a control build with the old placement peaked at **0 px in every band**).
The tool exists; it is not yet a gate.

**Acceptance for this section:** all three built, and the rotation and translation tests running in the
same place as `verify-layers`. **None of the three exists today.**

**The sub-pixel jitter was the first thing built that could have joined this list, and it was checked
against it before it shipped.** The offset enters the projection on the SAME term as the boresight
shift — a constant NDC offset on the z column — so a world point's NDC moves by exactly the offset
whatever its depth: a shear of the frustum, not a translation of the world. The proof is an integer
one and needs no correlation, which matters because §1.9's own image test is the one that died at
1.39 M blades:

> `FB_JITTER=1.0,0.0` must produce the picture `FB_JITTER=0,0` produces, **shifted by exactly one
> pixel**, and identically at every range.

Measured: mean \|ΔY\| **0.020 codes** over 482 263 ground pixels, 0.207 % of them over 2 codes, and the
depth buffer **bit-identical** out to 35 m; the same comparison without applying the shift reads 10.667
codes and 43.8 %. The residual sits in the 25–80 m bands and is the exposure meter and the frustum's
edge cell answering to a frame shifted by one pixel. Per-band table in
[`stages/taa.md`](stages/taa.md) §1.

### 2 The pass topology as a contract

**The rule:** `Renderer` owns instance/adapter/device/queue/surface/targets, **every**
`BeginRenderPass`/`BeginComputePass` boundary and the encode order. An `DrawStage`
(`render/DrawStage.h`) draws **into the borrowed encoder** that `Renderer` has already opened, and
**never** opens or closes a pass itself.

Why this is a rule and not a matter of style:

1. **The pass count is a measured quantity.** The stage split was not allowed to change the number of
   `Begin*Pass` calls per frame; `RenderFrame` therefore counts them (`passCount`) and logs them
   (`Log::Debug("render","passcount")`) at the first SCENE frame and every 300 frames thereafter. A
   before/after diff is thus readable directly from the telemetry.
2. **A pass is expensive, a draw is not.** If every stage could draw its own pass boundaries, every new
   stage would multiply the topology — creepingly and unobtrusively.
3. **Attachments are a matter of contract.** Whoever opens the pass determines target views, load/store
   ops and clear values. The cloud resolve, for example, writes into TWO attachments whose ping-pong
   index the renderer queries BEFORE `Encode()` — the stage cannot build that descriptor itself.

Supplementary rules from `DrawStage.h`:

- **A stage self-gates.** "Nothing visible this frame" means: `Encode()` draws nothing. The renderer
  calls every stage in its slot **unconditionally** (the only exception is the overlay pass, whose
  `if (Overlay && Overlay->Active())` sits outside — an empty pass would still be a pass, and the
  pass-count invariant lives on exactly this outer `if`).
- **Two `Encode` forms.** `Encode(ctx, RenderPassEncoder&)` and
  `EncodeCompute(ctx, ComputePassEncoder&)`; the other one stays the inert default.
- **`Gpu`** (device/queue/formats/size/instance) is given to a stage ONCE at `Init`/`Configure`,
  never per frame. **`FrameContext`** is the shared per-frame state (camera basis, MVP, sun, moon,
  day factor, weather, frame number, size) — a stage never reaches back into `Renderer`.

#### 2.1 The complete encode order

Order as in `Renderer::RenderFrame()`:

| # | Pass | Stage(s) | Target | Note |
|---|---|---|---|---|
| 1 | Compute | `TransmittanceStage` | `TransLUT` (256×64) | recomputed every frame (TODO: cache while the sun is static) |
| 1 | ″ | `MultiScatterStage` | `MsLUT` (32×32) | SECOND dispatch in the SAME pass — WebGPU orders dispatches and makes the first one's writes visible, so Hillaire's multiple-scattering bake costs no pass |
| 2 | Compute | `SkyViewStage` | `SkyLUT` (192×108) | reads `TransLUT` + `MsLUT` |
| 2 | ″ | `IrradianceStage` | `IrrBuf` (2 × vec4) | second dispatch, reads the LUT written by the first: the hemisphere integral of the sky IS the ground's ambient, which is what puts sky and ground on ONE scale |
| 3 | **Shadow** (depth-only, 4096×1024 D32 atlas, clear 1.0) | `ShadowStage` | shadow atlas | four cascades as four VIEWPORTS into one atlas → one pass whatever the cascade count. Casters: the OSM building prisms |
| 4 | **Scene** (colour `HdrTex` + `VelTex`, depth `DepthTex`, all clear) | `SkyStage` | HDR | fills the background, first drawing in the pass. **Two colour attachments since the temporal round**: every pipeline recorded into this pass declares both (`stages/SceneTargets.h`), the motion one is cleared to the „world-fixed" sentinel, and only the ground cover writes it |
| 3 | ″ | `SunStage` | HDR | additive (One/One) |
| 3 | ″ | `MoonStage` | HDR | additive |
| 3 | ″ | `StarsStage` | HDR | additive, self-gated (EVS night only) |
| 3 | ″ | `TilesStage` | HDR + depth | terrain; render bundle (streaming) or direct draws (static) |
| 3 | ″ | **`UnitsStage`** | HDR | one indexed draw per unit, directly after the terrain; self-gates on an empty cast |
| 3 | ″ | `TileLightsStage` | HDR | night lights, depth-tested, self-gated |
| 3 | ″ | **`SpritesStage`** | HDR, premultiplied (α = 0 ⇒ additive) | ONE instanced draw of every effect billboard, directly before the overlay slot; depth-tested, no depth write; self-gates on an empty list |
| 5 | Cloud layer (`FB_CLOUDS=1` **and** the weather has a deck) | `CloudLayerStage` | `HdrTex`, premultiplied blend | its OWN pass, because it must SAMPLE `DepthTex` (which was still an attachment in the scene pass) |
| 6 | **Ambient occlusion** (colour = half-res R8, clear 1.0) | `AoStage` | AO buffer | same argument as the cloud pass: it samples `DepthTex`. The COMPOSITE is not a pass — the tonemap already reads every scene pixel |
| 7 | **Temporal resolve** (colour = the frame's history texture, clear) | `TaaStage` | `rgba16float` | same argument as the cloud and occlusion passes: it SAMPLES the colour and depth that were attachments a moment ago. Its output is what the tonemap reads instead of `HdrTex`. [`stages/taa.md`](stages/taa.md) |
| 8 | Tonemap (colour `FrameTex`, clear) | `TonemapStage` | 720p sRGB | ACES + the AO composite, weighted by the DIRECT fraction each surface wrote into the HDR alpha (`stages/SurfaceLight.h`) |
| 8 | Overlay (colour `FrameTex`, **LoadOp `Load`**) | the registered `OverlayStage` | 720p sRGB | only when one is registered AND `Active()`; preserves the tonemapped picture. Everything an equipped body draws — symbology, map sheet, sensor video — is this ONE pass |
| 9 | Upscale (colour = final, clear) | `UpscaleStage` | swapchain or offscreen | bilinear |

**Pass counts (the logged invariant).** The shadow and occlusion passes raised it by exactly two:

| Configuration | Passes | Verified |
|---|---|---|
| Standard, no weather deck | **9** (2 compute + shadow + scene + AO + TAA + tonemap + overlay + upscale) | |
| clouds armed AND the weather has a deck | **10** (+ the cloud layer pass) | |
| pedestrian oracle, no overlay, no deck | **8** | `passcount passes=8 clouds=1 cloudPass=0 cloudSheet=1 taa=1 overlay=0`, native AND browser |
| the same with `FB_TAA=0` | **7** | `passcount passes=7 … taa=0` |
| Loading screen | **2** (overlay text into `FrameTex` + upscale) | |

**The temporal resolve raised the count by one, and that is the only pass this round added.** The rule
it does not break is the one that matters: a stage SPLIT may not multiply passes. A new capability that
must sample what was an attachment gets its own boundary, as the shadow, cloud and occlusion passes
each did before it.

No order-critical follow-ups remain after `Finish()`/`Submit()`: the temporal resolve that owned them
(ping-pong flip, history snapshot, timestamp poll) went with the old chain.

#### 2.6 What a motion vector needs, and where the three parts live

A motion vector is not one quantity. `FrameContext` carries three, and each of them exists because the
other two cannot stand in for it:

| Part | Field | Why it is separate |
|---|---|---|
| the previous view-projection | `PrevMvp16` | camera-relative to the PREVIOUS eye |
| the eye's own step | `EyeDeltaM` | which is why a point held at THIS frame's eye has to be carried over before that matrix may be applied to it |
| the two frames' jitters | `JitterNdc`, `PrevJitterNdc` | both matrices carry their own sub-pixel offset, so their difference does too. The resolve subtracts it ONCE, for the depth-reprojected half and the vertex-written half alike, and that is what keeps the two halves from drifting apart |

`HistoryValid` is false on the first frame and after `Renderer::ResetTemporal()`; a caller that PLACES
the camera rather than walking it (the subject bench does, per cell) says so there and then renders
`TemporalSettleFrames()` before it reads the picture. **`ResetTemporal()` restarts the jitter phase as
well as emptying the accumulator** — the phase is part of the history, and a settle that begins at
phase `n mod 8` visits the same eight positions in a rotated order, which the resolve's feedback then
weights unequally. Without the phase reset the settled picture is still a function of how many frames
ran before it. `TemporalSettleFrames()` is **128**, measured; the curve is in
[`stages/taa.md`](stages/taa.md) §7.

**World-fixed geometry writes no motion vector at all** — it is reconstructed from its own depth, which
is exact and costs no attachment write. What an opaque stage does write is the SENTINEL, and only
because it may be drawn over something that moved: a facade in front of a blade would otherwise inherit
the blade's velocity. [`stages/SceneTargets.h`](../../sim/src/render/stages/SceneTargets.h) is where
that contract is stated once instead of in twelve stages.

#### 2.2 The loading screen

A separate, short frame path (`if (LoadingScreen)`): a black `FrameTex`, the overlay's text encode, then
upscale. No scene, no sky. The app keeps the simulation frozen meanwhile — the first live frame is
therefore already at full target resolution, without a low-res ladder. Threshold and timeout live with
the client (`FB_LOAD_THRESH` 0.95; `FB_LOAD_TIMEOUT_MS` 30000 — `clients/AppWasm.cpp`).

**It draws nothing today.** The text pipeline was deleted with the avionics group and no client calls
`SetLoadingScreen`, so the path is reachable and empty — see §7.

---

### 3 Stage catalogue

> **One document per pass lives in [`stages/`](stages/)** — contract, honest state and gaps per pass.
> This table is the topology's view of them: what each stage draws, into what, and what gates it.

| Class | Kind | Target / result | Gate |
|---|---|---|---|
| `TransmittanceStage` | compute | `TransLUT` 256×64 rgba16float | always |
| `MultiScatterStage` | compute | the multiple-scattering LUT | always |
| `SkyViewStage` | compute | `SkyLUT` 192×108 rgba16float | always |
| `IrradianceStage` | compute | the two irradiances — **THE** scale everything else is metered against | always |
| `ShadowStage` | render, depth only | the sun cascade; casters are the OSM building prisms | a caster set exists |
| `SkyStage` | render | sky dome + cloud-deck value-noise sheet | always (SVS pins day=1) |
| `SunStage` | render, additive | sun disc + forward glow | returns `vec4f(0)` outside EVS |
| `MoonStage` | render, additive | moon as an illuminated sphere; owns the NASA LROC albedo texture | as sun |
| `StarsStage` | render, instanced additive | HYG star field at true alt/az | self-gated: no SVS, no day, no catalogue → no draw |
| `TilesStage` | render | terrain, the ground material and the stand over it (§6, [`stages/terrain.md`](stages/terrain.md)) | always, once configured |
| `BenchGroundStage` | render | the `--rig` bench's floor plane and neutral card | self-gated on "no plane declared" |
| `BuildingsStage` | render | the extruded OSM footprints | a building field is set |
| `UnitsStage` | render, indexed | one entity per published pose | self-gated on an empty cast |
| `TileLightsStage` | render, instanced additive | night-light sprites on the ground | self-gated like stars |
| `SpritesStage` | render, instanced premultiplied | flame / trail / flare / fireball / fire / lamp billboards | self-gated on an empty effect list |
| `CloudLayerStage` | render | the whole cloud chain: ray ∩ shell per deck, trapezoid march, premultiplied into `HdrTex` ([`clouds.md`](clouds.md)) | armed AND the weather has a deck |
| `AoStage` | render | screen-space ambient occlusion | always |
| `TaaStage` | render | the temporal resolve, paired with the sub-pixel jitter | always |
| `ExposureStage` | compute | gain + white point, read by the tonemap | always |
| `TonemapStage` | render | ACES → `FrameTex`; one pipeline | always |
| `UpscaleStage` | render | 720p → display resolution, bilinear | always |

In addition three pure **WGSL splice headers** (no classes, no independently compilable shader — the
consumer concatenates them in front of its own code):

| Header | Content | Consumers |
|---|---|---|
| `AtmoCommon.h` | `Atmo` uniform struct + scattering physics | transmittance, sky view, sky, sun, moon, tiles, clouds |
| `AtmoSample.h` | sky-view LUT sampling + exposure (`kSkyExposure = 8.0`) | sky, sun, tiles, clouds |
| `AtmoHaze.h` | **THE air**: Koschmieder σ₀, its molecular/aerosol split, the two scale heights (8 km / 1.2 km), `kMinSunUp`, the per-channel extinction law, the inscatter colour, and the deck's sun transmittance — C++ AND WGSL, like the density below. Its Rayleigh coefficients ARE `AtmoCommon.h`'s, read directly in WGSL and tied to the C++ mirror by a `const_assert` | `TilesStage`, `CloudLayerStage`, `--cloudcheck` |
| `CloudDensityWGSL.h` | the shared cloud density function plus the constants printed from its C++ half | `CloudLayerStage`, `--cloudcheck` |

---

### 4 The atmosphere (Hillaire 2020)

Two compute LUTs plus one fullscreen sky pass. The sky-view LUT is also what the terrain's and the
cloud deck's haze dissolve INTO (`AtmoHaze.h`), so sky, ground and cloud converge on one colour; the
transmittance LUT feeds sky, sun and the cloud march's sun colour, and since the `FB_AP` removal no
longer the terrain.

**Shared resources owned by `Renderer`** (because 3+ consumers read them):

| Resource | Format/size | Note |
|---|---|---|
| `TransLUT` | 256×64 rgba16float, `StorageBinding\|TextureBinding` | parameterised only by altitude × sun cos θ → needs no camera |
| `SkyLUT` | 192×108 rgba16float | single scattering, raymarched |
| `LutSamp` | linear, **U = Repeat** (azimuth wraps), V = ClampToEdge | |
| `AtmoBuf` | 11 × vec4 = 44 floats | see table below |

**Content of `AtmoBuf`** (`Renderer::UpdateAtmosphere`, order = layout):

| Index | Field | Meaning |
|---|---|---|
| 0 | `camPosMm` | eye in megametres (ECEF/1e6) |
| 1 | `sunDir` | sun direction ECEF |
| 2 | `up` | radial up at the eye |
| 3 | `sunTan` | sun direction, tangentially projected |
| 4 | `side` | `up × sunTan` |
| 5–7 | `camRight`, `camUp`, `camFwd` | **rolled** camera basis (only the view-ray reconstruction uses it) |
| 8 | `params` | `tan(30°)`, aspect, `cos(0.5°)` (sun angular radius), `30` (disc intensity) |
| 9 | `moonDir` | xyz direction, w = illuminated phase fraction |
| 10 | `skyExtra` | x = day factor, y = EVS gate, z = cloud cover, w = lunar angular radius `0.0045 × FB_MOON_SCALE` |

**Atmosphere constants** (`AtmoCommon.h`, Hillaire's standard values): `groundRadiusMM = 6.360`,
`atmosphereRadiusMM = 6.460`, Rayleigh base `(5.802, 13.558, 33.1)`, Mie scattering `3.996`, Mie
absorption `4.4`, ozone `(0.650, 1.881, 0.085)`.

**Day factor** (`DaylightFactor`, verbatim from the predecessor code `atmo.h::w3_daylight`):
`t = clamp((sunElDeg + 9)/12, 0, 1)`, then `t²(3−2t)` (smoothstep). Full day above ≈ +3°, dark from
≈ −9° (nautical twilight). ONE number for sky, ground and star fade.

**SVS vs. EVS** (the TAB toggle):

| Mode | Ground source | Sun | Additional effects |
|---|---|---|---|
| SVS (`GroundPhoto = 0`) | OSM render | fixed 45° elevation, azimuth 180° | day factor pinned to 1, no stars/lights/clouds |
| EVS (`GroundPhoto = 1`) | aerial imagery | real ephemeris (`core/Ephemeris.h`) | stars, night lights, clouds, moonlight |

Rationale in the code: SVS is a **time-independent database view**; only EVS is "the real camera", so
only there does dawn/dusk have to match the picture's brightness.

**Init-order contract** (`Renderer::CreateAtmosphere`, documented explicitly as a CONTRACT): WebGPU
bind groups pin a concrete `TextureView` at creation — there is no "rebind later". So the stages have
to be configured in dependency order:

```
Transmittance (owns TransLUT)
   └─ SkyView   (reads TransLUT, writes SkyLUT)
        └─ Sky  (reads SkyLUT + AtmoBuf)
   ├─ Sun       (reads TransLUT for the sun colour + AtmoBuf)
   └─ Moon      (builds the albedo texture from the bytes staged by SetMoonTexture + AtmoBuf)
…only then CreateTerrainPipeline() → TilesStage::Configure(… LutSamp, SkyLUT …)
```

The terrain takes the **LUT sampler**, not the albedo one, and that is a correctness detail rather
than tidiness: the sky-view LUT wraps in azimuth (`AddressMode::Repeat`) and its seam sits at u = 0 —
which is the SUN's own azimuth. The removed `FB_AP` block sampled it with the ClampToEdge tile
sampler, i.e. with a filtered seam through the brightest part of the far field.

Ephemerides (`core/Ephemeris.h`, pure functions — moved down out of `render/` in the C2 round
because `core/`/`sensors/` may not include `render/` and visual acquisition needs the sun,
the run's declared instant): `SunPos` is a verbatim port of the NOAA
approximation formulas (< ~0.5° error); `MoonPos`/`MoonPhase` are a port of Paul Schlyter's
approximation (public domain) **without** its long perturbation-term table — good to about a degree,
enough for a disc plus phase, not for navigation. The phase is `(1 − cos(elongation))/2`.

---

### 5 The cloud chain

Moved out into [`clouds.md`](clouds.md) — the cloud chain has a spec of its own (rebuild) and a state
of its own.

---

### 6 The terrain stage in detail

`TilesStage` is the only stage with real per-frame CPU state.

**Vertex layout** (`render/ChunkVtx.h`, `w3_vtx`): `pos[3]`, `uv[2]`, `norm[3]` = 32 B, tightly
packed, with a `_Static_assert` on size and offsets 0/12/20. The reason is in the header: writer (tile
worker) and reader (draw call) used to agree only **by hand** — when the normals were added, the stride
moved from 20 to 32 and every hand-written number had to move with it; an error there renders garbage
instead of breaking.

**Geometry**: `pos` is the ECEF **offset to the tile origin** (float; at z14 < 2 km → sub-centimetre),
`origin` is the double anchor the frame subtracts. The construction is in `render/ChunkMesh.h`
(`w3_chunk_build_ecef`): the same regular grid structure as the ENU path, but every node is projected
through the exact inverse Mercator and geodetic→ECEF — no tangent-plane error, no dependency on a home
origin. Normals are real ECEF cross products of the neighbouring offsets and therefore carry the tile's
curvature for free. `err` = maximum height error of the decimation in metres — projection-independent,
and exactly the number the streamer's LOD builds on.

**The frame uniform** (`U`, 36 floats = 144 B, written once per frame in `Encode`): the camera-relative
MVP + sun (the `FrameContext`'s own `Mvp20`), then the ATMOSPHERE as numbers —
`haze = (σ₀, camera altitude ASL, ground radius under the camera in Mm, –)` and one `vec4` per weather
deck, `(baseM, topM, sun optical depth, cover)`. The decks arrive through `Renderer::SetCloudSky`,
which hands the SAME `CloudSky` to the cloud stage and to this one — the seam that makes "deck and
ground see one atmosphere" a fact of the wiring rather than a claim.

**Light and air in the fragment shader** (all of it out of `stages/AtmoHaze.h`, shared verbatim with
the cloud march):

| Step | Formula | Note |
|---|---|---|
| deck attenuation | `sunThru = Π_i [(1−cover_i) + cover_i·exp(−τ_i·frac_i)]`, `frac_i = clamp((top_i − z_frag)/thick_i, 0, 1)` | statistical, per deck, per fragment — **not** a shadow map. `cover` is calibrated to be an area fraction, so `1−cover` really is the share of sun rays that miss the deck; `frac` is what makes a ridge inside the deck partly lit and one above its top fully lit |
| lit albedo | `albedo · (0.4 + 0.15·(1−sunThru) + 3.0·N·L·sunThru) · light` | the direct term is what a deck takes away; `0.15` is what it gives back as diffuse (derivation in the shader comment: overcast diffuse illuminance ≈ 1.0–1.5× clear-sky diffuse) |
| haze | `T₁₃ = exp(−[β_R·exp(−z̄/8000) + σ_A·exp(−z̄/1200)]·d)`, `c = c·T + inscatter·(1−T)` | σ₀ = 3.912/visibility (Koschmieder) split into a fixed molecular part β_R and the aerosol remainder σ_A; two scale heights, and β_R is a **vec3** (λ⁻⁴) so the extinction colours. `z̄` = mean altitude of the sight line, `d` = the camera-relative fragment distance. Derivations + citations: [`clouds.md`](clouds.md) |
| inscatter | `skyViewSample(dir↓horizon) + the sun halo × EVS` | identical to what the sky pass paints, so the horizon has no edge; a below-horizon direction is projected onto the horizon, because the LUT's below-horizon branch answers a different question (`AtmoHaze.h` comment) |

Telemetry: `Log::Debug("render","terrain_light")` on change — `visM`, `sigma0PerM`, the three decks'
`τ`, `groundSunThru` and `groundLitFrac` (the share of ground radiance that is direction-dependent
direct light). That last number is how "flat and grey under a closed deck" is measured instead of
eyeballed: 0.882 clear → **3.6·10⁻⁶** under 100 % low cover.

**Per-draw data** (storage buffer `TileBuf`, one `Tile{a: vec4f, b: vec4f}` per draw, 32 B):

| Field | Meaning |
|---|---|
| `a.xyz` | `origin_ecef − cam_ecef` (float, camera-relative) |
| `a.w` | layer in the albedo array |
| `b.x` | per-tile brightness gain of the imagery (1.0 for OSM) |

The draw selects its entry via `firstInstance`, so `instance_index == draw index`.

**Albedo**: `texture_2d_array`, edge length 512 (client's choice), **growing** up to the adapter's real
`maxTextureArrayLayers` (target device 2048; default cap 256). What is uploaded is always a **complete
sRGB mip pyramid** (`render/Mips.h`: colour averaging in LINEAR light — decode, average, re-encode,
so that distance does not darken; alpha is already linear). Two layers per tile are possible: the
eagerly baked **base** layer and the lazily loaded **photo/overlay** layer.

**Sampler** (`Renderer::CreateTileTexture`, shared with the tonemap): ClampToEdge (a bake IS a tile —
nothing wraps), linear/linear, **trilinear** over the mip chain, `maxAnisotropy = 16`.

**The grazing mip bias** (terrain fragment shader): at grazing view angles the UV footprint anisotropy
exceeds the 16:1 hardware cap → vertical streaks. Correction:
`gbias = clamp(1.0 · (−log2(grazeV) − 2.5), 0, 1.2)` with `grazeV` = downward component of the view
ray. It only kicks in below ≈ 10° of depression, cap 1.2 (≈ 2.3× the footprint). The comment calls this
explicitly a **user decision** (2026-07-23): sharpness beats freedom from streaks; a residual streak in
the outermost horizon band is accepted.

**Render bundle**: signature = FNV-1a over the draw **structure** (count, bind-group handle after an
array growth, per tile the vertex-buffer handle + vertex count). `TileBuf` and uniform **contents**
change every frame, but the bundle references those buffers only by handle — which is why only a
structural change triggers a re-record (a few per second on approach, **zero** when parked). Counter:
`GetBundleRecords()`.

**Two-phase commit**: a tile may only be drawn **one pass AFTER** its GPU upload
(`FrameNo > PhotoUpTick + 1` for the overlay layer; the streamer side mirrors this, see
`world-and-terrain.md`). Otherwise a draw references a layer whose `WriteTexture` is not yet visible →
a black frame.

**Invariant counters** (once per second as `Log::Debug("render","present")` with a `violation` flag):

| Counter | Meaning — should stay 0 |
|---|---|
| `notReadyDraws` | a draw without a committed layer |
| `wrongModeDraws` | SVS showed EVS or vice versa (mode bleed) |
| `blackDraws` | layer index < 0 |

---

### 7 There is no text stage

`render/OverlayStage.h` is the seam and nothing implements it. The glyph pipeline, the geometry buffer
and the baked font ROM lived in the avionics group and were deleted on 2026-08-07. **No client calls
`SetLoadingScreen`, so nothing regresses** — but a pedestrian loading screen, a debug read-out or any
text at all needs a new stage in `render/`.

---

### 8 Camera and ground truth

#### 8.1 `Camera.h` — attitude in, basis out

The file is deliberately **pure maths**: no GL, no globals, no side effects. It deliberately does
**not** compute the eye POSITION — that depends on the terrain and belongs to the tile side.

Render space is ENU with `E = +X`, `up = +Y`, `N = −Z`. The basis comes from yaw/pitch, then it is
rolled about the forward axis:

```
up = u·cos(roll) + s·sin(roll)
sr = s·cos(roll) − u·sin(roll)
```

The `+s` is the place that **mirrors silently**: it once read `−s`, and a right bank looked like a left
bank. Nothing crashed, nothing looked broken — a tilted horizon is a tilted horizon in either
direction. Precisely for that reason the logic exists ONCE (`w3_basis_from`) and is used by both paths
instead of copied.

**`CameraBasisEcef`** is the ONE attitude→ECEF basis that native and WASM share. It used to exist
character-identically twice — in `AppNative.cpp` and `AppWasm.cpp`, each with a comment asking that
it be kept identical to the other. That made "the oracle's frames" and "the browser's frames" the same
camera only **by hand**. It computes in **doubles** (the eye lies in the 6.4e6 m range) and rotates the
render basis into ECEF via `EnuAxesEcef` (`core/Geodesy.h`, closed form, right-handed, det +1 →
triangle winding is preserved); the render→ENU mapping is `(e, n, u) = (x, −z, y)`.

The renderer knows three camera sources, in this order of precedence:

| Source | Setter | Property |
|---|---|---|
| Full basis | `SetCameraBasis(eye, fwd, right, up)` | carries **roll** — the horizon tilts in a bank. Wins over both others. |
| Scripted camera | `SetCamera(eye, target)` | up is derived radially, **no** roll |
| Default orbit | — | circles above the terrain field's centre (1500 m high, 6500 m radius, 0.2 rad/s) — bring-up/still image only |

**Horizon dip** (`w3_horizon_dip_rad`): `acos(R/(R+h))` with `R = 6 371 000 m` (mean spherical radius —
the terrain stands on the WGS84 ellipsoid, but the dip is a small-angle spherical result for which the
mean radius is the standard value). A horizon line drawn level reads too HIGH at altitude; it has to
fall by exactly this angle so that it overlays the real, curved-away horizon (MIL-STD-1787, conformal).
It is consumed by the overlay symbology, not by the terrain pass — there the curvature arises by
itself, because the ECEF tiles really do tilt away.

#### 8.2 Eye height from the model geometry

The eye height at ground level does **not** come from a fixed number but from the BODY's own contact
geometry: the largest downward offset over its ground-contact points, measured from the reference point.

It is used at the spawn seam: a body asked to "stand on its contacts" is placed at
`GroundElevM + clearance` once its reference point is valid, and initialised again. The spawn height is
thereby geometry-true instead of guessed; there is no "held→live" jump.

> **This is the one paragraph of this file whose supplier the cut removed.** The clearance used to be
> read off a third-party solver's ground reactions; that solver is gone and the body format that
> replaces it is spec-only ([`../body-format.md`](../body-format.md)). The RULE stands — the number is a
> property of the body's contacts and never a constant — and the implementation is roadmap R5's.

The camera itself is, in today's client, **the controlled body's eye**: geodetic → ECEF → `SetCameraBasis`.
A clamp "never below the surface" is **not** wired — see *Gaps*. The pedestrian client
(`clients/AppWalk.cpp`) is the same seam with the clearance supplied on the command line (`--ground`,
`--eye`), which is what lets a picture be proved while the body format is unbuilt.

**In the browser's DIRECTED view it is not the eye but a director's** (`clients/CameraDirector`,
contract in [`../clients/clients.md`](../clients/clients.md)). Nothing in `render/` changed for it and
nothing may: it produces the same `SetCameraBasis` quadruple, from published poses and published damage
signatures only, and the renderer cannot tell which of the two wrote it. One thing DOES follow through
to this file — the camera's own lat/lon is handed to `World::Update` rather than the aircraft's,
because a tripod standing at a wreck twenty kilometres behind the flight needs the quadtree refined
under the WRECK. The framing distances are solved out of this file's own field of view: at
the scene's declared 60° a feature of height *h* at range *D* fills `h / (2·D·tan 30°)` =
`h / (1.1547·D)` of the frame (the FOV is `Scene::FovDeg()`, a runtime number, so this equation is
solved against whatever the scene declared), and every tripod range in that class is that equation with a chosen
fraction (the wreck fire, 22× its own height, is 3.9 % = 28 of 720 lines).

#### 8.3 "Crash → engine off, no freeze"

The physical judge (`core/FlightMonitor`) belongs to the client, not to the module. When it trips,
`units/SimUnit::RunMonitors` cuts the engine **through the same controls path a pilot would use**
— and does nothing else. **No freeze, no special case in the renderer:** the physics keeps computing,
the wreck slides, the renderer draws it. A run's FAIL verdict cuts the propulsion ONLY if the entity was
still effective — for a kill that would be the verdict acting on the body instead of the damage.

---

### 9 The rule `grep -c 'R"(' Renderer.cpp` == 0

**Satisfied today: 0.** Not a single raw WGSL string literal remains in `Renderer.cpp`.

What the rule states: the render stage split is **complete**, and verifiably so with a one-liner rather
than with a judgement. Every piece of WGSL lives in exactly one `stages/` file; `Renderer` is
therefore **only** an orchestrator — device, targets, pass boundaries, order, shared resources. A new
shader idea can no longer wander "just quickly" into the orchestrator, because the rule would make it
visible immediately.

Distribution today, measured 2026-08-07 by `grep -o 'R"('`: **38 WGSL blocks over 31 stage files**, and
**0 in `Renderer.cpp`**. Plus **4 pure splice headers** without a class of their own (`AtmoCommon.h`,
`AtmoSample.h`, `AtmoHaze.h`, `CloudDensityWGSL.h`). Two of the four carry a C++ half of the same
formula and were checked against their shader twin by a `--cloudcheck` mode that **no longer exists** —
it lived in the deleted `gpu_native`; the C++/WGSL twins are unchecked today.
