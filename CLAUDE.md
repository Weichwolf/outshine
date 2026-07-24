# FlightBox — Architektur

Ein **JSBSim-gestützter F-16-Simulator auf DCS-World-Niveau** — eine **WASM-App** mit **Tileserver-
Backend, das die ganze Welt abbildet**. Die Physik ist **JSBSim**; FlightBox ist die Welt drumherum —
globales Gelände, Renderer, HUD, Steuerung. **Fokus: eine akkurate F-16-Simulation.** Genau zwei
Qualitätsachsen zählen: **korrektes Rendering** und **realistisches F-16-Flugverhalten**.

Der Aircraft-Plugin-Mechanismus (unten) bleibt der saubere Weg, ein JSBSim-Modell einzuhängen — aber das
Produkt ist die F-16; andere Modelle sind Nebensache, nicht das Ziel.

```
fb-tiles (Server: weltweit DEM/OSM/Luftbild)  ──HTTP──▶  Command Center (Client)
                                                          = JSBSim + FBW + Autopilot (LOWLEVEL|Loiter)
                                                            + WebGPU-ECEF-Renderer + HUD
                                                            als EIN Prozess (WASM-Browser | native CLI)
```

**libJSBSim kompiliert DIREKT ins Command Center** — WASM (Browser, via Emscripten) und native CLI.
Physik, Regelung und Rendering teilen **einen** Adressraum: die Kamera liest den JSBSim-Zustand
direkt. **Kein iNav, kein Wire-Protokoll, kein separater World-Prozess, kein Hub.** Server-seitig
läuft **nur** `fb-tiles`. Nichts ist vorgeladen — jede Kachel on-demand → **jeder Punkt der Erde ist
ein gültiger Start**.

## Prinzipien (nicht verhandelbar)

1. **Physik nicht neu schreiben.** JSBSim (LGPL, NASA/FlightGear-erprobt) ist die Wahrheit. Eigener
   Code nur an den Nähten: der C-aufrufbare Adapter (`sim/src/fdm/jsbsim_adapter.*`), die Regelung,
   der Renderer. **JSBSim ist ein gepinntes, read-only Git-Submodul** (`sim/vendor/jsbsim`) — nie gepatcht;
   der Build baut libJSBSim aus dem Submodul, so ist die Physik bit-identisch zum gepinnten Commit
   und update-fähig.
2. **JSBSim läuft IM Client.** libJSBSim linkt in die CC — WASM via Emscripten, native fürs CLI. Die
   Modell-XML + Engine-/Tabellendaten reisen in Emscriptens virtuellem FS (embed/preload). Keine
   Telemetrie-Grenze zwischen Physik und Bild — sie sind derselbe Prozess. Gäbe es einen Wire
   dazwischen, wäre es die alte Architektur; der Sinn des Pivots ist, ihn zu streichen.
3. **Server-seitig nur zwei schlanke, unabhängige Container:** `fb-tiles` (`tiles/`, :8081, reine
   Tile-API — weltweit DEM/OSM/Luftbild) und `fb-sim` (`sim/`, :8080, reiner Web-Host: serviert die
   WASM-App + env-config; `sim/up.sh`). **Alles andere ist Client** — Physik, FBW, Autopilot, Renderer,
   HUD. Kein SITL, kein World-Prozess, kein Hub.
4. **Sim läuft so schnell wie sinnvoll.** Die Sim-Mathematik ist deterministisch; Wall-Clock-Tempo
   ändert das Ergebnis nicht. Live-Flug im Browser = Echtzeit; Batch/Screenshot/Headless = so schnell
   die Maschine kann. Gibt das Tempo das Ergebnis, ist die Kopplung nicht-deterministisch = ein Bug.
5. **F-16 zuerst, full-scale, vanilla JSBSim-Modell.** Die F-16 ist die **vanilla JSBSim-F-16**
   (`jsbsim/aircraft/f16`, full-scale, echte FLCS) — **Referenz ist das MODELL selbst**, nicht der
   absolute echte Jet. Das Modell ist gepinnt/vanilla; seine validierte
   Charakteristik ist die Wahrheit (z.B. Rollrate ~190 °/s = Modell-Eigenschaft, akzeptiert). FlightBox
   muss das Modell **treu fliegen** — der FBW/Loiter kommandiert die echte FLCS, verzerrt sie nicht; kein
   künstliches Departure aus Spawn/Trim, keine Divergenz. Bewertet werden **korrekte Integration +
   Rendering**, nicht Modell-vs-echter-Jet. Flugzeuge sind **Plugins**; die F-16 ist das Produkt.

