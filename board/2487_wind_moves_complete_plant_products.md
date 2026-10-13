Type: feature
State: open
Architecture: ready
Priority: P0
Parent: 2348
Depends:
Area: generators, render
Tags: vegetation, python, wind

# Wind moves complete plant products without rebuilding geometry

## Ergebnis und Besitzer
2487 besitzt Windbindung und Bewegung aus stabilen Pflanzenplänen; 2486 besitzt ruhende Anschlüsse.
`wind_field/plan/products/experiment.py`: Holz, 41 Blätter und LOD-Anker verwenden dieselbe Bewegung.
Keine kamerabezogene Neuplanung, Vereinigung oder Rindenberechnung während der Bewegung.

## Verfahren und Grenzen
Erster eigener Pfad: kontinuierliches gerichtetes Verschiebungsfeld aus Stamm- und Astmoden.
Ruhepositionen binden einmalig Werte und analytische Gradienten; Zeit ändert nur Koeffizienten.
Gemeinsame Punkte erhalten dieselbe Abbildung unabhängig von Mesh, Organ und LOD.
Normalen folgen der invers-transponierten Ableitung. Begrenzte Amplituden verhindern lokale Faltungen.
Eine globale Determinantenschranke gilt für das kontinuierliche Feld, nicht automatisch für grobe Dreiecke.
Für F(x)=x+dφ(x) gilt det(DF)=1+d·∇φ. Horizontales d eliminiert Stamm-/Höhengate-Terme.
Astgewichte S(u)·(1−r²/R²)³ haben Richtungsableitung höchstens 1,5/L+1,7174/R;
Summe der amplitudengewichteten Grenzen muss unter 0,9 bleiben. Beispielgrenze det≥0,5848.
Stamm ist an der Wurzel fixiert; Astmoden verwenden glatte kompakte radiale Gewichte.
Dies ist eine kontrollierte geometrische Näherung, noch keine kalibrierte Balkendynamik.
Blattflattern, Windrichtungswechsel, Böen, Kollisionen und Waldaggregation bleiben offen.

## Referenz und Alternativen
Ghidra 2348: SDK-Elterndaten 0x1cc3990, VFX-Astrotationen 0x1f90500; Paketheader bestätigt
unterschiedliche SDK-/VFX-Verfahren. Keine vollständige numerische SpeedTree-Gleichwertigkeit belegt.
SIGGRAPH-Recherche ergänzt durch Habel/Kusternig/Wimmer, Eurographics 2009,
[Physically Guided Animation of Trees](https://www.cg.tuwien.ac.at/research/publications/2009/Habel_09_PGT/).
Paper unter `doc/references/vegetation/eg/habel-2009-physically-guided-animation.pdf`, Seite 3 optisch geprüft:
Hierarchiegewichte und eingespannte verjüngte Balken; spätere dynamische Kalibrierung benötigt deren Materialparameter.
Seite 4 optisch geprüft: quartischer Fit u=c₂x²+c₄x⁴ und tangentiale Längenkorrektur,
keine exakte Bogenlängenparametrisierung. Gegen numerische Integration vergleichen, bevor Genauigkeit behauptet wird.
Der originale VFX-Astshader kürzt den Ankerabstand winkelabhängig bis auf 90 %;
SDK-Normalisierung erhält den jeweiligen Ankerabstand, nicht automatisch die gekrümmte Astlänge.
Vergleich: direkte räumliche Auswertung gegenüber vorberechneten Gewichten bei identischen Eingaben.
Vorbereitung, Positions-/Normalenupdate, Datenvolumen und Bildkosten getrennt messen.

## Abnahme
Nullwind, fixierte Wurzel, endliche Werte, positive Determinante, unabhängige Differenzenprüfung
der Ableitung/Normalen und zeit-/reihenfolgeunabhängige Auswertung belegen.
Gemeinsame Anker in Nah-/Grobprodukten identisch bewegen, Dreiecke und Rindenfelder erhalten.
Mehrere Zeitpunkte in 2560×1440 rendern und tatsächlich betrachten; bewegte Darstellung nachführen.
Bild, Kosten und Referenzlücke bleiben getrennte Ergebnisse; keine Geräte-/Waldleistung behaupten.

## Gemessener Stand
Seed 17: 23.457 Nahvertices / 1.989 Grobvertices; vier Moden, 41 gemeinsame Blattanker exakt gleich.
Gespeicherte Bindung 3.002.496 / 254.592 Bytes; Positions-/Normalenupdate ohne IO/Renderer messen.
Sieben CPU-Läufe, NumPy/OpenBLAS ein Thread: Median 11,69 / 0,871 ms mit gespeicherter Bindung,
25,81 / 2,218 ms bei direkter Feldneuauswertung, numerisch identische Produkte. Kein GPU-/Gerätenachweis.
Je LOD 298 unabhängige Punktproben × fünf Zeiten: skalare Position ≤2,8e−17 m,
Differenzen-Jacobi ≤4,2e−8, Normalen ≤1,8e−8. Nullwind/Wurzel/umgekehrte Reihenfolge geprüft.
41 Zeitproben aller Dreiecke: positive Fläche und Normalenausrichtung; minimale Flächenquote 0,7204.
Maximale Kantenstreckung über alle drei Dreieckskanten und 41 Zeiten 10,03 %: sichtbare Biegung, noch keine längenerhaltende Astmechanik.
Ruhende Netze und Werte/Gradienten werden gespeichert und nach Wiederladen identisch geprüft.
LOD-Gleichheit betrifft gemeinsame Anker; geglättete Dreiecksflächen können zwischen LODs abweichen.
Auch exportierte Blattwurzelvertices liegen exakt an den 41 bewegten Ankern in beiden LODs.
Zusammengefasste Blätter speichern eigene Farbvariation; 41 Werte bleiben bei allen Zeiten erhalten.
Ruhender Vergleich plus zwei Nahzeitpunkte und eine Grobansicht verwenden denselben Farbsatz.
Frischer Offline-Prozess (`wind_replay.py`) reproduziert acht gespeicherte Meshprodukte bitgleich aus Ruhemesh/Bindung.
Alle vier 2560×1440-Ansichten optisch geprüft; grobe Stammfacetten bleiben sichtbar.
Clip-Frame 12 bei 0,5 s ist zum separat aufgebauten Standbild pixelidentisch:
3.686.400 Pixel verglichen, null Abweichungen. Der fortlaufende Mesh-/Normalenupdate erhält dessen Bild.
Windclip nach Nutzerkorrektur zurückgestellt; Geometrie-/Materialgrundfunktionen haben Vorrang.
Unfertige Videoarbeiten unter `build/experiments/vegetation/deferred-wind-video` gesichert.
