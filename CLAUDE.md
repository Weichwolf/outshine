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
| `bfm` (`set task bfm`) | dieselbe Phasenmaschine in ihrer Kampf-Phase: KEIN Autopilot-Modus, sondern ein eigenes Regelgesetz auf Manual-Stick, das gegen den GELOCKTEN Radarkontakt fliegt (systems/FBBfmTrack) — Lead/Pure/Lag aus Aspekt, Peilung und Annäherungsrate, Energie gegen die gemessene Corner-Speed, und bei Kontaktverlust Extrapolation + Suchmuster. Missionen: `sim/missions/bfm-*.fbm` |

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
  `build/fb-gym`) — headless: `--mission FILE [--out DIR] [--timeout N] [--threads N]
  [--elev tiles|const|swiss]`. `--threads` ist GYM-ONLY (s.u. "Etappe 4").
  KEIN Dawn-/wgpu-Symbol im Binary (verifiziert per `nm`). Der Missions-Kern (Prinzip 4: so schnell wie
  die Maschine kann).
- **`gpu_native`** (`app/FBAppNative.cpp`, `make -C sim native`) — der Referenz-Renderer/Frame-Orakel:
  `--mission --interval` (PNG-Beweisframes on top of derselben `FBRunMission`-Schleife, über einen
  GPU-freien `FBMissionTickHook`, den nur `FBAppNative.cpp` mit einem echten `FBRenderer`/`FBWorld`
  implementiert) + der No-Module-Screenshot-Modus. `--mission` OHNE `--interval` bleibt headless (`wie
  bisher`, Regressions-Kennzahlen unverändert).
- **wasm** (`app/FBAppWasm.cpp`, `make -C sim wasm`) — der Browser, Verhalten unverändert.

**Multi-Unit:** eine Mission beschreibt einen VERBAND mehrerer Einheiten/Module verschiedener Fraktionen
(je Unit: Modul, Fraktion, Spawn, Flightplan/Ziele — z.B. F-16 vs. MiG-29 im Gym; evolutionäre
Piloten-Turniere über Telemetrie-Fitness). **Etappe 1 GEBAUT:** der fdm/-Adapter ist instanzfähig —
`FBFdm` ist ein Objekt pro Airframe (keine globale Instanz, keine statischen mutablen Globals mehr),
mehrere koexistieren im selben Prozess mit unabhängiger Physik (Beweis: `make -C sim test-fdm` →
`build/fb-test-two-fdm`). **Etappe 2 GEBAUT:** der Akteur ist EIN Objekt (`units/FBSimUnit`).
**Etappe 3 GEBAUT:** der Verband ist MISSIONSDATEN — `.fbm` trägt eine Liste von `unit`-Blöcken, jeder
Client hält eine `FBActorList`, jede Einheit bekommt eigene `FBFdm`/`FBModule`/Monitore/Telemetriedatei,
und das Missions-Urteil fällt PRO EINHEIT (Details unten + `doc/mission-format.md`). Die
**Snapshot-Disziplin steht ab jetzt**, obwohl noch niemand cross-unit liest: pro Tick rechnen ERST alle
Einheiten, DANN macht eine Barriere (`FBSimUnit::PublishPose`) die neuen Posen gemeinsam sichtbar —
`FBUnit::GetPose()` (das, was die `FBWorld`-Registry zeigt) liefert immer den Stand des zuletzt
ABGESCHLOSSENEN Ticks, also kann die Tick-Reihenfolge kein Ergebnis beeinflussen.

**Etappe 4 GEBAUT — Thread pro Einheit, aber NUR im Gym.** `fb-gym --threads N` (Default 1 = der
sequenzielle Referenzpfad) parallelisiert GENAU eine Phase des Ticks: den STEP jeder Einheit (Modul +
eigene `FBFdm`). native und wasm bleiben Single-Thread im Sim-Loop — Echtzeit braucht keine
Parallelphysik, und der Browser erspart sich den pthreads/SharedArrayBuffer-Build; `app/FBTickPool.h/
.cpp` wird ausschließlich von `FBMissionRunner.cpp` inkludiert, ist NICHT Teil der Core-Lib und erreicht
den WASM-Build nie.

- **Pool + Barriere** (`app/FBTickPool`, C++17-Eigenbau — `std::barrier` ist C++20): N-1 Worker werden
  EINMAL für den Lauf erzeugt (bei 10 Hz über Tausende Ticks wäre ein Thread pro Tick reiner Overhead)
  und parken auf einer Condition-Variable. `RunTick(job, count)` verteilt die Indizes über einen
  ATOMAREN Zähler — wer frei ist, nimmt die nächste Einheit (dynamischer Plan, für ungleiche Last) —
  und kehrt erst zurück, wenn jeder Index fertig ist: dieses Return IST die Lockstep-Barriere. Der
  aufrufende Thread arbeitet mit, `--threads 1` erzeugt gar keinen Thread und die Schleife läuft inline.
- **Sequenziell bleibt** (und zwar in Akteurs-Reihenfolge): das Laden/Spawnen der Modelle (JSBSims
  statische `Element::convert`-Einheitentabelle wird beim XML-Parsen per `operator[]` MUTIERT), das
  Elevation-Sampling (der Provider ist das EINE geteilte Objekt des Clients — `FBTilesElevation` fährt
  den Tile-Streamer), `PublishPose` (das IST die Barriere), beide Monitore + die Envelope-Checks,
  Telemetrie-Sampling und der `FBMissionTickHook` (der Renderer des nativen Orakels).
- **Log/Telemetrie ohne Determinismus-Verlust:** Telemetrie ist längst pro Einheit (eigener Bus, eigene
  Datei) und wird in der sequenziellen Phase gesampelt. Für `FBLog` wurde die statische Fassade
  BEHALTEN, aber ihr KONTEXT thread-lokal: `Sink_`/`Level_` sind Boot-Konfiguration und bleiben
  prozessweit, `TimeS_`/`Unit_` plus ein neuer `ThreadSink_` sind `thread_local` — ein Thread, der
  Einheit `two` rechnet, IST in einem anderen Kontext als einer, der `lead` rechnet (die Alternative,
  ein Kontext-Objekt durch jede `Run()`-Signatur zu fädeln, ist genau das, was diese Fassade vermeidet).
  Kein Worker schreibt je direkt in einen gemeinsamen Sink: der Runner zeigt jeden Worker auf den
  `FBBufferedLogSink` (`app/FBLogSinks.h`) DER EINHEIT, die er rechnet, und dräniert die Puffer an der
  Barriere in Einheiten-Reihenfolge in den echten Sink. Zeilenposition hängt damit nie am Scheduler.
- **Bewiesen:** `payerne-pair`/`payerne-pair-fail`/`payerne-four`/`payerne-mixed` liefern über
  `--threads 1..4` und je 5 Wiederholungen EINEN einzigen Fingerabdruck (SHA-256 aller `telemetry*.csv`
  + normalisierter `events.log` + Exit-Code, inkl. `decisive=`-Attribution); die 7 Einzel-Missionen ×
  const/swiss sind mit dem Default byte-identisch zum Stand vor Etappe 4.
