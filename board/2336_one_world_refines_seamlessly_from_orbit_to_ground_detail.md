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
Weltbedarf folgt möglicher Bild-/Spielwirkung, nicht einer vollständigen feinen Radiusfüllung.
Bis 240 km am Boden und später Orbit → Nahdetail; Rundum-Abdeckung bedeutet verfügbare
Eltern, nicht alle feinen Daten/Assets. Verdeckte Bereiche dürfen dauerhaft grob bleiben.
Heute fragt GroundPatchwork fein → grob; Playable fragt feinste Kontaktkachel und grobe Basis
zugleich. TerrainPathPreparation plant alle Stufen; TerrainSourceCoverage fordert feinste
Höhen unter Gebäuden unabhängig von Verdeckung. Assettreffer kommen noch nach Quellarbeit.
Koerbersee-Diagnose: 16.255 Höhenblätter × 33² = 17.701.695 CPU-Vertices;
16.255 × 32² × 2 = 33.290.240 CPU-Dreiecke. Arrays: (53.105.085 + 99.870.720) × 4
= 583,56 MiB. Das sind CPU-Zwischenprodukte, keine gezeichneten Dreiecke. Der vorherige
erfolgreiche Shot meldet 37.776 erzeugte Gebäudedreiecke laut ROW. ROW zählt keine
GPU-Terraindreiecke; ein Verhältnis zu sichtbaren GPU-Dreiecken ist daraus nicht ableitbar.
TerrainSurvey erzeugt Lichtproben ohne Topologie; Positionen/Extrema erhalten.
Der finale globale CPU-Meshaufbau ist entfernt. TerrainSurvey erhält Lichtproben/Extrema;
Audio liest publizierte, deformierte Höhen direkt, maximal 64 Proben ohne Geometrieallokation.
Grobe Audioverdeckung; feine Quellen bleiben teuer. Koers Peak-Footprint fällt von 3,40 auf 1,83 GiB
(Bytes/1024³), Laden von 8,61 auf 7,88 s; Stadt-Laden bleibt bei 12–16 s. Quellenarbeit ist weiter offen.

## P0: grobe Sichtbarkeit vor feineren API-Anfragen
1. Zuerst gröbste Eltern für den Weltbedarf aus dem Assetcache; bei Miss nur grobes DEM
   beschaffen. Kein Start aller feinen Requests, auch nicht durch BlockAt, Gebäudehöhen,
   Wasserklassifikation oder vorbereitete Kamerapfade. Unbekannt/NoData bleibt unterscheidbar.
   Regionale Kontaktplanung folgt 2281; grobe DEM stützen den Entwurf, benötigte Höhen ergänzen ihn.
   Feines Relief wird unter festgelegten Kontakten geformt; keine globale serielle Baufolge.
2. Auf einem Worker grobe Geländehierarchie und Rundum-Horizont von nah nach fern auswerten.
   Höhenwinkel/Bounds berücksichtigen Kamera-ECEF, Erdkrümmung und Höhe. Unterbäume hinter
   Bergmassiven nicht verfeinern. Quadtree-Horizont gegen niedrig aufgelöste Tiefen-/Cubemap-
   Auswahl mit echten Koerbersee-DEM-Daten in Python vergleichen; keine feinen Daten vorladen.
3. Nur möglicherweise sichtbare Knoten nach projiziertem Formfehler/Pixelbedarf verfeinern;
   Eltern bleiben bis Kinder vollständig sind. Verdeckung erneut nach jeder groben Lieferung
   bewerten, dann Assetindex/Provider anfragen. Geländeschatten, Reflexion, hohe Bauwerke und
   aktive Interaktion haben eigene Bedarfe; Sichtbarkeit ist mehr als aktueller Farbbild-Frustum.
