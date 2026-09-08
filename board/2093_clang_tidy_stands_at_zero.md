Type: bug
State: active
Area: build, all
Parent: 2188
Depends:

# Static analysis will report zero findings with preserved engine contracts

## Auftrag und Stand

Aktuelles Ziel 2026-09-08: null clang-tidy-Befunde, vollständig dokumentierte API und
belegte Architektur-SOLL-Verträge nach 2188. Tidy- und Dokumentationslücken bleiben
offen; aktuelle Zahlen und Nachweise liefern die vollständigen Make-Läufe.
Symbol-Erreichbarkeit ist seit 9b82331b ein vollständiger Verdachtsbericht, kein
Nullziel. Ungeprüfte Kandidaten löschen wäre kein zulässiger Reparaturweg.

## Entscheidung

API-Ownership/Fehlerzustände nach 2189/2190/2191 zuerst. Dokumentation beschreibt
Einheiten, Koordinaten, Lebensdauer, Threadbindung, Fehlergarantien und Invalidierung.
Interne Tidy-Befunde nach Ursache gruppieren: fehlende direkte Includes, explizite
Konversionen/Einheiten, unklare Zuständigkeiten und überkomplexe Zustandsverarbeitung.
Komplexität durch vollständige fachliche Typen und Phasen senken, keine beliebigen
Funktionshälften. Keine Warnungsunterdrückung, keine Grenzwertlockerung, kein blindes
Fixit. Bei jeder Änderung Consumer und Fehlerpfade prüfen.

Verbindlich mit umsetzen: exceptionsfreie Runtime, expected/nodiscard und passende
static_assert-Verträge nach 2194. Compiler-Schalter erst mit belegten Fehlerpfaden.

Referenzen: SDL3/Khronos für Plattform/Materialien; belegte Filament-/Cesium-/AAA-
Verfahren nach 2188. Unveröffentlichte RAGE-Interna werden nicht behauptet.

## Abnahme

- [ ] make lint meldet null Tidy-Befunde; Analysefehler dürfen nicht als null gelten.
- [ ] Öffentliche API null undokumentiert und ihre tatsächlichen Verträge geprüft.
- [ ] Relevante Konventions-/Fehler-/Lebensdauer-Tests inklusive Negativkontrollen grün.
- [ ] Bildwirksame Änderungen durch Places-PNGs und unabhängige Orakel abgenommen.
- [ ] Komplexitätsabbau erhält Determinismus, begrenzte Arbeit und Fehlersicherheit.

Andere rote Gates bleiben sichtbar und ihren WIs zugeordnet. Zielabschluss verlangt
mehr als grüne Zähler: sämtliche API-SOLL-Abnahmen aus 2188 tatsächlich nachweisen.
