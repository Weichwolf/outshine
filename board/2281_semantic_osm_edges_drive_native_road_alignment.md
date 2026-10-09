Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: roads, bridges, tunnels, topology

# Roads, rails and paths form a continuous usable transport network

## Ergebnis und Ist
Straßen, Geh-/Radwege, Bahn/Tram, Brücken und Tunnel stimmen in Form und Netzfunktion.
Aktueller Schwerpunkt: sichtbare Netzfehler in Wien, anschließend schwierige selbst gewählte
POIs in Feldkirch, Flensburg, Husum, Tokyo und Central Park. Neueste Bilder bleiben in shots/places.
Alignment, Corridors, RoadMesher/RoadSurfaceBuilder und Terrain-Deformation bestehen.
Mesher und Terrainkontakt teilen den vollständigen Anschlussumriss; Zweigverbindungen
dürfen keine Straßenränder durch ein Dreieck verlieren. Seitenflächen nutzen Kantennormalen.
Knoten im Kontaktkern reichen nicht: interpolierte Terrain-Zellen durchdringen Fahrbahnränder.
Starre Fahrbahnflächen benötigen konservativen Zellkontakt; zusätzliche Verfeinerung allein
behebt konkurrierende Randhöhen nicht. Wasserflächen gehören nicht in diesen Straßenvergleich.
Native Anschlussflächen erhalten eine gemeinsame Stützweite aus den Zell-Durchmessern aller
betroffenen Terrain-Seiten. Der zusätzliche Kontakt schneidet nur; ursprünglicher Kern und
weiche Ausläufe bleiben erhalten. Grobe/feine Seiten werten dasselbe räumliche Höhenfeld aus.
Synchroner und asynchroner Aufbau teilen die Implementierung; native Treffer laden das Ergebnis.
Gespeicherte horizontale Koordinaten bestimmen die gemeinsame Höhenebene; unabhängiges
Millimeterrunden der Höhe darf schmale Dreiecke nicht aus dieser Ebene kippen.
Places nutzen überwiegend polygonale Kontakte; Hermite-Kontaktprofile und adaptive
Nahfahrbahnen sind dort noch nicht integriert. Brückenanschlüsse sind verbessert, das Netz
hat weiterhin falsche Profile/Übergänge. Vorhandene Straßenqualität bleibt erhalten.
Lokale Straßenverfeinerung berücksichtigt bisher nur benannte Rundkurse; Places benötigen
den allgemeinen nativen Straßenbestand, nach Kameraabstand und Kontaktfehler ausgewählt.
Gelieferte Rail-/Transit- und Wegunterklassen bestimmen Breite, Oberfläche und Neigungsregeln;
`class=rail` erreicht als `kind=rail` das Schienenrezept. Originaltags bleiben erhalten.
Unbekannte Wegunterklassen behalten das allgemeine Rezept; ungültige Unterklassen werden nicht geraten.
Native Rezepte werden versioniert; Quellen bleiben erhalten. Tunnel sind von der Oberfläche getrennt.
Treppenstufen, Bahnsteige, Pier-/Brückenwege und Seilbahnen fehlen noch.

## Besitzer und nächste Lieferung
OSM-Adapter normalisiert vorhandene Semantik; world/navigation hält das native logische Netz.
generators/road besitzt Profile/Geometrie, Terrain konsumiert deren Kontaktdeformation.
Ein gemeinsamer Entwurf liefert sichtbare Oberfläche, Bodenauftrag und befahrbare Kontakte.
Keine zweite Straßenpipeline oder vollständige Quellenmigration als Vorbedingung.
1. An echten POIs Quelllinien, Ebenen und gerenderte Kontakte übereinander auswerten.
   Fehlende Abschnitte, falsche Kreuzungen, Höhen/Neigungen und Lücken getrennt lokalisieren.
   Erst einen vollständigen realen Anschluss samt Brücke sichtbar und funktional schließen.
