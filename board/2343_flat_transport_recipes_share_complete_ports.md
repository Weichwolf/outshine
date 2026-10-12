Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2342
Area: generators, world
Tags: infrastructure, solver, recipes

# Flat transport recipes share complete junction ports

## Ergebnis und Ist
Aus Quellgraph/Nutzungsbilanz (2342) entstehen zusammenhängende Straßen-, Wege- und Schienenflächen
mit funktionalen Verbindungen. Besitzer: generators/road; nativer Bauteilplan für 2281, ohne Höhen/Ebenen.
[Flächenplan](../test/experiments/infrastructure/infrastructure_flat_surface_network.py) und
[Ports](../test/experiments/infrastructure/infrastructure_flat_ports.py) teilen vollständige gerade Mesh-Ränder.
Alle verwendeten Bauteile der eingefrorenen Ebenen-0-Fenster mit 1 km Radius sind geprüft:
8627/8679 öffentliche Auto-Verbindungen; drei fehlen ohne gerichtete Umfahrung (Basel 3, Wien/Zürich 0).
Keine zusätzlichen schwachen oder starken Netztrennungen. Das belegt nur diese
Auto-Fenster. Wege/Rad/Schienen, Verkehrsfluss, Höhen und tragbare Gesamtkosten bleiben offen.

## Bauteil- und Verkehrsvertrag
- [Randschnitte](../test/experiments/infrastructure/infrastructure_flat_cuts.py) wählen echte Querschnitte
  per räumlichem Index. Konflikte rücken begrenzt nach außen; kurze breite Restketten gehören zum Anschluss.
  [Bordbögen](../test/experiments/infrastructure/infrastructure_flat_corners.py) runden tangential nach innen.
- [Portspuren](../test/experiments/infrastructure/infrastructure_port_lane_experiment.py) übernehmen
  Richtung/Zugang und Fahrseite. Mehrfachkontakte bleiben getrennt; schmale Zweirichtungsstrecken teilen
  eine Spur. Fahrzeugprofile bestimmen Breite/Radius, keine einheitlichen Werte für alle Verkehrsmodi.
  Jede öffentliche Auto-Endkarosserie muss auf öffentlicher Motorfläche liegen; private Wege sind kein Ausweichraum.
- [Gerichtete Routen](../test/experiments/infrastructure/infrastructure_directed_routes.py) erhalten
  Quellkontakte und gerichtete Anfragen. Portversätze und Umfahrungen erhalten die Windung um Verkehrsinseln.
  Windung allein beweist keinen Verkehrsfluss; Spurzuordnung/Abbiegekonflikte brauchen eigene Regeln.
- [Fahrkurven](../test/experiments/infrastructure/infrastructure_junction_turn_experiment.py): G2-Klothoiden
  mit Kreisbogen oder polynomialem Tangentenwinkel; Endkrümmung null, Radius über analytische Extrema prüfen.
  Länge durch Fläche/Arbeitsbudget begrenzen. [Quellketten](../test/experiments/infrastructure/infrastructure_route_turns.py)
  folgen einem begrenzten azyklischen Posengraph: 64 Ecken, drei Posen/Ecke, acht Nachfolger/Pose.
  Inselumfahrung und Nähe/Ferne/Ziel erhalten; gleiche Posenpaare und Vereinfachungen wiederverwenden.
- [Quintische Routen](../test/experiments/infrastructure/infrastructure_spline_routes.py) prüfen zuerst
  bis zu 66 Punkte mit festen Endtangenten/Endkrümmung null. Quelle und Abstände von 1/2/3/4 Mindestradien
  liefern fünf Kandidaten. [Korridore](../test/experiments/infrastructure/infrastructure_corridor_experiment.py)
  übernehmen beide Nachbarposen; nach Interpolation löst ein positiv definites Fünfdiagonalsystem die Glättung in O(N).
- [Rahmenwahl](../test/experiments/infrastructure/infrastructure_shared_frame_experiment.py): Richtungen
  in 5°-Schritten innerhalb ±30°, danach 17 Positionen; höchstens 64 teure Bauteilversuche.
  Karosserie auf beiden Flächen, volle Nachbarfahrten und angefragten Gewinn prüfen. Außenrahmen bleiben fest.
- [Hermite-Passung](../test/experiments/infrastructure/infrastructure_hermite_route_fitting.py) teilt
  Position/Tangente/Beschleunigung mit dünner Jacobi-Matrix: höchstens 18 Knoten, 40 Auswertungen/Versuch,
  1024 tatsächliche Residuenaufrufe/Verbindung einschließlich Ableitungen. Hüllenablehnungen ergänzen
  Randviertel/Körpermitte; [Randindex](../test/experiments/infrastructure/infrastructure_surface_clearance.py) begrenzt Abstandsmessungen.
