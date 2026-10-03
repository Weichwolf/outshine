Type: feature
State: open
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2280
Area: generators, world, render, engine
Tags: terrain, water, coastline, contacts

# Terrain and water form correct relief, levels and shores

## Ergebnis und Ist
Meer/Fluss/See als eigene animierte Geometrie mit kohärenten Pegeln/Ufern/Inseln;
Terrain mit richtigen Straßen-/Gebäudekontakten. Terrain-Stempel, WaterField,
WaterSurfaceBuilder/WaterDepth und dielektrisches Material bestehen. WaterField ist
noch Tile-/OsmField-gebunden; globale Körper/Pegelherkunft und Küstenabschluss fehlen.
Flensburgs Wasserfälle/überflutete Gebäude bleiben Geometriefehler.

## Besitzer und fehlender Vertrag
2280 liefert normalisierte Wasser-/Küstenringe und native Höhensamples mit Datum/NoData;
clipped Tiles garantieren keinen geschlossenen Gewässerkörper. OSM-Erweiterung rekonstruiert
Abdeckung/Identität; Wasser-/Terrain-Generatoren erzeugen Körper/Bett/Ufer. `world/ground`
hält native WaterBody/Terrain-Produkte, Renderer animiert/beleuchtet sie. Laying koordiniert.
Zuerst Flensburgs Küste/Pegelursache bis zum korrekten Bild lösen. Nachbar-/Abschluss-
vertrag offener Küsten vor Umsetzung festlegen; dieser offene Teil hält `planned`.

## Verfahren und Invarianten
- Raster-/Kamera-/Wasserhöhendatum konsistent umrechnen. DSM-Dächer/Bäume sind kein nackter
  Boden; NoData kein Nullboden. Städte/Hänge nicht pauschal glätten, Herkunft erhalten.
- Ringe/Höfe/Inseln und Komponenten über Tile-Ränder erhalten. Offene Ketten benötigen
  Nachbar-/Abschlussinformation; keine geratenen Küsten als vollständige Abdeckung melden.
- Erklärter Pegel je Körper: Meer im passenden Datum, Flussgefälle entlang Verlauf.
  Keine zufälligen Tile-Pegel oder Grundwassermesh unter jedem Terrain-Tile; Tide unbekannt.
- Körper/Bett/Ufer zusammen konstruieren, Terrain schneidet Wasser am Ufer. Stempel
  respektieren Wasser/erhöhte Bauwerke; Klassenmasken ersetzen keine Wassergeometrie.
- Endgültiges Kontaktrelief verfeinern, Nachbargrenzen abstimmen. Ring-/Punktlimits
  melden Nichtlieferbarkeit statt stillen Gewässerverlust; native Bounds/Fehler erhalten.
- Windwellen, Fresnel, Tiefenabsorption, Reflexion/Transmission und Schaum aus demselben
  Wasserprodukt. Wetter/Eis/Nässe aus 2172, Licht/Reflexionsintegration aus 2155;
  keine Doppelbelichtung oder unabhängige blaue Terrainklasse.

## Abnahme
Flensburg/Husum/Malcesine/Koerbersee: richtige Pegel, Ufer/Inseln/Kontakte, Wasser unter
Brücken und Gebäude auf Boden. Bewegung/Wellen verändern weder Abdeckung noch Pegel.
