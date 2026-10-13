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
Acht nächste Nachbarn sind nach Gegenbeispiel unzureichend; Limit 600 Mio. Blockerpaare je Anfrage.
Quellentags wandern zu Nachfolgern, entfallen außerhalb der Einflusszone; erreichte Quellen werden gemeinsame Knoten. Asymmetrische Probe: Ankunft 10, Abschluss 33.
Segment-Innenlage konservativ über Profil-Ableitungsgrenzen prüfen; ungelöste Quellen ausweisen.
Shapely 2.2.0/GEOS 3.14.1 verknüpft Kreuzungen. Drei Netze: 49/50/44 Schleifen, 191/192 Quellen erledigt.
Eine gelappte Quelle bleibt stationär, zehn Segmente verworfen. Dicht/Sektoren liefern identisches Wachstum.
`leaf_closed_reload.py`: tatsächliche Hub-IDs nach Neuindizierung, Netze/Karten und 52 gemeinsame LOD-Vertices.
Feinradius je Form einheitlich, Reliefgewicht 0,25; hydraulische Breiten/Dichte offen. Reproduktion: `leaf_closed_experiment.py` auf Cache aus `leaf_material_experiment.py`/`leaf_material_replay.py`.

## Texelfilterung
[Heckbert 1986](../doc/references/vegetation/ieee/1986-survey-texture-mapping.pdf), Seite 6 optisch geprüft.
`leaf_texture_filter.py`: 6×6-Gaussquadratur im UV-Texel; B≥|dw/dt|, D=hypot(W+B,L)/(2N).
Radius r+D/3 umfasst die vollständige ursprüngliche 3r-Unterstützung; identische Summationsfolge.
`leaf_filter_experiment.py`: volle Karten bitgleich, 1024 Proben gegen 16² und zwölf gegen 64² Quadratur.
Mediane 599–980 statt 4351–4764 ms; Stichproben-RMS 0,00013–0,00065 statt 0,082–0,178.
Keine globale Fehlerschranke; Bildgewinn klein, Shaderfilterung bleibt nötig.

## Wachstum steuert Aderarchitektur
`leaf_development.py`: 160 Wachstumsschritte mit je 24 Darts, höchstens 100 Beruhigungsschritte, 2048 Knoten.
Quellengeburt mit Abstand zu Adern/Quellen; eigener apikal gesetzter Anfangsattraktor bei 90 % der Länge.
Plan in reifen Materialkoordinaten; Nähe/Richtung im wachsenden Blatt, Geburtsabstand 0,08·min(W,L).
Eigene Dehnung F(x,y)=σ·(x,y−kL(x/W)²), k=2(1−σ), σ∈[0,03;1]; explizite Inverse, det(J)=σ²>0.
Quadratischer Rückweg: Sehnenfehler |k|L(Δx/W)²/4 auf min(W,L)/2000 begrenzen; Segmente in Kontur zertifizieren.
Richtungsbindung 0,6 bei aktiven Quellen; eigene Geometrie, kein biologisch rekonstruierter Tensor.
`leaf_development_experiment.py`: drei wechselnde dichte/indexierte Läufe, gleiche Graphen/Bilanzen/Historie.
852/854/1710 Knoten, GEOS bestätigt Bäume; 0/2/0 offene Quellen, Beruhigungs-/Knotenbudget separat geprüft.
`leaf_development_checks.py`: 128 Deformationspunkte, Sehnenmitte und numerische Jacobi-Determinante.
`leaf_development_reload.py`: fünf echte Produkte und gemeinsame LOD-Koordinaten; Bildvergleich dreier Wachstumsmodelle.
Gebogene Seitenadern lesbarer; natürliche Farbe, Artenkalibrierung und Rand-/anisotrope Wachstumsrezepte offen.

