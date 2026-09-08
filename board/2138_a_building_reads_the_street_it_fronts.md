Type: feature
State: open
Parent: 2169
Area: generators, world
Tags: webcam, measured
Depends: 2173, 2121

# Buildings receive their street and generate functional facades

## IST / Reparatur

`StructureBakes.cpp::RawOf` leert `RawTile::Ways` und füllt sie im gelesenen Pfad nicht.
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

- [ ] Fronted-Counter mit/ohne Ways unterscheidet sich; falsch zugeordnete Straße scheitert
      am Zugangsoracle. Straße/Parzelle über Tilekante bleibt dieselbe.
- [ ] Darmstadt/Husum/Rosenheim/Graz: variierende plausible Fassaden und Dächer bei 200/30/5 m,
      keine Pflicht zur Kopie einer konkreten Fassade. Alle Körper erfüllen 2168.
- [ ] Wiederholung/LOD-Flimmern/Kosten unter Bewegung nach 2092; Defaultverteilung im Manifest.

Wahl: Straßenbezug als PCG-Eingang wie Unreal; RAGE als visuelle Frontage-Referenz.
Keine nachträgliche bloße Textur über falsche Gebäudemassen.