- [Fahrzeughüllen](../test/experiments/infrastructure/infrastructure_vehicle_sweep.py): Körperradius r,
  Krümmung k und Änderung k' ergeben A = k+r*(|k'|+k²). Endhülle + A*ds²/8 muss auf Fahrfläche liegen;
  unklare Intervalle halbieren, höchstens 4096 Prüfposen. G1-Sprünge trennen. Bézier-Hüllen begrenzen
  v/a/j und Winkeldifferentiale bei positiver Mindestgeschwindigkeit; Linienmesh separat nach Fehler erzeugen.

## Gemeinsamer Straßenraum
[Kurvenraum](../test/experiments/infrastructure/infrastructure_source_roadbeds.py) und
[Hermite-Raum](../test/experiments/infrastructure/infrastructure_fitted_roadbeds.py) ergänzen die Fläche.
[Wendekurven](../test/experiments/infrastructure/infrastructure_return_roadbeds.py): Quelle ≤1024,
danach CCC/CSC ≤512 Residuen; zusammen ≤1536. Volle Hüllen bilden, G2-Nähte/Ports/Windung erhalten.
Maximale Aufweitung sqrt((R+w/2)^2+L^2)−R: Mindestradius R, Breite w, Längsreichweite L.
Quelle zuerst mit w/2 Reserve, bei Ablehnung bis Eckreichweite. Gebäude/fremde Verkehrsflächen reservieren;
Suche räumlich und auf neue Fläche eingrenzen, endgültige Körper gegen alle Reservationen prüfen.
Offene Blockränder dürfen weichen; geschlossene und verschachtelte Freiräume samt Überlappungszuordnung erhalten.
Hüllenkonstruktion an Reservationen kappen, ganze Körper am nativen Mesh nachprüfen; 4096 Posen gemeinsam.
Polygonale Fahrflächen behalten die feinste Binärpräzision. Mesh-Rundungsreste höchstens
64·eps·max(1,|Koordinate|) m schließen; echte/breitere Freiräume erhalten. Nur gültige konservative Hilfspuffer nutzen.
[Resttaschen](../test/experiments/infrastructure/infrastructure_roadbed_island_cleanup.py) im Baurezept schließen:
nur neu abgetrennte Teile einer vorhandenen Insel, unreserviert, ≤1 m² und ohne 1 m breiten Innenraum.
Größte Insel erhalten. [Restinsel-Rezept](../test/experiments/infrastructure/infrastructure_roadbed_void_recipe.py):
berührte erzeugte Hohlräume ≤1 m² ohne 1 m Innenbreite nur unreserviert schließen; explizit bilanzieren.
[Komposition](../test/experiments/infrastructure/infrastructure_complete_roadbed_module.py) prüft dabei
öffentliche Fahrfläche und physischen Mesh-Besitz getrennt; auch dort können Resttaschen entstehen.

[Gemeinsame Schnitte](../test/experiments/infrastructure/infrastructure_cut_setbacks.py) ändern lokalen
Besitz innerhalb gleicher Gesamtfläche. Alle Achsenstationen rücken nach außen; Mehrfachkontakte bleiben
identisch. Nachbarn, Außenkontakte, Anfragen und frühere Fahrten erhalten; feinste Präzision fortführen.
[Korridorgrenzen](../test/experiments/infrastructure/probes/roadbeds/corridor_product_probe.py) vor engen Biegungen
in den geraden Abschnitt legen; der Nachbaranschluss übernimmt die Biegung. Beide neu lösen; gesamte Fläche bleibt.
Eingang/Ausgang/beide mit 1/2/3 Mindestradien, danach 0,5/1,5 probieren; nur gezielten Gewinn übernehmen.
[Feste Schnitte](../test/experiments/infrastructure/probes/roadbeds/fixed_cut_frame_probe.py) vergleichen gemeinsame
Portwinkel ohne Besitzwechsel; der Direktkurvenfilter bleibt günstiger als ungefilterte Winkelversuche.
[Mehrfachschnitte](../test/experiments/infrastructure/probes/roadbeds/joint_cut_recipe_probe.py) lösen
überlappende Nachbarn oder weitere Schnitte desselben Anschlusses gemeinsam und erhalten frühere Produkte.
[Gemeinsamer Fahrraum](../test/experiments/infrastructure/probes/roadbeds/shared_road_space_probe.py) löst
fehlende Fahrten einmal nach; Aufweitung/Freiräume bleiben an ursprünglicher Fläche begrenzt.
[Gekoppelte Produkte](../test/experiments/infrastructure/infrastructure_coupled_roadbed_experiment.py) besitzen
Fläche einmal und triangulieren gemeinsam. Außenrahmen/Anfragen/Bestand erhalten; Körper, Freiräume,
Reservationen und Schnitte am gemeinsamen Mesh prüfen. Schnitt-/Rahmenänderungen getrennt deklarieren.

