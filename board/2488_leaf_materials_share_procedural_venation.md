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
2488 erzeugt Blattoberflächen aus stabilen Form-/Aderplänen; 2346 besitzt Organformen, 2481 Rinde.
Grundfunktionen prüfen, bevor botanische Artenrezepte freigegeben werden.
Metallic-Roughness bleibt die Basis; zusätzliche Blatttransluzenz ist erlaubt.
Alle Karten prozedural erzeugen und als Assets cachen; Referenztexturen ausschließlich vergleichen.

## Referenzen und Abnahme
Ghidra: SpeedTree-10-Leaf-Mesh übernimmt Material-/Meshassets über SetupSkinForMesh und
CopyGeneratorMaterials. Kein eigener SpeedTree-Adergenerator daraus belegt, kein fremder Code übernommen.
[Leaf-Mesh-Dokumentation](https://docs.unity3d.com/speedtree-modeler/manual/leaf-mesh-generator.html).
Runions et al., SIGGRAPH 2005: [Modeling and visualization of leaf venation patterns](https://www.algorithmicbotany.org/papers/venation.sig2005.pdf).
[Lokales Paper](../doc/references/vegetation/siggraph/2005-leaf-venation.pdf): Seiten 2–5 optisch geprüft;
Figur 3 Aderordnungen, Figur 4 Rückkopplung, Abschnitt 3.3 Wachstum/Quellengeburt, Figur 7 relative Nachbarn.
SpeedTree Games/Broadleaf/Leaves/Front_02 und Normal optisch verglichen: feine geschlossene Zellen,
subtile Farb-/Reliefvariation. Keine Assetübernahme und kein kontrollierter Gleichwertigkeitsbeleg.
Alternativen verwenden identische Eingaben/Qualität; Geometrie, Bild, Kosten und Speicher separat prüfen.
Gespeicherte Netze/Karten in frischem Prozess laden; gemeinsame Nah-/Grobkoordinaten erhalten.
Ober-/Unterseite und Nahbilder mindestens 2560×1440 betrachten; funktionale Formen sind keine Artenfreigabe.

## Offene Hauptadern und Material
`leaf_venation.py`: Attraktoren beeinflussen den nächsten Aderknoten; normierte Richtungen summieren,
begrenzte Schritte ergänzen, nahe Quellen entfernen. Kleinste ID bei Gleichstand ist eine eigene Konvention.
Nahe neue Knoten verwerfen verhindert Wiederholung, ersetzt aber keinen globalen Kreuzungsnachweis.
Dichte Suche/cKDTree liefern dieselben drei Pläne: 595/532/881 Knoten, verbundene Eltern-vor-Kind-Struktur.
Zwei übrige Quellen der schmalen Form als Stillstand ausgewiesen; keine pauschale Flächenabdeckung.
Vererbte Radien aus Terminalbelastungen nach Murray; biologische hydraulische Kalibrierung bleibt offen.
Materialversion 2: linearer Farbraum, Bump 60 µm, gespeicherte Transmission; PNG-Quantisierung ≤0,5/255.
52 gemeinsame Nah-/Grobvertices je Form exakt in Position und UV, dieselben gespeicherten Karten.
`leaf_material_replay.py --folder` prüft auch neue Experimentprodukte ohne Ader-/Materialneuberechnung.

## Unterteilungsunabhängige Aderhülle
`leaf_vein_field.py`: maximierte verjüngende Segmenthüllen in Metern statt addierter Knotenkerne.
Für q(t)=(A−2Bt+Ct²)/(r0+dt)² reichen Endpunkte und t=(Br0+Ad)/(Cr0+Bd) bei positivem Nenner.
Lineare Segmentunterteilung erhält die Hülle; drei Radiusprofile, 1/2/7/31 Segmente, 2145 Punkte geprüft.
Indexkugel: halbe Segmentlänge+3×max(r0,r1) umfasst jede Unterstützung. Globale C1-Stetigkeit offen.
Unabhängige numerische Minimierung: maximal 1,33e−14 Fehler; keine auflösungsabhängige Linienbreite.
Reliefhöhe/Rauheit/Kontrast gemeinsam angepasst; Beiträge zum Bildgewinn nicht isoliert gemessen.

## Geschlossene Feinadern
Relative Nachbarn nach Runions Abschnitt 4; offene Linsen als explizite Gleichstandskonvention.
Exakte Vorauswahl in acht 45°-Sektoren: nähere Punkte blockieren fernere im gleichen Sektor.
Für 0<ru<rv und Winkel<45° gilt |u−v|²=rv²+ru²−2ru·rv·cos(Winkel)<rv².
Alle minimalen Distanzgleichstände und Quellkontakte behalten; alle Knoten bleiben Blocker.
174 Quellen gegen unabhängige Definition; 32 gleich weit entfernte Nachbarn bleiben erhalten.
Eine feste Acht-Nachbarn-Liste ist nach Gegenbeispiel unzureichend; 600 Mio. Blockerpaare je Anfrage begrenzen.
Quellentags wandern zu Nachfolgern und entfallen nach Verlassen der Einflusszone;
erreichte Quellen werden gemeinsame Netzknoten. Asymmetrische Probe: Ankunft 10, Abschluss 33.
Segment-Innenlage konservativ über Profil-Ableitungsgrenzen prüfen; ungelöste Quellen ausweisen.
Shapely 2.2.0/GEOS 3.14.1 verknüpft Kreuzungen. Drei Netze: 49/50/44 Schleifen, 191/192 Quellen erledigt.
Eine gelappte Quelle bleibt stationär, zehn Segmente verworfen. Dicht/Sektoren liefern identisches Wachstum.
Aktuelle Mediane dicht 1123–2725 ms, Sektoren 304–449 ms; Prozess-Peak 138,13 MiB inklusive Referenzen.
Kreuzungsverknüpfung 180–303 ms, gefiltertes Feinmaterial einschließlich PNG-Export 818–1305 ms.
`leaf_closed_reload.py`: tatsächliche Hub-IDs nach Neuindizierung, Netze/Karten und 52 gemeinsame LOD-Vertices.
Feinradius je Form einheitlich, Reliefgewicht 0,25; hydraulische Breiten und natürliche Dichte offen.
Reproduktion: `leaf_closed_experiment.py` auf Cache aus `leaf_material_experiment.py`/`leaf_material_replay.py`.

## Texelfilterung
[Heckbert 1986](../doc/references/vegetation/ieee/1986-survey-texture-mapping.pdf), Seite 6 optisch geprüft,
Textfassung ohne Figuren. SIGGRAPH 1986 „Filtering by Repeated Integration“ als Alternative gesichtet.
[P B R T](https://pbr-book.org/4ed/Textures_and_Materials/Texture_Sampling_and_Antialiasing) begründet Supersampling.
`leaf_texture_filter.py`: 6×6-Gaussquadratur mittelt Feinaderhöhen im UV-Texel vor Materialrasterung.
Mit B≥|dw/dt| liegt jeder Texelpunkt höchstens D=hypot(W+B,L)/(2N) vom Mittelpunkt entfernt.
Auswertung mit r+D/3 umfasst daher jede Unterstützung des ursprünglichen 3r-Kerns; nur diese Texel integrieren.
Identische Quadratur/Summationsreihenfolge wie vollständige Auswertung, ungültige Radien vor Aufweitung ablehnen.
`leaf_filter_experiment.py`: vollständige Karten, drei Kostenläufe und je 1024 aktive Texel gegen 16×16;
zwölf schwierige Texel je Form zusätzlich gegen unabhängige 64×64-Mittelpunkte. Keine globale Fehlerschranke.
Median 599–980 ms statt 4351–4764 ms; RMS 0,00013–0,00065 statt 0,082–0,178 in diesen Proben.
Vorher/Nachher-Nahbild: etwas ruhigere Feinadern, kleiner Bildgewinn; Shaderfilterung bleibt nötig.

## Wachstum steuert Aderarchitektur
`leaf_development.py` ergänzt laufende Quellengeburt mit Abstand zu Adern und bestehenden Quellen.
160 Wachstumsschritte mit je 24 vorgegebenen Darts, danach höchstens 100 Beruhigungsschritte;
maximal 2048 Knoten. Ein apikal gesetzter Anfangsattraktor bei 90 % der Länge ist eine eigene Kalibrierung.
Quellen/Graph in reifen Materialkoordinaten speichern; Nähe und Wachstumsrichtung im aktuellen Blatt messen.
Gleichförmiges Wachstum allein ergibt zu rechtwinklige Seitenadern; Richtungsbindung rundet nur Anschlüsse.
Eigene geometrische Dehnung F(x,y)=σ·(x, y−kL(x/W)²), k=K(1−σ), σ von 0,03 bis 1.
Versuchsrezept: K=2 und Geburtsabstand 0,08·min(W,L).
Die explizite Inverse und det(J)=σ²>0 verhindern Faltungen; kein biologisch rekonstruierter Wachstumstensor.
Inverse eines geraden Wachstumsschritts ist quadratisch: Sehnenfehler genau |k|L(Δx/W)²/4.
Schrittlänge so begrenzen, dass der Fehler in reifen Koordinaten ≤min(W,L)/2000 ist;
Endsegmente zusätzlich innerhalb der reifen Kontur zertifizieren. Herkunft und lokale Fehlergrenze getrennt.
Richtungsbindung 0,6 an die eingehende Ader ist eine eigene Ergänzung; Quellen müssen weiterhin ziehen.
`leaf_development_experiment.py`: feste Kandidaten, drei wechselnde Läufe; dichte/indexierte Suche
liefern gleiche Graphen, Quellenbilanz und Historie. Schnellere gemessene Variante je Probe exportieren.
Wachstumsmediane je schnellerer Variante: 516/256/551 ms; Material/Export 754/443/886 ms separat.
Prozess-Peak 135,97 MiB einschließlich Vergleichs-/Rasterdaten; kein nativer Gerätebeleg.
852/854/1710 Knoten; unabhängige GEOS-Verknüpfung bestätigt je einen Baum ohne zusätzliche Kreuzungen.
0/2/0 ungelöste Quellen; Beruhigungs-/Knotenbudget und Quellenabstände separat geprüft.
`leaf_development_checks.py`: 128 Deformationspunkte, exakte Sehnenmitte, numerische Jacobi-Determinante.
`leaf_development_reload.py`: fünf echte Netz-/Materialprodukte frisch geladen; gemeinsame Grobkoordinaten geprüft.
3D-Vergleich isoliert feste Quellen, gleichförmiges und ortsabhängiges Wachstum bei gleichem Blattmaterial.
Gebogene Seitenadern sind lesbarer; Feinaderkopplung, natürliche Farbe, Artenkalibrierung und
vollständige Rand-/anisotrope Wachstumsrezepte bleiben offen. Kein SpeedTree-Niveau.
