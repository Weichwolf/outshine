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
Eine rundum verfügbare Welt bis 240 km am Boden, mit höhenabhängigem Horizont und später
schnellem Übergang Orbit → Nahdetail. Ferninhalte sind kompakt, Nahdetails gezielt erzeugt.
GeoCellId, GroundLattice, Gebäudepläne und LOD-Auswahl bestehen; Auswahl ist mehrfach/zu spät,
die konservative Zellhülle erzwingt oft Fine. Einfache Gebäudehüllen stellen Wien wieder dar;
die Fernzusammenfassung greift noch zu spät. Frame-Spitzen und Speicher sind nicht budgetgerecht.
Vollständige Fernquellen und Runtime-Hierarchie fehlen.

## Aktuelle Lieferung: einfache Gebäude wiederherstellen
Gebäude bestehen zunächst aus Grundrisswänden und Dach, einschließlich Innenhöfen,
gemessener Höhen und erhöhter Gebäudeteile. Prozedurale Sockelverzierungen, Dachaufbauten,
Gesimse und Fassadenunterteilungen entfallen. Straßen und Terrain-Deformation bleiben erhalten.
Fine und Shell verwenden dieselbe einfache Hülle; Massed bündelt entfernte Gebäude.
Runde Dächer triangulieren Grundriss und Dachspitze ohne pauschale rekursive Unterteilung;
Aufwand bleibt linear zur Grundrissgröße. Formen und Innenhöfe bleiben erhalten.
Die Entfernungsauswahl muss vor der Erzeugung greifen. Fernverbände behalten konservative
Formgrenzen; eine großzügigere Fehlertoleranz darf keine fehlenden Gebäude verdecken.
Zuerst Wien ohne GPU-Speicherabbruch sichtbar machen, dann Cluster und dichte Lastfälle prüfen.
Neue Nahdetails warten auf funktionierende Großstadt-LOD und einen belegten Bildgewinn.

## Besitzer und Abhängigkeiten
Generatoren besitzen Bedarfsplanung/Formfehler, Engine Residency/Publikation, Renderer Sichtbarkeit.
StructureCellPlanner/Detail, StructureBake und GroundLattice bilden einen gemeinsamen Plan.
Vorhandenes ProjectedErrorBudget erlaubt den ersten Schritt. Kein pauschales Warten auf 2188;
benötigte öffentliche Felder werden dort mit diesem Pfad integriert. 2280 führt die Jobs aus.

## Erste Lieferung: Bodenstadt
1. MVT/XML einmal in kompakte native Gebäudepläne überführen: Grundriss/Höfe/Parts, Höhen,
   Dach-/Fassadenparameter, Bounds und Terrainkontakte. Kontaktpläne brauchen kein fertiges Mesh.
2. Entfernung/Projektion und zulässigen Fehler vor Rasterbedarf/Mesh bestimmen. Eine Auswahl
   ersetzt unterschiedliche WholeTile-/Cell-Policies. Zellaufträge teilen Inputs derselben
   Auftragsgeneration; Teilbedarf darf keinen Parent-Digest reproduzieren müssen.
3. Fernverband → Massing → Hülle → Nahdetails aus demselben Plan. Fernverbände bündeln,
   Nahteile instanzieren. Standort/Seeds/Silhouette bleiben stabil. Jede Variante liefert
   Bounds, Kosten und eine konservative Schranke für die tatsächlich ausgelassene Form.
   meshoptimizer ordnet native Index-/Vertexbuffer für Cache und Fetch; keine zweite Meshkopie
   im residenten Endprodukt. Simplifizierung nur mit erhaltenen Höfen, Part-Grenzen und
   Attributnähten. Bibliotheksfehlerwerte ersetzen keine konservative Oberflächenschranke.
4. Für planbasierte Detailreduktionen analytische Schranken herleiten und unabhängig prüfen;
   Höfe/Öffnungen, beide Oberflächenrichtungen und Terrainfehler berücksichtigen. Vorhandene
   Triangle-/SurfaceError-Verfahren sind Entwicklungsorakel; keine Fine-Referenz pro Fernjob
   als Routine. Unbewiesene Varianten erhalten keine kleinere Schranke. Verfahren noch zu validieren.
