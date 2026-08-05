# FlightBox

Ein **JSBSim-gestützter F-16-Simulator auf DCS-World-Niveau** — WASM-App plus Tileserver-Backend, das
die ganze Welt abbildet. Die Physik ist JSBSim; FlightBox ist die Welt drumherum: globales Gelände,
Renderer, HUD, Avionik, Piloten-KI. **Genau zwei Qualitätsachsen zählen: korrektes Rendering und
realistisches F-16-Flugverhalten.**

## Der Dreiklang

> **`doc/` = was wir wollen · `src/` = was wir können · `test/` = was wir beweisen.**

Jede Aussage hat **genau einen Ort**, und die drei Bäume sind **identisch**: aus einem Pfad folgen die
beiden anderen, ohne zu suchen.

```
sim/src/sensors/FBVisualSystem.cpp   ← was wir können
doc/sensors/visual.md                ← was wir wollen
sim/test/sensors/visual.json         ← was wir beweisen
```

Daraus wird eine **Vollständigkeitsprüfung**: ein Verzeichnis mit zwei von drei hat ein benanntes Loch.
Absicht ohne Umsetzung = Lücke in `src/`. Umsetzung ohne Beleg = Lücke in `test/`. Beleg ohne Absicht =
ein Test, den niemand bestellt hat.

**Was das mit Kommentaren macht.** Sie fallen fast vollständig weg — Kopfblöcke, Benutzungshinweise,
jede Beschreibung des *was*, und jede Prosa über die Funktionsweise (die gehört nach `doc/`). Es bleibt
genau EINE Aufgabe: **das lokale Warum am Entscheidungspunkt** — warum diese Form *hier* statt der
naheliegenden Alternative, warum diese Zahl in *dieser* Klasse, warum diese Zeile gelöscht wurde. Das
trägt weder `doc/` (dort würde es zum Codekommentar mit Verzeichnis, und die Datei hörte auf, Absicht zu
sein) noch `test/` (der belegt das Ergebnis, nicht die Wahl). Beispiele: `kSeparationDelayS` wohnt beim
Besitzer der Warteschlange, weil `pilot/` nicht nach `missions/` sehen darf · `Resync_` gehört in
`FBRadarSystem` statt `FBF16Fcr`, weil ein Modul, das Emission über den MODUS führt, sonst keine Stelle
hätte · der Artfilter im Auge ist weg, weil die nächste Zeile der bessere Filter ist.

**Eine Erwartung ist ein Datum, kein Programm.** Eine Behauptung, die in C++ steckt, kann still aufhören
zu prüfen — gemessen: sieben Anker außerhalb ihres Bandes bei grünem Tor, und fünf von sechs absichtlich
zerstörten Modellwerten kamen durch das ganze Netz. Eine fehlende Zeile in einer Tabelle sieht man.
[`doc/testing.md`](doc/testing.md).

## Das Wissen steht in doc/

Diese Datei ist ein Session-Start-Zettel, kein Wissensspeicher. Alles Inhaltliche — Architektur, jedes
Subsystem, Soll/Ist/Lücken, Herleitungen — steht in **`doc/`** (englisch), Einstieg
[`doc/INDEX.md`](doc/INDEX.md); der EINE Skill `flightbox` lädt es.

**Widersprechen sich beide, hat `doc/` recht und diese Datei ist nachzuführen.**

Darin: `doc/modules/<jet>/` dokumentiert je den **echten** Jet aus den Handbüchern (Design-Ziele, keine
Defektkriterien); `doc/missions/` ist die Referenz des `.fbm`-Formats.

**Spec zuerst.** Jede Runde ändert ZUERST das `## Spec` ihrer Themendatei, baut, bis `## State` es
erfüllt (gemessen an dessen Ankern), führt `## State`/`## Gaps` nach und trägt eine Zeile ins
`journal.md`. Verworfenes bleibt mit seiner Messung in `## Gaps` — ein gemessener Fehlschlag ist Wissen.

**Dokumente enthalten Gegenwart und Zukunft, nie Vergangenheit.** Für die Historie ist Git zuständig:
eine überschriebene Fassung wird GELÖSCHT, nicht darunter stehen gelassen. Das ist kein Widerspruch zum
Satz davor — ein verworfener Ansatz mit seiner Messung ist eine *heute geltende* Aussage darüber, was
nicht funktioniert, und gehört als `## Gaps`-Zeile in die Gegenwart. Eine alte Formulierung derselben
Sache ist es nicht.

## Prinzipien (nicht verhandelbar)

1. **Physik nicht neu schreiben — Modell-Abweichung nur als Delta.** Die Engine ist die Wahrheit,
   gepinntes read-only Submodul (`sim/vendor/jsbsim`), nie gepatcht; eigener Code nur an den Nähten
   (FDM-Adapter, Regelung, Renderer). Geflogen wird FlightBox' Modellkopie (`sim/assets/aircraft/`, die
   EINE Wurzel), das Submodul ist deren **Basis**: jede Abweichung ist ein benannter, BELEGTER Eintrag
   in `sim/assets/MODEL-DELTAS.md` (ein besseres Missionsergebnis ist kein Beleg), Gate
   `make -C sim verify-models`.
2. **JSBSim läuft IM Client.** libJSBSim linkt direkt ins Command Center, WASM wie nativ. Keine
   Telemetrie-Grenze zwischen Physik und Bild — ein Prozess, ein Adressraum.
3. **Server-seitig nur zwei Container:** `fb-tiles` (`tiles/`, :8081, Tile-API) und `fb-sim` (`sim/`,
   :8080, Web-Host). Alles andere ist Client.
