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
Alle berührten Bauteile dreier 1-km-Radiusfenster prüfen/rendern öffentliche Autofahrten.
Kurvenräume schließen zusätzliche Netzbrüche im Wiener/Basler Motorfenster; vier weitere Zürcher Trennungen, gerichtete Fahrten, andere Modi und Kosten bleiben offen.
[Bordbögen](../test/experiments/infrastructure_flat_corners.py) runden Innenkanten tangential
mit konstruktiver Überlappung nach innen. Ungültige Bögen entfallen lokal samt Grund.
[Direkte Querschnitte](../test/experiments/infrastructure_flat_cuts.py) teilen die tatsächliche
Verkehrsfläche per Randschnitt/Suchindex. Kollidierende Schnitte rücken begrenzt nach außen;
Restketten kürzer als ihre eigene Breite bleiben im gemeinsamen Anschluss.
Öffentliche Auto-Achsenkontakte müssen die Karosserie auf der geplanten Motorfläche tragen,
auch bei fremden Modi. Ungültige Schnitte rücken nach außen oder entfallen mit Grund; Quellen bleiben fest.
[Schnittprüfung](../test/experiments/infrastructure_flat_ports.py) prüft gerade vollständige gemeinsame
Randsegmente unabhängig von der Flächenabdeckung; die drei Ebenen-0-Fenster bestehen beide Prüfungen.
[Port-Fahrspuren](../test/experiments/infrastructure_port_lane_experiment.py) übernehmen öffentliche
Richtung/Zugänge auf die tatsächlichen gemeinsamen Querschnitte. Getrennte Anschlüsse derselben
Bauteile und mehrfache Achsenkreuzungen bleiben getrennt. Schmale Zweirichtungsstrecken teilen
eine Spur; Fahrseite und Fahrzeugbreite steuern die Platzierung. Verkehrsregelung bleibt offen.
[Fahrkurven](../test/experiments/infrastructure_junction_turn_experiment.py) verwenden Klothoiden
mit optionalem Kreisbogen oder asymmetrische G2-Posekurven mit polynomialem Tangentenwinkel.
Endkrümmung null; Radius/Breite aus dem Fahrzeugprofil, analytische Extrema. Länge durch Arbeitsbudget/Fläche begrenzen, nicht pauschal durch Sehnenverhältnisse.
[Quellrouten](../test/experiments/infrastructure_route_turns.py) verbinden Portkontakte über
gerichtete kürzeste Wege. Ein begrenzter azyklischer Posengraph überspringt unnötige Quellknicke;
jede Verbindung prüft dieselben Kurvenrezepte und die vollständige Karosserie.
Höchstens 64 Ecken, drei Posen je Ecke, acht Nachfolger je Pose; Straßenräume verteilen sie auf Nähe/Ferne/Ziel. Inselumfahrung schon je Verbindung erhalten; identische Vereinfachungen entfallen.
Posenpaare einmal je Fläche/Fahrzeug/Toleranz prüfen; Untergrenzen aus Segmentzahl/Sehne begrenzen die Suche.
Versetzte Portposen setzen Endpositionen/Tangenten; die Quellachse gibt innere Kandidaten vor.
Ungültige Posen prüfen 61 Vorwärtsrichtungen innerhalb ±30°.
[Gemeinsame Rahmenwahl](../test/experiments/infrastructure_shared_frame_experiment.py) ergänzt diese Passung:
5°-Richtungen innerhalb ±30°, dann 17 Positionen; getrennte Fahrbahnhälften bzw. schmale gemeinsame Spur.
Höchstens 64 Paare je Bauteil; Posedomänen wiederverwenden. Kosten: Versatz + Mindestradius × Winkeländerung.
Karosserie auf beiden Flächen, Fahrkurve und Nachbarfahrten prüfen; nur angefragter Gewinn ohne Anfragen-/Fahrtenverlust.
Quellenkontakt/Richtung bleiben fest; alle Nachbarn teilen die Pose.
[Fahrzeughülle](../test/experiments/infrastructure_vehicle_sweep.py): bei Körperradius r und
maximaler Krümmung k sowie Krümmungsänderung k' gilt |q''(s)| ≤ A = k+r*(|k'|+k²).
Analytische Kurven prüfen die konvexe Hülle zweier Endkarosserien mit Randreserve A*ds²/8.
Die ganze Hülle mit Randreserve muss auf der Motorfläche liegen.
Nur unklare Intervalle werden halbiert; höchstens 4096 tatsächlich geprüfte Karosserieposen.
Gerade Ansätze sind eigene Teilkurven; ihre G1-Krümmungssprünge teilen keine Ableitungsgrenze.
Gerade Translationen nutzen den exakt extrudierten Körper. Quintische Quellrouten nutzen ebenfalls
Hüllen: Bézier-Ableitungen begrenzen Geschwindigkeit v, Beschleunigung a und dritte Ableitung j
über ihrem lokalen Parameter u. Positive Mindestgeschwindigkeit sichert θ'=cross(v,a)/|v|² und
θ''=(cross(v,j)-2*θ'*dot(v,a))/|v|². Damit gilt |q''(u)| ≤ a_max+r*(|θ''|_max+|θ'|_max²).
Linienpunkte entstehen separat nach a_max*du²/8; Karosserieprüfung erzeugt kein dichtes Linienmesh.
Wege/private Zufahrten sind kein Ausweichraum. Die geplante öffentliche Fläche hat ein eigenes Mesh.
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
Höchstens 66 Punkte, feste Endtangenten/Endkrümmung null; innen darf die Krümmung variieren.
Abstände von 1/2/3/4 Fahrzeug-Mindestradien und die Quelle liefern fünf Kandidaten.
Bézier-Hüllen begrenzen Geschwindigkeit/Krümmung; höchstens 4096 Karosserieposen sichern die Fahrt.
[Korridorfahrten](../test/experiments/infrastructure_corridor_experiment.py) übernehmen unverändert
die Portposen beider Nachbarknoten; gerichtete Modulrouten und dieselbe Karosserieprüfung verbinden sie.
Nicht durchfahrbare Auto-Anschlüsse bleiben in der Nutzungsbilanz; ungültige feste Endkarosserien
werden einmal vor der Kurvensuche geprüft und bleiben ungelöst.
Zweipunktige Quellachsen dürfen bei versetzten Ports ebenfalls quintische Verbindungen liefern.
Nach erfolgloser Interpolation: Quellabweichung + quadratische zweite Differenzen, feste Endpunkte;
positiv definites Fünfdiagonalsystem in O(N). Fläche, Ports und Karosserie-/Radiusprüfung bleiben fest.
Jeder Spline-Kandidat muss die Inselwindung seiner Quelle erhalten; sonst läuft die Suche weiter.
[Lokale Routenpassung](../test/experiments/infrastructure_hermite_route_fitting.py) folgt auf die Rahmenwahl.
Quintische Hermite-Abschnitte teilen Position/Tangente/Beschleunigung; feste Ports, Endkrümmung null.
Nachbarabhängigkeiten ergeben eine dünn besetzte Jacobi-Matrix. Höchstens 18 Knoten und 40 Solver-
Auswertungen je Versuch; insgesamt 1024 tatsächliche Residuenaufrufe je Verbindung, inklusive Ableitungen.
Quellabweichung/Karosserieabstand/Bernstein-Schranken optimieren; dieselbe Hüllen-/Radius-/Windungsprüfung entscheidet.
[Punktabstände](../test/experiments/infrastructure_surface_clearance.py): Randindex zuerst; nur Randnähe/außen messen.
Nach Hüllenablehnung Randviertel/Körpermitte als zusätzliche Optimierungspunkte; erst nach beiden Grundversuchen.
Dasselbe 1024-Residuenbudget und dieselbe vollständige Hüllenprüfung; Eckpunkte allein können Hindernisse übersehen.
Die Passung ergänzt nur fehlgeschlagene Fahrten bei festen Straßenflächen/Posen.
[Kurven-Straßenräume](../test/experiments/infrastructure_source_roadbeds.py) ergänzen fehlenden Fahrraum durch direkte Kurven oder quellfolgende G2-Ketten und vollständige Karosseriehüllen.
Maximale Aufweitung: sqrt((R+w/2)^2+L^2)−R; R Mindestradius, w Breite, L Längsreichweite. Quellkette zuerst mit w/2 Reserve suchen; nur bei Ablehnung bis zur Eckreichweite erweitern. Reservationen räumlich eingrenzen, Suche nur außerhalb vorhandener Fahrfläche schützen; endgültige Aufweitung gegen alle prüfen.
Gebäude/andere Verkehrsflächen sind Reservierungen. Offene Blockränder dürfen kohärent weichen;
Geschlossene Freiräume sind tatsächlich freie Komplementflächen, auch bei verschachtelten Inseln; eindeutige Zuordnung durch positive Flächenüberlappung und gerichtete Inselumfahrung bleiben erhalten.
Volle Hüllen gemeinsam bilden, keine Differenzfragmente rekonstruieren. Konstruktionsreserve vorsehen; alle Stücke teilen 4096 Prüfposen. Mesh/Fläche teilen Präzision; Außenports/Anfragen/Fahrten erhalten. Fahrflächen sind rein polygonal; Schnittlinien tragen keinen Fahrraum.
[Gemeinsame Schnittverschiebung](../test/experiments/infrastructure_cut_setbacks.py) erweitert den Anschluss
innerhalb der unveränderten Gesamtfläche: lokale Straßenstücke wechseln Besitzer, auch bei Schleifen mit mehreren Anschlüssen. Binäre Präzision erhält volle gemeinsame Querschnitte entlang derselben Quellachse.
Drei Kandidaten: 1/2/3 Mindestradien; beide Nachbarn neu lösen. Quellen-/Außenkontakte, Anfragen und gültige Fahrten erhalten; nur bei gezieltem Gewinn übernehmen. Schnitte ändern Besitz, nicht Gesamtfläche.
[Gekoppelte Schnitt-/Fahrraumpassung](../test/experiments/infrastructure_coupled_roadbed_experiment.py) löst Knoten samt Nachbarstraßen. Anfragen, Freiräume und Schnitte am gemeinsamen Dreiecksmesh prüfen; Außenrahmen bleiben fest.
Zürich 102 trägt samt Nachbarstraßen 12/12 Fahrten und schließt eine Fenster-Netztrennung; vollständige Netze bleiben offen.
[Fensterprüfung](../test/experiments/infrastructure_roadbed_window_experiment.py) übernimmt Fahrraum-/Schnittprodukte samt festen Posen.
Fahrten am tatsächlichen Mesh prüfen; Anfragen anhand Quellkontakten zuordnen und alte Gewinne erhalten. Unverträgliche Gruppen lokal ablehnen, gültigen Bestand behalten.
[Portgraph](../test/experiments/infrastructure_route_connectivity.py): angefragte/geprüfte Verbindungen, schwache
und starke Komponenten vergleichen. Netztrennende Auslassungen zuerst; Einzelkurvenquoten genügen nicht.
Offen: Spurzuordnung/Verkehrsregelung, feste ungültige Ports, vollständige Netze aller Modi und tragbare Kosten.

