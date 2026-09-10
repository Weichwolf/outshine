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
- [ ] Restore veröffentlicht nur vollständigen validierten Zustand; kein Teil-Restore.
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
