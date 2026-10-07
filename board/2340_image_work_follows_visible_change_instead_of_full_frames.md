Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: render, engine, world
Tags: performance, visibility, temporal, budgets

# Image work follows visible change and validated history

## Ergebnis und Ist
Kosten wachsen mit wirksamer Bildänderung, sichtbarem Detail und betroffenen Lichtregionen.
StageCache hält Atmosphäre, Reflexionsatlas und statische Sonnenschatten bereits wiederverwendbar.
Native Sichtbarkeit hat Submission-/Generation-Verträge; TAA rekonstruiert fertige Farben,
spart derzeit aber keine Raster-/Shadingarbeit. Kamera-/Projektionsänderung verwirft die Auswahl.
Alle zehn Places einschließlich Wien/Tokyo/Central Park müssen unter 10 ms pro Frame bleiben,
beim aktuellen Bildstand einschließlich
Anfangsframes/p99 bei unverändertem 1280×720@60/360°/Inhalt. Vorher keine neuen Bildfeatures.
GPU-Passzeit ist noch nicht direkt messbar; Fence-Warten erklärt keine einzelne GPU-Stufe.
2336 besitzt Weltrepräsentation/LOD, 2155 Licht, 2171 Materialien; dieser WI besitzt Arbeitsauswahl.

## Priorisierung nach Kostenreview
Aktuelle Offline-Fenster (2280): CP/Tokyo 2,59/3,62 ms p99 bei 1280×720/60/360°;
Wien nach Prototyp-Neubau 29,23 ms, davon schlechtester Frame 27,87 ms Fence-Warten.
Kein Beleg für generell langsames Pixel-Shading. Zuerst 2336/2280s unnötigen Ladebedarf
und 2339s Bereitschaft-/Peakursachen beheben; keine Renderer-Neufassung ohne direkten Passbefund.
Pixelbudget-Verfahren bleiben für reichere Materialien/Lichter/Wolken erforderlich.

## Besitzer und Umsetzung
Kandidaten zuerst mit echten Eingaben in kleinen Python-Experimenten vergleichen, anschließend
den besten gemessenen Ansatz nativ integrieren. Bild-/Spielwirkung bestimmt die zulässige Approximation.
SceneRenderer besitzt das Pixelbudget: vorderste Coverage/Tiefe bestimmen, Material/Licht erst
für wirksame Oberflächen auswerten. Stadt, Wald und offener Boden teilen denselben Pixelauftrag.
Kompakten Visibility-/Surface-Resolve gegen Depth-Prepass plus Clustered Forward vergleichen;
SDL_GPU-Shaderverträge, doppelte Geometriearbeit und zusätzlicher Speicherverkehr entscheiden.
Kein breites G-Buffer auf Verdacht. Generator-/Renderer-/Hybrid-Verdeckung mit denselben
Asset-Rohlingen vergleichen (2336/2280); Residency, Erzeugung und Draw getrennt bewerten. Transparenz/Reflexion/Schatten haben begrenzte Zusatzarbeit;
Hardware-Early-Z ersetzt keinen hierarchischen Ausschluss verdeckter Unterbäume.

1. RenderCatalogue/Compiled und SceneRenderer nutzen vorhandene Passabhängigkeiten/StageCache.
   Inhalt, Kamera/Projektion, Licht, Material, Zeit und Coverage bestimmen Gültigkeit getrennt.
   Keine pauschale Regel „statisches Objekt = fertiger Pixel“; Blickdrehung kann Inhalte freilegen.
2. Native Bounds/Transformationen liefern konservative projizierte Änderung einschließlich
   Kamera, Silhouette und Schattenwirkung. Qualitätsauftrag bestimmt die Pixeltoleranz;
   geometrischer Fehler und zeitlicher Versatz verbrauchen gemeinsam dieselbe zulässige Grenze.
   Helligkeit, Glanz, Transparenz und Coverage erhalten eigene Gültigkeitsprüfungen.
   TAA-Jitter ist kein Weltumbau; Frustum-Abdeckung muss dennoch alle Jitterpositionen enthalten.
3. Unveränderte Produkte wiederverwenden; relevante Änderungen markieren abhängige Bildregionen.
   Wiederverwendung von Tiefe/Normalen/Material getrennt von beleuchteter Farbe prüfen.
   Zuerst Sichtbarkeit/Geometrie, dann begrenzte Shading-Reprojektion mit Tiefe und Produktgeneration.
   Disocclusion, Quellenwechsel und unbewiesene History werden aktuell berechnet, niemals ausgelassen.
