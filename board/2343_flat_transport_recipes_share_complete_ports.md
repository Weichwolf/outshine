Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 2342
Area: generators, world
Tags: transport, solver, recipes

# Flat transport recipes share complete junction ports

## Ergebnis und Stand
Zusammenhängende Straßen-, Wege- und Schienenflächen mit funktionalen Verbindungen aus dem
Quellgraph/Nutzungsbilanz (2342). Besitzer: Transport/generators/road; Bereichsgrenzen: 2342,
native Lieferung: 2281. Höhenprofile folgen in 2344, Ebenenrezepte in 2345.
Ebenen-0-Fenster, 1 km Radius: Wien 909/909 (117dea678), Basel 2962/2962 (5ab84801f),
Zürich 4779/4808 (2ccdf237b). Nur öffentliche Auto-Verbindungen belegt.
Zürich 2134 mit Gesamtfenster 29/29: 8→10 rund 54 m statt 1,37 km Umweg. Noch 29 Fahrten offen.
Wege/Rad/Schienen, Verkehrsfluss, Höhen, weltweite Robustheit und tragbare Gesamtkosten bleiben offen.
Python-Fensterlauf: Laden, übrige Kurvensuche, Mesh-/Verbindungsprüfung; ein Core:
Wien 10,1 s, Basel 16,5 s, Zürich 71,4 s. Vorbereiteter Produktaufbau zusätzlich; keine Runtime-Ladezeiten.
Zürich: 49,8 s Kurvensuche, davon 37,0 s erfolglos; direkte Rezepte/Methodenfilter vor teurer Passung ausbauen.
Bilder: build/shots/experiments/transport/.

## Verbindlicher Bauteil- und Verkehrsvertrag
- [Flächenplan](../test/experiments/transport/transport_flat_surface_network.py),
  [Randschnitte](../test/experiments/transport/transport_flat_cuts.py) und
  [Ports](../test/experiments/transport/transport_flat_ports.py) teilen vollständige gerade Mesh-Ränder.
  Kurze breite Restketten gehören zum Anschluss. Außenkontakte und Quellidentitäten bleiben erhalten.
- [Portspuren](../test/experiments/transport/transport_port_lane_experiment.py) erhalten Richtung,
  Fahrseite, Zugänge und getrennte Mehrfachkontakte. Fahrzeugprofile bestimmen Breite/Radius.
  Öffentliche Auto-Endkarosserien liegen auf öffentlicher Motorfläche; private Wege sind kein Ausweichraum.
- [Gerichtete Routen](../test/experiments/transport/transport_directed_routes.py) erhalten alle
  Quellanfragen und die Windung um Verkehrsinseln. Windung allein beweist keinen Verkehrsfluss.
  G2-Kurven besitzen feste Endposen, Endkrümmung null und eine konservativ geprüfte Radiusschranke.
- [Fahrzeughüllen](../test/experiments/transport/transport_vehicle_sweep.py): Körperradius r,
  Krümmung k und Änderung k' ergeben A = k+r·(|k'|+k²). Endhülle + A·ds²/8 muss auf Fahrfläche liegen.
  Unklare Intervalle halbieren, höchstens 4096 Prüfposen; G1-Sprünge trennen. Mesh separat nach Fehler bauen.
- [Gemeinsame Produkte](../test/experiments/transport/transport_coupled_roadbed_experiment.py)
  besitzen Fläche einmal. Alle betroffenen Nachbarn, Quellen, früheren Fahrten, Reservationen und
  Schnittlinien am selben Mesh prüfen. Öffentliche Erweiterungen ausdrücklich speichern und beim Laden prüfen.
  Fußwegübergänge nutzen nur geeignete öffentliche Gehwege; private/Bahnflächen und Fußweggeometrie erhalten.
  Gebäude folgen mit explizitem Freiraum-/Zuschnittvertrag; ihr Baukörperrezept bleibt bei Buildings.

## Rezepte und Kostenentscheidungen
1. [Quell-Endbögen](../test/experiments/transport/probes/roadbeds/terminal_lane_alignment_probe.py)
   auf feste Spurstrahlen projizieren; unterschiedliche Spurversätze vor Kurvenbau berücksichtigen.
   [Direkte G2-Spurbögen](../test/experiments/transport/probes/roadbeds/rounded_lane_loop_probe.py)
   und [Wendekerne](../test/experiments/transport/probes/roadbeds/inset_turning_core_probe.py) zuerst prüfen.
   Einzelne Quellbögen über den positiven Schnitt beider fester Spurstrahlen konstruieren; parallele/rückwärtige Fälle ablehnen.
2. [Quintische Routen](../test/experiments/transport/transport_spline_routes.py) nutzen bis 66 Punkte;
   [Quellketten](../test/experiments/transport/transport_route_turns.py) höchstens 64 Ecken,
   drei Posen/Ecke, acht Nachfolger/Pose. [Korridore](../test/experiments/transport/transport_corridor_experiment.py)
   teilen Nachbarposen und glätten über ein positiv definites Fünfdiagonalsystem in O(N).
