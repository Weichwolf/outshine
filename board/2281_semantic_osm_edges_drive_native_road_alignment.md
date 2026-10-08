Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2169
Depends:
Area: generators, world, engine
Tags: roads, bridges, tunnels, topology

# Roads and structures form a continuous usable transport network

## Ergebnis und Ist
Straßen/Wege/Gehwege, Bahn/Tram und Brücken/Tunnel mit richtigen Ebenen und Anschlüssen.
Alignment, Corridors, RoadMesher/RoadSurfaceBuilder und Terrain-Deformation bestehen und
bleiben erhalten. Places verwenden überwiegend polygonale Kontakte aus Corridors;
Hermite-Kontaktprofile des RoadSurfaceBuilder sind dort noch nicht integriert. Klassenabhängige
Längsneigung, gemeinsame Höhen und komplexe Anschlüsse müssen im Place-Pfad gesichert werden.

## Besitzer und nächste Lieferung
OSM-Adapter besitzt Quellsemantik, world/navigation das logische Netz; generators/road
Profile/Geometrie, Terrain die Kontaktdeformation. Vorhandene Linien/Produkte reichen für
den nächsten Schritt. Erst einen realen Stadt-/Hafenanschluss samt Brücke bis zum Bild liefern;
keine vollständige Quellenmigration oder Hockenheim-Runde als Vorbedingung.
Native `EarthworkKind::Clearance`-Aufträge schneiden nur Terrain oberhalb ihrer Grenze;
sie füllen nie Boden und sperren keine tiefere Becken-Vertiefung. `Corridors::AppendTerrainStamps`
und `AppendJunctionTerrainStamp` erzeugen sie für Brückenspannen und erhöhte Knoten;
`EarthworkPress::BidsTerrain` trennt sie von tatsächlichen Kontakten. Widerlager und
Anschlussrampen behalten ihre Kontakte. Husums künstlicher Bodenriegel unter der Brücke
ist im Runtime-Bild beseitigt; breite Uferböschungen bleiben bei 2145 offen.
`DetermineWaterClearance` bestimmt den Wasserfreiraum beim Entwurf;
`ResolveBridgeConnections` führt Wasser- und Straßenkreuzungen vor der Geometrie zusammen.
Endhöhen gelten auch für einfache Zweiarm-Übergänge. Knoten und Rampen konsumieren dieselben
Höhen; einmalige und schrittweise Erzeugung behalten identische Geometrie/Kontakte.
Kurze überlappende Rampen erhalten beide Anschlusshöhen unabhängig von der Linienrichtung;
auch nicht geteilte Quellen-Endpunkte übernehmen fortgepflanzte Höhen.
Das registrierte Vektorschema versorgt Wasser-/Verkehrsfelder. Road-Site erhält native Pegel
im lokalen Up-Bezug; Inseln/fehlende Pegel heben keine Brücke an. Wasserfreiraum gilt über
dem Pegel, nicht zusätzlich über dem höchsten Boden. Fehlende Durchfahrtshöhen sind Klassenannahmen.
Straßen-Ausläufe blenden jetzt ohne den bisherigen Sprung zum Quellgelände aus.
Polygonale Straßenkontakte erfassen Schnitt und Auftrag an beiden Schultern;
die größte Achsen-/Randabweichung bestimmt den geplanten Kontakt und seine Auslaufbreite.
Polygonale Spannen tragen eine gemeinsame, im Kontaktauftrag gültige Wegkennung.
Physische Straßen-/Fundamentkerne haben Vorrang vor sämtlichen Außenböschungen;
fremde Kerne und echte Brückenfreiräume bleiben wirksam, Ausläufe sind keine Freiraumdecken.
Polygonale Ausläufe verwenden den Höhenbereich des tatsächlichen Kontakts; Längsneigung
nicht unbegrenzt über Endpunkte extrapolieren. Kennung in Terrain-Key und
Kandidatendigest; an gemeinsamen Ringkanten numerische Distanz bis 1 µm als Kontakt behandeln.
Überlappende physische Kerne/Höhen müssen gemeinsam geplant werden, nicht nachträglich kaschiert.
`CorridorCrossings` verbindet nur beteiligte Wege derselben Ebene und Brückenklasse.
Gemeinsame Innenknoten erhalten Ebenenbezug; tatsächliche Schnittpunkte teilen Position/Höhe.
Zehn-Meter-Nähe erzeugt keine Anschlüsse. Vorhandene Brückenenden bleiben angebunden.
Geschlossene Linien erzeugen ausschließlich ihr Straßenband und örtliche Böschungen;
eingeschlossene Hügel/Senken bleiben erhalten. Bodenkreuzungen übernehmen fortgepflanzte
Rampen-Endhöhen. Tatsächlich verbundene Ground-Ways derselben Ebene und ihre Knoten teilen
eine Terrain-Kontaktkennung; polygonale Ausläufe werden mit Quellboden gewichtet gemischt.
Alle Außenböschungen mischen gemeinsam mit Quellboden: quintisches `f`, Gewicht `(1-f)/f`.
Cut-only trägt keine positive Korrektur bei; Becken bleiben gegen Außenauftrag geschützt.
`CorridorContacts` bildet gepackte Nachbarschaften und lineare BFS aus belegten Verbindungen.
Eine waagerechte Brückenplatte am höchsten DEM-Punkt hebt Feldkirchs tiefere Anschlüsse
unnötig an. Brückendecks verbinden die Uferhöhen mit geneigtem Profil und erforderlichem
Freiraum; innere DEM-Hindernisse bestimmen nicht die Höhe des gesamten Überbaus.
Knoten und Spannen teilen lokale Wegstationen; beide Endkürzungen gemeinsam begrenzen.
Nächste Lieferung: Knotenhöhen/C1-Profil gemeinsam fitten; Fundamentwirkung aus dem tatsächlichen
Gelände-/Kontakthöhenbereich planen und bei hohen Unterschieden passende Gründung vorsehen.
Quellen-/LOD-Nähte sind geschlossen; gemeinsame Höhen liegen auf Float-Präzision.
Ein Fundament schneidet bis 17,4 m bei 6 m Auslauf; tatsächlichen Höhenbereich berücksichtigen.
Gleiche Kanten nutzen deterministisch die feinere Quelle; gemeinsame Ecken folgen der tatsächlich
angrenzenden gröbsten Mesh-Kante. Abdeckende Vorfahren dürfen innere Kanten nicht verändern.
Terrain-Auswahl berücksichtigt bislang Roh-DEM und deklarierte Fahrstrecken, keine Place-Kontakte.
Verfeinerung gegen deformierte Oberfläche bestimmen; benachbarte LOD-Kanten konform verbinden.
Skirts ersetzen keine gemeinsame Oberfläche. Höhe und Abtastung getrennt korrigieren;
unnötig hohe Kontakte nicht mit zusätzlichen Vertices kaschieren.
Ground-Ways haben überwiegend Kontakte, Brücken eigene Meshes; Nahfahrbahnen brauchen dieselbe adaptive Abtastung.