2. Gemeinsame Knotenhöhen und C1-Profile durch alle angeschlossenen Straßen/Schienen/Wege
   führen; danach passende Nahabtastung integrieren. Nicht nur Rampenoffsets begrenzen.
3. Schienen mit eigenen Breiten, zulässigen Profilen und Oberflächen darstellen; keine
   Straßenrezept-Kopie. Geh-/Radwege, Bahnübergänge und getrennte Ebenen bleiben verbunden.
4. Fehlende Überbau-/Portal-/Widerlagerdetails aus belegten Klassen und plausiblen Parametern
   ergänzen. Markierungen, Geländer, Signale und Beleuchtung anschließend instanzieren.

## Verbindlicher Entwurf
- Grobstes DEM → Infrastrukturentwurf → benötigtes feines DEM → Kontaktdeformation → Vegetation.
  Grobe Höhen stützen zusammenhängende Profile, Kreuzungen und Decks. Feines Relief ergänzt
  deren Umgebung und wird unter den festgelegten Kontakten geformt; es zeichnet nicht die Straße neu.
  Bedarf/Erwerb besitzt 2336, native Produkte 2280. Vegetation nutzt die finale Oberfläche und Belegung.
- Gelieferte Klasse, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene normalisieren und erhalten.
  MVT ist kein vollständiger Routinggraph. Randfragmente deterministisch vereinigen;
  tatsächliche Schnittpunkte teilen Position/Höhe nur bei kompatibler Ebene/Brückenklasse.
  Nähe allein erzeugt keine Verbindung. Quellendpunkte und Brückenenden bleiben angeschlossen.
  Fehlende Topologie/Angaben als Annahmen kennzeichnen, nicht als belegte OSM-Verbindung.
- Ein Alignment je Kette. Fahrbahn, Schultern, Bord/Gehweg, Knoten und Kollision daraus ableiten.
  Gemeinsame Endpositionen/Höhen/Tangenten über Tiles; vollständige Längs-/Querneigung
  klassenabhängig begrenzen. Kreuzungsebenen erfüllen alle angeschlossenen Klassengrenzen.
  Beide Endkürzungen gemeinsam begrenzen; kurze Rampen behalten beide Anschlusshöhen
  unabhängig von Linienrichtung oder getrennten Quellendpunkten.
- Brückendecks verbinden Uferhöhen mit geneigtem Profil und notwendigem Freiraum.
  Einzelne hohe DEM-Proben heben nicht den gesamten Überbau an. Wasserfreiraum gilt über
  dem nativen Pegel im gemeinsamen lokalen Up-Bezug; Inseln/fehlende Pegel heben nichts an.
  Wasser-/Straßenkreuzungen vor Geometrie zusammenführen; Pfeiler, Widerlager und Rampen
  mit passenden Gründungen. Tunnel brauchen Portale/Freiraum, keine Geländeübermalung.
- Straßenbett und Terrain teilen denselben Kontakt. Physische Kerne/Höhen gemeinsam planen;
  fremde Kerne und echte Freiräume respektieren. Geschlossene Linien bilden nur ein Band;
  eingeschlossene Hügel/Senken bleiben erhalten. Kontaktkennungen folgen verbundenen Wegen
  derselben Ebene und gehören zu Terrain-Key/Kandidatendigest.
