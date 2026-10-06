Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: engine, generators, world, render
Tags: lod, coverage, planetary, budgets

# Required detail is selected before source and geometry work

## Ergebnis und Ist
Rundum verfügbare Welt bis 240 km am Boden, später höhenabhängiger Horizont und Orbit → Nahdetail.
Der Geometriebedarf nimmt mit Entfernung ab: Häuser → Blockverbände → kompakte Skyline-Flächen.
Mehr Quellobjekte innerhalb gleicher Fern-Coverage erzeugen keine proportional größere Geometrie.
GeoCellId, GroundLattice, Gebäudepläne und vier Klassen bestehen. Der Stadtpfad verwendet nur
Fine/Shell/Massed; Skyline fehlt. WholeTile-/Cell-Auswahl, doppelte Vorbereitung und konservative
Zellvolumen erzwingen unnötige Einzelhüllen. Native Cluster sind überwiegend flache Blattgruppen.
Tokyo erzeugt weiterhin 29,65 Mio. Dreiecke; GPU-Streamkapazität ca. 3,88 GB. Eine Auswahlkorrektur
allein hat diese Menge und das Bild nicht verändert. Die Fernrepräsentation wird neu integriert,
statt die vorhandene Erzeugungskette weiter umzuordnen. Speicher- und Frame-Gates bleiben rot.

## Aktuelle Lieferung: Fernstadt vor weiteren Nahdetails
| Klasse | Produkt | Bedarf vor Erzeugung |
|---|---|---|
| Fine | Räumliche Nahfassaden/Dächer | Sichtbare Form, Interaktion, aktuelle Schatten |
| Shell | Grundrisswände/Dach ohne sekundäre Geometrie | Projizierter Formfehler rechtfertigt Hülle |
| Massed | Gemeinsam erfasste Blockverbände | Silhouette/Coverage bleiben, Einzelhäuser entfallen |
| Skyline | Wenige tiefenhaltige Impostor-/Silhouettenflächen | Fernbild und Parallaxe statt Einzelvolumen |

Zuerst Massed/Skyline im dichten Stadtpfad, dann verbleibende Nahkosten. Gebäudepläne behalten
Grundrisse/Höfe/Parts, Höhen, Materialangaben und Terrainkontakte. Straßenqualität und
Terrain-Deformation bleiben erhalten. Kontakt/Kollision hängen nicht von der Bildrepräsentation ab.
Keine vollständige Fine-Stadt als notwendiger erster Schritt einer Fernlieferung.
Ferne Details dürfen aus belegter Dichte/Nutzung/Relief statistisch angenähert werden; Silhouette,
Coverage, Farbe und Lichtwirkung entscheiden. Herkunft bleibt explizit. Näherkommen ersetzt die
Schätzung durch feinere quellengestützte Produkte mit stabilem Übergang.

## Experiment entscheidet, native Integration liefert
[Python-Experiment](../test/experiments/building_lod.py) liest echte gecachte MVTs und Szenariokamera.
Tokyo-Teilmenge, 640×360: 588.862 Pläne, 867.152 zugelassene Hüllendreiecke, 1.164 sichtbare Gebäude.
[Direkte Planaggregation](../test/experiments/building_massing.py) erzeugt 10.077 Dreiecke (~86× weniger),
verändert aber Lücken/Silhouette; kein abgenommener Ersatz. Ein Tiefenfeld ist bei gleichem Auge exakt;
zwei Punktlayer verlieren bei 4 m Translation 4.031 belegte Pixel. Resampling/Disocclusion bleiben offen.
Quellobjekte, Geometrie, Bildfehler, Bytes und Vorbereitung vergleichen; Stillstand, Drehung,
Translation und Lichtwechsel getrennt prüfen. Flaches Terrain/Dachmodell und Python-Zeiten
belegen weder native Framekosten noch Rundumabdeckung. Echte Höhen/Dächer und alle Places integrieren.
Ausgaben: `build/experiments/`; keine Generatorprodukte im persistenten Quellcache.

