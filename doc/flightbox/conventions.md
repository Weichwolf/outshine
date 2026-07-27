# Konventionen

Sprache, Namen, Struktur, und was nie im Code stehen darf.

## Sprache

**C++17, wie JSBSim. Nicht C.** Ordentliche Klassen nach C++-Best-Practice: RAII, klare Ownership,
minimale public API.

Der Coding-Style orientiert sich an JSBSim, weil FlightBox sich in dessen Ökosystem einfügt:

| Sache | Regel | Beispiel |
|---|---|---|
| Klassen | `FB`-Präfix (analog JSBSims `FG`) | `FBFlightControl`, `FBRenderer` |
| Methoden | PascalCase | `Run()`, `GetLoadFactor()` |
| Member | PascalCase | `LatDeg`, `EngState_` |
| Namensraum | ein `namespace FlightBox` | — |
| Dateien | Klasse pro Datei | `FBName.h` / `FBName.cpp` |
| Getter | inline im Header | — |
| Header-Guards | ja | — |

JSBSims LGPL-Banner wird nicht kopiert — unsere Dateien tragen unsere Lizenz.

## `extern "C"`

Nur für Funktionen, die von JavaScript **namentlich** gerufen werden. `EMSCRIPTEN_KEEPALIVE` allein
reicht nicht — Mangling bricht Exporte still.

Heute betrifft das genau zwei Symbole: `fb_toggle_ground` und `fb_set_ground` in `FBAppWasm.cpp`. Der
FDM-Adapter ist ausdrücklich **kein** solcher Fall und lebt in `namespace FlightBox`.

## Keine verstreuten Ausgaben

`core/`, `systems/`, `modules/`, `render/`, `world/`, `fdm/`, `units/` emittieren **nie** direkt. Kein
`printf`, kein `fprintf`, kein `std::cout`, kein `std::cerr`.

| Art | Kanal |
|---|---|
| diskrete Ereignisse | `FBLog` (`core/FBLog.h`) — geleveled, `tag` + `event` + key=val-Felder |
| periodischer Zustand | `FBTelemetryBus` (`core/FBTelemetry.h`) — Zeitreihe mit Schema |

Ausnahmen, abschließend:

- die Sink-Implementierungen selbst (`app/FBLogSinks.*`, `app/FBTelemetrySinks.*`)
- CLI-UX in `app/`: Usage, Hilfe, argv-Fehler, Bootstrap-Fehler vor dem Sink-Aufbau

Core bleibt I/O-frei, aber nicht formatierungsfrei: `snprintf` in einen lokalen Puffer ist überall
erlaubt, ein `FILE*` oder `fstream` nirgends.

## Kommentare

**Der Zweck eines Kommentars ist das nicht-offensichtliche WARUM.** Ein Kommentar, der beschreibt, *was*
die Zeile darunter tut, sagt dasselbe in zwei Sprachen und driftet weg — er wird weggelassen. Code und
Namen erklären sich selbst.

Was dieses Projekt zusätzlich verlangt: **jede Zahl trägt ihre Herkunft.** Eine Konstante ist entweder

- **hergeleitet** — dann steht die Herleitung dabei (die Formel, nicht das Ergebnis), oder
- **gemessen** — dann steht die Messung dabei (was, womit, welches Ergebnis), oder
- **gesetzt** — dann ist sie als `[SET]` gekennzeichnet und als Setzung benannt.

Eine Zahl ohne eine dieser drei Angaben ist ein Defekt.

> **Offene Entscheidung.** Der Bestand trägt diese Herleitungen als 15–25-zeilige Banner direkt im
> Quellcode. Die Herleitungen selbst sind das wertvollste Wissen im Baum und unstrittig; strittig ist
> ihr ORT. Kandidat: Herleitung nach `doc/flightbox/`, im Code eine Zeile Verweis. Siehe
> [TODO.md](TODO.md).

## Struktur

- `core/` zeigt **nie** nach `systems/` oder `modules/`.
- Peers rufen sich nie gegenseitig — ein Modul cycelt seine Systeme, die Systeme kennen einander nicht.
- Sensoren **schreiben** `FBState`, Displays **lesen** ihn.
- Waffen erhalten eine geborgte `FBWorld`-Referenz, nie eine globale.
- Der Renderer ist ein Bolt-on, nie eine Abhängigkeit der Physik- oder Terminierungs-Logik.
- Die Pass-Topologie des Renderers ist ein Vertrag: nur `FBRenderer` setzt Pass-Grenzen, kein Stage-Split
  darf sie vermehren.

## Architektur-Stil

Systeme bauen, nicht Features. Minimale public API, maximale Kapselung. Zustandsmaschinen statt
boolescher Flags. Komposition vor Vererbung. Registry-/Plugin-Muster. Phasen-orientierte Abläufe.

Defensiv an Systemgrenzen, vertrauend im Inneren. Feste Kapazitäten und keine Allokation im Tick-Pfad.
Kein Zufall in einer deterministischen Simulation — wo eine Streuung nötig ist, ist sie ein Modell, kein
Würfel.
