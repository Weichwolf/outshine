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
Aktuell 553 Dateien geprüft, keine Formatabweichung.

## Referenzprüfung

Referenz-Pins bleiben bis zur bewussten Neubewertung stabil. Die frühere Forderung,
jede gespeicherte Referenz müsse vom aktuellen Preparer-Code stammen, erzwingt
unnötige Neuerzeugung und widerspricht dem Cache-Vertrag. Ersetzen durch Prüfung
aller deklarierten SHA-256-Pins, Bilddimensionen und vollständigen Frame-Zeitpunkte.
Erzeugungsprovenienz historisch erhalten; Ableitungscaches dürfen weiterhin über
Producer-Versionen invalidiert werden, erwartete Testergebnisse niemals automatisch.
Fehlende Pins/Inputs im Renderlauf sind ungewertet/rot, keine leeren Erfolgsfälle.

## Verbleibende Arbeit

Offene Lint-Gruppen: Tidy, öffentliche Dokumentation und Writer-Coverage.
make test als Ganzes ist nicht neu abgenommen. Shaderpaket nach 2152: 455/455
SPIR-V-Artefakte reflektiert und gegen SDL-Bindings geprüft, zehn Testgruppen grün;
8/8 Compute-Verträge aus dem tatsächlichen C++-Katalog stimmen mit Reflection überein.
Der blinde MSL-Scanner ist ersetzt; vollständige Graphics-Selektor-/Shape-Abdeckung,
Stage-Interfaces und Backend-Abnahme bleiben in 2152 offen.
Doxygen wird mit geprüftem Prozessstatus und frischem temporärem XML-Output ausgeführt.
Die XML-Dateiliste muss sämtliche öffentlichen Header aus include/ enthalten; leere,
fehlende oder unvollständige Ausgabe ist rot, auch nach einem früheren grünen Lauf.
Client-Interna sind gemäß Kommentar-/API-Vertrag kein Dokumentationsinput.
Diagnosen, Headerabdeckung, Prozessstatus und Dauer stehen getrennt im JSON-Manifest;
Diagnosezahlen behaupten keine Entitätsabdeckung oder fachliche Vertragsvollständigkeit.
Enumwerte werden zusätzlich auf fehlende Dokumentation geprüft. make doc schlägt bei
Warnungen fehl; lint sammelt sie vollständig und entscheidet anschließend selbst.
Referenz: https://www.doxygen.nl/manual/config.html (XML, WARN_AS_ERROR, Warnungsarten).
Sieben Tests mit echtem Doxygen prüfen saubere und fehlende Dokumentation, Enumwerte,
Header mit Leerzeichen, leere Inputs, fehlendes Tool, Exitfehler trotz gültiger Ausgabe,
Timeout, Signal, fehlende Ausgabe nach vorherigem Erfolg und ausgelassene Header.
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