4. **Sim läuft so schnell wie sinnvoll.** Die Mathematik ist deterministisch. Gibt das Tempo das
   Ergebnis, ist die Kopplung nicht-deterministisch — ein Bug.
5. **F-16 zuerst.** Referenz ist das **Modell**, nicht der echte Jet — seine Eigenschaften sind
   akzeptiert, keine Defekte, und FlightBox muss es treu fliegen. Das gilt, weil das geflogene Modell
   exakt benannt ist: gepinnter Stand plus die belegte Delta-Liste aus Prinzip 1 (heute leer).

## Kein Cheaten

Die KI darf nicht an der Simulation vorbeigreifen. Strukturell gesichert, wo möglich per Compiler:

- **Zwei unbestechliche Richter**, nie vermischt, nie vom Modul gesehen: `core/FBFlightMonitor`
  (physikalisches K.O., modell-abgeleitet) und `core/FBMissionMonitor` (Missions-Urteil aus eigener
  Plankopie). Jeder Client, der eine Sim-Schleife fährt, füttert beide.
- **Wirken nur über simulierte Systeme.** Einziger State-Schreiber ist der Boot-Spawn: `FBFdm`s
  ladender Konstruktor ist privat, einziger Friend `FBFdmBoot`, das nur `missions/` und `clients/` nennen.
- **Sehen nur über Sensoren.** Die Unit-Registry erreicht genau sechs Dateien (Datalink, Radar, RWR,
  IRST, Auge, Flugkörper-Uplink). Ein Radarkontakt trägt keine Identität; die einzige Identitätsquelle
  ist IFF Mode 4, und die kennt kein „hostile". Ein Sichtkontakt trägt nicht einmal eine Entfernung —
  nur einen TYP, sobald die Winkelgröße ihn hergibt, und der ist der Modul-Registry-Schlüssel.
- **Schaden ist typgeschützt.** `core/FBSystemHealth` ist monoton, alle Mutatoren privat, genau ein
  Friend (`FBDamageModel`). Selbstheilung kompiliert nicht.

## Architektur in drei Sätzen

```
fb-tiles (DEM/OSM/Luftbild)  ──HTTP──▶  Command Center = JSBSim + FBW + Autopilot + Renderer + HUD
                                        als EIN Prozess (WASM | native)
```

FlightBox Core ist eine **reine Bibliothek** (`build/libfbcore.a`: alle Schichten unter den Clients
— `core/ fdm/ units/ sensors/ weapons/ systems/ pilot/ modules/ missions/` + libJSBSim; Schichtordnung — `make verify-layers` prüft die Ordnung). Drei Clients linken dagegen: **`fb-gym`** (headless, GPU-frei, der Missions-Kern),
**`gpu_native`** (Referenz-Renderer und Frame-Orakel) und **wasm** (der Browser).

Schichtung überall: **FBCore → Interface → Default → modul-spezifischer Override.**

## Build

Nur über Make-Targets. `sim/`: `core-lib` | `gym` | `native` | `wasm` (baut `worker` immer mit) |
`worker` | `image` | `up`.
Tore: `verify-layers` | `verify-guards` | `verify-models` | `verify-trees` | `verify-tests`.
Harnesses (zehn, unter `sim/test/<pfad>` neben ihrem Subjekt): `test-monitor` | `test-fdm` |
`test-corner` | `test-missile` | `test-gun` | `test-air` | `test-mig29` | `test-weather`.
`tiles/`: `build` | `image` | `run`.

**Gates:** Warnings = Errors (`-Wall -Wextra -Wpedantic`) · `nm build/fb-gym` = 0 Dawn/WebGPU-Symbole ·
zehn Harnesses mit unveraendertem Ergebnis (`test-air` ist rot und nennt sieben Anker) · Frame-Beweis oder numerische Messung · Regression über alle `sim/missions/*.fbm`
mit einzeln begründeten Abweichungen · Determinismus über `--threads 1/2/4` · `make wasm` baut und die
App startet · `verify-models` grün · vendor bleibt read-only.

**Regelkreis:** Mission definieren → headless simulieren → Telemetrie analysieren → Korrektur → Loop.
Exit 0/1/2/3 = SUCCESS/FAIL/CRASH/TIMEOUT. Der Exit-Code ist nicht immer das Urteil — die Leseregel
steht im Kopfkommentar der jeweiligen `.fbm`-Datei und ist verbindlich.

## Harte Regeln im Code

- **Keine verstreuten Ausgaben.** Ereignisse über `FBLog`, Zustand über `FBTelemetryBus`. Ausnahmen nur:
  die Sink-Implementierungen und CLI-UX in `clients/`. Core ist I/O-frei.
- **Jede Zahl trägt ihre Herkunft** — hergeleitet (mit Formel), gemessen (mit Messung) oder `[SET]`.
  Eine Zahl ohne eine der drei Angaben ist ein Defekt.
- `core/` zeigt nie nach `systems/` oder `modules/`. Peers rufen sich nie gegenseitig.
- `extern "C"` nur für von JS namentlich gerufene Symbole (heute zwei).
- C++17, JSBSim-naher Stil: `FB`-Präfix, PascalCase, `namespace FlightBox`, Klasse pro Datei.

## Host

emsdk in `~/Git/emsdk`, `nproc`-Shim in `~/.local/bin`. Container: `podman machine start`, dann
`tiles/up.sh` (:8081) und `sim/up.sh` (:8080); fb-sim mountet `sim/web` live. Native Builds brauchen
`sim/vendor/.compat-headers` (host-lokal). **macOS hat kein `timeout(1)`** — nicht in Skripte einbauen.
