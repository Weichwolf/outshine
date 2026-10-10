Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2169
Depends: 2345
Area: generators, world, engine
Tags: roads, bridges, tunnels, topology

# Roads, rails and paths form a continuous usable transport network

## Ergebnis und Ist
Plausible Straßen, Geh-/Radwege, Bahn/Tram, Brücken und Tunnel bilden ein funktionales Netz.
Vorrang: Wiener Anschlüsse/Brücken; danach Feldkirch, Basel Badischer Bahnhof, Zürich HB und Häfen.
Corridors liefert Geometrie/Kontakte; gemeinsame native Höhenplanung bindet Anschlüsse, Sekanten
und Freiraum vor Geometrie/Terrain; Sync/Async teilen sie. Anschlussflächen übertragen nativ
Längs-/Querneigung sowie vier gemeinsame Randpunkte/Normalen einschließlich Schultern/Unterseite.
Der Höhensolver aktualisiert die Ports; überlappende Knotenflächen bleiben als gemeinsame Bauteile offen.
Der 2D-Endpunktplan trennt Position, Ebene und rekonstruierte Verbindungen. Kreuzungen werden
jeweils einmal gespeichert; Grid-Zellen referenzieren sie. Gestapelte Decks behalten eigene Ports.
Doppelte MVT-Linien erzeugen keine zusätzlichen Knotenflächen; Bodenwege liefern Kernkontakte.
Offen: geografische Kachelports/Cacheplan, Ebenenübergänge, C1, Breitenfreiraum, Tunnel/Portale, Tragwerk, Treppen, Bahnsteige, Piers, Seilbahnen.
Der analytische RoadAlignment-/Surface-Pfad verlangt benannte Routen; auf denselben Plan vereinigen.
Zürich HB lädt wieder: gemeinsame Brückenebenen teilen eine physische Höhe, Verkehrsports bleiben
getrennt; kurze Kreuzungen im gemeinsamen Landekern erzeugen keinen falschen Unterpass.
Bahnhofsanschlüsse bleiben fehlerhaft. [2D-Flächenversuch](../test/experiments/infrastructure_network.py)
vereinigt ganze Wiener/Zürcher/Basler Quellfenster; funktionale Ports/Kurven sind noch nicht gelöst.
## Nächste Lieferung
2342 → 2343 → 2344 → 2345 liefern Graph, ebene Bauteile, Profile und Ebenenrezepte.
2345 liefert den geprüften gemeinsamen 2D-/2,5D-Bauteilplan für die native Integration.
Die Python-Netze werden vorher vollständig visuell geprüft; jeder native Schritt braucht Place-Gewinn.
OSM/DEM geben Verteilung und Bezug, keine unveränderlichen Baupläne. Plausibilität, Ästhetik,
Funktion und Kosten entscheiden; der Solver wählt irgendeine gültige kohärente Interpretation.
Feste Budgets, wenige Rezepte und lokale Reparaturen begrenzen die Arbeit. Nicht verwendete
Quellen mit ID/Grund erhalten; Anzahl/Netzlänge/Fläche des verwendeten Anteils ausweisen.
Unlösbare eigene Bedingungen erfordern ein anderes Rezept, neue Lage oder begrenzte Auslassung.
Der Versuch mit festen 10-m-Höhenzellen ist als Verkehrsflächenmodell verworfen:
Rasterecken erzeugten künstliche Anschlüsse/Freiraumkonflikte. Vektorgraph und echte Bauteilports entscheiden.
Keine native Gate-Arbeit während der Verfahrenssuche; erfolgreiche Python-Schritte zuerst liefern.

## 2D-Plan und Ebenen
- OSM-Adapter normalisiert gelieferte Klassen, Breite/Spuren, Oberfläche, Brücke/Tunnel/Ebene;
  world/navigation hält das native logische Netz. generators/road besitzt Profile/Geometrie.
  Terrain konsumiert Kontakte. Bedarf: 2336; Produkte/Cache: 2280; Gebäude: 2173; Pegel: 2145.
- Geografisch stabile GeoCellId-Zellen indexieren Vektorpläne; Straßenachsen bleiben kontinuierlich. Gemeinsame Randports:
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
  Gemeinsame Vierpunkt-Querschnitte einschließlich Schultern/Bord: Straße und Knoten speichern
  dieselben Positionen/Normalen; beschränkte Triangulierung erhält die Randstützpunkte. Komplexe Knoten/Brücken
  formstabil positionieren; überlappende kurze Knoten zu einem Bauteil vereinigen, statt Ports nach innen zu kürzen.
  Verletzte Krümmung/Steigung/Freiraum erfordert mehr Übergangsraum, neue Lage oder anderes Rezept.
  Wiener/Zürcher Inputs: Ebenen bei gleicher Netzgröße variieren, Port-/Konfliktkopplung separat;
  Solverzeit/Speicher samt Wachstum messen; drei lokale Ebenen sind Hypothese, keine Budgetfreigabe.
- Fehlendes Rezept: Bauform auf ein vorhandenes vereinfachen; Verkehrsverbindungen, erforderliche
  Ebenentrennung und Freiräume erhalten. Nicht lösbare lokale Bauteile mit betroffenen Quell-IDs ausweisen.
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
[Kern-/Rampenrezepte](../test/experiments/infrastructure_recipes.py), [gemeinsame Ports](../test/experiments/infrastructure_ported_junction.py): Flächen vor Mesh; feste Außenhöhen, starre Kerne/quintische Ansätze, Flächenneigung/Freiraum, LP-Vergleich.
[Höhenplanung](../test/experiments/road_profile_envelopes.py), [Landekern](../test/experiments/road_landing_topology.py): native Höhenränder/Portoffsets, Schnittvorrang, Höhenbindung/kurze Anschlüsse; kein vollständiger C1-/Breitenbeweis.
[Übergänge](../test/experiments/earthwork_transition.py), [Freiraum](../test/experiments/clearance_aprons.py), [Kreuzungsflächen](../test/experiments/road_junction_coverage.py), [Rasterkontakt](../test/experiments/road_terrain_contact.py): echte Inputs, gleiche Qualität.
Durchgehende reale Anschlüsse ohne Risse, verlorene Ebenen, Wasser-/Geländewände oder unbefahrbare Profile.
Verwendetes Netz auch ohne Terrain/Gebäude lückenlos; 2D-Review zeigt Tunnel gestrichelt,
Brücken blau, Ebenen separat. Nicht verwendete Quellen/Gründe, Netzfunktion, Bild/Kosten prüfen.
Nahbewegung/Rundumdrehung; Straßenqualität und Tokyo/Central Park erhalten. Place-PNGs zeigen Gewinn im selben Profilbudget; kritische Fehler bleiben rot.
