Type: bug
Depends: 2211
State: active
Area: include, scenario
Tags: measured, gate, door
Supersedes: 2107

# The door writes back what it reads, and every public entity is documented

**Benchmark** -- Unreal: a `UPROPERTY` is serialised BOTH ways by the same reflection data, so a
field that loads is a field that saves, and every public API carries its doc comment because the
tooling refuses otherwise. RAGE: `parCodeGen` metadata declares each field once and the reader
and writer are generated from it. **Both agree**: a declaration that can be read and not written
back does not exist in the grammar, and a public entity without its documentation is not public.

## Where it stands, measured 2026-09-04, `make lint`

```
  76 children the grammar declares, 12 the writer writes back, 64 it cannot    target 0
  702 undocumented public entities in include/                                  target 0
```

Both are lint guards and both are RED, and neither had an item -- board:2093 holds the
clang-tidy count only. `Engine::writeScenario` exists so that read -> write -> read is a
counter-control a client can run; with 64 children the writer drops, the control proves the
writer and nothing else. And `Unacted()` in `EngineHeld.h` lists every section it CARRIES
without acting on, which is CLAUDE.md's loud failure made quiet: `layers`, `providers`,
`compositors`, `placements`, `kinds`, `instances`, `regions`, `doors`, `tables` (board:2107), and
`tables` and `buses` and `sounds` counted TWICE in that list.

## What will be true

- [ ] Every child the grammar declares is written back by `writeScenario`, or the row leaves the
      grammar -- per child, with the decision on the line
- [ ] Every section `Unacted()` carries is either acted on or refused at `declare`; the list of
      what is carried silently shrinks to nothing and the duplicate rows go
- [ ] `make doc` reports 0 undocumented public entities, and the Doxygen line on each is the
      unit and the promise, not the name restated
- [ ] Proving case: `roundtrip` over every place reads back byte-identical, which the writer gap
      makes impossible today
- [ ] Negative control: drop one child from the writer and the lint guard goes RED at 1

## What will show I was wrong

If a child cannot be written back because the engine holds it in a form the grammar cannot
spell -- a derived value -- then it should never have been a declaration, and the row goes rather
than the writer growing a special case.

## Loading-Vertrag

Snapshot owns nur Werte; Engine-Zugriff serialisiert, Callback leiht Snapshot nur
für Aufruf. share addiert aktuell size_t vor Cast und läuft über; Summen in double
bilden, leere Nachfrage bleibt 1, kein Residency-Versprechen. SIZE_MAX-Kontrollfälle.
Einheiten gegen TilePool geprüft: FetchedMB tatsächlich MiB, Megabits tatsächlich
MiBit/s aus kumuliertem Poolzähler / preload-Zeit. Dokumentieren, Umbenennung und
intervallrichtige Ratenmessung bleiben offen; kein Netzwerk-Durchsatzversprechen.

Preload verlangt endliche nichtnegative Sekunden vor jeder Arbeit/Callback; NaN/Inf
und negative Budgets dürfen keine unbeschränkte Schleife oder stillen Erfolg liefern.
Öffentlichen Callback-/Snapshot-/Teilfortschrittsvertrag dokumentieren; Roots besitzt
Strings, setRoots ist Setup und migriert keine bereits geöffneten Ressourcen.

Float-Readback lehnt Colour und unbekannte Bufferwerte vor Stood/Draw ab; keine
GPU-Arbeit für ungültige Ausgabeanforderung. Öffentlichen Komponenten-/Raumvertrag
gegen Shader und Readback dokumentieren; Diagnose-Identität ist kein Entity-Handle.
Test ohne Renderziel verlangt spezifische Ablehnung und unveränderten Output.

handleEvent: Holds<bool> trennt behandelt/unbehandelt von Verarbeitungsfehlern.
Keine Szene, irrelevantes Event, kein Treffer oder keine ausgelöste Aktion liefern
false statt leerer/alter Fehler. Fehlender Host bei gebundener Aktion liefert Diagnose;
Scroll meldet tatsächliche Änderung. Callback-/Thread-/Borrow-Vertrag dokumentieren.

## Nicht implementierte Benchmark-API

Engine::bench ist zweimal deklariert, ohne Definition oder Archivsymbol; Benched
hat außerhalb des Headers keine Nutzer. Der Client misst bereits advance/render
über die öffentliche API (PlaceCamera.cpp). Benchmark-Orchestrierung bleibt beim
Client; tote bench-Deklarationen und ihren ausschließlich dazu gehörigen Ergebnistyp
entfernen, statt einen zweiten Frame-Loop in die Runtime einzubauen. Bestehende
Messpfade und Sampling-API erhalten. Prüfung: alle Referenzen durchsuchen, Client
bauen, vollständiger Lint. Kein Nachweis für GPU-Ausführungszeit durch CPU-Timer.

Höhenabfrage: sampleHeight validiert Winkel vor Welt-/Tilezugriff. NaN/Inf und
Werte außerhalb [-180,180] Longitude / [-90,90] Latitude ablehnen; Höhe ignoriert.
Außerhalb der Mercator-Abdeckung Fehler statt Höhe einer geklemmten Ersatzposition.
Kosten ehrlich dokumentieren: At kann Tiles vorbereiten, ist keine reine
Residency-Abfrage. API-Test trennt Eingabefehler von fehlender Welt, einschließlich
Grenzen und ignorierter NaN-Höhe; Negativkontrolle gegen bisherigen Code.

Diagnose-/Borrow-Verträge gegen Ledger, Session und Cost prüfen und dokumentieren:
declaration/unacted/measures verleihen veränderliche Engine-Daten, keine Snapshots;
standing ist nur Szenenobjekt-Präsenz. error ist Legacy-Diagnose, nicht zuverlässig
das Ergebnis des letzten expected-Aufrufs. Sampling beschreibt gespeicherte
CPU-Dauern, Reihenfolge, Überschreiben, Allokation und Threadbindung; kein GPU-Timer.

advance(elapsedS): endliche nichtnegative Sekunden vor Akkumulatoränderung prüfen;
auch nicht darstellbare Summe ablehnen. Ungültiger Aufruf darf keine Schritte
ausführen oder Zeitreste verändern. Null bleibt erlaubtes Nachholen bestehender
Zeitreste. API-/Negativtest mit NaN, ±Inf, negativer Zeit und anschließender Null.
Validierung von StepS/MostStepsInArrears und Überlastpolitik bleiben separat offen.

Zeitschrittdeklaration vor ships/Mutation validieren: StepS endlich >0,
MostStepsInArrears >0, Produkt endlich. Vorher akzeptierte Null/NaN führte zu
unterschiedlichen Takten in Integrate und Akkumulator; negative Limits blockierten
Schritte still. Ablehnung erhält vorherige Deklaration. API-Test mit gültiger
Baseline und ungültigen Kandidaten, Negativkontrolle. Globale Frame-Arbeitsbudgets
und geprüfte Ganzzahlkonvertierung im Szenario-Parser bleiben offen.

Parser-Nachhollimit: double→int erst nach Endlichkeit, Ganzzahligkeit und Bereich
[1,INT_MAX]. ReadSectionsOnto erhält Fehlerkanal; Basis- und Layer-Lesen reichen
den fachlichen Fehler durch. Tests mit Bruchzahlen, Null, negativen Werten und
Überlauf; fehlendes Attribut erhält bestehenden Wert. Keine stille Abschneidung.