## Steuerung — Fly-by-Wire + Autopilot

Ein **schlanker Fly-by-Wire-Layer** stabilisiert die Fluglage (Raten-/Attitude-PID auf die
JSBSim-`fcs/*-cmd-norm`-Eingänge; die F-16 hat eine eigene FLCS → `fcs/fbw-override=1` überbrückt sie,
direktes Ruder). Darüber der Autopilot — generische Guidance in `systems/FBAutopilot`, Verhalten
modul-überschreibbar:

| Modus | Mechanismus |
|---|---|
| `manual` | direkter Stick (Gamepad/Tastatur) durch den FBW |
| `lowlevel` (Boot-Default) | 450 kt @ 500 ft AGL: reaktiver Terrain-Fächer wählt das Tal, Wings-Level-Disziplin, Fence um den Spawn; optionaler A*-Wanderplaner (`?plan=1`) — `doc/lowlevel.md` |
| `loiter(lat, lon, alt, radius)` (`?ap=loiter`) | Bank-to-Circle: halte einen Kreis um das Zentrum in Zielhöhe/-radius |

Der Loiter-AP ist die **Kamera-Plattform** für weltweite Screenshots; LOWLEVEL ist der Missions-Kern.
Weitere Modi (goto/route, hold, RTH, Anflug/Landung) folgen — Richtung DCS-artige Missionen.

## Flugzeug = Modul + JSBSim-Modell

Ein steuerbares Flugzeug besteht aus zwei Teilen: dem **Code-Modul** (`FBModule`-Ableitung unter
`sim/src/modules/<name>/` — Systeme, Presets, Displays) und dem **JSBSim-Modell** (Verzeichnis mit
Aero/Masse/Antrieb + Engine-Daten). Die F-16 fliegt das vanilla Modell aus dem gepinnten Submodul
(`sim/vendor/jsbsim/aircraft/f16`); im WASM-Build reist es in Emscriptens virtuellem FS.

- **F-16-Kante:** die JSBSim-F-16 ist eine echte FLCS (`*-cmd-norm` = Raten-Sollwerte). FBW-Override
  überbrückt sie, sonst zwei genestete Rate-Loops.
- Aircraft-XML trägt EIGENE Lizenz (F-16 = GPL; die meisten LGPL) — Attribution per Datei.

## Sprache & Struktur

**C++ (C++17, wie JSBSim), nicht C.** Ordentliche Klassen nach C++-Best-Practice — RAII, klare
Ownership, minimale public API.

**Verzeichnisstruktur** (`sim/src/`) bildet eine DCS-World-artige Modul-Architektur ab: der Simulator
lädt beliebig viele **steuerbare Module** (heute die F-16; künftig Ka-52, F-18, …) zur LAUFZEIT — die
App hält jedes Modul polymorph hinter `FBModule*` (die Auswahl/der Dispatch ist der Mechanismus, nicht
eine Abkürzung zur einen heutigen F-16-Instanz). Jedes Modul trägt dieselben Systemkategorien (ein
reales F-16-Inventar: Steuerung/HOTAS, Flight Control, Antrieb, Anzeigen, Sensoren, Waffen, Defensiv,
Comms/Datalink, plus Guidance/Pfadplanung — `doc/f16/` INDEX.md), unterscheidet sich aber im VERHALTEN,
nicht nur in Zahlen. Deshalb: **FBCore → Interface → Default-Implementation → modul-spezifischer
Override**, nicht "gehört der F-16":

