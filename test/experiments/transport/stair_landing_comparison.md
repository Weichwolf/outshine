# Ganze gewählte Podeste und wiederverwendbare Profilkonstruktion (#511)

Der bisherige [24-Quellen-Eckstand](stair_corner_comparison.md) besteht zwölf
ausdrücklich geschützte überlappende Paarprofile. Die neue Prüfung erfasst alle
48 Quellendprofile: 41 an gewählten inneren Knoten, sieben weiterhin außen.
Beim alten Körper sind nur 24 der 41 inneren Profile ganz getragen; 17 bleiben
unvollständig. Der erste neue ganze Körper mit neun zusätzlichen Podesten
besteht sämtliche 41 Profile in Double und tatsächlichem Float32. Originale,
Achsen/Priors/Attribute/IDs und alle bisherigen Quellen bleiben erhalten.

Die optionale [feste Eingabe](fixtures/stair-landings-vienna.json) wählt keine
neuen Verbindungen oder Endpunkte. Sie erzeugt an bereits gewählten echten
Knoten nur tatsächlich fehlende ganze Profilflächen. Alle Nachbarn werden zuerst
zusammen betrachtet. Die neun Profilhüllen ergänzen zusammen 0,591984 m² außerhalb
des vorhandenen Verbunds; ihre ganze gemeinsam besessene Fläche ist 7,539117 m².
Die bestehenden expliziten Eckprofile und ihr ursprünglicher Acht-Podest-Vertrag
bleiben unverändert. Die neue Auswahl hat eine eigene Obergrenze von 16 kleinen
Podesten je bestehender begrenzter Komponente. Unveränderte Mesh-/LP-/Terrain-,
Zeit-/Speichergrenzen und lokale Auslassungsgründe bleiben erforderlich.

Der kleine [installierbare Kern](../../../lib/junction_pads.md) besitzt nur die
metrische Konstruktion aus zwei bis vier zentrierten Profilen. Quellen, Ebenen,
Treppen-/Brückenauswahl, Höhen, Reservationen und tatsächliche Kontaktprüfung
bleiben beim Aufrufer. Der bisherige Ground-only-Aufrufer importiert denselben
Kern mit seinen alten Semantik-/Budgetregeln. Keine SDK-/Fremdcode-Übernahme.

Die unabhängige Prüfung am gespeicherten Mesh kontrolliert Besitzerfläche und
die Vereinigung ihrer gewählten Nachbarflächen getrennt über den ganzen
Endquerschnitt: tatsächliche flache gleiche Höhe und ganze XY-Deckung in beiden
Präzisionen. Ein echtes vollständig gemeinsames Randprofil darf Kante an Kante
anschließen; positive gemeinsam besessene Fläche ist dafür nicht erforderlich.
Die bisherigen zwölf ausdrücklich überlappenden Paarprofile bleiben separat
verbindlich. Geschlossener gemeinsamer Körper, ganze Quellflächen, Klassen,
Unterführungen und zyklische Vertexlinks bleiben zusätzliche Prüfungen.

Zuerst SIGGRAPH Chen et al. 2008, S. 2/Fig. 2 auch optisch geprüft:
[Graph und 3D-Geometrie](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
sind getrennte Stufen. Das Paper schließt konstruktive Kreuzungsdetails aus;
unsere begrenzte Profilhülle ist kein dort belegter Kreuzungsalgorithmus.
Bestehende GEOS-/CDT-Bibliotheksoperationen bleiben die Grundlage.

```sh
export PYTHONPATH=lib/src:lib/tests:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest test_junction_pads stair_landing_checks \
  stair_corner_checks stair_corridor_checks stair_cycle_checks \
  stair_cycle_north_checks stair_cycle_south_checks stair_terrain_cells_checks -q
python test/experiments/transport/stair_landing_cost.py \
  --method profile_hull --output /tmp/outshine-transport-landing-hull.json
python test/experiments/transport/stair_landing_cost.py \
  --method profile_box --output /tmp/outshine-transport-landing-box.json
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-landings-vienna.json \
  --base-network /tmp/outshine-transport-stair-corner-full-edcd738ce \
  --base-terrain /tmp/outshine-transport-stair-corner-full-edcd738ce \
  --output /tmp/outshine-transport-landings-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py`; Pfeiler mit
`stair_support_experiment.py`/`stair_support_verify.py`, derselben Fixture und den
neuen geprüften Produkten. `stair_landing_cost.py --profile <frische.bin>` liefert
getrennte Allokationsphasen. Native/Frame-/Weltlade-/Statikabnahme bleibt offen.
