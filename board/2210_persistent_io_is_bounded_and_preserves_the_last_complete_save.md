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
- SourceSet-Query-Endzustände und nichtendliche Retry-Zeiten explizit behandeln.
- Restore-Namensauflösung profilieren; Persistenzschema und vollständiger Savegame-Zustand
  bleiben Aufgaben von 2131/2141. Der Deklarationswriter ist noch kein kompletter Savegame-Pfad.

## Nächster Schritt: einmalige Antwortübergabe
Wire, Fetched und Delivery bekommen einen expliziten Consumed-Zustand. Take liefert
Payload genau einmal; Move überträgt Besitz und konsumiert die Quelle. Kopieren verbieten,
Move noexcept; Metadaten nach Take bleiben lesbar für die bestehende Retry-Auswertung.
Consumer behandeln konsumierte Antworten als Fehler. TilePool übernimmt den Byte-Vektor
per Move statt ihn nochmals zu kopieren. Keine neuen Frame-Allokationen.
Negativkontrolle: mehrfaches Take und Take am verschobenen Objekt scheitern im Altstand.
Tests prüfen Move-Konstruktion/-Zuweisung, Bytes/Metadaten, leere Antworten und Retry.

## Abnahme
- [x] Reale Write-/Close-/Rename-Fehler erhalten vorige Datei und öffentliche Save-Daten.
- [x] Gleichzeitige Writer veröffentlichen nur vollständige Produkte; exklusive Tempdateien.
- [x] Restore-Batch mit spätem Fehler erhält alle Traits; Negativkontrolle erkennt Teilpublikation.
- [x] XML-Tag-/Bytegrenzen, Reader- und Szenario-Erhaltung durch unabhängige Fixtures geprüft.
- [x] Quellantwort-Negativkontrolle erkennt den zweiten Aufruf nach ungültigem Meaning.
- [ ] Antwortbesitz und Endzustände vollständig; einmaliger Verbrauch nachweisbar.
- [ ] IO-, Parser-, Speicher- und Zeitbudgets vollständig durch Tests erzwungen.
- [ ] make lint einschließlich clang-tidy grün; neue fachliche Schritte mit Regressionen.
