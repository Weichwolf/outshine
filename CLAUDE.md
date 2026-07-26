# FlightBox — Architektur

Ein **JSBSim-gestützter F-16-Simulator auf DCS-World-Niveau** — eine **WASM-App** mit **Tileserver-
Backend, das die ganze Welt abbildet**. Die Physik ist **JSBSim**; FlightBox ist die Welt drumherum —
globales Gelände, Renderer, HUD, Steuerung. **Fokus: eine akkurate F-16-Simulation.** Genau zwei
Qualitätsachsen zählen: **korrektes Rendering** und **realistisches F-16-Flugverhalten**.

Der Aircraft-Plugin-Mechanismus (unten) bleibt der saubere Weg, ein JSBSim-Modell einzuhängen — aber das
Produkt ist die F-16; andere Modelle sind Nebensache, nicht das Ziel.

```
fb-tiles (Server: weltweit DEM/OSM/Luftbild)  ──HTTP──▶  Command Center (Client)
                                                          = JSBSim + FBW + Autopilot (Pilot/Mission|Manual)
                                                            + WebGPU-ECEF-Renderer + HUD
                                                            als EIN Prozess (WASM-Browser | native CLI)
```

FlightBox Core (JSBSim-Sim + Module + Missionen + KI) ist eine **reine Bibliothek** (`sim/`s
`core-lib`-Target, `build/libfbcore.a`); drei **Clients** linken dagegen: **gym** (headless, Mission
rein → Telemetrie raus, KEIN Renderer/GPU), **native** (der Referenz-Renderer/Frame-Orakel — Dawn,
`--mission --interval`, Screenshot-Modi) und **wasm** (der Browser). Details: `## Core-Lib + Clients`
unten.

**libJSBSim kompiliert DIREKT ins Command Center** — WASM (Browser, via Emscripten) und native CLI.
Physik, Regelung und Rendering teilen **einen** Adressraum: die Kamera liest den JSBSim-Zustand
direkt. **Kein iNav, kein Wire-Protokoll, kein separater World-Prozess, kein Hub.** Server-seitig
läuft **nur** `fb-tiles`. Nichts ist vorgeladen — jede Kachel on-demand → **jeder Punkt der Erde ist
ein gültiger Start**.

## Prinzipien (nicht verhandelbar)

1. **Physik nicht neu schreiben.** JSBSim (LGPL, NASA/FlightGear-erprobt) ist die Wahrheit. Eigener
   Code nur an den Nähten: der FDM-Adapter (`sim/src/fdm/FBFdm.*`, die EINE Übersetzungseinheit mit
   JSBSim-Headern — Details `sim/src/fdm/`-Absatz unten), die Regelung, der Renderer. **JSBSim ist ein
   gepinntes, read-only Git-Submodul** (`sim/vendor/jsbsim`) — nie gepatcht;
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
   muss das Modell **treu fliegen** — der FBW/Autopilot kommandiert die echte FLCS, verzerrt sie nicht; kein
   künstliches Departure aus Spawn/Trim, keine Divergenz. Bewertet werden **korrekte Integration +
   Rendering**, nicht Modell-vs-echter-Jet. Flugzeuge sind **Plugins**; die F-16 ist das Produkt.

## Steuerung — Fly-by-Wire + Autopilot

Ein **schlanker Fly-by-Wire-Layer** stabilisiert die Fluglage (Raten-/Attitude-PID auf die
JSBSim-`fcs/*-cmd-norm`-Eingänge; die F-16 hat eine eigene FLCS → `fcs/fbw-override=1` überbrückt sie,
direktes Ruder). Darüber der Autopilot — generische Guidance in `systems/FBAutopilot`, Verhalten
modul-überschreibbar:

| Modus | Mechanismus |
|---|---|
| `manual` (`?ap=manual`) | direkter Stick (Gamepad/Tastatur) durch den FBW |
| `pilot` (Boot-Default) | FBF16Pilot (`systems/FBPilot`, F-16-Override `modules/f16/FBF16Pilot`) fliegt eine `.fbm`-Mission als Phasenmaschine, deren START-Phase die deklarative `spawn`-Zeile bestimmt (Boden: Preflight → Takeoff → Climb → Route → …; Luftstart: direkt Route — kein Taxi/Rotate zu simulieren), kommandiert je Phase `FBAutopilot::Direct` (Punkt-zu-Punkt Kurs/Höhe/Speed) — `doc/mission-format.md` |

`FBAutopilot::Direct` ist das interne Guidance-Primitiv, das der Pilot je Phase kommandiert — keine
eigene Flugmodus-Wahl, nur die Ausführung dessen, was der Pilot verlangt. Die Pilot-Phasen + ihr
Regelkreis (FBPilot → FBAutopilot::Direct → FBFlightControl → JSBSim, 10 Hz Entscheidung über 100 Hz
FDM-Substeps) sind der **Missions-Kern**; `manual` bleibt als direkter Stick-Sandkasten (noch ohne
gebundenes HOTAS — `FBInputSystem` ist weiterhin der NoOp-Default). Weitere Phasen (Anflug/Landung)
folgen — Richtung DCS-artige Missionen.

## Flugzeug = Modul + JSBSim-Modell

Ein steuerbares Flugzeug besteht aus zwei Teilen: dem **Code-Modul** (`FBModule`-Ableitung unter
`sim/src/modules/<name>/` — Systeme, Presets, Displays) und dem **JSBSim-Modell** (Verzeichnis mit
Aero/Masse/Antrieb + Engine-Daten). Die F-16 fliegt das vanilla Modell aus dem gepinnten Submodul
(`sim/vendor/jsbsim/aircraft/f16`); im WASM-Build reist es in Emscriptens virtuellem FS.

