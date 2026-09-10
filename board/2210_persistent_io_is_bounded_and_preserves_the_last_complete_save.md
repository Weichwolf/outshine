Type: defect
State: active
Parent: 2191
Area: engine, base, io
Tags: architecture, audit, persistence
Depends:

# Persistent IO is bounded and preserves the last complete save

## Beleg und Auswirkung

Keeping.cpp Engine::save öffnet das endgültige Ziel mit wb. Short-write/close-Fehler
werden erkannt, aber die vorherige gültige Datei ist dann bereits überschrieben.
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

- [ ] Injizierter Short-write, close-/rename-Fehler: alte Save-Datei unverändert lesbar.
- [ ] Übergröße/Lesefehler/lange Layerkette: begrenzter Speicher und präziser Fehler.
- [ ] Restore veröffentlicht nur vollständigen validierten Zustand; kein Teil-Restore.
- [ ] Gleichzeitige Save-Versuche kollidieren nicht in gemeinsamen temporären Namen.
- [ ] Negative Kontrolle direktes wb verletzt Erhaltungsoracle; Make-Lint und IO-Tests.

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
drei rote Gruppen. Keine atomare Save-/Zeitbudget-/Gesamtspeicherabnahme behauptet.

## Atomare Veröffentlichung
Gemeinsamer WriteFileAtomically-Baustein: exklusives wbx im Zielverzeichnis, begrenzte
Namenskollisionsversuche, vollständiges fwrite/fclose, dann filesystem::rename mit
error_code. RAII schließt und entfernt eigene temporäre Dateien bei Fehlern. Existierendes
Ziel niemals vor erfolgreicher Veröffentlichung öffnen/trunkieren. Gleicher Datenträger
durch Geschwisterdatei; Sichtbarkeitsatomizität, keine fsync-/Crash-Durability-Zusage.
Reale POSIX-Dateigrößenlimits im isolierten Testprozess erzeugen Write-/Close-Fehler;
Rename auf ein Verzeichnis muss verweigern. Gleichzeitige Writer dürfen nur vollständige
Produkte veröffentlichen. Reader- und öffentlicher Save-Vertrag werden mitgeprüft.