## Kopplung von Haupt- und Feinadern
`leaf_coupled_experiment.py` lädt entwickelte Hauptpläne unverändert und ergänzt je 64 Feinaderquellen.
Die schmale Form stoppt bei 2048 Arbeitsknoten mit 43 offenen Quellen; Eingang allein hat 1710 Knoten.
Kapazität 4096 Knoten, Standard 2048; unverändert höchstens 600 Mio. Blockerpaare je Anfrage und 4096 Rohkanten.
Keine Lockerung eines Laufzeitgates: zusätzliche Kapazität/Kosten separat messen, native Budgets offen.
Drei wechselnde Läufe liefern gleiche Graphen: Sektor-Mediane 412/420/712 ms statt 2799/2807/9069 ms.
Verknüpfte Netze: 1441/1393/2326 Knoten, 41/36/44 Zyklen; 0/2/0 offene Feinaderquellen.
Zwei offene Hauptplanquellen der gelappten Form bleiben davon getrennt; keine vollständige Artenfreigabe.
Prozess-Peak 221,05 MiB einschließlich dichter Vergleichsmatrizen; Material/Verknüpfung separat im Report.
Frischer Reload bestätigt Primärpräfix/Hashes und je 52 gemeinsame Nah-/Grobkoordinaten ohne Neubau.
Optisch: Feinverbindungen sichtbar, kleiner Bildgewinn; Hauptaderenden/Kunststoffwirkung bleiben offen.

## Radiusabhängiges Relief
Bisheriges normiertes Feld gibt jeder Mittellinie Höhe eins, auch dünnen Endadern.
Kugelhülle nach Nahbild verworfen: Mittelrippe zu breit/flach. Stattdessen H(t)=r(t)·max(1−D(t)/(9r(t)²),0)³.
Für a=|v|²,b=(p−A)·v,c=|p−A|²,d=r1−r0 sind stationäre Stellen Nullstellen von
(9d³−ad)t²+(18r0d²−6ar0−4bd)t+9r0²d+6br0+5cd; Endpunkte zusätzlich prüfen.
Stabile quadratische Wurzeln, linearer Sonderfall; räumliche Kugelschranke wie beim normierten Feld.
SIGGRAPH 2005 §3.5 Seite 5 optisch geprüft: Radien, kein Reliefmodell. Ghidra ComputeSpine: RadiusProfile-Aufruf.
Eigene Reliefableitung; weder SpeedTree-Blattrelief noch botanisch gemessene Oberfläche behauptet.
Kostenvergleich ausdrücklich 128²-Punkte je Verfahren; alle Endkarten weiterhin 512², Renderings 2560×1440.
80 unabhängige Optimierungen (Fehler ≤1,3e−14 m), 1000 Unterteilungsproben; Normfeld erzeugt alte Karten bytegleich.
Dicht/indexiert identisch; Mediane 5668/5676/11364 gegen 185/197/341 ms. Peak 481,14 MiB inkl. dichter Referenz.
Nahbild optisch: feinere Adern flacher, weiche Querschnitte erhalten; stumpfe Enden/Farbgleichförmigkeit bleiben.

## Netzabhängige Endprofile
Aktiv: freie Enden von Übergängen zu Feinadern über Grade im vollständigen verknüpften Netz unterscheiden.
Nur unverzweigte Endketten ändern; Grad >2, Wurzel und innere Kreuzungen behalten ihren Radius.
STRtree erkennt innere Kreuzungen mit 1e−12 m Abstand; Profil stoppt davor, unteilbares Endsegment bleibt geschützt.
Materialprofil: maximal acht Endradien Bogenlänge, durch nächste Verzweigung begrenzt; Smoothstep-Gewichtung.
Freies Ziel 5 % des Ausgangsradius, verbundenes Ziel min(Ausgangs-, Feinradius); eigene Gestaltung, kein botanisches Gesetz.
Originalgraph/Quellen unverändert, abgeleitete Materialradien separat cachen und prüfen.
17/19/41 freie und 20/21/19 verbundene Enden verjüngt; eine terminale Verzweigung geschützt.
Unabhängig 110/111/148 Verzweigungsradien geprüft; vier synthetische Gegenbeispiele, Cache-/LOD-Replay bestanden.
Profilmediane 8,6/9,3/14,8 ms; vollständige Materialkarten 1,74/2,12/1,93 s, Prozess-Peak 148,27 MiB.
Nahbild optisch: Enden laufen feiner aus, Anschlüsse bleiben sichtbar; Farbe und Anatomiekalibrierung offen.
