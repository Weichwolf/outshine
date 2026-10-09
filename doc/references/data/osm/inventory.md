# OSM-Lieferumfang und Rezeptplan

Stand: 2026-10-09. Anbieter: OpenFreeMap. Runtime bleibt auf
`20260927_080001_pt`; Inventur nutzt denselben Endpunkt. Aktuelles
[TileJSON](https://tiles.openfreemap.org/planet) nennt `20261004_113936_pt`.
Das aktualisiert weder Runtime noch bestehende Caches automatisch.

## Befund und Grenzen

32 gezielte Standorte, je eine Kachel auf z10/z12 und 3×3 auf z14:
32 × (1 + 1 + 9) = 352 Kacheln. 352 erfolgreiche HTTP-Antworten, null Fehler.
69.298.575 Payload-Bytes / 1.048.576 = 66,09 MiB. Vier parallele Transfers.
Städte, Altstädte, Häfen, Kraftwerke, Wind-/Solarparks, Flughäfen und Alpen;
Europa, Asien, Afrika, Nord-/Südamerika und Ozeanien. Kein Beweis globaler Vollständigkeit.

[Messdaten samt Koordinaten, URLs und Hashes](openfreemap-inventory-20261009.json),
[alle beobachteten Klassen und Schlüssel](openfreemap-classes-20261009.md),
[geliefertes TileJSON](openfreemap-tilejson-20261009.json).
Reproduktion: `test/experiments/osm_provider_inventory.py`; Ausgabe/Downloads ins System-Temp.
Featurezahlen enthalten Zoom- und Kachelpuffer-Duplikate, sind keine Objektzahlen.
Die Standortnamen benennen Untersuchungsgebiete, keine bewiesenen Einzelobjekte.

**Gebäude liefern ausschließlich `render_height`, `render_min_height`, `colour`, `hide_3d`.**
Die Stichprobe bestätigt das veröffentlichte Feldschema. Nutzung, Dachform, Material,
Baujahr, Geschosse und Schornstein-/Kühlturmklasse fehlen. Der
[OpenMapTiles-Gebäudelayer](https://github.com/openmaptiles/openmaptiles/blob/master/layers/building/building.yaml)
importiert OSM-Gebäude intern, exportiert aber nur diese vier Eigenschaften.
Mehr OSM-Tags beim Import bedeutet deshalb keine vollständigen Tags im gelieferten MVT.

POI: 137 Klassen und 476 Unterklassen beobachtet, überwiegend Einrichtungen/Nutzungen.
Kein beobachteter Eintrag bezeichnet Schornstein, Kühlturm, Windgenerator oder Solarpark;
ihre Quellschlüssel fehlen auch im veröffentlichten TileJSON. Das ist eine Lieferlücke.
Generische Industrieflächen beweisen keine bestimmte Anlage. Nicht beobachtet heißt
bei anderen Klassen nicht weltweit ausgeschlossen. Upstream-Schemata sind Referenzen;
das tatsächliche Anbieter-TileJSON und Payload entscheiden.

## Verantwortlichkeiten und Reihenfolge

| Reihenfolge | Inhalt / gemeinsames Rezept | Besitzer |
|---|---|---|
| 1 | Alle gelieferten Layer/Attribute auf SSD erhalten; genutzte native Semantik ohne Quellenverlust normalisieren | 2341, Cachevertrag 2280 |
| 2 | Sichere Gebäude: Haus/Reihe, Block/Büro, Halle, Garage, Glasbau; einfache vollständige Dächer | 2173 |
| 3 | Sondervolumen: Schornstein, Silo/Tank, Wasserturm, Kirche, Kühlturm; nur belegte Klasse oder klarer Ersatz | 2173 |
| 4 | Straßen/Schienen/Wege, Brücken/Tunnel/Stege, Flughafengeometrie, Seilbahnen | 2281, 2338 |
| 5 | Gewässer, Kanäle, Becken, Hafen-/Uferkontakte, Dämme | 2145, 2338 |
| 6 | Technische Anlagen: Windräder, Solarreihen, Kräne, Masten/Leitungen | 2338; Sonderklassendaten fehlen |
| 7 | POI-Familien: Zugänge/Barrieren, Straßenmöbel, Parken/ÖPNV, Sport/Spiel, Versorgung/Gewerbe | 2338; Beschriftung/Nahfassade 2173 |
| 8 | Landuse/Landcover: Landwirtschaft, Fels/Sand/Schnee/Nässe, Industrie-/Wohnflächen | 2337, 2171; Pflanzen zuletzt 2111 |

Gemeinsame Primitive und Parameterrezepte statt eines Generators je Tagwert. Grenzen,
Ortsnamen und mehrsprachige Namen bleiben Metadaten; nicht jedes Attribut erzeugt Geometrie.
Nutzung ist keine Bauform: Ein Restaurant-POI kann nur ein Geschoss eines Gebäudes belegen.
POI-Zuordnung braucht räumliche und fachliche Eindeutigkeit; kein pauschales Nearest-Neighbor.
Fehlende Sonderklassendaten lassen sich durch einen besseren Shader nicht wiederherstellen.
Vor Quellenwechsel oder Datenanreicherung den fehlenden Liefervertrag gezielt prüfen (2341).

## Optische Abnahme

Die zehn Pflicht-Places bleiben erhalten. Zusätzliche Szenarien entstehen an tatsächlich
gelieferten Features: mindestens ein Nah-/Fernbeispiel je physischem Rezept, weitere Beispiele
für runde/konkave Formen, Parts, Höhenintervalle und Übergänge. Gemeinsame Rezepte dürfen
mehrere Klassen bedienen; jeder beobachtete Klassenwert braucht eine explizite Zuordnung.
Verdeckte/fehlende Merkmale gelten nicht als optisch geprüft. Quelle und Ersatz unterscheiden.
Bild, Vollständigkeit, p99, Laden und Speicher gemeinsam im AGENTS-Profil vergleichen.
Diese Inventur enthält keine neuen gerenderten Sonderbau-Abnahmen.

Schema-Snapshots in diesem Ordner sind Forschungsreferenzen aus
`https://raw.githubusercontent.com/openmaptiles/openmaptiles/master/layers/<layer>/<layer>.yaml`,
abgerufen am 2026-10-09; keine eingebundene Bibliothek oder Laufzeitabhängigkeit.
