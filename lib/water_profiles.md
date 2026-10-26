# Gemeinsame physische Wasserhöhen (#760)

`outshine_ref.water.fit_physical_surface` übernimmt die in #843/#846 geprüfte
Float64-Referenz unverändert in das installierbare Paket. Experimente importieren
diesen Kern; es gibt keine zweite Solver-/Geodäsieimplementierung im Experimentbaum.
Der Kern importiert weder Experimente, Renderer, Originaldaten noch Quellprovider.

```python
from outshine_ref.water import fit_physical_surface

fit = fit_physical_surface(
    surface, anchor_lon_lat, direction_rows,
    up_bounds_m=(minimum_enu_up, maximum_enu_up),
    primary_rows=two_existing_row_indices_per_face,
    fixed=explicit_enu_up_contacts,
    contact_points=explicit_physical_enu_points,
)
```

`surface` ist das bestehende unveränderliche `Mesh`. Seine Z-Werte sind weiche
**ENU-Up**-Ziele; XY und Dreiecksindices bleiben unverändert. `anchor_lon_lat`
deklariert den WGS84-Rahmenursprung bei Modellhöhe0. Es ist keine Kameraänderung
und keine beobachtete ASL-/Geoidumrechnung. Gemeinsame Vertexindices teilen dieselbe
physische Höhe. Das Ergebnis ist das bestehende `HeightFit` mit eigenem unveränderlichem
Float64-Mesh oder `mesh=None` samt Grund. Bericht/Qualitätsgrenzen bleiben erhalten.

Jede endliche `(N,6)`-Richtungszeile enthält Face-ID, drei ENU-Richtungskomponenten,
untere und obere physische Neigung; das Intervall enthält Null. Das projizierte
Richtungsfeld und seine Metrik sind explizit: horizontale Meter auf der parallelen
Ellipsoidfläche bei der jeweiligen Abfragehöhe. `primary_rows` ist ein ganzzahliges
Array `(Faces,2)` mit zwei vorhandenen unabhängigen Bedingungen desselben Faces.
Die daraus abgeleitete Normalnormgrenze liefert ein konservativeres lineares QP.
Ohne diese Angabe bleibt die ursprüngliche SOCP-Referenz; es werden keine Richtungen
oder Schranken erfunden. `fixed` enthält ausschließlich vorhandene Vertexindices
und harte ENU-Up-Höhen. Quellen-/Datumbeleg für diese Pins besitzt der Aufrufer.

`contact_points` ergänzt vorhandene Vertex-Pins um physische Punkte innerhalb der
Facetten und auf geteilten Kanten. Jede Zeile enthält `(East, North, Up)` im selben
expliziten Rahmen wie das Mesh, keine geografischen Gradwerte. Ein unverändertes
XY-Dreieck mit baryzentrischen Gewichten w erhält die harte Gleichung
`sum(w_i * vertex_up_i) = point.Up`; mit Solver-Datum d lautet ihre rechte Seite
`point.Up - d*sum(w_i)`. Alle Verbraucher benutzen weiterhin dieselben physischen
Dreiecke. Es gibt keine zusätzliche interpolierte Kontaktfläche/Neutriangulation.

Der bestehende `SurfaceSampler` wählt ein eindeutiges Blatt ohne XY-Toleranz:
kein Snapping, keine Extrapolation auf nahe Ufer. Gemeinsame Kanten haben dieselben
Vertexindices; sonst mehrdeutige oder außerhalb liegende Kontakte liefern explizite
Abwesenheit mit Eingabeindex/Grund. Widersprüchliche Punkte, Vertex-Pins oder
Prismengrenzen werden nicht gelockert. Nach dem Fit prüft ein unabhängiger
Kreuzprodukt-Reader den physischen Ebenenabstand als ENU-Up-Fehler mit unveränderter
1e-9-Grenze. Geodätische Rückprojektion hat ihre eigene ECEF-Rundungsgröße und ist
kein Beleg für gemessene Wasserstandgenauigkeit.

Maximal 4096 Punkte; größere Mengen werden vor Kopie/Indexaufbau abgewiesen.
Die begrenzte Kandidatenabfrage verwendet maximal 262144 Treffer, einschließlich
konservativer Batchgrenzen des gemeinsamen Samplers. Nicht alle Baumtraversal-/
GEOS-Allokationen sind damit gemessen. Punkte werden besessen kopiert; bei `None`
oder leerer Menge wird kein Kontaktindex gebaut. Physische Koordinaten, Quellen-IDs,
Datum und Auswahl des richtigen Wasserbereichs besitzt weiterhin der Aufrufer.
Keine automatisch erfundenen See-/Terrain-/Brücken-/Regionskontakte.

