# Rendering — der WebGPU-Renderer

**Quellen dieser Datei:** `sim/src/render/` (14 Dateien: `FBRenderer.h/.cpp`, `FBCamera.h`,
`FBDrawStage.h`, `FBFrameContext.h`, `FBGpu.h`, `FBHudGeometry.h/.cpp`, `FBHudFont.h`,
`FBHudFontRom.h`, `FBChunkMesh.h`, `FBChunkVtx.h`, `FBMips.h`, `FBEphemeris.h`) und
`sim/src/render/stages/` (39 Dateien), plus CLAUDE.mds Abschnitte `render/`, `render/stages/` und
„Rendering (das Herzstück)". Jede Zahl unten steht so im Quelltext; Herleitungen und Setzungen sind
als solche gekennzeichnet. Widersprüche zwischen CLAUDE.md und Code stehen unter *Offene Punkte* —
sie sind nicht stillschweigend aufgelöst.

Nachbardateien: `architecture.md` (Lib/Client-Split), `world-and-terrain.md` (was den Renderer mit
Geometrie und Albedo füttert). Die HUD-**Symbologie** (was gezeichnet wird) gehört nicht hierher,
sondern zu `systems/FBDisplaySystem` + `modules/f16/displays/FBF16Hud`; diese Datei beschreibt nur
das **Backend** (womit gezeichnet wird).

---

## 1 Grundentscheidungen

| Entscheidung | Umsetzung | Herleitung / Grund |
|---|---|---|
| **API: WebGPU** | EIN Quelltext, ZWEI Link-Ziele — emdawnwebgpu (Browser) und natives Dawn (`gpu_native`) | Dieselbe Dawn-Header-Familie beidseitig: „write once, link twice". Native Dawn rendert wirklich; ein headless Browser mit SwiftShader gibt diesen Beweis nicht. |
| **WGS84-ECEF, camera-relative** | Vertices tragen den Versatz zum Tile-Origin; das Frame subtrahiert `origin_ecef − cam_ecef`; die Kamera sitzt im Ursprung | Keine absoluten 6,4e6-m-Koordinaten erreichen je ein `float`. Präzision ist überall auf der Erde AM Auge am besten. |
| **Reversed-Z (Depth32Float)** | Clear `0.0`, `CompareFunction::Greater`, unendliche Ferne, `zn = 0.05 m` | `[0,1]`-Clip (nativ, nicht GLs `[-1,1]`) → volle Mantisse für die Tiefe; fernes Gelände z-fightet nicht. |
| **HDR + ACES** | Szene → `rgba16float`-Ziel, ein Fullscreen-Tonemap (Narkowicz-ACES-Fit) → sRGB | Licht bleibt bis dahin linear; Display-Encoding passiert an GENAU EINER Stelle (dem Tonemap-Pass, dessen sRGB-View beim Store encodiert). |
| **RenderBundle-Submission** | Terrain-Draws werden einmal aufgezeichnet und je Frame abgespielt; Neuaufzeichnung nur bei STRUKTUR-Änderung | ~N CPU-Draw-Encodes werden zu einem `ExecuteBundles`. |
| **Feature-Gates = gebackene Konstanten** | Env-getriebener String-Replace am Shader-Quelltext vor `CreateShaderModule` | Ein toter Pfad wird vom Shader-Compiler wegoptimiert und kostet nichts — kein Laufzeit-Branch. |
| **Fixes 720p-Frametarget** | Szene + Tonemap + HUD landen in `FrameTex` (1280×720); ein Upscale-Pass legt es auf die Swapchain | Die Sim-Auflösung ist stabil, die Anzeige folgt Canvas × DPR. |

### 1.1 Zwei Link-Ziele, ein Renderer

| | Browser (WASM/emdawnwebgpu) | Native (Dawn, `gpu_native`) |
|---|---|---|
| Bring-up | `FBRenderer::Init(canvasSelector, w, h)` — asynchron, Callbacks `AllowSpontaneous`; `Ready()` pollt die App | `FBRenderer::InitOffscreen(w, h)` — blockierend über `Instance::WaitAny(future, UINT64_MAX)` |
| Voraussetzung | Browser-Event-Loop pumpt die Callbacks | Instance-Feature `TimedWaitAny` wird beim `CreateInstance` angefordert |
| Finales Ziel | `wgpu::Surface` auf `#gpu`, Format = erstes sRGB-fähige aus `SurfaceCapabilities` (sonst `BGRA8Unorm`) | `OffscreenTex`: `RGBA8UnormSrgb`, Usage `RenderAttachment\|CopySrc` |
| Rückgabe | — | `ReadPixels()` → dicht gepacktes W·H·4 RGBA8, bereits sRGB-codiert → direkt für `stb_image_write` |

Beim Adapter wird protokolliert, **worauf** WebGPU wirklich läuft (`FBLog::Info("render","adapter")`):
`adapterType == CPU` heißt Software-Rasterizer (SwiftShader/lavapipe/WARP) — dann ist hohe CPU-Last
der Browser, nicht unser Code. Ebenfalls geloggt: `maxTextureArrayLayers`, `maxBufferSize`,
`maxTextureDimension2D`.

Geräteverlust ist kein Absturz: der `DeviceLostCallback` setzt `DeviceLost`, GPU-Operationen werden
danach übersprungen, die CPU-Seite (Tile-Streaming, Zähler) läuft weiter. `DeviceUsable()` =
`DeviceReady && !DeviceLost`.

### 1.2 Camera-relative ECEF und die Projektion

`MvpCamRel()` (`FBRenderer.cpp`, static) baut **Projektion × View** so:

- View = reine Rotation aus der ECEF-Kamerabasis (`right`, `camUp`, `−fwd`) — keine Translation, weil
  die Vertices schon vorverschoben ankommen.
- Projektion = unendliches Reversed-Z: `p = {f/asp, 0,0,0, 0,f,0,0, 0,0,0,−1, 0,0,zn,0}` mit
  `f = 1/tan(fov/2)`, `fov = 60°`, `zn = 0.05`. Daraus `z_clip = zn`, `w = −z_eye`, also
  **`depth = zn / (−z_eye)`** — monoton fallend mit der Entfernung, 1 bei `zn`, → 0 im Unendlichen.
- Keine explizite Far-Plane. Die „Sichtweite" ist eine Eigenschaft des **Streamings** (Radius
  `FB_VIEW_KM`, Default 240 km — s. `world-and-terrain.md`), nicht der Projektion.

Der 20-float-Block `Mvp20` im `FBFrameContext` ist diese Matrix (Index 0..15, column-major) plus die
Sonnenrichtung (16..18) plus ein Pad — genau so, wie ihn die Terrain-Uniform erwartet.

### 1.3 Tiefenzustände je Stage

