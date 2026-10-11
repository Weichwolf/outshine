# Infrastruktur-Experimente

Python-Verfahren für zusammenhängende OSM-Straßen, Wege und Schienen. Alle Aufrufe
laufen vom Repository-Root. Ergebnis, Abhängigkeiten und offene Lücken stehen in
[2342](../../../board/2342_flat_transport_axes_have_a_complete_source_usage_ledger.md),
[2343](../../../board/2343_flat_transport_recipes_share_complete_ports.md),
[2344](../../../board/2344_transport_profiles_fit_the_vector_plan.md) und
[2345](../../../board/2345_grade_separated_recipes_preserve_usable_local_networks.md).

- `infrastructure_*.py`: Vektorplan, Bauteile, Fahrkurven, lokale Prüfungen und Renderings.
- `road_*.py`, `earthwork_*.py`, `clearance_aprons.py`: frühere Profil-/Kontaktvergleiche.
- `probes/<Thema>/`: fallbezogene Verfahrensvergleiche und Fehlerdiagnosen.
- `infrastructure_network_recipes.json`: gemeinsame Rezept- und Fahrzeugparameter.
- Weitere Dateien erhalten frühere Asset-/LOD-/Speichervergleiche.

Eingaben und Messergebnisse liegen unter `build/experiments/infrastructure/`;
ausgewählte Bilder unter `build/shots/experiments/infrastructure/`, Logs im System-Temp.
Die Probes verwenden ihre jeweiligen Eingabestände; gemessene Hashes bleiben unverändert.
Für Probe-Aufrufe zeigt `PYTHONPATH=test/experiments/infrastructure` auf die Solvermodule.

```sh
python3 test/experiments/infrastructure/infrastructure_roadbed_window_experiment.py --help
python3 test/experiments/infrastructure/infrastructure_coupled_roadbed_experiment.py --help
python3 test/experiments/infrastructure/infrastructure_roadbed_window_render.py --help
```