Die ganze Facette wird über die bestehende Prismenhülle geprüft, nicht nur an
Stichproben. Grenzen bleiben8192 Vertices,16384 Faces,65536 Zeilen und lokale Normalenkappe
delta<0,01. Endliche geordnete ENU-Up-Prismen dürfen null und negative Höhen enthalten;
Unterrand und rekonstruierter Mittelpunkt müssen oberhalb der normalen Singularität
`-a(1-e²)` bleiben. Für nichtpositive Prismen vergrößert die gemessene
Vorwärts-/Rückwärtsabweichung des Mittelpunkts samt Float64-Zuschlag die ganze Hülle.
[Herleitung und signierte Gegenfälle](../test/experiments/water/signed_physical_water.md).
Die bisherigen positiven Ill-Eingaben bleiben bitgleich. Default5s/200Iterationen sind
kooperative Offline-Limits, kein Framebudget. Auch überlaufene Solverschritte und
fehlende gültige Ergebnisse bleiben ausdrücklich möglich. Physische Neigungs-/
Kontaktprüfung1e-9 und endliche Primal-/Dualprüfung1e-7 sind unverändert. Unzulässige
Eingaben/primäre Basen werden abgewiesen; kein versteckter Fallback oder gelockerter Pin.

`water.frame` enthält die geprüften Ziel-/Frame-/Normalenhüllenverfahren;
`water.geodetic.query_facet` fragt dieselben physischen Dreiecke geodätisch vertikal ab.
Seine numerischen Baryzentriken sind keine native watertight-/BVH-Abnahme. Der
normalisierte exakte Dreiecks-Massoperator hat einen Besitzer in `water._mass`;
auch die älteren Wasserexperimente verwenden ihn jetzt. Es gibt keine native ABI.

Mathematische Definitionen, Primärquellen und Alternativen bleiben an ihrem
fachlichen Einzelort: [physischer Fit](../test/experiments/water/physical_river_fit.md),
[lineares QP](../test/experiments/water/linear_physical_river.md),
[geodätischer Jacobian](../test/experiments/water/geodetic_facet.md).
Regionale Nahtstellen, See-/Meerpegel, Quellen-/Klassen-/Kurszulassung und vollständige
Teilprodukte, Terrain/Brücken und native Cache-/Rendererintegration bleiben offen.

Installation mit `pip install './lib[water]'` ergänzt Clarabel und pyproj/PROJ;
Basisimporte bleiben frei von diesen zusätzlichen Imports. Der ausdrücklich gewählte
OSQP-Vergleichspfad benötigt separat `pip install 'osqp>=1.1,<2'`; er ist kein
Geschwindigkeitsdefault. Abhängigkeiten sind unverändert zur geprüften Forschung,
nur ihre Paketzuordnung ist jetzt explizit. Kein nativer Geräte-/Zeit-/Speicherpass.

Fünf Bibliothekstests ergänzen die vorhandenen Verfahrens-/Originaltests: verzögerter
Import, Besitz/Unveränderlichkeit, echte harte Pins, Nullbudget ohne Solveraufruf,
ungültiger Rahmen und Abfrage derselben physischen Geometrie. Der Replay-Treiber
[water_library_replay.py](../test/experiments/water/water_library_replay.py) verlangt
Originalmanifest, vorher eingefrorene Vergleichs-NPZ samt QP-Dateien und den tatsächlich
geladenen Source-/Wheel-Paketpfad ausdrücklich. Er prüft Originalarrays/Vertices und
die tatsächlichen P/A/q/b exakt; Source/Wheel sowie zwei synthetische gemeinsame
Kontakte nach +0,25m Prioränderung werden getrennt verglichen. Die synthetischen Pins
sind keine beobachteten Pegel. Bestehender OSQP-Kontrolloutput wird wiederverwendet.
Weitere Kontakttests prüfen Innen-/Kantenpunkte, widersprüchliche Bedingungen,
außerhalb liegende und mehrdeutige Flächen sowie Begrenzung vor Kontaktaufbau.
Der eigenständig mit versionierten Originaleingaben ausführbare
[Punktkontaktvergleich](../test/experiments/water/point_contact_experiment.py) verschiebt
weiche Ziele um +0,25 m und hält drei ausdrücklich synthetische Punkte der zuvor
gelösten Originalfläche fest. Das prüft den Kontaktvertrag, keine Beobachtungspegel.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:lib/tests:test/experiments/water \
  .venv/bin/python -W error -m unittest test_water_point_contacts test_water_profiles test_physical_river_fit \
  test_linear_physical_river test_native_river_inputs test_geodetic_facet \
  test_river_membership test_river_surface test_profile_mass test_profile_precision
```

Wheel-Bau, Installationsziel, Vergleichsbilder und Logs gehören ausschließlich ins
System-Tempverzeichnis. Der Replay-Treiber kopiert keine Quelle/Assets ins Paket.
Quellenepoch/Provider-IDs bleiben beim Originalfixture; kein Anbieter- oder Cachezugriff.
