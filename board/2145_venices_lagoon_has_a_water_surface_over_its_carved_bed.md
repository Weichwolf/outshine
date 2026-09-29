Type: bug
State: active
Architecture: planned
Priority: P1
Parent: 2169
Area: world, render
Tags: webcam, measured
Depends: 2121, 2173

# Water bodies have coherent levels, valid surfaces and constructed banks

## IST

Wasserflächen existieren. Der frühere Fan-Pfad überdeckte aber konkave Einbuchtungen und
Inseln. Der Polygonpfad ist jetzt aktiv und dieselbe `WaterField::Surface::LevelM` versorgt
logische Fläche, Bodenform und Renderoberfläche. Aktuelle Bilder zeigen weiterhin dunkle
Wasserflächen, gezahnte/geböschte Ufer und in Husum durchquerende helle Bänder.

## Implementierung

- Besitzgrenze: `WaterField` hält geografische Körper, Ringe und Pegel. Ein
  `Generators::WaterSurfaceBuilder` erhält einen unveränderlichen, für die
  Kandidatenrevision gepinnten Input aus Ringen, Pegeln, Höhen und TangentFrame
  und liefert native `Geometry` samt Zählern/Fehlern. `Engine::State` plant,
  veröffentlicht und meldet nur; dort entstehen keine Dreiecke. Basin-Stamps
  werden vom selben validierten Wasserkörper abgeleitet, nicht aus einem zweiten
  unabhängigen Ring-Walk. Keine `Engine::State`- oder Renderer-Abhängigkeit im
  Generator. Methoden heißen nach `Build`, `Advance`, `Take` und `Cancel` statt
  nach einem allgemeinen `Grounds`-/`Laying`-Vorgang.
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
  2257 ergänzt geschützte Gerinne auch für `WaterField::Course`, nicht nur Polygonflächen;
  Konflikte mit Pads/Korridoren werden konstruktiv gelöst oder abgelehnt.

## Abnahme

- [ ] Konvexer See reproduziert bestehende Geometrie; konkaver Ring mit Insel
      beweist Innen-/Außenfläche und Winding analytisch. Offener, degenerierter
      oder selbstschneidender Ring liefert einen lokalen Fehler mit Body-ID.
      Unterbrochene Arbeit und Retry ergeben identische Indizes und Pegel; ein
      späterer Kandidat ändert den gepinnten Input nicht rückwirkend.
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

Quellaudit 2026-09-23: `OsmVector`/`OsmField` erhalten Außen- und Innenringe
mit `Feature.FirstRing/RingCount` und `Ring.Exterior`. `WaterField::UsableRing`
verwirft dagegen jeden Innenring; `WaterField::Surface` enthält nur einen
Außenring. Der aktive Fan kann Inseln somit auch mit einem besseren
Triangulator nicht sehen. Zuerst `WaterField` auf einen pro Feature/äußerem
Teil zusammengehörigen Body mit geordneten Ringreferenzen, Pegel und
Quellrevision umstellen. Ein ungültiger Innenring verwirft den betroffenen
Body mit Diagnose, statt ihn still als Wasser zu füllen. Pegel aus dem
zugehörigen Außenring ableiten; Innenringe benötigen keine eigene Pegelprobe.
Paced Admission, Retry und Revisionswechsel dürfen keinen Teil-Body
veröffentlichen. Der bestehende MVT-Ringtest liefert ein Loch-Fixture für
einen neuen End-to-End-WaterField-Test. Lokale Triangulationsreferenz:
`/Users/cosmo/Git/earcut.hpp` bei `f25bc76` (ISC); vor Übernahme Lizenz,
Degeneratfälle und Arbeitsbudget prüfen.

## Vorhandener Polygonpfad und offene Lieferung

WaterField und GroundSnapshot halten Außen-/Innenringe und einen gemeinsamen Pegel.
AppendWaterSurfaceGeometry trianguliert konkave Polygone mit Löchern in native Geometry;
der Basin-Stamp erhält Inseln. Earcut f25bc76/ISC ist gepinnt. Der alte Fan ist ersetzt.
Offen bleiben selbstschneidende Ringe, Tilegrenzen, Flussprofile, Ufer/Quai und geschützte
Gerinne. Malcesines künstliche Steilwände und dunkler Fußstreifen bleiben sichtbare Fehler.

## Archivziel

Koerbersee braucht lesbare Berg-/Waldreflexion, Malcesine ruhige Fernflächen und feine
windabhängige Wellen, Husum einen sauberen Kai-/Wasserkontakt. Gemeinsame Wasserparameter
mit physikalisch unterschiedlichen Zuständen statt einer globalen blauen Fläche.
Der genaue historische Tidenstand ist aus OSM und Momentanwetter nicht ableitbar;
fehlende Pegeldaten bleiben unbewiesen, keine Foto-basierte Höhenkorrektur.