## Nächste Lieferung und Abnahme
1. Drei Basler Verbindungen ohne Umfahrung priorisieren. Gemeinsame Portposen/Korridore
   gemeinsam lösen; Basler Restfahrten teilen einen Ausgang. [Portgraph](../test/experiments/infrastructure/infrastructure_route_connectivity.py)
   prüft schwache/starke Komponenten und gerichtete Alternativen; DAG-Zielmasken in 64er-Batches.
   Enge Schleifen brauchen zuerst einen kinematisch brauchbaren Hindernispfad; lokale C2-Passung verfeinert ihn.
   [Quellführung](../test/experiments/infrastructure/probes/roadbeds/steering_source_hint_probe.py) nutzt Sichtbarkeit
   und verbleibende Quelllänge; Inselkerne schützen, zusätzliche volle Drehungen ausschließen.
   [Basel 6→28](../test/experiments/infrastructure/probes/roadbeds/extend_coupled_product_probe.py): 98 Fahrten auf vier Bauteilen, alle 97 bisherigen erhalten.
   [Geometriediagnose](../test/experiments/infrastructure/probes/roadbeds/fit_geometry_diagnostic.py) trennt echte
   Radius-/Hüllenfehler von konservativen Schranken. Gebäude folgen dem Straßenraum mit explizitem Zuschnitt;
   Quellidentität, brauchbare Restgrundrisse und nichtmotorisierte Verbindungen erhalten.
   [Lenkpfad → Mesh](../test/experiments/infrastructure/probes/roadbeds/steering_roadbed_product_probe.py):
   Wien 1480 gewinnt 4→1 bei unveränderten Grenzen und erhält alle acht bisherigen Fahrten;
   Bauteil 805 ist nach Schließen einer 0,43-m²-Resttasche vollständig befahrbar (7/7).
   Bauteil 89 45/45; zwei Kurven gemeinsam samt Bestand geprüft; 33 Kollisionsstationen, 1024-Aufrufe-Budget.
   Grundrisse samt Quellen/Tags schneiden; unbrauchbare Reste nach 25-m²-/3-m-Rezept verwerfen.
   Alle drei vollständigen Fenster bestätigen den Gewinn ohne Verlust vorhandener Fahrten:
   Wien 900/909, Zürich 4774/4808, Basel 2953/2962; Messcommit `cce1141b5`, ein Core, zusammen 131 s.
   Python-Such-/Prüfkosten bleiben zu hoch; aktuelles Bild unter `build/shots/experiments/infrastructure/`.
2. Wege, Rad und Schienen mit ihren eigenen Zugangs-/Querschnitts-/Kurvenprofilen integrieren;
   Auto-Masken/Karosserien sind dafür keine Spezifikation. Danach Spurführung und Abbiegekonflikte lösen.
3. Kosten pro Modul/Verbindung, Speicher und Skalierung vergleichen; nachgewiesen unnötige Sucharbeit beseitigen.
   [Hermite-Jacobi-Matrix](../test/experiments/infrastructure/infrastructure_hermite_jacobian.py): alternierende
   Knotengruppen brauchen minimal 7/12 Differenzversuche für das feste Sparsitätsmuster; gleiche Passungs-/Hüllenbudgets.
4. [Fensterprüfung](../test/experiments/infrastructure/infrastructure_roadbed_window_experiment.py) übernimmt
   Produkte/Posen ohne vorherige Fahrten zu verlieren. [Gesamtbilder](../test/experiments/infrastructure/infrastructure_roadbed_window_render.py)
   und Nahansichten öffnen: keine Risse/Faltungen/Doppelstücke, ruhige Kurven und konstruktive Kontakte.
5. [Weltsurvey](../test/experiments/infrastructure/infrastructure_world_survey.py): hunderte bis tausende reale
   Netze, Quellen/Nutzungsbilanz und ausgewählte Bilder. Flächen/Zugänge allein beweisen keine Befahrbarkeit.
Erst annähernd fehlerfreie vollständige 2D-/2,5D-Netze mit tragbaren Kosten nativ übertragen (Goal, 2344/2345).
Ungelöste lokale Teile samt Grund bleiben sichtbar bilanziert; gültige Lösungen blockieren nie den übrigen Aufbau.
[Referenzen und kleine Rezepte](../doc/references/infrastructure/README.md); gemeinsame Arbeits-/Abnahmeregeln in AGENTS.
