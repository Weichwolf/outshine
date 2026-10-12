Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2342
Area: generators, world
Tags: transport, solver, recipes

# Flat transport recipes share complete junction ports

## Ergebnis und Ist
Aus Quellgraph/Nutzungsbilanz (2342) entstehen zusammenhängende Straßen-, Wege- und Schienenflächen
mit funktionalen Verbindungen. Besitzer: Transport/generators/road; Bereichsgrenzen: 2342, nativer Plan: 2281.
[Flächenplan](../test/experiments/transport/transport_flat_surface_network.py) und
[Ports](../test/experiments/transport/transport_flat_ports.py) teilen vollständige gerade Mesh-Ränder.
Ebenen-0-Fenster mit 1 km Radius: Wien 907/909 (`62a64e8a6`), Zürich 4774/4808 (`03af0cec8`).
Basel 2962/2962 (`5ab84801f`); frühere Verbesserungen erhalten, keine zusätzlichen schwachen/starken Netztrennungen.
Nur Auto-Fenster belegt. Wege/Rad/Schienen, Verkehrsfluss, Höhen und tragbare Gesamtkosten bleiben offen.

## Bauteil- und Verkehrsvertrag
- [Randschnitte](../test/experiments/transport/transport_flat_cuts.py) wählen echte Querschnitte
  per räumlichem Index. Konflikte rücken begrenzt nach außen; kurze breite Restketten gehören zum Anschluss.
  [Bordbögen](../test/experiments/transport/transport_flat_corners.py) runden tangential nach innen.
- [Portspuren](../test/experiments/transport/transport_port_lane_experiment.py) übernehmen
  Richtung/Zugang und Fahrseite. Mehrfachkontakte bleiben getrennt; schmale Zweirichtungsstrecken teilen
  eine Spur. Fahrzeugprofile bestimmen Breite/Radius, keine einheitlichen Werte für alle Verkehrsmodi.
  Jede öffentliche Auto-Endkarosserie muss auf öffentlicher Motorfläche liegen; private Wege sind kein Ausweichraum.
- [Gerichtete Routen](../test/experiments/transport/transport_directed_routes.py) erhalten
  Quellkontakte und gerichtete Anfragen. Portversätze und Umfahrungen erhalten die Windung um Verkehrsinseln.
  Windung allein beweist keinen Verkehrsfluss; Spurzuordnung/Abbiegekonflikte brauchen eigene Regeln.
- [Fahrkurven](../test/experiments/transport/transport_junction_turn_experiment.py): G2-Klothoiden
  mit Kreisbogen oder polynomialem Tangentenwinkel; Endkrümmung null, Radius über analytische Extrema prüfen.
  Länge durch Fläche/Arbeitsbudget begrenzen. [Quellketten](../test/experiments/transport/transport_route_turns.py)
  folgen einem begrenzten azyklischen Posengraph: 64 Ecken, drei Posen/Ecke, acht Nachfolger/Pose.
  Inselumfahrung und Nähe/Ferne/Ziel erhalten; gleiche Posenpaare und Vereinfachungen wiederverwenden.
- [Quintische Routen](../test/experiments/transport/transport_spline_routes.py) prüfen zuerst
  bis zu 66 Punkte mit festen Endtangenten/Endkrümmung null. Quelle und Abstände von 1/2/3/4 Mindestradien
  liefern fünf Kandidaten. [Korridore](../test/experiments/transport/transport_corridor_experiment.py)
  übernehmen beide Nachbarposen; nach Interpolation löst ein positiv definites Fünfdiagonalsystem die Glättung in O(N).
- [Rahmenwahl](../test/experiments/transport/transport_shared_frame_experiment.py): Richtungen
  in 5°-Schritten innerhalb ±30°, danach 17 Positionen; höchstens 64 teure Bauteilversuche.
  Karosserie auf beiden Flächen, volle Nachbarfahrten und angefragten Gewinn prüfen. Außenrahmen bleiben fest.
- [Hermite-Passung](../test/experiments/transport/transport_hermite_route_fitting.py) teilt
  Position/Tangente/Beschleunigung mit dünner Jacobi-Matrix: höchstens 18 Knoten, 40 Auswertungen/Versuch,
  1024 tatsächliche Residuenaufrufe/Verbindung einschließlich Ableitungen. Hüllenablehnungen ergänzen
  Randviertel/Körpermitte; [Randindex](../test/experiments/transport/transport_surface_clearance.py) begrenzt Abstandsmessungen.
- [Fahrzeughüllen](../test/experiments/transport/transport_vehicle_sweep.py): Körperradius r,
  Krümmung k und Änderung k' ergeben A = k+r*(|k'|+k²). Endhülle + A*ds²/8 muss auf Fahrfläche liegen;
  unklare Intervalle halbieren, höchstens 4096 Prüfposen. G1-Sprünge trennen. Bézier-Hüllen begrenzen
  v/a/j und Winkeldifferentiale bei positiver Mindestgeschwindigkeit; Linienmesh separat nach Fehler erzeugen.

