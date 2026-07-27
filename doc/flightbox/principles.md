# Prinzipien und Integrität

Was immer wahr sein muss. Diese Datei ist die einzige im Baum, die nicht beschreibt, was gebaut ist,
sondern was nicht verhandelbar ist. Jede andere Datei setzt sie voraus.

## Was FlightBox ist

Ein JSBSim-gestützter F-16-Simulator auf DCS-World-Niveau: eine WASM-App mit einem Tileserver-Backend,
das die ganze Welt abbildet. Die Physik ist JSBSim; FlightBox ist die Welt drumherum — globales Gelände,
Renderer, HUD, Steuerung, Avionik, Piloten-KI.

**Genau zwei Qualitätsachsen zählen:** korrektes Rendering und realistisches F-16-Flugverhalten.
Alles andere ist Mittel zum Zweck.

Der Aircraft-Plugin-Mechanismus ist der saubere Weg, ein JSBSim-Modell einzuhängen — aber das Produkt
ist die F-16. Andere Modelle sind Nebensache, nicht das Ziel.

## Die fünf Prinzipien

### 1. Physik nicht neu schreiben

JSBSim (LGPL, NASA/FlightGear-erprobt) ist die Wahrheit. Eigener Code nur an den Nähten: der
FDM-Adapter, die Regelung, der Renderer.

JSBSim ist ein **gepinntes, read-only Git-Submodul** (`sim/vendor/jsbsim`) und wird nie gepatcht. Der
Build baut libJSBSim aus dem Submodul — die Physik ist damit bit-identisch zum gepinnten Commit und
update-fähig. Wo eine Erweiterung nötig ist (Zuladung, Schaden), wird sie über modell-eigene
JSBSim-APIs zur Laufzeit bestückt, nie durch Ändern eines Modell-XML.

### 2. JSBSim läuft IM Client

libJSBSim linkt direkt in das Command Center — WASM via Emscripten, nativ fürs CLI. Modell-XML und
Engine-/Tabellendaten reisen in Emscriptens virtuellem FS.

**Keine Telemetrie-Grenze zwischen Physik und Bild.** Sie sind derselbe Prozess, teilen einen
Adressraum, die Kamera liest den JSBSim-Zustand direkt. Gäbe es einen Wire dazwischen, wäre es die alte
Architektur; der Sinn des Pivots war, ihn zu streichen.

### 3. Server-seitig nur zwei schlanke Container

| Container | Pfad | Port | Aufgabe |
|---|---|---|---|
| `fb-tiles` | `tiles/` | 8081 | reine Tile-API — weltweit DEM/OSM/Luftbild |
| `fb-sim` | `sim/` | 8080 | reiner Web-Host: serviert die WASM-App + env-config |

**Alles andere ist Client** — Physik, FBW, Autopilot, Renderer, HUD, Avionik, KI. Kein SITL, kein
World-Prozess, kein Hub.

### 4. Sim läuft so schnell wie sinnvoll

Die Sim-Mathematik ist deterministisch; Wall-Clock-Tempo ändert das Ergebnis nicht.

- Live-Flug im Browser = Echtzeit
- Batch/Screenshot/Headless = so schnell die Maschine kann

**Gibt das Tempo das Ergebnis, ist die Kopplung nicht-deterministisch — das ist ein Bug.**

### 5. F-16 zuerst, full-scale, vanilla JSBSim-Modell

Die F-16 ist die vanilla JSBSim-F-16 (`sim/vendor/jsbsim/aircraft/f16`, full-scale, echte FLCS).

**Referenz ist das MODELL selbst, nicht der absolute echte Jet.** Das Modell ist gepinnt und vanilla;
seine validierte Charakteristik ist die Wahrheit — eine Rollrate von ~190 °/s ist eine
Modell-Eigenschaft und wird akzeptiert, kein Defekt.

FlightBox muss das Modell **treu fliegen**: der FBW/Autopilot kommandiert die echte FLCS, verzerrt sie
nicht; kein künstliches Departure aus Spawn oder Trimm, keine Divergenz. Bewertet werden korrekte
Integration und korrektes Rendering, nicht Modell-gegen-echten-Jet.

Recherchierte Zahlen des realen Jets (`doc/f16/`) sind **Design-Ziele, keine Defektkriterien**.

## Kein Cheaten

Die Piloten-KI darf nicht gewinnen, indem sie an der Simulation vorbeigreift. Das ist nicht per
Konvention gesichert, sondern per Struktur — und wo möglich per Compiler statt per Grep.

### Zwei unbestechliche Richter, nie vermischt

Zwei Fragen, also zwei Instanzen. Beide gehören dem Runner/der App, keine ist je vom Modul sichtbar.

