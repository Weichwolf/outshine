Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, bark, materials

# Bark coordinates follow stable branch plans

## Ergebnis und Besitzer
Die vollständige Testpflanze erhält prozedurale Rindenstruktur entlang ihrer bestehenden Astachsen.
2348 besitzt SpeedTree-Funktionsanalyse, 2353 die vereinigte Holzoberfläche, 2351 räumliche Rahmen.
Material-/Koordinaten-Funktionstest vor weltweiten Artenrezepten; keine botanische oder native Freigabe.

## Sichtbarer Bedarf und Verfahren
Aktueller Anschlussverlust (2486): gemischte Koordinaten verbiegen die Rindenfasern am Kragen.
Getrennte physische Achsfelder erst nach Auswertung mischen; Gewichte und vollständige Layer exportieren.
Ghidra ComputeBlendSkin/Worker und bestehende UV-Befunde zuerst prüfen; eigene Auswertung separat belegen.
Die erste vollständige Layerprobe begrenzt Achsen/Shaderfelder explizit auf acht; große Bäume brauchen
räumliche/materialbezogene Aufteilung, bevor dieses Verfahren dort eingesetzt werden kann.
Ghidra-Lauf 25: ComputeBlendSkin (`0x1cb4e90`) und Worker (`0x1c2c920`) terminal erfolgreich.
Elternabfrage, zusätzliche Skinvertices, Profillängen und Noise belegt; genaue gepackte Blendkanäle offen.
[SpeedTree-Primärdokumentation](https://docs8.speedtree.com/modeler/doku.php?id=branch_generator)
unterscheidet Texturblendregion, Kontrast/Noise und Lichtangleichung; v8-Dokument, kein v10-Oracle-Beleg.
Die aktuellen Stämme sind glatte einfarbige Funktionskörper ohne sichtbare Rindenstruktur.
Ghidra: ComputeBranchTexCoords (0x1cbadd0), ComputeBranchVCoords (0x1c51190),
ComputeBlendedUVs (0x1c739f0), FillBaseTexCoordValues (0x1cad8e0); vier Dekompilierungen terminal erfolgreich.
BlendedUVs hebt U-Sprünge über einer halben Periode durch Periodenoffsets auf;
fünf zyklische Glättungsdurchläufe mit Faktor 0,25. ELF-Konstanten 0,5/0,25 gegengeprüft.
V-Koordinaten verwenden Elternaufrufe, Materialseitenverhältnis und Profile; Feldbindung offen.
Eigene räumliche Materialkoordinaten aus demselben kameraunabhängigen Astplan, keine SpeedTree-UV-Kopie.
Kreisachsen analytisch auf Bogenlänge und zwei lokale Querkoordinaten projizieren.
Elternphase am Ansatz vererben, Kinderchart innerhalb begrenzter radialer Unterstützung weich einmischen.
Gemeinsame Meshvertices erhalten ein Feld; kein Winkel-U und damit keine umlaufende UV-Naht.
Das ist keine verzerrungsfreie globale Parametrisierung oder vollständige SpeedTree-Propertybindung.
Keine Fototexturen oder fremden SpeedTree-Materialien übernehmen.
[Perlin/SIGGRAPH 1985](../doc/references/README.md) begründet räumliche Textur/Rauschkomposition.
Eigenes anisotropes Blender-Rauschen steuert Farbe/Normalen in Metern; dünne Zweige bleiben glatter.
Vollständiges versioniertes Materialrezept mit physischer Skala/Farben/Relief liegt beim Meshprodukt;
der Bildadapter lädt es direkt, Mesh-/Organprodukte bleiben unverändert wiederverwendbar.
Blender-Nodes sind der Bildadapter; native Materialauswertung, grobe LOD-Filterung und Rindengeometrie offen.
`bark_layers.py` exportiert getrennte Achscharts, Radien und nicht negative Gewichte mit Summe eins.
Die Reihenfolge der bisherigen lokalen Überblendung bleibt erhalten; Linearwerte treffen die alten Felder.
Indexierte Abfragen vervollständigen sämtliche Ecken eines beitragenden Dreiecks, auch bei Eckgewicht null;
sonst würden interpolierte Nullkoordinaten das aktive Feld im Dreiecksinneren verbiegen.
`layered_bark_material.py` wertet Noise je Chart aus und mischt erst Höhe/Radius; Farbe und Bump folgen.
Uniforme Instanzskalierung bleibt physisch korrekt; Attribute sind am Mesh und unabhängig von der Kamera.
Versioniertes vollständiges Layerprodukt, acht Felder/1 Mio. Punkt-Achsen-Paare als ausdrückliche Grenze.
Budgetfehler vor Charts melden; kein Ersatz für Materialpartitionierung großer Körper oder native Shaderkosten.
Perlin SIGGRAPH 1985 Seite 8 samt Musterbild erneut optisch geprüft; keine Fototexturen übernommen.

## Funktions- und Kostenbeleg
765 analytische Punkte: Gerade, positive/negative Kreisachsen bis π, Rundumnaht, Transformation/Skalierung.
Unabhängige skalare Nächstpunktminimierung: größter beobachteter Parameterfehler 5,36×10⁻⁹.
64 Achsen / 80.311 Holzvertices: dichte und räumlich begrenzte Felder exakt identisch.
cKDTree-Suchkugel enthält die Unterstützung über halbe Bogenlänge plus doppelten Basisradius.
Arbeit vor Feldberechnung begrenzen; leere/ungültige/zu große Anfragen expandieren weder Achsen noch Blätter.
Zusatzattribute 2.569.952 Bytes; Geometrie, Ecknormalen und alle Organprodukte exakt erhalten.
Lokale Erzeugung 0,45 s gegen dicht 4,85 s Median aus drei abwechselnden frischen Prozessen;
Peak-RSS bis 114,9 gegen 135,3 MiB einschließlich Imports/Eingabe; Einlesen separat.
Kleine Shell startet Messworker nach exec: geerbte Experiment-RSS-Spitze gehört nicht zur Messung.
Keine Runtime-/Gerätefreigabe; 382.646 statt 5.139.904 Punkt-/Achsen-Auswertungen.
`bark_experiment.py` erzeugt eigenständige vollständige Produkte, `bark_preview.py` prüft ihre Bildwirkung.
`bark_layer_experiment.py`: drei Seeds/vier Achsen, drei abwechselnde Läufe desselben verbundenen Nahmesh.
Indexiert 108,61–112,87 gegen dicht 114,14–115,60 ms Median; geringer lokaler CPU-Gewinn.
13.558–13.793 statt 20.976–21.340 Punkt-Achsen-Abfragen, einschließlich vervollständigter Dreiecksecken.
839.040–853.600 Attributbytes; Prüfprozess 152,02 MiB Peak einschließlich zweier Holzaufbauten/Imports.
Alle relevanten Chart-/Gewichte-/Radiuswerte bytegleich; je 200 unabhängige Dreiecksinnenpunkte feldgleich.
Die alten linearen Koordinaten/Radien sind aus den Layern reproduzierbar, nur die nichtlineare Auswertung ändert sich.
Meshvertices, Faces, Ecknormalen und Blattprodukte bleiben exakt; keine größere Blatt-/Artenfreigabe.
2560×1440-Anschlussaufnahme optisch gegen Vorherbild geprüft: Achsfasern statt künstlicher Verwirbelung.
84,10 s gegen vorher 72,05 s EEVEE-Bildaufbau, je ein Lauf; mehr Shaderarbeit, keine GPU-Profilerzeit.
Gesamtpflanze, Blattnähe und Anschluss tatsächlich optisch geöffnet; derselbe 2560×1440-Bildauftrag.
`pixels.py`: am Anschluss 185.344 geänderte Pixel, maximal 34/255; weiterhin null schwarze Pixel.
27 übrige Showcase-Ansichten pixelgleich, einschließlich frisch gerenderter Graph-/Legacy-Rindenansichten.

## Abnahme und offene Lücke
Gleiche Holzgeometrie und Organanker; Koordinaten und Materialdetails ändern nur die Oberfläche.
2560×1440-Nahbild und Gesamtpflanze optisch geprüft: längs gerichtete Struktur folgt dem Astansatz,
dünne Zweige glatter; keine schwarzen Pixel. Bildgewinn am Nahdetail, Krone weiterhin Funktionsprobe.
Periodische Nähte, gebogene Äste, Ansatz und dünne Zweigenden prüfen; Details vor teurer Arbeit wählen.
Vollständige SpeedTree-Propertybindung, Rindengeometrie, Wachstum, Wind und native LODs bleiben offen.