## Gemeinsamer Straßenraum
[Kurvenraum](../test/experiments/transport/transport_source_roadbeds.py) und
[Hermite-Raum](../test/experiments/transport/transport_fitted_roadbeds.py) ergänzen die Fläche.
[Wendekurven](../test/experiments/transport/transport_return_roadbeds.py): Quelle ≤1024,
danach CCC/CSC ≤512 Residuen; zusammen ≤1536. Volle Hüllen bilden, G2-Nähte/Ports/Windung erhalten.
Maximale Aufweitung sqrt((R+w/2)^2+L^2)−R: Mindestradius R, Breite w, Längsreichweite L.
Quelle zuerst mit w/2 Reserve, bei Ablehnung bis Eckreichweite. Gebäude/fremde Verkehrsflächen reservieren;
Suche räumlich und auf neue Fläche eingrenzen, endgültige Körper gegen alle Reservationen prüfen.
Offene Blockränder dürfen weichen; geschlossene und verschachtelte Freiräume samt Überlappungszuordnung erhalten.
Hüllenkonstruktion an Reservationen kappen, ganze Körper am nativen Mesh nachprüfen; 4096 Posen gemeinsam.
Fahrflächen vor Triangulation kanonisch normalisieren, unabhängig von GEOS-Metadaten; feinste Binärpräzision, Quell-/Kontaktlinien separat.
[Konforme Kanten](../test/experiments/transport/transport_conforming_triangles.py) teilen Dreiecke an vorhandenen Randvertices; Positionen/Fläche bleiben erhalten.
Mesh-/Schnitt-Rundungsreste höchstens 64·eps·max(1,|Koordinate|) m; echte Risse/Freiräume erhalten, keine T-Verbindungen kaschieren.
[Resttaschen](../test/experiments/transport/transport_roadbed_island_cleanup.py) im Baurezept schließen:
nur neu abgetrennte Teile einer vorhandenen Insel, unreserviert, ≤1 m² und ohne 1 m breiten Innenraum.
Größte Insel erhalten. [Restinsel-Rezept](../test/experiments/transport/transport_roadbed_void_recipe.py):
berührte erzeugte Hohlräume ≤1 m² ohne 1 m Innenbreite nur unreserviert schließen; explizit bilanzieren.
[Komposition](../test/experiments/transport/transport_complete_roadbed_module.py) prüft dabei
öffentliche Fahrfläche und physischen Mesh-Besitz getrennt; auch dort können Resttaschen entstehen.

[Gemeinsame Schnitte](../test/experiments/transport/transport_cut_setbacks.py) ändern lokalen
Besitz innerhalb gleicher Gesamtfläche. Alle Achsenstationen rücken nach außen; Mehrfachkontakte bleiben
identisch. Nachbarn, Außenkontakte, Anfragen und frühere Fahrten erhalten; feinste Präzision fortführen.
[Korridorgrenzen](../test/experiments/transport/probes/roadbeds/corridor_product_probe.py) vor engen Biegungen
in den geraden Abschnitt legen; der Nachbaranschluss übernimmt die Biegung. Beide neu lösen; gesamte Fläche bleibt.
Eingang/Ausgang/beide mit 1/2/3 Mindestradien, danach 0,5/1,5 probieren; nur gezielten Gewinn übernehmen.
[Feste Schnitte](../test/experiments/transport/probes/roadbeds/fixed_cut_frame_probe.py) vergleichen gemeinsame
Portwinkel ohne Besitzwechsel; der Direktkurvenfilter bleibt günstiger als ungefilterte Winkelversuche.
[Mehrfachschnitte](../test/experiments/transport/probes/roadbeds/joint_cut_recipe_probe.py) lösen
überlappende Nachbarn oder weitere Schnitte desselben Anschlusses gemeinsam und erhalten frühere Produkte.
[Gemeinsamer Fahrraum](../test/experiments/transport/probes/roadbeds/shared_road_space_probe.py) löst
fehlende Fahrten einmal nach; Aufweitung/Freiräume bleiben an ursprünglicher Fläche begrenzt.
[Gekoppelte Produkte](../test/experiments/transport/transport_coupled_roadbed_experiment.py) besitzen
Fläche einmal und triangulieren gemeinsam. Außenrahmen/Anfragen/Bestand erhalten; Körper, Freiräume,
Reservationen und Schnitte am gemeinsamen Mesh prüfen. Schnitt-/Rahmenänderungen getrennt deklarieren; gemeinsame öffentliche Erweiterungen speichern und beim Laden gegen das Mesh prüfen.

