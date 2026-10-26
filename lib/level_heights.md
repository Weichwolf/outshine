# Gemeinsame ebene Höhen

`outshine_ref.transport.level_heights.fit_level_heights` übernimmt den in
[PR #730](https://github.com/Weichwolf/outshine/pull/730) geprüften Höhenkern.
Die Bibliothek kennt Mesh, Gradrahmen und Vertexgruppen; räumliche Gruppierung,
Quellen, Klassen, Kontakte und vollständige physische Zulassung besitzt der Aufrufer.
Übernahme und aktuelle Belege: [#732](https://github.com/Weichwolf/outshine/issues/732).
[Wheel-, Geometrie-, Bild- und Kostenvergleich](../test/experiments/transport/stair_level_library_comparison.md).

```python
from outshine_ref.transport import HeightBudget
from outshine_ref.transport.level_heights import fit_level_heights

fit = fit_level_heights(
    surface,
    grade_frames,
    [first_flat_vertex_ids, second_flat_vertex_ids],
    budget=HeightBudget(time_limit_s=8, max_iterations=100),
)
```

`surface` ist ein unveränderliches `Mesh`; seine ursprünglichen Z-Werte sind
weiche Höhenpriors. `grade_frames` ist ein reelles NumPy-Array `(Faces,2,2)`;
`norm(frame @ gradient) <= 1` begrenzt die ganze Facette anisotrop.
Gruppen sind Listen/Tupel aus eindimensionalen ganzzahligen ID-Arrays oder
ID-Listen/Tupeln: je mindestens drei vorhandene, eindeutige IDs, disjunkt zwischen
Gruppen. Jede Gruppe erhält eine gemeinsame, frei gefittete Höhe. Überlappende
physische Ebenen muss der Aufrufer vorher kohärent vereinigen.

Das Ergebnis verwendet bestehendes `HeightFit`: ein eigenes unveränderliches
Float64-`Mesh` mit **tatsächlich Float32-exakten XYZ-Werten**, oder `mesh=None`
mit Grund. XY muss bereits Float32-exakt sein und bleibt unverändert; Faces ebenso.
Alle Gruppen sind exakt eben. Dies ist keine regionale Ursprungskonvention und
kein Beleg für spätere Kamera-/GPU-Transformationen (#717). Die gemeinsame
Koordinatenbasis, konforme nicht überlappende Topologie und Straßen-/Bau-/Pflanzen-
Freiräume bleiben Verantwortung des Aufrufers.

Höchstens 20 Gruppen, 4096 Vertices und 8192 Faces, zusätzlich zu kleineren
`HeightBudget`-Eingangsgrenzen. Höchstens acht Solversekunden und 100 Kegeliterationen
für freien Gruppenfit und anschließende Ebenenbindung gemeinsam. Größere Zeit-/
Iterationsbudgets werden abgewiesen. Danach verbleibende Zeit und höchstens 100
endliche Kombinationen unmittelbar benachbarter Float32-Höhen; höchstens 5 mm
Änderung gegenüber der kontinuierlichen Lösung. Ganze Gruppen ändern sich gemeinsam,
auch außerhalb der problematischen Facette. Nur betroffene Nachbarfacetten werden
je Vorschlag ausgewertet. Normale ganze D/Q-Kontakt-/Körperprüfungen bleiben nötig.

Der Abbruch wird zwischen begrenzten Solver-/Suchschritten geprüft; einzelne
Schritte können über die Zeitgrenze hinaus enden. Kein Framezeitversprechen,
allgemeiner Konvergenz-/Optimalitätsbeweis oder versteckter MILP-Fallback.
Budgetende, ungeeignete Repräsentation und fehlende gültige Kandidaten ergeben
explizite Abwesenheit. Ungültige Typen/IDs/Gruppen/Frames werden als Fehler abgewiesen.
Bereits vollständig gültige, exakt repräsentierbare Priors geben dasselbe eigene
Mesh ohne Solverarbeit zurück. Gruppen/Frames werden vor Solverfreigabe begrenzt
kopiert; übergroße Formen werden vor diesen Kopien abgewiesen.

Kein Import des numerischen Kerns durch den bisherigen `transport`-Wurzelimport.
SciPy/Clarabel bleiben die vorhandenen, verzögert geladenen Solver; Installation
bei tatsächlichem Bedarf mit `pip install ./lib[transport]`.
Experimente importieren diesen Kern. Ihre geometrischen Regionen und der negative
kontinuierliche/MILP-Verfahrensvergleich bleiben außerhalb der Bibliothek.
Bestehende Bibliotheksdateien, `Mesh`, `HeightBudget`, `HeightFit`, `Plan` und
Fachübergaben werden nicht geändert; keine neue Abhängigkeit oder SDK-Implementierung.

Fester ganzer Kontrollinput: `test/outshine/data/transport/flat-group-height-input.npz`
mit Herkunft/SHA256 im benachbarten JSON, 520 Vertices/765 Faces/vier Gruppen.
`lib/tests/test_level_heights.py` prüft dazu unabhängige Face-Normalen, exakte
Gruppen, analytische Gegenfälle, fehlende Solverarbeit, Grenzen und ungültige Daten.
Bibliotheksübernahme ersetzt weder breitere Netzplanung (#434) noch native
Integration oder Geräteabnahme (#48).