- Böschungen erreichen Quellgelände mit stetiger Höhe/Neigung: quintisches f, gemeinsames
  Gewicht (1-f)/f. Breite hypot(Mindestbreite, 1,875 × Höhendifferenz / Böschungsneigung);
  Quellneigung separat bewerten. Höhenunterschied/Schnitt/Auftrag an beiden Rändern aus
  unverändertem Quellboden im Kern bestimmen, nicht aus Auslaufrändern. Nicht unbegrenzt
  über Endpunkte extrapolieren. Becken gegen Außenauftrag schützen, Fundamente örtlich halten.
  Kandidaten decken maximale Übergangsbreite/Höhen ab; Ausblenden versteckt keine Höhenfehler.
  Nullauslauf bleibt ausdrücklicher harter Kontakt. Clearance schneidet nur, füllt nie Boden;
  quintische Freiraumausläufe respektieren physische Kontakte und Quellrelief.
  Steile Einschnitte/Stützböschungen sind zulässig. Kontaktprodukte unterscheiden natürlichen
  Fels-/Bodenschnitt und bauliche Sicherung für Fels, Beton/Mauerwerk und Materialübergänge (2171).
  Kernkorrekturen entscheiden die Zulässigkeit eines Kontakts. Weiche Ausläufe begrenzen ihre
  Korrektur örtlich; hohes Quellrelief außerhalb des Kerns darf den Kontakt nicht pauschal verwerfen.
- Verfeinerung gegen deformierte Oberfläche und Kontakte bestimmen. Deterministische gemeinsame
  Kanten/Ecken und konforme LOD-Ränder statt Risse; innere Kanten nicht durch abdeckende Eltern
  verändern. Kontakt-/Höhenfehler vor zusätzlichen Vertices beheben. 2336 besitzt Arbeitsauswahl,
  2280 versionierte native Rezepte/Cache, 2145 Pegel/Ufer, 2171 metrische Baustoffe.
- Terrain-Zellen, die eine starre Fahrbahnfläche schneiden, unter deren gemeinsamer Kontaktebene
  halten; nur Knoten innerhalb des Umrisses abzusenken genügt nicht. Rasterunterstützung begrenzen,
  Profile/Quellrelief und weiche Ausläufe erhalten. Zellkontakt budgetiert vor Asset-Speicherung
  erzeugen; Cachetreffer wiederholen ihn nicht. Geneigte Straßen-/Weg-/Brückenbänder benötigen ihren
  eigenen Zellkontakt; CPU-Beleg ersetzt keine Prüfung des GPU-Lattice.

## Forschungsgrundlage
[Street Modeling, SIGGRAPH 2008](../doc/references/infrastructure/siggraph/2008-interactive-procedural-street-modeling.pdf):
Graph und Geometrie trennen; reale Linien erhalten, keine Tensorfeld-Neuerzeugung.
[Geometry Clipmaps, SIGGRAPH 2004](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf):
Gemeinsame Ränder; Nahtschluss allein garantiert keine weiche Böschung.
[Übergangsvergleich](../test/experiments/earthwork_transition.py),
[Clearance-Ausläufe](../test/experiments/clearance_aprons.py): gleiche Inputs/Qualität vor Integration vergleichen.
[Kreuzungsflächen](../test/experiments/road_junction_coverage.py): gelieferte Linienrichtungen,
gemeinsamer vollständiger Umriss statt Dreiecksreduktion; Kontakt und Fahrbahn separat prüfen.
[Rasterkontakte](../test/experiments/road_terrain_contact.py): native Terrain-/Straßendreiecke;
Knotentest gegen konservative Zellüberdeckung bei gleicher Fahrbahn und Terrain-Auflösung.

## Abnahme
Durchgehende reale Abschnitte/Anschlüsse ohne Risse, verlorene Ebenen, Wasser-/Geländewände
oder unbefahrbare Profilwechsel. Quellenabdeckung, Netzfunktion, Bild und Kosten getrennt prüfen.
99 % ist der gewünschte Vollständigkeitsmaßstab, keine unbelegte Weltquote: sichtbare belegte
Abschnitte/Anschlüsse mit definiertem Vergleichsbestand zählen; kritische Kontaktfehler bleiben rot.
Nahkamera/Bewegung und Rundumdrehung prüfen; Tokio/Central Park und bestehende Straßenqualität
bleiben erhalten. Bildgewinn in den neuesten Place-PNGs, Arbeit/Bytes im unveränderten Profilbudget.