| Stage | `depthWriteEnabled` | `depthCompare` | Wirkung |
|---|---|---|---|
| `FBSkyStage` | false | `Always` | Hintergrund; alles zeichnet darüber |
| `FBSunStage`, `FBMoonStage` | — (additiv, im selben Pass direkt nach Sky) | wie Sky | reine Addition, reihenfolgeunabhängig |
| `FBStarsStage` | false | `Always` | „im Unendlichen"; Terrain übermalt sie |
| `FBTilesStage` | true | `Greater` | Reversed-Z: näher = größer |
| `FBTileLightsStage` | false | `GreaterEqual` | Hügel verdecken ferne Lichter, aber die Sprites schreiben keine Tiefe |

### 1.4 HDR-Format: warum `rgba16float` und nicht `rg11b10ufloat`

`rg11b10ufloat` war die Bandbreiten-Wahl und ist **verworfen**: es hat keinen Alphakanal und keine
garantierte Blend-Unterstützung — der Wolken-Pass blendet premultipliziertes Alpha darüber, das ging
kaputt. `rgba16float` ist das Standard-Blend-fähige HDR-Format; die 4 zusätzlichen Byte/Pixel bei
720p sind vernachlässigbar (`FBRenderer::OnAdapter`, Kommentar dort).

### 1.5 Der Present-Pfad

| Ressource | Größe/Format | Rolle |
|---|---|---|
| `HdrTex` | Width×Height, `HdrFormat` (rgba16float), `RenderAttachment\|TextureBinding` | Szenenziel, lineare Radianz |
| `DepthTex` | Width×Height, `Depth32Float`, `RenderAttachment\|TextureBinding` | Reversed-Z; der Wolken-Pass SAMPLED sie (deshalb `TextureBinding`) |
| `FrameTex` | Width×Height (1280×720), `SurfaceFormat` (sRGB), `+CopySrc` | Szene+Tonemap+HUD landen hier |
| Swapchain | Canvas `clientSize × devicePixelRatio`, gekappt bei 4096 | folgt der Anzeige |

`SyncSwapSize()` rekonfiguriert die Surface nur, wenn sich Breite ODER Höhe um **≥ 8 px** ändert
(Hysterese gegen Sub-Pixel-Jitter). Szene und HUD bleiben davon unberührt — nur Swapchain und
Upscale-Viewport folgen.

### 1.6 Feature-Gates und Env-Schalter

Der Mechanismus ist zweistufig: entweder wird eine **Konstante in den WGSL-Quelltext gebacken**
(String-Replace vor `CreateShaderModule`, dann strippt der Shader-Compiler den toten Block), oder
eine ganze Ressourcen-/Pipeline-Gruppe wird **gar nicht erst gebaut**.

| Schalter | Default | Wirkung | Ort |
|---|---|---|---|
| `FB_CLOUDS` | 0 (aus) | armiert den kompletten Volumetrik+Dome-Wolkenpfad. Aus = `CreateClouds()` läuft nie: keine Noise-Volumes, keine Pipelines, kein VRAM | `FBRenderer::OnDevice` |
| `FB_AP` | 0 (aus) | Terrain-Aerial-Perspective. `const AP_ON : f32 = 0.0` wird im Terrain-Shader ersetzt; aus = der ganze tLUT/Inscatter/Glow-Block strippt weg | `FBTilesStage.cpp` |
| `FB_CLOUD_QUALITY` | 1.0 | skaliert die Schrittzahl des Raymarch; 0 schaltet den Pass ab | `FBCloudMarchStage` |
| `FB_MOON_SCALE` | 1.0 | Multiplikator auf den echten Mondwinkelradius (0,0045 rad ≈ 0,5°) | `FBRenderer::UpdateAtmosphere` |
| `FB_CLOUD_CELLS` | 1 (an) | F1-Zellenfeld; aus = dessen `worley2D`-Erzeugung wird aus dem Bake-Shader ersetzt | `FBCloudBaseBakeStage`, `FBCloudMarchStage` |
| `FB_BASE_*`, `FB_SS_*`, `FB_D2_*`, `FB_CELL_KM`, `FB_CELL_DOME`, `FB_CONE_R`, `FB_MOONLIGHT`, `FB_CLOUD_BASE_M`, `FB_CLOUD_THICK_M` | s. Code | Sweep-Haken der Wolken-Forschung (`doc/clouds/`) | Cloud-Stages |
| `FB_PHOTO_ZMAX` | 11 | ab welchem Zoom eine Luftbild-Kachel als „hell genug" gilt (Gain-Rechnung) | `FBTilesStage.cpp` |
| `FB_PHOTO_EMA` / `FB_PHOTO_MAXGAIN` / `FB_PHOTO_LOG` | 0.08 / 2.5 / aus | adaptive Helligkeitsanpassung der Luftbild-Layer | `FBTilesStage.cpp` |
| `FB_SHAPEHIST` | aus | numerisches Dichte-Histogramm des Basis-Noise (Lab-Werkzeug) | `FBCloudBaseBakeStage::ShapeStats` |
| `FB_GPU_NOOP` / `FB_GPU_BISECT` | Compile-Defines | Bisect-Stufen: inerte Frames bzw. nur Acquire — trennt Init-seitigen von Frame-seitigem Gerätetod | `FBRenderer::RenderFrame` |

---

## 2 Die Pass-Topologie als Vertrag

**Die Regel:** `FBRenderer` besitzt Instance/Adapter/Device/Queue/Surface/Targets, **jede**
`BeginRenderPass`/`BeginComputePass`-Grenze und die Encode-Reihenfolge. Eine `FBDrawStage`
(`render/FBDrawStage.h`) zeichnet **in den geliehenen Encoder**, den `FBRenderer` bereits geöffnet
hat, und öffnet oder schließt **nie** selbst einen Pass.

Warum das eine Regel und keine Stilfrage ist:

1. **Die Pass-Zahl ist eine Messgröße.** Der Stage-Split durfte die Anzahl der `Begin*Pass`-Aufrufe
   pro Frame nicht verändern; `RenderFrame` zählt sie deshalb mit (`passCount`) und loggt sie
   (`FBLog::Debug("render","passcount")`) beim ersten SZENEN-Frame und danach alle 300 Frames. Ein
   Vorher/Nachher-Diff ist damit direkt aus der Telemetrie ablesbar.
2. **Ein Pass ist teuer, ein Draw nicht.** Dürfte jede Stage eigene Pass-Grenzen ziehen, würde jede
   neue Stage die Topologie vermehren — schleichend und unauffällig.
3. **Attachments sind Vertragssache.** Wer den Pass öffnet, bestimmt Ziel-Views, Load/Store-Ops und
   Clear-Werte. Der Cloud-Resolve schreibt z. B. in ZWEI Attachments, deren Ping-Pong-Index der
   Renderer VOR `Encode()` abfragt — die Stage kann diesen Descriptor gar nicht selbst bauen.

