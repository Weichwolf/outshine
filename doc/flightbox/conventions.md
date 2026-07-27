# Konventionen

> Body still in German — translation pass pending (see [roadmap](roadmap.md)). The working rule below
> is normative and in English.

Sprache, Namen, Struktur, und was nie im Code stehen darf.

## The working rule (spec first)

This documentation is **spec-driven**: every topic file carries the same four sections, and they have
different owners in time.

| Section | Content | Changes when |
|---|---|---|
| `## Spec` | the contract: what the thing must be able to do, acceptance criteria, measurement anchors | only by **decision** — never by building |
| `## State` | what is built, with commit and measurement. Honest, including "nothing" | when a round lands |
| `## Gaps` | the difference Spec − State, ordered by value, **including rejected approaches with their measurements** | when a round lands |
| `## Knowledge` | derivations, formulas, measured constants | when something is derived or measured |

A round that intends to change behaviour therefore runs like this:

1. **Change the Spec of its topic file first.** If the round cannot say what the contract becomes, it
   is not ready to start. A Spec change is a decision and is made as one.
2. **Build until State meets Spec** — measured against the Spec's own anchors, not against a feeling.
   Measurements beat inspection; the mission control loop ([`build-and-ops.md`](build-and-ops.md)) is
   how a claim about behaviour gets settled.
3. **Update State and Gaps, and add one line to [`journal.md`](journal.md)** (commit, what it built,
   what it measured).
4. **Rejected approaches stay in Gaps** — with their measurements. A measured failure is knowledge;
   deleting it means someone re-runs the experiment.

Two consequences worth stating: there is no second list of open work anywhere (no `TODO.md`, no
trailing "offene Punkte" per file — Gaps is the one place), and `CLAUDE.md` is touched only when a
session-start fact changed, kept under 100 lines.

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

> **Entschieden und umgesetzt** (roadmap R1, commit `f77f1cf`): the derivations live HERE — in the
> `## Knowledge` section of the topic file — and the code carries a one-liner plus a reference. The
> proof that no behaviour changed is the unchanged `sim/tools/strip_comments.py` hash. Der Bestand trug
> diese Herleitungen zuvor als 15–25-zeilige Banner direkt im Quellcode.

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
