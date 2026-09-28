Type: debt
State: active
Architecture: planned
Priority: P1
Area: world, render
Tags: webcam, measured
Parent: 2188
Depends:

# Every visual representation obeys a measured screen error

## IST / Umsetzung

Terrain hat bereits Fehlerselektion; Gebäude wählen Details teilweise beim Ingest. Das
ist noch keine gemeinsame kontinuierliche Qualitätsleiter für Straßen, Wasser, Bäume und
Gebäude. Einstieg `include/generation/Generate.h`, Unseen/Detail-Auswahl, GroundLattice,
Generator-Bakes und Render-Cluster. 2166 besitzt den finalen Terrainfehler.

Pro Renderprodukt geometrischen/visuellen Fehler und Bounds tragen. Auswahl nach projiziertem
Fehler mit konservativem Nahfall; Hysterese/Morph und stabiler Material-/Feature-ID. Proxies
asynchron vorbereiten, selektiv verfeinern. Makrosilhouette und wesentliche Öffnungen erhalten.
Wasser darf nicht pauschal zum konvexen Hull werden: Inseln/konkave Buchten blieben sonst falsch.
Straße/Schiene dürfen vereinfacht werden, ohne logisches Netz, Alignment oder Kontakt mitzunehmen.
Fernvegetation erhält Kronenvolumen und Coverage, nahe Blattgeometrie ein Overdrawbudget.

- [ ] 1/10/100/1000-m-Leiter plus Fernblick und Bewegung; Mesh-/Materialwechsel ohne
      wandernde Gebäude, geschlossene Unterführungen oder verdeckte Wasserinseln.
- [ ] Gegen unabhängige feine Referenz Fehler messen; forced-coarse verletzt Qualitätsoracle.
      Forced-fine ist Kostenmessung, nicht automatisch ein garantiert rotes Performanceoracle.
- [ ] 2092/2104 messen Gesamtzeit und Speicher; Graph-/Kontaktgrößen unabhängig ausweisen.
      Bestehende Speicherobergrenzen bleiben erhalten, keine unbegründete Anhebung.

## Architekturentscheidung

Ein natives Geometriemodell, residente Grobrepräsentation und renderer-eigene Auswahl
nach konservativ projiziertem Fehler. Generatoren erzeugen quellenstabile Produkte;
Renderer entscheidet pro Frame mit Hysterese und begrenzten Uploads. Feines Detail erst
nach vollständigem Nachweis; unbekannte Schranke erhält den konservativen Fallback.

Kein verpflichtender Visibility Buffer oder virtualisierter Clusterbaum für den nächsten
LOD-Schritt. Vorhandenes flaches Cluster-Cooking und Hardware-Rasterisierung weiterverwenden.
Der ungenutzte CookDag-Prototyp ist kein fertiger Hierarchiepfad. Eine vereinfachende
Hierarchie oder Materialauswertung nach Sichtbarkeit braucht gemessenen Fehlernutzen,
einen eigenen ausführbaren WI und SDL_GPU-konforme Kosten-/Überlaufverträge.

Gebäude: 2313 → 2312 → 2298. Wald: 2111 nutzt geteilte native Prototypen und eigene
Coverage-Leiter. Terrain besitzt finale Form-/Fehlerzertifikate in 2166. Wasser und
Straßen bewahren Inseln, Öffnungen und logisches Netz. Diese Pfade benötigen keine
vorher fertige universelle Hierarchie. Alpha-Coverage, Beleuchtung und volumetrischer
Fehler sind keine durch Dreiecksabstand bewiesenen Geometriefehler; getrennt messen.
2314 verteilt später Kosten anhand sichtbaren Beitrags über alle Familien einschließlich
Himmel/Wolken. Kontakt/Navigation und deren Residency bleiben davon unabhängig.

Gebäude-LOD darf nicht vom Kameraort beim Bake abhängen. Hockenheim bei 74,85 s
zeigt trotz identischer Endkamera nach `Refined` und Settle 5.679 abweichende
Pixel in 43 Horizontzeilen: akzeptierte Tiles tragen verschiedene `RawTile::Eye`-
Detailentscheidungen innerhalb der 64-m-Wiederverwendung. Erzeuge pro Quellrevision
stabile native Detailprodukte/Proxies mit Fehler und Bounds; der Renderer wählt
pro Frame nach projiziertem Fehler mit Hysterese. Fehlende feine Produkte halten
den residenten Proxy und melden dessen Qualitätsgrenze, ohne Neubake alle 64 m.
Gegenprobe: dieselbe Endkamera nach Sprung und Fahrt ergibt dieselbe Geometrie,
Silhouette und PNG; schnelle Fahrt bleibt innerhalb CPU/GPU- und Uploadbudgets.

Referenz: [Epic, Nanite Deep Dive](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf).
[SDL-GPU-Indirektdraws](https://wiki.libsdl.org/SDL3/SDL_DrawGPUIndexedPrimitivesIndirect).
Abnahme: Kamera-Nahfälle, Grenzrisse, schnelle Bewegung, fehlende Seiten und Queue-Überlauf;
Bildfehler gegen feine Referenz sowie CPU/GPU-p95/p99, Rasterlast und Speicher messen.


## Vorhandene Verträge und offene Grenze

TreeMesher erhält Eltern sichtbarer Äste und begrenzt Astvereinfachung nach Reach
und projizierter Dicke. Alle Ranks erhalten Blattansätze und physisches Blattmaß.
TreeLeaf begrenzt Abstand und einseitige Blattflächenabweichung auf 2 % [SET].
Intervallbudget: 0,02 × Gesamtfläche × k/n; disjunkte Intervalle partitionieren n.
Die Dreiecksungleichung begrenzt die Summenabweichung. Keine Skalierung zum Kaschieren.

TreeLeafSimplificationBoundsTheSurface und TreeGeometryUsesNativeMaterials prüfen
exportierte Dreiecksflächen unabhängig von der Auswahl. Entfernte Flächenschranke
verletzt beide Oracles bei weiterhin erfüllter Abstandsschranke. Historie hält Belege.
Flächensumme und XYZ-Summenprojektionen sind keine sichtbare Projektionsunion.

Noch umzusetzen: kollektive Ast-/Blatt-Coverage, räumliche Aggregation, tatsächliche
Kameraauswahl mit Hysterese, stabile Übergänge unter Bewegung und wechselndem Licht.
Nahgeometrie, Mittelrepräsentation und Fernkronen teilen GPU-Prototypen; keine
expandierten Blattnetze pro Weltinstanz. Die bisher dunkle, feinkörnige Fernkrone
ist visuell nicht abgenommen. Waldintegration folgt 2111/2169, nicht weiterer
isolierter Konturarbeit. Material und Kronenlicht gehören zu 2171/2167.
