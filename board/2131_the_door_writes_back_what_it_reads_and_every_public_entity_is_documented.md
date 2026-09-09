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

## Nächster Struktur-Schritt

ReadScenario bündelt sämtliche Inhaltsdomänen (Cognitive Complexity 103).
Abschnittsleser für Quellen, Assets, UI/Input, Entitäten, Räume, Audio, Tabellen,
Events, Kameras und Bodies extrahieren; zentrale Funktion koordiniert Reihenfolge,
Grammatik und Unread-Prüfung. Daten-/Fehlerverhalten erhalten, keine neue API.
Vorhandene Parser-, Projektions- und Assembly-Tests sowie vollständiger Lint;
kein Bildunterschied erwartet, da ausschließlich Zuständigkeiten getrennt werden.

ReadScenario publiziert erst nach Grammatik, allen Abschnittslesern und Unread-
Prüfung einen vollständigen Kandidaten. Fehler erhalten das Zielobjekt; Erfolg
löscht alte Diagnosen. Nothrow-Move-Assignment als Publikationsinvariante prüfen.
Tests für frühe XML-/Grammatikfehler, späte Asset-/Tabellen-/Unread-Fehler und
anschließenden Erfolg; bestehende Listen dürfen weder gelöscht noch ergänzt werden.

Writer-Physik: deklarierten dial/stepS/mostStepsInArrears schreiben; bisher
verschwindet der ganze Abschnitt. Zahlen roundtrip-fähig statt %.12g ausgeben,
XML-Attributtexte escapen. Test liest Ausgabe erneut und prüft exakte Schrittzeit,
Nachhollimit und Sonderzeichen; Negativkontrolle gegen ausgelassenen Abschnitt.

XML-Attributwerte: Referenz https://www.w3.org/TR/xml/#AVNormalize und #NT-CharRef.
Beim Parsen einmal dekodieren: amp/lt/gt/quot/apos, dezimale/hexadezimale
Zeichenreferenzen; ungültige XML-Codepoints/Referenzen ablehnen. Literal-Whitespace
normalisieren, referenzierte Tabs/Zeilenumbrüche erhalten. Bestehende Offsets durch
Verkürzung innerhalb des Attributbereichs erhalten, keine rekursive Expansion.
Writer schreibt Tab/CR/LF als Zeichenreferenzen. Unabhängige XML-Fälle neben dem
Physik-Rundlauf; das ist kein Nachweis vollständiger XML-Konformität des Parsers.

XML-Dekodierung verwendet einen wiederverwendeten Scratch-String pro Parse statt
einer Allokation pro langem Attribut. Kapazität höchstens bisher größte Eingabe;
Scratch lebt nur während Parse. Wiederverwendung muss alte Inhalte löschen und
Referenz-/Normalisierungsfehler unverändert erkennen.
