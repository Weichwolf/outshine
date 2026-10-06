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
Rundum verfügbare Welt bis 240 km am Boden, später höhenabhängiger Horizont und Orbit → Nahdetail. Der Geometriebedarf nimmt mit Entfernung ab: Häuser → Blockverbände → kompakte Skyline-Flächen.
Native Quellabfragen liefern Dach-/Wandtreffer mit DEM-Kontakt vor jedem Vertex-/Indexaufbau.
Sichtauswahl umfasst jetzt alle Detailstufen/Zellen einer Kachel. Nahe Inhalte bleiben unverändert
vollständig; ferne Flächen werden nur bei geringeren geschätzten Kosten zu Tiefenpatches.
Rundumprodukte behalten Materialkoordinaten/Normalen und aktuelle Beleuchtung; Translationreuse null.
| 720p60, offline, 360° | Dreiecke vorher → jetzt | p99 ms vorher → jetzt | Laden s |
|---|---|---|---|
| Wien | 3,08 Mio. → 1,61 Mio. | 70,54 → 21,34 | 94,26 |
| Central Park | 3,47 Mio. → 1,24 Mio. | 48,88 → 39,54 | 69,67 |
| Tokyo | 13,36 Mio. → 2,36 Mio. | 134,70 → 18,65 | 109,66 |
Bilder geöffnet: 1,28/0,41/0,80 % geänderte Pixel; Schatten-/Coverageänderungen nicht abgenommen.
Gemessener Prozesspeak CP/Tokyo: 2,68/3,01 GB; hoher Speicher-/Ladebedarf bleibt unbegründet.
Alle drei Frame-Gates bleiben rot. Dreieckszahl allein beweist die Ursache der Frame-Spitzen nicht.
Wien-Prüfwert rund 300.000 (= 3 Mio. / 10), kein belegtes Optimum. Kachelweise Sichtauswahl
ist noch keine gemeinsame Welthierarchie; Verdecker über Kachelgrenzen und stabile Bewegung fehlen.

## Aktuelle Lieferung: Fernstadt vor weiteren Nahdetails
| Klasse | Produkt | Bedarf vor Erzeugung |
|---|---|---|
| Fine | Räumliche Nahfassaden/Dächer | Sichtbare Form, Interaktion, aktuelle Schatten |
| Shell | Grundrisswände/Dach ohne sekundäre Geometrie | Projizierter Formfehler rechtfertigt Hülle |
| Massed | Gemeinsam erfasste Blockverbände | Silhouette/Coverage bleiben, Einzelhäuser entfallen |
| Skyline | Wenige tiefenhaltige Impostor-/Silhouettenflächen | Fernbild und Parallaxe statt Einzelvolumen |

Zuerst Massed/Skyline, dann Nahkosten; Pläne behalten Grundrisse/Höfe/Parts, Höhen, Material und Terrainkontakt. Straßenqualität und
Terrain-Deformation bleiben erhalten. Kontakt/Kollision hängen nicht von der Bildrepräsentation ab.
Ferne Details dürfen aus belegter Dichte/Nutzung/Relief statistisch angenähert werden; Silhouette,
Coverage, Farbe und Lichtwirkung entscheiden. Herkunft bleibt explizit. Näherkommen ersetzt die
Schätzung durch feinere quellengestützte Produkte mit stabilem Übergang.

## Neue Lieferentscheidung
Wien: 100.446 verschiedene nicht versteckte Polygone aus 49 Nahkacheln/12,4 MB; Python 5,3 s.
Der Bestand reicht bis etwa 8,64 km; Parts/Kachelfragmente zählen separat. 240 km bleiben offen.
Engine fragt fertige Assets an; nur Cachemisses starten Anreicherung/Asset-Erzeugung (2280).
Warmstart lädt Asset-Rohlinge; Nahdetails entstehen budgetiert aus ihren fertigen Plänen. Boxhüllen: bis zu zwei sichtbare Wände und ein Dach
(sechs Dreiecke); Massed/Skyline ersetzen ferne Einzelhüllen. Grundrisse/Höfe/Teile nahe erhalten.
[Python](../test/experiments/building_surface.py): Tokyo 588.862 Pläne → 8.782 Flächendreiecke
im flachen Modell, keine native Abnahme. Generator-/Renderer-/Hybrid-Verdeckung nativ vergleichen:
identische Inputs/Bilder, Erstaufbau/Warmstart, Bytes, 360°/Translation und aktuelle Schatten.
Warmstart Wien <1 s und wenige ms Draw als Prüfziele; der neue Ablauf ersetzt ungeeignete Verfahren.

## Neue Erzeugungskette und Zuständigkeit
1. Cachemisses erzeugen vollständige angeforderte Assets samt Fachplänen/LOD-Produkten. OSM: gepackte Ringe,
   Höhen/Kontakte, Herkunft/Erscheinung; Terrain und Vegetation behalten passende Fachformate.
   Gemeinsamer Assetindex (2280) nutzt Bounds, Eltern/Kinder, Qualität/Kosten ohne Quelltypen.
   Gepackte Bereiche/Morton-Ordnung prüfen; Quadtree versus BVH messen, GeoCellId beibehalten.
