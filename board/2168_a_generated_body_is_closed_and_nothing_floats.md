Type: bug
State: open
Area: generators, render
Tags: webcam, measured
Depends: 2121, 2173

# Generated solids close correctly and contact their intended support

## IST / Diagnose

Aktuelle Stadtbilder zeigen punktierte Trauflinien; Graz/Feldkirch auffällige Gelände-
Gebäude-Kontakte. Sichtbare dunkle Pixel beweisen weder Loch noch invertierte Fläche.
Depth, Primitive-/Face-ID, unbeleuchtete Farbe und Drahtgitter trennen Topologie,
Z-Fighting, Beleuchtung und fehlende Flächen. Vorige kategorische Pixelursachen gelten
als Hypothesen, bis ein Geometrieoracle sie bestätigt.

## Umsetzung und Abnahme

- `src/generators/building/StructureBake.*`/Structures, Straßenmesher: gemeinsame
  geometrische Randpositionen für Dach/Wand, korrekte Innenhöfe/Valleys, keine schwebenden
  Sockel. Vertex-Splits für harte Normalen/UV sind zulässig; Topologie auf geometrischem
  Weld mit skalenbegründeter Toleranz prüfen, nicht Renderindex-Gleichheit erzwingen.
- Geschlossene Körper: Rand-/Nonmanifold-Kanten, Degeneration, Selbstschnitt und orientiertes
  Volumen prüfen. Centroid→Face ist für konkave Körper kein allgemeines Außenoracle!
  Stattdessen orientierte Topologie plus unabhängige Innen/Außen-Ray-/Winding-Prüfung.
  Offene deklarierte Surfaces separat behandeln, nicht künstlich verschließen.
- Kontakt gegen den richtigen Träger prüfen: Fundament/Terrain, Brückenauflager,
  Dachaufbau/Dach. Dem Terrain fernzubleiben ist bei einer Brücke korrekt.
- [ ] Entfernte Dachfläche, invertierte Komponente, sich schneidender Dachkörper und
      angehobenes Fundament machen jeweils das zuständige Oracle rot.
- [ ] Alle neun Places plus vorhandene OldTown/ZurichPlan-Fälle visuell prüfen; keine
      Traufspalten oder unerklärte Kontaktfehler. 2144 besitzt Terrain-Nähte, kein Doppelauftrag.

Wahl: allgemeine Solid-/Surface-Topologie statt unbewiesener Unreal/RAGE-Importbehauptung.
Visueller Benchmark bleibt die lesbare bauliche Konstruktion im Foto.
