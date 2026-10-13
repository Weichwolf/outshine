Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, surfaces, bark

# Spatial graph wood has organic connected joins

## Ergebnis und Besitzer
Die räumliche Graphpflanze erhält gemeinsame Holzflächen und lokale organische Astansätze.
2485 besitzt kontinuierliche Achsen/Anker, 2353 Vereinigung/Kragen/Normalen, 2481 Rindenkoordinaten.
Bestehende Anker/Blätter bleiben erhalten; hochwertige Pflanzenrezepte folgen nach dem Funktionsnachbau.

## Verfahren und Quellen
SIGGRAPH-/Ghidra-Welding-Befunde aus 2352/2353 zuerst; keine Gleichsetzung mit Mesh-Booleans.
[Bloomenthal, SIGGRAPH 1985](../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf)
begründet lokale organische Anschlüsse; 2353 besitzt das optisch geprüfte Booleans-Paper/Normalenverfahren.
Ghidra-Lauf 23: BuildWeldingPropertyRecs terminal erfolgreich; 18 ELF-Namen für Freigaben,
Raycasts, Weld-Skalar, Licht-/Textur-/Noise-Mischung, Offset und Ausbreitung bestätigt.
Das ist Property-Inventar, kein numerischer Original-Weld-Beleg oder Beleg für Booleans.
Gemeinsame Körper aus kanonischen Achsflächen, lokale Eltern-/Kindfelder nur bei Nahbedarf ergänzen.
Räumliche Elternachsen und profilierte/elliptische Querschnitte im Anschluss berücksichtigen.
Rindenkoordinaten aus tatsächlichen Achsen und durchgehendem geerbtem Verlauf; Kosten/Alternativen messen.

## Umsetzung und geprüfter Stand
`graph_wood_axes.py`: vollständige Graphachsen, transformierte Flächen und allgemeine physische Charts.
`graph_collars.py`: lokale Eltern-/Kindintervalle aus tatsächlichen Kurven; Ellipsen über eingeschriebene
Hüllradien berücksichtigen. Geschlossene Originalachsen vollständig beibehalten und Zusatzhüllen vereinigen.
`graph_wood_surface.py`: Blockfelder, vorhandene Embree-/Double-Normalenangleichung und lokale Vereinigung;
direktes Grobprodukt ohne Nahkragen. `graph_wood_material.py`: geerbte Charts plus kompakte Kindübergänge.
`spine_closest.py`: Endpunkte und alle reellen stationären Wurzeln des kubischen Abstandspolynoms.
[NumPy polycompanion](https://numpy.org/doc/stable/reference/generated/numpy.polynomial.polynomial.polycompanion.html)
und gebündelte Eigenwerte; Kontrollpunkt-Boxen liefern konservative Vorauswahl mit numerischem Zuschlag.
279 unabhängige Punkte gegen unterteilte Scalar-Minimierung; kleinster Abstand bis Rundungsfehler gleich.
Drei Seeds: 10.484–10.666 Dreiecke, geschlossene orientierte Einzelkomponente/Eulerzahl zwei,
188 unabhängige Innenproben je Pflanze; 41 Blattanker vor/nach Bearbeitung unverändert.
Verlust zum Originalholz ≤4,49e−12 m³, unter dem 1e−10-m³-Probenbudget; Zusatzvolumen rund 5,3–5,7e−7 m³.
Keine Normalenumkehr; Rindenfelder endlich/positiv, dichte/indexierte Auswertung bytegleich.
Grobprodukt 694 Dreiecke und null Kragen; source-station-Innenproben und Originalholz erhalten.
Kontinuierliche Kurvenpunkte liegen teilweise schon außerhalb grober Ausgangsflächen: die erste
Innenprobe verlangte fälschlich eine exakte Kurvenhülle. Erhaltung der tatsächlichen Ausgangsfläche
und Kurvenapproximation getrennt prüfen; gültige geometrische/Pixel-LOD-Grenzen bleiben offen.
Drei lokale CPU-Läufe auf 5.335 echten Materialpunkten: skalar 1.753 ms, Batch 308 ms,
begrenzter Batch 82,5 ms; 42.680 → 9.456 stationäre Abfragen, gleiche Parameter/Abstände.
Indexierte Rinde 109 statt dicht 115 ms; 10.161 statt 21.340 Achspunktabfragen, identische Felder.
Blockkragen 325 statt dicht 442 ms, alle Mesharrays identisch; 172,56 MiB Peak des Prüfprozesses.
Seed 17: Achsen 4,0 / Kragen 341,6 / Normalen 126,2 / Vereinigung 206,0 / Rinde 118,7 ms,
jeweils lokale serielle Stufen; keine Native-/GPU-/Gerätezeit. Die erhebliche Nahkostenlücke bleibt offen.
Vier Ansichten in 2560×1440 optisch geöffnet: Gesamtpflanze, Grobprodukt, Blattnähe und Astanschluss.
Der Anschluss ist geschlossen und trägt durchgehende Rinde; lokale Facetten und die verwirbelte
Rindenrichtung am Kragen bleiben sichtbar. Das Funktionsbeispiel erreicht noch keine SpeedTree-Bildqualität.

## Abnahme
Geschlossene orientierte Flächen, erhaltenes Originalholz und unveränderte Organanker unabhängig prüfen.
Begrenzte Nah-/Grobprodukte aus demselben Plan, sichtbare Anschluss-/Materialprüfung in 2560×1440.
Aufbau, Feld, Vereinigung, Normalen, Attribute und Bytes getrennt messen; commit/push/showcase.
Vollständige Original-Property-/Wind-/Artenbindung und native Kosten bleiben offen.
