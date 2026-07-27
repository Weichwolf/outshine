# Welt, Gelände und Tile-Streaming

**Quellen dieser Datei:** `sim/src/world/` (`FBWorld.h/.cpp`, `FBTerrainLoader.h/.cpp`,
`FBTilesElevation.h`), `sim/src/terrain/` (`geo.h/.cpp`, `geo_ecef.cpp`, `mesh.h`, `terrain.h/.cpp`,
`osmmesh.h`, `osmmesh_terrain.cpp`), `sim/src/render/FBChunkMesh.h` + `FBChunkVtx.h` (die
Mesh-Endstufe), `sim/src/app/FBTileWorkerMain.cpp` + `sim/web/fbtw-worker.js`, `sim/Makefile`
(Targets `wasm`/`worker`) sowie `tiles/` (Server: `Makefile`, `nginx.conf`, `src/*`) — der Server ist
hier **aus Klientensicht** dokumentiert; die einzige Ausnahme ist §9 (`/wx`), das die
**Server-seitige** Wetter-Datenart beschreibt (`tiles/src/wx.c`, `grib2.c`, `wxfmt.h`). Dazu CLAUDE.mds Abschnitte
`world/`, `terrain/` und „Rendering".

Nachbardateien: `rendering.md` (was mit der Geometrie passiert, sobald sie auf der GPU liegt),
`architecture.md` (Lib/Client-Split).

---

## 1 Der Datenweg, einmal durchgezogen

```
fb-tiles :8081  ──HTTP──▶  Byte-Cache (JS-Map im Browser | in-memory im native Client)
      /t/terrain/z/x/y            │
      /bake/{osm|photo}/z/x/y     ▼
      /t/lights/z/x/y     osmmesh_fetch_tile ──▶ Terrarium-PNG dekodieren, 4 Nachbarn stitchen,
      /t/stars/band/0/0                          reguläres ENU-Gitter bauen
      /elev?lat&lon                │
                                   ▼
                    w3_chunk_build_ecef  ──▶ w3_vtx[] (ECEF-Versatz zum Tile-Origin,
                    (render/FBChunkMesh.h)     UV, ECEF-Normale) + err (m) + origin (double)
                                   │
                                   ▼        + fb_build_pyramid (sRGB-Mip-Kette der Albedo)
                            FBWorld (Quadtree, Budgets, 2-Phasen-Commit, Eviction)
                                   │  UploadTile / SetDrawList
                                   ▼
                            FBRenderer → FBTilesStage (rendering.md §6)
```

Im Browser läuft der eingerahmte Block (Fetch, Decode, Mesh, Mips) in **eigenen Web Workern**; nativ
läuft er inline mit blockierendem libcurl. Die Poll-Schnittstelle darüber (`fb_stream_*`) ist für
beide identisch.

---

## 2 `FBWorld` — der Streamer

`sim/src/world/FBWorld.h/.cpp`. Ein chunked-LOD-**Quadtree** (Cesium-Art), der pro Frame eine
Zeichenliste des LOD-Schnitts an `FBRenderer` gibt. Portiert aus dem Vorgänger-Engine-Code
(`tiles/walk.h` Prioritäts-Refinement, `lru.h` Grace-Eviction/Budget), mit **korrigierter**
Coverage-Semantik (s. §2.3).

### 2.1 Was es besitzt und was es nur borgt

| | Inhalt |
|---|---|
| **Besitzt** | die Knotentabelle `std::vector<Node>` + den `unordered_map`-Index, die Zeichenliste `DrawSlots`, die Arbeitsliste `WorkList`, den Pyramiden-Scratch, den Nachtlicht-Puffer, alle Zähler |
| **Borgt** | `FBRenderer*` (Upload-Ziel) und **`const FBUnitRegistry*`** (`SetUnits`/`Units()`) |

