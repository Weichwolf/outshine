Type: feature
State: open
Architecture: planned
Priority: P2
Parent: 2169
Depends: 2336
Area: world, generators, render, engine
Tags: vegetation, forest, grass, seasons

# Vegetation ranges from distant forests to individual grass blades

## Ergebnis und vorhandene Fähigkeit
Standortgerechte Wälder, Stadtbäume, Sträucher und Unterwuchs erzeugen plausible Dichte,
Silhouetten, Jahreszeiten, Wind und Schatten. TreeGrower/TreeMesher/TreeFoliage,
TreePrototype, ForestDraw und native Instanz-/Materialpfade existieren. Der Weltpfad
zeichnet überwiegend Kronenkarten; nahes Laub, Standorte und vollständige Residency fehlen.
Vegetation wird nach Infrastruktur, Terrain, Materialien und Licht/Wetter ausgebaut.

## Besitzer und nächste Lieferung
Original-OSM-Landcover/Baumdaten und Terrain/Wetter liefern Standortparameter.
Bestehende Generatoren/Prototypen wiederverwenden; VegetationStreaming koordiniert
begrenzte native Produkte und ForestDraw die Instanzen. Zuerst einen geeigneten Place
mit vorhandener Art und echter Nah-/Mittelgeometrie statt Kronenkarten verbessern.
Renderinstanzen, prozedurale Form und spätere physikalische Windbiegung teilen
Standort/Identität und 2172s Wind; 2136 besitzt die allgemeine physikalische Wirkung.
2336 muss den gemeinsamen Eltern-/Kind- und Fehler-/Residency-Vertrag bereitstellen;
fehlende Vertragsfähigkeit, nicht eine feste Klassenquote, ist der technische Blocker.

## Umsetzung und Invarianten
- Standort, Spezies, Alter/Dichte und Phänologie aus erlaubten Quellen und stabilen
  Welt-/Objekt-Seeds ableiten. Keine Orts-Sonderpflanzung oder neue Zufallswelt beim LOD-Wechsel.
- Fernwald als zusammengefasster Verband, danach Kronen/Einzelbäume, Äste/Blätter und
  bodennahe Halme. Produkte teilen deterministische Form und konservative Silhouetten-/Fehlergrenzen.
- Geometrie/Material je Prototyp teilen; kompakte Instanzen statt jedes Blatt pro Baum
  zu duplizieren. Raumverankerter Wind, Schatten und Jahreszustand bleiben kohärent.
- Atlant/Impostor-Produkte nur begrenzt im RAM/GPU halten, kein persistenter Generatorcache.
  Fehlende Detailstufe erhält gültigen Elternstand und meldet Qualitätslücke.
- Terrainkontakt und nackter Boden/DSM-Bewuchs getrennt behandeln. Gras und Unterwuchs
  besitzen Standort-/Überdeckungsgrenzen, stabile Filter und begrenzten Overdraw.

## Abnahme
Nahe Krone/Grashalm, Wald am Horizont und Kameradrehung bleiben vollständig und ohne
Form-/Spezieswechsel. Vegetationslast nutzt dasselbe Zeit-/Speicherbudget wie eine Stadt.
Winter-/Sommerbilder passen zum Zustand; keine sichtbaren Nahkarten oder Vegetation im Wasser.