4. Gemittelte DEM-Mips liefern keine garantierten Höhenintervalle. Herkunft/Zustand belegt keine
   Höhenfehlerschranke. Unbekannte Bounds sperren keinen ganzen Ast: grobe Eltern erhalten,
   unsichere Randknoten priorisiert nachladen; bekannte Intervalle für sicheren Ausschluss nutzen.
   Approximationen mit Silhouetten-/Disocclusionvergleich bewerten, keine unsichtbaren Löcher.
5. Blickdrehung nutzt Rundumbedarf. Bewegung/Flughöhe öffnen neu sichtbare Äste frühzeitig,
   mit Hysterese/Prädiktion und begrenztem parallelem IO; kein World-Reset bei Grenzübertritt.
   Im Alpental hinter dem Berg erst Detail laden, wenn ein Bedarf entsteht; Radius allein reicht nicht.
6. Native Kontakt-/Audioabfragen nutzen finale Höhenblätter/räumlichen Index, lokale Dreiecke
   nur entlang benötigter Segmente; kein feines Weltmesh auf Vorrat. Audioarbeit/Stimmen begrenzen,
   [GitHub-Verfahren und Cubemap-Abgrenzung](../doc/references/audio/bounded-spatial-audio.md); Hallausbau bleibt 2136.

## Gebäude: Nachfrage von nah nach fern
Auch OSM wird vor Requests ausgewählt: zuerst benötigte nahe Zellen/grobe Verbände, deren
native Depth/Coverage auswerten, dann möglicherweise sichtbare Kinder. Keine vollständige
feine Stadtkachel-Ringfüllung vor Verdeckung. Bewegung/Pose prädizieren den nächsten Bedarf;
Drehung allein startet keine feine Weltfüllung. Gebäude-Bounds beachten Höhe und Dach/Silhouette.
Dichte Städte: vordere Flächen verdecken viele Einzelhäuser; hintere Rohlinge bleiben auf SSD.
Altstadt: weniger Häuser, aber komplexere nahe Formen/Sichtlücken. Beide teilen Bild-/Zeitbudget;
ähnliche Arbeit bei ähnlicher Bildkomplexität ist eine Hypothese, keine garantierte Gleichheit.
MVT liefert ganze Kacheln: versteckte Objekte in benötigten Kacheln sind unvermeidbare Quellbytes,
aber kein Auftrag für ihren Fine-Aufbau. Grobe MVT-Stufen ohne Gebäude sind keine leeren Städte.
Verdecker brauchen tatsächlich belegte Coverage; eine Cluster-AABB ist keine massive Hauswand.

## Raumhierarchie und Cachevertrag
2280 besitzt Index/Bytes; dieser WI besitzt Bedarf/Eltern-Kind-Auswahl. Generatoren besitzen
Fachpläne; Engine koordiniert native Bounds/Qualität/Kosten ohne OSM-Semantik (2188).
Stabile räumliche Zellen/Produktstufen/Versionen statt Kameraposition als Rohling-ID.
Kamera/Projektion wählen Produkte und Runtime-Details; unveränderte Rohlinge wiederverwenden.
Kachelzulassung nutzt native Metadaten; [Modell](../test/experiments/native_tile_admission.py) prüft reale Stückzahlen. Quellen erst bei Miss öffnen; gepackte Bereiche/Morton-Ordnung erhalten.
Nur erforderliche Produkte erzeugen; SSD/RAM/GPU begrenzen.
Laibungstiefe nur expandieren, wenn `FocalPx × 0,16 m / max(d − 64 m, 1 m)` das
Pixelbudget überschreitet. 64 m ist der Wiederverwendungsradius; Form-/Shell-Grenzen bleiben gleich.

