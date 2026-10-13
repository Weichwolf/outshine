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
Wachstum der Blattkontur bleibt offen; geschlossene Feinadern ergänzen unten den festen Formplan.
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
Zwei übrige Quellen der schmalen Form als Stillstand ausgewiesen; keine pauschale Abdeckung.
`leaf_vein_field.py`: maximierte sich verjüngende Segmenthüllen in Metern; keine additive Knotenverdickung.
96×96-Materialvergleich und unabhängige numerische Minimierung: maximal 1,33e−14 Fehler.
Materialversion 2: linearer Farbraum, Bump 60 µm, gespeicherte Transmission; PNG-Quantisierung ≤0,5/255.
52 gemeinsame Nah-/Grobvertices je Form exakt in Position und UV, dieselben gespeicherten Karten.
Ober-/Unterseite in 2560×1440 betrachtet: matte Adern ohne Knotenverdickungen; kein SpeedTree-Niveau.
SpeedTree Games/Broadleaf/Leaves/Front_02 und Normal optisch betrachtet: feine geschlossene Zellen,
subtile Farb-/Reliefvariation; keine Übernahme und kein kontrollierter Gleichwertigkeitsbeleg.
Vorbelegte Mittelrippenstationen wegen regelmäßigem Queradern-Raster verworfen;
aus einem Wurzelknoten gewachsene Mittel-/Seitenadern erhalten die Quellenabhängigkeit.
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

## Geschlossene Feinadern
Runions Abschnitt 4 benötigt relative Nachbarn statt nur eines nächsten Aderknotens.
Zuerst vollständige Paarprüfung gegen exakte Vorauswahl in acht 45°-Sektoren vergleichen.
Ein näherer Punkt im selben Sektor blockiert jeden ferneren Kandidaten; alle minimalen
Distanzgleichstände und mit der Quelle zusammenfallende Knoten erhalten. Alle Knoten bleiben Blocker.
Streng offene Linsen als explizite Gleichstandskonvention; keine feste k-Nächste-Nachbarn-Näherung.
Quellentags und Zusammenführung nutzen diesen Baustein; Netzprodukte unten separat geprüft.
Optisch geprüft: PDF-Seite 5, Gleichung 2/Figur 7; Sektorverfahren ist eine eigene Alternative.
Für 0<ru<rv und Winkel<45° gilt |u−v|²=rv²+ru²−2ru·rv·cos(Winkel)<rv².
595/532/881 gespeicherte Knoten, je 128 Quellen, fünf abwechselnde Läufe: identische Nachbarlisten.
Dicht 127,03–338,06 ms, Sektoren 49,02–65,21 ms; Prozess-Peak 86,13 MiB inklusive Referenzarrays.
174 Quellen gegen unabhängige Definition; Gleichstände/Duplikate/Kollinearität/Quellkontakt geprüft.
Gegenbeispiel: entfernter relativer Nachbar fehlt unter den nächsten acht Punkten.
32 exakt gleich weit entfernte Nachbarn bleiben vollständig erhalten; keine Acht-Nachbarn-Grenze.
Beide Verfahren begrenzen eine Anfrage auf 600 Mio. Blockerpaare; Gleichstände können quadratisch kosten.
Quellentags an beeinflusste Nachfolger weitergeben und nach Verlassen des
Einflussbereichs entfernen. Erreichte Quellen erhalten gemeinsame Netzknoten; Verbindungen
und verbleibende Quellen explizit speichern. Anfangsnetz bleibt erhalten, neue Feinadern haben
zunächst einen eigenen einheitlichen Radius; keine rekonstruierte hydraulische Breitenlösung.
Segment-Innenlage konservativ über Profil-Ableitungsgrenzen prüfen; ungelöste Quellen ausweisen.
Gleiche Wachstumsprodukte mit beiden Nachbarschaftssuchen und unabhängige Zyklusprüfung verlangen.
Drei feste Proben: 49/50/44 unabhängige Schleifen; 191 von 192 Quellen abgeschlossen.
Eine gelappte Quelle bleibt bei konservativer Konturprüfung stationär; zehn Segmente verworfen.
Quellenmarken wandern zur Spitze: asymmetrische Probe erste Ankunft Schritt 10, Abschluss 33.
Dicht/Sektoren erzeugen identische Knoten, Kanten, Quellhubs und Ankunftszeiten (drei Läufe).
Wachstumsmediane dicht 1123–2725 ms, Sektoren 304–449 ms; Prozess-Peak 138,13 MiB inklusive Referenzen.
Shapely 2.2.0/GEOS 3.14.1 verknüpft Kreuzungen: 180–303 ms inklusive geometrischer Gegenprüfung.
Gefiltertes Feinmaterial aus Hauptkarten 818–1305 ms; einschließlich Rasterung/PNG-Export.
Frischer Cacheprozess prüft korrekte Hub-IDs nach Neuindizierung und je 52 gemeinsame LOD-Vertices.
Feinradius einheitlich je Form, Reliefgewicht 0,25; hydraulische Breiten und natürliche Dichte offen.
Nahaufnahme in 2560×1440 vorher/nachher geprüft: Feinadern etwas ruhiger, kleiner Bildgewinn;
Primäradern noch kantig, Farbvariation und natürliche Maschendichte unzureichend.
Reproduktion: `leaf_closed_experiment.py` auf Cache aus `leaf_material_experiment.py` und
`leaf_material_replay.py`; `leaf_closed_reload.py` prüft den frischen Ladeprozess.

## Texelfilterung
[Heckbert 1986](../doc/references/vegetation/ieee/1986-survey-texture-mapping.pdf), Seite 6 optisch geprüft;
Textfassung ohne Figuren. SIGGRAPH 1986 „Filtering by Repeated Integration“ als Alternative gesichtet:
benötigt bereits integrierte Felder, hier ist das kontinuierliche Aderfeld verfügbar.
[P B R T](https://pbr-book.org/4ed/Textures_and_Materials/Texture_Sampling_and_Antialiasing) begründet Supersampling.
`leaf_texture_filter.py`: 6×6-Gaussquadratur mittelt Feinaderhöhen im UV-Texel vor Materialrasterung.
Mit B≥|dw/dt| liegt jeder Texelpunkt höchstens D=hypot(W+B,L)/(2N) vom Mittelpunkt entfernt.
Auswertung mit r+D/3 umfasst daher jede mögliche Unterstützung des ursprünglichen 3r-Kerns.
Nur diese Texel integrieren; dieselbe Quadratur und Summationsreihenfolge wie vollständige Auswertung.
Mittelpunkt, 2×2, 3×3, 4×4 zunächst verglichen; 6×6 senkt den RMS-Feldfehler unter 0,001.
`leaf_filter_experiment.py` vergleicht vollständige Karten, drei wechselnde Kostenläufe und je 1024
aktive Texel gegen 16×16; zwölf schwierige Texel je Form zusätzlich gegen unabhängige 64×64-Mittelpunkte.
Keine globale Integrationsfehlerschranke und keine vollständige Aliasfreiheit; Shaderfilterung bleibt nötig.
Median Vorfilterung 599–980 ms statt 4351–4764 ms; identische vollständige Karten.
RMS-Feldfehler 0,00013–0,00065 statt 0,082–0,178 an den gemessenen aktiven Texeln.
