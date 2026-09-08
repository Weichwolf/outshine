Type: bug
State: open
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2121, 2173

# Water bodies have coherent levels, valid surfaces and constructed banks

## IST

Die frühere pauschale Diagnose „kein Deckel“ ist nicht mehr haltbar. `Laying.cpp` erzeugt
Wasserflächen; im vorigen Diagnosebestand Husum 220 Flächen/2998 Dreiecke, Malcesine
58/1874. Der genutzte Fan-Pfad ist gegen konkave Polygone/Löcher zu prüfen; ein vorhandener
Earclip-Helfer beweist nicht dessen Verwendung. Aktuelle Bilder zeigen dunkle Wasserflächen,
gezahnte/geböschte Ufer und in Husum durchquerende helle Bänder.

## Implementierung

- Ein WaterBody-Modell für Geometrie, Niveau/Datum, Outer-/Inner-Ringe, Bed und Bank.
  Polygon-Clipping/Triangulation für konkave Multipolygone und Inseln; Tilegrenzen teilen IDs.
- Niveau für See zusammenhängend; Fluss längs stetig mit plausibler Falllinie, Meer mit
  deklariertem Referenzniveau. Zeitabhängige Pegel nur aus vorhandenen Daten oder ausdrücklich
  simuliert, niemals als exakter beobachteter Wasserstand behaupten.
- OSM-Quai/Stützmauer als Wand mit Oberkante, Fundament und Material; natürliche Böschung
  separat. Basin-Press-Apron nicht pauschal zum sichtbaren Ufer machen. Höhenänderungen begrenzen
  und mit Quelle protokollieren; gültigen Berg nicht an den See-Level ziehen.
- Derselbe ausgeschnittene Wasserkörper beliefert Bed, Surface und 2129; Unterschiede nach
  Ablehnungsgrund zählen. Wasser unter Brücken erhalten; Straße nicht auf Wasserniveau pressen.

## Abnahme

- [ ] Konkaver See mit Insel, Fluss über Tilegrenze, Hafen mit Brücke: keine Landüberdeckung,
      fehlende Surface oder Höhensprünge. Absichtlich falscher Ring erzeugt lokalen roten Befund.
- [ ] Husum ohne Treppen/Bänder im Wasser; Malcesine ohne künstlichen Uferkamm;
      Koerbersee hat eine durchgehende Oberfläche. Venice-Regression zusätzlich erhalten.
- [ ] Water-ID/Bed/Surface-Counter und Querschnitte erklären jeden Unterschied. Reflexion
      separat in 2129 abnehmen; geometrische Korrektheit nicht aus dunkler Farbe ableiten.

Wahl: getrennte Wasseroberfläche und Geländeform wie öffentliche Unreal-Water-Konzepte;
RAGE ist visuelle Referenz. Ein Deckel allein behebt keine falsche Uferkonstruktion.
