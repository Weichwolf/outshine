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
Bildverlust: geteilte Normalen an Anschlussvertices (bis 9,3° im mittleren Kragen).
Ghidra-Lauf 24: AverageNormals (`0x1ca3720`) mittelt/normalisiert drei Vektorpaare und schreibt beide Seiten.
Pure Originalroutine: 3.078 Paare einschließlich Null/Gegensätzen; eigener Double-Nachbau ≤1,36e−7
Komponentenfehler gegenüber Original-Float, beide Originalausgaben identisch. Kein vollständiges Branch-Lighting.
[Thürmer/Wüthrich 1998, Primärabstract](https://doi.org/10.1080/10867651.1998.10487487)
begründet Winkelgewichtung gegen Tesselierungsabhängigkeit; Bloomenthal Seite 3 optisch erneut geprüft.
`join_normals.py`: vorhandene Differentialnormalen nach Eckwinkel lokal mitteln, nicht Facetnormale ersetzen.
Eine eigene Erweiterung: keine Gleichsetzung mit Original-Paarmittelung oder vollständigem Papierverfahren.
Ganze Vertices bei >45° zur Mittelrichtung oder nicht auswärts gerichteter Mittelung erhalten;
Caps und Geometrie außerhalb von zweimal Kragenspanne unverändert lassen.

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
Drei lokale CPU-Läufe auf 5.335 echten Materialpunkten: skalar 1.717 ms, Batch 306 ms,
begrenzter Batch 82,4 ms; 42.680 → 9.456 stationäre Abfragen, gleiche Parameter/Abstände.
Indexierte Rinde 110 statt dicht 116 ms; 10.161 statt 21.340 Achspunktabfragen, identische Felder.
Blockkragen 324 statt dicht 440 ms, alle Mesharrays identisch; 165,66 MiB Peak des Prüfprozesses.
Seed 17: Achsen 4,5 / Kragen 402,6 / Normalen 128,0 / Vereinigung 211,0 / Rinde 108,7 ms,
jeweils lokale serielle Stufen; keine Native-/GPU-/Gerätezeit. Die erhebliche Nahkostenlücke bleibt offen.
Vier Ansichten in 2560×1440 optisch geöffnet: Gesamtpflanze, Grobprodukt, Blattnähe und Astanschluss.
Der Anschluss ist geschlossen und trägt durchgehende Rinde; lokale Facetten und die verwirbelte
Rindenrichtung am Kragen bleiben sichtbar. Das Funktionsbeispiel erreicht noch keine SpeedTree-Bildqualität.
Winkelgewichtung gegen skalare Eckwinkel, lokalen harten Split und echte drei Seed-Produkte prüfen.
Ein kontrollierter Dreieckssplit erhält die Winkelmittelung; gleiche Gewichte ändern sie um 0,0599
Vektorabstand. Flächen-/Winkel-/Gleichgewichtung bleiben separate Vergleichsergebnisse.
Winkelangleichung für drei Seeds: 15,86–16,24 ms Median, je drei Läufe auf demselben Nahmesh.
3.168–3.247 lokale Vertices, 8.057–8.303 geänderte Ecken; alle Orientierungs-/Erhaltungsprüfungen grün.
Neue Anschlussaufnahme optisch mit Vorherbild verglichen: nur kleine lokale Schattierungsglättung;
aufgewölbte Form und verwirbelte Rindenrichtung bleiben die nächsten sichtbaren Defizite.
Gespeichertes Nahprodukt: alle 3.247 lokalen Vertices mit exakt gemeinsamen Ecknormalen geprüft.
`pixels.py`: Anschlussbild 37.549 geänderte Pixel, maximal 6/255; Grobbild pixelgleich.
Gesamt-/Blattnah-/Anschluss-/Grobbild und aktueller Kostenplot in 2560×1440 tatsächlich geöffnet.

## Abnahme
Geschlossene orientierte Flächen, erhaltenes Originalholz und unveränderte Organanker unabhängig prüfen.
Begrenzte Nah-/Grobprodukte aus demselben Plan, sichtbare Anschluss-/Materialprüfung in 2560×1440.
Aufbau, Feld, Vereinigung, Normalen, Attribute und Bytes getrennt messen; commit/push/showcase.
Vollständige Original-Property-/Wind-/Artenbindung und native Kosten bleiben offen.
