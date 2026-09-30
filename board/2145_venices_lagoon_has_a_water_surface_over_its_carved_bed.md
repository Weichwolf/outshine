Type: bug
State: active
Architecture: planned
Priority: P0
Parent: 2169
Area: world, render
Tags: webcam, water, coastline, osm
Depends:

# Water bodies have coherent levels, valid surfaces and constructed banks

## Ergebnis und vorhandene Fähigkeit
Wasser folgt Original-OSM-Körpern, hat zusammenhängende Pegel und plausible Anschlüsse.
Flensburgs Hafen/Förde, Husums Kai und Malcesines Ufer enthalten keine künstlichen
Wasserfälle, erhöhten Meeresflächen oder überfluteten Gebäude.
`WaterField` hält bereits Außen-/Innenringe und einen gemeinsamen Pegel pro Fläche.
`WaterSurfaceBuilder` trianguliert konkave Flächen und Inseln; Basin-Stamps erhalten
Innenringe. Earcut f25bc76/ISC ist gepinnt. Straßenqualität bleibt erhalten.

## Aktueller Blocker
Flensburg zeigt weiterhin künstliche Höhenkanten und falsche Wasserwirkung.
Die bestehende Kachelquelle enthält Hafen/Förde in einer separaten `ocean`-Ebene;
`GroundStack::CreateVectorField` lädt sie nicht, und die Klassifikation kennt nur
`water_polygons/ocean`. Die Runtime veröffentlicht Binnenflächen, keine vollständige
Meeresfläche. Das erklärt fehlende Abdeckung; die genaue Herkunft jeder Höhenkante
ist zusätzlich gegen ursprüngliches DEM und Terrain-Stamps zu prüfen.
Einzelne gekachelte Binnenflächen erhalten unabhängig aus DEM-Randproben geschätzte
Pegel. Diese Heuristik beweist weder gemeinsame Pegel noch einen Meereswasserstand.
2326 korrigiert abweichend geglättete Materialgrenzen; das repariert keine fehlende Küste.
`WaterField` lehnt Ringe über 512 Punkten ab; `BuildWaterSurfaces` zählt verweigerte
Topologie nur als Metrik und meldet trotzdem Erfolg. Eine vollständige Welt ist damit
nicht bewiesen. Diese Grenze darf beim Original-OSM-Anschluss keine Gewässer löschen.

## Implementierung und Besitz
- `world/ground` leitet native WaterBody-Produkte aus dem gemeinsamen Original-OSM-
  Snapshot ab: typed Node/Way/Relation-ID, Tags, Außen-/Innenringe, Klasse und Pegelherkunft.
  Geschlossene Wasserflächen und Multipolygone erhalten getrennte Außenkomponenten
  mit eindeutig zugeordneten Inseln. Fehlende oder widersprüchliche Daten bleiben benannt.
- Gerichtete `natural=coastline`-Ketten erzeugen mit deklarierter Quellenabdeckung
  Marineflächen. Anschlüsse an Nachbarregionen sind Quellenabhängigkeiten; unvollständige
  Ketten erzeugen keinen geratenen Wasser-/Landabschluss. Den regionalen Abschluss-
  und Fehlervertrag vor Runtime-Anschluss festlegen. Keine neue Abhängigkeit von MVT.
- Binnenpegel gelten für den ursprünglichen zusammenhängenden Körper, nicht für dessen
  Renderstücke. Meer nutzt ein ausdrücklich deklariertes Referenzniveau im DEM-Datum.
  Ein exakter historischer Tidenstand ist mit den erlaubten Eingaben nicht gegeben.
  Flüsse behalten ein stetiges Profil entlang ihrer gerichteten Verbindung.
- Kein Grundwasser-Mesh pro Terrain-Kachel und keine Begrenzung aller Gewässer auf NN.
  OSM bestimmt Gewässerexistenz; Körperpegel und DEM nutzen denselben Höhenbezug.
  Seen unter Meereshöhe bleiben zulässig. Regionen teilen nur Darstellung und LOD,
  nicht den Pegel unabhängiger Gewässer. Das Meer bleibt erdkrümmungsgerecht.
- Wasser darf im begrenzten Uferband unter Gelände reichen; Tiefentest verdeckt Land.
  Quellenabdeckung und Inseln begrenzen die Fläche weiterhin. Ein globales Niveau
  würde trockene Senken fluten. Tiefendifferenz steuert Uferübergang und Absorption;
  kein Anheben der gesamten Wasserfläche zur Reparatur eines fehlerhaften Ufers.
- `Generators::WaterSurfaceBuilder` erhält gepinnte Ringe/Pegel und TangentFrame,
  liefert native Geometry und lokale Fehler. Materialgrenzen, logische Fläche,
  Wasseroberfläche und Basin-Stamps verwenden denselben Körper und dieselbe Revision.
  Wasser ist eine eigene Oberfläche; ein blaues Terrain-Material ersetzt sie nicht.
- 2327 trennt vorhandene Wasserdreiecke vom Terrain-Material. Danach erhält derselbe
  Renderpfad zeit-/windabhängige Normalen und begrenzte geometrische Wellen; feine
  Wellen laufen im Shader, nahe Silhouetten brauchen ausreichend tessellierte LODs.
  Wetteränderung regeneriert weder Gewässergrenzen noch Terrain. Transmission nutzt
  Fresnel und Wassertiefe; bekannte fehlende Reflexionen bleiben Aufgabe von 2129.
- Engine koordiniert begrenzte IO-/Compute-Arbeit und atomare Veröffentlichung.
  Quellenfehler erhalten den Altstand und einen roten Befund. Keine generierten
  Runtime-Disk-Caches, Kamera-Sonderfälle oder Foto-basierte Geometriekorrekturen.
  Fehlende nötige Gewässer/ungültige Ringe verhindern vollständige Publikation.
  Große Ringe räumlich aufteilen oder unter bewiesenem Projektionsfehler vereinfachen;
  Quellringe, Inseln und Körper-ID erhalten. Budgets verzögern Arbeit, entfernen sie nicht.
- OSM-Quai/Stützmauer erhält Wand, Oberkante und Material; natürliche Böschung bleibt
  separat. Bed/Bank-Deformation respektiert gültiges Bergrelief und Gebäudefreiraum.
  Brücken stehen über erhaltenem Wasser; Straßen werden nicht auf dessen Pegel gedrückt.
  Geschützte Gerinne aus 2257 gelten auch für Liniengewässer. Reflexion folgt in 2129.

## Abnahme
- Konkaver See mit Insel, Fluss über Regionsgrenze und Hafen mit Brücke bleiben
  vollständig, ohne Landüberdeckung, Pegelsprung oder künstliche Höhenkante.
- Originalquelle, Körper-ID, Pegelherkunft und Terrain-Querschnitt erklären jeden
  Anschluss. Ungültige Topologie erzeugt einen lokalen Fehler und keine Ersatzfläche.
- Flensburg/Husum/Malcesine/Koerbersee verbessern Wasser-/Landkontakt sichtbar im
  Place-Budget. Erhaltene Venice-Diagnose bleibt offen, bis sie tatsächlich korrekt ist.
