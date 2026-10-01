Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends:
Area: world, generators, render, engine
Tags: terrain, water, coastline, contacts

# Terrain and water form correct relief, levels and shores

## Ergebnis und vorhandene Fähigkeit
Terrain zeigt plausibles Relief mit sauberen Straßen-/Gebäudekontakten. Meer, Flüsse
und Seen sind eigene animierte Geometrie mit zusammenhängenden Pegeln, Ufern und Inseln.
GLO-30-Samples, Terrain-Stempel, WaterField, WaterSurfaceBuilder und dielektrische
Wasserflächen existieren. Native Küstenabschlüsse/übergreifende Pegel sind offen;
Flensburgs künstliche Wasserfälle und überflutete Gebäude bleiben reale Fehler.

## Besitzer und nächste Lieferung
`world/ground` hält finales Terrain sowie semantische WaterBody-Produkte mit Original-ID,
Außen-/Innenringen und Pegelherkunft. WaterField ist derzeit tile-/OsmField-gebunden und liefert Surface-Ringe ohne globale
WaterBody-Identität/Pegelherkunft. Diesen Vertrag ablösen. Native WaterSurfaceBuilder/WaterDepth liefern
Flächen/Tiefen; Laying koordiniert Carving, Material und Publikation.
Zuerst Flensburgs Originalküste und Pegel-/Terrainursache bis ins richtige Bild lösen.
Den Abschlussvertrag offener Küsten und benötigter Nachbarzellen vor Implementierung
festlegen; dessen ungeklärter Teil hält die Architektur `planned`.

## Umsetzung und Invarianten
- EGM2008, Kamerahöhen und lokale Wasserstände konsistent umrechnen. DSM-Dach-/Baumanteile
  behandeln, ohne Städte oder steile Hänge pauschal zu glätten. NoData erzeugt keine Falten.
- Original-Wasserpolygone/-multipolygone und gerichtete Küsten aus gepinnten Zellbeständen
  ableiten. Inseln und getrennte Komponenten erhalten. Offene Ketten brauchen explizite
  Nachbar-/Abschlussinformation; keine geratenen Land-/Wasserabschlüsse als fertige Welt.
- Ein Pegel pro zusammenhängendem Körper mit belegter/erklärter Herkunft. Meereshöhe im
  passenden Datum, Flussgefälle entlang des Verlaufs. Keine zufälligen Tile-Randpegel.
  Kein pauschales Grundwassermesh unter jedem Terrain-Tile.
- Körper/Bett/Ufer gemeinsam konstruieren; Terrain darf die Wasserfläche am Ufer schneiden.
  Straßen-/Gebäudestempel respektieren Wasser und erhöhte Bauwerke. Klassenmasken ersetzen
  keine Wassergeometrie; Materialgrenzen stimmen mit denselben Originalringen überein.
- Relief verfeinert die endgültige deformierte Oberfläche; Nachbargrenzen stimmen überein.
  Harte Ring-/Punktlimits melden Nichtlieferbarkeit, statt Gewässer still zu entfernen.
- Wasser nutzt Wind/Wetter, Normaldetail, Tiefe, Fresnel, Reflexion und Transmission;
  Energieaufteilung und Atmosphäre konsistent. Keine starre blaue Fläche oder Doppelbelichtung.

## Abnahme
Flensburg, Husum, Malcesine und Koerbersee zeigen richtige Pegel, Ufer, Inseln und
Terrainanschlüsse. Gebäude stehen auf tatsächlichem Boden, Wasser bleibt unter Brücken.
Wellen/Reflexion verändern Wasser, ohne seine Abdeckung/Pegel bei Bewegung zu verschieben.
