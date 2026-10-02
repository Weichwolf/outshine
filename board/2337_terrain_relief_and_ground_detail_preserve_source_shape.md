Type: feature
State: open
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: generators, world, render, engine
Tags: terrain, relief, rock, ground-detail

# Source-shaped terrain gains plausible relief and surface detail

## Ergebnis und vorhandene Fähigkeit
Körbersee/Malcesine zeigen lesbare Grate, Rinnen, Fels-/Schutt-/Wiesenwechsel und
Nahboden ohne gleichförmiges Rauschen. TerrainRefinement, TerrainPress, TerrainMesh,
GroundLattice und groundRock.glsl bestehen. Die aktuelle groundRock.glsl nutzt isotropes Value-Noise für Farbe/Bump; gerichtete
Schichtung und Bruchlinien fehlen. Die durchgehende Bildwirkung aus finalem
Relief, Oberflächenmaßstab und gefilterter Ergänzung ist nicht belegt.

## Besitzer und nächste Lieferung
Import besitzt GLO-30-Samples/Datum/NoData. TerrainPress/Refinement besitzen endgültige
Geometrie und Fehler; world hält native Produkte. GroundMaterials und Renderer besitzen
Baustoffparameter/Filter (2171). Engine koordiniert Bedarf und geschlossene Publikation.
Zuerst ein vollständiges Körbersee-Bild mit quellengetreuer Silhouette, korrektem See
und lesbaren Hangflächen liefern; vorhandene Felder/Pässe ausbauen, keinen zweiten Terrainpfad.

## Gewählter prozeduraler Aufbau
1. Aus finalem Höhenfeld Neigung, Exposition und Tal-/Hangrichtung ableiten; robuste
   Frames haben eine feste Ersatzachse auf flachen Flächen. Raumbezug/Seeds bleiben
   über Tile-/LOD-Grenzen stabil, ohne kamera-relative Noise-Koordinaten.
2. Richtungsabhängige Schichtfelder mit begrenztem Domain-Warp kombinieren. Ridge-Noise
   liefert plausible Rinnen; Voronoi-Zellgrenzen liefern sparsame Bruchlinien. Hang-/
   Expositionsparameter steuern Verteilung, nicht neue Fullscreen-Felder pro Effekt.
   Das ist eine plausible Geologiegrammatik, keine aus GLO-30 erkannte Gesteinsart.
3. Derselbe metrische Höhen-/Materialentwurf speist Geometrie und Shader. Rinnen mit
   sichtbarer Form erzeugt TerrainRefinement/Press im Compute-Worker; Feinrisse liefern
   Normalen/Roughness in groundRock. Silhouettenfehler begrenzen geometrische Ergänzung.
4. Pixel-Footprint wählt eine begrenzte Frequenzmenge; unterschwellige Risse integrieren
   zu mittlerer Farbe und Normalvarianz/Roughness. fwidth/Signed-Distance-Antialiasing
   glättet Bruchlinien. Fernrelief bekommt keine hochfrequente Pixelarbeit.
5. GroundMaterials hält metrische Parameter; GroundClassBuffer/GroundStorage transportieren
   render-eigene Daten. groundLit komponiert eine BRDF mit 2155s Licht und 2172s Zustand.
   Parameter/Version und native Bounds gehen durch 2188s öffentliche Produktgrenze.

## Umsetzung und Invarianten
- Grobrelief, Hangneigung und Exposition bestimmen plausible Fels/Schutt/Boden-Verteilung;
  OSM-Landcover und belegte Flächen gehen vor. Rinnen/Schichtung folgen Gelände statt
  isotropem Farbnoise. Original-GLO-30 ist DSM, kein garantierter nackter Erdboden.
- Quellenrelief, Kontakt-Deformation und unbelegte prozedurale Ergänzung unterscheiden.
  Ergänzungen verändern keine belegten Gipfel/Küsten oder Straßen-/Gebäudeanschlüsse.
  Offene Höhendatums-/NoData-Probleme gehören in 2145/2280, kein Shaderkaschieren.
- Metrisches Detail: sichtbares mittleres Relief als begrenzte Geometrie/Displacement,
  subpixeliges Detail als gefilterte Normal-/Roughness-Antwort. Nur tatsächlich
  konservative Oberflächenfehler als Zertifikat melden; kein Normaldetail als Formbeweis.
- Zusammengehörige Nachbarflächen teilen Samples/Raumbezug und sichere Übergänge.
  Detail vor Erzeugung nach Projektion/Fehler auswählen (2336); vorhandene Verfeinerung
  kann unabhängig von noch fehlenden globalen Grobstufen verbessert werden.
- Schnee/Schmelze aus 2172 folgt Exposition und gespeichertem Wetterzustand, respektiert
  Relief und Material; keine alleinige Höhenfarbgrenze. Vegetationsstandorte folgen 2111.
- Keine Place-Texturen, Fotoformen oder persistenten Generatorprodukte. Kompakte Parameter
  und stabile Welt-Seeds halten Variation über Detailwechsel und Kameradrehung kohärent.

## Abnahme
Bergsilhouette, Nahhang und Ufer erhalten Quellenformen/Kontakte und gewinnen lesbare
Flächenstruktur. Winter und Schmelze verändern Zustand statt Quelle. Bewegung zeigt
keine Nähte oder Flimmern; reale Place-Bilder und getrennte Kosten belegen den Gewinn.
