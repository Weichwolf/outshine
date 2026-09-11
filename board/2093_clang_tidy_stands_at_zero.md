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
API-Ownership/Fehlerzustände nach 2190/2191 zuerst. Dokumentation beschreibt
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
## JSON-Eingabegrenze

ParseValueInside bündelt Container, Literale und Zahlen (Komplexität 98).
ParseString akzeptiert unbekannte Escapes und rohe Steuerzeichen; fehlgeschlagene
Dokumente geben Teilknoten frei. Grammatikphasen trennen, Escapes vor Decode prüfen,
Referenzen nur für erfolgreich geparste Dokumente freigeben. Null-/übergroße Eingaben
vor Kopie begrenzen. RFC-8259-Beispiele, ungültige Escapes/Separatoren/Zahlen und
Parsefehler nach gültigem Präfix unabhängig prüfen. Über-/Unterlauf von double
ablehnen statt auf Höchstwert/null zu sättigen; darstellbare Subnormale, signed zero
und Grenzwerte erhalten. Unicode-Surrogatersatz separat behandeln.
Referenz: https://www.rfc-editor.org/rfc/rfc8259.html

## PNG-Höhendaten

ReadPng mischt Container, Header, Inflation und Zeilenfilter (Komplexität 42).
CRC, Kompressions-/Filtermethode und Abschluss werden bisher nicht geprüft.
Containerprüfung und Filterrekonstruktion fachlich trennen; beschädigte Chunks,
fehlendes IEND und ungültige Methoden ablehnen, bevor Höhendaten publiziert werden.
RGB/RGBA-Bits unverändert erhalten, keine Farbkonvertierung. Größen vor Addition
prüfen. Unabhängiger Paeth-Pixeltest und gezielt korrumpierte Container prüfen
Erfolg und Ablehnung. Referenz: https://www.w3.org/TR/png-3/

## Generierter C++-Code

CrownBuild.h wird vom Provenienzgenerator erzeugt und bleibt Teil der Analyse.
Include-Guard und constexpr string_view statt pragma once und C-Array; der Consumer
übernimmt den Digest weiterhin als eigenen String. Hash-Algorithmus und Dateiauswahl bleiben gleich; der Generator selbst wird mitgehasht.

## Materialindex-Konversion

PieceSurface unterscheidet Geometrie- und registrierte Materialtabellen. Nackte
Ganzzahlen dürfen die Geometriedomäne nicht implizit auswählen. Explizite Konstruktion,
constexpr-Fabrik für registrierte Referenzen; statische Negativprüfung und bestehender
Instanz-/Materialtest prüfen Index, Domäne und unveränderte Darstellung.

## Cluster-Cooking

CookClusters hat zwei produktive Aufrufer: Render-Shape und Gebäude-Bake. Ungültige
Indizes werden bisher nur bei Bounds übersprungen, aber an GPU-Verbraucher weitergegeben;
Float-Überläufe können Morton-Codes und Bounds ungültig machen. Eingangsview, Layout,
Indexbereich und Ergebnisgrenzen explizit validieren; Fehler bis Shape-/Bake-Verbraucher
weitergeben. Dreiecke vollständig erhalten, Morton-Gleichstände deterministisch ordnen,
konservative endliche Bounds unabhängig prüfen. CookDag und seine exklusiven Ergebnis-
felder haben weder Aufrufer noch Tests und sind keine installierte öffentliche API.
Den ungenutzten Weld-Prototyp entfernen; er ist kein implementiertes HLOD-System.
Cluster-Metriken gehören zur jeweiligen Shape, nicht in globale Atomics; Kamera-
Hilfsshapes und andere Engine-Instanzen dürfen die Werte nicht überschreiben.
Nachweis: Sanitizer-/Fehlerfälle, unabhängige Dreiecks- und Sphere-Prüfung sowie Places.

## Abnahme

