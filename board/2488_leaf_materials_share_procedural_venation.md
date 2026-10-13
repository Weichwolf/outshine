Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, leaf, material

# Leaf materials share procedural venation with their shape

## Ergebnis und Besitzer
2488 erzeugt eigene Blattoberflächen; 2346 besitzt Blattgeometrie, 2481 Rinde.
Ausgangspunkt: Blattmaterial hatte nur Farbvariation und eine Mittelrippe.
Blattform, Aderplan und Materialprodukt müssen aus demselben stabilen Organrezept stammen.
Grundfunktionsproben zuerst; keine vorgezogene Freigabe botanischer Artenrezepte.

## Referenz und Verfahren
SpeedTree-10-Leaf-Mesh konsumiert Material-/Meshassets; Ghidra untersucht SetupSkinForMesh
und CopyGeneratorMaterials, ohne fremde Texturen oder Codes zu übernehmen.
[Offizielle Leaf-Mesh-Dokumentation](https://docs.unity3d.com/speedtree-modeler/manual/leaf-mesh-generator.html).
Eigenes Aderverfahren nach Runions et al., SIGGRAPH 2005,
[Modeling and visualization of leaf venation patterns](https://www.algorithmicbotany.org/papers/venation.sig2005.pdf).
Paper unter `doc/references/vegetation/siggraph/2005-leaf-venation.pdf`, auch optisch prüfen.
PDF-Seiten 2/5 tatsächlich betrachtet: Blattkonturen, nächster Aderknoten, Quellenauslöschung,
Richtungssumme und verzweigte Radiusableitung. Feste Kontur/Quellen sind ein eigener begrenzter Teilpfad.
Erster Pfad: offenes Adernetz aus einem Wurzelknoten; Attraktoren beeinflussen ihren nächsten Aderknoten,
normierte Richtungen summieren, Quellen nahe Adern entfernen, begrenzte Schritte ergänzen.
Gleichstände wählen die kleinste ID; neue Punkte nahe vorhandenen Knoten werden verworfen.
Das verhindert wiederholte identische Zweige, ist eine eigene Ergänzung und kein Paper-Gleichwertigkeitsbeleg.
Wachstum der Blattkontur und geschlossene Adermaschen bleiben eigene offene Funktionen.
Radius aus dem verbundenen Graphen ableiten; physische Feldbreiten statt auflösungsabhängiger Linien.
Farbe und Relief aus derselben Aderstruktur; versioniertes Produkt mit Mesh/UV/Materialkarten exportieren.

## Verfahrensvergleich und Abnahme
Dichte nächste-Nachbar-Zuordnung gegen cKDTree bei identischen Punkten und Anforderungen vergleichen.
Gleiche vollständige Aderpläne; Gleichstände deterministisch, leere/ungültige und begrenzte Fälle prüfen.
Verbundenheit, Elternreihenfolge, endliche Geometrie und Grenzen gegen unabhängige Prüfungen belegen.
Materialfelder mit skalarer Segmentreferenz vergleichen; gespeicherte Produkte erneut laden.
Nah-/Grobmesh verwenden dieselben Materialkoordinaten; keine Neuplanung bei Kameradrehung.
Erzeugung/Materialraster/Einlesen, Speicher und Bildwirkung getrennt ausweisen.
2560×1440-Nahansichten von Ober-/Unterseite und mehreren Blattformen tatsächlich betrachten.
Belegte SpeedTree-Operationen, eigene Ergänzung und noch fehlende Gleichwertigkeit ausdrücklich trennen.

## Gemessener Teilstand
`leaf_venation.py`: drei Form-/Seed-Proben mit je 700 gleichen Attraktoren, 595/532/881 Knoten.
Eltern vor Kindern, verbunden, positive vererbte Radien; dichte und indexierte Pläne stimmen überein.
Zwei übrige Quellen der schmalen Form als Stillstand ausgewiesen; übrige Quellen entfernt. Keine pauschale Abdeckung.
Drei abwechselnde Läufe: dichte Erzeugung Median 204,97–671,13 ms, cKDTree 54,99–126,20 ms.
`leaf_vein_field.py`: kompakte Segmentfelder in Metern, Unterstützungskugel enthält jedes wirksame Segment.
96×96-Vergleich beider Verfahren: Farbe/Relief/Rauheit ≤1,2e−16 unterschiedlich;
unabhängige skalare Segmentprüfung zusätzlich. Vollständige 512×512-Karten 327–333 ms indexiert.
Gemeinsames Relief steuert Bump; Materialrezept enthält physische Höhe und linearen Farbraum.
PNG-Quantisierung nach Laden ≤0,5/255; Metadaten/Netz/Mesh/Materialkarten separat gespeichert.
Prüfprozess 141,73 MiB Peak, einschließlich Imports/dichter Vergleichsarrays; kein Gerätebudget.
`leaf_material_replay.py`: 52 gemeinsame Nah-/Grobvertices je Form exakt in Position und UV;
Grobprodukte verwenden dieselben gespeicherten Materialkarten ohne Ader-/Materialneuberechnung.
Ober-/Unterseite/Vorher/Grobprodukt in 2560×1440 optisch betrachtet; erhabene Adern zu stark, Feinmaschen fehlen.
SpeedTree Games/Broadleaf/Leaves/Front_02 und Normal optisch betrachtet: feine geschlossene Zellen,
subtile Farb-/Reliefvariation; keine Übernahme der Assets und kein kontrollierter Gleichwertigkeitsbeleg.
Vorbelegte Mittelrippenstationen nach Nahbild wegen regelmäßigem Queradern-Raster verworfen;
aus einem Wurzelknoten gewachsene Mittel-/Seitenadern erhalten stattdessen die Quellenabhängigkeit.
