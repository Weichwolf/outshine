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
Aus dem Quellgraph und der Nutzungsbilanz (2342) entstehen zusammenhängende Bänder/Kurven/Anschlüsse.
Besitzer: generators/road; nativer Bauteilplan für 2281, ohne Höhen/Ebenen.
[Python-Flächenplan](../test/experiments/infrastructure/infrastructure_flat_surface_network.py) teilt gemeinsame
Mesh-Ränder; alle gewählten Ebenen-0-Fenster bestehen Flächen-/Kantenprüfung.
Keine zusätzlichen schwachen Netztrennungen; 8611/8679 Autofahrten. Zürich ohne starke Netztrennung; Modi/Verkehrsfluss/Kosten offen.
[Bordbögen](../test/experiments/infrastructure/infrastructure_flat_corners.py) runden Innenkanten tangential
mit konstruktiver Überlappung nach innen. Ungültige Bögen entfallen lokal samt Grund.
[Direkte Querschnitte](../test/experiments/infrastructure/infrastructure_flat_cuts.py) teilen die tatsächliche
Verkehrsfläche per Randschnitt/Suchindex. Kollidierende Schnitte rücken begrenzt nach außen;
Restketten kürzer als ihre eigene Breite bleiben im gemeinsamen Anschluss.
Öffentliche Auto-Achsenkontakte müssen die Karosserie auf der geplanten Motorfläche tragen,
auch bei fremden Modi. Ungültige Schnitte rücken nach außen oder entfallen mit Grund; Quellen bleiben fest.
[Schnittprüfung](../test/experiments/infrastructure/infrastructure_flat_ports.py) prüft gerade vollständige gemeinsame
Randsegmente unabhängig von der Flächenabdeckung; die drei Ebenen-0-Fenster bestehen beide Prüfungen.
[Port-Fahrspuren](../test/experiments/infrastructure/infrastructure_port_lane_experiment.py) übernehmen öffentliche
Richtung/Zugänge auf die tatsächlichen gemeinsamen Querschnitte. Getrennte Anschlüsse derselben
Bauteile und mehrfache Achsenkreuzungen bleiben getrennt. Schmale Zweirichtungsstrecken teilen
eine Spur; Fahrseite und Fahrzeugbreite steuern die Platzierung. Verkehrsregelung bleibt offen.
[Fahrkurven](../test/experiments/infrastructure/infrastructure_junction_turn_experiment.py) verwenden Klothoiden
mit optionalem Kreisbogen oder asymmetrische G2-Posekurven mit polynomialem Tangentenwinkel.
Endkrümmung null; Radius/Breite aus dem Fahrzeugprofil, analytische Extrema. Länge durch Arbeitsbudget/Fläche begrenzen, nicht pauschal durch Sehnenverhältnisse.
[Quellrouten](../test/experiments/infrastructure/infrastructure_route_turns.py) verbinden Portkontakte über
gerichtete kürzeste Wege. Ein begrenzter azyklischer Posengraph überspringt unnötige Quellknicke;
jede Verbindung prüft dieselben Kurvenrezepte und die vollständige Karosserie.
64 Ecken, drei Posen/Ecke, acht Nachfolger/Pose; Nähe/Ferne/Ziel abdecken. Inselumfahrung je Verbindung erhalten; identische Vereinfachungen entfallen.
Posenpaare je Fläche/Fahrzeug/Toleranz wiederverwenden; Segmentzahl/Sehne begrenzen die Suche.
Versetzte Portposen setzen Endpositionen/Tangenten; die Quellachse gibt innere Kandidaten vor.
Ungültige Posen prüfen 61 Vorwärtsrichtungen innerhalb ±30°.
[Gemeinsame Rahmenwahl](../test/experiments/infrastructure/infrastructure_shared_frame_experiment.py) ergänzt diese Passung:
5°-Richtungen innerhalb ±30°, dann 17 Positionen; getrennte Fahrbahnhälften bzw. schmale gemeinsame Spur.
Höchstens 64 teure Bauteilversuche; Posedomänen wiederverwenden. Kosten: Versatz + Mindestradius × Winkeländerung.
Karosserie auf beiden Flächen, Fahrkurve und Nachbarfahrten prüfen; nur angefragter Gewinn ohne Anfragen-/Fahrtenverlust.
Quellenkontakt/Richtung bleiben fest; alle Nachbarn teilen die Pose.
[Fahrzeughülle](../test/experiments/infrastructure/infrastructure_vehicle_sweep.py): bei Körperradius r und
maximaler Krümmung k sowie Krümmungsänderung k' gilt |q''(s)| ≤ A = k+r*(|k'|+k²).
Endkarosserie-Hülle + A*ds²/8 muss auf der Motorfläche liegen; unklare Intervalle halbieren, höchstens 4096 Posen.
G1-Sprünge teilen keine Ableitungsgrenze; Geraden extrudieren den Körper.
Bézier-Hüllen begrenzen v, a, j und Winkeldifferentiale bei positiver Mindestgeschwindigkeit:
|q''(u)| ≤ a_max+r*(|θ''|_max+|θ'|_max²).
Linienpunkte entstehen separat nach a_max*du²/8; Karosserieprüfung erzeugt kein dichtes Linienmesh.
Wege/private Zufahrten sind kein Ausweichraum. Die geplante öffentliche Fläche hat ein eigenes Mesh.
Einbahnkurven erhalten Quell-Windungszahlen um Verkehrsinseln, inklusive Portversätzen
über den Querschnitt. Legitime Abbiegekurven dürfen benachbarte öffentliche Fläche nutzen;
eine durch Karosseriebreite eingeschränkte Routenfläche wurde verworfen.
Spurzuordnung und alternative gerichtete Routen brauchen einen Verkehrsflussvertrag.
Windungszahlen sind eine notwendige topologische Bedingung, kein vollständiger Verkehrsflussbeweis.
[Quintische Routen](../test/experiments/infrastructure/infrastructure_spline_routes.py) laufen vor der Posekette:
Höchstens 66 Punkte, feste Endtangenten/Endkrümmung null; innen darf die Krümmung variieren.
Abstände von 1/2/3/4 Fahrzeug-Mindestradien und die Quelle liefern fünf Kandidaten.
Bézier-Hüllen begrenzen Geschwindigkeit/Krümmung; Koeffizientenprodukte gebündelt mit gleicher Summierreihenfolge. Maximal 4096 Karosserieposen.
[Korridorfahrten](../test/experiments/infrastructure/infrastructure_corridor_experiment.py) übernehmen unverändert
die Portposen beider Nachbarknoten; gerichtete Modulrouten und dieselbe Karosserieprüfung verbinden sie.
Nicht durchfahrbare Auto-Anschlüsse bleiben in der Nutzungsbilanz; ungültige feste Endkarosserien
werden einmal vor der Kurvensuche geprüft und bleiben ungelöst.
Zweipunktige Quellachsen dürfen bei versetzten Ports ebenfalls quintische Verbindungen liefern.
Nach erfolgloser Interpolation: Quellabweichung + quadratische zweite Differenzen, feste Endpunkte;
positiv definites Fünfdiagonalsystem in O(N). Fläche, Ports und Karosserie-/Radiusprüfung bleiben fest.
Jeder Spline-Kandidat muss die Inselwindung seiner Quelle erhalten; sonst läuft die Suche weiter.
[Lokale Routenpassung](../test/experiments/infrastructure/infrastructure_hermite_route_fitting.py) folgt auf die Rahmenwahl.
Quintische Hermite-Abschnitte teilen Position/Tangente/Beschleunigung; feste Ports, Endkrümmung null.
Nachbarabhängigkeiten ergeben eine dünn besetzte Jacobi-Matrix. Höchstens 18 Knoten und 40 Solver-
Auswertungen je Versuch; insgesamt 1024 tatsächliche Residuenaufrufe je Verbindung, inklusive Ableitungen.
Quellabweichung/Karosserieabstand/Bernstein-Schranken optimieren; Hüllen/Radius/Windung prüfen.
[Punktabstände](../test/experiments/infrastructure/infrastructure_surface_clearance.py): Randindex zuerst; nur Randnähe/außen messen.
Nach beiden Grundversuchen ergänzen Hüllenablehnungen Randviertel/Körpermitte; Hüllen/Residuenbudget bleiben fest.
[Startwiederverwendung](../test/experiments/infrastructure/probes/roadbeds/construction_reuse_probe.py):
34 Straßenräume erhalten acht gültige Ergebnisse, ohne Zeit-/Anschlussgewinn. Vorerst nicht integrieren.
[Kurven-Straßenräume](../test/experiments/infrastructure/infrastructure_source_roadbeds.py) ergänzen fehlenden Fahrraum durch direkte Kurven oder quellfolgende G2-Ketten und vollständige Karosseriehüllen.
[Hermite-Fahrraum](../test/experiments/infrastructure/infrastructure_fitted_roadbeds.py) ergänzt die Rezepte.
[Wendekurven](../test/experiments/infrastructure/infrastructure_return_roadbeds.py): Quelle ≤1024, dann CCC/CSC ≤512; gesamt ≤1536.
G2-Nähte/Ports/Hüllen prüfen; 1024 Residuen/Passung, 255 Intervalle. Starre Budgethalbierung verliert gültige Fahrten; Gesamtkosten optimieren. Mesh-Rand/neue Schlitze: 64·eps·max(1,|Koordinate|) m; alte/breitere Freiräume erhalten. Nur gültige, konservative Hilfspuffer nutzen.
Maximale Aufweitung: sqrt((R+w/2)^2+L^2)−R; R Mindestradius, w Breite, L Längsreichweite. Quellkette zuerst mit w/2 Reserve suchen; nur bei Ablehnung bis zur Eckreichweite erweitern. Reservationen räumlich eingrenzen, Suche nur außerhalb vorhandener Fahrfläche schützen; endgültige Aufweitung gegen alle prüfen.
Gebäude/andere Verkehrsflächen sind Reservierungen. Offene Blockränder dürfen kohärent weichen;
Geschlossene Komplementflächen, verschachtelte Inseln, ihre Zuordnung durch Flächenüberlappung und gerichtete Umfahrung erhalten.
Volle Hüllen gemeinsam bilden; Konstruktionsreserve an Reservationen kappen, ganze Körper am nativen Mesh neu prüfen. Alle Stücke teilen 4096 Prüfposen. Kleinste Binärpräzision; Außenports/Anfragen/Fahrten erhalten. Polygonale Fahrflächen; Schnittlinien sind kein Fahrraum.
[Gemeinsame Schnittverschiebung](../test/experiments/infrastructure/infrastructure_cut_setbacks.py) erweitert den Anschluss
innerhalb der unveränderten Gesamtfläche: lokale Straßenstücke wechseln Besitzer, auch bei Schleifen mit mehreren Anschlüssen. Binäre Präzision erhält volle gemeinsame Querschnitte entlang derselben Quellachse.
Mehrere Achsen je Schnitt behalten eindeutige Kontakte; alle Stationen rücken nach außen. Neue/verlorene Kontakte ablehnen.
1/2/3 Mindestradien: Eingang, Ausgang, beide; Nachbarn neu lösen. Quellen-/Außenkontakte, Anfragen und gültige Fahrten erhalten; nur bei gezieltem Gewinn übernehmen. Schnitte ändern Besitz, nicht Gesamtfläche.
[Gekoppelte Passung](../test/experiments/infrastructure/infrastructure_coupled_roadbed_experiment.py): gültige Teilnetze, volle Anfragenbilanz; Bestand sichern, Auslassungen begründen. Körper/Freiräume/Schnitte am gemeinsamen Mesh; Außenrahmen fest.
Schnitt-/Rahmenänderungen getrennt deklarieren; alle Portnachbarn prüfen. G2-Winkelumlauf speichern; bei Radiusfehler Drehsinne und gemeinsame Winkel prüfen.
[Fensterprüfung](../test/experiments/infrastructure/infrastructure_roadbed_window_experiment.py) übernimmt Fahrraum-/Schnittprodukte samt festen Posen.
Quellfahrten erhalten; unverträgliche Gruppen ablehnen. [Gesamtbild](../test/experiments/infrastructure/infrastructure_roadbed_window_render.py) zeigt publizierte Meshes.
[Portgraph](../test/experiments/infrastructure/infrastructure_route_connectivity.py): Komponenten und gerichtete Umfahrungen prüfen. DAG-Zielmasken in 64er-Batches statt quadratischer Matrix.
18 von 68 fehlenden Direktfahrten ohne Umfahrung zuerst lösen: Wien 8, Basel 10, Zürich 0. Verkehrsfluss/Modi/Kosten offen.

## Umsetzung und Abnahme
1. Gerade/Ende/Kurve, T/X und Einfädelung als kleine Rezepte mit gemeinsamen vollständigen Querschnitten.
2. [Gemeinsame Schnittrezepte](../test/experiments/infrastructure/probes/roadbeds/joint_cut_recipe_probe.py): überlappende Nachbarn gemeinsam lösen.
   Schnittbedarf teilen statt Besitzpläne überlagern; frühere Fahrten erhalten. Mehrfachschnitte/langen Fahrten lokal folgen.
   [Gemeinsamen Straßenraum](../test/experiments/infrastructure/probes/roadbeds/shared_road_space_probe.py) nutzen:
   ungelöste Fahrten einmal nachlösen; Aufweitung/Freiräume/Schnitte an ursprünglicher Fläche prüfen.
3. Mesh, Kontakt und Kollision aus demselben Bauteilplan; Fläche einmal besitzen und triangulieren.
   Freiräume/volle Karosserien am zusammengefügten Mesh prüfen; Einzelkandidaten sind kein Netzbeleg.
4. Kleine Fälle, dann alle verwendeten Bauteile der Wiener/Zürcher/Basler Fenster rendern und öffnen.
5. Weltweit hunderte bis tausende Fälle: [Survey](../test/experiments/infrastructure/infrastructure_world_survey.py),
   Originalkacheln, Nutzungsbilanz/Kosten und ausgewählte Nahansichten gemäß AGENTS.
   Native Übertragung erst nach annähernd fehlerfreien vollständigen 2D-/2,5D-Netzen gemäß Goal.
Gemeinsame Portpositionen/Normalen, plausible Breiten/Kurven, keine Risse/Faltungen/inneren Doppelstücke.
Verwendungsgrad und Kosten gehören zur Qualität; ungültige Teile bleiben außerhalb des Plans.
Überflüssige Zweierknoten ohne Richtungs-/Breitenwechsel erhalten kein eigenes Modul.
Ruhige Kurven und zusammenhängende Knotenformen im Bild entscheiden; Originalknicke dürfen weichen.
[Ports](../test/experiments/infrastructure/infrastructure_ported_junction.py), [Referenzen](../doc/references/infrastructure/README.md).