## Fernstadt vor weiteren Nahdetails
| Stufe | Produkt | Bedarfsentscheidung |
|---|---|---|
| Fine | Räumliche Fassaden/Dächer | Nahe Form, Interaktion, aktuelle Schatten |
| Shell | Grundrisswände/Dach | Projizierter Formfehler rechtfertigt Hülle |
| Massed | Blockverbände | Silhouette/Coverage ohne Einzelhausgeometrie |
| Skyline | Tiefenhaltige Impostorflächen | Fernbild/Parallaxe statt Einzelvolumen |
Zuerst Massed/Skyline. Ferne Kosten nehmen ab; Kontakte/Straßen/Parts/Höfe nahe erhalten.
`PreparedStructureTile::Surfaces` enthält angereicherte Formen, keine Fine-Dreiecksmeshes.
Ganze Formen/Koordinaten vor Bedarf dekodieren ist der Fehler; fertige Formen nicht erneut erzeugen.
Wien/CP/Tokyo: 1,61/1,24/2,36 Mio. Gebäudedreiecke (ROW, keine GPU-Zählung); verdeckte/ferne Einzelobjekte vor Planung und Emission sparen.
[Python-Flächenmodell](../test/experiments/building_surface.py): Tokyo 588.862 Pläne → 8.782
Flächendreiecke im flachen Modell; kein nativer/GPU-/Bildnachweis. Generator-/Renderer-/Hybrid-
Verdeckung mit identischen Inputs, 360° und Bewegung vergleichen; frühere Bilder nicht löschen.
2336 wählt kompakte Zellbounds/Fernprodukte; 2280 lädt unabhängige Form-/Kontaktblöcke erst danach.
Unbeleuchtete Tiefe/Coverage, Normalen und Materialien wiederverwenden; Licht/Pose bleiben aktuell.
Brücken/Überhänge/Kronen brauchen mehrschichtige oder passende native Produkte.

## Reihenfolge und fehlende Verträge
P0: Gebäudebedarf vor Decode (2280), dann grober DEM-Bootstrap/Sichtbedarf vor Requests
einschließlich Nebenpfaden, räumliche Cacheprodukte und aktive Kontakte; Straßen-/Wasserqualität erhalten.
2339 zählt angeforderte/unterdrückte Kinder, Bytes/Stufe und Besitzer der Peaks im selben Ausbau.
Danach Fernstadt und Lade-/Uploadspitzen; neue Gebäudequalität folgt dem integrierten Gewinn.
Fehlend: gemeinsame native Hierarchieknoten mit Bounds/Höhenunsicherheit, Produkt-/Kostenbezug
und getrenntem Bild-/Interaktionsbedarf; dieselbe Auswahl für Builtins und externe Erweiterungen.
Keine neue SDK-Gesamtarchitektur vor der laufenden Koerbersee-Integration.

## Forschungsgrundlage
[MapLibre/Cesium/GigaVoxels](../doc/references/engine/visibility-driven-demand.md):
geprüfte Auswahl-/Fallbackpfade; GigaVoxels-PDF lokal. Kein Tokyo-Durchsatzbeweis dieser Projekte.
[Clipmaps](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf),
[GPU-Driven](../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
[Billboard Clouds](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf).
[Engine-Recherche](../doc/references/README.md): Hierarchical Image Caching, Layered Depth Images,
Far Voxels, Morton/BVH; räumliche Wiederverwendung statt ungeprüfter fertiger Beleuchtung.
Clipmaps allein liefern keine Quell-Sichtbarkeitsauswahl; CPU-Auswahl spart bereits vor GPU-Culling.

## Abnahme
Koerbersee: erster DEM-Requestbatch nur grobe Eltern; verdeckte Kinder weder API/Decode/Erzeugung
noch GPU-Residency. Aufstieg/Bewegung erschließen sie ohne Löcher; gleiche Rundum-Silhouette.
Tokyo/Wien/CP: nahe OSM-Batches vor fernen; verdeckte Kinder nicht anfordern/aufbauen.
Gleicher Inhalt/Profil bei weniger Daten/Geometrie/Peak; Schatten/Reflexionen erhalten.
Anfragen/Bytes pro Stufe und tatsächlich wirksame Geometrie belegen; Quellobjektzahl ist kein Budget.
Kalte API-Beschaffung, native Treffer und Runtime getrennt; datierte Bilder öffnen/vergleichen.
