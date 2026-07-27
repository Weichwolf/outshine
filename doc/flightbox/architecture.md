# Architektur

Der Grundriss. Wer was besitzt, was gegen was linkt, und wo eine Datei hingehört.

## Der Prozess

```
fb-tiles (Server: weltweit DEM/OSM/Luftbild)  ──HTTP──▶  Command Center (Client)
                                                          = JSBSim + FBW + Autopilot (Pilot/Mission|Manual)
                                                            + WebGPU-ECEF-Renderer + HUD
                                                            als EIN Prozess (WASM-Browser | native CLI)
```

Nichts ist vorgeladen — jede Kachel on-demand. Daraus folgt: **jeder Punkt der Erde ist ein gültiger
Start.**

## Core-Lib + drei Clients

FlightBox Core ist eine **reine Bibliothek**. `sim/`s `core-lib`-Target baut den Simulator selbst zu
einem nativen Archiv `build/libfbcore.a`:

| Enthalten | Nicht enthalten |
|---|---|
| `core/`, `fdm/`, `systems/`, `modules/` (inkl. F-16), `units/`, der `.fbm`-Parser, libJSBSim | `render/`, `world/` (Tile-Streaming), `app/FBTickPool` |

Genau **eine** Ausnahme: `render/FBHudGeometry.cpp` — reine CPU-Vertexlisten-Mathematik ohne
WebGPU-/Dawn-Include, die `systems/FBDisplaySystem` strukturell braucht.

WASM ist ein anderes Toolchain-Target (emcc/wasm32) und kompiliert dieselbe Quelldatei-Liste
zwangsläufig selbst nach. Das ist Cross-Compile, keine Duplikation der Architektur.

Drei Clients linken bzw. kompilieren dagegen:

| Client | Quelle | Target | Rolle |
|---|---|---|---|
| **`fb-gym`** | `app/FBAppGym.cpp` + `app/FBMissionRunner.cpp` | `make -C sim gym` | headless: Mission rein → Telemetrie raus. **Kein Dawn-/wgpu-Symbol im Binary** (per `nm` verifiziert). Der Missions-Kern. |
| **`gpu_native`** | `app/FBAppNative.cpp` | `make -C sim native` | Referenz-Renderer und Frame-Orakel. `--mission --interval` erzeugt PNG-Beweisframes über einen GPU-freien Tick-Hook auf derselben `FBRunMission`-Schleife. Ohne `--interval` headless. |
| **wasm** | `app/FBAppWasm.cpp` | `make -C sim wasm` | der Browser. |

`fb-gym`-Optionen: `--mission FILE [--out DIR] [--timeout N] [--threads N] [--elev tiles|const|swiss]`.
`--threads` ist **gym-only**.

## Verzeichnisse

| Verzeichnis | Zuständigkeit | Doku |
|---|---|---|
| `sim/src/app/` | Einstiegspunkte, App-Lifecycle, Missions-Orchestrator, Sink-Implementierungen, der Gym-Threadpool | [units-and-missions.md](units-and-missions.md), [build-and-ops.md](build-and-ops.md) |
| `sim/src/core/` | Avionik-Bus, Kommandobus, Log, Telemetrie, die zwei Richter, Missionsdaten-Typen, Schadensmodell, Ballistik, Elevation-Hook, Basistypen. **Zeigt nie nach `systems/` oder `modules/`.** | [core.md](core.md) |
| `sim/src/math/` | Value-Math (`FBMat4`) | [core.md](core.md) |
| `sim/src/fdm/` | der JSBSim-Adapter. Die eine Übersetzungseinheit mit JSBSim-Headern. | [fdm.md](fdm.md) |
| `sim/src/units/` | Welt-Entitäten: `FBUnit`, `FBSimUnit`, `FBUnitRegistry` | [units-and-missions.md](units-and-missions.md) |
| `sim/src/systems/` | die generischen, airframe-agnostischen Systemslots eines Moduls | [systems.md](systems.md), [sensors.md](sensors.md), [pilot-ai.md](pilot-ai.md), [weapons-and-damage.md](weapons-and-damage.md) |
| `sim/src/modules/` | `FBModule`-Basisschnittstelle + Registry | [modules-f16.md](modules-f16.md) |
| `sim/src/modules/f16/` | das F-16-Modul und seine Overrides | [modules-f16.md](modules-f16.md) |
| `sim/src/modules/stores,missile,ground/` | die Module abgeworfener Waffen und statischer Bodenziele | [weapons-and-damage.md](weapons-and-damage.md) |
| `sim/src/render/`, `render/stages/` | WebGPU-Renderer, eine Klasse je Shader | [rendering.md](rendering.md) |
| `sim/src/world/`, `sim/src/terrain/` | Welt, Tile-Streaming, Gelände-Mathematik | [world-and-terrain.md](world-and-terrain.md) |
| `tiles/` | fb-tiles, der Tile-Server (eigenes Makefile) | [world-and-terrain.md](world-and-terrain.md) |
| `temp/` | Migrationsgut der Vor-Architektur. Read-only-Steinbruch, **keine lebende Architektur.** | — |

## Das Schichtungsmuster

