Type: feature
State: open
Architecture: planned
Priority: P2
Parent: 2169
Depends: 2336
Area: generators, render, world
Tags: vegetation, forest, grass, seasons

# Vegetation ranges from distant forests to individual grass blades

## Ergebnis und Ist
Standortgerechter Wald/Stadtbaum/Strauch/Unterwuchs mit Dichte, Jahreszeit, Wind und Schatten.
TreeGrower/Mesher/Foliage, TreePrototype und ForestDraw bestehen; Weltpfad überwiegend
Kronenkarten, Nahlaub/Standorte/Residency fehlen. Ausbau nach Infrastruktur/Terrain/
Material/Licht/Wetter; keine festen Zeit-/Speicherquoten gegenüber Städten.

## Besitzer und fehlender Vertrag
2336 liefert gemeinsame Eltern-/Kind-, Fehler- und Residency-Auswahl. OSM-Adapter liefert
verfügbare Landcover-/Baumhinweise; Vegetationsgenerator besitzt Standort/Art/Form,
ForestDraw Instanzen. VegetationStreaming ist aktuelle Koordination und wird durch
allgemeinen Lebenszyklus aus 2188 ersetzt. Zuerst geeigneten Place mit Nah-/Mittelgeometrie
statt Kronenkarten verbessern. 2172 liefert gemeinsamen Wind/Zustand, 2136 spätere Kräfte.

## Verfahren und Invarianten
- Standort/Spezies/Alter/Dichte/Phänologie aus erlaubten Inputs und stabilen Welt-Seeds;
  unbelegte Spezies plausible Ergänzung, keine Ortsbepflanzung oder neue Welt je LOD.
- Fernwaldverband → Kronen/Einzelbäume → Äste/Blätter → Nahhalme; derselbe deterministische
  Formplan und konservative Silhouetten-/Fehlergrenzen. Gültigen Elternstand bis Kind halten.
- Prototypen/Material teilen, kompakte Instanzen statt Blattmesh pro Baum. Wind/Schatten/
  Jahreszustand raumverankert; spätere physikalische Biegung nutzt dieselben Identitäten.
- Atlanten/Impostors begrenzt nur RAM/GPU, kein Runtime-Diskcache. Terrainkontakt,
  Boden/DSM-Bewuchs unterscheiden; Wasser/Freiraum respektieren, Overdraw/Überdeckung begrenzen.

## Abnahme
Nahkrone/Grashalm, Wald am Horizont und Rundumdrehung vollständig ohne Form-/Artwechsel.
Sommer/Winter passend, kein Nahkarteneindruck oder Wachstum im Wasser; dasselbe gemeinsame
Zeit-/Speicherbudget wie eine Stadt. Standorte/Formverfahren noch integrieren, daher `planned`.
