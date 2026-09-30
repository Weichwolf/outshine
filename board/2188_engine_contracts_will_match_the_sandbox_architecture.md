Type: debt
State: active
Architecture: ready
Priority: P0
Area: engine, world, render, simulation, audio
Tags: architecture, integration
Parent: 2169
Depends:

# Ausführbarer Weg zur Webcam-Annäherung

## Besitzer und Datenfluss

Provider liefern OSM/DEM/Wetter mit Provenienz. World hält semantische Formen und Zustand.
Generatoren erzeugen native Geometrie/Materialparameter. Engine koordiniert begrenzte Jobs,
Residency und Publikation. Render konsumiert Snapshots; keine Generatoraufrufe im Frame.
Double-Welt und kamera-relative GPU-Daten teilen Ursprung und Höhenbezug.
Quellwechsel invalidieren gezielt; Zeit/Wetter erzeugen keine unveränderte Stadt neu.

## Kleine ausführbare Reserve

| Reihenfolge | WI | Nächste vollständige Lieferung | Owner |
|---|---|---|---|
| 1 / P0 | 2324, 2170 | Acht Webcam-Kameras mit Archiv und transparentem Kalibrierstatus | assets/places, client/PlaceCamera, Vergleichsmanifest |
| 2 / P0 | 2319, 2322 | Vollständige Stadt und Rundumsicht innerhalb der Lade-/Framegrenzen | StructureBuildQueue, StructureCellPlanner, TilePieces, GroundPublication |
| 3 / P0 | 2166, 2280, 2173, 2145 | Originalformen und Bauwerksklassen; zusammenhängende Gewässer ohne künstliche Uferwände | GroundLattice/DEM, OsmSourceSnapshot, BuildingShape/Bake, WaterField |
| 4 / P1 | 2171, 2138 | Rosenheim/Husum: Dach, Wand, Glas und Sockel klar lesbar | BuildingMesh, FacadeUv, Materialshader |
| 5 / P1 | 2172, 2140 | Koerbersee/Rosenheim: bedeckter Himmel und kohärentes Weltlicht | WeatherSnapshot, SkyStage, Cloud-Komposition/Irradiance |

Materialarbeit kann parallel zu ungelöster Ferndarstellung vorbereitet werden; keine
zweite Architekturkampagne. Jeder Schritt endet im geöffneten Bild. Bei zwei Reparaturen
ohne Bildgewinn Ansatz neu entscheiden und ein unabhängiges sichtbares Feature liefern.
2138 braucht Gebäudesemantik, nicht fertige weltweite Navigation. Wolken brauchen den
Snapshot, nicht den vollständigen Schneesolver. Vegetation bleibt zuletzt.

## Anschlusslieferungen

2129: Reflexion vorhandener Wasserflächen nach 2327, unabhängig vom vollständigen
Küstenumbau in 2145. 2167/2128/2155: Schattenfüllung,
lokales Nachtlicht, stabile HDR-Antwort. 2325: Nässe, Schnee und Schmelze aus Wetterzustand.
2111/2176/2282: danach standortgerechte Dichte und Phänologie im gemeinsamen Budget.
Physik, Hockenheim-Runde, Verkehrssimulation, Audio und Spiel folgen nach dem Meilenstein.

## Gemeinsame Abnahme

2324 benennt genau acht Standard-Places. Kamerafit, Form, Materialien, Licht/Wetter und
Kosten getrennt bewerten. Alle Bilder öffnen; kein erfolgreicher Hash ersetzt Bildqualität.
Keine Grenzwerterhöhung, verschwundene Inhalte oder schlechtere Straßen als Optimierung.
Format, fokussierte Suite, vollständiger Lint/API und Place-Gate gehören zur Lieferung.
Board enthält Entscheidungen und kurze Fertig-Kriterien; Messprotokolle bleiben in Temp/Git.
