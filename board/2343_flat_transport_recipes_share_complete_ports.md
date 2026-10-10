Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2342
Area: generators, world
Tags: infrastructure, solver, recipes

# Flat transport recipes share complete junction ports

## Ergebnis und fehlender Vertrag
2342 liefert den quellbezogenen ebenen Graph samt Nutzungsbilanz.
Aus ihm entstehen einfache zusammenhängende Bänder, Kurven und Anschlussflächen ohne Terrain.
Besitzer: generators/road. Ergebnis ist der native Bauteilplan für 2281, noch ohne Höhen/Ebenen.
[Python-Flächenplan](../test/experiments/infrastructure_flat_surface_network.py) teilt gemeinsame
Randsegmente im selben Mesh; die gewählten Ebenen-0-Fenster bestehen die Flächen-/Kantenprüfung.
Verkehrsverbindungen auf realen Netzen, Kollisionsprodukt und Budgetgrenzen bleiben offen;
gegliederte Flächen allein liefern noch kein plausibles fertiges Netz. Kleine Beispiele sind renderbar.
[Bordbögen](../test/experiments/infrastructure_flat_corners.py) runden Innenkanten tangential
mit konstruktiver Überlappung nach innen. Ungültige Bögen entfallen lokal samt Grund.
[Direkte Querschnitte](../test/experiments/infrastructure_flat_cuts.py) teilen die tatsächliche
Verkehrsfläche per Randschnitt/Suchindex. Kollidierende Schnitte rücken begrenzt nach außen;
kurze Zwischenabschnitte gehen in den gemeinsamen Anschluss ein. Restketten kürzer als ihre eigene
Breite bleiben ebenfalls im Knoten: sie tragen keinen brauchbaren eigenständigen Korridor.
Ein Fahrzeugprofil ergänzt die Schnittsuche: jeder öffentliche Auto-Achsenkontakt muss die
Karosserie auf der unveränderten Motorfläche tragen, auch bei Schnitten fremder Modi.
Unbrauchbare Schnitte rücken innerhalb derselben begrenzten Suche nach außen oder entfallen
mit Grund. Das ändert nur die Teilung, weder Straßenfläche noch Quellen/Zugänge.
[Schnittprüfung](../test/experiments/infrastructure_flat_ports.py) prüft gerade vollständige gemeinsame
Randsegmente unabhängig von der Flächenabdeckung; die drei Ebenen-0-Fenster bestehen beide Prüfungen.
[Port-Fahrspuren](../test/experiments/infrastructure_port_lane_experiment.py) übernehmen öffentliche
Richtung/Zugänge auf die tatsächlichen gemeinsamen Querschnitte. Getrennte Anschlüsse derselben
Bauteile und mehrfache Achsenkreuzungen bleiben getrennt. Schmale Zweirichtungsstrecken teilen
eine Spur; Fahrseite und Fahrzeugbreite steuern die Platzierung. Verkehrsregelung bleibt offen.
[Fahrkurven](../test/experiments/infrastructure_junction_turn_experiment.py) verwenden Klothoiden
mit optionalem Kreisbogen oder asymmetrische G2-Posekurven mit polynomialem Tangentenwinkel.
Endkrümmung null, Krümmungsextrema analytisch; Radius/Breite kommen aus dem Fahrzeugprofil.
Klothoiden werden vor der teuren Karosserieprüfung nach tatsächlicher Kurvenlänge sortiert;
der erste gültige Kandidat liefert dasselbe Minimum wie die vollständige Kandidatenprüfung.
[Quellrouten](../test/experiments/infrastructure_route_turns.py) verbinden Portkontakte über
gerichtete kürzeste Wege. Ein begrenzter azyklischer Posengraph überspringt unnötige Quellknicke;
jede Verbindung prüft dieselben Kurvenrezepte und die vollständige Karosserie.
Höchstens 64 Ecken, drei Posen je Ecke und acht Nachfolger je Pose begrenzen die lokale Suche.
Identische Vereinfachungen entfallen; Posenpaare werden bei fester Fläche/Fahrzeug/Toleranz einmal
geprüft. Eine Untergrenze aus Segmentanzahl und Sehnenlänge verwirft aussichtslose Kandidaten.
Versetzte Portposen setzen feste Anfangs-/Endpositionen und Tangenten des Posengraphen; die
Quellachse gibt innere Kandidaten vor. Vorwärtsrichtung bleibt verbindlich, exakte Parallelität nicht.
Ragt eine Portkarosserie aus der Motorfläche, prüft das Rezept 61 Richtungen innerhalb ±30° und
wählt den größten Randabstand bei unveränderter Position. Alle Verbindungen teilen diese feste
Portausrichtung; Nachbarkorridore müssen denselben gespeicherten Rahmen übernehmen.
[Gemeinsame Rahmenwahl](../test/experiments/infrastructure_shared_frame_experiment.py) ergänzt die
statische Passung: Richtungen in 5°-Schritten innerhalb ±30° zur Quelle, höchstens 64 Paare je Bauteil.
Sie prüft die volle Karosserie auf beiden Nachbarflächen, dann die Fahrkurve und alle betroffenen
Nachbarfahrten. Nur der angefragte Fahrgewinn ohne Verlust oder Änderung vorhandener Anfragen zählt.
Position, Quellenkontakt und Richtung bleiben fest; alle Fahrten teilen denselben gespeicherten Rahmen.
Erfolglose Kandidaten ändern nichts. Konfliktfreie Fahrspuren und weitere Rahmenrezepte bleiben offen.
[Fahrzeughülle](../test/experiments/infrastructure_vehicle_sweep.py): bei Körperradius r und
maximaler Krümmung k sowie Krümmungsänderung k' gilt |q''(s)| ≤ A = k+r*(|k'|+k²).
Analytische Kurven prüfen die konvexe Hülle zweier Endkarosserien mit Randreserve A*ds²/8.
Sie umfasst alle linearen Eckinterpolationen; die Reserve umfasst deren kontinuierliche Abweichung.
Enthaltene Endposen allein genügen nicht: die ganze Hülle muss auf der Motorfläche liegen,
mit Reserve zu Außenrändern und Löchern. Nur unklare Intervalle werden halbiert; höchstens 4096
tatsächlich geprüfte Karosserieposen, keine Suchschleife ohne Budget. Gerade Ansätze sind eigene
Teilkurven; G1-Krümmungssprünge an ihren Enden überschreiten keine gemeinsame Ableitungsgrenze.
Gerade Translationen nutzen den exakt extrudierten Körper. Quintische Quellrouten nutzen ebenfalls
Hüllen: Bézier-Ableitungen begrenzen Geschwindigkeit v, Beschleunigung a und dritte Ableitung j
über ihrem lokalen Parameter u. Positive Mindestgeschwindigkeit sichert θ'=cross(v,a)/|v|² und
θ''=(cross(v,j)-2*θ'*dot(v,a))/|v|². Damit gilt |q''(u)| ≤ a_max+r*(|θ''|_max+|θ'|_max²).
Linienpunkte entstehen separat nach a_max*du²/8; Karosserieprüfung erzeugt kein dichtes Linienmesh.
[Hüllenprüfung](../test/experiments/infrastructure_swept_hull_experiment.py) vergleicht Eckbahnen,
unabhängige numerische Integration, versteckte Löcher und enge Kurven bei festem Arbeitsbudget.
Wege/private Zufahrten sind kein Ausweichraum. Der gemeinsame Mesh begrenzt öffentliche Motorfläche.
Gerichtete Erreichbarkeit allein genügt nicht: bei Einbahnabschnitten muss die Kurve die
Windungszahlen der gewählten Quellroute um Verkehrsinseln erhalten. Der Vergleich schließt
Portversätze über den gemeinsamen Querschnitt. Eine durch Karosseriebreite eingeschränkte
Routenfläche wurde verworfen: legitime Abbiegekurven dürfen benachbarte öffentliche Fläche nutzen.
Ein Gegenfall prüft verbotene Abkürzung und erlaubte Umfahrung; überlappende Fahrspuren und
alternative gerichtete Routen brauchen weiterhin eine verbindliche Richtungszuordnung.
Windungszahlen sind eine notwendige topologische Bedingung, kein vollständiger Verkehrsflussbeweis.
[Rezeptprüfungen](../test/experiments/infrastructure_route_experiment.py) umfassen Inselumfahrung,
verdeckte Hindernisse zwischen Endposen und identische Ergebnisse bei wiederverwendeten Verbindungen.
[Quintische Routen](../test/experiments/infrastructure_spline_routes.py) laufen vor der Posekette:
SciPy interpoliert höchstens 66 Stützpunkte mit festen Endtangenten und Endkrümmung null.
Abstände von 1/2/3/4 Fahrzeug-Mindestradien und die unveränderte Quelle bilden fünf Kandidaten.
Bézier-Hüllen begrenzen Geschwindigkeit und Krümmung nach fester Unterteilung; höchstens 4096
Karosserieposen sichern die gesamte Fahrt. Im Inneren entfällt der Zwang zur Krümmung null.
[Unabhängige Prüfung](../test/experiments/infrastructure_spline_experiment.py) deckt Hüllen,
Endbedingungen, Hindernisse und Budgets ab; erfolglose Splines fallen auf die Posekette zurück.
[Korridorfahrten](../test/experiments/infrastructure_corridor_experiment.py) übernehmen unverändert
die Portposen beider Nachbarknoten; gerichtete Modulrouten und dieselbe Karosserieprüfung verbinden sie.
Nicht durchfahrbare Auto-Anschlüsse erscheinen separat in der Nutzungsbilanz. Ungültige feste
Endkarosserien werden einmal vor der Kurvensuche geprüft; sie bleiben explizit ungelöst.
Zweipunktige Quellachsen dürfen bei versetzten Ports ebenfalls quintische Verbindungen liefern.
Bei erfolgloser Interpolation glättet ein begrenztes Zusatzrezept die Führungsachse:
Abweichung zur Quelle plus quadratische zweite Differenzen, feste Endpunkte, positiv definites
Fünfdiagonalsystem in O(N). Fläche, Ports, Krümmungs- und Karosserieprüfung bleiben unverändert.
Jeder Spline-Kandidat muss die Inselwindung seiner Quelle erhalten; sonst läuft die Suche weiter.
Offen: durchgehende Spurzuordnung, ungültige feste Portposen, enge Kreisverkehre, Verkehrsregelung,
vollständige Netze und tragbare Kosten. Erfolgreiche Einzelkurven sind keine fertige Verkehrssimulation.
[Road synthesis, Eurographics 2010](../doc/references/infrastructure/eurographics/2010-procedural-generation-of-roads.pdf).