4. Bildwirksame Bewegung begrenzt Updateintervalle; akkumulierte Änderung löst Aktualisierung aus.
   Entfernung allein begründet weder geringere Frequenz noch niedrigere Auflösung.
   Schnelle Drehung, Lichtwechsel, Wetter und lokale Aktionen erzwingen betroffene Arbeit.
   Schatten, Reflexionen und Simulation dürfen auch ohne direkte Sichtbarkeit benötigt werden.
5. Statische und dynamische Inhalte trennen. Physik/Commands behalten ihren festen Takt;
   Darstellungsprodukte können seltener aktualisieren. Konvergierte unveränderte Bilder weiter
   präsentieren, ohne erneut die gesamte Szene zu berechnen; laufende TAA-Konvergenz berücksichtigen.
6. Qualitätsstufen wählen räumliche/zeitliche Auflösung pro tatsächlich benötigtem Pass.
   Silhouetten, Tiefenkanten und Nahinteraktion bleiben fein; Himmel/Wolken und glatte Fernbeiträge
   dürfen mit validierter Rekonstruktion gröber sein. Profil/Sichtweite bleiben im Vergleich gleich.
   Bounded Ressourcen/Listen und transaktionale GPU-Lebensdauer aus 2188 verwenden.

## Reihenfolge und Grenzen
Zuerst Geometrie-/Residentkosten in 2336 und gemessene GPU-Arbeit eingrenzen, keine Pixelcache-
Architektur auf Verdacht. Danach ein bewegter Stadtfall mit wiederverwendbarer Sichtbarkeit,
dann ein stationärer Licht-/Materialfall. Keine vollständige Renderer-Neufassung vor diesem Nachweis.
GPU-Arbeit folgt hierarchischem Bildbedarf, nicht einem flachen Test jedes Hauses/Baums.
Bei gleicher projizierter Coverage/Qualitätsgrenze soll höhere Objektzahl hauptsächlich die
Vorbereitung beeinflussen; überdeckte/subpixelige Unterbäume als ein Produkt behandeln.
Vegetation übernimmt diesen Vertrag später; ihre Implementierung wird nicht vorgezogen.
Rasterisierung bleibt Basis; Ray-Box für Innenräume (2171), begrenzte SSR-DDA (2155) und
Projektionsgitter für geeignete Wasserflächen (2145) sind gezielte Verfahren, kein universeller Ersatz.

## Forschungsgrundlage
[Lokaler SoftGL-Vergleich](../doc/references/geometry/softgl-local-review.md): kompakte
Sortierschlüssel und streng gültige Tiefenwiederverwendung auf native Cluster übertragen.
Zusätzliche Hierarchieprüfungen und Vertexfilter nur bei geringerer tatsächlicher Framezeit;
Diagnosezähler getrennt messen. Keine Warm-up-Frames in der Place-Abnahme.
[GPU-Driven Rendering, SIGGRAPH 2015](../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf):
kompakte native Auswahl, Instancing und gebündelte Einreichung.
[TAA Survey, CGF 2020](../doc/references/presentation/cgf/2020-temporal-antialiasing-survey.pdf):
Reprojektion/Validierung und Grenzen bei Occlusion, Beleuchtung und subpixeliger Information.
Lokales Filament `ef1a133d` dient als Integrationsvergleich, nicht als Kostenbeweis.

## Abnahme
Unbewegte Kamera, Drehung, Bewegung, Lichtwechsel und neue Geometrie separat messen.
Keine verschwindenden Inhalte, Geisterbilder, Helligkeits-/Coverage-Sprünge oder verzögerten Aktionen.
Native Geometriearbeit, Shadingarbeit, Host-Encoding, Fence-Warten und GPU-Zeit getrennt ausweisen;
fehlende GPU-Zeit bleibt unbekannt. Weniger Gesamtarbeit/Bytes bei gleicher oder besserer
Bildqualität im identischen Place-Profil nachweisen. Gates/Logs bleiben in AGENTS/Git/Temp.
Wien unter 10 ms ohne Zusatzframes, fehlende Inhalte oder gelockerte Formgrenzen; dichte und
dünne Quellen bei gleicher Coverage vergleichen. Faktor zehn nicht aus kleineren Bildern,
anderer Sichtweite oder einem einzelnen günstigen Lauf ableiten.
