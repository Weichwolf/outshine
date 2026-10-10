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
Corridors liefert Geometrie/Kontakte; gemeinsame native Höhenplanung bindet Anschlüsse, Sekanten
und Freiraum vor Geometrie/Terrain; Sync/Async teilen sie. Anschlussflächen übertragen nativ
Längs-/Querneigung samt Flächennormale an die Straßenbänder; ein Mittelpunkt allein genügt nicht.
Der 2D-Endpunktplan trennt Position, Ebene und rekonstruierte Verbindungen. Kreuzungen werden
jeweils einmal gespeichert; Grid-Zellen referenzieren sie. Gestapelte Decks behalten eigene Ports.
Doppelte MVT-Linien erzeugen keine zusätzlichen Knotenflächen. Bodenwege liefern auch ohne Erdarbeiten Kernkontakte.
Offen: geografische Kachelports/Cacheplan, Ebenenübergänge, C1, Breitenfreiraum, Tunnel/Portale, Tragwerk, Treppen, Bahnsteige, Piers, Seilbahnen.
Der analytische RoadAlignment-/Surface-Pfad verlangt benannte Routen; auf denselben Plan vereinigen.
Zürich HB lädt wieder: gemeinsame Brückenebenen teilen eine physische Höhe, Verkehrsports bleiben
getrennt; kurze Kreuzungen im gemeinsamen Landekern erzeugen keinen falschen Unterpass.
Bahnhofsanschlüsse bleiben fehlerhaft; Assettreffer 1,5 s bei 1280×720/60.

## Nächste Lieferung
1. Einen realen Anschluss samt Brücke als undekoriertes, funktionales Grundmodell schließen.
   Die native Produktauswahl aus 2188 liefert eine reine Infrastrukturansicht ohne Terrain/Gebäude;
   Höheninputs/Kontakte/Licht bleiben aktiv. Wien zeigt auch darin fehlerhafte Nebenrampen und
   Anschlüsse: zuerst den Netz-/Bauteilplan schließen, danach den Terrainkontakt prüfen.
2. Geografisch stabile 2D-Kacheln mit Randports und expliziten Übergängen nativ planen/speichern.
   Kettenprofile und begrenzte Bauteilrezepte daraus ableiten; kein zweiter Netzaufbau.
3. Headless-Python: 2D mit 1–2 Ebenen, dann orthografisches 2,5D mit 2–3; endliche Rezepte/Solver an realen Inputs vergleichen.
   Erfolgreiche PNGs/Parameter/Messwerte nach `build/shots/experiments/infrastructure/{2d,2_5d}/`; öffnen, nach Budgetbeleg integrieren.
4. Solarpunk 2050: Bauhaus/Art déco, Grünstreifen/Mittelgrün/Pflanzbereiche bei verfügbarem Raum;
   im Querschnitt reservieren, Vegetation berücksichtigt Fahrwege/Sicht/Freiraum. Decks dürfen stützenfrei wirken.

## 2D-Plan und Ebenen
- OSM-Adapter normalisiert gelieferte Klassen, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene;
  world/navigation hält das native logische Netz. generators/road besitzt Profile/Geometrie.
  Terrain konsumiert Kontakte. Bedarf: 2336; Produkte/Cache: 2280; Gebäude: 2173; Pegel: 2145.
- Geografisch stabile GeoCellId-Zellen partitionieren/indexieren Vektorpläne; Straßenachsen bleiben
  kontinuierlich. Gemeinsame Randports:
  Position, Richtung, Breite, Ebene, Höhe/Tangente und Besitzer; räumlicher Regelprüfer repariert nur Konfliktnachbarn.
  Brücken-/Rampeneinheiten dürfen Zellgrenzen überspannen; ihre gekoppelten Profile gemeinsam lösen.
  Zellen indexieren Teile des gemeinsamen Plans, Nachbarn übernehmen dieselben Randwerte.
  Explizite Übergänge verbinden Ebenen. MVT ist kein vollständiger Routinggraph.
