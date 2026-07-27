# FlightBox

Ein **JSBSim-gestützter F-16-Simulator auf DCS-World-Niveau** — WASM-App plus Tileserver-Backend, das
die ganze Welt abbildet. Die Physik ist JSBSim; FlightBox ist die Welt drumherum: globales Gelände,
Renderer, HUD, Avionik, Piloten-KI. **Genau zwei Qualitätsachsen zählen: korrektes Rendering und
realistisches F-16-Flugverhalten.**

## Das Wissen steht in doc/flightbox/

Diese Datei ist ein Session-Start-Zettel, kein Wissensspeicher. Alles Inhaltliche — Architektur, jedes
Subsystem, Herleitungen, Fortschritt, offene Punkte — steht in **`doc/flightbox/`**, Einstieg
[`doc/flightbox/INDEX.md`](doc/flightbox/INDEX.md). Der Skill `flightbox` lädt es aufgabenbezogen.

**Widersprechen sich beide, hat `doc/flightbox/` recht und diese Datei ist nachzuführen.**

Daneben: `doc/f16/` (Skill `f16-systems`) dokumentiert den **echten** Jet aus den Handbüchern —
Design-Ziele, keine Defektkriterien. `doc/mission-format.md` ist die Referenz des `.fbm`-Formats.

**Doku wird mitgeführt, nicht nachgeholt.** Jede Runde aktualisiert die betroffene Subsystem-Datei,
trägt sich in `PROGRESS.md` ein und pflegt `TODO.md` — auch die ehrlichen Negative und verworfenen
Ansätze mit ihren Messungen. Eine gemessene Fehlschlagsvariante ist Wissen.

## Prinzipien (nicht verhandelbar)

1. **Physik nicht neu schreiben.** JSBSim ist die Wahrheit, gepinntes read-only Submodul
   (`sim/vendor/jsbsim`), nie gepatcht. Eigener Code nur an den Nähten: FDM-Adapter, Regelung, Renderer.
2. **JSBSim läuft IM Client.** libJSBSim linkt direkt ins Command Center, WASM wie nativ. Keine
   Telemetrie-Grenze zwischen Physik und Bild — ein Prozess, ein Adressraum.
3. **Server-seitig nur zwei Container:** `fb-tiles` (`tiles/`, :8081, Tile-API) und `fb-sim` (`sim/`,
   :8080, Web-Host). Alles andere ist Client.
4. **Sim läuft so schnell wie sinnvoll.** Die Mathematik ist deterministisch. Gibt das Tempo das
   Ergebnis, ist die Kopplung nicht-deterministisch — ein Bug.
5. **F-16 zuerst, vanilla JSBSim-Modell.** Referenz ist das **Modell**, nicht der echte Jet. Seine
   Eigenschaften sind akzeptiert, keine Defekte. FlightBox muss es treu fliegen.

## Kein Cheaten

Die KI darf nicht an der Simulation vorbeigreifen. Strukturell gesichert, wo möglich per Compiler:

- **Zwei unbestechliche Richter**, nie vermischt, nie vom Modul gesehen: `core/FBFlightMonitor`
  (physikalisches K.O., modell-abgeleitet) und `core/FBMissionMonitor` (Missions-Urteil aus eigener
  Plankopie). Jeder Client, der eine Sim-Schleife fährt, füttert beide.
- **Wirken nur über simulierte Systeme.** Einziger State-Schreiber ist der Boot-Spawn: `FBFdm`s
  ladender Konstruktor ist privat, einziger Friend `FBFdmBoot`, das nur `app/` nennt.
- **Sehen nur über Sensoren.** Die Unit-Registry erreicht genau vier Dateien (Datalink, Radar, RWR,
  Flugkörper-Uplink). Ein Radarkontakt trägt keine Identität; die einzige Identitätsquelle ist IFF
  Mode 4, und die kennt kein „hostile".
- **Schaden ist typgeschützt.** `core/FBSystemHealth` ist monoton, alle Mutatoren privat, genau ein
  Friend (`FBDamageModel`). Selbstheilung kompiliert nicht.

## Architektur in drei Sätzen

```
fb-tiles (DEM/OSM/Luftbild)  ──HTTP──▶  Command Center = JSBSim + FBW + Autopilot + Renderer + HUD
                                        als EIN Prozess (WASM | native)
```

FlightBox Core ist eine **reine Bibliothek** (`build/libfbcore.a`: `core/ fdm/ systems/ modules/
units/` + libJSBSim). Drei Clients linken dagegen: **`fb-gym`** (headless, GPU-frei, der Missions-Kern),
**`gpu_native`** (Referenz-Renderer und Frame-Orakel) und **wasm** (der Browser).

Schichtung überall: **FBCore → Interface → Default → modul-spezifischer Override.**

## Build

Nur über Make-Targets. `sim/`: `core-lib` | `gym` | `native` | `wasm` (baut `worker` immer mit) |
`worker` | `image` | `up` | `test-monitor` | `test-fdm` | `test-corner` | `test-missile` | `test-gun`.
`tiles/`: `build` | `image` | `run`.

**Gates:** Warnings = Errors (`-Wall -Wextra -Wpedantic`) · `nm build/fb-gym` = 0 Dawn/WebGPU-Symbole ·
sieben Harnesses rc=0 · Frame-Beweis oder numerische Messung · Regression über alle `sim/missions/*.fbm`
mit einzeln begründeten Abweichungen · Determinismus über `--threads 1/2/4` · vendor bleibt read-only.

**Regelkreis:** Mission definieren → headless simulieren → Telemetrie analysieren → Korrektur → Loop.
Exit 0/1/2/3 = SUCCESS/FAIL/CRASH/TIMEOUT. Der Exit-Code ist nicht immer das Urteil — die Leseregel
steht im Kopfkommentar der jeweiligen `.fbm`-Datei und ist verbindlich.

## Harte Regeln im Code

- **Keine verstreuten Ausgaben.** Ereignisse über `FBLog`, Zustand über `FBTelemetryBus`. Ausnahmen nur:
  die Sink-Implementierungen und CLI-UX in `app/`. Core ist I/O-frei.
- **Jede Zahl trägt ihre Herkunft** — hergeleitet (mit Formel), gemessen (mit Messung) oder `[SET]`.
  Eine Zahl ohne eine der drei Angaben ist ein Defekt.
- `core/` zeigt nie nach `systems/` oder `modules/`. Peers rufen sich nie gegenseitig.
- `extern "C"` nur für von JS namentlich gerufene Symbole (heute zwei).
- C++17, JSBSim-naher Stil: `FB`-Präfix, PascalCase, `namespace FlightBox`, Klasse pro Datei.

## Host

emsdk in `~/Git/emsdk`, `nproc`-Shim in `~/.local/bin`. Container: `podman machine start`, dann
`tiles/up.sh` (:8081) und `sim/up.sh` (:8080); fb-sim mountet `sim/web` live. Native Builds brauchen
`sim/vendor/.compat-headers` (host-lokal). **macOS hat kein `timeout(1)`** — nicht in Skripte einbauen.
