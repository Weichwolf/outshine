Type: debt
State: active
Architecture: ready
Priority: P0
Area: engine, world, render, simulation, audio
Tags: architecture, integration
Parent: 2169
Depends:

# Engine-Design und ausführbarer Weg zur Outshine-Sandbox

## Zuständigkeiten und Datenfluss

| System | Besitzt und liefert | Grenze |
|---|---|---|
| Provider / world | OSM-/DEM-Daten, Provenienz, semantische Netze und Gebäudeattribute | Keine Rendergeometrie als Navigationsquelle |
| generators | Native Straßen, Gebäude, Gelände, Wasser; Materialkoordinaten und Kontaktprodukte | Kein Renderer, kein Szenario-Sonderfall |
| engine streaming | Begrenzte Jobs, Abbruch, Residency und konsistente Publikation | Keine fremden Generatoralgorithmen im Koordinator |
| simulation | Fester Tick, Körper, Fahrzeuge, Agenten, Interaktion und Spielzustand | Rendering interpoliert, bestimmt aber nicht Physik oder Regeln |
| render | Sichtbarkeit, Instancing, LOD, Materialauswertung, Licht, Schatten und Atmosphäre | GPU-Produkte mit eindeutiger Lebensdauer; gemeinsame Weltreferenz |
| audio | Räumliche Quellen und akustische Szene aus Weltzustand | Eigene Echtzeitgrenzen; keine Renderabhängigkeit |
| scenario / UI | Deklarative Inhalte, Regeln und begrenzte Commands; versioniertes Savegame | Skripte besitzen weder Weltobjekte noch Renderer |

Ein engine-eigenes Geometriemodell; Importformate enden am Adapter. Weltpositionen Double,
GPU kamera-relatives Float. Navigation, Kontakt und Darstellung teilen stabile Identitäten.
Quellrevisionen reisen mit Produkten; veraltete Ergebnisse dürfen keine neuen verdrängen.
Fehler lassen eine gültige Darstellung stehen oder melden eine sichtbare Lücke ausdrücklich.
Sichtbarkeit und Budget begrenzen Arbeit vor Erzeugung/Upload; Überlast ist kein stiller Datenverlust.

## Jetzt ausführen

| Reihenfolge | WI | Fertiges Ergebnis | Besitzer / konkreter nächster Schritt |
|---|---|---|---|
| 1 / P0 | 2319 | Wien zeigt vollständige Gebäudedeckung | GroundPublication/TilePieces: Verlust zwischen Quelle, Bake und aktiver Darstellung lokalisieren und reparieren |
| 2 / P0 | 2281, 2121, 2133 | Ein durchgehend nutzbarer Straßenraum | RoadAlignment/Contact und semantisches Netz: Knoten, Straßenprofil, Geländeanschluss und getrennte Ebenen verbinden |
| 3 / P1 | 2171 | Lesbare Baustoffe statt flacher Farbflächen | GroundMaterials/SubjectMaterials/Shader: Asphalt, Beton, Putz, Glas und Dachmaterial in derselben Szene |
| 4 / P1 | 2173 → 2138 | Gebäude mit plausibler Masse und echter Nahgeometrie | BuildingShape/BuildingMesh: Semantik, Hof, Eingang, Fensterlaibung und Dachabschluss |

Materialarbeit braucht keinen fertigen Wetter-/NPC-/Vegetationsausbau. Fassadendetails brauchen
keinen vollständigen weltweiten Router; lokale zugängliche Straßenfront reicht als Eingang.
Ein blockierter Schritt sperrt nur seinen Pfad. Bereits begonnene LOD-Arbeit abschließen,
aber nicht vor vollständige Stadt und nutzbare Straße schieben. Keine weitere CPU-Präzisionsreserve.

## Danach integrieren, nicht als getrennte Demos stehen lassen

| Lieferung | WIs | Einbindung in dieselbe Szene |
|---|---|---|
| Physisches Fahren/Gehen | 2127, 2297, 2261 | Steuerung → fester Tick → Straßen-/Terrainkontakt → Kamera und Audio |
| Glaubwürdiges Gesamtbild | 2167, 2128, 2129, 2155 | Materialien mit Himmel, Schatten, Reflexionen und konsistenter Belichtung verbinden |
| Wetter und Tageszeit | 2172, 2140, 2213, 2212 | Ein Zustand steuert Luft, Wolken, Oberflächen, Licht und Geräusche |
| Belebte Straßen | 2136, 2130 | Agenten verwenden logische Wege und gültige Kontakte; Entfernung reduziert Aufwand |
| Spielbare Sandbox | 2141, 2135, 2242, 2151 | Interaktion/Regeln/UI und persistenter Zustand über die öffentliche API |
| Vegetation zuletzt | 2111, 2176, 2282 | Standortgerechte Pflanzen ergänzen fertige Weltprodukte und teilen das Framebudget |

## Laufzeit ist Teil jeder Lieferung

2132 lädt vor Bewegung. 2228 zählt tatsächliche Besitzer; 2298 wählt gültige residente Details.
2314 verteilt Detail erst auf Basis gemessener Kosten-/Qualitätsstufen. Keine allgemeine
Budgetmaschine als Vorbedingung für einen sichtbaren Baustoff oder eine funktionierende Straße.
Fehlende Gebäude, verlorene Kontakte und Überschreiben aktueller Quellen sind Fehler, keine LOD.
Runtime-Zertifikate dürfen nicht optimistisch sein; sie sind Mittel für eine bessere Szene.

## Arbeitsvertrag

2169 beschreibt den Endzustand und die Lieferstufen. Feature-WIs halten Ergebnis, vorhandene
Fähigkeit, Besitzer, Daten-/Fehlerfluss, nächste Implementierung und kurze Fertig-Kriterien.
Depends nennt nur technische Blocker, Parent nur Zugehörigkeit. Keine Test-Tagebücher;
konkrete Läufe, Mutationen und Messprotokolle stehen in Git und System-Temp-Logs.
Code bleibt durch Format, fokussierte Tests, vollständigen Lint/API und alle Places abgesichert.
Abnahmebefehle: make format; betroffene make suite; LINT_JOBS=2 make lint;
outshine-client run/render gemäß Szenario/Asset, alle zehn Place-PNGs persönlich öffnen.
Die Widerlegung ist konkret: eine Lieferung ohne ihr sichtbares/spielbares Ergebnis bleibt offen.