- **F-16-Kante:** die JSBSim-F-16 ist eine echte FLCS (`*-cmd-norm` = Raten-Sollwerte). FBW-Override
  überbrückt sie, sonst zwei genestete Rate-Loops.
- Aircraft-XML trägt EIGENE Lizenz (F-16 = GPL; die meisten LGPL) — Attribution per Datei.

## Core-Lib + Clients (gym/native/wasm)

**Lib/Client-Split:** `sim/`s `core-lib`-Target baut den Simulator selbst — `core/` + `fdm/` +
`systems/` + `modules/` (inkl. F-16) + `units/` + den `.fbm`-Parser + libJSBSim, EINMAL, als natives
Archiv `build/libfbcore.a` — zu **einer** Bibliothek. `render/` und das `world/`-Tile-Streaming gehören
NICHT dazu (eine Ausnahme: `render/FBHudGeometry.cpp`, reine CPU-Vertexlisten-Mathematik ohne WebGPU/
Dawn-Include, die `systems/FBDisplaySystem` strukturell braucht). WASM ist ein ANDERES Toolchain-Target
(emcc/wasm32) und kompiliert dieselbe Quelldatei-Liste zwangsläufig selbst nach (Cross-Compile, keine
Duplikation der Architektur).

Drei Clients linken/kompilieren dagegen:

- **`fb-gym`** (`app/FBAppGym.cpp` + `app/FBMissionRunner.cpp`, `make -C sim gym` →
  `build/fb-gym`) — headless: `--mission FILE [--out DIR] [--timeout N] [--elev tiles|const|swiss]`.
  KEIN Dawn-/wgpu-Symbol im Binary (verifiziert per `nm`). Der Missions-Kern (Prinzip 4: so schnell wie
  die Maschine kann).
- **`gpu_native`** (`app/FBAppNative.cpp`, `make -C sim native`) — der Referenz-Renderer/Frame-Orakel:
  `--mission --interval` (PNG-Beweisframes on top of derselben `FBRunMission`-Schleife, über einen
  GPU-freien `FBMissionTickHook`, den nur `FBAppNative.cpp` mit einem echten `FBRenderer`/`FBWorld`
  implementiert) + der No-Module-Screenshot-Modus. `--mission` OHNE `--interval` bleibt headless (`wie
  bisher`, Regressions-Kennzahlen unverändert).
- **wasm** (`app/FBAppWasm.cpp`, `make -C sim wasm`) — der Browser, Verhalten unverändert.

**Ausblick Multi-Unit (beschlossen, noch nicht gebaut):** Missionen beschreiben künftig einen VERBAND
mehrerer Einheiten/Module verschiedener Fraktionen (je Unit: Modul, Fraktion, Flightplan/Ziele — z.B.
F-16 vs. MiG-29 im Gym; evolutionäre Piloten-Turniere über Telemetrie-Fitness). Dafür festgelegt:
**Multi-Threading ist ein reines GYM-Feature** — Thread pro Unit mit EIGENER JSBSim-Instanz, Lockstep-
Barrier pro Tick, Cross-Unit-Lesezugriffe (Sensoren/Waffen) nur auf den SNAPSHOT des letzten Ticks →
bit-reproduzierbar (Prinzip 4). native/wasm bleiben Single-Thread im Sim-Loop (Echtzeit braucht keine
Parallelphysik; der Browser erspart sich den pthreads/SharedArrayBuffer-Build). **Etappe 1 ist GEBAUT:**
der fdm/-Adapter ist instanzfähig — `FBFdm` ist ein Objekt pro Airframe (keine globale Instanz, keine
statischen mutablen Globals mehr), mehrere koexistieren im selben Prozess mit unabhängiger Physik
(Beweis: `make -C sim test-fdm` → `build/fb-test-two-fdm`, drei gleichzeitige F-16-Instanzen, eigene
IC/Boden/Tankfüllung, gegenläufig gesteuert; die Zwillings-Instanz reproduziert die erste bit-genau).
Noch offen: Thread-pro-Unit + Barrier, Unit-Listen, und Telemetrie-/Log-Sinks per-Thread gepuffert und
am Barrier gemerged.

