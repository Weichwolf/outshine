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

Sechs Tests mit echtem clang-tidy prüfen Prozessfehler und Abdeckung gegen die
Compile-Datenbank. Scanner laufen vor Quellmutation; Logs liegen im System-Temp.
Formatierung nutzt die Git-Dateiauswahl und sichere Pfade. Vier clang-format-Tests
prüfen Fix/Idempotenz, Abdeckung, Toolausfall und Dateistatus einschließlich Leerzeichen.

## Referenzprüfung

Referenz-Pins bleiben bis zur bewussten Neubewertung stabil. Die frühere Forderung,
jede gespeicherte Referenz müsse vom aktuellen Preparer-Code stammen, erzwingt
unnötige Neuerzeugung und widerspricht dem Cache-Vertrag. Ersetzen durch Prüfung
aller deklarierten SHA-256-Pins, Bilddimensionen und vollständigen Frame-Zeitpunkte.
Erzeugungsprovenienz historisch erhalten; Ableitungscaches dürfen weiterhin über
Producer-Versionen invalidiert werden, erwartete Testergebnisse niemals automatisch.
Fehlende Pins/Inputs im Renderlauf sind ungewertet/rot, keine leeren Erfolgsfälle.

## Typnamen und Namespaces

EveryTypeNameIsDeclaredOnce entfernte Namespace-Kontext per grep und verlangte eine
historische Anzahl gleicher Kurznamen. Das verwechselt legale, fachlich unterschiedliche
Typen mit Duplikaten und widerspricht der Importgrenze: Gltf::Material beschreibt das
Format, outshine::Material die Engine. Auch lokale Konstantennamen sind keine globalen
Identitäten. Den unbegründeten Namenszähler entfernen; keine Ersatz-Zählerheuristik.
Compiler prüfen Sprachregeln, Tierprüfungen Abhängigkeiten. Semantische Doppelmodelle
und gemeinsam zu nutzende Mathematik fachlich über WI 2150 prüfen.

## Iterationskosten

Build-Audit-Claims nur einmal ausführen: sanitisierte Wrapper instrumentieren keine
externen Shell-/nm-Prozesse. Negativkontrollen und Shared-Harness-Prüfungen erhalten.
Weitere Claims nach belegtem Fehlernutzen und tatsächlichen Laufzeitkosten bewerten.

## Verbleibende Arbeit

Aktuelles Lint vollständig grün: 0 Tidy-Befunde (189/189 Units), Dokumentation 24/24 Header,
null Diagnosen. Fachliche API-Abnahme bleibt 2188. P0 vor Featureausbau nach 2169.
make test als Ganzes ist rot; konkrete Ergebnisse unten. Shaderpaket nach 2152: 455/455
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

## Aktueller Gesamtlauf und nächste Schritte
make test: 2792 Arme, 2775 PASS, 1 FAIL, 2 BUILD, 14 UNPREPARED; keine Timeouts/Signale.
Gemessen: 642748 ms Run + 1692318 ms Build/Vorbereitung = 2335066 ms gesamt.
Die alte Run-Grenze 230000 ms ist überschritten; Population und Kosten neu bewerten,
nicht einfach das Limit hochsetzen. Wiederaufbau/Pruning kleiner Fixtures kostet IO.
- Beide veralteten Testaufrufe migriert: WGS84-Konstante im nativen Namespace,
  Eye::HasExplicitCamera statt StandsInside. GeographicLib regulär/sanitisiert und
  GPU-Lens bestehen (3/3); unveränderte Prüfkriterien. make lint vollständig grün.
- MipmappedChessRepeatsLinearPixels: 420 lineare Kanäle weichen ab, Tiefe identisch;
  unveränderte exakte Prüfung, Untersuchung in 2179.
- Neun Place-Renders warten auf Vegetation; P0-Abnahmen wie vereinbart explizit
  ohne Vegetation konfigurieren, separate Vegetationstests erhalten. Keine Zeitlockerung.
- Gelände-Audits ohne Vegetation: Footprint/Normals/Lattice im warmen Cache grün.
  Air/Sun erreichen jetzt Bildprüfungen (8107/5412 ms), beide FAIL statt Timeout:
  Air verletzt Wiederholbarkeit; Sun verletzt monotone Bildhelligkeit bei 5/30/75°.
  Sun mischt Belichtungshistorie mit Lichtprüfung: 5° nach 75° fällt von 37,022 auf
  1,140; 30° liegt mit 35,774 unter 5°. Ursache nicht allein daraus ableitbar.
  Fixierte Belichtung, zeitlich definierte Aufnahme und unabhängige Lichtgeometrie
  in 2218 spezifizieren. Bis dahin keine Grenzwerte oder Assertions lockern.
  Air: ungefragten Komplettdump der Measures und build/*.rgba entfernt; fachliche
  Bandmessungen, Fehler und sämtliche Assertions bleiben erhalten.
- ClaimCorpus erkennt eigene PID als fremden Runner: Reentranz/Ownership korrigieren.
- Automatischer Rebuild ruft prepare.py all auf, das bei Oracle-Manifesten Blender
  starten kann. Normalen Testpfad von Referenzerzeugung trennen (2218).

## Writer-Gate
Literalinventar bleibt Diagnose; Tool-/Analysefehler sind rot. Alle 17 ScenarioWrite-
Tests sind verpflichtendes Verhaltensgate und aktuell grün. API-Abnahme bleibt offen.

Lint schreibt standardmäßig eine Ergebniszeile; vollständige Ausgabe im System-Temp,
bei Fehlern höchstens zwölf Diagnosezeilen. --verbose erhält die Detailansicht.
