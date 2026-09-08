Type: bug
State: active
Parent: 2188
Area: test, gate
Tags: measured, gate
Depends: 2093, 2131, 2152

# The gates distinguish a clean result from a missing check

## Vertrag und nachgewiesener Stand

Benchmark: nachgewiesene Prüfabdeckung und explizite Tool-Ergebnisse statt Zählerheuristik.
Der Tidy-Runner gleicht alle src/*.cpp mit der Compile-Datenbank ab, einschließlich
Main.cpp. Er protokolliert Status und Diagnose je Unit und trennt vollständig sauber,
vollständig mit Befunden sowie unvollständig/fehlgeschlagen. Null Befunde sind bei
vollständigem Erfolg zulässig. Pfade werden vor dem Deduplizieren kanonisiert.

Nachgewiesen am 2026-09-08: 175/175 erfolgreiche Toolaufrufe, 215 eindeutige Befunde;
66/66 Repository-Regeln grün. Die vorherige Warnungszahl 182 war unvollständig:
Main.cpp fehlte, Statusfehler wurden ignoriert, Headerpfade nicht normalisiert.
Sieben dabei sichtbar gewordene Compilerdiagnosen an sechs Units sind behoben:
optionale Aggregate-Member an den Erzeugungsstellen vollständig initialisieren.
Leere Container/Spans/Callbacks behalten ihre bisherigen Werte.

Sechs Tests mit tatsächlichem clang-tidy und injizierten Prozessfehlern prüfen den
Ausführungsvertrag. Fixtures übernehmen die echte Compile-Konfiguration aus der
Datenbank. Scanner-Tests laufen weiter vor Quellmutation. Logs und Ausführungsmanifest
liegen im System-Tempverzeichnis; keine Warnungsunterdrückung als Reparatur.

Formatierung: make format und Lint benutzen dieselbe Git-basierte Dateiauswahl, mit
sicherer Übergabe von Pfaden. Leere Abdeckung und Toolfehler sind rot; Dateiverdikte
werden gezählt, nicht Quelltextzeilen in Diagnosen. Der alte Bericht „740 Dateien“
war falsch: 20 von 546 Dateien hatten Formatdiagnosen. Keine Stilregeln gelockert.
Vier Tests mit echtem clang-format prüfen Diagnostics/Fix/Idempotenz, leere Abdeckung,
fehlendes Tool sowie eigene/ignorierte/gelöschte Dateien und Pfade mit Leerzeichen.
546 Dateien geprüft, keine Formatabweichung.

## Verbleibende Arbeit

Aktuelle Lint-Gruppen rot: Tidy (212), öffentliche Dokumentation (673), Writer-Coverage.
make test als Ganzes ist nicht neu abgenommen. Shaderpaket nach 2152: 455/455
SPIR-V-Artefakte reflektiert und gegen SDL-Bindings geprüft, acht Testgruppen grün.
Der blinde MSL-Scanner ist ersetzt; vollständige Renderer-Selektor-/Shape-Abdeckung,
Stage-Interfaces und Backend-Abnahme bleiben in 2152 offen.
Gate-Dauer und Abdeckung je Teil ausweisen; keine langsamen Pflichtprüfungen entfernen.
Der Client-Link meldet doppelte rpath-/SDL3-Einträge als Warnungen trotz Exit 0.
Transitive Linkflags mit korrekter Reihenfolge konsolidieren; Linkerwarnungen
plattformgerecht als Fehler behandeln und den Negativfall prüfen.

## Abnahme

- [x] Vollständiger warnungsfreier Fixture-Lauf grün; echte Tidy-Diagnose rot.
- [x] Fehlendes Tool, leere/falsche Datenbank, doppelte Konfiguration, Parsefehler,
      Prozessabbruch und Timeout rot; auch neben regulären Warnungen einer anderen Unit.
- [x] Jede Source-Unit einschließlich Client-Einstieg erreicht die Compile-Datenbank;
      vollständiger erfolgreicher Lauf durch individuelles Statusmanifest belegt.
- [ ] Jeder Gate-Teil berichtet tatsächliche Abdeckung, Ergebnis und Dauer;
      Artefaktnachweis durch vollständige Renderer-Vertragsprüfung ergänzen.
- [ ] make test und make lint vollständig grün; Zeitgrenzen aus gemessenem Umfang
      begründen. Langsame Gate-Teile reparieren, nicht aus der Pflicht entfernen.
