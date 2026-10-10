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
[Bordbögen](../test/experiments/infrastructure_flat_corners.py) runden Innenkanten tangential,
ohne bestehende Verkehrsfläche zu entfernen. Ungültige Bogenstücke entfallen lokal samt Grund;
ein Konflikt verwirft keine ganze Komponente.
Die Bogenpolygone überlappen konstruktiv nach innen; Rundungsreste dürfen keine eigene Fläche
bilden. Ein schräger T-Anschluss prüft diese numerische Verbindung ohne Place-Sonderfall.
[Schnittprüfung](../test/experiments/infrastructure_flat_ports.py) prüft gerade vollständige innere
Querschnitte unabhängig von der Flächenabdeckung; eine lückenlose Fläche allein genügt nicht.
[Direkte Querschnitte](../test/experiments/infrastructure_flat_cuts.py) grenzen die tatsächliche
Verkehrsfläche per Randschnitt/Suchindex ab und polygonisieren gemeinsame Grenzen.
Gleichartige Nachbarstücke werden vereinigt. Kollidierende Schnitte rücken begrenzt nach außen;
kurze Zwischenabschnitte gehen in den gemeinsamen Anschluss ein. Die drei vollständigen
Ebenen-0-Fenster bestehen damit auch die Querschnittprüfung.
Quellteilung darf keine künstlichen Bauteile erzeugen; Kosten und weltweite Vielfalt weiter prüfen.
[Port-Fahrspuren](../test/experiments/infrastructure_port_lane_experiment.py) übernehmen öffentliche
Richtung/Zugänge auf die tatsächlichen gemeinsamen Querschnitte. Getrennte Anschlüsse derselben
Bauteile und mehrfache Achsenkreuzungen bleiben getrennt. Schmale Zweirichtungsstrecken teilen
eine Spur; Fahrseite und Fahrzeugbreite steuern die Platzierung. Verkehrsregelung bleibt offen.
[Fahrkurven](../test/experiments/infrastructure_junction_turn_experiment.py) verwenden Klothoiden
mit optionalem Kreisbogen oder asymmetrische G2-Posekurven mit polynomialem Tangentenwinkel.
Endkrümmung null, Krümmungsextrema analytisch; Radius/Breite kommen aus dem Fahrzeugprofil.
[Quellrouten](../test/experiments/infrastructure_route_turns.py) verbinden Portkontakte über
gerichtete kürzeste Wege. Ein begrenzter azyklischer Posengraph überspringt unnötige Quellknicke;
jede Verbindung prüft dieselben Kurvenrezepte und die vollständige Karosserie.
Höchstens 64 Ecken, drei Posen je Ecke und acht Nachfolger je Pose begrenzen die lokale Suche.
Identische Vereinfachungen entfallen; identische Posenpaare nutzen innerhalb desselben Bauteils
dieselbe Prüfung. Eine Untergrenze aus Segmentanzahl und Sehnenlänge verwirft aussichtslose Kandidaten.
Es gibt keinen globalen Memo-Speicher: Fläche, Fahrzeug und Toleranz bleiben innerhalb der Suche fest.
[Fahrzeughülle](../test/experiments/infrastructure_vehicle_sweep.py): bei Körperradius r und
maximaler Krümmung k bewegt sich jeder Körperpunkt höchstens (1+k*r)*ds. Diese Grenze sichert
die Zwischenräume der Abtastung; gerade Translationen nutzen den exakt extrudierten Körper.
Wege/private Zufahrten sind kein Ausweichraum. Der gemeinsame Mesh begrenzt öffentliche Motorfläche.
[Rezeptprüfungen](../test/experiments/infrastructure_route_experiment.py) umfassen Inselumfahrung,
verdeckte Hindernisse zwischen Endposen und identische Ergebnisse bei wiederverwendeten Verbindungen.
Offen: versetzte Quellrouten, ungültige feste Portposen, enge Kreisverkehre, Verkehrsregelung,
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
