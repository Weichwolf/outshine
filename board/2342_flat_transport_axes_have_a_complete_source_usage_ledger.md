Type: feature
State: active
Architecture: planned
Priority: P0
Parent: 2169
Depends: 
Area: generators, world
Tags: infrastructure, solver, recipes

# Flat transport axes provide a bounded plausible vector network

## Ergebnis und Ist
Ein ebener quellbezogener Vektorgraph als erstes gültiges Grundmodell; kein Höhenraster.
OSM-Achsen geben Verteilung/Verbindungshinweise. Kleine lokale Anpassungen sind ausdrücklich erlaubt.
Der vorhandene Flächenversuch vereinigt Bänder, löst aber weder Ports noch alle Verkehrsverbindungen.
Besitzer: generators/road; OSM-Semantik verbleibt in generators/osm. Native Lieferung folgt in 2281.
[Ebener Vektorversuch](../test/experiments/infrastructure_flat_network.py) liefert kleine Grundfälle,
Quell-IDs pro nodelter Kante, begrenzte lokale Endkorrekturen und eine vollständige Nutzungsbilanz.
Andere Ebenen/Innenräume bleiben in diesem Schritt ausdrücklich nicht verwendet.
Lückenlose Bauteilflächen, Kurven/Routenregeln und native Integration sind damit noch nicht bewiesen.
[Quellrichtung](../test/experiments/infrastructure_source_directions.py) unterscheidet umgekehrte
Quellen und überträgt deren Orientierung auf jede gemeinsam genodelte Kante, auch bei Ringen.
Numerisch identische Knoten werden gemeinsam mit dem Mesh unter derselben Verschiebungsgrenze
vereinigt; reine Rundungsreste sind keine befahrbaren Kanten. Unklare Richtungen werden gezählt.
Geometrische Richtung ist noch keine Zugangserlaubnis; Modus/Zugang und Abbiegen weiter lösen.

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
[Flächenversuch](../test/experiments/infrastructure_network.py),
[Graphversuch](../test/experiments/infrastructure_network_graph.py),
[Street Modeling, SIGGRAPH 2008](../doc/references/infrastructure/siggraph/2008-interactive-procedural-street-modeling.pdf).
