Type: feature
State: active
Priority: P0
Architecture: ready
Parent: 2169
Area: world, generators
Tags: webcam, measured
Depends:

# OSM semantics reach generators and unknowns remain explicit

## Ergebnis und vorhandene Fähigkeit
Originalklassen, Parts, Höhen und Dächer bestimmen Konstruktion und Material.
Unbekannte, widersprüchliche und plausibel ergänzte Werte bleiben unterscheidbar.
`OsmXmlReader`/`OsmChunkSetLoader` erhalten Nodes, Ways, Relations und sämtliche Tags.
`OsmElements` prüft konsumierte Referenzhüllen; fremde offene Fernrelationen bleiben erhalten.
`OsmBuildingFootprints` und native Bakes pinnen Koordinaten, Original-ID und Höhenintervalle.
Höfe/Mindesthöhe erreichen Mesh und Terrain-Stempel; Klasse und Dach-Tags fehlen im Meshplan.
Regionale Client-Diagnosen sind sichtbar; die vollständige visuelle Place-Abnahme ist offen.

## Belegte Defizite
Rosenheims Originalweg 619896097 trägt `man_made=chimney`, `building=yes`, `height=80`.
Explizite Sonderbauwerksklasse hat Vorrang; schlanke Form oder Nähe beweisen keine Identität.
Feldkirchs bisherige Kachel liefert height=4/min_height=7. Original-Part 987120770
trägt levels=1/min_level=2 ohne metrische Höhen. Kein blindes Addieren oder Ignorieren.
Wiens Originalwege 241884106/1235545599 haben min_height=5.5 ohne Gesamthöhe;
die reduzierte Kachel lieferte height=min_height=5. `OsmBuildingHeights::Resolve`
erhält Herkunft und gültiges Intervall aus deklarierter Policy. Metrische Widersprüche
bleiben Fehler, Geschosskonflikte markiert. Echte Places müssen die native Ableitung zeigen.

## Besitzer und Datenfluss
- `world/data` liefert gepinnte Originalobjekte; Formattypen enden am Adapter.
  XML/PBF dürfen IDs, Tags, Nodefolgen und Relationsrollen nicht reduzieren.
  Fehlende Quellen explizit melden; kein VersaTiles-Fallback oder dauerhaftes MVT-Overlay.
  MVT-IDs sind keine bewiesenen OSM-IDs; kein Join allein durch räumliche Nähe.
- `world/ground` normalisiert Einheiten und Höhen, besitzt Footprints und Quellbezug.
  `engine/streaming/StructureBuildQueue` publiziert native Inputs atomar (2280/2330).
  `generators/building/StructureBake` und `StructurePlan` konsumieren diese Semantik;
  keine zweite Importqueue oder Place-Sondergeometrie.

## Implementierung und Invarianten
1. building/part, height/min_height, levels/min_level, roof shape/height/levels/
   direction/orientation, Nutzung, Material und Farbe bis zur Konstruktion erhalten.
   Explizite Höhe hat Vorrang; Levels und fehlende Höhen folgen deklarierter Policy.
   Eltern/Parts nicht doppelt extrudieren; Höfe offen halten. Erhöhte Parts stempeln
   den Boden nicht. Vorhandenes Massing erhalten; `RowCut` bleibt auf Terrace beschränkt.
2. Präzise Dachform durchreichen: unbekannter Wert wird weder Flat noch pitched=true.
   `osm-only` nutzt belegte Form/Part-Geometrie; fehlende Form bleibt unbekannt mit
   technischem Abschluss. `plausible` ist der visuelle Standard und ergänzt fehlende
   Formen aus Nutzung, Grundriss, Nachbarbebauung und regionaler OSM-Evidenz.
   Begrenzter Kandidatenraum, Seed aus Original-ID und Welt-Seed; Annahme/Konfidenz
   im ConstructionResult. Tilefolge, Kamera oder Policy-Wechsel ändern belegte Formen nicht.
   Dachfläche, Last, Sonne, Wasser und Wartungszugang begrenzen Konstruktionen;
   explizite Material-/Formangaben haben Vorrang. Keine Place-Presets oder Solarpunk-Umgestaltung.
3. Schornsteine erhalten keine Wohnfassaden, Kirchen keine Bürohausannahme aus Höhe.
   Dachdetails aus belegten Tags/Geometrie oder explizit plausibler Generatorpolitik;
   keine unbegründete Serienausstattung. Nächste Lieferung: diese Klassen und Dachformen
   durch den nativen Runtime-Pfad ins tatsächliche Rosenheim-/Flensburg-Bild bringen.
4. Brücken/Tunnel/Layers und Stützmauern bleiben Konstruktionen mit gemeinsamem
   Höhen-/Kontaktmodell (2121/2175); keine Brücke auf DEM pressen. Straßenfortschritt erhalten.
5. highway/railway, access/oneway, lanes/turn restrictions, gauge/electrified sowie
   tree/species/genus/leaf_type behalten denselben Original-Eingangsvertrag.
   `TransportTopology`/`OsmTransportLoader` liefern bereits native Verkehrsprodukte.
   2280 besitzt Quellenresidenz, 2133 Konnektivität, 2176 spätere Vegetationsarten.
   Fehlende Tags sind unbekannt oder markiert ergänzt; Defaults sind keine Messung.

## Widerlegbare Abnahme
Rosenheim zeigt den belegten Schornstein ohne Wohnfenster; Flensburg plausible Kirchenformen.
Wien/Feldkirch behalten gültige Herkunft/Intervalle, Durchfahrten und unverformten Boden.
Höfe, Parts und explizite Dachformen bleiben bei Tile-/LOD-Wechsel und Reimport stabil.
Unbekannte Dachwerte bleiben unbekannt; Policy-Wechsel betrifft nur unbelegte Formen.
OSM-/DEM-/Seed-identische Läufe sind deterministisch. Tatsächliche Place-PNGs öffnen;
Stadtsilhouette, Schatten, Form und Framekosten entscheiden über plausible Ergänzungen.
Keine Pflicht zur Fotokopie ungetaggter Landmarken. Format, fokussierte Prüfungen,
Places und vollständiger Lint nach AGENTS; Quellen-Diagnosen ersetzen kein Place-Gate.

[OSM Simple 3D Buildings](https://wiki.openstreetmap.org/wiki/Simple_3D_Buildings).