- **Skalierung, ehrlich:** ein F-16-Step kostet ~95-100 µs und ist praktisch phasenunabhängig (Bodenroll
  vs. Reiseflug ≤7 % Unterschied) — eine Mission kann über Flugphasen also kaum Ungleichlast erzeugen.
  Gemessen (Apple A18 Pro, 2 P- + 4 E-Kerne): 2 Einheiten 1.29–1.41x bei 2 Threads, 4 Einheiten
  1.49x/1.53x/1.77x bei 2/3/4 Threads. Die Decke ist die MASCHINE, nicht die Barriere: zwei
  UNABHÄNGIGE `fb-gym`-Prozesse skalieren genauso schlecht (0.42 s allein → 0.58 s je, = 1.45x), und
  eine Spin-vor-Park-Variante der Barriere bewegte nichts. Threading lohnt ab ~4 Einheiten auf echten
  Performance-Kernen; darunter ist es ein Faktor <1.5.

Als `wallS`/`speedup` der `SUMMARY`-Zeile misst der Runner seither `steady_clock` statt `clock()` —
letzteres summiert bei mehreren Threads die CPU-Zeit und hätte einen schnelleren Lauf als langsameren
gemeldet.

**Etappe 5 GEBAUT — Einheiten sehen einander, aber nur über ein System:** der kooperative Datalink
(`systems/FBDatalinkSystem` + `modules/f16/FBF16Datalink`, s.u.). **Etappe 6 GEBAUT — das FCR-Radar:**
`systems/FBRadarSystem` + `modules/f16/FBF16Fcr` (s.u.) sind der AKTIVE Sensor daneben — Scanvolumen
statt Reichweite, Kontaktaufbau und -verlust in Zeit, ANONYME Kontakte und IFF Mode 4 als einzige
Identitätsquelle. Damit ist die Registry-Konsumentenliste in `systems/`+`modules/` vollständig: genau
diese zwei Dateien, nachweisbar per Grep (s.u. „Kein Cheaten"). Der Pilot nutzt das Radar noch NICHT —
es schreibt ausschließlich `FBState`, weshalb alle bestehenden Missionen spaltengenau unverändert
fliegen (die elf `fcr_*`/`iff_xpdr`-Spalten hängen hinten an).

**Etappe 8 GEBAUT — das Avionik-Datenmodell:** `FBState` ist der typisierte BLOCK-Bus mit
Dreizustands-Gültigkeit, und der Pilot bedient Avionik nur noch über einen Kommando-/Quittungs-Pfad mit
zwei Latenzklassen (s.u. `core/`). Reines Refactoring plus zwei neue Kanäle: alle Bestandsmissionen
bleiben auf ihren bisherigen Spalten byte-identisch.

**Der Missions-Runner ist reiner Orchestrator, genau vier Schritte, KEIN Missions-Wissen im Code:**
Mission laden → Welt mit ihren Akteuren aufsetzen (Elevation für die deklarative `spawn`-Zeile auflösen,
Modul über `modules/FBModuleRegistry` spawnen) → Akteure ausführen (Modul takten, beide Monitore
füttern) → Welt validieren (die Monitore haben's längst entschieden). Ein AKTEUR ist EIN Objekt
(`units/FBSimUnit`, s.u.), und die Schritte 2-4 sind SCHLEIFEN über eine `FBActorList` davon — ein
Eintrag je `unit`-Block der Missionsdatei, in Dateireihenfolge (Index 0 = primärer Akteur: kanonische
`telemetry.csv`, Kamera-Auge). Gesamturteil: ein Physik-K.O. IRGENDEINER Einheit beendet den Lauf (die
konservative Lesart — kein Wrack integriert im Hintergrund weiter), und das MISSIONS-Urteil fällt PRO
EINHEIT — jede Einheit MIT Zielen trägt ihren eigenen `FBMissionMonitor`; SUCCESS erst, wenn ALLE ihre
Ziele erreicht haben, FAIL/TIMEOUT sobald eine scheitert. Der Runner ist die einzige Stelle, die aus N
Urteilen eines macht; er fällt keines selbst. Je Akteur emittiert er eine maschinenlesbare
`UNIT_RESULT`-Zeile (Teilergebnis + Grund + Telemetriepfad), und bei mehr als einer Einheit trägt JEDE
akteursbezogene Log-Zeile (auch die modulinternen) als erstes Feld `unit=<callsign>` — bei genau einer
Einheit entfällt beides, weil es nichts zu unterscheiden gibt (und alte Regressions-Baselines
byte-identisch bleiben). Eine `.fbm`-Zeile `module <name>`
(Pflichtfeld je Block, `doc/mission-format.md`) wählt das Modul über die Registry (Name → Factory,
`std::unique_ptr<FBModule>`; heute registriert nur `modules/f16/FBF16ModuleRegistration.cpp` den Namen
`"f16"`). `FBMissionRunner.cpp`/`FBAppGym.cpp` inkludieren NIE einen konkreten Modul-Header — sie halten
alles über `FBModule`s generische Accessoren (`Autopilot()`/`FlightControl()`/`PilotSystem()`/
`Controls()`/`Displays()`/`AirDataSystem()`/`FlightPlan()`/`Telemetry()`/`SetRunway()`/
`SetGroundAsl()`/`ApplySetup(key,value)`, auf der Basisklasse selbst deklariert — CLAUDE.md's "FBCore →
Interface → Default → Override" jetzt auch für den Modul-ZUGRIFF, nicht nur dessen Verhalten). Der
Anfangszustand einer Unit ist reine Daten-Deklaration (`FBSpawn`: Position, Höhe-ODER-Boden, Kurs,
Speed) — KEINE getrennten Boden-/Luft-Codepfade, eine EINZIGE IC-Anwendung
(`FBMissionBoot.h::FBMissionSpawnActor`, das den fertigen Akteur liefert); `set <key> <value>`-Zeilen tragen Systemzustand als
Missionsdaten, der Runner reicht die rohe KV-Liste nur durch, das MODUL interpretiert seine eigenen
Schlüssel (`FBModule::ApplySetup`, unbekannter Schlüssel = Laufzeit-FAIL). Wegpunkt-Sequenzierung ist
Akteurs-Verhalten, nicht Runner-Buchhaltung: `systems/FBNavSystem::AdvanceWaypoint` sitzt im Modul.
`FBMission` ist missionsweite Daten (`name`/`runway`/`timeout`) plus eine LISTE von `FBMissionUnit`
(Callsign, Modul, `FBUnitTeam`, `FBSpawn`, `set`-KV, eigener `FBFlightPlan`) — ein Einzelflug ist der
Sonderfall „ein Block", kein zweiter Dialekt.

**Die Akteursliste WÄCHST zur Laufzeit — an genau einer Stelle: ein abgeworfener Store.** Er ist
strukturell dieselbe Einheit wie ein Jet (eigene FDM-Instanz auf seinem eigenen gepinnten Modell, eigenes
Modul aus derselben Registry, eigene Telemetriedatei, dieselben zwei Monitore), also gibt es keinen
zweiten Codepfad: `FBMissionBoot.h::FBMissionSpawnStore` ist derselbe Vier-Schritt-Spawn, dessen IC aus
dem TRÄGERZUSTAND kommt statt aus einer Missionsdatei (`FBFdmSpawn::Ballistic`: Position + Stationsversatz,
Trägerlage, Trägergeschwindigkeit an dieser Station inkl. ω × r; kein Trimm, kein erfundener Ejektor-
Impuls). Die Tick-Semantik ist die Bedingung dafür, dass das deterministisch bleibt: eine neue Einheit
wird am ENDE des Ticks angehängt, in dem der Abwurf kommandiert wurde, und erst im NÄCHSTEN gerechnet —
sonst hinge das Ergebnis davon ab, wann in der (threadverteilten) Step-Phase sie aufgetaucht ist. Die
Kapazität ist vorreserviert (eine je belegte Station), im Tick-Pfad wird nichts allokiert. Das ENDE einer
Waffe entscheidet derselbe `FBFlightMonitor` wie bei jedem Jet — für sie ist das die Detonation statt
eines Absturzes: der Lauf endet deswegen nicht, die Einheit wird stillgelegt (nicht aus der Liste
gelöscht: das würde Indizes verschieben), `UNIT_RESULT` nennt sie `IMPACT`.

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
             (`FBMissionSpawnActor`: die EINE deklarative IC-Anwendung für EINEN `unit`-Block, Boden ODER
             Luft, generisch über FBModuleRegistry, kein konkreter Modultyp — liefert den fertigen
             `units/FBSimUnit` inkl. eigenem FBMissionMonitor, sofern der Block Ziele hat; da nur DIESER
             Header `fdm/FBFdmBoot.h` nennen darf, ist er auch der einzige Produzent eines
             vollständigen Akteurs), FBTickPool.h/.cpp (die GYM-ONLY Lockstep-Worker, die die STEP-Phase
             des Ticks parallelisieren — nur von FBMissionRunner.cpp inkludiert, nicht in der Core-Lib,
             nie im WASM-Build, s.o. "Etappe 4"), FBModelRoots.h (die ZWEI JSBSim-Modellwurzeln eines
             Clients — das gepinnte read-only Submodul und FlightBox' eigene Assets, weil ein Modell, das
             das Submodul nicht hat, dort auch nicht liegen darf; WELCHE ein Modul braucht, sagt das
             Modul selbst, `FBModule::FdmModelVendored`), FBLogSinks/FBTelemetrySinks (die
             I/O-Sink-Implementierungen, inkl. FBBufferedLogSink = der Pro-Einheit-Logpuffer)
  core/      FBStore.h — der STORE-KATALOG als reine Daten (Kind/Schlüssel/JSBSim-Modell/Masse/
             Widerstandsfläche/Lebensdauer) plus `FBStoreRelease`: jede Zahl darin stammt aus dem
             eigenen gepinnten Modell des Stores oder ist per genannter Formel daraus abgeleitet (Mk-82:
             Masse = dessen `<emptywt>`, CdA = dessen eigene CDmin-Tabelle × dessen Flügelfläche) — eine
             Waffe hat am Pylon kein Verhalten, nur Masse, Widerstand und einen Modellnamen, ihr
             Verhalten IST das FDM, das sie beim Abwurf wird.
             FBState — der AVIONIK-BUS: kein flaches Feldbündel mehr, sondern ein Satz typisierter
             AUSGABEBLÖCKE (`FBAvionicsBlocks.h`: Platform, Env, AirData, RadarAlt, Nav, Cruise,
             FireControl, Ufc, Stores, Airframe, Warnings, Radar, Datalink, Bfm), je Block GENAU EIN
             Schreibersystem (im Blockkommentar benannt) und ein Kopf `{StampS, Status}`
             (`FBBlockStatus.h`) mit DREI Zuständen: `Invalid` (Zahlen bedeuten nichts — nie
             geschrieben oder Quellsystem aus/ausgefallen), `Valid`, `Held` (ABSICHTLICH eingefroren,
             letzte Werte + Zeitstempel der letzten echten Aktualisierung). Der dritte Zustand ist
             belegt, nicht erfunden: der echte Jet FRIERT mehrere CRUS-Rechenfelder bei ausgefahrenem
             Fahrwerk EIN, statt sie ungültig zu machen (`doc/f16/controls-commands.md`) — heute
             gleichermaßen das Radarbild zwischen zwei Sweeps, das Netzbild zwischen zwei Zyklen und
             die BFM-Schätzung jenseits ihres Extrapolationsfensters. Übernommen ist die SEMANTIK
             eines Multiplexbus-Jets (definierte Datengruppen, ein Erzeuger, Gültigkeitsflag), NICHT
             seine Adressierung/Wortpackung — der Transport bleibt eine typisierte Struktur per
             Referenz in EINEM Adressraum, kein string-indizierter Property-Tree. `FBStateBusTelemetry`
             veröffentlicht jeden Blockstatus als eigene Telemetriespalte (`blk_*`), weil ein
             gehaltener Wert sonst wie ein frischer aussieht. Dazu die KOMMANDOSEITE:
             `FBAvionicsCommand.h` (Ziel/Vorschlagswert → Quittung {Ergebnis, Grund}, die zwei
             Latenzklassen HOTAS/DED und der Ablehnungskatalog aus `doc/f16/controls-commands.md` §6
             plus zwei ausdrücklich EIGENE Gründe: `OutOfRange` — die Quellen dokumentieren keine
             Bereichsprüfung, FlightBox lehnt ab statt still zu klemmen — und `ChannelBusy`) und
             `FBCommandBus.h/.cpp` (feste Kapazität, keine Allokation: erzwingt Latenz, Kanalbelegung
             und die Manöver-Sperre für Kopf-nach-unten-Eingaben; zugleich FBTelemetrySource "cmd" und
             FBLog-Quelle `CMD_ISSUE`/`CMD_ACK`/`CMD_REJECT`). FBDatalinkTrack,
             FBRadarContact (der ANONYME Radarkontakt + FBIffReply — der
             bewusste Gegenentwurf zum Datalink-Track, s.u. "Kein Cheaten"), FBMode,
             FBMasterMode, FBFlightPlan/FBRunway/FBSpawn (Wegpunkt-/Runway-/
             deklarativer-Spawn-Value-Types für FBPilot/den Orchestrator), FBTeam (`FBUnitTeam` —
             die Fraktion, im core/, weil sie BEIDES ist: Missionsdaten und Welt-Identität), FBMissionFile
             (`.fbm`-Parser: missionsweite Daten + eine LISTE von `FBMissionUnit`-Blöcken, je
             Callsign/`ModuleName`/`FBUnitTeam`/`FBSpawn`/`SetKV`/`FBFlightPlan`), FBMissionMonitor
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
               (`FBLogSinks.h/.cpp`), core/systems/render/world/fdm rufen nur die Fassade. Neben der
               Zeit trägt die Fassade die UNIT-Attribution (`FBLog::SetUnit`/`FBLogUnitScope`, vom
               Client um den akteursbezogenen Teil seiner Schleife gelegt): ist sie gesetzt, führt jede
               Zeile `unit=<callsign>` als erstes Feld — leer bei einer Einzel-Einheit, s.o.
               Der KONTEXT der Fassade (Zeit, Unit-Label, ein optionaler `ThreadSink_`) ist
               `thread_local`, die KONFIGURATION (Sink, Level) prozessweit — das macht sie im
               Gym-Parallelpfad thread-fest, ohne die Fassade aufzugeben (s.o. "Etappe 4").
               FBTelemetry (`FBTelemetry.h/.cpp`) — periodisch gesampelter Zustand (Zeitreihe, Schema).
               Klassen DEKLARIEREN sich als `FBTelemetrySource` (`DeclareTelemetry`/`SampleTelemetry`);
               der EINE `FBTelemetryBus` sampelt jede registrierte Source pro Tick in genau eine Zeile
               (Registrierungsreihenfolge = Spaltenreihenfolge) und reicht sie an einen injizierten
               `FBTelemetrySink` (z. B. `FBCsvTelemetrySink`, app/). FBAirDataSystem/FBPilot/
               FBFlightControl/FBAirframeControls implementieren `FBTelemetrySource` direkt;
               FBFdmTelemetrySource (fdm/, s.u.) trägt die rohe FDM-Pose bei.
             FBWeaponUplink (die Lenkfunk-Werttypen: was ein Schütze einer von ihm gestarteten Waffe
             sendet — eine SCHÄTZUNG seines eigenen Radars mit deren Alter, nie eine Wahrheit),
             FBAtmosphere (ISA-Dichte/Staudruck, header-only — für die zwei Stellen, die über Luft
             rechnen müssen, in der sie gerade nicht fliegen: die Startbereichs-Integration und der
             Verstärkungsplan des Raketen-Autopiloten)
  math/      Value-Math (FBMat4 — Value-Types, Operatoren inline im Header)
  render/    FBRenderer (Orchestrator: Device/Swapchain/Targets, JEDE Begin/EndRenderPass-Grenze,
             die Encode-Reihenfolge — Pass-Topologie ist ein Vertrag, kein Stage-Split darf sie
             vermehren), FBCamera, FBMips, FBChunkMesh/FBChunkVtx, FBEphemeris, FBGpu/FBFrameContext
             (Device-Handle bzw. geteilter Frame-Zustand, den jede Stage bekommt; FBCamera trägt
             zusätzlich `FBCameraBasisEcef` — die EINE Lage→ECEF-Basis, die native und WASM teilen,
             auf `core/FBGeodesy.h`s `FBGeoToEcef`/`FBEnuAxesEcef`), FBDrawStage
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
             zusätzlich die von der Core-Lib GEBORGTE Unit-Registry — `SetUnits`, s.u. units/),
             FBTilesElevation (der Elevation-Hook-Provider auf fb_stream_ground, s.o. — NICHT
             Teil der Core-Lib)
  units/     FBUnit-Basisschnittstelle + FBSimUnit + FBUnitRegistry (Details s.u., nach dem
             Verzeichnisbaum). `FBUnitKind` unterscheidet Aircraft und Weapon: eine abgefeuerte Waffe
             ist strukturell dieselbe Einheit (eigene FDM-Instanz, eigenes Modul, gleiche Monitore) —
             die KIND-Unterscheidung existiert nur für die zwei Dinge, die dem BESITZER gehören: ihr
             physikalisches K.O. ist eine Detonation und beendet den Lauf nicht, und Luft-Luft-Sensoren
             suchen keine Bomben.
  systems/   die generischen, airframe-agnostischen System-Slots eines Moduls — Interface + Default
             in EINER Klasse, ein Modul überschreibt per Ableitung (Zahlen-Tuning bleibt Preset/
             Config, keine leere Ableitung dafür):
               FBAutopilot (Guidance), FBFlightControl (FBW-Innenschleife),
               FBAirDataSystem (CAS/Mach/G-Last, FPM-Richtung als Ground-Track/Flightpath-Angle aus dem
               ENU-Geschwindigkeitsvektor), FBRadarAltimeter (AGL aus DER SELBEN DEM-Quelle, die die App
               schon für `SetAgl` auflöst — keine zweite Terrain-Abfrage; zugleich der REFERENZFALL für
               `Invalid`: stromlos publiziert die Box keine 0 ft, sie macht ihren Block ungültig, und
               jeder Konsument muss sagen, was er ohne sie tut — belegt in `doc/f16/controls-commands.md`
               §6.4), FBWarningSystem (der Warnsatz als Bitmaske; macht die Gültigkeitsköpfe
               konsequent: eine Warnung, deren Quellblock ungültig ist, meldet sich als INHIBITED statt
               als „keine Warnung"), FBNavSystem (ein Steerpoint +
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
               FBDatalinkSystem (eigene Datei, `FBDatalinkSystem.h/.cpp`): der Comms/Datalink-Slot —
               das generische KOOPERATIVE Netz (MIDS/Link-16, `doc/f16/datalink-iff.md`) und die erste
               echte Cross-Unit-Wahrnehmung. Kein Sensor im Suchsinn: jeder Teilnehmer sendet seine
               EIGENE Navigationslösung + Identität, jeder in Reichweite empfängt sie. Der Default liest
               die geborgte `units/FBUnitRegistry` (nur dort und im künftigen Radar landet sie), filtert
               nach Fraktion, verlangt einen SENDENDEN Absender (dessen `FBUnitSignature`, im
               Tick-Barriere-Snapshot publiziert), begrenzt auf min(Terminal-Reichweite, Funkhorizont der
               beiden Höhen), aktualisiert das Bild nur im 1-Hz-NETZZYKLUS (`kNetPeriodS`) und hält einen
               nicht mehr empfangenen Track 3 Zyklen, bevor er fällt. Ergebnis sind
               `core/FBDatalinkTrack`s fester Kapazität IN FBState — mit Alter, nie „live". Zwei
               Schalter, weil das echte Terminal zwei hat: POWER (`SetPowered`, aus = blind UND stumm)
               und XMT (`SetTransmit`, aus = EMCON: empfängt weiter, wird nur nicht mehr gehört).
               `AcceptContact` ist der Override-Punkt (F-16: `modules/f16/FBF16Datalink`);
               FBRadarSystem (eigene Datei, `FBRadarSystem.h/.cpp`): der Sensors-Slot — das generische
               AKTIVE Luft-Luft-Radar, das bewusste Gegenstück zum Datalink (kooperativ = Identität
               geschenkt, aktiv = nur ein Echo). Ein `FBRadarScanVolume` (Azimut- und Elevations-Fenster
               RELATIV ZUR NASE — volle Roll/Pitch/Yaw-Rotation, das Volumen kippt mit dem Jet —,
               Entfernungstor, Frame-Zeit, Auto-Lock- und Single-Target-Flags) IST der Modus; ein Ziel
               wird erst nach `kHitsToFirm` aufeinanderfolgenden Looks zum Track (Aufbau kostet ZEIT) und
               nach dem Verlassen des Volumens noch `max(kMinCoastS, kCoastFrames·FrameS)` gehalten
               (Coast: eingefrorene Geometrie, hochlaufendes `LookAgeS`), bevor er fällt. Ergebnis sind
               `core/FBRadarContact`s fester Kapazität IN FBState — ANONYM (s.u. „Kein Cheaten"), Identität
               nur über IFF Mode 4 (`FBIffReply`: friendly | no-reply | not-interrogated, kein „hostile";
               die APX-113-Transponderhälfte wird in `FBUnitSignature` publiziert wie XMT). Terrain-
               Maskierung ist BEWUSST NICHT modelliert (im Header dokumentiert): Luft-Luft-Sichtlinie ist
               frei, Maskierung bräuchte einen DEM-Raymarch je Kontakt je Look. `ActiveVolume()` ist DER
               Override-Punkt — ein ganzer Modus-Satz ist nichts als eine Auswahl unter Volumina, und ein
               Lock nichts als ein anderes Volumen (F-16: `modules/f16/FBF16Fcr`);
               FBStoresSystem (`.h/.cpp`): der Stores/SMS-Slot — Stationsinventar, Master-Arm-
               Verriegelung, der EINE Abwurfpfad. Eine belegte Station ist eine JSBSim-PUNKTMASSE auf dem
               Trägerflugzeug und die Summe der Widerstandsflächen eine JSBSim-EXTERNAL-FORCE, d.h.
               Masse/Schwerpunkt/Trägheit und Zusatzwiderstand einer Zuladung sind Physik der Engine,
               nicht Rechnung dieser Klasse (vendor bleibt read-only: beide Mechanismen werden über die
               Modell-eigenen APIs zur Laufzeit bestückt, kein Modell-XML wird gepatcht). Sie SPAWNT
               nichts — ein Abwurf legt einen `FBStoreRelease` (core/FBStore.h) in eine Warteschlange,
               die der Besitzer leert, weil das Erzeugen eines FDM hinter fdm/FBFdmBoot.h liegt.
               Ausgelöst wird ausschließlich über den Kommandobus (`WeaponRelease`), also ablehnbar:
               Master Arm nicht ARM / Gewicht auf dem Fahrwerk = hardware_precedence, keine belegte
               Station = out_of_context;
               FBSystemSlots.h — Input/HOTAS, Propulsion, Weapons, Defensive: Interface
               + NoOp-Default für die restlichen F-16-Systemkategorien, ein Modul füllt sie bei Bedarf
               per Ableitung (Comms/Datalink und Sensors sind wie Displays aus dieser Datei
               herausgewachsen, s.o.).
               Sensoren SCHREIBEN/Displays LESEN den geteilten FBState; Waffen/
               Defensiv erhalten eine geborgte FBWorld-Referenz (nie global).
               FBPilot (`FBPilot.h/.cpp`): die Missions-Ebene ÜBER Guidance/FlightControl — FBAutopilot
               (Manöver) und FBFlightControl (100 Hz, die Hände) bleiben unangetastet; FBPilot entscheidet
               WOHIN (FBFlightPlan-Wegpunkte, optionale FBRunway) und gibt das im ~10-Hz-Entscheidungstakt
               (vom Modul gedrosselt wie jeder andere Slot) als `FBPilotCommands` aus: eine Guidance-
               Anfrage an FBAutopilot (`FBPilotGuidance::None/Manual/Direct` — None = "AP
               unangetastet lassen") plus optionale (`std::optional`) Airframe-Kommandos an
               FBAirframeControls. Avionik bedient der Pilot AUSSCHLIESSLICH über den Kommandobus
               (`core/FBCommandBus`) — er hält keine Systemzeiger; was er im Flug eingibt, ist sein
               BRIEF (`brief_*`-Missionszeilen, `doc/mission-format.md`), Eingabe für Eingabe, in der
               Latenzklasse der jeweiligen Bedienung und mit dem Risiko, abgelehnt zu werden. Ohne
               Brief bedient er nichts. Die Phasen-Zustandsmaschine (Idle/Preflight/Takeoff/Climb/Route/
               Approach/Flare/Rollout/Shutdown, doc/f16/procedures-*.md) ist das Prozedur-Gerüst; Run()
               ist der Override-Punkt (analog FBAutopilot::Run). Die Phase **Bfm** (Missionsdaten:
               `set task bfm`) ist die einzige mit EIGENEM Regelgesetz statt eines Autopilot-Modus: sie
               fliegt Manual-Stick (wie Takeoff/Flare/Rollout), rollt den Auftriebsvektor auf einen
               Zielpunkt und zieht das g, das den Geschwindigkeitsvektor in kBfmTurnTimeS auf ihn dreht
               (+ dem Gravitationsanteil ENTLANG der Auftriebsachse, cos(roll)cos(pitch) — NICHT
               1/cos(roll): eine 90°-Schräglage kostet nur ihr eigenes Kurven-g, die Nase fällt, und
               genau das macht den Energiekampf möglich). Verfolgungsart aus der Geometrie: Lead
               (Winkel gewinnen), Lag (Overshoot verhindern — Aimpunkt hinter UND über ihm, der High
               Yo-Yo), Pure dazwischen; Reichweiten-/Energie-Management über einen Closure-FAHRPLAN
               (gewünschte Annäherungsrate ∝ Restentfernung) plus Speed-Matching auf die GESCHÄTZTE
               Zielgeschwindigkeit. Alle Zahlen sind virtuelle Hooks (F-16: Corner-Speed 380 KCAS,
               gemessen via `make -C sim test-corner`).
               FBBfmTrack (`FBBfmTrack.h/.cpp`): das Bild, gegen das diese Phase regelt — ausschließlich
               aus dem Radar-BLOCK des FBState (Kontakte + Lock-Index, Kopf zuerst) gebaut (kein FBWorld, keine Registry, kein
               Datalink-Track im Include-Baum): geschätzte Zielposition + Geschwindigkeitsvektor aus
               aufeinanderfolgenden Looks, extrapoliert solange ein Lock fehlt, danach nur noch das
               zuletzt GEMESSENE Datum. Zugleich FBTelemetrySource "bfm" (Aspekt/ATA/Range/Closure/
               eigene Energiehöhe + die Integrale Lock-Sekunden und Kontrollpositions-Sekunden) — die
               Fitness-Kanäle der späteren evolutionären Runde, alle aus EIGENER Perspektive
               berechenbar. FBAirframeControls (`.h/.cpp`): das
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
             Runner, gehalten von `units/FBSimUnit` (ein `std::unique_ptr<FBFdm>` pro Einheit). Module und
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
             **AUSSENLASTEN** laufen über zwei Modell-eigene JSBSim-Mechanismen, die dieser Adapter zur
             LAUFZEIT bestückt statt ein Modell-XML zu patchen (vendor bleibt read-only, Prinzip 1):
             `AddStorePointMass`/`SetStorePointMassLbs` treibt FGMassBalances eigene AddPointMass-API (die
             `<pointmass>`-Mechanik, von der die F-16 genau eine deklariert — ihren Piloten), also kommen
             Masse, Schwerpunkt UND Trägheitsmomente einer Zuladung aus der Engine; `SetStoresDrag` legt
             eine EIGENE, `fb-stores` benannte `<external_reactions>`-Kraft an (CdA·qbar entgegen der
             Körper-x-Achse, am Schwerpunkt der belegten Stationen), statt eine vom Modell für einen
             anderen Zweck deklarierte Kraft umzudeuten. Ohne Zuladung wird keins von beidem je angelegt —
             ein sauberer Jet ist bit-identisch zu einem, der nie von Stores gehört hat.
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
  modules/   FBModule-Basisschnittstelle (`Run(fb_fdm_state&, dt, const FBUnitRegistry*, const FBWorld*)`
             + `SetUnitIdentity(id,team)` [Boot-Wiring: wer die Einheit IST, für Systeme die andere
             Einheiten beobachten] PLUS die generischen
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
                         Ableitung eingehängt wird. FBF16Datalink (`.h`) ist die erste solche Ableitung
                         mit echtem Inhalt: das MIDS-LVT-Terminal — Link-16-Reichweite (~300 nm LOS)
                         statt des generischen Platzhalters und der TNDL-Kontaktfilter der HSD
                         (FR ON = alle Freundlichen / FL ON = nur Flight Leads / FR OFF) als Override von
                         `FBDatalinkSystem::AcceptContact`; Missions-Schalter `datalink`,
                         `datalink_xmt`, `datalink_filter`, `datalink_range_nm`. FBF16Fcr (`.h/.cpp`)
                         ist die zweite: der AN/APG-68 als MODUS-SATZ über `FBRadarSystem::ActiveVolume`
                         — CRM (Power-up-Suche, ±60°/±10,5°, 40 nm, kein Auto-Lock) plus die vier
                         ACM-Sub-Modi (HUD Scan ±15°/±10°, Boresight ±5°-Kegel, Vertical Scan
                         ±5°/−13°…+47°, Slewable ±10° um den Cursor; alle 10 nm, alle mit Auto-Lock des
                         NÄCHSTEN festen Tracks) und STT als eigenes Volumen (Gimbal ±60°, 0,1 s Frame,
                         Single-Target: der Lock kostet jedes andere Trackfile). Missions-Schalter
                         `fcr_mode`, `fcr_range_nm`, `fcr_slew_az`/`_el`, `iff_xpdr`,
                         `iff_interrogator`. Die Winkel/Frames sind DEKLARIERTE Modellparameter (die
                         doc/f16/-Quelle gibt die Taxonomie, keine Zahlen) — im Header als solche
                         ausgewiesen, nicht als Zitat verkleidet. KEINE HUD-Symbologie: `doc/f16/hud-
                         symbology.md` kennt weder TD-Box noch Locked-Target-Symbol, also wird keine
                         erfunden. Trägt drei F-16-eigene HUD-Platzhalter (eigene
                         `.h/.cpp`, kein genereller Systemslot, da airframe-spezifisch): FBF16FireControl
                         (das FEUERLEITSYSTEM: der "B"-Range-Provider — Slant-Range aus Distanz +
                         Höhendifferenz zur Steerpoint-Elevation, Pythagoras — PLUS der
                         Luft-Luft-STARTBEREICH der gewählten Waffe, aus einer Vorwärtsintegration ihrer
                         Leistungstabelle [core/FBStore.h's FBWeaponPerf, bewusst eine GRÖBERE Kopie
                         dessen, was das Waffenmodell wirklich tut — ein Feuerleitrechner rechnet aus
                         einer gespeicherten Tabelle, und der Fehler dieser Vorhersage ist eine echte
                         Eigenschaft jedes Schusses, die die Abfangmission misst] gegen die aktuelle
                         Radargeometrie: Raero/Rtr/Rmin + Zeit bis Suchereinschaltung und bis Einschlag,
                         alles im FireControl-Block; dazu ein EIGENER systems/FBBfmTrack, der aus dem
                         gelockten Kontakt die Zielschätzung baut, mit der der SMS eine Runde programmiert
                         und die er danach als Lenkfunk aussendet), FBF16Ufc (ALOW-Floor + gewählte
                         Steerpoint-Nummer), FBF16Sms (der SMS: NUR die Pylon-Geometrie dieses Musters
                         — neun Stationen, verankert an den Referenzen, die das Modell selbst hergibt
                         [Tank-Butt-Line ±65 in für 4/6, halbe Spannweite 180 in für die Spitzen,
                         CG-Station längs, weil doc/f16/weapons.md §4.5 die Stationsdaten selbst als T4
                         markiert] — alles Verhalten ist systems/FBStoresSystem). FBF16Max7456 (eigene Datei,
                         `.h/.cpp`): der MAX7456-CHIP-spezifische Hook (Interlace-Jitter,
                         Helligkeitskurve, Sync-Artefakte, …) — heute ein echter, von FBF16Module
                         gehaltener NoOp-Override-Punkt, getrennt vom generischen Font-System in
                         render/FBHudFont.h
  modules/stores/        FBStoreModule (`.h/.cpp`) + FBStoreModuleRegistration.cpp: das Modul, mit dem
                         ein ABGEWORFENER Store fliegt — ein vollwertiges FBModule, dessen Systemslots
                         alle Default/NoOp sind (eine Mk-82 hat weder Autopilot noch Pilot noch
                         Anzeigen) und dessen Run() genau eines tut: das eigene FDM in 100-Hz-Substeps
                         integrieren, ohne je einen Steuerkanal zu schreiben. Damit ist die Flugbahn die
                         Aerodynamik des gepinnten Modells plus Schwerkraft und sonst nichts. EINE
                         Klasse, N Registry-Namen: jeder UNGELENKTE Katalogeintrag aus core/FBStore.h
                         registriert sie unter seinem eigenen Schlüssel (heute `mk82`), Modellname = der
                         des Stores. Eine GELENKTE Waffe ist ein anderes Modul (modules/missile, s.u.),
                         kein Flag auf diesem — welche der beiden ein Eintrag ist, sagt sein `Guided`-Flag,
                         gelesen an genau je einer Stelle in den beiden Registrierungsdateien.
  modules/missile/       Das Gegenstück für einen LENKFLUGKÖRPER, heute die AIM-120 (`aim120`):
                         FBMissileModule (`.h/.cpp`) + FBMissileModuleRegistration.cpp, plus die drei
                         Slots, die eine Bombe nicht hat und die hier ECHTE Systeme sind:
                         FBMissileSeeker (`.h/.cpp`, eine systems/FBRadarSystem — eigenes aktives Radar,
                         ±10° geschwenktes Sichtfeld, ±45°-Gimbal nach dem Lock, aus bis die Lenkung es
                         bei der Aktivierungsentfernung einschaltet; das EINZIGE hier, das die
                         Unit-Registry sieht), FBMissileGuidance (`.h/.cpp`, eine systems/FBPilot-
                         Ableitung — Proportionalnavigation N=4 mit Herleitung im Header, darunter zwei
                         staudruck-geplante Querbeschleunigungs-Regelkreise auf Beschleunigungsmesser +
                         Kreisel, Ausgabe = RUDERKOMMANDOS durch FBAutopilot(Manual)/FBFlightControl in
                         FBFdm::SetControls; eigene Telemetriespalten `msl_*`, da der Bus pro Einheit
                         aufgebaut wird und ein Jet-Trace sich dadurch um keine Spalte ändert) und
                         FBMissileUplink (`.h/.cpp`, eine systems/FBDatalinkSystem — empfängt die
                         Lenkfunk-Aussendung SEINES Schützen aus dessen units/FBUnit-Signatur und
                         veröffentlicht sie als den einen Datalink-Track auf seinem eigenen Bus).
                         Drei Phasen (INERTIAL/MIDCOURSE/TERMINAL), Übergang durch ERFASSUNG, nicht durch
                         Timer; verliert der Schütze den Lock, fliegt die Runde auf ihrer letzten
                         Information weiter. Ihr JSBSim-Modell ist FlightBox-EIGEN
                         (`sim/assets/aircraft/aim120` — das gepinnte Submodul hat keine AMRAAM und ist
                         read-only, Prinzip 1), erreicht über `FBModule::FdmModelVendored() == false`
                         und app/FBModelRoots.h.
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
Callsign/Kind-Enum `Aircraft/…`/`FBUnitTeam` aus `core/FBTeam.h` —, geodätische Pose, publizierte
Emissions-Signatur `FBUnitSignature` [heute: sendet das Datalink-Terminal, antwortet der IFF-Transponder
— was FREMDE Sensoren an dieser Einheit wahrnehmen dürfen], `virtual void Run(dt, const FBUnitRegistry*, const FBWorld*)`) und, darauf
aufbauend, **`FBSimUnit` — EINE simulierte Einheit als EIN
Objekt**: sie BESITZT ihre `FBFdm` und das `FBModule`, das sie fliegt (in dieser Deklarationsreihenfolge,
damit die Zelle das Modul überlebt, das sie nur borgt), hält den geteilten `fb_fdm_state`, die
aufgelöste Boden-ASL, die Telemetrie-Source + den eigenen `FBTelemetryBus` und BEIDE unbestechlichen
Richter (`core/FBFlightMonitor` + optional `core/FBMissionMonitor` — letzterer nur, wenn die Mission
dieser Einheit Ziele gegeben hat). `Run(dt, world)` taktet das Modul, das seinerseits FDM und
Systemslots cycelt; die Pose wird einmal pro Tick PUBLIZIERT (`PublishPose`, die Barriere nach allen
`Run()`s) — `GetPose()` zeigt damit immer den zuletzt ABGESCHLOSSENEN Tick, die Snapshot-Regel für
cross-unit-Lesezugriffe. Eine `FBActorList` (`std::vector<std::unique_ptr<FBSimUnit>>`) ist die
Besetzung einer Mission, die jeder Client hält. Das ersetzt das, was vorher als verstreute Locals im Missions-Runner und
als acht file-scope Statics im Browser-Client stand — der frühere `FBOwnshipUnit` (nur eine Pose-Sicht
auf `fb_fdm_state`) geht darin auf, es gibt keinen zweiten Unit-Begriff mehr. **Die Anti-Cheat-Struktur
bleibt:** ein FBSimUnit lässt sich nur aus einer bereits gespawnten `FBFdm` bauen, und die gibt es nur
über `fdm/FBFdmBoot` (app/-only) — `grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' src/systems
src/modules` bleibt ohne Treffer, das Modul sieht die Richter nach wie vor nicht (es wird von IHNEN
beobachtet). **`FBUnitRegistry`** ist die Liste „wer existiert" selbst: geborgte `const FBUnit*` in
Registrierungs- = Missions-Deklarationsreihenfolge, keine Ownership, kein Welt-Mutationspfad. Sie lag
früher als Member in `world/FBWorld` — also auf der RENDERER-Seite des Lib/Client-Splits, weshalb
`fb-gym` (linkt kein `world/`) jedem Modul `world = nullptr` reichte und ein simulierter Sensor im
einzigen Client, der die Missionsschleife wirklich fährt, nie eine andere Einheit hätte sehen können.
Die Besetzung ist Simulationszustand, keine Rendering-Sache: sie steht deshalb in der Core-Lib, der
Client (Runner wie Browser) besitzt genau EINE und reicht sie beim Tick durch (`FBSimUnit::Run` →
`FBModule::Run` → Sensor-Slot); `FBWorld` BORGT sie nur noch (`SetUnits`/`Units()`) für die Zeichenseite.
Der `const FBWorld*` bleibt getrennt daneben: er ist die TERRAIN-Seite, die ein Sensor braucht
(Maskierung), nicht die Einheiten-Seite. KI-Einheiten (freundlich/feindlich/neutral) hängen sich später
an dieselbe Schnittstelle, sobald sie real existieren.

**Telemetrie bei N>1:** eine CSV je Einheit, feste Spaltenzahl — der PRIMÄRE Akteur behält den
kanonischen Namen `telemetry.csv`, jeder weitere bekommt `telemetry_<callsign>.csv` (das Callsign der
`unit`-Zeile, vom Parser auf `[A-Za-z0-9_-]` beschränkt, damit es dateisicher ist). Eine breite Zeile mit
Präfix-Spalten scheidet aus: das Spaltenset eines Akteurs folgt SEINEM Modul, eine geteilte Zeile
würde entweder alle Module in ein Schema zwingen oder den Header von der Besetzung der Mission
abhängig machen; die Datei-je-Einheit braucht bei N=1 keinen Sonderfall (die Zeilen bleiben
byte-identisch).

**Coding-Style: an JSBSim orientiert** (unser Code fügt sich in dessen Ökosystem): Klassen `FB`-Präfix
(analog `FG` — `FBFlightControl`, `FBRenderer`), PascalCase-Methoden (`Run()`, `GetLoadFactor()`),
Member PascalCase, ein `namespace FlightBox`, Header-Guards, Klasse-pro-Datei `FBName.h/.cpp`,
Getter inline im Header. JSBSims LGPL-Banner nicht kopieren — unsere Dateien tragen unsere Lizenz.

## Engineering-Konventionen

- **Build nur über Make-Targets** — jedes Projekt trägt sein eigenes Makefile: `sim/` baut die CC
  (`make -C sim wasm` | `worker` | `core-lib` | `native` | `gym` | `test-monitor` | `test-fdm` |
  `test-corner` | `image` | `up`), `tiles/` den
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
  (zeilenbasiert, zero-dependency, `doc/mission-format.md` — missionsweit sind `name`/`timeout` Pflicht
  und `runway` optional, je `unit`-Block `module`/`spawn` Pflicht), geparst von `core/FBMissionFile.h` (reine Text→`FBMission`-Funktion,
  kein File-I/O — das macht die App). Orchestrator: `FBRunMission` (`app/FBMissionRunner.cpp`, geteilt
  von `fb-gym` und `gpu_native --mission`) — vier Schritte, keine Missions-Spezifika im Code (s.o.
  "Der Missions-Runner ist reiner Orchestrator") — löst `module` über `FBModuleRegistry` auf, spawnt
  die deklarative `FBSpawn` (Boden ODER Luft, EINE IC-Anwendung — `FBMissionBoot.h`) und treibt die
  Sim-Sekunden so schnell wie möglich (Prinzip 4) — `fb-gym` ist reines JSBSim, KEIN Renderer/
  GPU-Gerät; native `--interval` schaltet periodische Beweis-Frames über einen GPU-freien Hook dazu
  (Renderer bleibt Bolt-on, nie Abhängigkeit der Physik-/Terminierungs-Logik). Terminierung
  SUCCESS/FAIL/CRASH/TIMEOUT → Exit-Codes 0/1/2/3 (beide Monitore, s.u. "Kein Cheaten", kombiniert),
  worauf der Regelkreis branch. Telemetrie je Lauf in `--out/`: `telemetry.csv` (10 Hz, feste
  Spaltenzahl, inkl. `fuelLbs` — FGPropulsion-Tanksumme, der Blockgültigkeit `blk_*`, dem Warnsatz
  `warn_*` und dem Kommandostrom `cmd_*`) + `events.log` (`t=SEK EVENT key=val`, greppbar, inkl.
  `cmd CMD_ISSUE`/`CMD_ACK`/`CMD_REJECT`) — das Analyse-Werkzeug für die Pilot-KI, kein Produktionspfad.
  Neue Quellen werden IMMER hinten angehängt (`units/FBSimUnit::StartTelemetry`), damit keine je
  gemessene Spalte ihre Position verliert.
- **Kein Cheaten:** ZWEI unbestechliche, Runner-/App-eigene Instanzen, nie vom Modul gesehen, nie
  vermischt — zwei Fragen, nicht eine. `core/FBFlightMonitor` entscheidet K.O. (Absturz/LOC): rein
  physikalisch, modul-agnostisch (kennt keine Flugzeug-Typen, keine deklarierten Zahlen; Struktur-/
  Gear-Wahrheit kommt aus dem gepinnten JSBSim-Modell selbst). `core/FBMissionMonitor` (dessen
  Geschwister) entscheidet das MISSIONS-Urteil (SUCCESS/FAIL/TIMEOUT) aus der Missionsdatei selbst:
  Wegpunkte erreicht (gegen die EIGENE, unveränderliche Kopie von `FBFlightPlan`, nie das Modul-eigene
  mutierte Exemplar — reine Positions-Beobachtung, keine Modul-Selbstauskunft), Bodenkontakt abseits
  der zugewiesenen Runway (RESULT FAIL, nicht CRASH), Timeout. Beide werden von JEDEM Client gefüttert,
  der eine Sim-Schleife fährt — `FBMissionRunner` (fb-gym/gpu_native `--mission`) genauso wie der
  WASM-App-eigene Frame-Loop (`FBAppWasm.cpp`) — je EINE Definition, kein zweiter Paralleltest. Bei
  mehreren Einheiten gibt es je Einheit ein eigenes Paar (das MISSIONS-Urteil ist per Akteur, das
  Gesamturteil eine reine Kombination im Runner — s.o.), nie einen geteilten Richter für die Besetzung.
  Piloten/Module wirken NUR über die simulierten Systeme (`fcs/*-cmd-norm` via FBFlightControl/
  FBAutopilot, FBAirframeControls für Gear/Brakes/Steer/Speedbrake/Engine, Throttle, Tank-Füllstand via
  `FBFdm::SetFuel*`, Waffenauslösung via Kommandobus -> `FBStoresSystem::Release`) — der einzige
  State-Schreiber (JSBSim-IC/Trim) ist der App-eigene Boot-Spawn (`FBMissionBoot.h::FBMissionSpawnActor`
  und, für einen abgeworfenen Store, `FBMissionSpawnStore` im selben Header, plus die gleichrangigen
  App-Boot-Pfade `FBAppWasm.cpp`s `?ap=manual` und dedizierte Test-Harnesses). Auch eine Waffe kann sich
  deshalb nicht selbst in die Welt setzen: das Modul legt einen Datensatz in eine Warteschlange, der
  BESITZER spawnt. Die IC-Abschottung ist STRUKTURELL, nicht bloß
  grep-belegt (s. `fdm/`-Absatz: privater Lade-Konstruktor, Friend `FBFdmBoot`, eigener Header) —
  `systems/` und `modules/` erreichen die IC nicht und sehen `FBFlightMonitor`/`FBMissionMonitor` nie.
  Und die Gegenrichtung, seit es Cross-Unit-Wahrnehmung gibt: Piloten **sehen** nur über simulierte
  SENSOREN. Die Einheiten-Registry (`units/FBUnitRegistry`, die Welt-Wahrheit „wer existiert wo") reicht
  ausschließlich bis zu den SENSOR-Slots des Moduls — heute genau zwei, `systems/FBDatalinkSystem`
  (kooperativ) und `systems/FBRadarSystem` (aktiv);
  sie steht in KEINER Pilot-Signatur (`FBPilot::Run` trägt seit dieser Etappe auch keinen
  `const FBWorld*` mehr, den nie jemand las) und in keinem Member. Was ein Pilot über andere Einheiten
  weiß, steht in `FBState` — das, was die Sensoren dort HINEINGESCHRIEBEN haben, mit ihrer Reichweite,
  ihrem Scanvolumen, ihrem Netzzyklus und ihrem Alter. Grep-Beleg: `#include "FBUnitRegistry.h"` und
  `.Units()` erscheinen in `src/systems`/`src/modules` in GENAU ZWEI Dateien (`FBDatalinkSystem.cpp`,
  `FBRadarSystem.cpp`); sonst kommt der Typ dort nur als Vorwärtsdeklaration/Parameter vor
  (`FBModule::Run` → `FBF16Module::Run`). Die zweite Hälfte dieser Grenze ist der Kontakt SELBST:
  `core/FBRadarContact` trägt Range/Bearing/Az/El/Closure und eine radar-eigene Tracknummer — **keine
  Unit-Id, kein Callsign, kein Team**. Die Registry weiß, wer da fliegt; das Radar darf es nicht
  durchreichen. Die einzige legitime Identitätsquelle ist IFF Mode 4, und sie ist ZWEIWERTIG (gültiger
  Reply = friendly, kein Reply = unbekannt) — `FBIffReply` hat gar keinen Wert „hostile".
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
