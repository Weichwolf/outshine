Type: feature
State: active
Architecture: ready
Priority: P1
Parent: 2169
Depends:
Area: client, scenario
Tags: places, visual, regression

# Venedig, Central Park, Shibuya und Jura ergänzen die Weltabnahme

## Ergebnis

`Venice`, `CentralPark`, `Shibuya` und `Jura` sind wieder auswählbare Places und Teil der Render-Suite.
Die historischen Perspektiven ergänzen die bestehenden Kameras; keine davon entfällt.
Die Aufnahmen zeigen Wasserstadt, Parkumgebung, japanische Großstadt und Jura zunächst ohne Vegetation.

## Vorhandene Fähigkeit und Entscheidung

Vor 631d20ce8 enthielt PlaceCamera diese vier Kameras. Alle verwenden 60 m über DEM,
55° Sichtfeld und −6° Neigung. Venice: 45.438/12.3358, Blickrichtung 30°,
2026-06-21T11:11:00Z. CentralPark: 40.7968/−73.9520, Blickrichtung 218.32°,
2026-06-21T16:56:00Z. Shibuya: 35.6595/139.7005, Blickrichtung 40°,
2026-06-21T02:41:00Z. Jura: 47.2492/7.5108, Blickrichtung 156.53°,
2026-06-21T11:30:00Z. Historische Höhe nicht als absolute Meereshöhe interpretieren.
Die native geodätische Kamera unterstützt DEM-Abtastung bereits; nur der Place-Katalog
lehnt sie pauschal ab. Der Loader validiert Koordinaten/Projektion weiter, überlässt die
Höhenauflösung aber wie andere Szenarien der Engine. Fehlendes DEM bleibt Ladefehler.

## Umsetzung

Besitzer: src/assets/places für Deklarationen; src/client/PlaceCamera.cpp für den Katalog;
Engine/GeodeticCamera für die vorhandene Höhenauflösung. Keine zweite Kamerarechnung.
Historische Perspektiven als Szenarien deklarieren. Vier Renderfälle unter
test/outshine/integration/places ergänzen. Katalogprüfung erhält alle vier Einträge und
prüft geländerelative Kameras. Keine Place-Sonderpfade oder angehobenen Aufnahmegrenzen.

## Fertig, wenn

Alle vier Kameras laden und rendern über outshine-client mit vollständiger Welt. Hash-PNGs
im Haupt-Checkout öffnen; fehlende Inhalte und falsche Höhe lassen die Abnahme scheitern.
make format; test/scripts/test_place_catalog.py; Places-Suite; make lint.
