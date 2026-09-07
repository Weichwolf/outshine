Type: feature
State: active
Area: generators
Tags: webcam, measured
Depends: 2123

# Forests and urban trees populate suitable ground

## Aktueller Beleg

Die neun Render zeigen praktisch keine lesbare Baumvegetation, besonders auffällig in
Koerbersee, Wien, Olympiaturm und Feldkirch. Frühere Logs meldeten `flora placed 0`;
die damals vermutete Cover-/Region-Frame-Differenz ist keine neu bewiesene Ursache.

## Implementierung

1. `src/generators/flora/Forest.cpp`, Asking/Yield und ForestDraw: Kandidaten, Ablehnungsgründe,
   platzierte Instanzen, hochgeladene Instanzen, sichtbare Draws getrennt zählen. `noTemplate`,
   Dichte, Neigung, Treeline, Kapazität und Frame-Projektion bis zur GPU verfolgen.
2. Ursache dort reparieren, wo Zahl erstmals falsch wird; Unit-/Region-Frame nicht neu
   erraten. Positive Placement-Fixture und falsches Frame als Negativkontrolle.
3. OSM forest/wood/tree/tree_row/park plus Höhe/Neigung/Feuchte in plausible Bestände
   übersetzen; Artenmischung/Kronenform/Dichte aus deklarierten regionalen Verteilungen.
   Gebäude/Wege/Wasser aussparen, Waldkante unregelmäßig, keine Kopie einzelner Fotobäume.
4. Weltkoordinaten-Seed, geteilte Prototypen/Instancing, Nahgeometrie/Fernkronen mit LOD;
   Alpha-Cutout, Blatttransmission, Normalen und Shadows mit 2171/2128 integrieren.
   Wind und saisonale Änderung aus 2172 später konsistent einspeisen.

- [ ] Positive Counts bis zum Draw und sichtbare Baumkronen an den vier Referenzen.
- [ ] Platzierungsregeln über Tilegrenzen, Rückkehr und anderer Blickrichtung stabil;
      offenes Wasser/Straße bleiben frei. Keine stillen Abbrüche bei voller Kapazität.
- [ ] Overdraw, Schatten, Instanzen/Bytes und Frame-p99 nach 2092 mit dichter Vegetation.

Wahl: prozedurale Foliage/Instancing wie öffentliche Unreal-Konzepte; RAGE ist visueller
Dichte-/Distanzbenchmark. Die aktuelle leere Welt wird nicht durch manuell gesetzte Bäume repariert.