- Aktueller Anschlussentwurf: Positionsauflösung 1e-7 Grad; Endpunkt-Abweichungen bis 0,25 m
  nur bei gleicher Ebene/Brückenklasse vereinigen. Deckungsgleiche Boden-/Brückenenden
  verbinden nur kompatible Verkehrsklassen (Straße, Weg, Bahn); das bleibt rekonstruierte Topologie. Andere Ebenen bleiben getrennt.
  Nähe allein darf keine Kreuzungsverbindung oder versteckte Rampen erzeugen. Bahn-/Wegkreuzungen
  bleiben getrennte Ports; echte niveaugleiche Bahnübergänge brauchen ein eigenes belegtes Rezept.
  Niveaugleiche Straßen-/Weganschlüsse teilen ihre Höhe; Bodenweg-Endpunkte verbinden Brückenachsen,
  nahe Brückenenden allein verbinden keinen durchlaufenden Weg.
- Physische Höhenbindung ist getrennt vom Verkehrsnetz; gleiche Ebene/Brückenklasse teilt Höhe auch zwischen Verkehrsklassen.
  Bei gemeinsamem eigenen Endport gehören Kreuzungen innerhalb des 4-m-Landekerns beider
  Quellachsen zum Anschluss; begrenzte Weglänge statt bloßer räumlicher Nähe. Andere Kreuzungen
  behalten Ebenenfreiraum. Kein Aufweichen von Steigung, Freiraum oder Solvergrenzen.
- Pro Kreuzung Ebenenordnung und Freiraum zwischen geplanten Profilen samt Deckdicke lösen;
  Layer ist Ordnung, keine feste Höhe. Fuß-/Radweg/Treppe/Path: 2 m Akteur + 0,5 m Reserve;
  Straßen/Bahn behalten eigene Freiräume. Gelieferte Maße respektieren; fehlende Maße sind Rezepte.
- Wenige parametrische Rezepte: Band, Kreuzung/Abzweig, Einfädelung, Rampe, Brückendeck/Tragwerk,
  Tunnel/Portal und Schienenknoten. Derselbe Plan liefert Mesh, Kollision und Terrainkontakt.
  Ein U-Querschnitt: flach/breit Gehweg/Bord, höher gesicherter Steg, kräftig Brückendeck;
  Seiten zugleich Tragwand/Geländer. Maße, Material, Kollision und Portöffnungen gemeinsam planen.
  Unterklassen bestimmen Breite/Material/Profil; unbekannte nutzen das allgemeine Rezept. Tags erhalten.

## Profile und konstruktive Anschlüsse
- Bauteilplan vor Mesh: 2D-Topologie/Ebenen lösen, endliche Rezepte mit benannten Ports einsetzen.
  Gemeinsame Flächen-/Randunterteilung vor Triangulierung; Profilgrenzen erhalten, Mesh bestimmt Höhenbedingungen. Komplexe Knoten/Brücken
  formstabil positionieren; verbindende Alignments begrenzt anpassen.
  Verletzte Krümmung/Steigung/Freiraum erfordert mehr Übergangsraum, neue Lage oder anderes Rezept.
  Wiener/Zürcher Inputs: Ebenen bei gleicher Netzgröße variieren, Port-/Konfliktkopplung separat;
  Solverzeit/Speicher samt Wachstum messen; drei lokale Ebenen sind Hypothese, keine Budgetfreigabe.
- Fehlendes Rezept: Bauform auf ein vorhandenes vereinfachen; Verkehrsverbindungen, erforderliche
  Ebenentrennung und Freiräume erhalten. Keine verlorenen Wege oder versteckten Kreuzungen.
- Ein Alignment je Kette; Fahrbahn, Schultern, Bord/Gehweg, Knoten und Kollision daraus ableiten.
  Gemeinsame Endposition/Höhe/Tangente; gesamte Längs-/Querneigung klassenabhängig begrenzen.
  Rampen behalten beide Höhen; Endkürzungen gemeinsam begrenzen. Höhen/Neigungen gemeinsam lösen.
  Reale Zuschnittlänge begrenzt Neigung. Gerichtete Konfliktzyklen liefern zusätzliche
  Bedingungen nur für betroffene Knotenflächen; Neigungen dürfen ihre Richtung ändern.
  Alle zuvor gelösten Bedingungen und klassenabhängigen Gradgrenzen bleiben verbindlich.
