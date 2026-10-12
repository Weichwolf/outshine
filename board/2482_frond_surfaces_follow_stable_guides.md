Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, fronds, organs

# Frond surfaces follow stable guides

## Ergebnis und Besitzer
Eigenständige Frondflächen aus stabilen Führungskurven und eigenen Blatt-/Fiederformen erzeugen.
2348 besitzt SpeedTree-Funktionsanalyse, 2346 die Organ-/Pflanzenpläne, 2351 räumliche Rahmen.
Funktionsproben vor weltweiten Pflanzenrezepten; keine Arten- oder native Freigabe.

## Bedarf und Verfahren
Der Generator besitzt bislang Blätter/Grashalme, aber keine Frond-/Meshbindung an eine Führungskurve.
Ghidra an CFrondNode: ComputeFrondSkin (0x1b8ab20), ComputeFrondSkinFromMesh (0x1b842f0),
ComputeSkin (0x1b8ebb0), ComputeFrame (0x1b9fcd0), BuildPropertyRecs (0x1b96480),
BuildShapePropertyRecs (0x1b91f30); sechs Funktionen, beide Läufe terminal erfolgreich.
ComputeSkin wählt direkt erzeugte Flächen oder den Meshpfad; Frame übernimmt Elternlage/-transform.
Meshpfad nutzt Asset-Bounds und interpolierte Guide-/Spine-Daten; vollständige Feldbindung offen.
ELF-Strings belegen Breite/Höhe, Fold/Curl/Roll, Boundary, Unification und Abschnittsvarianz.
Gleiche stabile Guide- und Organpläne für Unterteilungen erhalten, Bedarf vor teurer Geometrie bestimmen.
Eigene räumliche Führung und Querform mit korrekten Differentialnormalen; keine fremden Assets/Texturen.
[SIGGRAPH-Kurs 2003](../doc/references/README.md), Algorithmus 4: Fiedern aus Elternprofilen,
Länge/Breite/Winkel an gemeinsamer Mittelachse. PDF-Seiten 63/64 samt botanischen Vergleichsbildern geprüft.
Gebündelte und skalare Auswertung bei gleichen Formen, Geometrieanforderungen und Inputs vergleichen.

## Umsetzung und Beleg
`frond_pattern.py`: eigene flache Ausgangsflächen, stabile Pinna-/Spitzen-IDs und Anker.
33 Teilblätter; 1.518 grobe / 6.204 feine Dreiecke, alle gemeinsamen Punkte exakt gleich.
Budget vor Prototypmeshing/Organpositionen prüfen; grobe Flächen entstehen direkt, ohne feinen Vorlauf.
Quell- und Verformungsprodukt teilen 1-Mio.-Vertex-/2-Mio.-Dreiecksgrenzen; auch überhöhte Anfragen
enden vor Prototyp-/Organaufbau. Drei Budgetfälle mit absichtlich verbotenen Aufrufen unabhängig geprüft.
`frond_surface.py`: physische Kreisführung, Breiten-/Höhen-/Rollprofile, quadratische Querwölbung.
Positionsabbildung und Jacobi-Matrix aus derselben Form; Normalen mit Kofaktormatrix transformieren.
Konservative Quader-/Profilgrenze hält den ganzen Querschnitt innerhalb regulärer Krümmung.
Vier Rezepte, je 97 unabhängige Punkte und Dreiecks-/Normalenprüfung an Fiedern und Mittelachsen.
Scipy-Rotationen, numerische Ableitungen und inverse Normalenlösung sind unabhängige Referenzen.
Größter beobachteter Jacobi-Fehler 1,88×10⁻⁹; Topologie, UVs und IDs exakt erhalten.
Frische Messprozesse: gebündelt 6,42–7,33 ms gegen punktweise 1.263–1.451 ms, je drei Läufe.
Peak-RSS bis 82,0 / 83,4 MiB einschließlich Imports/Eingabe; Rohprodukt 696.828 Bytes.
Ausgangsfläche 4,49 ms separat; gleiche Produkte durch vollständige Arrayhashes geprüft.
Fertig gespeicherte Mesh-/Placementprodukte und vollständige Guide-/Organpläne für spätere Details.
Kamera und Unterteilung gehören nicht zur Planidentität; Rosette teilt dieselben Frondprodukte.

## Abnahme und offene Lücke
Unabhängige Differential-/Normalen-, Anschluss-, Unterteilungs- und Budgetprüfungen bestanden.
2560×1440-Formvergleich, Rosette und Nahansicht desselben Produkts optisch geprüft.
Vollständige wiederverwendbare Produkte speichern; Bildprobe zeigt global verformte, noch einfache Fiedern.
Kosten und Speicher getrennt messen; Original-Oracle-Semantik von eigener Interpretation unterscheiden.
Vollständige SpeedTree-Property-/Original-Mesh-Oracle-Bindung, Singularenden, lokale Fiederbiegung,
Wind/Wachstum, LOD-/Arten- und native Freigabe bleiben offen. Kreisführung ersetzt keine freie Spine.
