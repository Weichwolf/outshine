Type: defect
State: open
Parent: 2191
Area: engine, base, io
Tags: architecture, audit, persistence
Depends:

# Persistent IO is bounded and preserves the last complete save

## Beleg und Auswirkung

Keeping.cpp Engine::save öffnet das endgültige Ziel mit wb. Short-write/close-Fehler
werden erkannt, aber die vorherige gültige Datei ist dann bereits überschrieben.
EngineHeld.h SlurpFile liest bis EOF in einen wachsenden String, ohne Bytebudget oder
ferror-Prüfung. readScenario und restore verwenden es; der Save-Schreibpfad hat dagegen
ein Größenlimit. Abbruch/Lesefehler sind deshalb nicht sauber von vollständigem Input getrennt.
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
