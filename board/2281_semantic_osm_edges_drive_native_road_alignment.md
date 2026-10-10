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
Plausible Straßen, Geh-/Radwege, Bahn/Tram, Brücken und Tunnel bilden ein funktionales Netz.
Vorrang: Wiener Anschlüsse/Brücken; danach Feldkirch, Basel Badischer Bahnhof, Zürich HB und Häfen.
Corridors liefert Place-Geometrie und polygonale Kontakte. Gemeinsame native Höhenplanung
bindet Anschlüsse, zulässige Sekanten und Freiraum vor Geometrie/Terrain; Sync/Async teilen sie.
Der 2D-Endpunktplan trennt Position, Ebene und rekonstruierte Verbindungen. Kreuzungen werden
jeweils einmal gespeichert; Grid-Zellen referenzieren sie. Gestapelte Decks behalten eigene Ports.
Doppelte MVT-Linien erzeugen keine zusätzlichen Knotenflächen. Alle Bodenwege liefern Kernkontakte,
auch ohne Erdarbeiten; Flensburgs Wegöffnung durch fremden Aushub ist geschlossen.
Offen: geografische Kachelports/Cacheplan, explizite Ebenenübergänge, C1, Freiraum über volle Breite,
Tunnel/Portale, Tragwerk sowie Treppen, Bahnsteige, Piers und Seilbahnen.
Der analytische RoadAlignment-/Surface-Pfad verlangt benannte Routen; auf denselben Plan vereinigen.

## Nächste Lieferung
1. Einen realen Anschluss samt Brücke als undekoriertes, funktionales Grundmodell schließen.
   Infrastruktur allein über Szenario rendern; Höheninputs/Kontakte/Licht bleiben aktiv.
2. Geografisch stabile 2D-Kacheln mit Randports und expliziten Übergängen nativ planen/speichern.
   Kettenprofile und begrenzte Bauteilrezepte daraus ableiten; kein zweiter Netzaufbau.
3. Eigene Schienen-/Wegprofile, Bahnübergänge und gestapelte Brücken/Tunnel integrieren.
4. Bauhaus-/Art-déco-Gestaltung/Material auf demselben konstruktiven Plan ergänzen.

## 2D-Plan und Ebenen
- OSM-Adapter normalisiert gelieferte Klassen, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene;
  world/navigation hält das native logische Netz. generators/road besitzt Profile/Geometrie.
  Terrain konsumiert Kontakte. Bedarf: 2336; Produkte/Cache: 2280; Gebäude: 2173; Pegel: 2145.
- Grid partitioniert/indexiert Vektorpläne, rastert keine Straßenachsen. Gemeinsame Randports:
  Position, Richtung, Breite, Ebene, Höhe/Tangente und Besitzer; nur begrenzte Nachbarbereiche.
  Explizite Übergänge verbinden Ebenen. MVT ist kein vollständiger Routinggraph.
- Aktueller Anschlussentwurf: Positionsauflösung 1e-7 Grad; Endpunkt-Abweichungen bis 0,25 m
  nur bei gleicher Ebene/Brückenklasse vereinigen. Deckungsgleiche Boden-/Brückenenden
  verbinden nur kompatible Verkehrsklassen (Straße, Weg, Bahn); das bleibt rekonstruierte Topologie. Andere Ebenen bleiben getrennt.
  Nähe allein darf keine Kreuzungsverbindung oder versteckte Rampen erzeugen. Bahn-/Wegkreuzungen
  bleiben getrennte Ports; echte niveaugleiche Bahnübergänge brauchen ein eigenes belegtes Rezept.
  Niveaugleiche Straßen-/Weganschlüsse teilen ihre native Höhe; Brückenübergänge bleiben klassifiziert.
  Kompatible Bodenweg-Endpunkte an Brückenachsen rekonstruieren gemeinsame Ports; ein Brückenende
  nahe einem durchlaufenden Weg allein erzeugt keinen Anschluss. Vor Profil/Geometrie abschließen.
- Pro Kreuzung Ebenenordnung und Freiraum zwischen geplanten Profilen samt Deckdicke lösen;
  Layer ist Ordnung, keine feste Höhe. Fuß-/Radweg/Treppe/Path: 2 m Akteur + 0,5 m Reserve;
  Straßen/Bahn behalten eigene Freiräume. Gelieferte Maße respektieren; fehlende Maße sind Rezepte.
- Wenige parametrische Rezepte: Band, Kreuzung/Abzweig, Einfädelung, Rampe, Brückendeck/Tragwerk,
  Tunnel/Portal und Schienenknoten. Derselbe Plan liefert Mesh, Kollision und Terrainkontakt.
  Rail-/Transit- und Wegunterklassen bestimmen Breite, Material, Profil; rail erreicht das Schienenrezept.
  Unbekannte Unterklassen nutzen das allgemeine Rezept; ungültige Werte nicht raten. Tags erhalten.

## Profile und konstruktive Anschlüsse
- Ein Alignment je Kette; Fahrbahn, Schultern, Bord/Gehweg, Knoten und Kollision daraus ableiten.
  Gemeinsame Endposition/Höhe/Tangente; gesamte Längs-/Querneigung klassenabhängig begrenzen.
  Beide Endkürzungen gemeinsam beschränken; Rampen behalten beide Höhen unabhängig von Richtung.
  Neigung über tatsächlich zugeschnittene Profillängen, ohne künstlich ebene Anschlussflächen.
  Höhen und Anschlussneigungen gemeinsam lösen. Gerichtete Konfliktzyklen liefern zusätzliche
  Bedingungen nur für betroffene Knotenflächen; Neigungen dürfen ihre Richtung ändern.
  Alle zuvor gelösten Bedingungen und klassenabhängigen Gradgrenzen bleiben verbindlich.
