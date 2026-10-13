# Transport-Experimente

Python-Verfahren für zusammenhängende OSM-Straßen, Wege und Schienen. Alle Aufrufe
laufen vom Repository-Root. Ergebnis, Abhängigkeiten und offene Lücken stehen in
[#18](https://github.com/Weichwolf/outshine/issues/18),
[#19](https://github.com/Weichwolf/outshine/issues/19),
[#20](https://github.com/Weichwolf/outshine/issues/20) und
[#21](https://github.com/Weichwolf/outshine/issues/21).

- `transport_*.py`: Vektorplan, Bauteile, Fahrkurven, lokale Prüfungen und Renderings.
- `road_*.py`, `earthwork_*.py`, `clearance_aprons.py`: frühere Profil-/Kontaktvergleiche.
- `probes/<Thema>/`: fallbezogene Verfahrensvergleiche und Fehlerdiagnosen.
- `transport_network_recipes.json`: gemeinsame Rezept- und Fahrzeugparameter.
- Weitere Dateien erhalten frühere Asset-/LOD-/Speichervergleiche.

Eingaben und Messergebnisse liegen unter `build/experiments/transport/`;
ausgewählte Bilder unter `build/shots/experiments/transport/`, Logs im System-Temp.
Die Probes verwenden ihre jeweiligen Eingabestände; gemessene Hashes bleiben unverändert.
Für Probe-Aufrufe zeigt `PYTHONPATH=test/experiments/transport` auf die Solvermodule.

```sh
python3 test/experiments/transport/transport_roadbed_window_experiment.py --help
python3 test/experiments/transport/transport_coupled_roadbed_experiment.py --help
python3 test/experiments/transport/transport_roadbed_window_render.py --help
```

Transport besitzt Verkehrsnetze; Buildings und Vegetation besitzen ihre eigenen Rezepte.
Gemeinsame räumliche Verträge stehen in [#18](https://github.com/Weichwolf/outshine/issues/18). Gebäudekonflikte hier prüfen nur diese Schnittstelle.
Historische Berichte behalten ursprüngliche Pfade, Hashes und Messcommits nach der Verschiebung.
Vor erneutem Anwenden hashgeprüfter Produkte die Nachweise unter den neuen Modulnamen regenerieren.

## Lokaler Showcase

Wie bei Buildings (8766) und Vegetation (8765) veröffentlicht der Transport-Showcase
einen festen Bildstand. Port 8767 zeigt Grundformen, Anschlüsse und echte OSM-Netze;
fehlende lokale Nachweise bleiben unvollständig. Bilder lassen sich vergrößern und
als PNG herunterladen. „Stand laden“ aktualisiert die Anzeige ohne automatisches Neuladen.

```sh
.venv/bin/python -m pip install numpy scipy 'shapely>=2.1' matplotlib pillow
.venv/bin/python test/experiments/transport/showcase.py --render-fixtures --serve
```

Die Grundfälle benutzen den bestehenden Flächensolver mit gemeinsamen Randschnitten.
GEOS ist als Systembibliothek erforderlich (`libgeos-dev` unter Debian).
Der Solver importiert die installierte Referenzbibliothek: bei vorhandenem `lib/`
einmal `.venv/bin/python -m pip install -e ./lib` ausführen.
Erzeugte Bilder, Produkte und Messreports liegen unter
`build/experiments/transport/showcase/fixtures/`, die veröffentlichte Website unter
`build/experiments/transport/showcase/site/`. Der Server erzeugt keine Geometrie bei Anfragen.

```sh
.venv/bin/python test/experiments/transport/showcase.py --publish
.venv/bin/python test/experiments/transport/showcase.py --serve --host 0.0.0.0 --port 8767
.venv/bin/python -W error test/experiments/transport/showcase_checks.py
```

`--catalog` wählt einen anderen Bildkatalog. Jeder Eintrag nennt Bild, Messreport und
Erzeugungsnachweis unter `build/`. `origin.json` enthält den Erzeugungscommit,
`working_tree_dirty` und unter `records` die SHA-256-Werte von Bild und Report,
indiziert durch den Report-Dateinamen ohne Erweiterung. Nur übereinstimmende lesbare
PNGs werden veröffentlicht. Synthetische Flächenprüfungen belegen keine glatten
Fahrkurven, vollständige Befahrbarkeit, Höhen oder native Echtzeitqualität.
Der Auftrag und die Abnahme stehen in [#49](https://github.com/Weichwolf/outshine/issues/49).

## Geschlossene 3D-Körper und gespeicherte Wiener Quellkachel

`transport.close_surface` aus `lib/` schließt bereits konforme Fahrflächen mit
expliziten Vertexhöhen. Obere Anschlussvertices bleiben gemeinsam, Innenlöcher
offen; Boden und Randwände bilden einen gerichteten Körper. Das ist ein
Geometrieverfahren, kein Höhen-/Verkehrsnetzsolver. Der Vergleich prüft dieselben
Körperbuffer, Kanten und Volumina für NumPy-Sortierung und eine Dictionary-Referenz.
Frische Worker messen sieben Aufbauzeiten und ihren vollständigen Peak-RSS.

```sh
.venv/bin/python -m pip install -r test/experiments/transport/requirements.txt
.venv/bin/python -m pip install -e ./lib
.venv/bin/python test/experiments/transport/capture_transport_tile.py
.venv/bin/python test/experiments/transport/transport_flat_network.py --input build/experiments/transport/inputs --output build/experiments/transport/flat-vienna Vienna
.venv/bin/python test/experiments/transport/transport_flat_surface_network.py --input build/experiments/transport/flat-vienna --output build/experiments/transport/surface-vienna-cuts --junction-mode cuts Vienna
.venv/bin/python test/experiments/transport/surface_solid_benchmark.py
.venv/bin/python test/experiments/transport/surface_solid_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
```

Die Aufnahme liest ausschließlich die bereits gepinnte OpenFreeMap-Kachel aus dem
Client-Quellcache und prüft ihren Hash. Originalfeatures, Attribute, IDs und
zurückgestellte Flächen bleiben im gespeicherten Input erhalten. Die ebene
Nutzungsbilanz nennt jeden Achsteil samt konkretem Grund bei Nichtverwendung.
Eine Quellkachel ersetzt nicht das frühere vollständige Wiener Testfenster.

Blender ist als Systempaket erforderlich (`blender` unter Debian). Vier
Workbench-Inspektionen in 1920×1080 liegen unter `build/experiments/transport/solid/`:
geneigter T-Anschluss, Ring, getrennte Querung und Wiener Quellkachel. Die Kamera
prüft sämtliche Vertices auf vollständige Bildabdeckung; Skriptfehler ergeben
einen fehlgeschlagenen Renderprozess. Der Showcase veröffentlicht nur den letzten
vollständig gerenderten, zu Bild und Report passenden Stand.
Flatheight und Brückenniveaus sind explizite Rezeptannahmen. DEM, Rampen, Stützen,
fahrbare Kurven und native Abnahme bleiben offen. Bibliotheksübernahme: #50.

```sh
.venv/bin/python -W error -m unittest discover -s lib/tests
.venv/bin/python -W error test/experiments/transport/surface_solid_checks.py
```