```
sim/src/
  app/       Einstiegspunkte + App-Lifecycle (FBAppWasm, FBAppNative, FBSimHost, FBTileWorkerMain)
  core/      FBState, FBMode, FBMasterMode, FBTelemetry, gemeinsame Basistypen — zeigt NIE nach
             systems/ oder modules/
  math/      Value-Math (FBMat4 — Value-Types, Operatoren inline im Header)
  render/    FBRenderer (Orchestrator: Device/Swapchain/Targets, JEDE Begin/EndRenderPass-Grenze,
             die Encode-Reihenfolge — Pass-Topologie ist ein Vertrag, kein Stage-Split darf sie
             vermehren), FBCamera, FBMips, FBChunkMesh/FBChunkVtx, FBEphemeris, FBGpu/FBFrameContext
             (Device-Handle bzw. geteilter Frame-Zustand, den jede Stage bekommt), FBDrawStage
             (Interface: ein Shader + seine Pipeline(s)/Bind-Group(s)/Draws, zeichnet in den
             GELIEHENEN Encoder — nie eigene Pass-Grenzen)
  render/stages/  Klasse pro Shader: FBTransmittanceStage/FBSkyViewStage/FBSkyStage (Hillaire-
             Atmosphäre + die Wolkendecke-Value-Noise-Sheet auf der Dome), FBSunStage/FBMoonStage
             (Sonnenscheibe+Glow bzw. Mond-als-belichtete-Kugel — additive Draws, direkt nach FBSkyStage
             in derselben Scene-Pass, gleiche Blend-Reihenfolge wie der frühere Ein-Shader-Composite;
             FBMoonStage besitzt die NASA-LROC-Albedo-Textur als alleinige Konsumentin), FBTilesStage
             (Terrain: Albedo-Array, RenderBundle/2-Phasen-Streaming-Zustand, Invarianten-Zähler),
             FBStarsStage, FBTileLightsStage, FBHudStage (WebGPU-Backend; die Symbologie-LOGIK bleibt
             vorerst in FBHud.h/FBHudSymbology.h, s.o.), FBUpscaleStage, FBUnitsStage/FBSpritesStage
             (NoOp, aber in der Encode-Ordnung verdrahtet: Units nach Terrain, Sprites vor HUD); Wolken
             als 6 Klassen (FBCloudMipDownStage teilt den Box-Downsample-Helfer, FBCloudBaseBakeStage/
             FBCloudDetailBakeStage/FBCloudCellBakeStage backen die 3 Noise-Volumes je einmal,
             FBCloudMarchStage marcht die WGS84-Kugelschale ins Viertel-Res-Ziel, FBCloudResolveStage
             löst temporal auf); FBTonemapStage (EIN Shader-Quellcode, zwei Pipelines — mit/ohne
             Wolken-Composite, analog HUD Solid/Line). FBRenderer.cpp führt keinen Inline-Shader mehr
             (jedes `R"(`-WGSL lebt in genau einer stages/-Datei, `grep -c 'R"(' FBRenderer.cpp` == 0)
             — der Render-Stage-Split ist damit abgeschlossen.
  world/     FBWorld, FBTerrainField, FBTerrainLoader (Tile-Streaming/Worker-Anbindung)
  systems/   die generischen, airframe-agnostischen System-Slots eines Moduls — Interface + Default
             in EINER Klasse, ein Modul überschreibt per Ableitung (Zahlen-Tuning bleibt Preset/
             Config, keine leere Ableitung dafür):
               FBAutopilot (Guidance), FBFlightControl (FBW-Innenschleife), FBPathPlan (Wanderplaner)
               — die drei heute REAL implementierten Systeme, `Run()`/`Update()` virtuell;
               FBSystemSlots.h — Input/HOTAS, Propulsion, Displays, Sensors, Weapons, Defensive, Comms:
               Interface + NoOp-Default für die restlichen F-16-Systemkategorien, ein Modul füllt sie
               bei Bedarf per Ableitung. Sensoren SCHREIBEN/Displays LESEN den geteilten FBState;
               Sensoren/Waffen/Defensiv erhalten eine geborgte FBWorld-Referenz (nie global).
  terrain/   leane Terrain-Lib (geo/mesh/osmmesh), flat
  fdm/       der C-aufrufbare JSBSim-Adapter (jsbsim_adapter.*), flat
  modules/   FBModule-Basisschnittstelle (`Run(fb_fdm_state&, dt, const FBWorld*)`, App hält jedes
             Modul dahinter polymorph; ein Modul cycelt seine Systeme intern, jedes im eigenen Takt —
             Peers rufen sich nie gegenseitig)
  modules/f16/           das F-16-Modul: FBF16Module komponiert die systems/-DEFAULTS (FBAutopilot/
                         FBFlightControl unverändert) mit dem F-16-Gain-Preset
                         (FBFlightControl::F16()), besitzt den optionalen FBPathPlan und cycelt alle
                         Systemslots — der Ort, an dem künftiges F-16-spezifisches Verhalten
                         (echtes Radar, echtes HOTAS-Binding, …) als Ableitung eingehängt wird
  modules/f16/displays/  HUD (MIL-STD-1787): FBHud (WebGPU-Backend-Shim), FBHudSymbology (Geometrie),
                         FBMax7456 (Font-Atlas-Daten) — das bestehende Rendering, noch nicht über
                         den generischen Displays-Systemslot geführt
```

Ein zukünftiges `units/` mit einer `FBUnit`-Basisschnittstelle für nicht steuerbare KI-Einheiten
(freundlich/feindlich/neutral) entsteht erst, sobald reale Einheiten existieren — keine leeren
Gerüst-Ordner ohne Inhalt.

**Coding-Style: an JSBSim orientiert** (unser Code fügt sich in dessen Ökosystem): Klassen `FB`-Präfix
(analog `FG` — `FBFlightControl`, `FBRenderer`), PascalCase-Methoden (`Run()`, `GetLoadFactor()`),
Member PascalCase, ein `namespace FlightBox`, Header-Guards, Klasse-pro-Datei `FBName.h/.cpp`,
Getter inline im Header. JSBSims LGPL-Banner nicht kopieren — unsere Dateien tragen unsere Lizenz.

## Engineering-Konventionen

- **Build nur über Make-Targets** — jedes Projekt trägt sein eigenes Makefile: `sim/` baut die CC
  (`make -C sim wasm` | `worker` | `native` | `image` | `up`), `tiles/` den Tile-Server
  (`make -C tiles build` | `image` | `run`). Rezepte leben im Makefile, nicht in Agenten-Köpfen.
- **`extern "C"` für jede von JS namentlich gerufene Funktion** (EMSCRIPTEN_KEEPALIVE reicht nicht —
  Mangling bricht Exporte still).
- **Frame-Beweis-Pflicht:** build-wirksame Änderungen gelten erst mit gerendertem Frame oder
  numerischer Messung als verifiziert.
- **Fidelity-Baseline + Mess-Konventionen:** `doc/fidelity-baseline.md` (Hash-Lock, [agl]-Log,
  Bare-Model-Vergleich, akzeptierte Modell-Eigenschaften). Ziel-GPU-Fähigkeiten:
  `doc/webgl-webgpu-report.txt`.
- JSBSim (`sim/vendor/jsbsim`) und das f16-Modell sind read-only; Warnings = Errors.

## Rendering (das Herzstück)

**API: WebGPU** — ein Renderer-Quelltext (`sim/src/render/`, WGSL inline), zwei Link-Ziele:
WASM via emdawnwebgpu (Browser) und natives Dawn (`gpu_native`, das Headless-PNG-Orakel für
Frame-Beweise). Natives [0,1]-Clip → volle Reversed-Z-Präzision; Compute für Atmosphären-LUTs
(Hillaire); HDR + ACES-Tonemap; RenderBundle-Submission (Re-Record nur bei Strukturänderung).
Feature-Gates sind gebackene Konstanten (env-getriebener String-Replace am Shader-Build) —
tote Pässe kosten nichts.

Globaler Standard: **WGS84-ECEF, camera-relative**; Horizont-Dip aus der Krümmung.
**Reversed-Z-Depth-Buffer** (32F) gegen z-Fighting fernen Geländes (near 0.01 m / far 240 km). osmmesh
liefert Terrain in per-Tile-ECEF (unser Code, nicht vendored). Reale Daten on-demand von `fb-tiles`
(OSM/Copernicus-DEM/Luftbild) — jeder Punkt der Erde gültig. HUD = MIL-STD-1787 (MAX7456-OSD-Font).
Kachel-Streaming ist **kamera-priorisiert** (nächste zuerst).

**Bodenwahrheit aus der Modell-Geometrie.** Die Augenhöhe am Boden kommt aus JSBSims Fahrwerks-Geometrie
(`fb_jsbsim_ground_clearance`, gear-down/up), nicht aus einer fixen Zahl — essenziell für Start/Aufsetzen/
Crash-Erkennung. Die Kamera geht **nie unter die Oberfläche** (Clamp auf Grund + Modell-Bauch-Clearance).
**Crash → Motor aus**, den Rest macht JSBSims Ground-Reactions, kein Freeze.

## temp/ — Migrationsgut

`temp/` hält das Material der Vor-Architektur (Validator, TS-Testsystem, skalierte Aircraft-Modelle,
Original-osmmesh/geo). Read-only-Steinbruch, keine lebende Architektur — Teile entfallen ersatzlos,
sobald ihr Nachfolger im `sim/`- oder `tiles/`-Baum steht.
