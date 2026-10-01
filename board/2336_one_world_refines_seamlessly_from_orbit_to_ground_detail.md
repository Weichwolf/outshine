Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2188
Area: world, engine, generators, render
Tags: planetary, lod, coverage, budgets

# One world refines seamlessly from orbit to ground detail

## Ergebnis und vorhandene Fähigkeit
Die vollständige Erde, Flug in mehreren Kilometern Höhe, Städte und Nahdetails verwenden
dieselbe Welt. Am Boden ist der volle 240-km-Umkreis um die Position verfügbar; Höhe
über Gelände erweitert die Abdeckung bis zum konservativen Horizont/Erdansicht.
Blickrichtung begrenzt Zeichenarbeit, nicht Inhalte. Geodätische Zellen, Double-Welt,
kamera-relative Daten und native Gebäude-LOD-Fundamente existieren; globale Grobstufen,
adaptive zertifizierte Varianten und ihre Runtime-Integration fehlen.

## Architekturentscheidungen und nächste Lieferung
`engine/streaming` besitzt positions-/höhenbezogenen Bedarf, Residency und Publikation.
GeoCellId beschreibt geodätische Abdeckung; GroundLattice, StructureCellPlanner und
bestehende Produktselektoren konsumieren dieselbe Eltern-/Kindhierarchie.
2188 liefert den noch fehlenden öffentlichen Abstands-/Fehlervertrag; dessen übrige
Erweiterungen sind kein zusätzlicher Blocker. Zuerst native Gebäude vor Geometrieerzeugung nach Entfernung/Fehler bündeln und die
volle Rundumsicht eines dichten Places halten; globale Grobquellen anschließend belegen.
Die weltweite Grobquelle ist noch kein fertiger Vertrag: ein kleinzelliger OSM-API-
Katalog allein liefert keine schnelle Erdansicht. Erlaubte Originalübersichten,
Quellenumfang und Lade-/Bytekosten nachweisen; keine heimliche vierte Quelle oder
persistente Generatorablage. Deshalb bleibt die globale Architektur `planned`.

## Umsetzung und Invarianten
- Generatoranforderungen enthalten räumliche Abdeckung, Entfernung/Projektion und
  erlaubten Bildschirmfehler. Produkte liefern native Parameter/Geometrie und konservative
  Grenzen; eingebaute/externe Generatoren teilen den Vertrag aus 2188.
- Hierarchie zuerst vollständig grob darstellen, danach auflösbare Details nachfordern.
  Subpixel-Relief darf zum Ellipsoid zurückkehren; sichtbare Küsten/Silhouetten erhalten.
  Eltern bleiben bis zur geschlossenen Kind-Publikation verfügbar; Übergänge ohne Löcher/Poppen.
- Gebäudestufen: Fernverband, Massing, Hülle, Nahdetails. Fernobjekte gemeinsam darstellen;
  Fassaden-/Dachdetails nur nah erzeugen. Vegetation übernimmt dieselbe Auswahl/Residency.
- Source-/Zell-/LOD-Produkte bleiben von Kamerahistorie unabhängig. Begrenzte Vorbereitung,
  Instancing, kompakte Parameter, Hysterese und stabile Auswahl statt Neuaufbau je Frame.
- Strukturvarianten verwenden echte Oberflächenfehler einschließlich Öffnungen und beider
  Distanzrichtungen. TriangleDistance/TriangleRegion/StructureSurfaceError sind CPU-Fundamente;
  adaptive vollständige Verfeinerung und Runtime-Zertifikate noch anschließen. Bis dahin
  konservative Zellhülle erhalten, keine kleinere Schranke allein aus CPU-Beweisen.
- Terrain-Scope und Quellrevisionen überleben Raster-Eviction; alte Produkte verlieren bei
  Quellen-/Formwechsel ihre Gültigkeit. Terrainbedarf folgt konsumierter Geometrie/Detailstufe.

## Abnahme
Boden-Place, hohe Flugkamera und Orbit separat auf Vollständigkeit, Warm-/Kaltstart,
Speicher und Framekosten prüfen. Nahtloser Zoom darf keine Löcher, stale Produkte oder
sichtbar unzulässigen Oberflächenfehler erzeugen. Der einzelne Grashalm ist Nahdetail
von 2111; kein vorausgeladenes weltweites Feinmodell und kein verkürzter Radius.
