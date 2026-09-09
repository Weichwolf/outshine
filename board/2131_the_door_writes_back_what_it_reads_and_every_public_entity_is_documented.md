Type: bug
Depends: 2211
State: active
Area: include, scenario
Tags: measured, gate, door
Supersedes: 2107

# Öffentliche API erfüllt dokumentierte Verträge; Szenarien bleiben serialisierbar

## Ziel und Zuständigkeit

Jede öffentliche Entität dokumentiert Einheiten, Koordinaten, Ownership, Lebensdauer,
Invalidierung, Threadbindung, Vorbedingungen, Kosten und Fehlergarantien soweit relevant.
Implementierung gegen WI 2188 prüfen; Dokumentation eines Mangels behebt ihn nicht.
WI 2211 blockiert die vollständige Umsetzung deklarierter Provider, nicht die übrige
API-Arbeit. Zustandsübergänge: WI 2191; Exceptions/Budgets: WI 2194; Lint: WI 2093.
Benchmark-Orchestrierung bleibt im Client, die Engine liefert nutzbare Laufzeitverträge.

## Offene Arbeit

- Öffentliche Header anhand aktueller Doxygen-Diagnosen vollständig prüfen; explizite
  Parameter-/Rückgabetags ergänzen. Keine Namenswiederholung als Vertrag ausgeben.
- Declare/Read/Restore/Layer-Pfade auf Teilmutation prüfen. Kandidaten validieren und
  Fehler mit Ursache weiterreichen; stabile Vorgängerzustände ausdrücklich nachweisen.
- Alle deklarierten, unterstützten Szenariowerte verlustfrei schreiben. Reader,
  Writer und Grammatik abgleichen; keine Funktion zum Bestehen des Guards entfernen.
  Abgeleitete Laufzeitwerte gehören nicht in die Eingabegrammatik.
- Unacted-Einträge durch wirkliche Umsetzung oder ausdrückliche Ablehnung ersetzen;
  keine still getragenen Abschnitte oder doppelten Diagnosen. Provider nach WI 2211.
- Loading-Messfelder passend zu MiB/Mibit/s benennen; Durchsatz aus demselben
  Messintervall ableiten, statt kumulierte Poolbytes durch einzelne Preload-Zeit teilen.
- Root-/Providerwechsel als validierten Lebenszyklus behandeln; aktuelle Setup-
  Vorbedingung migriert vorhandene Ressourcen nicht und wird nicht erzwungen.
- Event-Vertrag mit aktiver Szene, Host-Bindung und Scrolländerung testen. Der
  vorhandene Leerszenen-Test beweist diese Verarbeitungszweige nicht.
- Terrainabfragen: Residency von möglicher Tile-Vorbereitung trennen; interne
  Koordinatengrenzen prüfen. Öffentliche Höhenabfrage validiert bereits Winkel/Abdeckung.
- Zeitsteuerung: Überlastpolitik und Frame-Arbeitsbudget bestimmen, Restzeit und
  Zustände bei fehlgeschlagenen Schritten prüfen. Positive endliche StepS und gültige
  Nachhollimits allein sind kein Echtzeitnachweis.
- Diagnose-API: Legacy-error nicht als Ergebnis letzter expected-Aufrufe verwenden.
  Messwerte können veraltet sein; Snapshot-/Frischevertrag bei Nutzung berücksichtigen.
  Timing-Ring-Allokation ist noch nicht budgetiert oder gemeinsam transaktional.

## Abnahme

- Vollständiger make lint mit null clang-tidy- und Dokumentationsdiagnosen;
  alle öffentlichen Header erfasst. Grüne Zähler ersetzen keine Vertragsprüfung.
- Unabhängige öffentliche API-Tests für Fehler, Wiederverwendung und Lebensdauer;
  Mutationskontrollen müssen am beobachteten Verhalten scheitern, nicht am Build.
- Reader → Writer → Reader erhält die deklarierte Bedeutung für alle unterstützten
  Abschnitte, einschließlich Defaults, optionaler Werte und Layer. Erst eine
  kanonische Serialisierung darf zusätzlich byte-identische Ausgabe verlangen.
- Writer-Negativkontrolle entfernt gezielt ein unterstütztes Feld; der entsprechende
  Rundlauf muss scheitern. Ein statischer Grammatikzähler allein reicht nicht.
- Bestehende Regressionen für Loading-Überlauf, Preload-Budgets, Readback-Auswahl,
  Event-Ergebnisse, Höhenkoordinaten, Zeitwerte und exakte Nachhollimits erhalten.
  Implementierungsverlauf und einzelne Messergebnisse bleiben in Git/Temp-Logs.
- Bildwirksame Änderungen über den Client rendern und PNGs selbst prüfen;
  CPU-Messzeiten nicht als GPU-Ausführungs- oder Präsentationslatenz ausweisen.
