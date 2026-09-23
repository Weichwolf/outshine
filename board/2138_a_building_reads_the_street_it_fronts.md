Type: feature
State: open
Parent: 2169
Area: generators, world
Tags: webcam, measured
Depends: 2173, 2121

# Buildings receive their street and generate functional facades

## IST / Reparatur

`StructureBuildQueue.cpp::RawOf` leert `RawTile::Ways` und füllt sie im gelesenen Pfad nicht.
`StructureBake::NearestStreet`/Frontage können so ihre Straße nicht kennen. Die frühere
Abhängigkeit 2157 ist geschlossen/historisch und wurde durch die tatsächlichen offenen
Träger ersetzt. Alle Stadtbilder zeigen fast fensterlose Prismen.

1. Straßenlinien samt Klasse/Halbbreite aus derselben Semantik wie 2133 an den Bake reichen;
   über Tilegrenzen Nachbarschaft berücksichtigen. Eingänge zur zugänglichen Front,
   nicht zum geometrisch nächstgelegenen unzugänglichen Autobahn-/Brückensegment ausrichten.
2. Nutzung, Dachform und belegte Höhen aus 2173; gemeinsame Brandwand/Traufe, Innenhöfe,
   Erdgeschoss, Türen/Fenster/Reveals, Sockel, Dachentwässerung und Dachaufbauten generieren.
   Geometrie nach projizierter Größe staffeln (2123), Materialdetail in 2171.
3. Funktionale Straßenkante: Gehweg, Bordstein mit Fläche, Markierungen, Einfahrten,
   Übergänge und Entwässerung. 2121 besitzt Höhen-/Junction-Solve, dieses WI deren Bebauung.

## Begrenzter Konstruktionsraum

Kein Katalog fertiger Häuser, der OSM-Grundrisse verbiegt. Ein endlicher Satz
parametrischer Regeln zerlegt den tatsächlichen Grundriss in Baukörper, Hof,
Geschosse, Dachflächen, Sockel und Fronten. Fenster-, Tür-, Dach- und Materialmodule
sind wiederverwendbare Details, keine Quelle für Topologie oder Gebäudehöhe.
Explizite OSM-Maße, Geschosse, Eingänge, `building:part`, `min_height`, Nachbarwände,
Straßenzugang und Gelände-/Durchfahrtbedingungen sind harte Constraints. Fehlende
Werte erhalten typ-/ortsabhängige plausible Priors und einen stabilen Seed; diese
Annahmen bleiben im ConstructionResult sichtbar. Ästhetische Rhythmen, Dachneigung
und Fassadenstil sind weiche Scores nach erfüllter Funktion, nicht umgekehrt.
Der Solver bearbeitet ein Gebäude plus betroffene Nachbarn/Anschlüsse mit begrenzter
Kandidatenzahl und Arbeit. Zellen dienen Index, Streaming und lokalem Konfliktfenster;
Gebäude- und Straßenkonturen bleiben kontinuierlich über Zellgrenzen. Bei Widerspruch
oder Budgetende: diagnostizierte konservative Konstruktion nur wenn alle harten
Verträge gelten, sonst kein physisch gültiges Gebäude. Kein stilles Weglassen.

- [ ] Fronted-Counter mit/ohne Ways unterscheidet sich; falsch zugeordnete Straße scheitert
      am Zugangsoracle. Straße/Parzelle über Tilekante bleibt dieselbe.
- [ ] Darmstadt/Husum/Rosenheim/Graz: variierende plausible Fassaden und Dächer bei 200/30/5 m,
      keine Pflicht zur Kopie einer konkreten Fassade. Alle Körper erfüllen 2168.
- [ ] Gleicher OSM-/DEM-/Regelstand liefert unabhängig von Tile-Reihenfolge dasselbe
      Gebäude; geänderte Seeds variieren nur unbelegte Details. Gegenproben für
      blockierten Eingang, kollidierendes `building:part`, Hang und freie Durchfahrt.
- [ ] Wiederholung/LOD-Flimmern/Kosten unter Bewegung nach 2092; Defaultverteilung im Manifest.

Wahl: Straßenbezug als PCG-Eingang wie Unreal; RAGE als visuelle Frontage-Referenz.
Keine nachträgliche bloße Textur über falsche Gebäudemassen.