Ergänzende Regeln aus `FBDrawStage.h`:

- **Eine Stage self-gated.** „Nichts sichtbar diesen Frame" heißt: `Encode()` zeichnet nichts. Der
  Renderer ruft jede Stage in ihrem Slot **bedingungslos** auf (die einzige Ausnahme ist der HUD-Pass,
  dessen `if (HudEnabled)` außen sitzt — ein leerer Pass wäre trotzdem ein Pass, und die
  Pass-Zahl-Invariante lebt an genau diesem äußeren `if`).
- **Zwei `Encode`-Formen.** `Encode(ctx, RenderPassEncoder&)` und
  `EncodeCompute(ctx, ComputePassEncoder&)`; die jeweils andere bleibt der inerte Default.
- **`FBGpu`** (Device/Queue/Formate/Größe/Instance) bekommt eine Stage EINMAL bei `Init`/`Configure`,
  nie pro Frame. **`FBFrameContext`** ist der geteilte Pro-Frame-Zustand (Kamerabasis, MVP, Sonne,
  Mond, Tag-Faktor, Wetter, Frame-Nummer, Größe) — eine Stage greift nie in `FBRenderer` zurück.

### 2.1 Die vollständige Encode-Reihenfolge

Reihenfolge wie in `FBRenderer::RenderFrame()`:

| # | Pass | Stage(s) | Ziel | Anmerkung |
|---|---|---|---|---|
| 1 | Compute | `FBTransmittanceStage` | `TransLUT` (256×64) | jeden Frame neu (TODO: cachen, solange die Sonne steht) |
| 2 | Compute | `FBSkyViewStage` | `SkyLUT` (192×108) | liest `TransLUT` |
| 3 | **Scene** (Color `HdrTex`, Depth `DepthTex`, beide Clear) | `FBSkyStage` | HDR | füllt den Hintergrund, erste Zeichnung im Pass |
| 3 | ″ | `FBSunStage` | HDR | additiv (One/One) |
| 3 | ″ | `FBMoonStage` | HDR | additiv |
| 3 | ″ | `FBStarsStage` | HDR | additiv, self-gated (nur EVS-Nacht) |
| 3 | ″ | `FBTilesStage` | HDR + Depth | Terrain; RenderBundle (Streaming) oder Direktdraws (statisch) |
| 3 | ″ | **`FBUnitsStage`** | HDR | **NoOp**, aber fest verdrahtet: KI-/Waffen-Einheiten zeichnen direkt nach dem Terrain |
| 3 | ″ | `FBTileLightsStage` | HDR | Nachtlichter, tiefen-getestet, self-gated |
| 3 | ″ | **`FBSpritesStage`** | HDR | **NoOp**, fest verdrahtet: Effekt-Billboards direkt vor dem HUD |
| 4 | Cloud-March (nur `FB_CLOUDS=1`) | `FBCloudMarchStage` | `CloudLowTex` (¼-Auflösung) | EIGENER Pass, weil er `DepthTex` SAMPLEN muss (die im Szenenpass noch Attachment war) |
| 5 | Cloud-Resolve (nur `FB_CLOUDS=1`) | `FBCloudResolveStage` | `CloudHist[w]` + `CloudWSum[w]` (zwei Attachments) | temporales Upsampling in die Ping-Pong-Historie |
| 6 | Tonemap (Color `FrameTex`, Clear) | `FBTonemapStage` | 720p sRGB | ACES; komponiert dabei die Wolke, falls armiert |
| 7 | HUD (Color `FrameTex`, **LoadOp `Load`**) | `FBHudStage` | 720p sRGB | nur wenn `HudEnabled`; erhält das getonemappte Bild |
| 8 | Upscale (Color = final, Clear) | `FBUpscaleStage` | Swapchain oder Offscreen | bilinear |

**Pass-Zahlen (die geloggte Invariante):**

| Konfiguration | Passes |
|---|---|
| Standard, Wolken aus | **6** (2 Compute + Scene + Tonemap + HUD + Upscale) |
| `FB_CLOUDS=1` | **8** (+ March + Resolve) |
| HUD aus (Cloud-Lab) | **5** |
| Ladebildschirm | **2** (HUD-Text in `FrameTex` + Upscale) |

