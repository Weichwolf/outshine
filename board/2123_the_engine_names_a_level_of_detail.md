Type: debt
State: open
Area: world, render
Tags: webcam, measured
Depends: nothing

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

## Konkreter Baum-LOD-Befund

Nativer Birkenrender nach 2177: selbst Rank 3 hält 272934 Rinden- und 80640 Blattdreiecke.
TreeMesher::Draw entscheidet Astpräsenz nach Shoot.Reach, also Reichweite statt projizierter
Dicke/Abweichung; viele subpixelbreite Äste bleiben. TreePrototype reduziert Blattanker durch
stride = 16 << (2*rank) und vergrößert Blattfächer über LAI-Erhaltung. Im PNG bleiben dadurch
wenige große Büschel. Erforderlich: konservativer Silhouetten-/Coveragefehler, räumliche
Aggregation statt bloßem Index-Stride, gefilterte Ferndarstellung ohne riesige Einzelblätter.
Messartefakt build/tree-native/birch.png; Aufbaukosten/Overdraw plus Waldinstanzkosten separat.
