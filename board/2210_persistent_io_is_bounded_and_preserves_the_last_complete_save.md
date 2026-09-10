Type: defect
State: active
Parent: 2191
Area: engine, base, io
Tags: architecture, audit, persistence
Depends:

# Begrenztes IO und eindeutige Datenübergaben

## Ziel
Behandelbare IO-/Formatfehler ergeben checked Fehler und erhalten den zugesicherten
vorigen Zustand. Eingaben, Arbeitsmengen und Speicher sind begrenzt. Datenübergaben
haben einen Besitzer und explizite Zustände; keine leeren Zweitlieferungen als Erfolg.

## Vorhandene Verträge
- WriteFileAtomically: exklusive temporäre Geschwisterdatei mit begrenzten Versuchen,
  vollständiges fwrite/fclose, dann Rename mit error_code. RAII räumt eigene temporäre
  Dateien auf. Engine::save und Client-Roundtrip nutzen diesen Pfad.
  Sichtbarkeitsatomizität, keine fsync-/Crash-Durability oder Metadatenerhaltung zugesagt.
- ReadTextFile: RAII-FILE, EOF/Lesefehler getrennt, explizites Eingabebudget.
  Szenario plus ausgewählte Layer zusammen höchstens 16 MiB, Restore/Save 1 MiB.
  Parser-Gesamtspeicher und Laufzeit sind damit noch nicht begrenzt.
- Restore: erst parsen und alle Owner/Generationen/Komponenten prüfen, dann geprüfter
  Replace-Batch ohne Allokation für triviale Werte. Stabile Holder-Sortierung erhält
  Last-write-wins. Serialisierte Nutzung, keine atomare Sichtbarkeit paralleler Leser.
- XML: 16-MiB-Grenze vor Eingabekopie mit statisch geprüfter uint32-Darstellbarkeit;
  strikte Schließtag-/Self-close-Grenzen. Ablehnung erhält voriges Szenariodokument.
- SourceSet trennt Quellenauswahl/Transport von Antwortauswertung. Unbekanntes Meaning
  endet nach einem Quellaufruf als Refused, statt dieselbe Quelle unbegrenzt aufzurufen.
  Bytes, Absent-Fallback, Working, Retry-Budget/Backoff und Cancel sind geprüft.

## Offene Arbeit
- ContentStore-IO und Cachepfade auf Fehler, Maximalbytes, Besitz und sichere Veröffentlichung
  prüfen; vorhandene temporäre Veröffentlichung wiederverwenden statt duplizieren.
- Read-/Parser-Gesamtspeicher, lange Layerketten und Zeitbudgets numerisch begrenzen.
- XML-Parserphasen trennen; quadratisches Geschwister-Anhängen beseitigen, Konformitäts-
  und Randfälle prüfen. Bestehende Grenzprüfungen nicht lockern.
- Restore-Namensauflösung profilieren; Persistenzschema und vollständiger Savegame-Zustand
  bleiben Aufgaben von 2131/2141. Der Deklarationswriter ist noch kein kompletter Savegame-Pfad.

## Einmalige Antwortübergabe
Wire, Fetched und Delivery haben einen expliziten Consumed-Zustand. Take liefert
Payload genau einmal; Move überträgt Besitz und konsumiert die Quelle. Kopieren verbieten,
Move noexcept; Metadaten nach Take bleiben lesbar für die bestehende Retry-Auswertung.
Consumer behandeln konsumierte Antworten als Fehler. TilePool übernimmt den Byte-Vektor
per Move statt ihn nochmals zu kopieren. Keine neuen Frame-Allokationen.
Negativkontrolle: Altstand verletzt die Take-/Move-Verträge; zurückgesetzte TilePool-Kopie
verletzt den Allokationsnachweis. Vier Regressionen bestehen, Cache bleibt unabhängig.
Tests prüfen Move-Konstruktion/-Zuweisung, Bytes/Metadaten, leere Antworten und Retry.

## Abnahme
- [x] Reale Write-/Close-/Rename-Fehler erhalten vorige Datei und öffentliche Save-Daten.
- [x] Gleichzeitige Writer veröffentlichen nur vollständige Produkte; exklusive Tempdateien.
- [x] Restore-Batch mit spätem Fehler erhält alle Traits; Negativkontrolle erkennt Teilpublikation.
- [x] XML-Tag-/Bytegrenzen, Reader- und Szenario-Erhaltung durch unabhängige Fixtures geprüft.
- [x] Quellantwort-Negativkontrolle erkennt den zweiten Aufruf nach ungültigem Meaning.
- [x] Wire/Fetched/Delivery nur verschiebbar; einmaliger Take-Verbrauch und Puffertransfer geprüft.
- [x] Query-Endzustände, Cache-Abschluss, Move-Ticket und SourceSet-Bindung geprüft.
- [x] Ungültige Retry-Zeiten/Uhren und Deadline-Überlauf terminieren ohne Neustart.
- [ ] IO-, Parser-, Speicher- und Zeitbudgets vollständig durch Tests erzwungen.
- [ ] make lint einschließlich clang-tidy grün; neue fachliche Schritte mit Regressionen.

## Query-Zustandsmaschine
Ready/InFlight/Backoff/Finished statt indirekter Ticket-/Zeit-Flags.
Jedes terminale Ergebnis und Abandon schließen die Query; danach Consumed ohne Quellen-
oder Ledger-Zugriff. Move-Konstruktion überträgt Ticket und konsumiert die Quelle;
Move-Zuweisung verbieten, damit ein aktives Ticket nicht ohne Abbruch überschrieben wird.
Query an erzeugendes SourceSet binden; fremdes Collect ablehnen, Query unverändert.
SourceSet und benutzter Transport müssen aktive Queries überleben; Abandon bleibt explizit.
Tests: erneutes Collect nach Delivery/Refusal/Absent/Undeclared/Cancel, Move eines aktiven
Tickets und Owner-Verwechslung bestehen; Cache-Abschluss ebenfalls. Altstand verletzt
die Negativkontrolle. Fünf Regressionen grün, gültiger Retry unverändert.

## Retry-Zeitvertrag
Retry-After muss endlich, nichtnegativ und in Millisekunden darstellbar sein.
Ungültige Angaben beenden die Query als Refused mit endlichem 4000-ms-Fallback.
Backoff benötigt eine endliche, nichtnegative Uhr und eine darstellbare zukünftige
Deadline; ungültige Berechnung darf weder Retry-Zähler noch Transport starten.
Uhrwerte während Backoff ebenfalls validieren. Gültige Serververzögerungen erhalten,
exponentiellen lokalen Backoff ohne überlaufende Zwischenwerte berechnen.
Fake-Clock prüft NaN/Inf/negative Werte, Konversions-/Deadline-Überlauf und verlorene
Zeitauflösung. Reguläre Deadlines und lange Serververzögerungen bleiben erhalten.
Altstand scheitert an der Negativkontrolle; vier Datenpfad-Regressionen bestehen.
