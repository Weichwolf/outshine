Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, render, world
Tags: terrain, relief, rock, ground-detail

# Source-shaped terrain gains plausible relief and surface detail

## Ergebnis und Ist
Lesbare Grate/Rinnen/Fels-/Schutt-/Wiesenflächen und Nahboden statt gleichförmigem Noise.
TerrainRefinement/Press/Mesh, GroundLattice/GroundMaterials und groundRock.glsl bestehen;
Fels nutzt bisher isotropes Value-Noise, gerichtete Schichtung/Bruchlinien fehlen.

## Besitzer und nächste Lieferung
Höhenprovider besitzt Samples/Datum/NoData; TerrainPress/Refinement finales Kontaktrelief
und Fehler. Renderer besitzt Material/Filter, world native Produkte. Vorhandene Felder
reichen für Verbesserung ohne neuen Quellenvertrag. Zuerst Koerbersee mit erhaltener
Silhouette/See und lesbaren Hängen liefern; 2145 besitzt Kontakt-/Pegelkorrektur.

## Verfahren und Invarianten
1. Neigung/Exposition/Talrichtung aus endgültigem Höhenfeld; stabile Raumframes mit fester
   Ersatzachse bei flachem Boden. Welt-Seeds/Koordinaten über Tile-/LOD-Grenzen erhalten.
2. Gerichtete Schichtfelder mit begrenztem Domain-Warp; Ridge-Noise für Rinnen, sparsame
   Voronoi-Grenzen für Brüche. Plausible Geologie, keine aus DEM erkannte Gesteinsart.
3. Gemeinsamer metrischer Entwurf: sichtbares mittleres Relief im Compute als Geometrie,
   Feinrisse als Normalen/Roughness. Parameter über GroundMaterials/render-eigenen
   GroundClassBuffer/GroundStorage; groundLit nutzt 2171/2155s gemeinsame BRDF/Licht.
4. Pixel-Footprint begrenzt Frequenzen; subpixeliges Detail zu Farbe/Normalvarianz integrieren,
   fwidth/SDF-Antialiasing für Linien. Fernrelief erzeugt keine unnötige Pixel-/Mesharbeit.
5. Gelände/gelieferte Landcover-Klasse steuern Fels/Schutt/Boden. DSM-Bewuchs bleibt vom
   Boden unterscheidbar; Ergänzung verändert keine belegten Gipfel/Küsten oder Kontakte.
   Gröbere Samples benötigen Höhenfehlernachweis; Normaldetail beweist keine Formschranke.
6. Nachbarn teilen Samples/Raumbezug; Bedarf vor Erzeugung (2336). Nässe/Schnee/Schmelze
   aus 2172 folgen Exposition/Relief, Vegetationsstandorte 2111. Keine Fotoformen/Diskbakes.

## Abnahme
Koerbersee/Malcesine gewinnen lesbare Hänge/Nahflächen bei erhaltener Silhouette und
Kontakten. Bewegung ohne Nähte/Flimmern; Winter ändert Zustand statt Quellenform.
Bildgewinn und getrennte CPU/GPU/Bytekosten am echten Place belegen.