- Höhenplanung bevorzugt Schnitt; Auftrag nur für notwendige Kontakte/Freiräume. Hohe einzelne
  DEM-Proben heben keinen ganzen Überbau. Wasserfreiraum über nativem Pegel im gemeinsamen
  Up-Bezug; Inseln/fehlende Pegel erzeugen keine Hebung. Pfeiler/Widerlager plausibel gründen.
  Tunnel brauchen eigene Portale/Freiraum. Keine Übermalung; RoadAlignmentBuilder verweigert
  Brücke/Tunnel bis zum Struktursolver. Ebenerdige Bänder nur obere Fläche, Decks auch unten/seitlich.
- Gespeicherte horizontale Koordinaten bestimmen die gemeinsame Höhenebene. Unabhängiges
  Millimeterrunden darf schmale Dreiecke nicht kippen; Seiten nutzen Kantennormalen.

## Terrainkontakt
- Regionale Profile/Anschlüsse und Gebäudekontakte zuerst, finales Terrain darunter, Vegetation zuletzt.
  Grobes DEM liefert Bezug; feines Relief verschiebt geplante Anschlüsse nicht. Keine globale serielle Baufolge.
- Fahrbahn/Kernkontakt teilen Geometrie und Höhe. Fremde Kerne/Freiräume respektieren; geschlossene
  Linien bilden Bänder, füllen keine Hügel/Senken. Verbundene Kontakte gleicher Ebene im Terrain-Key/Digest.
- Böschung stetig in Höhe/Neigung: quintisches f, Gewicht (1-f)/f; Breite
  hypot(Mindestbreite, 1,875 × Höhendifferenz / Böschungsneigung). Quellneigung separat prüfen;
  Schnitt/Auftrag an beiden Kernrändern aus unverändertem Quellboden bestimmen. Enden nicht unbegrenzt
  extrapolieren. Kerne bestimmen Zulässigkeit; Ausläufe begrenzen örtlich, fremdes Relief verwirft keinen Kern.
- Kandidaten decken Übergangsbreite/Höhen ab. Nullauslauf ist harter Kontakt; Clearance schneidet nur.
  Quintische Freiraumausläufe respektieren Quellrelief/Kerne. Becken schützen, Fundamente örtlich halten.
  Steile Einschnitte zulässig; Kontakt unterscheidet Fels/Boden und bauliche Sicherung für Material in 2171.
- Terrain-Zellen unter schneidender starrer Fahrbahn/Kontaktebene halten; reine Knotenabsenkung reicht nicht.
  Mesher/Kontakt teilen Anschlussumriss/Zellstützweite; zusätzliche Überdeckung schneidet nur.
  Auch geneigte Bänder brauchen Zellkontakt; vor Speicherung budgetiert erzeugen, Treffer wiederholen ihn nicht.
  GPU-Lattice prüfen. Gemeinsame Kanten/Ecken/konforme LOD-Ränder; Eltern verändern keine inneren Kanten.
  Kontakt-/Höhenfehler vor Verfeinerung beheben; 2336 wählt Arbeit, 2280 versioniert native Rezepte/Cache.

## Verfahren und Abnahme
[Street Modeling, SIGGRAPH 2008](../doc/references/infrastructure/siggraph/2008-interactive-procedural-street-modeling.pdf): Graph vor Geometrie.
[Straßenmodelle, Eurographics 2010](../doc/references/infrastructure/eurographics/2010-procedural-generation-of-roads.pdf): Profile, Querschnitte, Bauwerke, Terrainkontakte.
[SUMO/CARLA/OSM2World](../doc/references/infrastructure/README.md): Netzfunktion und isolierte Meshes; keine Vermessungstreue.
[2D-Kacheln](../test/experiments/infrastructure_tiles.py): Feldkirchs 19.230 Abschnitte → 20.094 Teile/133 ENU-Kacheln bei 256 m; Bedarf: 2.711 statt 19.230 Kandidaten, 1.294 gleiche Treffer. Geografischer/native Vertrag offen.
[Höhenplanung](../test/experiments/road_profile_envelopes.py): gerichteter Freiraum, Höhenränder/Portoffsets, Schnittvorrang/Minimax und nativer LP-Vergleich; kein C1-/Breitenbeweis.
[Übergänge](../test/experiments/earthwork_transition.py), [Freiraum](../test/experiments/clearance_aprons.py), [Kreuzungsflächen](../test/experiments/road_junction_coverage.py), [Rasterkontakt](../test/experiments/road_terrain_contact.py): echte Inputs, gleiche Qualität.
Durchgehende reale Anschlüsse ohne Risse, verlorene Ebenen, Wasser-/Geländewände oder unbefahrbare Profile.
Auch ohne Terrain/Gebäude lückenlos. Quellenabdeckung, Netzfunktion, Bild und Kosten getrennt prüfen.
Nahbewegung und Rundumdrehung; Straßenqualität und Tokyo/Central Park erhalten. Neueste Place-PNGs
zeigen Gewinn im unveränderten Profilbudget. Kritische Anschluss-/Kontaktfehler bleiben rot.
