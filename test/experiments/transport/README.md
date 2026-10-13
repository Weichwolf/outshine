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
