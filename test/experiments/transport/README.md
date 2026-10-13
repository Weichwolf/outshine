# Transport-Experimente

Python-Verfahren für zusammenhängende OSM-Straßen, Wege und Schienen. Alle Aufrufe
laufen vom Repository-Root. Ergebnis, Abhängigkeiten und offene Lücken stehen in
[2342](../../../board/2342_flat_transport_axes_have_a_complete_source_usage_ledger.md),
[2343](../../../board/2343_flat_transport_recipes_share_complete_ports.md),
[2344](../../../board/2344_transport_profiles_fit_the_vector_plan.md) und
[2345](../../../board/2345_grade_separated_recipes_preserve_usable_local_networks.md).

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
Gemeinsame räumliche Verträge stehen in 2342. Gebäudekonflikte hier prüfen nur diese Schnittstelle.
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
