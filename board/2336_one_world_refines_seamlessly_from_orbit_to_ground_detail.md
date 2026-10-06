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
die Fernzusammenfassung greift noch zu spät. Tokyo hält 29,65 Mio. erzeugte Gebäudedreiecke.
Begrenzte Upload-Batches beheben den belegten Metal-Speicherabbruch; Hausflächen/Türme sind wieder da.
Bei 1280×720/60, 240 km und voller Drehung bleiben Tokyo p50/p99 35,65/479,44 ms, Central Park 13,20/84,27 ms
rot (`15e11a364`); kein sichtbarer Bildgewinn. Erzeugte Dreiecke sind keine Messung ausgeführter GPU-Arbeit.
Isolierte Passabschlüsse lokalisieren die Hauptkosten im nativen Rasterpass: Tokyo/Central Park
~33,5/12,4 ms; konstante Tokyo-Beleuchtung ~32,9 ms. Diagnose, keine GPU-Timestamps/Abnahme.

## Aktuelle Lieferung: einfache Hüllen mit wirksamer Fernreduktion
Gebäude bestehen zunächst aus Grundrisswänden und Dach, einschließlich Innenhöfen,
gemessener Höhen und erhöhter Gebäudeteile. Prozedurale Sockelverzierungen, Dachaufbauten,
Gesimse und Fassadenunterteilungen entfallen. Straßen und Terrain-Deformation bleiben erhalten.
Fine und Shell verwenden dieselbe einfache Hülle; Massed bündelt entfernte Gebäude.
Runde Dächer linear triangulieren; Formen, Höfe und gemeinsame Ringkontakte innerhalb 1 mm erhalten.
Vor Erzeugung auswählen; konservative Formgrenzen nicht lockern, um fehlende Gebäude zu verdecken.
Draw-Bedarf vor Attributallokation bestimmen; starre Posen nutzen den Positionsbuffer als Vorpose.
Tangenten/Farben belegen eigene dichte Arenen; Instanz-Offsets adressieren ihre Verbraucher.
Späte kleine Tangentenmeshes reservieren keine positionsabhängige Lücke; Tokyo 41,46 MB → 16 KiB.
Tokyos GPU-Streamkapazität ohne Transfer sinkt 5,36 → 3,88 GB (Emission/Vorpose/Tangenten); Bilder pixelgleich.
CPU-Heap ~4,79 GB; aktueller OS-Spitzenfootprint 11,33 GB, maximaler RSS 2,85 GB.
Frühere systemweite Swapouts 6,83 GB; Prozessanteil und Framegewinn nicht bewiesen.

## Auswahl der einfachen Hülle
BuildingMesh liefert eine positive beidseitige Shell-Schranke aus Laibungstiefe, Millimeter-
und Float-Rundung, konservativ auf Viertelmeter aufgerundet. Dächer/Höfe/Parts bleiben gleich.
StructureBake führt das Maximum aller belegten Gebäude einer Zelle bis zum AcceptedInput;
unbekannte Mesher, ausgelassene Formen und Massing liefern keine engere Shell-Schranke.
Zellplanung nutzt diese Schranke vor der Erzeugung; Massed behält die volle Zellhülle.
Shell bleibt bei Bewegung wiederverwendbar, sichtbare Nahlaibungen bleiben Fine.
Render-Schranken ändern keine Terrain-Semantik; Bild-/Kostenabnahme bleibt erforderlich.
Zeitliche HiZ-Verdeckung gilt nur bei identischer Projektion, Weltbasis und unveränderten
Geometrie-, Draw-Tabellen- und Terrainständen. Tabellen-Rebuilds erhöhen die Produktgeneration. Bewegung erzeugt zunächst vollständige Frustum-/LOD-Sichtbarkeit;
stationäre Folgebilder dürfen mit belegter Tiefe verfeinern und danach die Auswahl wiederverwenden.
Nur erfolgreich eingereichte GPU-Arbeit bestätigt den Auswahlzustand; Fehler bleiben wiederholbar.
Verworfene Diagnosen bleiben außerhalb der Runtime: `e37b7fce7`, `ca95cc4b3`, `9079ead97`.
Rückseitenkegel/Fetch-Umordnung sparen insgesamt nicht; erzwungene Shell-Eltern verlieren die Stadt.
Tiefenblöcke erfassen Randpixel mit Quellgröße/Blockspanne. Zusatzpässe brauchen Gesamtersparnis;
Multi-Draw erhält Reihenfolge/Instanzen/Culling; ausgeführte Geometrie/Warten getrennt messen.