2. Kameraort/Höhe, Projektionsmaßstab und Qualitätsauftrag wählen räumliche Produkte.
   Rundumbedarf ist unabhängig von Blickrichtung; Drehung wählt nur bereits verfügbare Flächen.
   Größere Entfernung erlaubt größere Weltfehler bei kontrolliertem Pixelfehler.
   Bei Bewegung nur betroffene Knoten/Detailgrenzen neu bewerten; unveränderte Produkte behalten.
   Eltern aus dem Cache wählen; nur fehlende Rohlinge erzeugen. Nahe Details daraus verfeinern,
   ohne Quelldecode/Anreicherung oder Neubau unveränderter Rohlinge.
3. Fernprodukte direkt aus Plänen erfassen; hierarchische Ray-Bündel von nah nach fern
   gegen gebündelten Raster-Capture/Clustergeometrie vergleichen. Verdeckte Äste überspringen;
   ausreichend kleine Eltern direkt auswerten statt alle Einzelgrundrisse zu triangulieren.
   Ray-Abstand in Pixeln bestimmt mit Projektion/Entfernung die Verbandsgröße; Silhouette,
   dünne Türme und Tiefensprünge adaptiv feiner erfassen. Keine Fine-Referenz pro Fernauftrag.
   Capture auf vorhandenem Device bündeln; kein eigener Renderer/Device je Gebäude/Zelle.
4. Generische Oberflächenprodukte speichern Depth/Coverage, Normalen und Basisfarbe/Metallic/Roughness.
   Licht, Fahrzeuglichter und Wolkenschatten bleiben aktuell; bewegte Objekte/Laub eigene Produkte.
   Nicht repräsentierte Effekte behalten Geometrie; Brücken/Überhänge/Kronen verlangen mehrere Tiefen.
   Volumen und dynamische Produkte teilen den Qualitätsauftrag, nicht erzwungen denselben Speicher.
5. Aktualisierung folgt Parallaxe, Disocclusion, Inhalt und gemessener Pixeländerung. Stabile
   Assetprodukte von SSD/RAM/GPU wiederverwenden. Betroffene Regionen ergänzen, keine pauschale Weltinvalidierung.
   Vollständige gültige Eltern bis zur atomaren Kind-Publikation behalten; Hysterese verhindert Poppen.
6. Generator besitzt Pläne/Produkte und Qualitätsbelege; Engine Residency/Publikation,
   Renderer Sichtauswahl/aktuelles Shading. Builtins und externe Generatoren teilen die öffentliche API.
   2188 integriert nur dafür fehlende Verträge; 2280 besitzt Quellen und Auftragslebensdauer.

## Qualität passend zur Repräsentation
Bild-/Spielwirkung und Echtzeit bestimmen die zulässige Approximation, keine allgemeine CAD-Toleranz.
Geometrische Vereinfachung verlangt passende Formgrenzen. Bildfelder verlangen Bounds und einen
geprüften Bild-/Parallaxegültigkeitsbereich; Abstand zur flachen Trägergeometrie ist nicht ihr Bildfehler.
ProjectedErrorBudget: e_px ≈ e_m × f_px / d; Off-axis/Tiefe/Verdeckung separat prüfen.
Silhouette/Coverage, Helligkeit, Material und zeitlichen Versatz getrennt messen.

## Fernwelt, Flug und Orbit
Niedrige MVT-Zooms enthalten nicht automatisch Gebäude. Belegte Grobinformation darf plausible
Fernverbände steuern; fehlende Pflicht-Nahinhalte bleiben ein Fehler. Globale Grobprodukte
vor regionalen Kindern, kein weltweites Feinmodell. Terrain folgt eigener Qualität; subpixeliges Relief geht zum Ellipsoid über. Vegetation 2111 folgt später.

## Forschungsgrundlage
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
[Clipmaps](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf), [Materialtransport](2171_consistent_materials_unify_the_world.md), [Arbeitsauswahl](2340_image_work_follows_visible_change_instead_of_full_frames.md).

## Abnahme
Zuerst Tokyo/Wien/Central Park: vollständige Fernverbände bei deutlich weniger Erzeugung,
residenten Bytes und ausgeführter Geometrie, gleicher/besserer Bildqualität und unverändertem Profil.
Drehung, Bewegung, Licht und Inhaltsänderungen ohne Löcher/Geisterbilder/Poppen. Danach Flug/Orbit.
Abstandsbänder zeigen abnehmenden Vertexbedarf; Quellobjektzahl ersetzt keine Bildkostenmessung.
Native CPU/GPU, Laden und Peaks getrennt belegen; AGENTS-Gates gelten.