## Umsetzung und Abnahme
1. Gerade/Ende/Kurve, T/X und Einfädelung als kleine Rezepte mit gemeinsamen vollständigen Querschnitten.
2. Überlappende kurze Knoten zu einem Bauteil vereinigen; Außenports gemeinsam speichern.
   Kurven bei zu engem Radius vereinfachen/verschieben, dann messen; ungelöste lokale Teile ausweisen.
3. Mesh, Kontakt und Kollision aus demselben Bauteilplan; Fläche einmal besitzen und triangulieren.
4. Kleine Fälle, dann alle verwendeten Bauteile der Wiener/Zürcher/Basler Fenster rendern und öffnen.
5. Weltweit hunderte bis tausende Beispiele nach Topologie, Klassen und gelieferten Tags untersuchen.
   [Survey](../test/experiments/infrastructure_world_survey.py) nutzt gespeicherte Originalkacheln,
   schreibt kompakte Messwerte und nachstellbare Defizite; Bilder nur nach Auswahl gemäß AGENTS.
   Nahansichten müssen Anschlüsse, Fahrkurven und lokale Auslassungen plausibel zeigen.
   Native Übertragung erst nach annähernd fehlerfreien vollständigen 2D-/2,5D-Netzen gemäß Goal.
Gemeinsame Portpositionen/Normalen, plausible Breiten/Kurven, keine Risse/Faltungen/inneren Doppelstücke.
Verwendungsgrad und Kosten gehören zur Qualität; ungültige Teile bleiben außerhalb des Plans.
Überflüssige Zweierknoten ohne Richtungs-/Breitenwechsel erhalten kein eigenes Modul.
Ruhige Kurven und zusammenhängende Knotenformen im Bild entscheiden; Originalknicke dürfen weichen.
[Gemeinsame Ports](../test/experiments/infrastructure_ported_junction.py),
[OSM2World/SUMO/CARLA](../doc/references/infrastructure/README.md).
