Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, render, world
Tags: terrain, rock, ground-detail

# Source-shaped terrain gains directional rock and ground detail

## Ergebnis und Ist
Lesbare Grate/Rinnen, Fels/Schutt/Wiesen und Nahboden bei erhaltener Quellenform.
TerrainRefinement/Press/Mesh, GroundLattice/GroundMaterials und groundRock.glsl bestehen;
isotropes Value-Noise liefert noch keine glaubwürdige gerichtete Felsstruktur.

## Besitzer und nächste Lieferung
Höhenprovider liefert Raster/Datum/NoData; Terrain-Generator finales Kontaktrelief,
Renderer Material/Filter. Bestehende Höhenfelder genügen. Zuerst Koerbersee mit erhaltener
Silhouette und See verbessern; 2145 besitzt Pegel/Kontakte, 2171 die gemeinsame Materialantwort.

## Verfahren
- Neigung/Exposition/Talrichtung aus endgültigem Höhenfeld; stabile Raumframes auch auf
  flachem Boden. Weltkoordinaten/Seeds über Tile-/LOD-Grenzen erhalten.
- Gerichtete Schichten mit begrenztem Domain-Warp, Ridge-Noise für Rinnen, sparsame
  Voronoi-Brüche. Plausible Geologie ergänzen, keine aus DEM belegte Gesteinsart behaupten.
- Sichtbares mittleres Relief als budgetierte Geometrie, Feinrisse als Normalen/Roughness.
  Gemeinsame metrische Parameter über GroundMaterials/GroundClassBuffer/GroundStorage.
- Pixel-Footprint begrenzt Frequenzen; subpixeliges Detail in Farbe/Normalvarianz überführen,
  Linien gefiltert auswerten. Fernrelief benötigt keine Nahmesh-/Pixelarbeit.
- Gelände/Landcover steuert Fels/Schutt/Boden. DSM-Bewuchs vom Boden unterscheiden; Gipfel,
  Küsten und Kontakte erhalten. Nachbarn teilen Samples, gröbere Raster brauchen Höhenfehler.
  Auswahl aus 2336, Nässe/Schnee aus 2172, Standorte aus 2111 anschließen; keine Diskbakes.

## Forschungsgrundlage
[Gabor-Noise, SIGGRAPH 2009](../doc/references/materials/siggraph/2009-sparse-gabor-noise.pdf)
([Primärquelle/Einordnung](../doc/references/README.md)) liefert gerichtete Spektren und
anisotrope Filter als Grundlage für Felsrisse. Zuerst gefilterte Richtungsstruktur im
vorhandenen groundRock integrieren; keine neue Noise-Pipeline oder unbelegte Geologie.
Clipmap-Prinzipien aus 2336 begrenzen Aktualisierung, nicht die Quellenform.

## Abnahme
Koerbersee/Malcesine zeigen gerichtete lesbare Hänge ohne verlorene Silhouette/Kontakte.
Keine Nähte oder Flimmern bei Bewegung; CPU/GPU/Bytes und Bildgewinn separat belegen.