3. [Hermite-Passung](../test/experiments/transport/transport_hermite_route_fitting.py): höchstens 18 Knoten,
   40 Auswertungen/Versuch und 1024 tatsächliche Residuenaufrufe samt Ableitungen; Versuche mit anderem
   Budget ausdrücklich messen. [Randindex](../test/experiments/transport/transport_surface_clearance.py)
   begrenzt Messungen. [Rückfahrten](../test/experiments/transport/transport_return_roadbeds.py)
   teilen 1536 Aufrufe: Quelle bis 1024, CCC/CSC bis 512; analytische CCC-Kerne bis 128 Aufrufe/Seed.
   Methodenkompatible Familien vor Rangfolge/Versuchslimit wählen; CSC/CCC mit linearen Krümmungsübergängen
   und drei Längenparametern schließen. Gerade Ein-/Ausläufe unabhängig wählen; gemeinsame Endports bleiben fest.
4. R ist der Mindestradius, w die Breite, L die größere Körperreichweite vor/hinter der Hinterachse.
   Randaufweitungen bleiben an der ursprünglichen Fläche begrenzt: sqrt((R+w/2)²+L²)−R.
   [Schleifen-Wendeplätze](../test/experiments/transport/transport_loop_court.py) lösen bis acht Kurven
   gemeinsam in einem festen Quellraum mit Reichweite 2R+L+w/2. Keine kumulative Aufweitung.
   Inseln, Reservationen, alle Körper und sämtliche bisherigen Fahrten bleiben verbindlich.
5. [Rahmenwahl](../test/experiments/transport/transport_shared_frame_experiment.py): ±30° in 5°-Schritten,
   17 Positionen, höchstens 64 teure Versuche. [Gemeinsame Schnitte](../test/experiments/transport/transport_cut_setbacks.py)
   nur quellbezogen verschieben. [Wendeplätze](../test/experiments/transport/probes/roadbeds/coupled_turning_court_probe.py)
   lösen betroffene Nachbarn; [Randschnitt-Rezepte](../test/experiments/transport/probes/roadbeds/replay_cut_product_probe.py)
   rekonstruieren Quellstationen und deklarierte Rahmen, beweisen sämtliche alten Fahrten frisch.
   Gemeinsamen Raum einmal nachlösen: Originalverbindungen sofort erhalten, frühere Zusatzfahrten
   vor Veröffentlichung vollständig prüfen; Aufweitung bleibt an der ursprünglichen Fläche begrenzt.
6. Vor Triangulation kanonisch normalisieren. [Konforme Kanten](../test/experiments/transport/transport_conforming_triangles.py)
   teilen an vorhandenen Vertices. Rundungsreste höchstens 64·eps·max(1,|Koordinate|) m; echte Risse erhalten.
   Rundungsreste komponentenweise erodieren; numerische MultiPolygon-Puffer dürfen keinen realen Flächenverlust vortäuschen.
   Neu abgetrennte Resttaschen nur unreserviert, ≤1 m² und ohne 1 m Innenbreite schließen/bilanzieren;
   größte Insel erhalten. Deklarierte Reservationen weiterreichen statt Quellenmasken erneut aufzubauen.

## Nächste Lieferung und Abnahme
Zürich 2260 mit Gesamtfenster 16/16: gemeinsame Schnitte 4 m nach außen, +0,21 m² Fläche.
1316 im Gesamtfenster 29/29: zwei Wendeprofile mit öffentlichen Fußwegübergängen, +8,86 m² Fläche.
1555 im Gesamtfenster 5/5: asymmetrische Ansätze 4/32 m, 61,4 m Fahrt statt 932,9 m Umweg;
physisch +2,88 m² Fläche. Alle 14 früheren Produkte und 24 Zusatzfahrten erhalten. Fußweggeometrie erhalten.
Als Nächstes 1433/1758; verbleibende
[Umwege](../test/experiments/transport/probes/roadbeds/window_detour_probe.py) durch direkte gültige Verbindungen ersetzen.
Danach Wege/Rad/Schienen mit eigenen Zugangs-, Querschnitts- und Kurvenprofilen sowie Spurführung/Abbiegekonflikte.
[Fensterprüfung](../test/experiments/transport/transport_roadbed_window_experiment.py) erhält sämtliche bisherigen Fahrten;
[Gesamtbilder](../test/experiments/transport/transport_roadbed_window_render.py) und Nahbilder öffnen: keine Risse/Faltungen/Doppelstücke.
Kosten pro Modul/Verbindung, Speicher und Skalierung messen; unnötige Suche ersetzen. [Weltsurvey](../test/experiments/transport/transport_world_survey.py)
prüft hunderte bis tausende reale Netze mit Quellen-/Nutzungsbilanz. Flächen/Zugänge allein beweisen keine Befahrbarkeit.
Erst annähernd fehlerfreie vollständige 2D-/2,5D-Netze mit tragbaren Kosten nativ übertragen.
Ungelöste Teile mit Grund bilanzieren; sie blockieren nie den gültigen restlichen Aufbau.
[Referenzen](../doc/references/transport/README.md); gemeinsame Arbeits-/Abnahmeregeln in AGENTS.
