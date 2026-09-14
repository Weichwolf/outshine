Type: bug
State: active
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2121, 2173

# Water bodies have coherent levels, valid surfaces and constructed banks

## IST

Die frühere pauschale Diagnose „kein Deckel“ ist nicht mehr haltbar. `Laying.cpp` erzeugt
Wasserflächen; im vorigen Diagnosebestand Husum 220 Flächen/2998 Dreiecke, Malcesine
58/1874. Der genutzte Fan-Pfad ist gegen konkave Polygone/Löcher zu prüfen; der unbenutzte
Earclip-Nebenpfad wurde entfernt. Aktuelle Bilder zeigen dunkle Wasserflächen,
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

## P0: Wasseraufnahme in prüfbare Phasen trennen

WaterField nutzt gemeinsame Ringauswahl und Höhenleser; Flussprofile und
Flächenpegel sind getrennt. Bestehende Filter, Pegelheuristik und Reihenfolge
bleiben erhalten. Deklarierte Ringe prüfen Pending→Ready, fehlende Höhen,
Tunnel-/Größen-/Layerfilter, beide Flussrichtungen und niedrigen Flächenpegel
samt Ausreißer. Test grün; ausgeschaltete Pending-Sperre scheitert siebenmal.
Wien ohne Vegetation geöffnet: 0/921600 Pixel verändert; p50/p95/p99
5.14/5.79/6.14 ms, 0/120 über 16.67 ms. Keine vollständige Wasserabnahme.
Aufnahme/Bereitschaft ohne Diagnose. Aktive Flächengenerierung, Löcher und
Writer-Coverage bleiben offen.
Die doppelte Abfrage vor/nach Mark_.Take setzt derzeit stabile GroundQuery-
Antworten voraus. Übergang Ready→Pending und atomare Veröffentlichung separat
prüfen; die Aufteilung allein beweist diesen Lebensdauervertrag nicht.

## P0: ein Datenmodell, ein aktiver Geometriepfad

Quell-/Test-/Header-Audit: WaterField::Tessellate hatte keinen Aufrufer. Der aktive
Pfad in Engine::State::Grounds baut weiterhin einen Fan in native Geometry.
Der tote Earclip-/Flussstreifenpfad mit abweichend interleavten ECEF-Daten ist
entfernt, ebenso sein exklusiver Anchor-Zustand und der GroundStack-Setup-Aufruf.
WaterField hält geografische Wasserdaten und Pegel. Build und unveränderte
Aufnahmeprüfungen grün; Wien geöffnet und pixelgleich (0/921600). Lint: 57 Befunde,
keiner in WaterField; Writer weiter rot. Damit ist kein aktiver Geometriefehler
behoben: Fan durch gemeinsamen Polygon-Generator mit validierten Außen-/Innenringen
und vollständigem Ergebnis/Fehler ersetzen. Wasser-, Bed- und Bank-Verträge oben gelten.