## Umsetzung und Abnahme
1. Gerade/Ende/Kurve, T/X und Einfädelung als kleine Rezepte mit gemeinsamen vollständigen Querschnitten.
2. Fahrflächenbedarf aus Quellrouten/Portnähe bestimmen, unabhängig vom Mesh-Batch; kurze Fahrknoten gemeinsam lösen.
   Mehrfachschnitte erhalten; lange Fahrten aus lokalen Stücken bauen. Große Infrastrukturflächen bestimmen keine Solverdomäne.
3. Mesh, Kontakt und Kollision aus demselben Bauteilplan; Fläche einmal besitzen und triangulieren.
   Freiräume/volle Karosserien am zusammengefügten Mesh prüfen; Einzelkandidaten sind kein Netzbeleg.
4. Kleine Fälle, dann alle verwendeten Bauteile der Wiener/Zürcher/Basler Fenster rendern und öffnen.
5. Weltweit hunderte bis tausende Fälle: [Survey](../test/experiments/infrastructure_world_survey.py),
   Originalkacheln, Nutzungsbilanz/Kosten und ausgewählte Nahansichten gemäß AGENTS.
   Native Übertragung erst nach annähernd fehlerfreien vollständigen 2D-/2,5D-Netzen gemäß Goal.
Gemeinsame Portpositionen/Normalen, plausible Breiten/Kurven, keine Risse/Faltungen/inneren Doppelstücke.
Verwendungsgrad und Kosten gehören zur Qualität; ungültige Teile bleiben außerhalb des Plans.
Überflüssige Zweierknoten ohne Richtungs-/Breitenwechsel erhalten kein eigenes Modul.
Ruhige Kurven und zusammenhängende Knotenformen im Bild entscheiden; Originalknicke dürfen weichen.
[Gemeinsame Ports](../test/experiments/infrastructure_ported_junction.py), [OSM2World/SUMO/CARLA](../doc/references/infrastructure/README.md).
