# Reale Walmdachpläne für die native Integration

`hip-plans.json` enthält die 48 realen Grundrisse aus dem Vergleich in Issue #67:
je 16 kantenreiche Quellen aus Wien, Tokyo und Rosenheim, ursprüngliche Ringe
einschließlich Innenhöfen, stabile IDs und SHA-256 der gepinnten OpenFreeMap-Tiles.
Die Daten stammen von OpenStreetMap-Mitwirkenden (ODbL); Quell-URLs und Projektion
stehen pro Datensatz. Internet und lokale Quellcaches werden beim Lesen nicht benötigt.

Alle Koordinaten sind metrisch. `outer`/`holes` verwenden Ost/Nord relativ zum
angegebenen Place-Ursprung. Die erwarteten Knoten und Ebenen sind lokal zum
`frame.origin_xy_m`: Z ist Dachhöhe entlang der Quellflächennormalen.
Dies ist kein CGA-Weltframe und keine Geländeprobe. `source_edge` nennt Ring- und
Kantenindex nach positiver Außenring-/negativer Innenringorientierung.
Die Ebene liefert Wavefront-Distanz; Dachhöhe = Distanz × `height_factor`.
Die Datei enthält Standarddächer, keine angeglichenen Firste oder Überhänge.

Knotenreihenfolge und kollineare Unterteilungen sind keine Gleichheitsanforderung.
Native Vergleiche prüfen Facettenbesitz, geometrische Abdeckung und Höhen innerhalb
der angegebenen metrischen Toleranzen. Die Fixture ist eine Python-Planreferenz,
kein Nachweis nativer Float32-Geometrie, Geräteperformance oder vollständiger SDK-Gleichheit.

Offline prüfen, ohne erwartete Werte zu ändern:

```sh
.venv/bin/python -W error test/experiments/buildings/verify_hip_fixture.py
```

Nur ausdrücklich bei einer Referenzänderung mit vorhandenen gepinnten Quellcaches erzeugen:

```sh
.venv/bin/python test/experiments/buildings/export_hip_fixture.py
```

Die Export-Provenienz benennt Commit, Dirty-Zustand und Implementierungs-Hashes.
Normale Prüfungen schreiben die Fixture nicht neu.
