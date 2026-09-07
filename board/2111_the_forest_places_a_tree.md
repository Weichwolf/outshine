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

## Diagnose auf dev/codex, 2026-09-07

`make shots PLACE='--measures Koerbersee'`, `build/forest-diagnosis.log`, Exit 0.
Asking veröffentlicht jetzt sämtliche vorhandenen Yield-Notes und Full-Claims.
Koerbersee-53208246.png geöffnet: weiterhin keine lesbaren Baumkronen; p99 7,26 ms,
0/120 Standframes über 16,67 ms. Digestabweichung trotz rein diagnostischer Änderung
unterstreicht 2154; sie ist kein visueller Fortschritt.

| gemessen | Wert |
|---|---:|
| Gebäude platziert | 11 |
| Flora platziert | 4085 |
| Region gesamt | 11 + 4085 = 4096 |
| Flora Full-Claims | 1 |
| erzeugte Draw-Instanzen, alle Generatoren | 4096 |
| Flora noTemplate / zeroDensity / densityDraw | 516 / 29461 / 127462 |
| Flora noSpecies / aboveTreeline | 0 / 0 |

Damit ist die frühere pauschale Null-Platzierungsdiagnose für diesen aktuellen Ort widerlegt.
Quellpfad geprüft: `Asking.cpp` schreibt `World.Instances`; eine Suche über `src/` findet
keinen Übergabe-/Renderer-Leser dieses Vektors. `World.Instanced` zählt lediglich dessen Größe.
`Shipping::Stands` liest Arten, erzeugt aber nur Stem-Höhen und `ForestDraw(ClusterId{0},
stems.front().HeightM)`; kein TreePrototype-Mesh wird dort gebaut/registriert. Ein blindes
Weiterreichen dieser Cluster-ID wäre falsch. Species-Identität muss bis zur Instanz erhalten
bleiben. `Grows` bearbeitet nur den Tile am Auge und kehrt nach World.Placed > 0 zurück:
Ringweite Vegetation und Wiedereintritt fehlen unabhängig von der ersten Sichtbarkeitsreparatur.

Nächster Umsetzungsschritt: vorhandene TreePrototype-Ausgabe in echte registrierte
Geometrie/Materialien überführen, stabiler Prototypbezug pro platzierter Art, Instanzen im
korrekten Regionsframe an den Renderer. Danach begrenzte Tile-Jobs/LOD statt Regionskapazität
blind erhöhen. 2123/2124 bleiben Voraussetzungen der vollständigen Abnahme.

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

## Erhalt der Artidentität, dev/codex

`Solid::Variant` erhält den vom Forest gewählten Artenindex. Das Feld nutzt die bisherigen
vier Paddingbytes: sizeof(Solid) bleibt 48 Bytes. ForestDraw hält Prototyp-ID/Höhe pro Art
und skaliert relativ zu dieser Höhe. Shipping vergibt getrennte IDs für Arten und Gebäude;
das sind weiterhin Katalogreferenzen, noch keine registrierten GPU-Geometrien.

`ForestInstancesKeepTheirSpecies` prüft zwei Arten mit unterschiedlichen Prototyphöhen,
einen BodyRange mit Offset sowie Positions-/Höhenbeibehaltung. Echter Mutationslauf mit
`Prototypes_[0]` statt `Prototypes_[body.Variant]`: Art-ID und Maßstab rot, drei Checkfehler.
Wiederhergestellt: `make suite SUITE=outshine/conventions`, 10/10 PASS, Exit 0
(`build/forest-species-restored.log`; Mutation `build/forest-species-negative.log`).
Die neue Fixture benötigte vorab einen korrigierten Rasteraufbau; kein Produktoracle gelockert.

`make shots PLACE='--measures Koerbersee'`, Exit 0, `build/forest-species-place.log`:
54fe2b37, p99 7,82 ms, 0/120 Standframes über Budget; 4085 Flora und 4096 Instanzen.
PNG geöffnet: weiterhin keine lesbaren Baumkronen, keine beanspruchte Bildverbesserung.
Nächster Schritt bleibt Prototypgeometrie + MR-Materialien + echte instanzierte Übergabe,
danach räumlich vollständige begrenzte Tile-Platzierung. WI bleibt active/offen.

## Native Prototypgeometrie / neues Wachstumsfinding

TreePrototype::GeometryAt(rank) übergibt vorhandene Stammgeometrie und die erzeugte
Blattmorphologie als native Geometry in Metern, mit getrennten MR-Materialien. Der bisherige
20-Float-Tree-Parameterblock wird dafür ausdrücklich nicht als Materialzeile verwendet.
Dies ist die mesherseitige Übergabe, noch keine ringweite instanzierte Renderer-Anbindung.
Der erste echte Prototyprender fand den Wachstumsfehler 2177; dessen Reparatur lässt eine
Krone entstehen. Aktueller nativer Render: build/tree-native/birch.png, visuell geprüft.

Nächste notwendige Arbeit: Screen-error-LOD für Äste und räumlich gleichmäßige Blatt-/Kronen-
Coverage. Rank 3 hat 272934 + 80640 = 353574 Dreiecke pro Birke; für Wald nicht tragbar.
Die Expansion der Blattfächer in Geometry ist ein funktionsfähiger nativer Mesher, aber kein
Ersatz für geteilte Prototyp-/Blattdaten und budgetierte Instanzen. Diese großen Prototypen
nicht ungeprüft pro Baum in den Piece-Pool kopieren. Regionsframe/Tile-Streaming weiter offen.

## Aktueller Prototyp-LOD

2123 erhält jetzt alle Blattansätze und das deklarierte Maß statt 80-cm-Ersatzblättern.
Native Birke Rank 3: 2184 Rinden- plus 915500 Blattdreiecke. Die Blattkontur reduziert
112 auf 20 Dreiecke je Blatt unter geometrischem Fehlerbudget und zusätzlicher 2-%-
Flächenschranke (gemessener Verlust 0,18 %). Das ist gegenüber der
feinen Referenz billiger, gegenüber dem alten 716-Riesenblatt-Proxy teurer. Kleine Blätter
sind sichtbar, Kronendeckung/Licht bleiben unzureichend. Messung, Bilder und echte
Negativkontrollen in 2123. Keine Wald-Framezeit daraus ableiten.
Renderer-Handoff, gemeinsame Prototyp-Instanzen und ringweite Platzierung bleiben offen;
auch den aktuellen Prototyp nicht pro Weltinstanz in den Piece-Pool kopieren.
