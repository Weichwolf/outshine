Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 
Area: generators, world
Tags: transport, solver, recipes

# Flat transport axes provide a bounded plausible vector network

## Ergebnis und Ist
Ein ebener quellbezogener Vektorgraph als erstes gültiges Grundmodell; kein Höhenraster.
OSM-Achsen geben Verteilung/Verbindungshinweise. Kleine lokale Anpassungen sind ausdrücklich erlaubt.
Der vorhandene Flächenversuch vereinigt Bänder, löst aber weder Ports noch alle Verkehrsverbindungen.
Besitzer: Transport/generators/road; OSM-Semantik verbleibt in generators/osm. Native Lieferung: 2281.
[Ebener Vektorversuch](../test/experiments/transport/transport_flat_network.py) liefert kleine Grundfälle,
Quell-IDs pro nodelter Kante, begrenzte lokale Endkorrekturen und eine vollständige Nutzungsbilanz.
Andere Ebenen/Innenräume bleiben in diesem Schritt ausdrücklich nicht verwendet.
Lückenlose Bauteilflächen, Kurven/Routenregeln und native Integration sind damit noch nicht bewiesen.
[Quellrichtung](../test/experiments/transport/transport_source_directions.py) unterscheidet umgekehrte
Quellen und überträgt deren Orientierung auf jede gemeinsam genodelte Kante, auch bei Ringen.
Numerisch identische Knoten werden gemeinsam mit dem Mesh unter derselben Verschiebungsgrenze
vereinigt; reine Rundungsreste sind keine befahrbaren Kanten. Unklare Richtungen werden gezählt.
[Verkehrsprüfung](../test/experiments/transport/transport_travel_experiment.py) übersetzt gelieferte
Richtung/Zugänge und Klassenrezepte in gerichtete Modusmasken. Spezifische Angaben überschreiben
allgemeine Zugänge; private/beschränkte Verbindungen bleiben getrennt vom öffentlichen Netz.
Absteigen/Seitenwegbedarf und unklare Werte werden quellbezogen ausgewiesen, nicht still geraten.
Auto→Treppe und Rückfahrt in Einbahnrichtung sind ausgeschlossen. Fahrspuren, Abbiegeverbindungen
und Fahrzeughüllen bleiben offen; Zugangsregeln allein beweisen keine Befahrbarkeit.
Motorzugang trennt PKW/LKW/Bus samt Klasseneinbahnregeln; Busfahrbahnen erlauben standardmäßig nur Busse.
Größen-/Lastgrenzen vor Routenbau prüfen; bislang offen. Fehlende Angaben sind Rezeptannahmen.
Grundstückszufahrten ohne Zugangstags sind keine belegten öffentlichen Durchgangsstraßen.

## Bereichsgrenzen und parallele Arbeit
Transport besitzt Straßen/Wege/Schienen samt verkehrsbezogenen Brücken, Tunneln und Ports.
Buildings (2173) und Vegetation (2111) werden unabhängig parallel entwickelt.
Dieser Agent bearbeitet Transport auf master; Buildings/Vegetation arbeiten auf eigenen Git-Branches.
Gemeinsame OSM-Aufbereitung erhält Quellidentitäten/Tags; keine getrennten Quellsilos.
Transport publiziert belegte Flächen, Höhenintervalle, Freiräume und Zugangskontakte.
Buildings besitzt Baukörper/Durchgänge; Vegetation besitzt Standorte/Wuchsformen.
Beide konsumieren diese räumlichen Verträge statt Verkehrsgeometrie erneut zu lösen.
Transport fordert Gebäudedurchgänge und vegetationsfreie Räume an; deren Umsetzung bleibt beim jeweiligen Besitzer.
Gebäudekonflikte im Transportexperiment prüfen nur den Vertrag, nicht Gebäuderezepte.
Eine Quelle darf mehrere Produkte beeinflussen; world bleibt frei von OSM-Semantik.

## Umsetzung in kleinen Schritten
1. Gerade, T, X, Kurve, Sackgasse, kurzer Verbinder, knapp verfehlter Anschluss und Insel einzeln lösen.
   Einfache Rezepte; unklare oder nicht unterstützte Fälle lokal auslassen und begründen.
2. Kompatible Enden/Innenkontakte räumlich finden, kurze Verbindungen vereinigen, Achsen gemeinsam nodeln.
   Quell-IDs, Richtung, Zugang und Klassen behalten; physische Kontakte und Verkehrsverbindungen trennen.
   Lokale Nähe erlaubt eine plausible Verbindung, beweist aber keine originale OSM-Topologie.
3. Dieselben Verfahren auf die vollständigen gewählten Wiener/Zürcher/Basler Fenster anwenden.
   Quell-/Plan-Bilder öffnen. Arbeit pro Komponente begrenzen; Budgetende liefert den gültigen bisherigen Plan.
4. Nutzungsbilanz: jede Quelle verwendet, als Duplikat repräsentiert oder nicht verwendet mit Grund.
   Anzahl, Netzlänge, Fläche und lokale Auslassungen berichten; feste Zähler, keine Ausgabe im Hot Path.
   Gründe: nicht unterstütztes Rezept, unklare Verbindung, ungültige Geometrie oder Budgetende.

## Fehlender Vertrag und Abnahme
Quellbezogener ebener Graph mit gemeinsam gespeicherten Knoten und vollständiger Nutzungsbilanz.
Kein stiller Verlust, keine ungültigen Verbindungen; isolierte ungelöste Teile blockieren das übrige Netz nicht.
Budget und Wiederholungsgrenze explizit; Laufzeit/Spitzen/Wachstum mit echten Eingaben messen.
Ein paar zusammenhängende Straßen sind nur der erste Fall, kein Beleg für das ganze Fenster.
[Flächenversuch](../test/experiments/transport/transport_network.py),
[Graphversuch](../test/experiments/transport/transport_network_graph.py),
[Street Modeling, SIGGRAPH 2008](../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf).
