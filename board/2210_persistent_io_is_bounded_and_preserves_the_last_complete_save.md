Type: defect
State: active
Parent: 2191
Area: engine, base, io
Tags: architecture, audit, persistence
Depends:

# Persistent IO is bounded and preserves the last complete save

## Beleg und Auswirkung

Engine::save veröffentlicht über WriteFileAtomically: temporäre Geschwisterdatei,
Write/Close prüfen, danach Rename. Vorige Datei bleibt bei geprüften IO-Fehlern erhalten.
ReadTextFile ersetzt SlurpFile: EOF/Lesefehler getrennt, limitierte Eingaben, RAII-FILE.
readScenario begrenzt Hauptdatei plus ausgewählte Layer auf insgesamt 16 MiB; restore
verwendet dieselbe 1-MiB-Grenze wie save. Gesamter Parser-/Allokationsbedarf bleibt offen.
ContentStore::Write verwendet bereits temporäre Datei und Rename: vorhandene Fähigkeit prüfen.

## Entscheidung

Benchmark: transaktionale Dateiveröffentlichung und begrenztes IO an Systemgrenzen.
Save in eindeutige temporäre Datei im Zielverzeichnis schreiben; vollständiges Schreiben/
Schließen und vereinbarten Durability-Vertrag erfüllen, dann atomar ersetzen. Alte Datei
bei jedem Fehler erhalten. Reader akzeptieren explizite Maximalbytes und unterscheiden
EOF, Fehler, Übergröße; Limits auch für rekursive Szenariolayer und kumulatives Laden.
Keine schleichende Lockerung, kein pauschales Abort bei behandelbarem IO-Fehler.

## Abnahme

- [x] Reale Write-/Close-/Rename-Fehler: vorherige Daten erhalten; öffentlicher Save mitgeprüft.
- [ ] Übergröße/Lesefehler/lange Layerkette: begrenzter Speicher und präziser Fehler.
- [x] Restore prüft alle Zeilen/Komponenten vor Batch-Ersatz; Fehler erhalten alle Traits.
- [x] Gleichzeitige Writer verwenden exklusive temporäre Dateien und publizieren vollständig.
- [x] Bisheriges direktes wb verletzt das öffentliche Erhaltungsoracle ohne Buildfehler.
- [ ] Make-Lint insgesamt grün; drei projektweite rote Gruppen bleiben.

## Begrenzter gemeinsamer Reader
ReadTextFile im base/io-Tier ersetzt inline SlurpFile aus EngineHeld. Explizites
Byte-Limit, NUL-Pfade ablehnen, EOF von ferror unterscheiden, FILE per RAII schließen.
Bei exakter Grenze höchstens ein zusätzliches Byte zur Übergrößenerkennung lesen.
Restore: vorhandene Save-Grenze 1 MiB. Szenario plus ausgewählte Layer: zusammen
16 MiB als gesetztes Startbudget, nicht gemessen; nach erfolgreichen Reads verbleibende
Bytes reduzieren. Kein Parser-/Allokationsgesamtbudget und kein IO-Zeitlimit behauptet.
Unabhängige Datei-Fixtures prüfen leer/exakt/zu groß, fehlend, eingebettetes NUL und
Lesefehler. Mutation ohne Budgetprüfung muss scheitern. Bestehende Parser-Regression,
öffentlicher Save-/Szenario-Vertrag und Make-Gates gemeinsam migrieren.

Nachweis: IO-Grenzfälle und Parser-Regressionsfall grün, einschließlich öffentlichem
Übergrößen- und kumulativem Layerfall. Mutant verwirft Übergrößenfehler und scheitert
ohne Buildfehler; wiederhergestellter Code grün. Lint 182/315, 32 Repository-Tests grün,
drei rote Gruppen. Zeitbudget-/Gesamtspeicherabnahme bleibt offen.

## Atomare Veröffentlichung
Gemeinsamer WriteFileAtomically-Baustein: exklusives wbx im Zielverzeichnis, begrenzte
Namenskollisionsversuche, vollständiges fwrite/fclose, dann filesystem::rename mit
error_code. RAII schließt und entfernt eigene temporäre Dateien bei Fehlern. Existierendes
Ziel niemals vor erfolgreicher Veröffentlichung öffnen/trunkieren. Gleicher Datenträger
durch Geschwisterdatei; Sichtbarkeitsatomizität, keine fsync-/Crash-Durability-Zusage.
Reale POSIX-Dateigrößenlimits im isolierten Testprozess erzeugen Write-/Close-Fehler;
Rename auf ein Verzeichnis muss verweigern. Gleichzeitige Writer dürfen nur vollständige
Produkte veröffentlichen. Reader- und öffentlicher Save-Vertrag werden mitgeprüft.