- Höhenplanung bevorzugt Schnitt; Auftrag nur für notwendige Kontakte/Freiräume. Hohe einzelne
  DEM-Proben heben keinen ganzen Überbau. Wasserfreiraum über nativem Pegel im gemeinsamen
  Up-Bezug; Inseln/fehlende Pegel erzeugen keine Hebung. Pfeiler/Widerlager plausibel gründen.
  Tunnel/Durchfahrten: gespeicherter 3D-Freiraum mit Innenhülle/Portalen; Gebäude schneiden, Terrainportale lokal als Mesh öffnen;
  Obergeschosse erhalten. Höfe über Innenringe/Blockumriss plus Wegenetz erkennen; alle Außenanschlüsse erhalten.
  Keine Übermalung; RoadAlignmentBuilder verweigert Brücke/Tunnel bis zum Struktursolver. Ebenerdige Bänder nur obere Fläche, Decks auch unten/seitlich.
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
  Geneigte Bänder brauchen gespeicherten Zellkontakt. GPU-Lattice, Kanten/Ecken/konforme LOD-Ränder prüfen.
  Kontakt-/Höhenfehler vor Verfeinerung beheben; 2336 wählt Arbeit, 2280 versioniert native Rezepte/Cache.

## Verfahren und Abnahme
[Street Modeling, SIGGRAPH 2008](../doc/references/infrastructure/siggraph/2008-interactive-procedural-street-modeling.pdf): Graph vor Geometrie; [Eurographics 2010](../doc/references/infrastructure/eurographics/2010-procedural-generation-of-roads.pdf): Profile, Bauwerke, Terrainkontakte.
[SUMO/CARLA/OSM2World](../doc/references/infrastructure/README.md): Netzfunktion und isolierte Meshes; keine Vermessungstreue.
[2D-Kacheln](../test/experiments/infrastructure_tiles.py): Feldkirchs 19.230 Abschnitte → 20.094 Teile/133 ENU-Kacheln bei 256 m; Bedarf: 2.711 statt 19.230 Kandidaten, 1.294 gleiche Treffer. Geografischer/native Vertrag offen.
[Kern-/Rampenrezepte](../test/experiments/infrastructure_recipes.py), [gemeinsame Ports](../test/experiments/infrastructure_ported_junction.py): Flächen vor Mesh; feste Außenhöhen, starre Kerne/quintische Ansätze, Flächenneigung/Freiraum, LP-Vergleich.
Topologische Unter-/Zielhüllen liefern lokalen Minimaxbedarf direkt in O(V+E); Gesamtgraph, horizontale Krümmung und gemeinsame native Randunterteilung offen.
[Höhenplanung](../test/experiments/road_profile_envelopes.py), [Landekern](../test/experiments/road_landing_topology.py): native Höhenränder/Portoffsets, Schnittvorrang, Höhenbindung/kurze Anschlüsse; kein vollständiger C1-/Breitenbeweis.
[Übergänge](../test/experiments/earthwork_transition.py), [Freiraum](../test/experiments/clearance_aprons.py), [Kreuzungsflächen](../test/experiments/road_junction_coverage.py), [Rasterkontakt](../test/experiments/road_terrain_contact.py): echte Inputs, gleiche Qualität.
Durchgehende reale Anschlüsse ohne Risse, verlorene Ebenen, Wasser-/Geländewände oder unbefahrbare Profile.
Auch ohne Terrain/Gebäude lückenlos; 2D-Review zeigt Tunnel gestrichelt, Brücken blau, Ebenen separat. Quellenabdeckung, Netzfunktion, Bild/Kosten prüfen.
Nahbewegung/Rundumdrehung; Straßenqualität und Tokyo/Central Park erhalten. Place-PNGs zeigen Gewinn im selben Profilbudget; kritische Fehler bleiben rot.