## Verfahren
- Gelieferte Klasse, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene normalisieren. MVT ist
  kein vollständiger Routinggraph: Randsegmente/IDs vereinigen, keine Kreuzung aus bloßer Nähe.
  Fehlende Topologie ausdrücklich behandeln, belegte Ebenen erhalten.
- Ein Alignment je zusammenhängender Kette; Schultern, Bord/Gehweg und Knoten davon ableiten.
  Gemeinsame Endpunkte/Höhen/Tangenten über Knoten und Tiles; klassenabhängige Längs- und
  Querneigung begrenzen. Kreuzungsebenen halten die Grenzen aller angeschlossenen Klassen.
  Die Rampenkorrektur begrenzt bisher nur ihren eigenen Offset;
  das vollständige Höhenprofil muss die Klassengrenze einhalten. Segmentwellen/Nähte
  an der Ursache beheben, keine zweite Pipeline.
- Geschlossene Straßenlinien bleiben Bänder; keine Durchschnittshöhen-Plattform im Inneren.
  Natürliche Hügel/Senken und Wasser innerhalb der Schleife bleiben erhalten.
- Straßenbett und Terrain aus demselben Kontaktprofil. Übergänge erreichen das ursprüngliche
  Gelände mit stetiger Höhe und Neigung; Breite aus Höhendifferenz und Böschungsneigung.
  Keine SimCity-Terrassen: Übergänge ohne künstliche Stufen und mit stetiger Neigung.
  Quintischer Übergang; `hypot(Mindestbreite, 1,875 × Höhendifferenz / Böschungsneigung)`
  begrenzt die zusätzliche Steigung auf ebenem Quellboden. Höhendifferenz aus dem geplanten
  Kontakt und tatsächlichem Quellgelände im Kern, nicht vom Auslaufrand; Quellneigung separat bewerten.
  Bedarf an beiden Fahrbahnrändern für Schnitt und Auftrag erfassen; Profile dürfen eine
  unbekannte Höhendifferenz nicht stillschweigend als Null behandeln.
  Räumliche Kandidaten umfassen den maximal zulässigen Übergang und Höhenbereich. Unzulässige Kontakte vor dem Ausblenden ablehnen;
  Ausblenden darf die Höhenprüfung nicht verbergen. Profil-/Polygonböschungen und Fundamente
  teilen ein Bodenfeld; keine harte Fremdböschung am echten Kontakt. Fundamentwirkung bleibt örtlich begrenzt; hohe
  Geländeunterschiede brauchen passende Gründung/Stützung, keine großräumige Planierung.
  Auslaufbereiche füllen keine Wasserbecken. Native Terrain-/Region-Rezepte
  versionieren, Quellcache erhalten. Keine harte Höhenbegrenzung am Auslaufrand.
  `test/experiments/earthwork_transition.py` vergleicht Übergänge; der unabhängige Kreisbogentest
  bewertet Höhe/Krümmung gegen integrierte Kreisgeometrie, Segmentierungsgrenze bleibt erhalten.
