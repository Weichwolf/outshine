# HUD — das Backend

> Body still in German — translation pass pending (see [roadmap](../roadmap.md)).

**Herkunft:** ausgelagert aus `rendering.md` §7 (Stand `793e1fe`), unverändert übernommen.
Die **Symbologie** (was gezeichnet wird) liegt nicht hier, sondern in `systems/FBDisplaySystem`
bzw. dessen F-16-Override — siehe [`../sim/systems.md`](../sim/systems.md) und
[`../aircraft/f16.md`](../aircraft/f16.md). Diese Datei beschreibt das **Backend** (womit gezeichnet
wird).

## Spec

The HUD **backend**: how the picture is drawn, never what is drawn.

| Contract | Acceptance / measurement anchor |
|---|---|
| Airframe-agnostic | the geometry buffer, the font and the stage know no aircraft type; symbology lives in `systems/FBDisplaySystem` and its module override |
| Geometry is CPU-side and WebGPU-free | `render/FBHudGeometry.cpp` is the documented core-lib exception |
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

The symbology implementation is documented in [`../aircraft/f16.md`](../aircraft/f16.md).

## Gaps

### Open work (from the retired `TODO.md` §4)

| # | Thing |
|---|---|
| 4.5 | the 8-tap HUD glow is missing (`TODO` in `FBHudStage.cpp`) — it is what gives a real combiner its luminance feel |
| 5.4 | no lock / TD-box symbology, because `doc/f16/hud-symbology.md` documents none. It will not be invented (see `../aircraft/f16.md`). |

### Inventory (German, from the previous `Offene Punkte` section)

(siehe [`renderer.md`](renderer.md) — die Sammelliste der Renderer-Runde ist dort
vollständig erhalten; die hierher gehörenden Punkte stehen oben unter Gaps.)


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### Die drei Schichten

Drei Schichten, sauber getrennt: **Geometrie** (CPU, airframe-agnostisch) → **Backend** (WebGPU) →
**Font** (generisches Bitmap-System). Die **Symbologie** selbst liegt außerhalb, in
`systems/FBDisplaySystem::BuildHud` bzw. dessen F-16-Override.

#### 7.1 `FBHudGeometry` (`render/FBHudGeometry.h/.cpp`)

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

#### 7.2 `FBHudStage` (`render/stages/FBHudStage.h/.cpp`)

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

#### 7.3 `FBHudFont` (`render/FBHudFont.h` + generiertes `FBHudFontRom.h`)

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
