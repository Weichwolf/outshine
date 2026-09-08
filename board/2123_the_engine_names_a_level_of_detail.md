Type: debt
State: active
Area: world, render
Tags: webcam, measured
Depends:

# Every visual representation obeys a measured screen error

## IST / Umsetzung

Terrain hat bereits Fehlerselektion; Gebäude wählen Details teilweise beim Ingest. Das
ist noch keine gemeinsame kontinuierliche Qualitätsleiter für Straßen, Wasser, Bäume und
Gebäude. Einstieg `include/generate/Generate.h`, Unseen/Detail-Auswahl, GroundLattice,
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

Wahl: Cesium-artiger Fehlervertrag, Unreal-HLOD als Clusterkonzept; RAGE visuelle Distanzleiter.
Kein Hardware-Nanite-Versprechen, da SDL_GPU die verwendbaren Mechanismen vorgibt.


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