## Nächste Lieferung und Abnahme
1. [Umwege](../test/experiments/transport/probes/roadbeds/window_detour_probe.py) statt bloßer Erreichbarkeit:
   Basel vollständig; [Wien 574/684/685 gemeinsam](../test/experiments/transport/probes/roadbeds/connected_cut_region_probe.py): 36/36; 659 mit Nachbarn 56/56. Alle früheren Fahrten im Gesamtfenster erhalten. Wien 1480 lokal 12/13, Gesamtfenster offen; 1066 mit Gesamtfenster 13/13; Zürich 2134: 60→1369 m.
   [Gekoppelte Wendeplätze](../test/experiments/transport/probes/roadbeds/coupled_turning_court_probe.py): nur betroffene Nachbarn neu lösen; Quintik-Zufahrten erhalten G2 auch hinter Quellbiegungen. Wien 659: Schnitte 8 m nach außen, +111,7 m², keine Gebäudekonflikte.
   [Versetzter Wendekern](../test/experiments/transport/probes/roadbeds/inset_turning_core_probe.py): 1066 mit 8-m-Zufahrten, +28,8 m² Gesamtbauteil; feste Ports, G2/Radius am Mesh geprüft.
   [Breitere Ports](../test/experiments/transport/transport_court_partition.py) auf festen Schnittebenen verteilen neue Fläche einmal; Bestand/Quellkontakte erhalten.
   [Portgraph](../test/experiments/transport/transport_route_connectivity.py): schwache/starke Komponenten, gerichtete Alternativen, 64er-DAG-Batches.
   [Quellführung](../test/experiments/transport/probes/roadbeds/steering_source_hint_probe.py): Sichtbarkeit/Restlänge, Inselkerne, keine Zusatzdrehung; C2-Passung danach.
   [Gemeinsames Kurvenende](../test/experiments/transport/probes/roadbeds/shared_route_guide_probe.py): Basel 101 Fahrten auf vier Bauteilen, Bestand erhalten.
   [Geometriediagnose](../test/experiments/transport/probes/roadbeds/fit_geometry_diagnostic.py) trennt echte
   Radius-/Hüllenfehler von konservativen Schranken. Gebäude folgen dem Straßenraum mit explizitem Zuschnitt;
   Quellidentität, brauchbare Restgrundrisse und nichtmotorisierte Verbindungen erhalten.
   [Lenkpfad → Mesh](../test/experiments/transport/probes/roadbeds/steering_roadbed_product_probe.py) nutzt deklarierte Reservationen samt zulässigen Fußwegübergängen; keine erneute Quellmaske.
   [Direkte G2-Spurbögen](../test/experiments/transport/probes/roadbeds/rounded_lane_loop_probe.py): Wien 1480 lokal 11/13 statt 9/13; zwei Fußwegübergänge auf vorhandener Geometrie, keine privaten Wege/Bahn nutzen. Erstauflösung, frischer Ladeprozess und Gesamtfenster geprüft; Fußweggeometrie erhalten.
   [Gegenfahrt](../test/experiments/transport/probes/roadbeds/reuse_lane_loop_probe.py) passt bewährte Schleifen an feste Spurports; exakte Bézier-Unterteilung schärft die Radiusschranke ohne Geometrieänderung. 1480: 4→2 lokal belegt; 7→2 offen.
   Wien: 14 aufgebaute Produkte; alle früheren Produktfahrten und acht frühere Einzelverbesserungen erhalten, 907 direkte Fahrten.
   Fensterprüfung mit vorbereiteten Produkten auf einem Core: Wien 11,5 s, Basel 16,5 s; Aufbau kostet zusätzlich. Kosten bleiben zu hoch; Bilder: `build/shots/experiments/transport/`.
2. Wege, Rad und Schienen mit ihren eigenen Zugangs-/Querschnitts-/Kurvenprofilen integrieren;
   Auto-Masken/Karosserien sind dafür keine Spezifikation. Danach Spurführung und Abbiegekonflikte lösen.
3. Kosten pro Modul/Verbindung, Speicher und Skalierung vergleichen; nachgewiesen unnötige Sucharbeit beseitigen.
   [Hermite-Jacobi-Matrix](../test/experiments/transport/transport_hermite_jacobian.py): alternierende
   Knotengruppen brauchen minimal 7/12 Differenzversuche für das feste Sparsitätsmuster; gleiche Passungs-/Hüllenbudgets.
4. [Fensterprüfung](../test/experiments/transport/transport_roadbed_window_experiment.py) übernimmt
   Produkte/Posen ohne vorherige Fahrten zu verlieren. [Gesamtbilder](../test/experiments/transport/transport_roadbed_window_render.py)
   und Nahansichten öffnen: keine Risse/Faltungen/Doppelstücke, ruhige Kurven und konstruktive Kontakte.
5. [Weltsurvey](../test/experiments/transport/transport_world_survey.py): hunderte bis tausende reale
   Netze, Quellen/Nutzungsbilanz und ausgewählte Bilder. Flächen/Zugänge allein beweisen keine Befahrbarkeit.
Erst annähernd fehlerfreie vollständige 2D-/2,5D-Netze mit tragbaren Kosten nativ übertragen (Goal, 2344/2345).
Ungelöste lokale Teile samt Grund bleiben sichtbar bilanziert; gültige Lösungen blockieren nie den übrigen Aufbau.
[Referenzen und kleine Rezepte](../doc/references/transport/README.md); gemeinsame Arbeits-/Abnahmeregeln in AGENTS.