## Bildabhängige Fernrepräsentation
Ferne Gebäude gemeinsam auf tiefenhaltige Karten mit zwei Dreiecken je Karte projizieren;
Rundum-/Layered-Capture prüfen. Bedarf vor Mesh-Aufbau; einfache Hüllen oder Quellpläne erfassen,
Raster-/Ray-Capture vergleichen. Karte und Capture erhalten lineare Basisfarbe, Normalen und
Metallic/Roughness nach nativer Komposition; unrepräsentierte Lobes werden ausdrücklich abgewiesen.
ImpostorCard verwirft noch Tiefe. Vor Stadtintegration Tiefenraum/Rückprojektion und Kameragültigkeit
gegen native Hüllen prüfen; Runtime-Captures auf vorhandenem Device bündeln, kein Asset-Baker je Zelle.
Capture an Kameraposition/Zelle binden, nicht Blickrichtung: Drehung verwendet dieselbe Karte.
Aktueller Bake ist orthografisch; Stadt-Capture braucht Ursprung und inverse Projektion für Rückprojektion.
Coverage/Normalen erhalten; Beleuchtung und Gesamthelligkeit aktuell auswerten.
Update bei zu großer Pixelverschiebung, Inhaltsänderung oder Disocclusion; gültiger Hüllen-Fallback.
Nur bild-/schattenwirksame Geometrie nach jeweiligem Bedarf; grobe Fernoccluder statt Nahdetails.
Silhouette/Parallaxe/Bytekosten begrenzen Auflösung/Ansichten; rundum, nur RAM/GPU.
[Billboard Clouds, SIGGRAPH 2003](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf);
Overdraw/Capturekosten gegen Clustergeometrie messen; Arbeitsintervalle/History besitzt 2340.

## Besitzer und Abhängigkeiten
Generatoren besitzen Planung/Formfehler, Engine Residency/Publikation, Renderer Sichtbarkeit.
ProjectedErrorBudget besteht; fehlende öffentliche Felder integriert 2188, Jobs 2280.

## Erste Lieferung: Bodenstadt
1. MVT/XML einmal in kompakte native Gebäudepläne überführen: Grundriss/Höfe/Parts, Höhen,
   Dach-/Fassadenparameter, Bounds und Terrainkontakte. Kontaktpläne brauchen kein fertiges Mesh.
2. Entfernung/Projektion und zulässigen Fehler vor Rasterbedarf/Mesh bestimmen. Eine Auswahl
   ersetzt unterschiedliche WholeTile-/Cell-Policies. Zellaufträge teilen Inputs derselben
   Auftragsgeneration; Teilbedarf darf keinen Parent-Digest reproduzieren müssen.
3. Fernverband → Massing → Hülle → Nahdetails aus demselben Plan. Fernverbände bündeln,
   Nahteile instanzieren. Standort/Seeds/Silhouette bleiben stabil. Jede Variante liefert
   Bounds, Kosten und eine konservative Schranke für die tatsächlich ausgelassene Form.
   [meshoptimizer](https://github.com/zeux/meshoptimizer) ordnet Index-/Vertexbuffer für Cache und Fetch; keine zweite Meshkopie
   im residenten Endprodukt; Umordnung nur bei gemessenem Nutzen. Simplifizierung erhält Höfe,
   Part-Grenzen/Attributnähte; Bibliotheksfehlerwerte ersetzen keine konservative Oberflächenschranke.
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
## Forschungsgrundlage
[GPU-Driven Rendering](../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
[Geometry Clipmaps](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf): Batches, Cluster-Bounds, inkrementelle Gitter.
[Simplification Envelopes, SIGGRAPH 1996](../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf):
Beidseitige Abstände begrenzen; Stichproben und Simplifier-Metriken beweisen keine Hülle.

## Abnahme
Zuerst dichte Bodenstadt: vollständige Fernverbände und Nahdetails ohne überflüssige Feinmeshes,
Löcher, Formwechsel oder neue IO-Arbeit beim Drehen. Danach Flug und Orbit separat prüfen.
Gleiche Inhalte/Profil/Sichtweite, geringere gemessene Arbeit/Bytes und AGENTS-Budget;
reine CPU-Beweise/ein Sichtweitenparameter schließen den WI nicht. [Bibliotheksvertrag](../doc/dependencies.md).