Nach `Finish()`/`Submit()` folgen noch drei reihenfolgekritische Nacharbeiten, die keine Passes sind:
`Cloud->ResolveTimestamps(enc)` **vor** `Finish`, `Cloud->PollTimestamps()` **nach** `Submit`, und
`Resolve->Advance(ctx)` (Ping-Pong-Flip + Snapshot der View-Proj/Eye als „previous"), erst **nachdem**
der Tonemap dieses Frames Ergebnis gelesen hat.

### 2.2 Der Ladebildschirm

Ein eigener, kurzer Frame-Pfad (`if (LoadingScreen)`): schwarzes `FrameTex`, `Hud->EncodeLoadingText`
(nur die Text-Pipeline: „LOADING TERRAIN x%" + Kachelzähler), dann Upscale. Keine Szene, kein Himmel.
Die App hält JSBSim solange eingefroren — der erste geflogene Frame ist damit schon in voller
Zielauflösung, ohne Low-Res-Leiter. Schwelle und Timeout liegen beim Client (`FB_LOAD_THRESH` 0,95;
`FB_LOAD_TIMEOUT_MS` 30000 — `app/FBAppWasm.cpp`).

---

## 3 Stage-Katalog

| Klasse | Datei (`render/stages/`) | Art | Ziel / Ergebnis | Gate |
|---|---|---|---|---|
| `FBTransmittanceStage` | `.h/.cpp` | Compute | `TransLUT` 256×64 rgba16float | immer |
| `FBSkyViewStage` | `.h/.cpp` | Compute | `SkyLUT` 192×108 rgba16float | immer |
| `FBSkyStage` | `.h/.cpp` | Render | Himmelsdom + Wolkendecken-Value-Noise-Sheet | immer (SVS pinnt Tag=1) |
| `FBSunStage` | `.h/.cpp` | Render, additiv | Sonnenscheibe + Vorwärts-Glow | gibt `vec4f(0)` außerhalb EVS |
| `FBMoonStage` | `.h/.cpp` | Render, additiv | Mond als beleuchtete Kugel; besitzt die NASA-LROC-Albedotextur | wie Sun |
| `FBStarsStage` | `.h/.cpp` | Render, instanziert additiv | HYG-Sternfeld an wahrem Alt/Az | self-gated: kein SVS, kein Tag, kein Katalog → kein Draw |
| `FBTilesStage` | `.h/.cpp` | Render | Gelände (s. §6) | immer, sobald konfiguriert |
| `FBUnitsStage` | `.h` | — | **NoOp** | — |
| `FBTileLightsStage` | `.h/.cpp` | Render, instanziert additiv | Nachtlicht-Sprites am Boden | self-gated wie Stars |
| `FBSpritesStage` | `.h` | — | **NoOp** | — |
| `FBCloudMipDownStage` | `.h/.cpp` | Compute-Helfer (Init) | Box-Downsample 2×2×2 für Mip-Ketten; **submittet eigene Command-Buffer** (kein Frame-Stage) | Wolken armiert |
| `FBCloudBaseBakeStage` | `.h/.cpp` | Bake (einmal) | Perlin-Worley-Basisvolumen 128³ RGBA8, volle Mip-Kette | ″ |
| `FBCloudDetailBakeStage` | `.h/.cpp` | Bake (einmal) | Worley-Oktaven-Detailvolumen 32³, volle Mip-Kette | ″ |
| `FBCloudCellBakeStage` | `.h/.cpp` | Bake (einmal) | 512² F1-Zellenfeld, EINE Mip-Stufe | ″ |
| `FBCloudMarchStage` | `.h/.cpp` | Render | Raymarch in `CloudLowTex` (¼-Auflösung, premultipliziert) | ″ |
| `FBCloudResolveStage` | `.h/.cpp` | Render (2 Attachments) | temporales Upsampling in `CloudHist`/`CloudWSum` Ping-Pong | ″ |
| `FBTonemapStage` | `.h/.cpp` | Render | ACES → `FrameTex`; **ein Shader-Quelltext, zwei Pipelines** | immer |
| `FBHudStage` | `.h/.cpp` | Render | HUD-Overlay (s. §7) | `HudEnabled` |
| `FBUpscaleStage` | `.h/.cpp` | Render | 720p → Anzeigeauflösung, bilinear | immer |

Dazu drei reine **WGSL-Splice-Header** (keine Klassen, kein eigenständig kompilierbarer Shader — der
Konsument konkateniert sie vor seinen eigenen Code):

| Header | Inhalt | Konsumenten |
|---|---|---|
| `FBAtmoCommon.h` | `Atmo`-Uniform-Struct + Streuungsphysik | Transmittance, SkyView, Sky, Tiles (Aerial Perspective) |
| `FBAtmoSample.h` | Sky-View-LUT-Sampling + Belichtung (`kSkyExposure = 8.0`) | Sky, Tiles |
| `FBCloudNoiseCommon.h` | Hash/Worley/Perlin/`remap` | die drei Bake-Shader + March (nur `remap`) |

---

## 4 Die Atmosphäre (Hillaire 2020)

Zwei Compute-LUTs plus ein Fullscreen-Himmelspass; dieselbe Transmittance-LUT treibt auch die
Aerial-Perspective des Geländes, damit Himmel und Boden dieselbe Luft sehen.

**Geteilte, `FBRenderer`-eigene Ressourcen** (weil 3+ Konsumenten lesen):

| Ressource | Format/Größe | Anmerkung |
|---|---|---|
| `TransLUT` | 256×64 rgba16float, `StorageBinding\|TextureBinding` | nur Höhe × Sonnen-cos-θ parametrisiert → braucht keine Kamera |
| `SkyLUT` | 192×108 rgba16float | Einfachstreuung, raymarched |
| `LutSamp` | linear, **U = Repeat** (Azimut wickelt), V = ClampToEdge | |
| `AtmoBuf` | 11 × vec4 = 44 float | s. Tabelle unten |

**Inhalt von `AtmoBuf`** (`FBRenderer::UpdateAtmosphere`, Reihenfolge = Layout):

| Index | Feld | Bedeutung |
|---|---|---|
| 0 | `camPosMm` | Auge in Megametern (ECEF/1e6) |
| 1 | `sunDir` | Sonnenrichtung ECEF |
| 2 | `up` | radiales Up am Auge |
| 3 | `sunTan` | Sonnenrichtung, tangential projiziert |
| 4 | `side` | `up × sunTan` |
| 5–7 | `camRight`, `camUp`, `camFwd` | **gerollte** Kamerabasis (nur die Sichtstrahl-Rekonstruktion nutzt sie) |
| 8 | `params` | `tan(30°)`, Aspect, `cos(0,5°)` (Sonnenwinkelradius), `30` (Scheibenintensität) |
| 9 | `moonDir` | xyz Richtung, w = beleuchteter Phasenanteil |
| 10 | `skyExtra` | x = Tagesfaktor, y = EVS-Gate, z = Wolkendeckung, w = Mondwinkelradius `0.0045 × FB_MOON_SCALE` |

**Atmosphären-Konstanten** (`FBAtmoCommon.h`, Hillaires Standardwerte): `groundRadiusMM = 6.360`,
`atmosphereRadiusMM = 6.460`, Rayleigh-Basis `(5.802, 13.558, 33.1)`, Mie-Streuung `3.996`,
Mie-Absorption `4.4`, Ozon `(0.650, 1.881, 0.085)`.

**Tagesfaktor** (`DaylightFactor`, verbatim aus dem Vorgänger-Code `atmo.h::w3_daylight`):
`t = clamp((sunElDeg + 9)/12, 0, 1)`, dann `t²(3−2t)` (Smoothstep). Voller Tag über ≈ +3°, dunkel ab
≈ −9° (nautische Dämmerung). EINE Zahl für Himmel, Boden und Sternen-Fade.

**SVS vs. EVS** (der TAB-Umschalter):

| Modus | Bodenquelle | Sonne | Zusatzeffekte |
|---|---|---|---|
| SVS (`GroundPhoto = 0`) | OSM-Render | fest 45° Elevation, Azimut 180° | Tagesfaktor auf 1 gepinnt, keine Sterne/Lichter/Wolken |
| EVS (`GroundPhoto = 1`) | Luftbild | echte Ephemeride (`FBEphemeris.h`, über `SetHud`) | Sterne, Nachtlichter, Wolken, Mondlicht |

Begründung im Code: SVS ist eine **zeitunabhängige Datenbanksicht**; nur EVS ist „die echte Kamera",
also muss nur dort Morgen-/Abenddämmerung zur Bildhelligkeit passen.

**Init-Order-Vertrag** (`FBRenderer::CreateAtmosphere`, ausdrücklich als CONTRACT dokumentiert):
WebGPU-Bind-Groups pinnen beim Erzeugen eine konkrete `TextureView` — es gibt kein „später
umbinden". Also müssen die Stages in Abhängigkeitsreihenfolge konfiguriert werden:

```
Transmittance (besitzt TransLUT)
   └─ SkyView   (liest TransLUT, schreibt SkyLUT)
        └─ Sky  (liest SkyLUT + AtmoBuf)
   ├─ Sun       (liest TransLUT für die Sonnenfarbe + AtmoBuf)
   └─ Moon      (baut die Albedotextur aus den von SetMoonTexture gestagten Bytes + AtmoBuf)
…danach erst CreateTerrainPipeline() → FBTilesStage::Configure(… TransLUT, SkyLUT …)
```

Ephemeriden (`render/FBEphemeris.h`, reine Funktionen): `SunPos` ist ein Verbatim-Port der
NOAA-Näherungsformeln (< ~0,5° Fehler); `MoonPos`/`MoonPhase` sind ein Port von Paul Schlyters
Näherung (public domain) **ohne** dessen lange Störungsterm-Tabelle — gut auf etwa ein Grad, genug
für eine Scheibe plus Phase, nicht für Navigation. Die Phase ist `(1 − cos(Elongation))/2`.

---

## 5 Die Wolkenkette

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

---

## 6 Die Terrain-Stage im Detail

`FBTilesStage` ist die einzige Stage mit echtem Pro-Frame-CPU-Zustand.

**Vertexlayout** (`render/FBChunkVtx.h`, `w3_vtx`): `pos[3]`, `uv[2]`, `norm[3]` = 32 B, dicht
gepackt, mit `_Static_assert` auf Größe und Offsets 0/12/20. Der Grund steht im Header: Schreiber
(Tile-Worker) und Leser (Draw-Call) stimmten früher nur **von Hand** überein — als die Normalen
dazukamen, wanderte der Stride von 20 auf 32 und jede handgeschriebene Zahl musste mitwandern; ein
Fehler dort rendert Müll statt zu brechen.

**Geometrie**: `pos` ist der ECEF-**Versatz zum Tile-Origin** (float; bei z14 < 2 km → Sub-Zentimeter),
`origin` ist der Doppel-Anker, den das Frame abzieht. Der Bau steht in `render/FBChunkMesh.h`
(`w3_chunk_build_ecef`): dieselbe reguläre Gitterstruktur wie der ENU-Pfad, aber jeder Knoten wird
durch die exakte Mercator-Inverse und Geodätisch→ECEF projiziert — kein Tangentialebenenfehler, keine
Abhängigkeit von einem Heimatursprung. Normalen sind echte ECEF-Kreuzprodukte der Nachbarversätze,
tragen die Krümmung der Kachel also gratis. `err` = maximaler Höhenfehler der Dezimierung in Metern —
projektionsunabhängig, und genau die Zahl, auf der das LOD des Streamers aufsetzt.

**Per-Draw-Daten** (Storage-Buffer `TileBuf`, ein `Tile{a: vec4f, b: vec4f}` je Draw, 32 B):

| Feld | Bedeutung |
|---|---|
| `a.xyz` | `origin_ecef − cam_ecef` (float, camera-relative) |
| `a.w` | Layer im Albedo-Array |
| `b.x` | Pro-Kachel-Helligkeits-Gain des Luftbilds (1,0 für OSM) |

Der Draw wählt seinen Eintrag über `firstInstance`, also gilt `instance_index == Draw-Index`.

**Albedo**: `texture_2d_array`, Kantenlänge 512 (Client-Wahl), **wachsend** bis zum echten
`maxTextureArrayLayers` des Adapters (Zielgerät 2048; Default-Cap 256). Hochgeladen wird immer eine
**komplette sRGB-Mip-Pyramide** (`render/FBMips.h`: Farbmittelung in LINEAREM Licht — dekodieren,
mitteln, re-enkodieren, damit Entfernung nicht abdunkelt; Alpha ist schon linear). Zwei Layer je
Kachel sind möglich: der eager gebackene **Base**-Layer und der lazy nachgeladene **Photo/Overlay**-Layer.

**Sampler** (`FBRenderer::CreateTileTexture`, geteilt mit dem Tonemap): ClampToEdge (ein Bake IST eine
Kachel — nichts wickelt), linear/linear, **trilinear** über die Mip-Kette, `maxAnisotropy = 16`.

**Der Grazing-Mip-Bias** (Terrain-Fragment-Shader): bei streifendem Blick übersteigt die
UV-Fußabdruck-Anisotropie den 16:1-Hardware-Deckel → vertikale Streifen. Korrektur:
`gbias = clamp(1.0 · (−log2(grazeV) − 2.5), 0, 1.2)` mit `grazeV` = Abwärtskomponente des
Sichtstrahls. Einsatz erst unter ≈ 10° Depression, Deckel 1,2 (≈ 2,3-facher Fußabdruck). Der Kommentar
nennt das ausdrücklich eine **Nutzerentscheidung** (2026-07-23): Schärfe schlägt Streifenfreiheit; ein
Reststreifen im äußersten Horizontband ist akzeptiert.

**RenderBundle**: Signatur = FNV-1a über die Draw-**Struktur** (Anzahl, Bind-Group-Handle nach einem
Array-Wachstum, je Kachel Vertexbuffer-Handle + Vertexzahl). `TileBuf`- und Uniform-**Inhalte** ändern
sich jeden Frame, aber das Bundle referenziert diese Buffer nur per Handle — deshalb löst nur eine
Strukturänderung eine Neuaufzeichnung aus (wenige pro Sekunde im Anflug, **null** im geparkten
Zustand). Zähler: `GetBundleRecords()`.

**2-Phasen-Commit**: eine Kachel darf erst **einen Pass NACH** ihrem GPU-Upload gezeichnet werden
(`FrameNo > PhotoUpTick + 1` für den Overlay-Layer; die Streamer-Seite spiegelt das, s.
`world-and-terrain.md`). Sonst referenziert ein Draw einen Layer, dessen `WriteTexture` noch nicht
sichtbar ist → ein schwarzer Frame.

**Invarianten-Zähler** (einmal pro Sekunde als `FBLog::Debug("render","present")` mit
`violation`-Flag):

| Zähler | Bedeutung — sollte 0 bleiben |
|---|---|
| `notReadyDraws` | ein Draw ohne committeten Layer |
| `wrongModeDraws` | SVS zeigte EVS oder umgekehrt (Modus-Bluten) |
| `blackDraws` | Layer-Index < 0 |

---

## 7 Das HUD-Backend

Drei Schichten, sauber getrennt: **Geometrie** (CPU, airframe-agnostisch) → **Backend** (WebGPU) →
**Font** (generisches Bitmap-System). Die **Symbologie** selbst liegt außerhalb, in
`systems/FBDisplaySystem::BuildHud` bzw. dessen F-16-Override.

### 7.1 `FBHudGeometry` (`render/FBHudGeometry.h/.cpp`)

Der wiederverwendete Pro-Frame-Geometriepuffer in **2D-Pixelkoordinaten**; er ersetzt die alten
GL-Shim-Globals (`w3_hud`/`w3_hudT`/`mx_v`) durch einen besessenen, rücksetzbaren Puffer. `Reset()`
leert, ohne Kapazität freizugeben — nach dem ersten Frame allokiert die (beschränkte, deterministische)
Symbologie nichts mehr.

Zwei Primitivarten, passend zu den zwei Pipelines:

| Art | Layout | Erzeuger |
|---|---|---|
| **Strokes** | `(x, y, d, hw, r, g, b)` × 6 je Segment | `Line()` (Haarlinie, hw = 0,5 px), `QLine()` (explizite Halbbreite), `Circle()` (n Sehnen), `Box()` |
| **Glyphs** | `(x, y, u, v, r, g, b)` × 6 je Zeichen | `Text()`, `Printf()` |

`d` ist der vorzeichenbehaftete Normalabstand des Vertex von der Mittellinie in Pixeln, `hw` die
nominale Halbbreite des Segments — daraus rechnet der Fragment-Shader analytische Deckung. Es gibt
**keine** harte `LineList`-Pipeline mehr; jede gerade Strecke (Schienen, Ticks, Carets, Boxen, der
konforme Horizont) geht durch denselben AA-Quad-Pfad. Kappen sind schlichte Butt-Kappen.

Obergrenzen, auf die `FBHudStage` seine GPU-Buffer dimensioniert: `kHudMaxStrokeFloats = 147456`,
`kHudMaxTextFloats = 32768`.

**Clipping** (`SetClip`/`ClearClip`): für die konforme Symbologie in der HUD-Combiner-Apertur.
Strokes werden per Liang-Barsky-Segmentclip **geschnitten** (ein teilweise sichtbarer Stroke emittiert
nur seinen sichtbaren Teil); Glyphen werden **ganz verworfen**, wenn ihre Content-Box das Rechteck
nicht schneidet — ein Glyph ist ein opaker Quad und ohne Shader-Änderung nicht weiter unterteilbar.
Geometrie außerhalb eines `SetClip`/`ClearClip`-Paares (Tapes, Textblöcke) bleibt unberührt.

### 7.2 `FBHudStage` (`render/stages/FBHudStage.h/.cpp`)

Reines WebGPU-Backend: Pipelines, Buffer, Atlas. Einmal je `Encode()` bittet es das **geliehene**
`FBDisplaySystem`, eine `FBHudGeometry` zu füllen; die Logik lebt dort, nicht hier.
`SetDisplaySystem(nullptr)` = leeres HUD.

| Detail | Wert |
|---|---|
| Pipelines | `Stroke` und `Text`, gemeinsame Pixel→NDC-Abbildung `scale = (2/W, 2/H)` |
| Blend | `SrcAlpha / OneMinusSrcAlpha` (Farbe), `One / OneMinusSrcAlpha` (Alpha) |
| Farbe | Der Fragment-Shader **linearisiert** (`pow(col, 2.2)`), damit die sRGB-Swapchain-View beim Schreiben wieder auf das gemeinte Displaygrün encodiert |
| Pass | LoadOp `Load` auf `FrameTex` — das getonemappte Bild bleibt stehen |
| Zweitnutzung | `EncodeLoadingText()` — Ladebildschirm, nur die Text-Pipeline |

Stroke-Deckung (analytischer Boxfilter über die Breite):
`alpha = clamp(hw + 0.5 − |d|, 0, 1)`, `alpha <= 0 → discard`. Eine pixelbündige 1-px-Linie
(`hw = 0,5`) rendert damit **exakt** wie die frühere harte `LineList`; jeder andere Winkel und jede
andere Breite bekommen eine weiche Kante statt einer Treppe.

### 7.3 `FBHudFont` (`render/FBHudFont.h` + generiertes `FBHudFontRom.h`)

Das generische, **airframe-agnostische** Font-System — jedes Modul-HUD zeichnet Text hierüber.

| Größe | Wert | Bedeutung |
|---|---|---|
| `kFontTile` | 16 | Kantenlänge der Glyphen-Bitmap (gewachsen von 8) |
| Bittiefe | 8 bit **Flächen-Coverage** (0..255) | echte boxgefilterte Deckung, keine 1-bpp-Maske |
| `kFontGlyphs` | 43 | Charset `" 0123456789A–Z-.:/+°"` |
| `kFontTilePad` | 18 = `kFontTile + 2` | 1 Texel transparenter **Gutter** rundum |
| Atlas | `43·18 × 18`, Format `R8Unorm`, Sampler **LINEAR** | Gutter-Texel bleiben auf ihrem Null-Init |
| `kFontAdvance` | 4,0 | öffentliche Screen-Pixel-Einheit: Zeichenvorschub |
| `kFontQuadSize` | 6,0 | öffentliche Screen-Pixel-Einheit: gezeichnete Kachel |

**Herkunft der Daten:** B612 Mono Bold (SIL OFL 1.1 — Airbus' eigene Cockpit-Schrift), gebacken von
`sim/tools/bake_hud_font.py` (Pillow: 8× supersampled, Box-Filter herunter auf 16×16) in das
GENERIERTE `FBHudFontRom.h`. Das Skript ist **keine Build-Abhängigkeit** — es läuft nur bei Font- oder
Charset-Wechsel. `FBHudFont.h` selbst bleibt handgepflegt (Atlas-Layout + Quad-Builder).

**Warum Advance und Quad-Größe unverändert blieben:** sie sind die öffentlichen Einheiten JEDES
Aufrufers. Nur die interne Rasterauflösung wuchs 8×8 → 16×16; Textgröße, Grundlinie und Vorschub auf
dem Schirm bleiben, wo sie waren. `kFontAdvance = 4` bei `kFontQuadSize = 6` heißt: der gezeichnete
Quad ist größer als der Vorschub, die Tinte aber schmaler (das Bake-Skript respektiert das über
`SAFE_X_FRACTION`) — Content < Advance ergibt eine klare Lücke.

**Gutter + Quad-Überstand:** Der Quad wird auf jeder Seite um **ein Texel in Screen-Pixeln**
(`texel = qs / kFontTile`) über die Content-Box hinaus aufgezogen, und die UVs decken die volle
gepolsterte Kachel ab. Damit sieht die Deckungsrekonstruktion außerhalb des Bitmaps echte Nullen statt
eines geklemmten Randtexel-Echos: randberührende Tinte erreicht volle Deckung, außen fällt es sauber
auf 0 — und nichts blutet in die Nachbarkachel.

**„Sharp bilinear" — echtes Antialiasing statt Alpha-Test.** Der Text-Fragment-Shader:

```wgsl
let t  = uv * texSize;              // Sampleort in Texel-Koordinaten
let fw = max(fwidth(t), 1e-4);      // Screen-Fußabdruck EINES Texels
let c  = floor(t - 0.5) + 0.5;      // Zentrum des getroffenen Texels
let f  = clamp((t - c - 0.5) / fw + 0.5, 0.0, 1.0);
let coverage = textureSampleLevel(atlas, samp, (c + f) / texSize, 0.0).r;
```

Der Sampleort wird also im Texelraum **verzerrt** (Bruchteil auf den `fwidth()`-Fußabdruck skaliert,
dann geklemmt), bevor LINEAR gesampelt wird. Eigenschaften:

- Bei Fußabdruck = 1 Texel ist das die **Identität** (schlichtes Bilinear).
- Vergrößert rastet es flach ein, außer in einer ≈ 1 Screen-Pixel breiten Rampe **direkt an jeder
  Bit-Kante** — eine Boxfilter-Näherung der idealen analytischen Kante, nicht ein Weichzeichner.
- Ergebnis ist **gerade Alpha**, kein harter `alpha < x → discard`. Ein Alpha-Test kennt nur an/aus
  und produziert genau die Treppe, die er verstecken soll; hier ist die Deckung eine echte Fläche.

Der Mechanismus ist **coverage-agnostisch** und unverändert seit der 8×8-1-bpp-Ära — die
ROM-Auflösung wuchs darunter weg, ohne dass der Shader angefasst werden musste.

**Die Grenze zu chip-spezifischen Eigenheiten.** MAX7456-Artefakte (Interlace-Jitter, Helligkeitskurve,
Sync-Artefakte, …) gehören **nicht** hierher. Ein Modul, das sie will, hängt sich aus SEINER eigenen
Klasse ein: `modules/f16/FBF16Max7456` — heute ein echter, von `FBF16Module` gehaltener NoOp-
Override-Punkt. `render/FBHudFont.h` bleibt generisch; sonst trüge jedes künftige Muster den Chip der
F-16 mit sich.

---

## 8 Kamera und Bodenwahrheit

### 8.1 `FBCamera.h` — Lage rein, Basis raus

Die Datei ist bewusst **reine Mathematik**: kein GL, keine Globals, keine Nebeneffekte. Sie berechnet
die Augen-POSITION absichtlich **nicht** — die hängt am Gelände und gehört der Tile-Seite.

Render-Raum ist ENU mit `E = +X`, `up = +Y`, `N = −Z`. Die Basis kommt aus Gier/Nick, dann wird um die
Vorwärtsachse gerollt:

```
up = u·cos(roll) + s·sin(roll)
sr = s·cos(roll) − u·sin(roll)
```

Das `+s` ist die Stelle, die **still spiegelt**: sie stand einmal auf `−s`, und eine Rechtsschräglage
sah aus wie eine Linksschräglage. Nichts stürzte ab, nichts sah kaputt aus — ein geneigter Horizont
ist in beide Richtungen ein geneigter Horizont. Genau deswegen ist die Logik EINMAL vorhanden
(`w3_basis_from`) und wird von beiden Pfaden benutzt statt kopiert.

**`FBCameraBasisEcef`** ist die EINE Lage→ECEF-Basis, die native und WASM teilen. Sie stand vorher
zeichengleich zweimal — in `FBAppNative.cpp` und `FBAppWasm.cpp`, jede mit einem Kommentar, der bat,
sie mit der anderen identisch zu halten. Damit waren „die Frames des Orakels" und „die Frames des
Browsers" nur **von Hand** dieselbe Kamera. Sie rechnet in **Doubles** (das Auge liegt im 6,4e6-m-
Bereich) und rotiert die Render-Basis über `FBEnuAxesEcef` (`core/FBGeodesy.h`, geschlossene Form,
rechtshändig, det +1 → Dreieckswicklung bleibt erhalten) ins ECEF; die Render→ENU-Abbildung ist
`(e, n, u) = (x, −z, y)`.

Der Renderer kennt drei Kameraquellen, in dieser Vorrangfolge:

| Quelle | Setter | Eigenschaft |
|---|---|---|
| Volle Basis | `SetCameraBasis(eye, fwd, right, up)` | trägt **Roll** — der Horizont kippt bei Schräglage. Gewinnt über beide anderen. |
| Skript-Kamera | `SetCamera(eye, target)` | Up ist radial abgeleitet, **kein** Roll |
| Default-Orbit | — | kreist über dem Terrainfeld-Zentrum (1500 m hoch, 6500 m Radius, 0,2 rad/s) — nur Bring-up/Standbild |

**Horizont-Dip** (`w3_horizon_dip_rad`): `acos(R/(R+h))` mit `R = 6 371 000 m` (Kugel-Mittelradius —
das Terrain steht auf dem WGS84-Ellipsoid, aber der Dip ist ein Kleinwinkel-Kugelresultat, für das der
Mittelradius der Standardwert ist). Ein waagerecht gezeichneter HUD-Horizont liest in der Höhe zu
HOCH; er muss um genau diesen Winkel fallen, damit er den echten, weggekrümmten Horizont überlagert
(MIL-STD-1787, konform). Konsumiert wird er von der Symbologie (`systems/FBDisplaySystem`,
`modules/f16/displays/FBF16Hud`), nicht vom Terrain-Pass — dort ergibt sich die Krümmung von selbst,
weil die ECEF-Kacheln wirklich wegkippen.

### 8.2 Augenhöhe aus der Modell-Geometrie

Die Augenhöhe am Boden kommt **nicht** aus einer festen Zahl, sondern aus JSBSims Fahrwerksgeometrie:
`FBFdm::GetGroundClearanceM(gearDown)` (`sim/src/fdm/FBFdm.cpp`) läuft über alle
Ground-Reaction-Einheiten und nimmt das größte `GetBodyLocation(3)` (Körper-z, nach unten positiv, ft
unter dem CG) — mit `gearDown = false` zählen nur nicht-einziehbare Kontakte (der Bauch).

Genutzt wird das an der Spawn-Naht: Ist `HeightOffsetM < 0` („auf dem Fahrwerk sitzen"), setzt der
Boot-Pfad nach dem ersten `RunIC()` — wenn der CG gültig ist — die Höhe auf
`GroundElevM + GetGroundClearanceM(true)` und läuft die IC erneut. Die Spawnhöhe ist damit
geometriewahr statt geraten; es gibt keinen „gehalten→scharf"-Sprung.

Die Kamera selbst ist im heutigen Client **das Auge des Flugzeugs**: `FBGeoToEcef(pose)` →
`SetCameraBasis`. Ein zusätzlicher Clamp „nie unter die Oberfläche" ist im Client-Code **nicht mehr
verdrahtet** — s. *Offene Punkte*.

### 8.3 „Crash → Motor aus, kein Freeze"

Der physikalische Richter (`core/FBFlightMonitor`) gehört dem Client, nicht dem Modul. Löst er aus,
schneidet `units/FBSimUnit::RunMonitors` das Triebwerk **über denselben Controls-Pfad, den ein Pilot
benutzen würde** (`Module_->Controls().EngineCutoff()`) — und sonst nichts. Kein Freeze, kein
Sonderfall im Renderer: JSBSims eigene Ground-Reactions rechnen weiter, das Wrack rutscht, der
Renderer zeichnet es. Dieselbe Kopplung gilt für Schaden (`ApplyDamageToAirframe`: Cutoff,
Throttle-Deckel, Ruderautorität, Zusatzwiderstand). Ein FAIL-Urteil der Mission schneidet den Motor
NUR, wenn die Einheit noch kampffähig war — bei einem Abschuss wäre das das Urteil, das aufs Flugzeug
wirkt, statt des Schadens.

---

## 9 Die Regel `grep -c 'R"(' FBRenderer.cpp` == 0

**Heute erfüllt: 0.** Kein einziges rohes WGSL-String-Literal steht mehr in `FBRenderer.cpp`.

Was die Regel aussagt: Der Render-Stage-Split ist **abgeschlossen**, und zwar überprüfbar mit einem
Einzeiler statt mit einem Urteil. Jedes WGSL lebt in genau einer `stages/`-Datei; `FBRenderer` ist
damit **nur noch** Orchestrator — Gerät, Ziele, Pass-Grenzen, Reihenfolge, geteilte Ressourcen. Eine
neue Shader-Idee kann nicht mehr „schnell mal" in den Orchestrator wandern, weil die Regel es sofort
sichtbar machen würde.

Verteilung der 20 WGSL-Blöcke über 17 Stage-Dateien (drei Dateien tragen zwei: `FBTonemapStage` mit/ohne
Wolken-Composite, `FBHudStage` Stroke/Text, `FBCloudBaseBakeStage` Bake + Histogramm) plus drei reine
Splice-Header ohne eigene Klasse.

---

## Offene Punkte

1. **`FBUnitsStage` und `FBSpritesStage` sind NoOp.** Beide Slots sind in der Encode-Ordnung fest
   verdrahtet (Units nach dem Terrain, Sprites vor dem HUD), aber sie zeichnen nichts. **Konsequenz:
   andere Flugzeuge, abgeworfene Waffen, Flugkörper, Bodenziele, Rauch und Fackeln sind unsichtbar.**
   Die gesamte Multi-Unit-Simulation (Etappen 1–6, Datalink, Radar, BFM, Intercept, Schadensmodell)
   existiert physikalisch und in der Telemetrie, aber es gibt kein Bild davon. Ein Frame-Beweis kann
   heute nichts über Einheiten aussagen.
2. **Reversed-Z-Zahlen widersprechen sich.** CLAUDE.md sagt „near 0,01 m / far 240 km". Der Code
   (`MvpCamRel`) nutzt eine **unendliche** Far-Plane und `zn = 0.05`. Die 240 km sind der
   Streaming-Sichtradius (`FB_VIEW_KM`), nicht die Far-Plane; die 0,01 m stehen nirgends im Code.
   Nicht aufgelöst — vermutlich veraltete Doku, aber das ist eine Vermutung.
3. **Der Kamera-Clamp „nie unter die Oberfläche" hat keinen Konsumenten mehr.** `fb-sim` liefert
   `window.FB_GROUND_CLEAR` an den Browser (`app/FBSimHost.cpp`, gelesen aus `/tmp/fb_clearance`), und
   `web/config.js` setzt es auf 0 — aber **kein** C++-Code liest es (`grep FB_GROUND_CLEAR src/` findet
   nur den Erzeuger). Die geometriewahre Höhe wirkt heute nur über den Spawn-IC
   (`GetGroundClearanceM`), nicht als Pro-Frame-Clamp. CLAUDE.mds Formulierung „die Kamera geht nie
   unter die Oberfläche" beschreibt damit einen Zustand, den der Code nicht mehr erzwingt.
4. **Toter Code in `FBCamera.h`.** `w3_frustum_from` und `w3_aabb_visible` (Gribb-Hartmann-Culling)
   haben im ganzen Baum **keinen Aufrufer** — das Culling passiert im Streamer (`FBWorld`) über
   Sichtradius und Frustum-Gewichtung, nicht über diese Ebenen. Genutzt sind aus dieser Datei nur
   `w3_cam_from`, `w3_basis_from`, `w3_horizon_dip_rad` (alle drei von der HUD-Symbologie) und
   `FBCameraBasisEcef`. Ob die Frustum-Helfer für eine künftige `FBUnitsStage` reserviert sind oder
   ersatzlos entfallen sollten, ist offen.
5. **Aerial Perspective ist per Default AUS** (`FB_AP=0`), obwohl die gesamte
   Transmittance-LUT-Infrastruktur dafür existiert und jeden Frame gerechnet wird. Das Gelände ist
   „lit albedo pur, volle Helligkeit bis zum Horizont" — eine ausdrückliche Nutzerentscheidung
   (2026-07-23), aber physikalisch falsch. Die LUT wird trotzdem berechnet, weil Himmel und Sonne sie
   brauchen.
6. **Wolken sind per Default AUS** (`FB_CLOUDS=0`, Nutzerentscheidung 2026-07-23) und es ist **keine
   Wetterquelle verdrahtet**: `FBState.Env.Cloud*` bleibt null, und `FBCloudMarchStage` liest „0" als
   „kein Wetterbericht" und setzt seine eigene Defaultdecke. Der Kommentar bei `FBRenderer::HudState`
   nennt das ausdrücklich: null IST hier der ehrliche Wert, solange kein open-meteo-Anschluss existiert.
7. **Transmittance-LUT wird jeden Frame neu gerechnet.** Sie hängt nur an Höhe und Sonnen-cos-θ; ein
   `TODO cache while the sun is static` steht im Header und ist offen.
8. **Upscale ist bilinear.** `TODO bicubic/sharpen` im Header; ebenso fehlt der 8-Tap-Glow des HUD
   (`TODO` in `FBHudStage.cpp`), der die Leuchtdichte-Anmutung eines echten Combiners ausmachen würde.
9. **`FBTerrainLoader`s statischer Pfad (`SetTerrain`/`SetAlbedoArray`) ist nur noch Bring-up.** Beide
   Clients fahren Streaming; der statische Pfad in `FBTilesStage` (Direktdraws, Layer == Kachelindex)
   bleibt als zweiter Codepfad bestehen und wird nirgends regelmäßig getestet.
10. **Der Ladebildschirm hat einen Timeout-Ausgang** (30 s, `FB_LOAD_TIMEOUT_MS`), nach dem er das
    Bild freigibt, egal wie wenig resident ist — der Log unterscheidet `converged` von `TIMEOUT`. Ein
    Frame-Beweis, der aus einem Timeout-Boot stammt, ist damit nicht dasselbe Bild wie einer aus einem
    konvergierten. Das ist heute nur an der Logzeile erkennbar, nicht am PNG.
