Type: feature
State: open
Architecture: ready
Priority: P2
Parent: 2169
Depends: 2336
Area: generators, render, world
Tags: vegetation, forest, grass, wind

# Complete forests scale from horizon stands to nearby leaves

## Ergebnis und Ist
SpeedTree ist der Qualitätsmaßstab: glaubwürdige Silhouetten, Verzweigung, Laub,
Wind und Schatten; vollständige Wälder teilen das Frame-/Speicherbudget mit der Stadt.
TreeGrower/Mesher/Foliage und vier Prototypstufen bestehen. Der Weltpfad nutzt überwiegend
Kronenkarten, ignoriert deklarierte Artenmischungen und bindet Waldflächen unvollständig an.
Native Struktur-/Wasser-/Wegflächen schließen Wurzeln bereits aus. Das bleibt erhalten.

## Besitzer und Datenfluss
OSM-Adapter besitzt Landcover-/Baumhinweise. VegetationTemplates besitzt deklarierte
Standortmischungen; Shipping löst Artennamen einmal in kompakte Prototypindizes auf.
Forest erzeugt einen stabilen Bestandsplan aus Bodenklasse, Höhe, Hang und Welt-Seed.
ForestDraw publiziert Instanzen. Vegetationsvorbereitung/Renderer besitzen gemeinsame
Prototypen, RAM-/GPU-Produkte, sichtbare Detailwahl, Wind und Schatten.
Depends 2336 betrifft ausschließlich gemeinsame Eltern-/Kindabdeckung und budgetierte
Formfehler. Standortauswahl und Prototypintegration warten nicht auf dessen Gesamtabschluss.
2172 liefert später Wetter/Jahreszustand, 2136 physikalische Biegung.

## Reihenfolge
Vegetation folgt nach Gebäuden, Terrain und Infrastruktur, vor Wolken (2169).
Der Qualitätsauftrag ändert diese Reihenfolge nicht.

## Implementierung in vollständigen Schritten
1. Deklarierte gewichtete Artenmischungen bis zur Runtime durchreichen. Unbekannte Arten
   und ungültige Gewichte am Konfigurationsrand ablehnen; kein uniformer Katalog-Fallback.
   Danach echte Waldflächen des registrierten Quellformats bis zur Bestandsplanung anbinden.
2. Ein Formplan je Art/Variante, gemeinsame Baumprototypen und kompakte Instanzen.
   Nahstufe mit Stamm/Ästen/Laub; Mittelstufe mit vereinfachten Ästen/Laubgruppen;
   Fernstufe mit Kronen; Horizontstufe mit räumlichen Waldverbänden statt Einzelbäumen.
   Distanz und erlaubter Bildschirmfehler bestimmen Aufwand vor Erzeugung.
3. Übergänge erhalten Form/Standorte und vollständige Rundumabdeckung. Räumlich gebündelte
   Sichtbarkeit/Draws, begrenzter Alpha-Overdraw und Schattenaufwand; keine Arbeit pro Blatt
   im CPU-Frame. Fernwald erzeugt keine Nahgeometrie und keine Einzelbaum-Draws.
4. Wind verformt geteilte Geometrie aus raumverankerten Parametern; Jahreszeit beeinflusst
   Laubmenge/Farbe. Unterwuchs/Gras nur bei sichtbarem Bildgewinn, im selben Qualitätsbudget.

## Forschungsgrundlage
[Plant Ecosystems, SIGGRAPH 1998](../doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf)
([Primärquelle/Einordnung](../doc/references/README.md)): Bestandsplan, Pflanzenform und
Darstellung trennen; Prototypen/Organe/Verbände teilen. Das Offline-Verfahren liefert keine
Echtzeitgarantie. Wind, Alpha-Overdraw und Schatten mit 2336s Auswahl budgetieren.
[Billboard Clouds](../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf)
als räumliche Fernprototypen gegen vereinfachte Kronengeometrie vergleichen; Alpha-Belegung,
Mips und Overdraw entscheiden zusammen mit GPU-/RAM-Bytes, nicht nur Dreieckzahlen.
[Echtzeit-Fernwald](../doc/references/vegetation/eg/2012-real-time-forests.pdf):
Bestandsdeckung, Kronenhöhe und mittlere Lichtantwort bei Aggregation erhalten.
Z-Felder/Shader-Maps sind Vergleichsmodelle; deren Texturkosten nicht blind übernehmen.
Nur eigene prozedurale Formen im RAM vorbereiten, keine persistenten Bake-Dateien.

## Invarianten und Abnahme
Keine Place-Sonderbepflanzung. Wasser/Freiraum und DSM-Bewuchs respektieren; plausible
Ergänzungen bleiben von belegten Arten unterscheidbar. Generierte Produkte nur RAM/GPU.
Wald, Stadt und Vegetation gemeinsam vollständig resident, keine feste Klassenquote.
Malcesine/Feldkirch zeigen geschlossene Fernbestände; ein naher Bestand zeigt räumliches
Laub statt Kronenkarten. Vorher/Nachher tatsächlich öffnen. Dichte Wald-Rundumdrehung
bei unverändertem Profil messen: p99, CPU/GPU, Peak-Speicher, Instanzen und Overdraw.
SpeedTree-Niveau ist erst mit Bild- und Laufzeitbelegen erreicht, nicht mit CPU-Tests.
