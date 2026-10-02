Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: world, generators, engine
Tags: roads, bridges, tunnels, osm

# Native transport networks form coherent roads and structures

## Ergebnis und vorhandene Fähigkeit
Durchgehende Straßen, Gehwege, Bahn-/Tramtrassen und Wege folgen Original-OSM und
verbinden korrekte Kreuzungen, Brücken, Tunnel und Ebenen. Bestehende Straßenprofile,
Alignment, Corridors, RoadMesher, RoadSurfaceBuilder und Terrain-Deformation erhalten.
Original-Transporttopologie und native Routen existieren; allgemeine Weltprodukte,
komplexe Anschlüsse und Ebenen sind noch nicht vollständig angeschlossen.

## Nächste Lieferung und Besitzer
`world/navigation` hält das logische Netz unabhängig von Rendergeometrie. Original-
Zell-Snapshots liefern IDs/Tags und vollständige konsumierte Referenzen. Native
Straßen-/Kontakte konsumieren anschließend semantische Produkte und Quellbelege;
vollständige Roh-Snapshots nicht allein wegen einer Renderinstanz dauerhaft pinnen. Bestehende
RoadAlignmentBuildQueue und `generators/road` erzeugen Profile und native Produkte;
`Laying` koordiniert Terrainkontakt und atomare Publikation. Den vorhandenen regionalen
Anschluss auf allgemeine Originalstraßen erweitern, ohne ausschließlich deklarierte Routen.

## Umsetzung und Invarianten
- highway, railway, waterway, access, lanes, width, surface, bridge, tunnel und layer
  erhalten. Kreuzungen nach Topologie/Ebene verbinden; räumliche Nähe ist keine Verbindung.
- Profile entlang zusammenhängender Ketten lösen; Übergänge, Schultern, Gehwege und
  Knoten aus demselben Alignment ableiten. Keine Segmentwellen oder offenen Kreuzungen.
- Brücken tragen getrennten Überbau, Pfeiler/Widerlager und Höhenanschlüsse. Tunnel
  schneiden Portale/Freiraum; Wasserläufe bleiben darunter offen. Keine Bodenstempel
  für schwebende Fahrbahnen und keine eingeebneten getrennten Ebenen.
- GLO-30-Kontakt, Bauwerke und Wasserpegel teilen Höhenbezug und Quellenrevision.
  Nur tatsächliche Kontakte verformen Terrain. Semantik/Funktion bleiben trotz Fern-LOD erhalten.
- Bestehende Renderer-/Mesherpfade nutzen; begrenzte Jobs, konsistente Kontaktprodukte
  und Eigentümer statt zweitem Straßenimport oder graphischem Netz aus Dreiecken.

- Straßenraum trägt vorhandene OSM-Klasse und Tags für Markierungen, Bord/Kai,
  Geländer, Signale, Beleuchtung und technische Kleinbauten bis zu nativen Produkten.
  Wiederholbare Elemente instanzieren; Nahgeometrie nicht für die Fernstadt erzeugen.

## Abnahme
Ein echter Stadt-/Hafen-Place zeigt durchgehende Straßen und eine korrekt angeschlossene
Brücke ohne Wasser-/Geländewände. Fehlende Klassen und konsumierte Referenzen explizit
melden. Bestehende Straßenbilder/Funktion erhalten oder verbessern. Hockenheim-Routen
bleiben Diagnosen; eine Runde ersetzt keine vollständige native Infrastruktur.