Atomare Abnahme: 15 Writer-Prüfungen sowie Assembly-/Reader-Regressionen grün.
Dateigrößenlimit erzeugt nachweislich getrennte fwrite- und fclose-Fehler. Öffentlicher
Save erhält vorherige Bytes; Altcode verletzt genau diesen Vertrag. Vier Writer mit
je acht Veröffentlichungen werden parallel gelesen: nur vollständige Produkte.
Pfad als string_view, Daten als span<const byte>; keine vertauschbaren Stringparameter.
Abschluss-Lint 182/315, 32 Repository-Tests grün; drei rote Gruppen. Crash-Durability
und Erhalt alter Dateimetadaten sind ausdrücklich nicht Teil dieses Vertrags.

## Restore-Phasen und Batch-Veröffentlichung
Restore parst bereits vor Mutation, publiziert aber über einzeln fehlbare Put-Aufrufe.
Column erhält einen Replace-Batch für trivial kopierzuweisbare Werte: alle vorhandenen
Owner/Generationen/Komponenten vorab prüfen, danach allokationsfrei ersetzen. Serialisierte
Nutzung bleibt Pflicht, keine atomare Sichtbarkeit für parallele Leser behaupten.
Saved-Trait-Zeilen separat parsen; stabile Holder-Sortierung ersetzt quadratische Suche
nach bereits gruppierten Zeilen. Wiederholte Werte behalten Dateireihenfolge (letzter gilt).
Ungültiger später Batch-Eintrag muss frühere Werte erhalten; öffentlicher Restore mit
spätem ungültigem Trait ebenfalls. Negativkontrolle vorgezogener Publikation; Lint.

Restore-Abnahme: 18 Prüfungen grün, dazu Assembly- und Save-Regression. Mutation
publiziert vor abgeschlossener Batch-Prüfung und scheitert an zwei Erhaltungsgarantien
ohne Buildfehler. Parser nutzt geliehene Zeilen; stabile Holder-Gruppierung erhält
Last-write-wins. Namensauflösung bleibt separat zu profilieren. Abschluss-Lint 181/315,
32 Repository-Tests grün, drei rote Gruppen; restore-Komplexitätsbefund beseitigt.

## XML-Taggrenzen
Xml::Parse akzeptiert derzeit beliebige Zeichen nach Schließtag-Namen und Attribute
nach dem Self-close-Slash. Dadurch passieren syntaktisch ungültige Szenarien die Grenze.
Schließtag erlaubt nach Namen nur Whitespace und >; Self-close muss unmittelbar />
sein. Unabhängige gültige/ungültige Fixtures einschließlich EOF prüfen; Altcode muss
die Ablehnungsfälle verletzen. Keine Bildänderung erwartet. Allgemeine Parserphasen,
quadratisches Anhängen von Geschwistern und Allokationsbudgets bleiben separat offen.
Taggrenzen-Abnahme: 18 Syntaxfälle grün; Altcode scheitert ohne Buildfehler.
Zeichenreferenzen und Szenario-Erhaltung ebenfalls grün. Lint: 181 tidy, 282 Doxygen,
32 Repository-Tests grün, drei rote Gruppen. Keine vollständige XML-Konformitätszusage.

## In-Memory-XML-Budget
Xml::Parse kopiert beliebig lange Eingaben vor jeder Budgetprüfung; uint32-Offsets
können bei großen Texten verengen. Eigene 16-MiB-Textgrenze vor Text_.assign, passend
zum gesetzten Szenario-Dateibudget. Das begrenzt die Eingabekopie, nicht gesamten RAM
oder Laufzeit. Exakt 16 MiB akzeptieren, ein Byte darüber ablehnen; ReadScenario muss
bei Übergröße vorheriges Dokument erhalten. Altcode-Negativkontrolle, gültiger Retry,
Parser-Regression und Lint. uint32-Darstellbarkeit der Grenze statisch absichern.
Budget-Abnahme: sieben Checks grün; Altcode verletzt die Grenze ohne Buildfehler.
Drei Parser-/Szenario-Regressionen grün; letzter Budgettest nach size_t-Korrektur grün.
Lint: 181 tidy, 282 Dokumentationsdiagnosen, 32 Repository-Tests grün; drei rote Gruppen.

## Quellantworten begrenzt behandeln
SourceSet trennt Antwortauswertung von Quellenauswahl/Transport.
Unbekanntes Meaning muss als Refused enden; bisher wiederholt die Collect-Schleife
unbegrenzt dieselbe abgeschlossene Quelle. Fake-Source liefert genau einmal ungültig,
danach Refused: Altstand braucht zwei Collect-Aufrufe, Korrektur genau einen.
Tests für Bytes, Absent-Fallback, Refused, Working, Retry-Budget/Backoff und Cancel bestehen;
OSM-Zustandserhalt bleibt grün.
Bestehende Cache-/Ledger-Semantik erhalten. Query-Endzustände, einmaliger Payloadverbrauch
und nichtendliche Retry-Zeiten bleiben eigene offene Lifecycle-/Validierungsarbeit.
