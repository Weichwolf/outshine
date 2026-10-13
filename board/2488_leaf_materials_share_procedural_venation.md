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
Metallic-Roughness bleibt die Basis; zusätzliche Blatttransluzenz ist als Erweiterung erlaubt.
Alle Karten prozedural erzeugen und als Assets cachen; Referenztexturen ausschließlich vergleichen.

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
Drei abwechselnde Läufe: dichte Erzeugung Median 186,93–595,53 ms, cKDTree 50,85–113,64 ms.
`leaf_vein_field.py`: maximierte sich verjüngende Segmenthüllen in Metern; keine additive Knotenverdickung.
96×96-Vergleich beider Verfahren: Farbe/Relief/Rauheit stimmen bis auf Rundungsfehler überein;
unabhängige numerische Minimierung: maximal 1,33e−14 Fehler an 97/98 Punkten je Graph.
Vollständige 512×512-Karten 346–407 ms indexiert, 356.013–402.255 ausgewertete Punkt-/Segmentpaare.
Gemeinsames Relief steuert Bump mit 60 µm statt 180 µm; mattere Oberfläche, linearer Farbraum.
Materialversion 2 benennt die Hülle; Transmission wird aus dem gespeicherten Rezept gebunden.
PNG-Quantisierung nach Laden ≤0,5/255; Metadaten/Netz/Mesh/Materialkarten separat gespeichert.
Prüfprozess 201,37 MiB Peak, einschließlich Imports/dichter Vergleichsarrays; kein Gerätebudget.
`leaf_material_replay.py`: 52 gemeinsame Nah-/Grobvertices je Form exakt in Position und UV;
Grobprodukte verwenden dieselben gespeicherten Materialkarten ohne Ader-/Materialneuberechnung.
Ober-/Unterseite in 2560×1440 optisch betrachtet: Knotenverdickungen entfernt, mattere flachere Adern.
Feinmaschen/Farbvariation und kalibrierte Lichtdurchlässigkeit fehlen; noch kein SpeedTree-Niveau.
SpeedTree Games/Broadleaf/Leaves/Front_02 und Normal optisch betrachtet: feine geschlossene Zellen,
subtile Farb-/Reliefvariation; keine Übernahme der Assets und kein kontrollierter Gleichwertigkeitsbeleg.
Vorbelegte Mittelrippenstationen nach Nahbild wegen regelmäßigem Queradern-Raster verworfen;
aus einem Wurzelknoten gewachsene Mittel-/Seitenadern erhalten stattdessen die Quellenabhängigkeit.

## Unterteilungsunabhängige Aderhülle
Addierte Segmentkerne erzeugten Verdickungen an Stützpunkten. Material bleibt unabhängig von
kollinearer Unterteilung, auch bei linear veränderlichem Radius. Eine maximierte kompakte
Aderhülle ersetzt die Summierung; Extremum des normierten Segmentabstands analytisch bestimmen.
Für q(t)=(A−2Bt+Ct²)/(r0+dt)² sind Endpunkte und t=(Br0+Ad)/(Cr0+Bd)
bei positivem Nenner ausreichend. Maximierte Hüllen erhalten daher jede lineare Unterteilung.
Die Indexkugel mit Radius halbe Segmentlänge+3×max(r0,r1) umfasst die gesamte Unterstützung.
An Hüllenwechseln ist globale C1-Stetigkeit ausdrücklich nicht bewiesen.
Drei Radiusprofile, je 2.145 Punkte: 1/2/7/31 Segmente, dichte/indexierte Auswertung ≤5,3e−15 verschieden.
Analytischer Querschnitt und unabhängige numerische Minimierung geprüft; altes Modell +0,233 Knotenhöhe.
Reliefhöhe/Rauheit/Farbkontrast gemeinsam angepasst; Einzelbeiträge zur Bildwirkung nicht isoliert gemessen.

## Geschlossene Feinadern: nächster Funktionsbaustein
Runions Abschnitt 4 benötigt relative Nachbarn statt nur eines nächsten Aderknotens.
Zuerst vollständige Paarprüfung gegen exakte Vorauswahl in acht 45°-Sektoren vergleichen.
Ein näherer Punkt im selben Sektor blockiert jeden ferneren Kandidaten; alle minimalen
Distanzgleichstände und mit der Quelle zusammenfallende Knoten erhalten. Alle Knoten bleiben Blocker.
Streng offene Linsen als explizite Gleichstandskonvention; keine feste k-Nächste-Nachbarn-Näherung.
Quellentags, Zusammenführung und echte geschlossene Adergraphen bleiben danach umzusetzen.
Optisch geprüft: PDF-Seite 5, Gleichung 2/Figur 7; Sektorverfahren ist eine eigene Alternative.
Für 0<ru<rv und Winkel<45° gilt |u−v|²=rv²+ru²−2ru·rv·cos(Winkel)<rv².
595/532/881 gespeicherte Knoten, je 128 Quellen, fünf abwechselnde Läufe: identische Nachbarlisten.
Dicht 127,03–338,06 ms, Sektoren 49,02–65,21 ms; Prozess-Peak 86,13 MiB inklusive Referenzarrays.
174 Quellen gegen unabhängige Definition; Gleichstände/Duplikate/Kollinearität/Quellkontakt geprüft.
Gegenbeispiel: entfernter relativer Nachbar fehlt unter den nächsten acht Punkten.
32 exakt gleich weit entfernte Nachbarn bleiben vollständig erhalten; keine Acht-Nachbarn-Grenze.
Beide Verfahren begrenzen eine Anfrage auf 600 Mio. Blockerpaare; Gleichstände können quadratisch kosten.