## Neue Erzeugungskette und Zuständigkeit
1. Generatoren halten kompakte Fachpläne vor jedem Vertex-/Indexaufbau. OSM: gepackte Ringe,
   Höhen/Kontakte, Herkunft/Erscheinung; Terrain und Vegetation behalten passende Fachformate.
   Gemeinsame Auswahl nutzt Bounds, Eltern/Kinder, Qualitätsgültigkeit und Kosten ohne Quelltypen.
   Gepackte Bereichsarrays/Morton-Ordnung prüfen; GeoCellId/Zellteilung weiterverwenden.
   Quadtree mit Höhen-Bounds versus BVH prüfen; kein verbindlicher OSM-only-Index oder zweiter Weltbaum.
2. Kameraort/Höhe, Projektionsmaßstab und Qualitätsauftrag wählen räumliche Produkte.
   Rundumbedarf ist unabhängig von Blickrichtung; Drehung wählt nur bereits verfügbare Flächen.
   Größere Entfernung erlaubt größere Weltfehler bei kontrolliertem Pixelfehler.
   Bei Bewegung nur betroffene Knoten/Detailgrenzen neu bewerten; unveränderte Produkte behalten.
   Eltern auswählen ohne Kindpläne zu triangulieren; Verfeinerung erzeugt nur benötigte Kinder.
3. Fernprodukte direkt aus Plänen erfassen; hierarchische Ray-Bündel von nah nach fern
   gegen gebündelten Raster-Capture/Clustergeometrie vergleichen. Verdeckte Äste überspringen;
   ausreichend kleine Eltern direkt auswerten statt alle Einzelgrundrisse zu triangulieren.
   Ray-Abstand in Pixeln bestimmt mit Projektion/Entfernung die Verbandsgröße; Silhouette,
   dünne Türme und Tiefensprünge adaptiv feiner erfassen. Keine Fine-Referenz pro Fernauftrag.
   Capture auf vorhandenem Device bündeln; kein eigener Renderer/Device je Gebäude/Zelle.
4. Generische Oberflächenprodukte speichern Depth/Coverage, Normalen und Basisfarbe/Metallic/Roughness.
   Licht, Fahrzeuglichter und Wolkenschatten bleiben aktuell; bewegte Objekte/Laub eigene Produkte.
   Nicht repräsentierte Materialeffekte behalten native Geometrie; keine stillen Ersatzlobes.
   Brücken/Überhänge/Kronen verlangen mehrere Tiefen, keine universelle 2,5D-Höhenkarte.
   Volumen und dynamische Produkte teilen den Qualitätsauftrag, nicht erzwungen denselben Speicher.
5. Aktualisierung folgt Parallaxe, Disocclusion, Inhalt und gemessener Pixeländerung. Stabile
   Produkte im RAM/GPU wiederverwenden. Betroffene Regionen ergänzen, keine pauschale Weltinvalidierung.
   Vollständige gültige Eltern bis zur atomaren Kind-Publikation behalten; Hysterese verhindert Poppen.
6. Generator besitzt Pläne/Produkte und Qualitätsbelege; Engine Residency/Publikation,
   Renderer Sichtauswahl/aktuelles Shading. Builtins und externe Generatoren teilen die öffentliche API.
   2188 integriert nur dafür fehlende Verträge; 2280 besitzt Quellen und Auftragslebensdauer.

## Qualität passend zur Repräsentation
Bild-/Spielwirkung und Echtzeit bestimmen die zulässige Approximation, keine allgemeine CAD-Toleranz.
Geometrische Vereinfachung verlangt passende Formgrenzen. Bildfelder verlangen Bounds und einen
geprüften Bild-/Parallaxegültigkeitsbereich; Abstand zur flachen Trägergeometrie ist nicht ihr Bildfehler.
ProjectedErrorBudget liefert näherungsweise e_px = e_m × f_px / d; Off-axis, Tiefe und Verdeckung
brauchen eigene Prüfung. Silhouette/Coverage, Helligkeit, Material und zeitlicher Versatz getrennt messen.
BuildingMesh berechnet Fine → Shell aus dem Plan ohne Vertex-/Indexaufbau; unbekannte Mesher bleiben ungeklärt.
Kein Routine-Fine-Bake als Fehlerorakel; CPU-Belege allein bestehen keinen Render-Gate.