**FBCore → Interface → Default-Implementation → modul-spezifischer Override.**

Nicht „gehört der F-16". Der Simulator lädt beliebig viele steuerbare Module zur Laufzeit und hält jedes
polymorph hinter `FBModule*`. Jedes Modul trägt dieselben Systemkategorien, unterscheidet sich aber im
**Verhalten**, nicht nur in Zahlen. Interface und Default leben in EINER Klasse; ein Modul überschreibt
per Ableitung. Reines Zahlen-Tuning bleibt Preset oder Config — dafür wird keine leere Ableitung
angelegt.

Das gilt seit der Orchestrator-Runde auch für den **Zugriff**: `FBModule` deklariert die generischen
Accessoren selbst (`Autopilot()`, `FlightControl()`, `PilotSystem()`, `Controls()`, `Displays()`,
`AirDataSystem()`, `FlightPlan()`, `Telemetry()`, `SetRunway()`, `SetGroundAsl()`, `ApplySetup()`), und
`FBMissionRunner.cpp` / `FBAppGym.cpp` inkludieren nie einen konkreten Modul-Header.

## Ein Flugzeug = Modul + JSBSim-Modell

| Teil | Wo | Was |
|---|---|---|
| Code-Modul | `sim/src/modules/<name>/` | `FBModule`-Ableitung: Systeme, Presets, Displays |
| JSBSim-Modell | `sim/assets/aircraft/<modell>/` | Aero, Masse, Antrieb, Engine-Daten |

**Eine Wurzel** (`app/FBModelRoots.h`): jedes Modell, das FlightBox fliegt, liegt in
`sim/assets/aircraft` — ein selbstständiges Verzeichnis je Modell, mit seiner `.xml` und seinen eigenen
`engine/`- und `Systems/`-Unterverzeichnissen (JSBSims eigenes Pro-Flugzeug-Layout, das seine Loader vor
jedem geteilten Pfad durchsuchen). Heute: `f16`, `mk82` (beide Kopien aus dem Submodul), `aim120`
(FlightBox-eigen, das Submodul hat keine AMRAAM).

Das gepinnte Submodul ist damit **kein Ladepfad mehr, sondern die Basis** — der Upstream-Stand, gegen den
`make -C sim verify-models` jede Kopie diffed. Abweichen darf eine Kopie nur als benannter Eintrag in
`sim/assets/MODEL-DELTAS.md` — die Delta-Regel, [`CLAUDE.md`](../../CLAUDE.md) Prinzip 1. Die frühere Unterscheidung
„vendored oder eigen" (`FBModule::FdmModelVendored()`) ist damit entfallen; ein Modul nennt nur noch
seinen Modellnamen.

Aircraft-XML trägt eine EIGENE Lizenz (F-16 = GPL, die meisten LGPL) — Attribution je Datei, der
`<fileheader>` jeder Kopie bleibt unverändert.

## Steuerung

Ein schlanker Fly-by-Wire-Layer stabilisiert die Fluglage (Raten-/Attitude-PID auf die
`fcs/*-cmd-norm`-Eingänge; die F-16 hat eine eigene FLCS → `fcs/fbw-override=1` überbrückt sie).
Darüber der Autopilot — generische Guidance in `systems/FBAutopilot`, Verhalten modul-überschreibbar.

| Modus | Mechanismus |
|---|---|
| `manual` (`?ap=manual`) | direkter Stick durch den FBW. `FBInputSystem` ist weiterhin NoOp-Default — kein gebundenes HOTAS. |
| `pilot` (Boot-Default) | `FBPilot` fliegt eine `.fbm`-Mission als Phasenmaschine und kommandiert je Phase `FBAutopilot::Direct` |
| `bfm` (`set task bfm`) | kein Autopilot-Modus, sondern ein eigenes Regelgesetz auf Manual-Stick gegen den gelockten Radarkontakt |
| `intercept` (`set task intercept`) | BVR: eine eigene Zustandsmaschine (`systems/FBEngagement`) auf Direct-Guidance, geflogen mit dem SENSOR |
| `attack` (`set task attack`) | die einzige Phase, deren Entscheidung ein Moment ist: ein Pickle auf den Cue des Feuerleitblocks |

Details: [pilot-ai.md](pilot-ai.md), [systems.md](systems.md).

## Multi-Unit

Eine Mission beschreibt einen **Verband** mehrerer Einheiten verschiedener Fraktionen. Je Einheit:
Modul, Fraktion, Spawn, Flugplan, Ziele. Jede bekommt eine eigene `FBFdm`, ein eigenes `FBModule`,
eigene Monitore und eine eigene Telemetriedatei; das Missions-Urteil fällt **pro Einheit**.

Die Snapshot-Disziplin steht: pro Tick rechnen erst alle Einheiten, dann macht eine Barriere die neuen
Posen gemeinsam sichtbar. `FBUnit::GetPose()` liefert damit immer den Stand des zuletzt
**abgeschlossenen** Ticks — die Tick-Reihenfolge kann kein Ergebnis beeinflussen.

Details und die vier Ausbau-Etappen: [units-and-missions.md](units-and-missions.md).
