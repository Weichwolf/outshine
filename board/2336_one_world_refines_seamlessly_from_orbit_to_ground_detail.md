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
die Fernzusammenfassung greift noch zu spät. Tokyo steht mit 29,65 Mio. Gebäudedreiecken,
aber p50 39 ms/p99 927 ms: frühe Cluster-Auswahl und kompakte Fernprodukte bleiben offen.

## Aktuelle Lieferung: einfache Gebäude wiederherstellen
Gebäude bestehen zunächst aus Grundrisswänden und Dach, einschließlich Innenhöfen,
gemessener Höhen und erhöhter Gebäudeteile. Prozedurale Sockelverzierungen, Dachaufbauten,
Gesimse und Fassadenunterteilungen entfallen. Straßen und Terrain-Deformation bleiben erhalten.
Fine und Shell verwenden dieselbe einfache Hülle; Massed bündelt entfernte Gebäude.
Runde Dächer linear triangulieren, ohne rekursive Unterteilung; Formen und Innenhöfe erhalten.
Die Entfernungsauswahl muss vor der Erzeugung greifen. Fernverbände behalten konservative
Formgrenzen; eine großzügigere Fehlertoleranz darf keine fehlenden Gebäude verdecken.
Hof-/Außenkontakte nach geografischer Umrechnung innerhalb 1 mm als gemeinsamen Ringpunkt verbinden.
Wien, Tokyo und Central Park vollständig rendern; kein Überspringen fehlgeschlagener Gebäude.
Neue Nahdetails warten auf funktionierende Großstadt-LOD und einen belegten Bildgewinn.

## Auswahl der einfachen Hülle
BuildingMesh liefert eine positive beidseitige Shell-Schranke aus Laibungstiefe, Millimeter-
und Float-Rundung, konservativ auf Viertelmeter aufgerundet. Dächer/Höfe/Parts bleiben gleich.
StructureBake führt das Maximum aller belegten Gebäude einer Zelle bis zum AcceptedInput;
unbekannte Mesher, ausgelassene Formen und Massing liefern keine engere Shell-Schranke.
Zellplanung nutzt diese Schranke vor der Erzeugung; Massed behält die volle Zellhülle.
Shell bleibt bei Bewegung wiederverwendbar, sichtbare Nahlaibungen bleiben Fine.
Render-Schranken ändern keine Terrain-Semantik. Runtime-Bild-/Kostenabnahme bleibt erforderlich.
Zeitliche HiZ-Verdeckung gilt nur bei identischer Projektion, Weltbasis und unveränderten
Geometrie-, Draw-Tabellen- und Terrainständen. Tabellen-Rebuilds erhöhen die Produktgeneration. Bewegung erzeugt zunächst vollständige Frustum-/LOD-Sichtbarkeit;
stationäre Folgebilder dürfen mit belegter Tiefe verfeinern und danach die Auswahl wiederverwenden.
Nur erfolgreich eingereichte GPU-Arbeit bestätigt den Auswahlzustand; Fehler bleiben wiederholbar.
Für bewegte Ansichten eine aktuelle Tiefenvorlage aus gültigen nativen Occludern prüfen;
vorige Sichtbarkeit darf Arbeit priorisieren, niemals neu freigelegte Inhalte ausschließen.
Zusätzliche Pässe nur bei belegter Gesamtersparnis; ausgeführte Geometrie und Wartepfad getrennt messen.
Benachbarte indirekte Batches mit gleichem Material, Vertexlayout und Bewegungsvertrag als
Multi-Draw einreichen. Direkte Batches begrenzen den Lauf; Reihenfolge, Instanzen und Culling bleiben gleich.
SDL-Aufrufe getrennt von logischen Batches zählen; unveränderte Bilder und gemessene Encodingkosten prüfen.

## Bildabhängige Fernrepräsentation
Fernverbände als tiefenhaltige Multi-View-/Layered-Impostors prüfen; bestehender ImpostorBaker
liefert bereits Tiefe, Normale und Materialidentität, ist aber noch keine Fernstadt-Pipeline.
Keine beleuchtete Farbe festbacken: Sonne/lokales Licht müssen dieselben Materialdaten nutzen.
Coverage, Parallaxe und Ansichtswechsel begrenzen die Wahl; bei unbewiesener Disocclusion Mesh-Fallback.
Atlasauflösung/-Ansichten nach Pixelwirkung und Bytekosten, rundum verfügbar, nur im RAM/GPU.
[Billboard Clouds, SIGGRAPH 2003](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf);
Overdraw/Capturekosten gegen Clustergeometrie messen. Arbeitsintervalle/History besitzt 2340.

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
  Höhenatlas nach Bedarf allokieren, transaktional wachsen; Page-IDs/Inhalte erhalten.
  Packung, aktuelle/temporäre Bytes und Framekosten gemeinsam begrenzen.
  Residente CPU-Terrainnetze für Kontakte/Audio getrennt vom Bildschirmdetail begründen;
  rasterbasierte Abfragen gegen unnötig ausmultiplizierte Dreiecke prüfen.
  Lokale Fehler und Rundum-Projektion steuern weiter die Auswahl; Sichtweite/Fehlertoleranz bleiben.

## Forschungsgrundlage
[GPU-Driven Rendering](../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf)
und [Geometry Clipmaps](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
([Primärquellen/Einordnung](../doc/references/README.md)): Material-Batches, Cluster-Bounds
und inkrementelle Gitteraktualisierung prüfen. GPU-Culling ersetzt keine frühe Bedarfsauswahl;
Clipmaps liefern keinen konservativen Fehler oder weltweite Abdeckung. SDL_GPU-Vertrag und
lokale Kosten entscheiden. Cesium-Elternabdeckung mit Rundum-Residency verbinden.
[Simplification Envelopes, SIGGRAPH 1996](../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf):
Beidseitige Abstände begrenzen; Stichproben und Simplifier-Metriken beweisen keine Hülle.

## Abnahme
Zuerst dichte Bodenstadt: vollständige Fernverbände und Nahdetails ohne überflüssige Feinmeshes,
Löcher, Formwechsel oder neue IO-Arbeit beim Drehen. Danach Flug und Orbit separat prüfen.
Gleiche Inhalte/Profil/Sichtweite, geringere gemessene Arbeit/Bytes und AGENTS-Budget;
reine CPU-Beweise oder ein gesetzter Sichtweitenparameter schließen den WI nicht.

Bibliotheksvertrag und Installation: [Abhängigkeiten](../doc/dependencies.md).
Verfahren: [meshoptimizer](https://github.com/zeux/meshoptimizer); GPU-Kosten lokal messen.