| Richter | Frage | Wahrheitsquelle |
|---|---|---|
| `core/FBFlightMonitor` | Ist das Flugzeug physikalisch K.O.? (Absturz, Kollision, LOC, Divergenz) | rein physikalisch, modul-agnostisch — Struktur- und Fahrwerks-Wahrheit aus dem gepinnten JSBSim-Modell selbst |
| `core/FBMissionMonitor` | Ist die Mission erfüllt oder gescheitert? | die eigene, unveränderliche Kopie der Missionsdatei — reine Positions-Beobachtung, nie Modul-Selbstauskunft |

Beide werden von **jedem** Client gefüttert, der eine Sim-Schleife fährt — `fb-gym`, `gpu_native
--mission` und der WASM-App-eigene Frame-Loop gleichermaßen. Je eine Definition, kein zweiter
Paralleltest. Bei mehreren Einheiten hat jede ihr eigenes Paar; das Gesamturteil ist eine reine
Kombination im Runner.

Belegprüfung: `grep -rn 'FBSimUnit\|FBFlightMonitor\|FBMissionMonitor' sim/src/systems sim/src/modules`
bleibt ohne Treffer.

### Der Pilot wirkt nur über simulierte Systeme

Erlaubte Wirkpfade, vollständig:

- `fcs/*-cmd-norm` über `FBFlightControl` / `FBAutopilot`
- `FBAirframeControls` für Fahrwerk, Bremsen, Bugradsteuerung, Speedbrake, Triebwerk
- Throttle, Tankfüllstand über `FBFdm::SetFuel*`
- Waffenauslösung ausschließlich über den Kommandobus → `FBStoresSystem::Release` / `FBGunSystem::Trigger`

**Der einzige State-Schreiber (JSBSim-IC/Trim) ist der App-eigene Boot-Spawn.** Die IC-Abschottung ist
strukturell: der ladende Konstruktor von `FBFdm` ist privat, einziger Friend ist `FBFdmBoot`, und
`FBFdmBoot.h` wird nur von `app/`-Dateien genannt. Wer `FBFdm.h` inkludiert — jedes Modul, jedes System
— erreicht damit keine IC. Es gibt kein Re-Init und kein Reset.

Auch eine Waffe kann sich nicht selbst in die Welt setzen: das Modul legt einen Datensatz in eine
Warteschlange, der **Besitzer** spawnt.

### Der Pilot sieht nur über simulierte Sensoren

Die Einheiten-Registry (`units/FBUnitRegistry`, die Welt-Wahrheit „wer existiert wo") reicht
ausschließlich bis zu den Sensor-Slots. Sie steht in keiner Pilot-Signatur und in keinem Member.

Belegprüfung: `#include "FBUnitRegistry.h"` und `.Units()` erscheinen in `sim/src/systems` und
`sim/src/modules` in **genau vier** Dateien:

| Datei | Warum sie dazugehört |
|---|---|
| `systems/FBDatalinkSystem.cpp` | kooperativer Sensor |
| `systems/FBRadarSystem.cpp` | aktiver Sensor |
| `systems/FBRwrSystem.cpp` | passiver Sensor — sieht nur publizierte Emissionen |
| `modules/missile/FBMissileUplink.cpp` | Empfänger einer publizierten Emission, aus demselben Grund |

Sonst kommt der Typ dort nur als Vorwärtsdeklaration oder Parameter vor.

Was ein Pilot über andere Einheiten weiß, steht in `FBState` — das, was die Sensoren dort
hineingeschrieben haben, mit ihrer Reichweite, ihrem Scanvolumen, ihrem Netzzyklus und ihrem Alter.

### Identität ist keine Sensorgabe

`core/FBRadarContact` trägt Entfernung, Peilung, Azimut, Elevation, Annäherungsrate und eine
radar-eigene Tracknummer — **keine Unit-Id, kein Callsign, kein Team**. Die Registry weiß, wer da
fliegt; das Radar darf es nicht durchreichen.

Die einzige legitime Identitätsquelle ist IFF Mode 4, und sie ist **zweiwertig**: gültiger Reply =
friendly, kein Reply = unbekannt. `FBIffReply` hat gar keinen Wert „hostile".

### Schaden ist schreibgeschützt durch den Typ

`core/FBSystemHealth` (eines je Einheit) ist monoton — nichts repariert sich im Flug — und **jeder
Mutator ist privat mit genau einem `friend`: `core/FBDamageModel`**. Das Modul bekommt es als `const&`
und liest. Kein System, kein Pilot, kein Modul kann sich selbst oder einen anderen beschädigen oder
reparieren. Das ist nicht per Grep belegt, sondern per Übersetzung: es kompiliert nicht.

Der einzige Weg in den Zustand ist eine aufgelöste Detonation oder ein aufgelöster Geschossstrom.

### Die Zelle wird nur über ihr Interface gelesen

Der Pilot liest Gewicht-auf-Rädern, Fahrwerksposition und Triebwerkslauf ausschließlich über
`FBAirframeControls`, nie an dieser Schnittstelle vorbei in ein FDM. So bleibt `systems/` airframe-
**und** instanz-agnostisch.