- Brücken mit Überbau/Pfeilern/Widerlagern und Anschlussprofil; Tunnel mit Portal/Freiraum.
  Nur tatsächliche Kontakte stempeln Boden. Gemeinsamer Bezug zu Gebäuden und Wasser aus 2145.
- Markierungen, Geländer, Signale und Beleuchtung aus Klassenparametern; Wiederholungen
  instanzieren. 2336 begrenzt Nahgeometrie, das logische Netz bleibt erhalten. 2171 beleuchtet
  metrische Straßenbaustoffe; Quellarchive sind kein dauerhafter Bestandteil des Netzes.

## Forschungsgrundlage
[Interactive Procedural Street Modeling, SIGGRAPH 2008](../doc/references/infrastructure/siggraph/2008-interactive-procedural-street-modeling.pdf)
([Primärquelle/Einordnung](../doc/references/README.md)): Graph und Geometrie getrennt halten.
OSM-Netz/Ebenen erhalten; keine Tensorfeld-Neuerzeugung realer Straßen. Gemeinsame Profile,
Anschlussregeln und instanzierte Tragwerksteile ergänzen die vorhandene Qualität.
[Geometry Clipmaps, SIGGRAPH 2004](../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf):
Gemeinsame Randgeometrie und räumliche Übergänge; Nahtschluss allein garantiert keine weiche Böschung.

## Abnahme
Durchgehende reale Straße samt korrekt angeschlossener Brücke ohne Gelände-/Wasserwände,
Lücken oder verlorene Ebenen. Bestehende Straßenqualität erhalten oder verbessern;
Netzfunktion und Bild separat prüfen, gemessene Arbeit/Bytes innerhalb AGENTS-Budget.
