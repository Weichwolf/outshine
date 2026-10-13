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