## Vorhandene Bausteine und fehlende Integration
Unbeleuchtetes Material-Capture/Atlas-Transport bestehen (2171). SurfaceReprojectionStage überträgt
perspektivische Tiefe/Material bei unverändertem Kameraort; Bewegung/Orthografie werden abgewiesen.
Rundum-Capture, bewegungsgültige Zellfelder, aktuelles Shading und die native Stadtanbindung fehlen.
Legacy-ImpostorCard ist flach; HiZ-History ersetzt keine aktuelle Arbeit in neu sichtbaren Bereichen (2340).

## Fernwelt, Flug und Orbit
Niedrige MVT-Zooms enthalten nicht automatisch Gebäude. Belegte Grobinformation darf plausible
Fernverbände steuern; fehlende Pflicht-Nahinhalte bleiben ein Fehler. Globale Grobprodukte
vor regionalen Kindern, kein weltweites Feinmodell. Terrain folgt eigener Höhen-/Silhouettenqualität;
subpixeliges Relief geht zum Ellipsoid über. Vegetation 2111 nutzt denselben Nah-/Fernbedarf später.
Geometrie-, Speicher-, Aufbereitungs- und Framekosten gemeinsam begrenzen; keine festen Klassenquoten.

## Forschungsgrundlage
Hierarchischer Mesh-/Oberflächen-Hybrid ist die Arbeitsrichtung; Baum/Capture nach Messung wählen.
[Hierarchical Image Caching, SIGGRAPH 1996](https://pages.cs.huji.ac.il/danix-lab/cglab/research/wa/):
räumliche Verbände, Projektionsgültigkeit und amortisierte Capturekosten; statische Bildfarbe ersetzen.
[Layered Depth Images, SIGGRAPH 1998](https://dash.harvard.edu/entities/publication/73120378-7e84-6bd4-e053-0100007fdf3b):
mehrere Tiefen und gefilterte Splats für Parallaxe/Disocclusion; Punkt-Replay allein reicht nicht.
[Far Voxels, SIGGRAPH 2005](https://www.crs4.it/vic/cgi-bin/bib-page.cgi?id=%27Gobbetti%3A2005%3AFV%27):
projektionstreue Fernaggregate/Hierarchie; Fine-Soup-Vorbereitung und Transparenzgrenzen nicht übernehmen.
[Karras, HPG 2012](https://research.nvidia.com/publication/2012-06_maximizing-parallelism-construction-bvhs-octrees-and-k-d-trees):
Morton-sortierte Bereiche/BVH; GPU-Aufbau ist keine Pflicht für statische Pläne auf einem Worker.
[Billboard Clouds](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
[GPU-Driven](../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
[Clipmaps](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf), [Materialtransport](2171_procedural_surfaces_carry_khronos_materials_at_every_distance.md), [Arbeitsauswahl](2340_image_work_follows_visible_change_instead_of_full_frames.md).

## Abnahme
Zuerst Tokyo/Wien/Central Park: vollständige Fernverbände bei deutlich weniger Erzeugung,
residenten Bytes und ausgeführter Geometrie, gleicher/besserer Bildqualität und unverändertem Profil.
Drehung, Bewegung, Licht und Inhaltsänderungen ohne Löcher/Geisterbilder/Poppen. Danach Flug/Orbit.
Abstandsbänder zeigen abnehmenden Vertexbedarf; Quellobjektzahl ersetzt keine Bildkostenmessung.
Native CPU/GPU, Laden und Peaks getrennt belegen; AGENTS-Gates gelten.
