Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends: 2280
Area: generators, world, engine
Tags: roads, bridges, tunnels, osm

# Native transport networks form coherent roads and structures

## Ergebnis und Ist
Straßen, Gehwege, Bahn/Tram und Wege mit sauberen Kreuzungen, Brücken/Tunneln und Ebenen.
Straßenprofile, Alignment, Corridors, RoadMesher/RoadSurfaceBuilder und Terrain-Deformation
bestehen und bleiben erhalten. Original-Netze existieren; neue Provider-Mittelachsen/
Ebenen sind noch nicht normalisiert, komplexe Anschlüsse und Weltintegration offen.

## Besitzer und fehlender Vertrag
2280 liefert normalisierte Transportlinien/Klassen/Ebenen samt Tile-Randidentität;
MVT ist kein vollständiger Routinggraph. OSM-Adapter besitzt Quellsemantik, `world/navigation`
das native logische Netz. RoadAlignmentBuildQueue/`generators/road` erzeugen Profile;
Laying koordiniert Kontakt/Publikation. Renderer konsumiert native Geometrie, keine Tags.
Zuerst einen durchgehenden Stadt-/Hafenanschluss samt Brücke bis zum Bild liefern.

## Verfahren und Invarianten
- Gelieferte Straßen-/Bahnklasse, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene erhalten.
  Überlappungen/Randsegmente eindeutig besitzen. Keine Kreuzung allein aus Nähe; fehlende
  Topologie/Ebenen durch deklarierte Normalisierung behandeln und Unsicherheit erhalten.
- Zusammenhängende Ketten besitzen ein Alignment; Übergänge/Schultern/Gehwege/Knoten
  daraus ableiten. Keine Segmentwellen, Nahtlücken oder zweite Straßenpipeline.
- Brücken mit getrenntem Überbau/Pfeilern/Widerlagern und Höhenanschlüssen; Tunnel mit
  Portal/Freiraum. Keine Bodenstempel für schwebende Fahrbahnen oder eingeebneten Ebenen.
- Terrain-, Gebäude- und Wasserbezug teilen Datum/Kontakte. Nur reale Kontakte verformen
  Boden; Netze/Funktion bleiben trotz Fern-LOD erhalten. Nicht volle Quellarchive pinnen.
- Markierungen, Bord/Kai, Geländer, Signale und Beleuchtung nutzen Klassenparameter;
  Wiederholungen instanzieren, Nahdetails nach 2336 begrenzen. Materialantwort besitzt 2171.

## Abnahme
Realer Place zeigt durchgehende Straße und korrekt angeschlossene Brücke ohne Wasser-/
Geländewände; vorhandene Straßenbilder/Funktion erhalten oder verbessern. Fehlende
Semantik bleibt explizit. Deklarierte Hockenheim-Routen sind kein Ersatz für Stadtnetze.
