Type: feature
State: open
Architecture: planned
Priority: P2
Parent: 2169
Depends: 2336
Area: generators, render, world
Tags: vegetation, forest, grass, wind

# Vegetation scales from distant forest to nearby leaves and grass

## Ergebnis und Ist
Standortgerechte Bäume/Sträucher/Unterwuchs mit Dichte, Jahreszeit, Wind und Schatten.
TreeGrower/Mesher/Foliage, TreePrototype und ForestDraw bestehen; Weltpfad überwiegend
Kronenkarten, Nahlaub/Standortauswahl unvollständig. Ausbau nach Infrastruktur/Bildbasis.
Vorhandene deklarierte Vegetation bleibt Bestandteil vollständiger Places.

## Besitzer und fehlender Vertrag
Depends 2336: gemeinsame Eltern-/Kindabdeckung und budgetierte Detailauswahl mit Formfehler.
OSM-Adapter liefert Landcover-/Baumhinweise; Vegetationsgenerator Standort/Art/Form,
ForestDraw Instanzen. Zuerst Nah-/Mittelgeometrie eines geeigneten Place statt Kronenkarten.
Form-/Standortverfahren noch zu integrieren, daher `planned`. 2172 liefert später Umweltzustand.

## Verfahren
- Standort/Art/Alter/Dichte aus erlaubten Inputs und stabilen Welt-Seeds; unbelegte Spezies
  ist plausible Ergänzung. Wasser/Freiraum und DSM-Bewuchs respektieren, keine Ortsbepflanzung.
- Fernwaldverband → Kronen/Einzelbäume → Äste/Blätter → Nahhalme aus einem Formplan.
  Konservative Fehler/Bounds und Elternstand bis Kind-Publikation; keine neue Welt je LOD.
- Gemeinsame Prototypen/Materialien, kompakte Instanzen statt Blattmesh pro Baum.
  Prototypvorbereitung braucht keine fertigen Gebäude; Standorte benötigen finales Kontaktrelief.
- Wind/Schatten/Jahreszustand raumverankert; später physikalische Biegung aus 2136.
  Atlanten/Impostors begrenzt nur RAM/GPU. Overdraw begrenzen; Wald und Stadt nutzen dasselbe
  dynamische Budget ohne Klassenquote. Kein eigener persistenter Generatorcache.

## Abnahme
Wald am Horizont, Nahkrone und Grashalm ohne Art-/Formwechsel oder Nahkarteneindruck.
Sommer/Winter plausibel, nichts wächst im Wasser; Rundumdrehung bleibt vollständig im Budget.