**Warum die Registry nur geborgt ist** — das ist keine Kosmetik, sondern die Konsequenz des
Lib/Client-Splits: die Besetzung der Welt („wer existiert wo") ist **Simulationszustand**, kein
Rendering-Ding. Sie lag früher als Member in `FBWorld`, also auf der **Renderer-Seite** des Splits.
`fb-gym` linkt kein `world/` — und reichte deshalb jedem Modul `world = nullptr`, sodass ein
simulierter Sensor ausgerechnet in dem Client, der die Missionsschleife wirklich fährt, nie eine
andere Einheit hätte sehen können. Heute lebt `units/FBUnitRegistry` in der Core-Lib, der Client
besitzt genau eine, `FBWorld` hält nur einen Zeiger darauf — für die **Zeichenseite** (`FBUnitsStage`,
heute NoOp). Kein Ownership, kein Welt-Mutationspfad.

Getrennt daneben steht der `const FBWorld*`, den ein Sensor bekommt: das ist die **Terrain-Seite**
(Maskierung), nicht die Einheiten-Seite.

### 2.2 Konstanten

| Konstante | Wert | Bedeutung / Herleitung |
|---|---|---|
| `kRootZ` | 8 | Wurzelring des Quadtrees |
| `kMaxZ` | 14 | feinste Stufe (die Vektor-Quelle endet dort, s. §7.2) |
| `kGrace` | 180 Passes | Hysterese vor der Eviction (lru.h) |
| `kSseK` | `720 / (2·tan(30°))` = `720/(2·0,57735)` ≈ **623,5** | Pixel-Brennweite, normiert auf 720 px Höhe bei 60° FOV |
| `kEdgeTau` | 384 px | Ziel-Kantenlänge einer Kachel auf dem Schirm; Blattkacheln landen zwischen `kEdgeTau/2` und `kEdgeTau` |
| `kCosView` | 0,5 | Frustum-Gewicht: < 60° neben der Achse = volle Priorität, sonst Faktor 0,05 |
| `kNodeCeil` | 6000 | Sicherheitsdeckel auf die Arbeitsmenge |
| `kEarthCirc` | 40 075 016,686 m | Äquatorumfang für `SpanM(z)` |
| Sichtradius | `FB_VIEW_KM · 1000`, Default **240 km** | Client-Parameter (`app/FBAppWasm.cpp`) |
| `Grid` | 32 | Dezimierung des Kachelgitters (32×32 Quads) |
| `TS` | 512 | Albedo-Kantenlänge in Texeln |

**LOD ist rein entfernungsbasiert** (Nutzerentscheidung 2026-07-23): gesplittet wird, wenn die
projizierte **Kantenlänge** `SpanM(z) · kSseK / dist` die Schwelle `kEdgeTau` übersteigt.
Höhenvarianz steht bewusst **nicht** in der Entscheidung — eine flache Kachel in der Nähe muss auf
dieselbe Stufe verfeinern wie eine zerklüftete in gleicher Entfernung (gleiche Albedo-Auflösung bei
gleicher Entfernung). Nebeneffekt: der alte Stillstand auf flachem Gelände (`err ≈ 0` verweigerte
jeden Split, `leaves` blieb bei 2–3) ist damit weg.

### 2.3 Das Refinement — und die korrigierte `walk.h`-Semantik

Vier Funktionen, alle nebeneffektfrei außer `Descend`:

| Funktion | Rolle |
|---|---|
| `Viable(z,x,y,eye)` | rein: Kartengrenzen **und** innerhalb des Sichtradius (`dist − span·0,71 ≤ ViewM`) |
| `WantSplit(z,x,y,eye)` | rein: Geometrie-Test oben; braucht **keine** Kacheldaten |
| `CanCover(z,x,y,eye)` | rein: kann dieser Teilbaum seine Fläche mit Kacheln decken, die DIESEN Pass fertig sind? |
| `Descend(...)` | die Zeichen-Traversierung; 1 = Fläche gedeckt |
| `RequestSubtree(...)` | kaskadiert die ANFRAGE bis zu den Zielblättern, ohne zu zeichnen |

**Der korrigierte Fehler** (Kommentar im Header, „sim-critic"): Sichtweite darf einen Split nur
**verhindern**, nie Coverage kosten. Ein Kind jenseits des Sichtradius macht den Vater zu einem
gezeichneten Blatt — Detail entfällt, Fläche nie. Die Viabilität aller vier Kinder wird
**nebeneffektfrei** geprüft, **bevor** der Vater ersetzt wird. `walk.h` behandelte „außerhalb der
Sicht" beim Split wie „außerhalb der Karte" und hat damit den Quadranten des Vaters gelöscht — ein
Loch.

**Ablauf in `Descend`** an einem Split-Knoten:

1. Alle viablen Kinder können decken → die **verfeinerte** Stufe zeichnen (Rekursion).
2. Sonst, wenn dieser Knoten selbst resident ist → **halten**: er zeichnet die ganze Fläche, während
   `RequestSubtree` die tieferen Ziele anfordert. Kein Zwischen-LOD wird je gebaut — nur die
   Geometrie-**Zielblätter**.
3. Sonst (Boot/Teleport, nichts resident) → zeichne, was an Nachkommen fertig ist; der Rest ist ein
   Loch, das ein residenter Vorfahre weiter oben oder der **Ladebildschirm** deckt.

Weil `WantSplit` nur Geometrie braucht, ist der **Zielschnitt sofort bekannt** — die Boot-/Teleport-
Anfrage geht direkt auf die Endblätter, ohne LOD-Leiter und ohne Bau-/Wegwerf-Churn.

### 2.4 Zwei Phasen und zwei Modi (die Bereitschafts-Prädikate)

| Prädikat | Bedingung | Zweck |
|---|---|---|
| `Uploaded(n)` | Mesh + Albedo da, Slot ≥ 0 | reine Buchhaltung |
| `Ready(n)` | `Uploaded && Pass > readyPass` | **2-Phasen-Commit**: zeichenbar erst EINEN Pass nach dem Upload, damit das `WriteTexture` submittiert und sichtbar ist |
| `ReadyMode(n)` | `Ready` und, wenn der ANGESEHENE Modus nicht der Basismodus ist, der Overlay-Layer aufgelöst (`alt == 1 && Pass > altPass + 1`, oder `alt == -1` = echtes Loch) | eine feine Kachel wird nie im FALSCHEN Modus gezeigt; das gröbere, richtige Elternteil hält |
| `CoversInMode(n)` | `ReadyMode` **oder** (`Ready` und im letzten Pass gezeichnet) | bei einem TAB-Umschalt bleibt die residente feine Kachel im ALTEN Modus stehen, bis ihr neuer Overlay landet — kein Re-Coarsen, kein Blitzen. Eine NEUE Kachel zählt erst mit `ReadyMode`, damit sie nie im falschen Modus aufpoppt |

Das `+1` in `ReadyMode` spiegelt exakt die Renderer-Seite (`FrameNo > PhotoUpTick + 1`,
`FBTilesStage`) — sonst gäbe es eine Ein-Frame-Lücke im falschen Modus.

### 2.5 Budgets pro Pass

Die Arbeitsliste wird **worst-first** sortiert (Priorität `weight / dist`, `weight` = 1 im Frustum,
0,05 außerhalb) und dann budgetiert abgearbeitet:

| Budget | Wert | Arbeit |
|---|---|---|
| `build` | 2 | `fb_stream_build` — Mesh pollen |
| `albedo` | 2 | `fb_stream_pyramid` — Basis-Mip-Pyramide pollen |
| `upload` | 6 | `FBRenderer::UploadTile` |
| `altBudget` | 2 | lazy Overlay des NICHT-Basismodus (nur für Kacheln, die diesen Pass angefasst wurden) |
| Lichter | 3 Dekodierungen | `/t/lights` je Pass, niedrigste Priorität |

Ein Photo-Basis-Loch (204 von `/bake/photo`) fällt auf OSM zurück, damit **jede** Kachel immer eine
Basis zeichnen kann. Ein Overlay-Loch markiert den Knoten dauerhaft mit `alt = -1` (nicht erneut
fragen).

### 2.6 Eviction, Fortschritt, Telemetrie

- **Eviction**: Knoten, die diesen Pass nicht angefasst wurden, altern (`stale++`); nach `kGrace`
  = 180 Passes werden sie freigegeben (`ReleaseTile`, `free(verts)`, Index-Fix per Swap-Pop).
- **`LoadProgress()`** = `TargetRdy / TargetTot` über den **Geometrie-Zielschnitt** (`CountTargets`,
  nebeneffektfrei). Der Client zeigt bis zur Schwelle den Ladebildschirm und hält JSBSim eingefroren
  (`rendering.md` §2.2).
- **Log** (1 Hz, `FBLog::Debug("world","fbworld")`): `leaves`, `drawn`, `pending`, `evicted`,
  `vramMB` (Mesh + Albedo), `nodes`, `lights`, plus die **Thrash-Sonde** `buildsPerMin` /
  `evictPerMin`. Beide gehen in einem konvergierten, stehenden Loiter gegen 0; ein stetiger Anstieg
  ist Evict-Rebuild-Churn.

### 2.7 Nachtlichter

Nur in EVS-Nacht (`SetNightLights`, vom Client an den Tagesfaktor gekoppelt: `sun_el < −3°`). Je
gezeichnetem Blatt wird `/t/lights/z/x/y` gestreamt und dekodiert: tile-lokale `(x,y)` ∈ [0,65535] →
Geo → ECEF auf der Kachel-Mittelhöhe + `kLightLiftM` = 6 m Anhebung (damit Terrain sauber verdeckt),
minus dem `Anchor` (Feldursprung, einmal in `Open` gesetzt). Klasse (0..7) → Farbe/Radius/Helligkeit
aus drei LUTs; `kLightGain` = 3,0 additiver HDR-Gain, damit die Kerne durch den ACES-Kompressor
kommen. Deckel `kLightBudget` = 65 536 Sprites, nächste zuerst.

---

## 3 `FBTerrainLoader` — die Streaming-C-ABI

`sim/src/world/FBTerrainLoader.h/.cpp`. Eine flache `extern "C"`-Schnittstelle; darüber pollt
`FBWorld` jeden Pass. **Nichts blockiert je die Render-Schleife im Browser.**

| Funktion | Vertrag |
|---|---|
| `fb_stream_open(base, lat, lon, z)` | einmal öffnen; setzt die Basis-URL und (WASM) startet den Worker-Pool |
| `fb_stream_set_base(mode)` | Boot-Basismodus (0 = OSM, 1 = Photo) — Prioritätshinweis an den Worker |
| `fb_stream_campos(lat, lon)` | laufende Kameraspur für die Nächste-zuerst-Pumpe des Workers |
| `fb_stream_build(z,x,y,grid, &verts,&nverts,origin,&err)` | 1 = fertig (malloc'te Verts, Aufrufer gibt frei), 0 = angefordert/pending |
| `fb_stream_pyramid(z,x,y,mode,ts,dst)` | > 0 = geschriebene Bytes der ganzen Pyramide, 0 = pending, **−1 = echtes Loch** (Server-204) |
| `fb_stream_ground(lat, lon)` | Bodenhöhe m ASL; WASM async (letzter aufgelöster Wert), nativ synchron; ≤ −1e8 = noch nie eine echte Probe |
| `fb_stream_dem(z,x,y,&bytes,&len)` | rohe Terrarium-Bytes; 1 = fertig, 0 = **pending** (nicht als Loch cachen!), −1 = Loch |
| `fb_stream_lights(z,x,y,dst,cap)` | ≥ 4 Bytes (Header auch bei count = 0), 0 = pending, −1 = nicht verfügbar |
| `fb_load_image_file(path, …)` | Bild-Datei → RGBA8 (WASM: eingebettetes MEMFS, z. B. `/moon.jpg`) |
| `fb_fetch_stars(base, dst, cap)` | HYG-Bänder verkettet, blockierender Startup-Fetch |
| `fb_fetch_text(url, dst, cap)` | blockierender Text-GET (z. B. die Boot-Mission von `fb-sim`s eigenem `web/`-Mount) |
| `fb_terrain_load(base, lat, lon, z, grid, out)` | **statischer Einmal-Pfad** (Bring-up): 4×4-Kachelfeld blockierend laden und zu EINEM Vertexarray mergen |

### 3.1 Byte-Zugriff: zwei Plattformen, ein Vertrag

| | WASM (Browser) | Native (CLI) |
|---|---|---|
| Mechanismus | JS-seitiger async Cache (EM_JS), **nicht blockierend** | blockierendes libcurl hinter einem kleinen In-Memory-Cache |
| Statuscodes | 200 terminal, 204 = Loch, alles andere = „nochmal fragen" | dieselbe Retry-Regel |
| Begründung | eine Seite darf ihren Frame-Loop nicht anhalten | `gpu_native` ist eine CLI mit eigenem Kontrollfluss; ein PNG-Dump-Frame darf blockieren |
| Retry | ASYNCIFY `emscripten_sleep(50)`, bis 60 Versuche | `usleep`, gleiche Zählung |

Der Rest (`fb_stream_*` selbst) ist **gemeinsam**; nur die drei Byte-Primitive `fbs_init`/`fbs_size`/
`fbs_copy` unterscheiden sich.

### 3.2 Der Worker-Pool (nur WASM)

`N = clamp(navigator.hardwareConcurrency − 2, 1, 6)` unabhängige `fbtileworker`-Instanzen. Jede ist
ein **eigenes WASM-Modul** mit eigenem `osmmesh`-Kontext und eigenem DEM-Cache — die ASYNCIFY-Regel
„ein Build gleichzeitig" gilt **pro Instanz**, also sind N parallele Builds sicher; die geringe
Cache-Redundanz zwischen Instanzen ist akzeptiert.

Geteilt bleibt eine Struktur auf der Renderthread-Seite (`Module.__fbw`):

| Feld | Rolle |
|---|---|
| `q` | Anfrage-Queue |
| `req` | Dedup-Set — eine Kachel ist höchstens einmal in der Queue oder in Arbeit, also wird nichts zweimal gebaut |
| `done` | Ergebnis-Map, aus der die Poll-Funktionen lesen |
| `pump()` | füllt JEDEN freien Worker mit dem besten Auftrag |

**Prioritätsschlüssel**: `prio · 1e18 + dist²` — Basiskacheln (`prio = 0`) grundsätzlich vor dem lazy
Overlay (`prio = 1`), innerhalb einer Klasse **nächste zuerst** (Kachelraum-Distanz zur
Kamerakachel, aus `fb_stream_campos`). Damit ist das Streaming **kamera-priorisiert**, und zwar an
der Stelle, an der es zählt (der Vergabe an Worker), zusätzlich zur Sortierung der `WorkList` in
`FBWorld`.

### 3.3 `/elev`: die strenge Antwortprüfung

`atof()` allein liefert für eine HTML-Fehlerseite, einen Proxy-Hinweis oder einen abgeschnittenen
Body **0.0** — und 0.0 besteht den Gültigkeitstest `> −1e8`, wird **gecacht** und vergiftet von da an
AGL, Radarhöhe und die Absturzprüfung mit „Meereshöhe", egal wo das Flugzeug ist. Deshalb parst der
Client streng: der ganze Body muss EINE endliche Zahl sein (führender/abschließender Whitespace ist
erlaubt, der Server beendet mit Newline). Alles andere ist keine Messung.

Nativ wird pro ≈ Kachelzelle (**≈ 33 m**) gecacht — ein fliegendes Flugzeug fragt erst nach
nennenswerter Strecke neu, eine stehende Kamera genau einmal. Der Startpfad fragt mit `?block=1`
(s. §7.3).

**Klemmung auf Meeresspiegel (beide Plattformen).** Eine ECHTE Probe wird auf ≥ 0 geklemmt: offene
Bathymetrie ist negativ (ETOPO-Seeboden), aber die Wasser*oberfläche* — und damit die Bodenreferenz
des Flugzeugs und der JSBSim-Boden — liegt bei 0, nicht auf dem Meeresgrund. Das
`−1e9`-„noch-keine-Probe"-Sentinel bleibt dabei unangetastet, sonst könnten Aufrufer nicht mehr
unterscheiden, ob eine Probe gelandet ist.

### 3.4 `[tileperf]` — die Kaltstart-Instrumentierung

`FB_TILEPERF=1` (nativ) bzw. der Worker-eigene `[tileperf-worker]`-Log messen die Stufen derselben
Pipeline: DEM-Fetch+Decode (`osmmesh_fetch_tile`), Mesh (`w3_chunk_build_ecef`), Albedo-Fetch,
Albedo-Decode (stbi), Mip-Pyramide (`fb_build_pyramid`). Zusammenfassung alle 32 Pyramiden und beim
Schließen; die letzte Zeile vor der Konvergenz ist die Kaltstart-Gesamtzeit. Kostet nichts, wenn aus
(ein gecachter Env-Test).

---

## 4 `FBTilesElevation` — der Elevation-Provider auf `fb_stream_ground`

`sim/src/world/FBTilesElevation.h`. Ein **dünner Pass-through**: `GroundElevM(lat, lon)` ruft
`fb_stream_ground(lat, lon)`, sonst nichts. Der Konstruktor macht nur `fb_stream_open(base, 0, 0, 8)`
— `fb_stream_ground` liest die vom Open gesetzte Basis-URL und ist von der dort übergebenen
(lat, lon) unabhängig; die dient nur als Saat für den Render-Quadtree und ist für eine
Punktabfrage an `/elev` bedeutungslos.

**Er liegt in `world/`, nicht in `core/` — und ist deshalb NICHT Teil der Core-Lib.** Grund: er hängt
an der Tile-Streaming-C-ABI, die zu `render`/`world` gehört und die die Core-Bibliothek absichtlich
ausschließt. Die Core-Seite sieht nur das Interface `core/FBElevationProvider.h`; welche
Implementierung dahintersteht, entscheidet der Client:

| Provider | Datei | Client / Schalter |
|---|---|---|
| `FBConstantElevation` | `core/` | primitives Fundament |
| `FBRunwayPlateauElevation` | `core/` | `fb-gym --elev const` |
| `FBBakedDemElevation` | `core/` | `fb-gym --elev swiss` (eingebackenes 90-m-Raster) |
| **`FBTilesElevation`** | **`world/`** | `fb-gym --elev tiles`, `gpu_native`, WASM — die einzige LIVE-DEM-Quelle |

Der Nutzen dieser Naht: **eine** Bodenwahrheit für alles (Missions-Bodenspawn, AGL/Radarhöhe,
Absturzerkennung) — und dieselbe DEM-Zahl, die der Renderer zeichnet, geht auch als
`position/terrain-elevation-asl-ft` in JSBSim (der „Crash-Vertrag": das Fahrwerk kollidiert gegen das
Gelände, das man sieht).

---

## 5 Die Terrain-Lib (`sim/src/terrain/`)

**Unser Code, nicht vendored** — eine ausgedünnte Fassung von libosmmesh: nur Gelände. Die
Vektor-/Gebäude-/Linien-/MVT-/PMTiles-Maschinerie des vollen osmmesh ist ausdrücklich außerhalb des
Scopes; FlightBox streamt Terrain.

| Datei | Inhalt |
|---|---|
| `geo.h` / `geo.cpp` | drei Konversionsschichten: **Web Mercator** (EPSG:3857 / Slippy-Tiles) ↔ lon/lat, **MVT-Lokalkoordinaten** → lon/lat, **ENU** (lokale Tangentialebene, Meter). Einziger zustandsbehafteter Teil: der ENU-Kontext, der sin/cos des Ursprungs cacht |
| `geo_ecef.cpp` | WGS84-**ECEF**-Konversionen + `osmmesh_tile_frac_to_geo` in voller Doppelpräzision. Eigene Übersetzungseinheit, damit sie zu 100 % testabdeckbar ist (`geo.cpp` trägt einen unerreichbaren Defensiv-Zweig) |
| `mesh.h` | der geteilte Mesh-Container: SoA (`positions`/`normals`/`uvs`/`indices`), ENU-Meter, Aufrufer besitzt den Speicher |
| `terrain.h` / `terrain.cpp` | Terrarium-PNG → float-Höhengitter → reguläres ENU-Mesh |
| `osmmesh.h` / `osmmesh_terrain.cpp` | der Kontext: Byte-Provider-Callback, Kachel holen, dekodieren, **stitchen**, meshen; dazu ein LRU dekodierter DEMs |

**Grenzen, die dokumentiert und einzuhalten sind:**

- ENU ist eine **flache Tangentialebene**, keine ECEF-Rundreise (Herleitung + Fehlerschranken §5.0).
  Für globales Rendern reicht das **nicht** — deshalb reprojiziert die Endstufe (§5.2) jeden Knoten
  exakt.
- Web-Mercator-Breite ist bei ± 85,05112878° gekappt (exakt `atan(sinh(π))·180/π`, WMTS-Spec /
  OGC Simple Tile Scheme); `osmmesh_geo_to_tile` weist alles darüber ab.
- MVT-`local_y` hat den Ursprung **oben links** (0 = Nordkante) — entgegengesetzt zur ENU-N-Achse;
  der Fast-Path `tile_enu_map` behandelt das Vorzeichen explizit.

### 5.0 Das ENU-Modell und seine Fehlerschranken

Die Projektion, verankert am ENU-Ursprung — der Kleinwinkel-Grenzfall der echten ECEF→ENU-Transformation:

```
e = (lon − lon0) · cos(lat0) · (π/180) · R
n = (lat − lat0)             · (π/180) · R
u = alt                                          R = 6378137 m (WGS84-Äquatorradius)
```

**Krümmungsfehler der n-Achse.** Dritter Taylor-Term von sinh gegen die lineare
Breite-zu-Meter-Beziehung: `Δn ≈ d³ / (6R²)`. Bei d = 10 km sind das **~4,1·10⁻⁶ m**, also
Mikrometer.

**Ellipsoid gegen Kugel.** ~0,3 % im Skalenfaktor — aber ein **BIAS, keine Formverzerrung**. Weil
ENU im selben Kugelmodell verankert ist, das alle Eingaben benutzen, ist die Rundreise
ENU→lon/lat exakt; die einzige Folge ist, dass „ein Meter ENU" gegen eine 6378137-m-Kugel definiert
ist statt gegen den lokalen Ellipsoidradius.

**Konsequenz:** volles ECEF an dieser Stelle kostete zwei sqrt/sin/cos je Konversion und kaufte bis
~100 km exakt nichts. Der globale Pfad steht deshalb DANEBEN (§5.2), nicht an seiner Stelle.

**Mercator→lokal auf einer Kachel ist ebenfalls linear.** Innerhalb einer z14-Kachel (~1,4 km
Vertikalausdehnung) ist die Mercator-y-Achse monoton und nahezu linear in der Breite. Der Fast-Path
`tile_enu_map` interpoliert deshalb rein linear zwischen den ENU-Positionen der Kachelecken.

**Der bekannte Restfehler des Fast-Path — eine Eigenschaft, kein Bug.** `d(lat)/d(y_mvt)` ist nicht
konstant (~0,04 % Unterschied Oberkante gegen Unterkante) und trägt am vertikalen Mittelpunkt der
Kachel höchstens **~0,3 m** Rest bei. Das ist GRÖSSER als der oben genannte Sub-mm-Modellfehler.
Deshalb prüft die Testsuite ausdrücklich nur die **vier Kachelecken** auf 1e-3 m Übereinstimmung mit
dem langsamen `osmmesh_tile_local_to_enu` — dort ist die n-Linearisierung exakt, und beide Pfade
benutzen dieselbe Längen-Linearisierung. Ein Mitte-der-Kachel-Kreuzvergleich würde eine Präzision
versprechen, die es nicht gibt. **Nicht durch Rücknahme der Trigonometrie in die heiße Schleife
„reparieren".**

### 5.1 Terrarium und das Stitching

Höhendekodierung (Mapzen/AWS-Terrarium-Spezifikation, jedes Pixel 24 bit RGB):

```
h = R·256 + G + B/256 − 32768   [m]
```

Gitterorientierung: Zeile 0 = **Nord**, Spalte 0 = **West** (PNG-Layout und Slippy-Konvention).
`build_mesh` platziert Vertex `(r,c)` bei `(c·dx, r·dy)`; weil `map->scale_n` bereits negativ ist
(ENU-N wächst nach Norden, `r` nach Süden), kommen die Positionen ohne weiteren Vorzeichenwechsel
ENU-korrekt heraus.

**Dreiecks-Winding — algebraisch bewiesen, nicht ergrünt.** Wenn ein Test hier fällt: erst die
Mathematik prüfen, nicht das Vorzeichen umdrehen. Mit `e = origin_e + lx·scale_e` (scale_e > 0) und
`n = origin_n + ly·scale_n` (scale_n < 0) gilt: `P(r+1,c)` liegt SÜDLICH von `P(r,c)`, `P(r,c+1)`
ÖSTLICH davon.

```
Dreieck (r,c), (r+1,c), (r+1,c+1):
  A = P(r+1,c)   − P(r,c) = (0, −, 0)     nach Süden
  B = P(r+1,c+1) − P(r,c) = (+, −, 0)     nach Südosten
  A × B = (0, 0, 0 − (−)·(+)) = (0, 0, +) → zeigt NACH OBEN

Dreieck (r,c), (r+1,c+1), (r,c+1):
  A = P(r+1,c+1) − P(r,c) = (+, −, 0)
  B = P(r,c+1)   − P(r,c) = (+, 0, 0)
  A × B = (0, 0, (+)·0 − (−)·(+)) = (0, 0, +) → zeigt NACH OBEN
```

Beide Dreiecke sind also CCW von +z (oben) gesehen, konsistent mit rechtshändigen, himmelwärts
zeigenden Normalen. Die Testsuite prüft das gegen, indem sie für ein flaches Gitter `normal.z ≈ 1`
verlangt.

**Normalen**: pro Fläche, flächengewichtet, je Vertex akkumuliert, am Ende normalisiert. Die
Flächengewichtung fällt daraus ab, dass die Flächennormale VOR der Akkumulation NICHT normalisiert
wird — das unnormierte Kreuzprodukt hat Betrag = 2 · Flächeninhalt. Für ein flaches Gitter haben alle
Flächen dieselbe Normale (0,0,1), das Ergebnis ist nach dem Re-Normalisieren also exakt (0,0,1).

**Stitching**: `osmmesh_fetch_tile` holt zusätzlich die vier Nachbarkacheln und gleicht die
Randhöhen ab — sonst klaffen an jeder Kachelgrenze Risse. Weil dieselbe Nachbarkachel bei jedem
angrenzenden Ziel erneut gebraucht wird (~15 Zugriffe je Ausgabekachel), sitzt davor ein **LRU
dekodierter Höhengitter** (`OM_DEM_LRU_CAP` = 128, abschaltbar per `FB_NODEMCACHE`) — der PNG-Decode
ist die dominante Kaltstart-Kosten.

Fehlende Kacheln sind **kein Fehler**: `osmmesh_fetch_tile` liefert `OSMMESH_OK` mit
`terrain == NULL`. Absenz wird auf dieser Ebene nicht als Störung behandelt; nur Decode-Fehler und
OOM geben negative Codes.

### 5.2 Die Endstufe: `w3_chunk_build_ecef` (`render/FBChunkMesh.h`)

Formal liegt sie in `render/`, gehört aber in diese Kette. Sie nimmt vom ENU-Mesh **nur das
Höhenfeld** (dessen Ränder schon gestitcht sind) und reprojiziert **jeden** Knoten durch die exakte
Mercator-Inverse und Geodätisch→ECEF. Damit gibt es weder Tangentialebenenfehler noch eine
Abhängigkeit von einem festen Heimatursprung.

| Ergebnis | Bedeutung |
|---|---|
| `verts` | `w3_vtx[]`: `pos` = ECEF-Versatz vom Origin (float; bei z14 < 2 km → Sub-Zentimeter), `norm` = echte ECEF-Flächennormale aus Kreuzprodukten der Nachbarversätze (trägt die Krümmung gratis), `uv` = `(frac_x, frac_y)` |
| `err` | max \|dezimierte Fläche − Quellhöhe\| in **Metern** — identisch gemessen wie im ENU-Pfad (eine Höhenfeld-Eigenschaft, projektionsunabhängig). Das ist die Zahl, die das LOD führt |
| `origin_out` | Kachelmitte in ECEF (double) — der Anker, den das Frame abzieht |

**Schürzen** (skirts): `skirt = max(2·err, 5 m)`, radial nach innen gezogen. Sie decken die
Sub-Pixel-Risse zwischen benachbarten LOD-Stufen — und sie sind auch der Grund, warum eine kurz
fehlende Nachbarkachel als Lücke tolerierbar ist. Das Mesh muss ein **reguläres Gitter** sein; eine
Dreieckssuppe wird abgelehnt (`return 0`).

---

## 6 Der Tile-Worker

`sim/src/app/FBTileWorkerMain.cpp` (C++/WASM) + `sim/web/fbtw-worker.js` (JS-Shim) +
`sim/web/fbtileworker.js/.wasm` (Artefakt).

**Warum ein eigenes WASM-Artefakt und kein pthread?** Der Byte-Cache ist eine **Main-Thread-JS-Map**.
Ein pthread würde jeden Fetch in genau den Thread zurückproxyen, den man gerade entlasten will. Ein
Web Worker besitzt seinen Fetch selbst. Deshalb ist der Worker ein eigenes Modul — **ohne WebGPU,
ohne JSBSim** — mit einem eigenen kleinen Export-Satz.

| Aspekt | Detail |
|---|---|
| Bau | `make -C sim worker` → `web/fbtileworker.js` + `.wasm`. `-sASYNCIFY -sFETCH -sINITIAL_MEMORY=64MB` |
| Exporte | `_fbtw_open`, `_fbtw_build`, `_fbtw_verts`, `_fbtw_nverts`, `_fbtw_err`, `_fbtw_origin`, `_fbtw_mips`, `_fbtw_mipbytes`, `_fbtw_ts`, `_fbtw_release` (+ `_malloc`/`_free`) — `extern "C"`, sonst zerlegt das Mangling die Namen still |
| Arbeit im Worker | DEM-/Albedo-Fetch, stbi-Decode, osmmesh-Meshing **und** die sRGB-Mip-Pyramide |
| Rückweg | fertige Vertexarrays + fertige Pyramiden als **Transferables** (zero-copy über `postMessage`) |
| Nebenläufigkeit | **EIN Build gleichzeitig je Instanz** — `fbtw_build` suspendiert unter ASYNCIFY am synchronen Fetch; ein überlappender zweiter Aufruf korrumpiert den geteilten Zustand. Der JS-Shim ruft es deshalb `{async: true}` und die Main-Seite gattert es |
| Kopie | Der Shim kopiert `verts`/`mips` mit `HEAPU8.slice()` heraus, **bevor** `fbtw_release` den Heap für den nächsten Build freigibt |

**Wenn der Worker fehlt**, hängt die App beim Start **still**: der Worker lädt sein Skript nicht
(404), meldet nie `opened`, `pump()` findet nie einen bereiten Worker, keine Kachel wird je gebaut,
`LoadProgress()` bleibt 0 — und der Ladebildschirm steht, bis der 30-s-Timeout ihn freigibt (dann auf
leeres Gelände). Genau deshalb hängt das Make-Target **`wasm` fest von `worker` ab** und baut immer
beide; `make -C sim worker` bleibt separat aufrufbar. Das ist eine Abhängigkeit im Makefile statt
zweier Targets, die man sich merken müsste.

---

## 7 `fb-tiles` aus Klientensicht

Der Server (`tiles/`, eigenes Makefile, eigenes Image, `tiles/up.sh`) ist **nicht** Teil dieser
Änderung und wird hier nur beschrieben: was er liefert, unter welchen Endpunkten, in welcher
Auflösung.

### 7.1 Endpunkte

| Endpunkt | Antwort | Statuscodes | Klient |
|---|---|---|---|
| `/t/terrain/z/x/y` | Terrarium-RGB-PNG (DEM) | 200 / 202 „fetching" / 204 „absent" | osmmesh via Provider-Callback; `fb_stream_dem` roh |
| `/t/vector/z/x/y` | Mapbox-Vector-Tile (pbf) | dito | serverintern (Bakes); der Sim-Client fragt es nicht direkt |
| `/t/imagery/z/x/y` | JPEG-Luftbild | dito | serverintern (Photo-Bakes) |
| `/bake/osm/z/x/y?tex=N&v=VER` | gerenderte OSM-Albedo (PNG) | 200 / 204 | `fb_stream_pyramid(mode=0)` |
| `/bake/photo/z/x/y?tex=N` | Luftbild-Albedo-Mosaik | 200 / 204 | `fb_stream_pyramid(mode=1)` |
| `/t/lights/z/x/y` | binäre Nachtlichtliste | 200 (auch leer) / 204 (kein Vektordatum) | `fb_stream_lights` |
| `/t/stars/{band}/0/0` | HYG-Sternband, 6 B/Stern | 200 / 404 | `fb_fetch_stars` (4 Bänder, verkettet) |
| `/elev?lat=&lon=[&block=1]` | Text: eine Zahl (m ASL) + Newline | 200 / **503 „no dem"** (kalt) | `fb_stream_ground` |
| `/wx` | globales Wind-/Wolkenpaket, Binärformat `FBWX` (§9) | 200 / **503** (kein GFS-Lauf erreichbar) | künftiger `FBWeatherProvider`; heute die Fixture in `tiles/testdata/` |
| `/health` | Text-Statistikzeile | 200 | Betrieb |

**Die Statuscode-Semantik ist der eigentliche Vertrag:**

| Code | Bedeutung für den Klienten |
|---|---|
| 200 | terminal — Bytes da |
| 202 | angenommen, Fetch läuft — **später nochmal fragen** |
| 204 | ein **echtes Loch**: hier gibt es nichts, und es wird auch nichts geben → nicht wiederholen (`fb_stream_pyramid` gibt −1, `FBWorld` merkt sich `alt = -1`) |
| 404 / 5xx | transient → wiederholen |
| 503 auf `/elev` | DEM noch kalt; `?block=1` wartet stattdessen bis 3 s |

Der Unterschied zwischen **absent** (204) und **leer** (200 mit count = 0) ist bei `/t/lights`
ausdrücklich modelliert: eine dunkle Ozeankachel ist etwas anderes als ein fehlendes Vektordatum.

### 7.2 Datenquellen und Auflösung

| Kind | Upstream | Max-Zoom | Content-Type |
|---|---|---|---|
| `terrain` | `s3.amazonaws.com/elevation-tiles-prod/terrarium/z/x/y.png` (Mapzen/AWS Terrarium; Copernicus-basiert) | 15 | image/png |
| `vector` | `tiles.versatiles.org/tiles/osm/z/x/y` (OSM/Shortbread) | 14 | MVT |
| `imagery` | ArcGIS `World_Imagery/MapServer/tile/z/y/x` (Esri; **y/x vertauscht** im URL-Muster) | 19 | image/jpeg |

Daraus folgt der `kMaxZ = 14` des Streamers: feiner als die Vektorquelle bringt der OSM-Bake nichts.
Die Höhenabfrage `/elev` samplet auf **z13** (`FB_DEM_Z`), **bilinear**, mit einem 24-Kacheln-LRU im
Speicher.

Bakes werden mit `?tex=N` angefordert (der Client fragt 512; Serverdefault 1024). Die OSM-Bakes
tragen `?v=FB_OSM_STYLE_VER` (heute **11**) — sie sind `Cache-Control: immutable`, also muss die URL
sich ändern, wenn sich das Rendering ändert (der Vorfall „Streifen-Bug noch sichtbar" kam genau
daher). Photo-Bakes bleiben unversioniert. `sim/src/terrain/style_ver.h` und `tiles/src/style_ver.h`
sind dieselbe Zahl auf beiden Seiten.

### 7.3 Vor dem Server: nginx

Das Container-Frontend (`tiles/nginx.conf`) hört auf **:8081** und cacht selbst
(`proxy_cache`, 5 GB, 30 d); nur Misses gehen an fb-tiles auf **127.0.0.1:8082**.

| Regel | Wert |
|---|---|
| Cache-Key | `$uri$is_args$args` — die `?v=`/`?tex=`-Query IST Teil der Kachel-Identität |
| Gültigkeit | 200 → 30 d, 404 → 1 min; **alles andere** (202, 500, 503) wird durch Auslassung nie gecacht |
| `proxy_cache_lock` | an, Timeout 300 s — N gleichzeitige Misses derselben Kachel kollabieren zu EINEM Upstream-Request, vor fb-tiles' eigenem Inflight-Dedup |
| `proxy_read_timeout` | 300 s — ein kalter Bake blockiert und darf nicht abgeschnitten werden |
| Nie gecacht | `/health`, `/elev` (Live-Werte) |
| `/wx` (eigener `location = /wx`-Block) | die EINZIGE cachebare Route ohne `immutable`: der Origin rechnet die Restlaufzeit des gelieferten GFS-Laufs und sagt sie als `Cache-Control: max-age`. nginx wertet die Antwort-Header VOR `proxy_cache_valid`, also entscheidet der Origin; `proxy_cache_valid 200 1h` ist nur der Rückfall. 503 bekommt **keine** Gültigkeitsregel → eine NOMADS-Störung kann nie als Wetter gespeichert werden |

fb-tiles selbst: Verbindungs-Thread-Pool (`TILES_THREADS`), Disk-Cache unter `TILES_CACHE`
(Default `/var/cache/fbtiles`), Sternbänder aus `STARS_DIR`. TLS ist ein dokumentierter, offener
Punkt in der Konfiguration.

---

## 8 On-Demand — und warum jeder Punkt der Erde ein gültiger Start ist

Nichts ist vorgeladen. Die Konsequenzen, in der Reihenfolge, in der sie greifen:

1. **Der Wurzelring wird um die Kamera gelegt** (`kRootZ` = 8, Radius `ceil(ViewM/span) + 1` Kacheln)
   — es gibt kein Gebiet, keinen „Kartenrand", keine Datei, die vorher da sein müsste.
2. **Der Zielschnitt ist reine Geometrie** (`WantSplit`) und daher sofort bekannt: eine Teleportation
   fordert direkt ihre Endblätter an, statt sich durch eine LOD-Leiter hochzubauen.
3. **Der Ladebildschirm hält die Sim an**, bis der Zielschnitt zu 95 % resident ist. Der erste
   geflogene Frame ist damit schon voll aufgelöst — und die Boden-DEM unter dem Spawn ist geladen,
   bevor JSBSim das erste Mal integriert.
4. **Fehlende Daten sind ein definierter Zustand, kein Fehler**: 204 = Loch (Photo fällt auf OSM
   zurück), fehlende Nachbarkachel = Lücke, die die Schürzen decken, kaltes `/elev` = 503 und der
   Startpfad fragt mit `?block=1`.

Damit ist ein `spawn`-Eintrag in einer `.fbm`-Datei überall auf der Erde gültig, ohne dass irgendwo
ein Gebiet definiert werden müsste.

---

## 9 Wetter — `/wx`

Die vierte Datenart neben DEM, Vektor und Luftbild, und die einzige **ungekachelte**: weltweiter
Wind und Bewölkung aus dem **NOAA GFS**, on-demand geholt, gecacht, in einem eigenen kompakten
Binärformat (`FBWX`) ausgeliefert. Server-Seite: `tiles/src/wx.c` (Endpunkt, Laufermittlung,
Quantisierung), `tiles/src/grib2.c` (GRIB2-Dekoder), `tiles/src/wxfmt.h` (**die Formatkonstanten —
dieser Header ist der maschinenlesbare Teil dieses Abschnitts**). Fixture eines echten Abrufs:
`tiles/testdata/wx-gfs-2026-07-27T00Z-step2-v1.wxb` (+ `manifest.txt`).

### 9.1 Warum EIN Blob und keine Kacheln

Bei 0,25° ist das globale Feld **einer** Variablen 1440×721 Punkte — die DEM-Situation genau
umgekehrt: nicht ein winziger Ausschnitt aus einem riesigen Datensatz, sondern ein Datensatz, der
als Ganzes klein ist. Drei Gründe für ein einziges Paket statt eines Blobs pro Variable:

1. Der Klient will **alles einmal pro Sitzung**. N Blobs = N Rundreisen und N Cache-Einträge für
   eine einzige logische Sache.
2. **Ein Lauf ist eine Atmosphäre.** Getrennte Blobs könnten über eine Laufgrenze fallen und dem
   Klienten Wind aus dem 06z- und Bewölkung aus dem 12z-Lauf geben. Im gepackten Paket ist der
   Laufzeitstempel ein **Header-Feld für alle 20 Felder gemeinsam**, und der Server weigert sich,
   Datensätze zweier Läufe in ein Paket zu schreiben.
3. Der Header ist selbstbeschreibend (Feldliste + Offsets), also kostet „alles in einem" nichts an
   Flexibilität: wer nur den Wind braucht, liest zehn Ebenen und ignoriert den Rest.

### 9.2 Der Endpunkt

```
GET /wx          → 200  application/octet-stream, FBWX-Blob (heute 8 317 984 B)
                 → 503  kein GFS-Lauf erreichbar UND keiner auf Platte
```

Keine Query-Parameter, keine Pfadvarianten. Der Statuscode-Vertrag ist der bestehende (§7.1), auf
diese Datenart angewandt:

| Code | Bedeutung |
|---|---|
| **200** | terminal — Bytes da. Auch der Fall „ältester Lauf von Platte, NOMADS gerade nicht erreichbar": dann trägt die Antwort `X-Wx-Stale: 1`. |
| **503** | keine Daten, transient → wiederholen. Wird **nie** gecacht (weder von nginx noch auf Platte). |
| **404** | nur für einen anderen Pfad (`/wx/irgendwas`) — es gibt genau eine Ressource. |
| **202** | kommt nicht vor. `/wx` **blockiert** wie `/bake`, bis das Paket fertig ist; das HTTP-Timeout des Klienten ist die einzige Frist. |
| **204** | kommt nicht vor. Bei DEM/Vektor unterscheidet 204 „echtes Loch" von „leer"; Wetter hat **kein Loch** — die Atmosphäre bedeckt jeden Punkt der Erde. Ein Feld ohne Wert an einem Gitterpunkt (nur die Wolkenuntergrenze) wird **im Blob** markiert, nicht per Statuscode (§9.5). |

Antwort-Header:

| Header | Inhalt |
|---|---|
| `Cache-Control: public, max-age=N` | Restlaufzeit des gelieferten Laufs: `Laufzeit + 6 h + 4 h − jetzt`, mindestens 300 s. **Kein `immutable`** — als einzige cachebare Route. |
| `X-Wx-Format: FBWX/1` | Formatversion, identisch mit dem Header-Feld |
| `X-Wx-Run` / `X-Wx-Valid` | ISO-8601-UTC des Laufs bzw. der Gültigkeitszeit (bei f000 gleich) |
| `X-Wx-Stale: 0\|1` | 1 = der Lauf konnte in diesem Durchgang nicht gegen NOMADS bestätigt werden (Netz weg) |
| `X-Cache-Status` | von nginx, wie bei allen cachebaren Routen |

### 9.3 Quelle und Laufermittlung

NOAA GFS über NOMADS, **0,25°**, ohne Schlüssel. Geholt wird über den `grib_filter`-CGI
(`https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl`), der serverseitig nach Variable und
Level zuschneidet — das volle GRIB (~500 MB) wird nie geladen. Zeitschritt **f000** (Analyse) = das
„Jetzt" des Simulators; `run == valid`.

**Drei** Filter-Requests, nicht einer: der CGI liefert das **Kreuzprodukt** aus gewählten Variablen
und Leveln, ein Sammel-Request zöge also zusätzlich TCDC auf den vier Druckflächen und HGT an der
Oberfläche mit (~5 MB Abfall). Die drei Gruppen treffen exakt die 20 gewünschten Records.

**Laufermittlung** (`wx_acquire`, `wx.c`): GFS läuft 4×/Tag (00/06/12/18Z) und landet ~3,5–5 h nach
seiner Analysezeit auf NOMADS. Der Server probiert vom **neuesten Zyklus rückwärts**, je Zyklus
zuerst Platte, dann Netz:

* **Nicht fertig** — NOMADS antwortet mit 404 *oder* mit `200` und einer HTML-Fehlerseite
  („Data file is not present"). Der Statuscode allein beweist also nichts; die eigentliche
  Unterscheidung ist die **GRIB-Magic am Anfang des Bodys** plus die Vollzähligkeit aller 20
  Records. Genau diese Prüfung ist der Grund, warum eine Fehlerseite hier nie als Wetter im Cache
  landen kann (die `/elev`-Lehre). Reaktion: nächstälterer Zyklus (bis zu 4 = 24 h zurück).
* **Nicht erreichbar** (Verbindungsfehler oder 5xx) — der Netzpfad wird **sofort** aufgegeben, statt
  vier weitere Zyklen anzuklopfen; der Rest des Durchgangs sucht nur noch auf Platte (bis zu 8
  Zyklen = 48 h) und liefert das Neueste, was da ist, mit `X-Wx-Stale: 1`.
* **Gar nichts** — 503, und nichts wird geschrieben.

Nach jedem Durchgang, der nichts Neues fand, sperrt ein **Dämpfer** (`g_next_probe`, 600 s) weitere
NOMADS-Probes; sonst würde jeder Request in der Lücke zwischen zwei Läufen erneut anklopfen.
Gleichzeitige Anfragen teilen sich **einen** Build (eine Mutex + Condvar, ein Artefakt = ein
Inflight-Schlüssel): 16 parallele Kaltstarts → `wx_built=1`.

### 9.4 Das Gitter

| | |
|---|---|
| Quellgitter | GFS 0,25°, 1440×721, Zeile 0 = 90 °N, Spalte 0 = 0 °E (Scanmodus 0) |
| Ausgabegitter | jeder **`grid_step`**-te Quellpunkt auf einem exakten Untergitter; Default `grid_step = 2` → **0,5°, 720×361** |
| Warum unterabgetastet und nicht gemittelt | so ist **jeder ausgelieferte Wert buchstäblich ein GFS-Gitterpunktwert** und keine von uns erfundene Zahl — das macht auch den Punktvergleich gegen einen unabhängigen GFS-Konsumenten (§9.7) überhaupt erst aussagekräftig. Für synoptische Felder ist der Verlust bedeutungslos: das 0,25°-Produkt ist selbst schon Ausgabe eines gröber auflösenden Spektralmodells. |
| Warum überhaupt `grid_step` | bei 0,25° wäre derselbe Feldsatz **33 MB** — weit jenseits dessen, was ein Klient einmal pro Sitzung zieht. `grid_step 2` ⇒ 8,3 MB, `4` ⇒ 2,1 MB. Es ist ein **Compile-Time-Makro** (`FB_WX_STEP` in `wx.c`), kein Query-Parameter: keine zweite Cache-Identität, kein Per-Request-Zweig. |

Der Klient kodiert davon **nichts**: `nx`, `ny`, `lat0`, `lon0`, `dlat`, `dlon` stehen im Header.

```
sample(i,j) liegt bei   lat = lat0 + j*dlat      (90 − 0,5·j)
                        lon = lon0 + i*dlon      (0,5·i)
Index im Plane:         idx = j*nx + i
```

`dlat` ist **negativ** (Zeile 0 = Nordpol). Längengrade laufen 0…359,5 °E; `flags` Bit 0
(`FB_WX_HDR_FLAG_LON_WRAP`) sagt, dass Spalte `nx−1` genau ein `dlon` vor Spalte 0 liegt — bilinear
also `i` modulo `nx` nehmen. Breitengrade wickeln **nicht**: `j` auf `[0, ny−1]` klemmen.

### 9.5 Das Format `FBWX`

Alles **little-endian**. Aufbau: Header (64 B) + `field_count` Deskriptoren (je 24 B) + die Planes.
Jeder Deskriptor trägt seinen **absoluten** Offset, ein Leser muss also nie Größen aufsummieren.

**Header, 64 Byte:**

| Off | Typ | Feld | Bedeutung |
|---:|---|---|---|
| 0 | u32 | `magic` | `0x58574246` = `'F','B','W','X'` |
| 4 | u16 | `format_version` | heute **1**. Ändert sich, sobald ein Byte etwas anderes bedeutet. |
| 6 | u16 | `header_bytes` | 64 — wo die Deskriptoren beginnen (wächst in künftigen Versionen) |
| 8 | u16 | `nx` | Spalten (720) |
| 10 | u16 | `ny` | Zeilen (361) |
| 12 | f32 | `lat0` | Breite der Zeile 0 (+90.0) |
| 16 | f32 | `lon0` | Länge der Spalte 0 (0.0) |
| 20 | f32 | `dlat` | Breitenschritt je Zeile (**−0.5**) |
| 24 | f32 | `dlon` | Längenschritt je Spalte (+0.5) |
| 28 | u32 | `run_epoch` | Analysezeit des GFS-Laufs, UTC-Sekunden — **aus dem GRIB selbst**, nicht aus der URL |
| 32 | u32 | `valid_epoch` | Gültigkeitszeit (= `run_epoch` bei f000) |
| 36 | u32 | reserviert | 0 |
| 40 | u16 | `field_count` | 20 |
| 42 | u16 | `desc_bytes` | 24 — Schrittweite der Deskriptortabelle |
| 44 | u32 | `payload_bytes` | Summe aller Planes (8 317 440) |
| 48 | u8 | `flags` | Bit 0 = Längengrad wickelt |
| 49 | u8 | `source` | 1 = NOAA GFS 0,25° |
| 50 | u16 | `grid_step` | Quellpunkte je Ausgabepunkt (2) |
| 52 | u8[12] | reserviert | 0 |

**Deskriptor, 24 Byte:**

| Off | Typ | Feld | Bedeutung |
|---:|---|---|---|
| 0 | u8 | `var` | 1 `WIND_U` (m/s, ostwärts) · 2 `WIND_V` (m/s, nordwärts) · 3 `HEIGHT` (m) · 4 `CLOUD` (%) · 5 `VIS` (m) |
| 1 | u8 | `level_kind` | 1 `AGL` · 2 `ISOBARIC` · 3 `CLOUD_LOW` · 4 `CLOUD_MID` · 5 `CLOUD_HIGH` · 6 `ATMOSPHERE` · 7 `SURFACE` · 8 `CLOUD_CEIL` |
| 2 | u16 | `level_value` | Meter bei `AGL`, hPa bei `ISOBARIC`, sonst 0 |
| 4 | u8 | `bits` | 8 oder 16 |
| 5 | u8 | `flags` | Bit 0 = das Feld kennt „kein Wert" |
| 6 | u16 | `missing_raw` | dieser Rohwert bedeutet „kein Wert" (nur wenn Bit 0 gesetzt) |
| 8 | f32 | `scale` | |
| 12 | f32 | `offset` | |
| 16 | u32 | `payload_off` | absoluter Byte-Offset der Plane im Blob |
| 20 | u32 | `payload_bytes` | `nx·ny·bits/8` |

**Wert-Rekonstruktion** — eine Zeile, für jedes Feld dieselbe:

```
raw = 8-bit-Byte oder 16-bit-LE-Wort an  payload_off + (j*nx + i) * bits/8
wert = offset + raw * scale            (Einheit aus `var`)
```

und, wenn `flags & 1` und `raw == missing_raw`: **kein Wert** (nicht 0, nicht interpolieren —
Nachbarpunkte, die ebenfalls „kein Wert" sind, aus der Interpolation ausschließen).

Die Einheit folgt aus `var`, deshalb gibt es kein Einheitenfeld. `HEIGHT` ist **geopotentielle Höhe
über MSL** (GFS-Parameter „Geopotential height", gpm) — der Unterschied zu geometrischen Metern
liegt bei 11 km unter 0,3 %, für die Zuordnung eines Windniveaus zu einer Flughöhe irrelevant.

### 9.6 Die 20 Felder und ihre Quantisierung

Die Quantisierungsfenster sind **fest verdrahtet, nie aus den Daten abgeleitet**: gleiche Eingabe →
gleiche Bytes, und ein Klient darf die Bedeutung eines Rohwerts hart kodieren. Werte außerhalb des
Fensters **sättigen** (klemmen), sie laufen nie über.

| # | var | level | bits | `scale` | `offset` | `missing_raw` | `payload_off` | Bytes | Auflösung |
|---:|---|---|---:|---|---:|---|---:|---:|---|
| 0 | `WIND_U` | `AGL` 10 m | 16 | 0.00549324788 | −180 | — | 544 | 519 840 | 0,0055 m/s |
| 1 | `WIND_V` | `AGL` 10 m | 16 | 0.00549324788 | −180 | — | 520 384 | 519 840 | 0,0055 m/s |
| 2 | `WIND_U` | `ISOBARIC` 850 | 16 | 0.00549324788 | −180 | — | 1 040 224 | 519 840 | 0,0055 m/s |
| 3 | `WIND_V` | `ISOBARIC` 850 | 16 | 0.00549324788 | −180 | — | 1 560 064 | 519 840 | 0,0055 m/s |
| 4 | `WIND_U` | `ISOBARIC` 700 | 16 | 0.00549324788 | −180 | — | 2 079 904 | 519 840 | 0,0055 m/s |
| 5 | `WIND_V` | `ISOBARIC` 700 | 16 | 0.00549324788 | −180 | — | 2 599 744 | 519 840 | 0,0055 m/s |
| 6 | `WIND_U` | `ISOBARIC` 500 | 16 | 0.00549324788 | −180 | — | 3 119 584 | 519 840 | 0,0055 m/s |
| 7 | `WIND_V` | `ISOBARIC` 500 | 16 | 0.00549324788 | −180 | — | 3 639 424 | 519 840 | 0,0055 m/s |
| 8 | `WIND_U` | `ISOBARIC` 250 | 16 | 0.00549324788 | −180 | — | 4 159 264 | 519 840 | 0,0055 m/s |
| 9 | `WIND_V` | `ISOBARIC` 250 | 16 | 0.00549324788 | −180 | — | 4 679 104 | 519 840 | 0,0055 m/s |
| 10 | `HEIGHT` | `ISOBARIC` 850 | 8 | 4.70588255 | 600 | — | 5 198 944 | 259 920 | 4,7 m |
| 11 | `HEIGHT` | `ISOBARIC` 700 | 8 | 5.88235283 | 2000 | — | 5 458 864 | 259 920 | 5,9 m |
| 12 | `HEIGHT` | `ISOBARIC` 500 | 8 | 7.05882359 | 4300 | — | 5 718 784 | 259 920 | 7,1 m |
| 13 | `HEIGHT` | `ISOBARIC` 250 | 8 | 11.7647057 | 8500 | — | 5 978 704 | 259 920 | 11,8 m |
| 14 | `HEIGHT` | `CLOUD_CEIL` | 16 | 0.305185109 | 0 | **65535** | 6 238 624 | 519 840 | 0,31 m |
| 15 | `CLOUD` | `ATMOSPHERE` | 8 | 0.392156869 | 0 | — | 6 758 464 | 259 920 | 0,39 % |
| 16 | `CLOUD` | `CLOUD_LOW` | 8 | 0.392156869 | 0 | — | 7 018 384 | 259 920 | 0,39 % |
| 17 | `CLOUD` | `CLOUD_MID` | 8 | 0.392156869 | 0 | — | 7 278 304 | 259 920 | 0,39 % |
| 18 | `CLOUD` | `CLOUD_HIGH` | 8 | 0.392156869 | 0 | — | 7 538 224 | 259 920 | 0,39 % |
| 19 | `VIS` | `SURFACE` | 16 | 0.373846024 | 0 | — | 7 798 144 | 519 840 | 0,37 m |

Warum diese Breiten:

* **Wind 16 bit über ±180 m/s.** Der stärkste je gemessene Jetstream liegt bei ~110 m/s; das Fenster
  ist bewusst weit und die Auflösung mit 0,0055 m/s trotzdem eine Größenordnung feiner als die
  Modellunsicherheit. Die Quelle selbst quantisiert gröber (GFS packt UGRD auf 9–13 Bit).
* **Höhen 8 bit mit engem Fenster je Druckfläche.** Eine geopotentielle Fläche variiert global nur
  um 800–2100 m; die Fenster oben lassen ≥400 m Reserve auf jeder Seite und lösen trotzdem 5–12 m
  auf. Sie dienen dazu, ein Windniveau in Metern zu **verorten**, nicht dazu, Höhenmesser zu setzen.
* **Bewölkung 8 bit.** Der Wertebereich ist 0–100 %, GFS liefert selbst nur eine Nachkommastelle.
* **Sichtweite 16 bit, obwohl es ein „Bewölkungsfeld" ist.** Sichtweite interessiert genau dort, wo
  sie klein ist: bei 8 bit wären 200 m Nebel ±47 m unsicher. GFS deckelt „unbegrenzt" bei
  ~24 135 m, das Fenster geht bis 24 500 m.
* **Wolkenuntergrenze mit echtem Missing-Flag.** GFS meldet „keine Untergrenze" nicht per Bitmap,
  sondern als Höhe ~20 000 m. Werte ≥ 19 000 m werden hier zu `missing_raw = 65535` — ein Renderer
  darf keine Wolkenbasis in 20 km Höhe zeichnen. Anteil im Beispiel: 47,2 % der Gitterpunkte, exakt
  deckungsgleich mit `GFS ≥ 19000` (nachgeprüft).

**Wolkenuntergrenze vs. Druck:** geprüft — der GFS-`f000`-Analyseschritt liefert die Untergrenze als
`HGT:cloud ceiling` (geopotentielle Höhe in m). Ein `PRES:cloud base`-Record existiert in diesem
Schritt **nicht**; die Höhe ist also nicht nur die bequemere, sondern die einzige Form.

**Bewölkung, welche Records:** im Analyseschritt heißen die Schichten `LCDC`/`MCDC`/`HCDC` (low/
middle/high cloud layer, momentan) und die Gesamtbedeckung `TCDC:entire atmosphere` — nicht, wie in
den Vorhersageschritten, viermal `TCDC` mit unterschiedlichem Level.

### 9.7 GRIB2 — warum der Dekoder eigener Code ist

Der pragmatische Weg wäre `wgrib2` oder die eccodes-Tools im Container gewesen. Gemessen:

* **`wgrib2` ist in Debian trixie nicht paketiert** (`apt-cache policy wgrib2` → leer).
* `libeccodes-tools` gibt es, zieht aber `libeccodes0` + `libnetcdf22` nach (~40 MB im Image) und
  hätte pro Abruf einen `fork/exec` plus entweder Textparsing von 20 Mio. Werten oder eine
  Zwischendatei bedeutet.

Stattdessen: **`tiles/src/grib2.c`**, ~330 Zeilen. Der Grund, dass das vertretbar ist, steht in den
Daten — alle 20 GFS-Records benutzen dieselbe, engste Teilmenge des Standards:

| | |
|---|---|
| Gitter | Template **3.0** (reguläres lat/lon), Scanmodus 0 |
| Produkt | Template **4.0** (auch 4.8 akzeptiert) |
| Packung | Template **5.3** — complex packing + spatial differencing 2. Ordnung, `mvm = 0`, keine Bitmap. (5.0 simple packing ist mit implementiert.) |
| **Nicht** dabei | JPEG2000 (5.40), PNG (5.41), Bitmaps in Sektion 6, Missing-Value-Management in 5.3 |

Was nicht dazugehört, wird **namentlich abgelehnt** (`fb_grib2_last_error`) statt geraten — es ist
eine Upstream-Antwort, also eine Systemgrenze. Eine Feinheit, die in keiner WMO-Tabelle klar
dasteht und die ein Nachbauer wissen muss: in Sektion 7 beginnen die drei Deskriptor-Arrays
(Gruppen-Referenzwerte, -Breiten, -Längen) **jeweils an einer Oktettgrenze**; ohne dieses Padding
ergibt die Summe der Gruppenlängen nicht die Punktzahl (erster Debug-Befund: 1 468 303 statt
1 038 240).

**Verifikation gegen unabhängige Referenzen.** Zwei, weil sie verschiedene Dinge belegen:

*1 — ecCodes 2.41 (Container, `grib_get -l lat,lon,1`), der maßgebliche Dekodier-Check* an fünf
Punkten, GFS 2026-07-27 00Z:

| Punkt | Größe | ecCodes | `/wx` | Δ | Quantisierungsschritt |
|---|---|---|---|---|---|
| 46,5 N / 7,5 E | u 250 hPa | 5,64641 m/s | 5,6443 | 0,0021 | 0,0055 |
| | v 250 hPa | −23,3125 | −23,3106 | 0,0019 | 0,0055 |
| | gh 250 hPa | 10 804,5 m | 10 805,9 | 1,4 | 11,8 |
| | vis | 24 134,8 m | 24 134,8 | 0,0 | 0,37 |
| 35,0 N / 139,5 E | ceiling | 5 948,38 m | 5 948,4 | 0,02 | 0,31 |
| | tcc / lcc / hcc | 95,5 / 29,7 / 95,5 % | 95,7 / 29,8 / 95,7 | ≤0,2 | 0,39 |
| 60,0 S / 60,0 W | 10u | 15,1738 m/s | 15,1751 | 0,0013 | 0,0055 |
| | lcc / mcc / hcc | 87,8 / 14,5 / 0 % | 87,8 / 14,5 / 0,0 | 0,0 | 0,39 |

Über **alle 20 Felder × alle 259 920 Gitterpunkte** beträgt der maximale Fehler gegen eine
unabhängige Vollfeld-Dekodierung exakt **0,5 Quantisierungsschritte** — also reine Rundung, kein
Dekodierfehler; die Missing-Maske der Wolkenuntergrenze stimmt punktweise überein.

*2 — Open-Meteo (`api.open-meteo.com/v1/gfs`, `models=gfs_global`), ein unabhängiger operationeller
GFS-Konsument*, 46,5 N / 7,5 E, 2026-07-27T00:00Z:

| Größe | Open-Meteo | `/wx` |
|---|---|---|
| Wind 250 hPa | 24,01 m/s aus 346° | 23,98 m/s aus 346,4° |
| gh 250 hPa | 10 803,81 m | 10 805,9 m |
| Wind 850 hPa | 2,04 m/s aus 210° | 2,11 m/s aus 210,5° |
| gh 850 hPa | 1 520,0 m | 1 522,4 m |
| Wind 10 m | 2,08 m/s aus 215° | 2,34 m/s aus 208,4° |
| Sichtweite | 24 140 m | 24 134,8 m |

Windrichtung/-betrag und Höhen stimmen auf <0,1 m/s bzw. <2,5 m — die Höhendifferenz ist genau ein
Viertel Quantisierungsschritt. Der 10-m-Wind weicht etwas mehr ab, weil Open-Meteo räumlich
interpoliert und geländeabhängig herunterskaliert (es meldet für den Punkt eine eigene
Geländehöhe), während `/wx` den rohen Gitterpunkt liefert. **Bewölkung eignet sich als Vergleich
nicht**: Open-Meteos GFS-Wolkenschichten sind aus relativer Feuchte gerechnet, nicht die
GFS-eigenen LCDC/MCDC/HCDC-Diagnosen — an 60 S/60 W meldet Open-Meteo „high 100 %", während GFS
selbst (per ecCodes bestätigt) 0 % sagt. Für die Wolken ist ecCodes die Referenz, nicht Open-Meteo.

### 9.8 Determinismus und die Gym-Fixture

Ein Blob ist eine **reine Funktion von (GFS-Lauf, Formatversion, `grid_step`)**. Es steht kein
Erzeugungszeitstempel drin (Offset 36 ist bewusst reserviert und null), keine Zufallszahl, keine
datenabgeleitete Quantisierung. Zwei unabhängige Kaltstarts desselben Zyklus — verschiedene Rechner,
verschiedene Compiler (clang/macOS gegen gcc/Debian im Container) — liefern **byte-identische**
8 317 984 Bytes (nachgemessen, md5 `17b33c82bafae29442eb6d1cc12fb6de`).

Damit gilt für die eingebackene Fixture:
`tiles/testdata/wx-gfs-2026-07-27T00Z-step2-v1.wxb`, sha256
`acded0200d49926203d4548301a2fd1586b6e3c5ecbf61fbd0355e6f9c609ede`. Sie ist ein unveränderter
200-Body von `/wx` und kann als feste Gym-Wetterlage übernommen werden; ein Regressionstest darf sie
per Prüfsumme vergleichen statt per Toleranz. Die Dateinamen auf Platte tragen `grid_step` und
Formatversion (`gfs2_2026072700_v1.wxb`), sind also wie die Bake-Dateinamen an die Version gekoppelt
— eine Formatänderung verschiebt das Artefakt, statt das alte zu vergiften.

### 9.9 Betriebszahlen

Gemessen am 2026-07-27, GFS-Zyklus 00Z, Apple A18 Pro, Podman-VM:

| Größe | Wert |
|---|---|
| Rohbytes von NOMADS | **15 451 174 B** GRIB2 in 3 Filter-Requests (7 313 561 + 4 463 283 + 3 674 330) |
| Ausgelieferte Bytes | **8 317 984 B** (Faktor 0,54 gegen roh, bei 20 Mio. → 260 k Gitterpunkten je Feld) |
| Über die Leitung (nginx gzip) | **4 599 798 B** |
| GRIB2-Dekodierung, 20 Felder / 20,8 Mio. Punkte | **0,10 s** (3 Läufe: 0,103 / 0,102 / 0,099) |
| Kalt, Origin direkt | **7,30 s** — praktisch vollständig NOMADS-Latenz (davon 0,10 s Dekodierung) |
| Kalt, durch nginx (`X-Cache-Status: MISS`) | **7,06 / 7,49 s** (zwei Läufe auf leerem Cache) |
| Von Platte nach Neustart (inkl. eines NOMADS-Probes) | **0,26 s** |
| Resident im Origin | **0,002–0,005 s** |
| Wiederholung durch nginx (`X-Cache-Status: HIT`) | **0,081 s** |
| 16 gleichzeitige Kaltstarts, Origin direkt im Container | 6,93 s Wall, **`wx_built = 1`**, alle 16 Antworten byte-identisch (Host-Binary: 6,12 s, gleiches Ergebnis) |
| 24 gleichzeitige Anfragen durch nginx nach einem MISS | 0,53 s Wall, **24× `HIT`**, Origin unberührt |
| Speicher während des Builds | Blob 8,3 MB + eine GRIB-Gruppe ≤7,3 MB + Dekoder-Scratch 8,3 MB |
| Platte im Origin | ein Zyklus = 8,3 MB; ältere werden nach dem Build **namentlich** weggeräumt (kein `readdir`), Dauerzustand ≤ 8 Dateien ≈ 75 MB |

Größe bei anderen `grid_step`: 1 → 33,2 MB · **2 → 8,3 MB** · 4 → 2,1 MB.

`/health` bekommt eine eigene Gruppe:

```
wx_served=N wx_built=N wx_disk_hits=N wx_fetch_fail=N wx_decode_fail=N wx_stale_served=N wx_run_fallback=N
```

`wx_run_fallback` zählt die Durchgänge, in denen der neueste Zyklus noch nicht veröffentlicht war
und auf den vorigen zurückgefallen wurde — der Normalfall in den ~4 h nach einer Analysezeit, kein
Fehler. `wx_fetch_fail` und `wx_decode_fail` sind die echten Fehlerzähler.

### 9.10 Was der Simulator daraus baut (noch offen)

Der Endpunkt existiert, der Konsument nicht. Die nächste Sim-Runde hängt daran:

* `FBElevationProvider`-Geschwister im Core: ein `FBWeatherProvider` (`WindAt(lat,lon,alt)` als
  Vertikalinterpolation über die vier Druckflächen + 10 m AGL, `CloudAt`, `VisibilityAt`), mit einer
  konstanten und einer blob-gestützten Implementierung, damit `fb-gym` ohne Netz läuft.
* JSBSim-Verdrahtung über `FGWinds` (`fdm/FBFdm`) — der Adapter hat heute keinen Windkanal.
* Wolken-Stages: `TCDC`/`LCDC`/`MCDC`/`HCDC` als Bedeckungs-Modulation, `CLOUD_CEIL` als
  Basishöhe der Deckschicht.
* Die Fixture oben als feste Gym-Wetterlage.

Server-seitig offen und bewusst so gelassen: nur der **Analyseschritt f000**, keine Vorhersage
(`fXXX`) — eine Sitzung länger als der Lauf sieht dieselbe Atmosphäre, bis nginx den nächsten Lauf
zieht. Die Struktur trägt es (`valid_epoch` ist ein eigenes Header-Feld, `parse_product` rechnet
Vorhersagezeiten bereits aus), es gibt nur noch keinen Konsumenten, der eine Zeitachse bräuchte.

---

## Offene Punkte

1. **`payerne-full` stürzt unter `--elev tiles` ab.** Die Mission fliegt mit `--elev const`/`swiss`
   sauber bis Exit 0; gegen die Live-DEM nicht. Ursache nicht in dieser Runde untersucht.
   Verdachtsflächen, die dieser Text nur benennen kann: der Auflösungsunterschied (`/elev` samplet
   z13-Terrarium bilinear, während `FBBakedDemElevation` ein 90-m-Raster ist), die
   Native-Cache-Zelle von ≈ 33 m (eine Landung tastet die Bahnhöhe damit gequantelt ab) und der
   Kaltstart-Pfad (503 → der letzte gültige Wert bleibt stehen). **Solange das offen ist, ist der
   Missions-Regelkreis faktisch an `const`/`swiss` gebunden und die Live-DEM ungetestet.**
2. **`FBUnitsStage` ist NoOp** (s. `rendering.md`): `FBWorld` **borgt** die Unit-Registry bereits für
   die Zeichenseite, aber es gibt keinen Konsumenten. Andere Einheiten, Waffen und Bodenziele sind
   im Bild unsichtbar.
3. **Terrain-Maskierung für Sensoren fehlt.** Der `const FBWorld*` wird bis in die Modul-`Run()`
   durchgereicht, damit ein Sensor Sichtlinien gegen Gelände prüfen KANN — heute prüft keiner.
   `systems/FBRadarSystem` dokumentiert das ausdrücklich als bewusste Auslassung (ein DEM-Raymarch je
   Kontakt je Look). Der Hook existiert, die Rechnung nicht.
4. **`fb_stream_ground` liefert einen Punkt, kein Feld.** `FBElevationProvider` deklariert bereits
   `GroundElevPatch` (Flächenabfrage) für künftiges Terrain-Sampling; `FBTilesElevation`
   implementiert nur `GroundElevM`. Geländefolge-Flug, Radarhöhen-Vorausschau und CFIT-Prognose haben
   damit keine Quelle.
5. **Der statische Ladepfad (`fb_terrain_load`, `FB_TERRAIN_MAX_TILES` = 64) ist Bring-up-Erbe.**
   Beide Clients fahren Streaming; der statische Pfad existiert weiter (inkl. seiner eigenen
   Codehälfte in `FBTilesStage`) und wird nirgends regelmäßig gefahren.
6. **Der DEM-Cache ist pro Worker-Instanz.** Bei N = 6 Workern wird dieselbe Nachbarkachel bis zu
   sechsmal geholt und dekodiert. Als „minor cross-instance cache redundancy accepted" im Code
   vermerkt — nicht gemessen, wie teuer es beim Kaltstart wirklich ist.
7. **Eviction ist rein zeitbasiert** (`kGrace` = 180 Passes), nicht speicherbasiert: es gibt keinen
   VRAM- oder Knoten-Druck-Auslöser außer dem harten Deckel `kNodeCeil` = 6000, der beim Erreichen
   schlicht **jeden weiteren Split verweigert**. Was passiert, wenn ein sehr langer Flug diesen
   Deckel erreicht, ist nicht dokumentiert und offenbar nicht gemessen.
8. **Der Nachtlicht-Deckel (65 536 Sprites) ist eine gesetzte Zahl** („team-lead cap"), keine aus
   einer Messung abgeleitete. Ebenso die Klassen-LUTs für Farbe/Radius/Helligkeit — ausdrücklich
   „cosmetic LUT".
9. **Zwei Bakes-Modi, ein einziger sichtbarer Umschalter.** SVS (OSM) und EVS (Photo) haben
   unterschiedliche Beleuchtungssemantik (`rendering.md` §4): SVS pinnt Tag = 1 und schaltet
   Sterne/Lichter/Wolken ab. Der Umschalter ist heute die TAB-Taste im Browser; `gpu_native` hat
   `--albedo osm|photo`. Es gibt keine Missionsdaten-Ebene dafür — eine `.fbm` kann den Bildmodus
   nicht deklarieren.
10. **TLS ist im Tile-Server nicht verdrahtet** (`nginx.conf`, ausdrücklich als dokumentierte Lücke
    vermerkt: `FB_DOMAIN` existiert als Env-Haken, es gibt aber keinen `listen 443 ssl;`-Block und
    kein ACME). Für zentrales Hosting ist das ein eigenes Stück Arbeit, kein Flag.
