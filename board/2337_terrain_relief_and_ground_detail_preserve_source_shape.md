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
GroundLattice und groundRock.glsl bestehen. Die durchgehende Bildwirkung aus finalem
Relief, Oberflächenmaßstab und gefilterter Ergänzung ist nicht belegt.

## Besitzer und nächste Lieferung
Import besitzt GLO-30-Samples/Datum/NoData. TerrainPress/Refinement besitzen endgültige
Geometrie und Fehler; world hält native Produkte. GroundMaterials und Renderer besitzen
Baustoffparameter/Filter (2171). Engine koordiniert Bedarf und geschlossene Publikation.
Zuerst ein vollständiges Körbersee-Bild mit quellengetreuer Silhouette, korrektem See
und lesbaren Hangflächen liefern; vorhandene Felder/Pässe ausbauen, keinen zweiten Terrainpfad.

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
