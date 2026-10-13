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
Zürich 4805/4808 (37dbc9435), öffentliche PKW-Klassenrezepte frisch geprüft; keine Größen-/Lastfreigabe.
Zürich 2134 mit Gesamtfenster 29/29: 8→10 rund 54 m statt 1,37 km Umweg. Noch drei Fahrten offen.
Wege/Rad/Schienen, Verkehrsfluss, Höhen, weltweite Robustheit und tragbare Gesamtkosten bleiben offen.
Python-Fensterlauf: Laden, übrige Kurvensuche, Mesh-/Verbindungsprüfung; ein Core:
Wien 10,1 s, Basel 16,5 s, Zürich 33,7 s. Vorbereiteter Produktaufbau zusätzlich; keine Runtime-Ladezeiten.
Zürich: übrige erfolglose Kurvensuche dominiert; direkte Rezepte/Methodenfilter vor teurer Passung ausbauen.
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
   und drei Längenparametern schließen. [Lange Spurwechsel](../test/experiments/transport/probes/roadbeds/split_straight_route_probe.py):
   zwei lokale Übergänge, eine Gerade; gemeinsame 1024 Aufrufe, G2-/Hüllen-/Gesamtflächenprüfung. Endports fest.
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
   Schlitze nur unter einem Koordinatenquantum und der Rundungsgrenze normalisieren; auflösbare Löcher erhalten.
   Neu abgetrennte Resttaschen nur unreserviert, ≤1 m² und ohne 1 m Innenbreite schließen/bilanzieren;
   größte Insel erhalten. Deklarierte Reservationen weiterreichen statt Quellenmasken erneut aufzubauen.

Öffentliche Ausgangsflächen bei Veröffentlichung erhalten, wenn ihre Überdeckung durch das native Mesh
nachgewiesen ist; unnötiges Ausschneiden/Wiedervereinen trennt numerische Berührungen. Rundungskontrolle
prüft Abdeckung der Ausgangsfläche innen sowie echte fehlende Flächen; keine erfundenen Mesh-Verbinder.
Nach Flächenvereinigung polygonale Anteile bereitstellen; Nullflächenlinien sind keine Fahrzeugfläche. Positive Fläche unverändert erhalten.

## Nächste Lieferung und Abnahme
34 gekoppelte Produkte im Zürcher Gesamtfenster; alle bisherigen Fahrten erhalten.
1565: 22/22 plus sieben Nachbarfahrten, +4,53 m²; gemeinsamer Schnitt vier Meter versetzt, Insel erhalten.
Leere Beiträge nicht erneut komponieren; Cachetreffer laden öffentliche Fläche. Native Fläche/Identitäten/Hüllen erneut prüfen.
Öffentliche Klassifizierung beim Laden nicht aus Quellen nachbauen: verändert Flächen und trennt Anschlüsse.
[Port-Aufteilung](../test/experiments/transport/transport_court_partition.py) begrenzt Erweiterungen
auf den nächsten Portmittelpunkt aller Nachbarn. Nach einem deklarierten Stationsversatz bleiben Schnittebenen beim Breitenlösen fest; Zusatzsegmente am
anderen Arm sind ungültig. Vor Partition auch Körperbedarf bestehender Fahrten zuordnen; Erweiterung genau einmal besitzen.
[Gemeinsame Auswahl](../test/experiments/transport/probes/roadbeds/joint_court_selection.py) prüft
höchstens 64 Kombinationen. Billigste Einzelkurven können gemeinsam eine Insel löschen;
Gegenrichtungen und betroffene Nachbarn zusammen wählen. Automatische Einbindung offen.
[Quellabkürzungen](../test/experiments/transport/probes/roadbeds/source_shortcut_probe.py) erhalten
Originalroute/Endposen. Öffentliche Übergänge auch im gemeinsamen Wendeplatz-Aufbau explizit prüfen und als Fahrfläche deklarieren;
private/Bahnflächen bleiben geschützt. Reservierte Gebäudezuschnitte fordert Transport bei Buildings an.
Offene Rückkehr-Wendeplätze brauchen keinen Inselanker. Radiusreserve passend zur verfügbaren
Fläche wählen, Mindestradius/Hüllenprüfung beibehalten. Keine Vergrößerung gültiger Restflächenlimits.
Optionale Kleinstspalt-Bereinigung: vollständig überdeckt, unreserviert, ≤0,05 m²,
kein 5-cm-Innenkreis. Fläche/Geometrie bilanzieren; konstruktive Vereinfachung, keine Rundungstoleranz.
[Richtungsframes kombinieren](../test/experiments/transport/probes/roadbeds/combine_directional_frames_probe.py)
erhält Quellstationen/Schnittlinien; [Port-Erweiterungen](../test/experiments/transport/probes/roadbeds/shared_port_extension_probe.py)
speichern gemeinsame öffentliche Unterstützung und prüfen erneutes Laden.

1. Verbleibende [Umwege](../test/experiments/transport/probes/roadbeds/window_detour_probe.py)
   durch direkte gültige Verbindungen ersetzen. Bewährte Endbogen-/Wendeplatz-/Portrezepte
   automatisch auswählen statt Place- oder Modul-Sonderfälle zu integrieren.
2. Wege/Rad/Schienen mit eigenen Zugangs-, Querschnitts- und Kurvenprofilen sowie
   Fahrzeugklassenzugang, Spurführung/Abbiegekonflikte. Zufahrten sind kein belegter Durchgangsverkehr; 2342.
3. [Fensterprüfung](../test/experiments/transport/transport_roadbed_window_experiment.py) erhält alle Fahrten;
   [Gesamtbilder](../test/experiments/transport/transport_roadbed_window_render.py) und Nahbilder öffnen:
   keine Risse/Faltungen/Doppelstücke. Kosten pro Modul/Verbindung, Speicher und Skalierung messen;
   erfolglose Suche durch passende begrenzte Rezepte ersetzen.
4. [Weltsurvey](../test/experiments/transport/transport_world_survey.py) auf hunderten bis tausenden
   realen Netzen mit Quellen-/Nutzungsbilanz. Erst annähernd fehlerfreie vollständige 2D-/2,5D-Netze
   mit tragbaren Kosten nativ übertragen. Höhe/Ebenen besitzen 2344/2345.
[Referenzen](../doc/references/transport/README.md); gemeinsame Arbeits-/Abnahmeregeln in AGENTS.