- [ ] make lint meldet null Tidy-Befunde; Analysefehler dürfen nicht als null gelten.
- [ ] Öffentliche API null undokumentiert und ihre tatsächlichen Verträge geprüft.
- [ ] Relevante Konventions-/Fehler-/Lebensdauer-Tests inklusive Negativkontrollen grün.
- [ ] Bildwirksame Änderungen durch Places-PNGs und unabhängige Orakel abgenommen.
- [ ] Komplexitätsabbau erhält Determinismus, begrenzte Arbeit und Fehlersicherheit.

Andere rote Gates bleiben sichtbar und ihren WIs zugeordnet. Zielabschluss verlangt
mehr als grüne Zähler: sämtliche API-SOLL-Abnahmen aus 2188 tatsächlich nachweisen.

## glTF-Texturtabelle transaktional aufbauen
ResolveFileSurface ersetzt Raster vor Fehlerprüfung und hinterlässt ungültige Bindungen.
Kandidat mit frischen Bindungen, gemeinsamer Socket-Dekodierung und getrennten
Materialmap-/Carrier-Prüfungen; Slots/Materialindizes vor Zugriff abgeglichen.
Altcode verletzt zwei Negativkontrollen: Rasteradresse/Pixels bei Fehler erhalten;
gültiger Ersatz entfernt alte Maps und erhält Filter/Wrap. Native Geometrie-/Materialmigration bleibt separat offen.

## Lichtprojektion in fachliche Phasen trennen
LightVisibilityStage::Build: Mittelpunkt der schattenwerfenden Platzierungen als
private CasterCentre-Operation trennen; Projektions-/Texelraster-/Frame-Arithmetik
unverändert. Analytische Achsen-/Reverse-Z-/PreView-Kontrolle und orthografischer
Rendervertrag grün; Vorzeichenmutant verletzt Frame-Orakel. Tidy-Befund beseitigt.
Numerische Eingabevalidierung und degenerierte Sonnen-/Up-Basen separat prüfen;
der Strukturumbau beweist noch keine vollständige Schatten-SOLL-Erfüllung.

## Build-Cache bei entfernten Headern
UpToDate akzeptiert fehlende Voraussetzungen: shell `-nt` ist dann wahr. Beim
Verschieben von Keyframes bleibt Pose mit altem Klassenlayout im Objektcache.
Existenz jeder Quelle und Compiler-Abhängigkeit vor dem Zeitvergleich verlangen.
Regression führt die echte Runner-Funktion mit gültigem, neuerem und entferntem
Header sowie entfernter Quelle aus; danach Import-/Posefälle ohne Cache-Löschung
neu gebaut: acht Prüfungen grün, Altcode verletzt die Cache-Negativkontrolle.
Keine Bildänderung beabsichtigt; vollständiges lint bleibt Pflicht.
## Vegetationskatalog als atomarer Snapshot
Load leert Bestand vor Fehlern und lässt Friction_ beim Reload stehen. IO bis 1 MiB
und Kandidatenpublikation; Blätter, Substrat, Templates, Regeln, Layer, Höhenlimit als
Parsing-Phasen. Altcode verletzt Erhaltung/Reload; fünf Regressionen grün. Regelzahlen vor Cast prüfen.
Rank 0..255 (ClassBuilder packt acht Bits), Lanes >=0, Priority int; Maße im Float-Bereich.
Flags bool oder 0/1; Altcode rot, zwei Regressionen grün. Andere Template-Zahlen offen.

## UI-Markup-Parser
Read publiziert Kandidaten; Fehler erhalten Baum/Adressen, Wiederverwendung setzt Scripted zurück.
Parserkandidat mit Text-/Tag-/Attribut-/Raw-Text-Phasen; erst vollständig publizieren.
Explizite Budgets: 1 MiB Quelltext, 65536 Knoten inkl. Dokument, 65536 Attribute,
256 offene Elemente. Entity-Suche auf bestehende Referenzlänge begrenzen.
Drei Parser-/Input-Tests grün, Altcode rot. HTML-Entity-/Raw-Endtag-Konformität bleibt offen.
