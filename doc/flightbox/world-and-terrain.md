# Welt, Gelände und Tile-Streaming

**Quellen dieser Datei:** `sim/src/world/` (`FBWorld.h/.cpp`, `FBTerrainLoader.h/.cpp`,
`FBTilesElevation.h`), `sim/src/terrain/` (`geo.h/.cpp`, `geo_ecef.cpp`, `mesh.h`, `terrain.h/.cpp`,
`osmmesh.h`, `osmmesh_terrain.cpp`), `sim/src/render/FBChunkMesh.h` + `FBChunkVtx.h` (die
Mesh-Endstufe), `sim/src/app/FBTileWorkerMain.cpp` + `sim/web/fbtw-worker.js`, `sim/Makefile`
(Targets `wasm`/`worker`) sowie `tiles/` (Server: `Makefile`, `nginx.conf`, `src/*`) — der Server ist
hier **aus Klientensicht** dokumentiert und wurde nicht verändert. Dazu CLAUDE.mds Abschnitte
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

- ENU ist eine **flache Tangentialebene**, keine ECEF-Rundreise. Für den ~20 km-ROI ist der Fehler
  sub-Millimeter (Herleitung in `geo.cpp`). Für globales Rendern reicht das **nicht** — deshalb
  reprojiziert die Endstufe (§5.2) jeden Knoten exakt.
- Web-Mercator-Breite ist bei ± 85,05112878° gekappt; `osmmesh_geo_to_tile` weist alles darüber ab.
- MVT-`local_y` hat den Ursprung **oben links** (0 = Nordkante) — entgegengesetzt zur ENU-N-Achse;
  der Fast-Path `tile_enu_map` behandelt das Vorzeichen explizit.

### 5.1 Terrarium und das Stitching

Höhendekodierung (Mapzen/AWS-Terrarium-Spezifikation, jedes Pixel 24 bit RGB):

```
h = R·256 + G + B/256 − 32768   [m]
```

Gitterorientierung: Zeile 0 = **Nord**, Spalte 0 = **West** (PNG-Layout und Slippy-Konvention).
`build_mesh` platziert Vertex `(r,c)` bei `(c·dx, r·dy)`; weil `map->scale_n` bereits negativ ist
(ENU-N wächst nach Norden, `r` nach Süden), kommen die Positionen ohne weiteren Vorzeichenwechsel
ENU-korrekt heraus.

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