5. Vollständigen groben Elternstand bis zur atomaren Kind-Publikation halten; Hysterese gegen
   Poppen. Änderung invalidiert nur betroffene Produkte, Drehung erzeugt keine Inhalte neu.
   Arbeit und residente Bytes gemeinsam budgetieren, keine festen Klassenquoten.

## Fernwelt, Flug und Orbit
- Quellabdeckung zuerst belegen: niedrige MVT-Zooms enthalten nicht automatisch Gebäude.
  Lieferbare Ferninformation oder einmalige Aggregation vollständiger Quelldaten nachweisen;
  fehlende Inhalte nicht als LOD deklarieren. Dieser Vertrag bleibt offen, daher `planned`.
- Globale Grobprodukte vor regionalen Kindern; Bedarf aus Höhe/Horizont und Bildschirmfehler.
  Kein weltweites Feinmodell, kein vollständiger Feinradius und kein persistenter Generatorcache.
- Terrainbedarf folgt konsumierter Form mit eigener Höhenfehlerschranke. Subpixelrelief darf
  zum Ellipsoid übergehen; sichtbare Küsten/Grate bleiben. Vegetation 2111 verwendet dieselbe
  Auswahl von Fernwald bis Nahlaub; Kollision und logische Netze bleiben eigenständig.
  Der Fehler gehört zur jeweiligen interpolierten Fläche; ein Kindfehler ist keine Untergrenze
  für den Elternfehler. Bedarf bleibt unabhängig von der Blickrichtung.
  Koerbersee rendert mit bedarfsgesteuertem Höhenatlas wieder vollständig.
  Renderer allokiert Atlas-Layer nach tatsächlichem Bedarf und wächst transaktional per GPU-Kopie;
  vorhandene Page-IDs/Inhalte bleiben gültig. Dichtere 2D-Packung begrenzt Array-Layer.
  Logische Kapazität ist kein Allokationsbudget: aktuelle/temporäre Bytes und Framekosten prüfen.
  Residente CPU-Terrainnetze für Kontakte/Audio getrennt vom Bildschirmdetail begründen;
  rasterbasierte Abfragen gegen unnötig ausmultiplizierte Dreiecke prüfen.
  Lokale Fehler und Rundum-Projektion steuern weiter die Auswahl; Sichtweite/Fehlertoleranz bleiben.

## Forschungsgrundlage
[GPU-Driven Rendering](../doc/references/downloads/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf)
und [Geometry Clipmaps](../doc/references/downloads/terrain/siggraph/2004-geometry-clipmaps.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Material-Batches, Cluster-Bounds
und inkrementelle Gitteraktualisierung prüfen. GPU-Culling ersetzt keine frühe Bedarfsauswahl;
Clipmaps liefern keinen konservativen Fehler oder weltweite Abdeckung. SDL_GPU-Vertrag und
lokale Kosten entscheiden. Cesium-Elternabdeckung mit Rundum-Residency verbinden.
[Simplification Envelopes, SIGGRAPH 1996](../doc/references/downloads/geometry/siggraph/1996-simplification-envelopes.pdf):
Beidseitige Abstände begrenzen; Stichproben und Simplifier-Metriken beweisen keine Hülle.

## Abnahme
Zuerst dichte Bodenstadt: vollständige Fernverbände und Nahdetails ohne überflüssige Feinmeshes,
Löcher, Formwechsel oder neue IO-Arbeit beim Drehen. Danach Flug und Orbit separat prüfen.
Gleiche Inhalte/Profil/Sichtweite, geringere gemessene Arbeit/Bytes und AGENTS-Budget;
reine CPU-Beweise oder ein gesetzter Sichtweitenparameter schließen den WI nicht.

Bibliotheksvertrag und Installation: [Abhängigkeiten](../doc/dependencies.md).
Verfahren: [meshoptimizer](https://github.com/zeux/meshoptimizer); GPU-Kosten lokal messen.