**Der Missions-Runner ist reiner Orchestrator, genau vier Schritte, KEIN Missions-Wissen im Code:**
Mission laden → Welt mit ihren Akteuren aufsetzen (Elevation für die deklarative `spawn`-Zeile auflösen,
Modul über `modules/FBModuleRegistry` spawnen) → Akteure ausführen (Modul takten, beide Monitore
füttern) → Welt validieren (die Monitore haben's längst entschieden). Eine `.fbm`-Zeile `module <name>`
(Pflichtfeld, `doc/mission-format.md`) wählt das Modul über die Registry (Name → Factory,
`std::unique_ptr<FBModule>`; heute registriert nur `modules/f16/FBF16ModuleRegistration.cpp` den Namen
`"f16"`). `FBMissionRunner.cpp`/`FBAppGym.cpp` inkludieren NIE einen konkreten Modul-Header — sie halten
alles über `FBModule`s generische Accessoren (`Autopilot()`/`FlightControl()`/`PilotSystem()`/
`Controls()`/`Displays()`/`AirDataSystem()`/`FlightPlan()`/`Telemetry()`/`SetRunway()`/
`SetGroundAsl()`/`ApplySetup(key,value)`, auf der Basisklasse selbst deklariert — CLAUDE.md's "FBCore →
Interface → Default → Override" jetzt auch für den Modul-ZUGRIFF, nicht nur dessen Verhalten). Der
Anfangszustand einer Unit ist reine Daten-Deklaration (`FBSpawn`: Position, Höhe-ODER-Boden, Kurs,
Speed) — KEINE getrennten Boden-/Luft-Codepfade, eine EINZIGE IC-Anwendung
(`FBMissionBoot.h::FBMissionApplySpawn`); `set <key> <value>`-Zeilen tragen Systemzustand als
Missionsdaten, der Runner reicht die rohe KV-Liste nur durch, das MODUL interpretiert seine eigenen
Schlüssel (`FBModule::ApplySetup`, unbekannter Schlüssel = Laufzeit-FAIL). Wegpunkt-Sequenzierung ist
Akteurs-Verhalten, nicht Runner-Buchhaltung: `systems/FBNavSystem::AdvanceWaypoint` sitzt im Modul.
Eine `.fbm`-Datei beschreibt heute genau EIN Modul — eine Eigenschaft des Runners, keine Grenze des
Datenmodells: eine künftige Mehr-Einheiten-Mission (ein Verband aus mehreren Modulen/Fraktionen) ist
`FBMission` mit einer LISTE solcher Pro-Einheit-Blöcke, keine Neukonstruktion.

**Elevation-Hook:** jeder Core-Konsument von Bodenhöhe (Missions-Ground-Spawn, AGL/Radar-Alt,
Crash-Erkennung) läuft über `FBElevationProvider` (`core/FBElevationProvider.h`,
`GroundElevM(lat,lon)` + eine `GroundElevPatch`-Flächenabfrage fürs künftige Terrain-Sampling), nie über
einen hartverdrahteten fb-tiles-Wire. Drei Implementierungen:

- `FBConstantElevation` (`core/`) — das primitive Fundament, eine feste Höhe überall.
- `FBRunwayPlateauElevation` (`core/`, Gym-Default `--elev const`) — eine Mission kann MEHRERE Runways
  bei unterschiedlicher Höhe haben (Start + künftiges `dest_runway`); dieser Provider hält alle
  Runways der Mission und liefert je Punkt: innerhalb Footprint+~5 km Marge die Runway-eigene Höhe,
  dann ~10 km Smoothstep-Falloff auf eine flache 0-m-Basis (überlappende Plateaus folgen der
  NÄHEREN Runway — die einfachste stetige Wahl). Kein Netz, kein Daten-Asset nötig.
- `FBTilesElevation` (`world/FBTilesElevation.h`, native+WASM) — dünner Pass-through auf das
  bestehende `fb_stream_ground`/`fb_stream_open` (unverändert, gleiche Zahlen).
- `FBBakedDemElevation` (`core/`, Gym `--elev swiss`) — lädt ein eingebackenes DEM-Raster
  (`sim/assets/swiss-dem-90m.bin`, `sim/tools/bake_swiss_dem.py` — Insel-Bbox 5.96–10.49°E/
  45.82–47.81°N, 90 m, ~18,9 MB, außerhalb/am Rand [~15 km Blend] 0 m), bilinear; außerhalb der Box
  0 m. Gym-Default: `swiss` wenn das Asset vorhanden ist, sonst `const` — ein bare `fb-gym --mission
  FILE` läuft immer, Netz oder nicht.

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
  app/       Einstiegspunkte + App-Lifecycle: FBAppWasm, FBAppNative (native, s.o.), FBAppGym (der
             Gym-Client, s.o.), FBSimHost, FBTileWorkerMain; FBMissionRunner.h/.cpp (der geteilte,
             GPU-freie Missions-ORCHESTRATOR, vier Schritte, keine Missions-Spezifika — FBRunMission,
             FBMissionTickHook — den FBAppNative UND FBAppGym treiben, s.o.), FBMissionBoot.h
             (`FBMissionApplySpawn`: die EINE deklarative IC-Anwendung, Boden ODER Luft, generisch
             über FBModule&, kein konkreter Modultyp), FBLogSinks/FBTelemetrySinks (die
             I/O-Sink-Implementierungen)
  core/      FBState, FBMode, FBMasterMode, FBFlightPlan/FBRunway/FBSpawn (Wegpunkt-/Runway-/
             deklarativer-Spawn-Value-Types für FBPilot/den Orchestrator), FBMissionFile
             (`.fbm`-Parser, trägt `ModuleName`/`FBSpawn`/`SetKV`), FBMissionMonitor
             (FBFlightMonitor-Geschwister: das MISSIONS-Urteil — Wegpunkte/Off-Runway/Timeout — aus
             der eigenen, unveränderlichen Plan-Kopie, nie dem Modul-eigenen Zustand), der
             Elevation-Hook (FBElevationProvider/FBConstantElevation/FBRunwayPlateauElevation/
             FBBakedDemElevation, s.o.), gemeinsame Basistypen — zeigt NIE nach systems/ oder
             modules/. Zwei getrennte Kanäle für alles, was sonst verstreutes printf/fprintf wäre:
               FBLog (`FBLog.h/.cpp`) — diskrete, geleveled Ereignisse (Debug/Info/Warn/Error), ein
               `tag` + `event` + strukturierte key=val-Felder (`FBLog::Warn("pilot", "sink_rate_high",
               {{"vs", -12.3}})`). Statische Fassade (Cross-Cutting-Infrastruktur, kein FBLog& durch
               jedes `Run()`), aber I/O-frei: emittiert wird nur, wenn ein `FBLogSink` injiziert ist
               (`FBLog::SetSink`) — die Sink-Implementierungen (stdout/Datei/Fan-out) leben in `app/`
               (`FBLogSinks.h/.cpp`), core/systems/render/world/fdm rufen nur die Fassade.
               FBTelemetry (`FBTelemetry.h/.cpp`) — periodisch gesampelter Zustand (Zeitreihe, Schema).
               Klassen DEKLARIEREN sich als `FBTelemetrySource` (`DeclareTelemetry`/`SampleTelemetry`);
               der EINE `FBTelemetryBus` sampelt jede registrierte Source pro Tick in genau eine Zeile
               (Registrierungsreihenfolge = Spaltenreihenfolge) und reicht sie an einen injizierten
               `FBTelemetrySink` (z. B. `FBCsvTelemetrySink`, app/). FBAirDataSystem/FBPilot/
               FBFlightControl/FBAirframeControls implementieren `FBTelemetrySource` direkt;
               FBFdmTelemetrySource (fdm/, s.u.) trägt die rohe FDM-Pose bei.
  math/      Value-Math (FBMat4 — Value-Types, Operatoren inline im Header)
  render/    FBRenderer (Orchestrator: Device/Swapchain/Targets, JEDE Begin/EndRenderPass-Grenze,
             die Encode-Reihenfolge — Pass-Topologie ist ein Vertrag, kein Stage-Split darf sie
             vermehren), FBCamera, FBMips, FBChunkMesh/FBChunkVtx, FBEphemeris, FBGpu/FBFrameContext
             (Device-Handle bzw. geteilter Frame-Zustand, den jede Stage bekommt), FBDrawStage
             (Interface: ein Shader + seine Pipeline(s)/Bind-Group(s)/Draws, zeichnet in den
             GELIEHENEN Encoder — nie eigene Pass-Grenzen), FBHudGeometry (der HUD-Geometriepuffer —
             Lines/Tris/Glyphs als wiederverwendete Vektoren, von FBDisplaySystem gefüllt), FBHudFont
             (das generische, airframe-agnostische Bitmap-Font-Rendering-System — 16x16-Zellen mit
             ECHTER 8-Bit-FLÄCHEN-Coverage statt 1bpp-Maske, gebaked aus B612 Mono [SIL OFL 1.1,
             Airbus-Cockpit-Typeface] via `sim/tools/bake_hud_font.py` [Pillow, 8x supersampled +
             Box-Filter runter auf 16x16; KEINE Build-Abhängigkeit, läuft nur bei Font-/Charset-
             Wechsel] in das GENERIERTE `FBHudFontRom.h` — `FBHudFont.h` selbst bleibt handgepflegt:
             gegutterter Atlas-Layout und Glyph-Quad-Builder mit Quad-Überstand, reine Backend-Ressource
             ohne Globals; kFontAdvance/kFontQuadSize sind die öffentlichen Screen-Pixel-Einheiten aller
             Aufrufer und bleiben unverändert, nur die interne Rasterauflösung wuchs 8x8 → 16x16.
             FBHudStage sampelt LINEAR und rekonstruiert daraus per Screen-Footprint-Coverage
             ("sharp bilinear") echtes Antialiasing statt hartem NEAREST-Alpha-Test — coverage-agnostisch,
             unverändert seit der 8x8-1bpp-Ära. Chip-SPEZIFISCHE Eigenheiten — MAX7456-Artefakte o.ä. —
             gehören NICHT hierher, sondern in einen moduleigenen Hook, s. modules/f16/FBF16Max7456)
  render/stages/  Klasse pro Shader: FBTransmittanceStage/FBSkyViewStage/FBSkyStage (Hillaire-
             Atmosphäre + die Wolkendecke-Value-Noise-Sheet auf der Dome), FBSunStage/FBMoonStage
             (Sonnenscheibe+Glow bzw. Mond-als-belichtete-Kugel — additive Draws, direkt nach FBSkyStage
             in derselben Scene-Pass, gleiche Blend-Reihenfolge wie der frühere Ein-Shader-Composite;
             FBMoonStage besitzt die NASA-LROC-Albedo-Textur als alleinige Konsumentin), FBTilesStage
             (Terrain: Albedo-Array, RenderBundle/2-Phasen-Streaming-Zustand, Invarianten-Zähler),
             FBStarsStage, FBTileLightsStage, FBHudStage (reines WebGPU-Backend: Pipelines/Buffer/
             Atlas; ruft pro Frame das geliehene FBDisplaySystem::BuildHud() für die Geometrie — die
             Symbologie-LOGIK lebt in systems/FBDisplaySystem, s.u.), FBUpscaleStage, FBUnitsStage/FBSpritesStage
             (NoOp, aber in der Encode-Ordnung verdrahtet: Units nach Terrain, Sprites vor HUD); Wolken
             als 6 Klassen (FBCloudMipDownStage teilt den Box-Downsample-Helfer, FBCloudBaseBakeStage/
             FBCloudDetailBakeStage/FBCloudCellBakeStage backen die 3 Noise-Volumes je einmal,
             FBCloudMarchStage marcht die WGS84-Kugelschale ins Viertel-Res-Ziel, FBCloudResolveStage
             löst temporal auf); FBTonemapStage (EIN Shader-Quellcode, zwei Pipelines — mit/ohne
             Wolken-Composite, analog HUD Solid/Line). FBRenderer.cpp führt keinen Inline-Shader mehr
             (jedes `R"(`-WGSL lebt in genau einer stages/-Datei, `grep -c 'R"(' FBRenderer.cpp` == 0)
             — der Render-Stage-Split ist damit abgeschlossen.
  world/     FBWorld, FBTerrainLoader (Tile-Streaming/Worker-Anbindung; FBWorld hält
             zusätzlich die geborgte FBUnit-Registry, s.u. units/), FBTilesElevation (der
             Elevation-Hook-Provider auf fb_stream_ground, s.o. — NICHT Teil der Core-Lib)
  units/     FBUnit-Basisschnittstelle + FBOwnshipUnit (Details s.u., nach dem Verzeichnisbaum)
  systems/   die generischen, airframe-agnostischen System-Slots eines Moduls — Interface + Default
             in EINER Klasse, ein Modul überschreibt per Ableitung (Zahlen-Tuning bleibt Preset/
             Config, keine leere Ableitung dafür):
               FBAutopilot (Guidance), FBFlightControl (FBW-Innenschleife),
               FBAirDataSystem (CAS/Mach/G-Last, FPM-Richtung als Ground-Track/Flightpath-Angle aus dem
               ENU-Geschwindigkeitsvektor), FBRadarAltimeter (AGL aus DER SELBEN DEM-Quelle, die die App
               schon für `SetAgl` auflöst — keine zweite Terrain-Abfrage), FBNavSystem (ein Steerpoint +
               Bullseye, planare ENU-Geodäsie wie `home_bearing`/`home_dist`; `AdvanceWaypoint` ist die
               Wegpunkt-Sequenzierung — Akteurs-Verhalten, das Modul ruft es selbst, nicht der Runner) —
               die heute REAL
               implementierten Systeme, `Run()`/`Update()` virtuell, dem Sensoren-SCHREIBEN-FBState-
               Muster folgend;
               FBDisplaySystem (eigene Datei, `FBDisplaySystem.h/.cpp`): das generische Default-HUD
               (MIL-STD-1787-artig — Waterline, konformer Horizont, Heading-/GS-/Alt-Tapes mit
               Steerpoint-Marker, NO-TELEMETRY-Fallback) als `BuildHud(state, FBHudEnv, FBHudGeometry&)`, der eine
               Override-Punkt (analog `FBAutopilot::Run`); FBHudEnv trägt Viewport/AGL/Have-Telemetry,
               FBHudGeometry (render/) ist der wiederverwendete Geometriepuffer, den FBHudStage pro
               Frame ausliest;
               FBSystemSlots.h — Input/HOTAS, Propulsion, Sensors, Weapons, Defensive, Comms: Interface
               + NoOp-Default für die restlichen F-16-Systemkategorien, ein Modul füllt sie bei Bedarf
               per Ableitung. Sensoren SCHREIBEN/Displays LESEN den geteilten FBState; Sensoren/Waffen/
               Defensiv erhalten eine geborgte FBWorld-Referenz (nie global).
               FBPilot (`FBPilot.h/.cpp`): die Missions-Ebene ÜBER Guidance/FlightControl — FBAutopilot
               (Manöver) und FBFlightControl (100 Hz, die Hände) bleiben unangetastet; FBPilot entscheidet
               WOHIN (FBFlightPlan-Wegpunkte, optionale FBRunway) und gibt das im ~10-Hz-Entscheidungstakt
               (vom Modul gedrosselt wie jeder andere Slot) als `FBPilotCommands` aus: eine Guidance-
               Anfrage an FBAutopilot (`FBPilotGuidance::None/Manual/Direct` — None = "AP
               unangetastet lassen") plus optionale (`std::optional`) Airframe-Kommandos an
               FBAirframeControls. Die Phasen-Zustandsmaschine (Idle/Preflight/Takeoff/Climb/Route/
               Approach/Flare/Rollout/Shutdown, doc/f16/procedures-*.md) ist das Prozedur-Gerüst; Run()
               ist der Override-Punkt (analog FBAutopilot::Run). FBAirframeControls (`.h/.cpp`): das
               Interface, das der Pilot bedient (Gear/Speedbrake/Radbremsen L+R/Bugradsteuerung/Engine-
               Start-Cutoff + WOW-/Gear-Positions-Getter) — Interface+NoOp-Default in einer Klasse wie
               FBSystemSlots.h, `FBJsbsimAirframeControls` daneben ist die reale, airframe-agnostische
               Ownship-Implementierung (forwarded auf den fdm/-Adapter).
  terrain/   leane Terrain-Lib (geo/mesh/osmmesh), flat
  fdm/       der JSBSim-Adapter, flat, INSTANZFÄHIG: `FBFdm` (`.h/.cpp`) ist EIN simuliertes Flugzeug —
             eine FGFDMExec-Instanz plus deren Zustand hinter einem pimpl; `FBFdm.cpp` ist die EINE
             Übersetzungseinheit, die einen JSBSim-Header inkludiert (die Ein-TU-Naht: jeder Aufrufer
             sieht nur den flachen POD-Zustand `fb_fdm_state` + die Methoden von `FBFdm`, nie
             `FGFDMExec`). KEINE statischen mutablen Globals mehr (grep-verifizierbar) — beliebig viele
             FBFdm koexistieren im selben Prozess mit unabhängiger Physik (jede FGFDMExec baut ihren
             eigenen Property-Root). Prozessweit bleibt in JSBSim SELBST nur, was keine Physik trägt und
             in `FBFdm.cpp` dokumentiert ist: `FGJSBBase::debug_lvl` (statisch, `SetDebugLevel` wirkt für
             alle), der eine globale Logger (`JSBSim::SetLogger`) und die im Konstruktor gelesenen
             `JSBSIM_DEBUG`/`JSBSIM_DISPERSE`-Env-Variablen.
             **OWNERSHIP:** wer die Einheit besitzt, besitzt ihre FBFdm — heute die App/der Missions-
             Runner (ein `std::unique_ptr<FBFdm>` pro Lauf), perspektivisch `units/FBUnit`. Module und
             Systeme BORGEN sie: `FBFdm&` zum Kommandieren, `const FBFdm&` zum Lesen — jede Kommando-
             Methode ist nicht-const, jeder Readback const, ein Lese-Handle kann also nicht schreiben.
             Das MODUL bekommt sie einmalig über `FBModule::AttachFdm` (die Registry baut Module
             argumentlos, also ist das der Konstruktor-Injektions-Ersatz) und reicht sie an die Systeme
             weiter, deren Zuordnung fix ist (`FBJsbsimAirframeControls`, konstruktor-injiziert).
             **IC-ABSCHOTTUNG (strukturell, nicht per Konvention):** der ladende Konstruktor von FBFdm ist
             privat, einziger Friend ist `FBFdmBoot` (`FBFdmBoot.h/.cpp`) — `FBFdmBoot::Spawn(FBFdmSpawn)`
             ist der EINE Weg, eine geladene, getrimmte Instanz zu erzeugen, und es gibt kein Re-Init/
             Reset auf FBFdm. Wer `FBFdm.h` inkludiert (jedes Modul/System, für die Referenz), erreicht
             damit KEINE IC; wer IC will, muss `FBFdmBoot.h` nennen — und das tun nur `app/`-Dateien
             (`FBMissionBoot.h`, `FBAppWasm.cpp`, die Test-Harnesses).
             `namespace FlightBox` wie der Rest des Baums — kein `extern "C"`: das war für die längst
             gelöschte `xp_bridge.c` der Vor-Pivot-Architektur, niemand ruft den Adapter heute aus C oder
             aus JS (der WASM-Export ist ausschließlich `fb_toggle_ground`/`fb_set_ground` in
             `FBAppWasm.cpp`); `extern "C"`/`EMSCRIPTEN_KEEPALIVE` bleibt Konvention einzig für von JS
             NAMENTLICH gerufene Symbole. FBFdmTelemetrySource (`.h/.cpp`) ist die Telemetrie-Source
             für die rohe FDM-Pose (lat/lon/alt/AGL/Lage/`fuelLbs`) — an der Adapter-Naht, weil
             `fb_fdm_state` dessen eigenes POD ist; borgt (konstruktor-injiziert) die `const FBFdm&`, den
             FDM-Zustand und die vom Aufrufer aufgelöste Boden-ASL. FBFdm trägt außerdem die generische
             Tank-Verdrahtung (FGPropulsion: `Get/SetFuel*`, je Tank oder Gesamtsumme/-prozent,
             proportional auf die modelleigenen Tankkapazitäten verteilt) — Spritmangel selbst simuliert
             JSBSim nativ (Triebwerk stirbt in der Physik), der Adapter macht es nur beobachtbar/setzbar.
  modules/   FBModule-Basisschnittstelle (`Run(fb_fdm_state&, dt, const FBWorld*)` PLUS die generischen
             System-Accessoren UND `ApplySetup(key,value)` — das Modul interpretiert seine eigenen
             `set`-Mission-Schlüssel, s.o. "Der Missions-Runner ist reiner Orchestrator"; App hält
             jedes Modul dahinter polymorph; ein Modul cycelt seine Systeme intern, jedes im eigenen
             Takt — Peers rufen sich nie gegenseitig), FBModuleRegistry.h/.cpp (Name→Factory, s.o.)
  modules/f16/           FBF16ModuleRegistration.cpp registriert `"f16"` bei FBModuleRegistry (s.o.) —
                         die EINE Datei außerhalb der Registry selbst, die FBF16Module.h nennt.
                         das F-16-Modul: FBF16Module komponiert die systems/-DEFAULTS (FBAutopilot/
                         FBFlightControl/FBAirDataSystem/FBRadarAltimeter/FBNavSystem unverändert) mit
                         dem F-16-Gain-Preset (FBFlightControl::F16()), besitzt den Piloten (FBF16Pilot,
                         `.h/.cpp`) und die Ownship-Airframe-Controls (FBJsbsimAirframeControls) und
                         cycelt alle Systemslots inkl. Piloten
                         (10 Hz) — solange dessen Phase Idle bleibt, ändert das NICHTS am bestehenden
                         Verhalten (FBPilotCommands bleibt neutral). FBF16Pilot ist heute ein dünnes
                         Skeleton (übernimmt das FBPilot-Default-Verhalten unverändert) — hier landen
                         künftig die F-16-eigenen Zahlen/Prozeduren: die Abhebegeschwindigkeit nach
                         Gewichtstabelle (doc/f16/procedures-takeoff-taxi.md) und der 11°-AoA-Anflug
                         (doc/f16/procedures-landing.md). Der Ort, an dem künftiges
                         F-16-spezifisches Verhalten (echtes Radar, echtes HOTAS-Binding, …) als
                         Ableitung eingehängt wird. Trägt drei F-16-eigene HUD-Platzhalter (eigene
                         `.h/.cpp`, kein genereller Systemslot, da airframe-spezifisch): FBF16FireControl
                         (der "B"-Range-Provider — Slant-Range aus Distanz + Höhendifferenz zur
                         Steerpoint-Elevation, Pythagoras), FBF16Ufc (ALOW-Floor + gewählte
                         Steerpoint-Nummer), FBF16Sms (Master-Arm-Status). FBF16Max7456 (eigene Datei,
                         `.h/.cpp`): der MAX7456-CHIP-spezifische Hook (Interlace-Jitter,
                         Helligkeitskurve, Sync-Artefakte, …) — heute ein echter, von FBF16Module
                         gehaltener NoOp-Override-Punkt, getrennt vom generischen Font-System in
                         render/FBHudFont.h
  modules/f16/displays/  FBF16Hud (`FBF16Hud.h/.cpp`): die F-16-eigene MIL-STD-1787-Symbologie — FPM,
                         konforme Pitch-Ladder, Heading-/CAS-/Alt-Tapes, Bank-Winkel-Skala, G-Last,
                         der ARM/Mach/Peak-G/NAV/Bullseye-Block, der R/AL/B/TTG/Dist>STPT-Block sowie
                         Steerpoint-Diamond (crossed-out jenseits der echten F-16C-TFOV/2) + Tadpole.
                         Überschreibt FBDisplaySystem::BuildHud (der Override-Punkt, s.o.), liest nur
                         FBState — geschrieben von FBAirDataSystem/FBRadarAltimeter/FBNavSystem/
                         FBF16FireControl/FBF16Ufc/FBF16Sms. Positionen/Formate gegen den DCS F-16C
                         Viper Guide (Part 16, p.706) UND den GPL-2.0 FlightGear-F-16-Mod
                         (github.com/NikolaiVChr/f16, Nasal/HUD — Fakten zitiert, kein Code kopiert)
                         abgeglichen. Alles sitzt in der echten Combiner-APERTURE (kApertureHalfWidthDeg/
                         -HeightDeg, ~25°xIFOV-Seitenverhältnis) statt am Bildschirmrand — Tapes/Blöcke an
                         deren Kanten/Ecken, die konforme Symbologie (Ladder/Horizont/FPM/Diamond/Tadpole)
                         am Fensterrand gescissort (`FBHudGeometry::SetClip`), der Diamond-Clamp fällt mit
                         dem Fensterrand zusammen.
```

`units/` (`sim/src/units/`) trägt die Welt-Entitäten-Basisschnittstelle: `FBUnit` (Identität — Id/
Kind-Enum `Aircraft/…`/Team-Enum `Friendly/Hostile/Neutral` —, geodätische Pose, `virtual void
Run(dt, const FBWorld*)`). `FBOwnshipUnit` ist die erste reale Einheit: der eigene Jet als FBUnit,
BORGT das App-eigene `fb_fdm_state` (keine Kopie der Wahrheit) und leitet die Pose daraus ab; `Run()`
bleibt leer, weil das Modul sein FDM/seine Systeme bereits selbst cycelt. FBWorld hält eine reine
BORGTE Registry (`RegisterUnit`/`Units()`, `std::vector<FBUnit*>`, keine Ownership) — die App
registriert das Ownship beim Boot, Sensoren/Waffen lesen sie über die geborgte FBWorld-Referenz, die
jeder Systemslot schon erhält. KI-Einheiten (freundlich/feindlich/neutral) hängen sich später an
dieselbe Schnittstelle, sobald sie real existieren.

**Coding-Style: an JSBSim orientiert** (unser Code fügt sich in dessen Ökosystem): Klassen `FB`-Präfix
(analog `FG` — `FBFlightControl`, `FBRenderer`), PascalCase-Methoden (`Run()`, `GetLoadFactor()`),
Member PascalCase, ein `namespace FlightBox`, Header-Guards, Klasse-pro-Datei `FBName.h/.cpp`,
Getter inline im Header. JSBSims LGPL-Banner nicht kopieren — unsere Dateien tragen unsere Lizenz.

## Engineering-Konventionen

- **Build nur über Make-Targets** — jedes Projekt trägt sein eigenes Makefile: `sim/` baut die CC
  (`make -C sim wasm` | `worker` | `core-lib` | `native` | `gym` | `image` | `up`), `tiles/` den
  Tile-Server (`make -C tiles build` | `image` | `run`). Rezepte leben im Makefile, nicht in
  Agenten-Köpfen.
- **`extern "C"` für jede von JS namentlich gerufene Funktion** (EMSCRIPTEN_KEEPALIVE reicht nicht —
  Mangling bricht Exporte still).
- **Frame-Beweis-Pflicht:** build-wirksame Änderungen gelten erst mit gerendertem Frame oder
  numerischer Messung als verifiziert.
- **Mess-Disziplin:** akzeptierte Modell-Eigenschaften der vanilla JSBSim-F-16 sind die Wahrheit,
  keine Defekte (Prinzip 5); Messungen laufen über den Missions-Regelkreis (Telemetrie), nicht über
  Einzelbeobachtungen. Ziel-GPU-Fähigkeiten: `doc/webgl-webgpu-report.txt`.
- JSBSim (`sim/vendor/jsbsim`) und das f16-Modell sind read-only; Warnings = Errors.
- **Keine verstreuten printf/fprintf/std::cout/std::cerr:** core/systems/modules/render/world/fdm/
  units emittieren NIE direkt — Ereignisse laufen über `FBLog` (core/FBLog.h), periodischer Zustand
  über `FBTelemetryBus` (core/FBTelemetry.h). Ausnahmen NUR: die Sink-Implementierungen selbst
  (`app/FBLogSinks.*`, `app/FBTelemetrySinks.*`) und CLI-UX in `app/` (Usage/Hilfe/argv-Fehler,
  Bootstrap-Fehler vor Sink-Aufbau). Core bleibt I/O-frei, aber nicht formatierungsfrei — `snprintf`
  in einen lokalen Puffer ist überall erlaubt (kein `FILE*`/`fstream`).
- **Missions-Regelkreis (Pilot-KI-Entwicklung):** Mission definieren → headless bis Abschluss/Fehler/
  Crash simulieren → Telemetrie maschinell analysieren → Korrektur → Loop. Format: `.fbm`
  (zeilenbasiert, zero-dependency, `doc/mission-format.md` — `name`/`module`/`spawn`/`timeout` sind
  Pflicht, `runway` optional), geparst von `core/FBMissionFile.h` (reine Text→`FBMission`-Funktion,
  kein File-I/O — das macht die App). Orchestrator: `FBRunMission` (`app/FBMissionRunner.cpp`, geteilt
  von `fb-gym` und `gpu_native --mission`) — vier Schritte, keine Missions-Spezifika im Code (s.o.
  "Der Missions-Runner ist reiner Orchestrator") — löst `module` über `FBModuleRegistry` auf, spawnt
  die deklarative `FBSpawn` (Boden ODER Luft, EINE IC-Anwendung — `FBMissionBoot.h`) und treibt die
  Sim-Sekunden so schnell wie möglich (Prinzip 4) — `fb-gym` ist reines JSBSim, KEIN Renderer/
  GPU-Gerät; native `--interval` schaltet periodische Beweis-Frames über einen GPU-freien Hook dazu
  (Renderer bleibt Bolt-on, nie Abhängigkeit der Physik-/Terminierungs-Logik). Terminierung
  SUCCESS/FAIL/CRASH/TIMEOUT → Exit-Codes 0/1/2/3 (beide Monitore, s.u. "Kein Cheaten", kombiniert),
  worauf der Regelkreis branch. Telemetrie je Lauf in `--out/`: `telemetry.csv` (10 Hz, feste
  Spaltenzahl, inkl. `fuelLbs` — FGPropulsion-Tanksumme) + `events.log` (`t=SEK EVENT key=val`,
  greppbar) — das Analyse-Werkzeug für die Pilot-KI, kein Produktionspfad.
- **Kein Cheaten:** ZWEI unbestechliche, Runner-/App-eigene Instanzen, nie vom Modul gesehen, nie
  vermischt — zwei Fragen, nicht eine. `core/FBFlightMonitor` entscheidet K.O. (Absturz/LOC): rein
  physikalisch, modul-agnostisch (kennt keine Flugzeug-Typen, keine deklarierten Zahlen; Struktur-/
  Gear-Wahrheit kommt aus dem gepinnten JSBSim-Modell selbst). `core/FBMissionMonitor` (dessen
  Geschwister) entscheidet das MISSIONS-Urteil (SUCCESS/FAIL/TIMEOUT) aus der Missionsdatei selbst:
  Wegpunkte erreicht (gegen die EIGENE, unveränderliche Kopie von `FBFlightPlan`, nie das Modul-eigene
  mutierte Exemplar — reine Positions-Beobachtung, keine Modul-Selbstauskunft), Bodenkontakt abseits
  der zugewiesenen Runway (RESULT FAIL, nicht CRASH), Timeout. Beide werden von JEDEM Client gefüttert,
  der eine Sim-Schleife fährt — `FBMissionRunner` (fb-gym/gpu_native `--mission`) genauso wie der
  WASM-App-eigene Frame-Loop (`FBAppWasm.cpp`) — je EINE Definition, kein zweiter Paralleltest.
  Piloten/Module wirken NUR über die simulierten Systeme (`fcs/*-cmd-norm` via FBFlightControl/
  FBAutopilot, FBAirframeControls für Gear/Brakes/Steer/Speedbrake/Engine, Throttle, Tank-Füllstand via
  `FBFdm::SetFuel*`) — der einzige State-Schreiber (JSBSim-IC/Trim) ist der App-eigene Boot-Spawn
  (`FBMissionBoot.h::FBMissionApplySpawn`, plus die gleichrangigen App-Boot-Pfade `FBAppWasm.cpp`s
  `?ap=manual` und dedizierte Test-Harnesses). Die IC-Abschottung ist STRUKTURELL, nicht bloß
  grep-belegt (s. `fdm/`-Absatz: privater Lade-Konstruktor, Friend `FBFdmBoot`, eigener Header) —
  `systems/` und `modules/` erreichen die IC nicht und sehen `FBFlightMonitor`/`FBMissionMonitor` nie.
  Der PILOT liest die Zelle ausschließlich über `FBAirframeControls` (WOW/Gear/Gewicht/Engine-Running),
  nie an dieser Schnittstelle vorbei in ein FDM — so bleibt `systems/` airframe- UND instanz-agnostisch.

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
(`FBFdm::GetGroundClearanceM`, gear-down/up), nicht aus einer fixen Zahl — essenziell für Start/Aufsetzen/
Crash-Erkennung. Die Kamera geht **nie unter die Oberfläche** (Clamp auf Grund + Modell-Bauch-Clearance).
**Crash → Motor aus**, den Rest macht JSBSims Ground-Reactions, kein Freeze.

## temp/ — Migrationsgut

`temp/` hält das Material der Vor-Architektur (Validator, TS-Testsystem, skalierte Aircraft-Modelle,
Original-osmmesh/geo). Read-only-Steinbruch, keine lebende Architektur — Teile entfallen ersatzlos,
sobald ihr Nachfolger im `sim/`- oder `tiles/`-Baum steht.

## Host & Betrieb (dieser Rechner) — transparentes Projektwissen, kein Agenten-Memory

Agenten führen KEIN verstecktes Memory; alles Betriebswissen steht hier:

- **emsdk** liegt in `~/Git/emsdk`; ein `nproc`-Shim liegt in `~/.local/bin` (macOS hat kein nproc).
- **Container:** Podman-VM zuerst (`podman machine start`), dann `tiles/up.sh` (fb-tiles, :8081) und
  `sim/up.sh` (fb-sim, :8080). fb-sim mountet `sim/web` LIVE — `make wasm` wirkt per Refresh.
- **WASM-Artefakte sind gitignored.** `make -C sim wasm` genügt — das `wasm`-Target hängt vom
  `worker`-Target ab und baut IMMER beide (gpu.js/gpu.wasm + fbtileworker.js/.wasm); `make -C sim
  worker` bleibt einzeln aufrufbar. Fehlt der Tile-Worker, hängt die App still beim Start (404 im
  Worker) — daher die feste Abhängigkeit, nicht zwei getrennt zu merkende Targets.
- **Git:** Commit-Mail ist der GitHub-noreply-Alias (nicht github@outshine.de); Push läuft per
  SSH-insteadOf. Native Builds brauchen `sim/vendor/.compat-headers` (gitignored, host-lokal).
