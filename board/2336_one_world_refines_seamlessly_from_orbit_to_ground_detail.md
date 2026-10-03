Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2188
Area: engine, generators, world, render
Tags: planetary, lod, coverage, budgets

# One world refines seamlessly from orbit to ground detail

## Ergebnis und Ist
240-km-Rundumwelt am Boden, erweiterter Horizont bei Höhe, schnelle Erdansicht im Orbit;
nahtloser Zoom bis ins Nahdetail. Blickrichtung begrenzt Zeichenarbeit, nicht Abdeckung.
Geodätische Zellen, Double-Welt, kamera-relative Daten und Gebäude-LOD-Fundamente bestehen;
globale Grobprodukte, adaptive zertifizierte Varianten und Runtime-Auswahl fehlen.

## Besitzer und fehlender Vertrag
2188 liefert den öffentlichen Abstands-/Produktschrankenvertrag, nicht die gesamte Sandbox.
Engine besitzt Weltbedarf/Residency/Publikation; Generatoren planen Quellen und Produkte,
Renderer Sichtbarkeit. GeoCellId, GroundLattice, StructureCellPlanner und Selektoren
in denselben Abdeckungsplan integrieren. Konkrete OSM-Pipelines bleiben generators/osm.
2280 liefert hierarchische Vektor-/Höhenquellen; grobe MVT-Kacheln enthalten nicht automatisch
alle Gebäude. Fehlende Ferninhalte dürfen nicht als LOD verschwinden. Globale Grobprodukte,
Quellenumfang und konservative Abdeckung erst belegen; deshalb `planned`.

## Verfahren und nächste Lieferung
1. Entfernung/Projektion/erlaubten Fehler vor Gebäudemesh und Terrainbedarf auswerten.
   Zellaufträge pinnen nur benötigte Raster; Parent-Belege bleiben getrennt erhalten.
   Parent-Identität und aktuelles vollständiges Zertifikat sichern die Ausgangsquelle;
   der Teilauftrag besitzt seinen eigenen Rasterdigest und nachgewiesene Abdeckung.
   Vor Publikation Parent und konsumierte Teilquellen erneut prüfen; keinen Teildigest
   mit dem gesamten Parent-Digest vergleichen oder LOD-Fehler dadurch kleiner deklarieren.
   Eine große Kachel darf einen kleinen Auftrag nicht an der Vorbereitungsgrenze sperren.
   Batches nach tatsächlichem Bedarf teilen, bestehende Grenzen nicht erhöhen.
   Native Fernverbände bündeln, vorhandene Rundumsicht halten; danach Flug-/Orbitbedarf.
2. Gebäude: Fernverband → Massing → Hülle → Nahdetails. Vegetation teilt Auswahl/Residency
   mit Fernwald → Kronen/Bäume → Äste/Blätter → Nahboden. Seeds/Form bleiben stabil.
3. Grobe Eltern vollständig bereitstellen, nur auflösbare Kinder nachfordern; Eltern bis
   atomarer Kind-Publikation halten. Hysterese/Übergänge verhindern Löcher und Poppen.
4. Productkosten/Bildfläche/Fehler gemeinsam für alle Klassen bewerten; Instancing und
   kompakte Parameter vor Detailarbeit. Kein voller Feinradius oder Neuaufbau je Frame.
5. Terrainbedarf folgt konsumierter Geometrie. Gröberes Höhensampling braucht eigene
   Höhenfehlerschranke; Gebäude-LOD legitimiert keinen beliebigen heightZoom.

## Invarianten und Abnahme
SurfaceError umfasst tatsächliche Öffnungen und beide Distanzrichtungen. TriangleDistance,
TriangleRegion und StructureSurfaceError sind CPU-Fundamente; adaptive Verfeinerung und
Runtime-Zertifikate fehlen. Bis dahin konservative Zellhülle, keine kleinere LOD-Schranke.
Quellen-/Raster-Eviction verliert keine Produktprovenienz; Änderungen invalidieren abhängige
Stände. Subpixelrelief darf zum Ellipsoid übergehen, sichtbare Küsten/Silhouetten bleiben.
Bodenstadt, Flug und Orbit separat auf volle Abdeckung, Warm-/Kaltstart, Zeit und Speicher
prüfen; Drehung lädt vorhandene Produkte nicht neu. Kein verkürzter Radius, globales
Feinmodell oder persistenter Generatorcache. Grashalm-Form besitzt 2111.
