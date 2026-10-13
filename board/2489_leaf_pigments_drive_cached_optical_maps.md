Type: feature
State: active
Architecture: ready
Priority: P0
Parent: 2488
Depends:
Area: generators, render
Tags: vegetation, python, optics, material

# Leaf pigments drive cached optical maps

## Ergebnis und Besitzer
2489 ergänzt 2488 um prozedurale Pigmentfelder und getrennte Reflexions-/Transmissionsfarben.
Bilddefizit: fast einheitlicher Grünton trotz räumlicher Adern; Farbraumübergabe ist korrekt linear.
Grundfunktionen und Pigmentzustände prüfen, keine botanischen Pflanzenrezepte freigeben.

## Quellen und Grenzen
Ghidra CopyGeneratorMaterials/SetupSkinForMesh übernehmen Materialassets; kein Pigmentmodell daraus belegt.
SIGGRAPH 2005 „Real-time rendering of plant leaves“ gesichtet; ergänzend
[Habel 2007](../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf), Seite 3 optisch geprüft:
Reflexion, Transmission und Normalen getrennt behandeln; unsere Texturen bleiben vollständig prozedural.
[Féret/de Boissieu 2024](../doc/references/vegetation/joss/2024-prospect-leaf-optics.pdf), Seite 4 optisch geprüft.
[Modellautor-Dokumentation](https://jbferet.gitlab.io/prospect/articles/prospect.html): PROSPECT-D modelliert
hemisphärische R/T aus Pigmenten, Wasser, Trockenmasse und Struktur; keine getrennte Blattober-/Unterseite.
Python-Referenzbibliothek prosail 2.0.5 und colour-science 0.4.7 nur in .venv, kein Fremdcode im Repository.
Das ist ein externes optisches Referenzmodell, kein eigener Nachbau von PROSPECT oder SpeedTree.

## Umsetzung und Abnahme
Eigene logarithmische Pigment-LUT gegen direkte Spektralauswertung prüfen, Fehler und Kosten getrennt.
Chlorophyll und Anthocyanin variieren; übrige Parameter je Tabelle fest und mit physikalischen Einheiten.
RGB unter D65 aus 400–780 nm; Spektralenergie, Farbraum, Gamut und Quantisierung ausdrücklich prüfen.
Kontinuierliche seeded Pigmentfelder in physischen Blattkoordinaten, gleiche Form-/Aderpläne und Kamera.
Reflexion/Transmission getrennt cachen und im Metallic-Roughness-plus-Transluzenz-Material verwenden.
Konvexe Shadermischung mit begrenzten Karten; keine Behauptung eines exakten PROSPECT-BRDF.
Drei Zustandsproben, direkte/Tabellenwerte, frischer Reload, Nah-/Grob-Konsistenz und 2560×1440-Bilder.
Globale botanische Kalibrierung, asymmetrische Blattseiten und reale Winkelmessungen bleiben offen.

## Verfahren und lokale Belege
`leaf_pigment_optics.py`: PROSPECT-D mit N=1,3, Car=10 µg/cm², Wasser=0,012 cm,
Trockenmasse=0,010 g/cm², Braunpigment=0, Einfallsparameter α=40°. Chl 0–80, Ant 0–10 µg/cm².
CIE-1931-2°/D65, 400–780 nm, lineares sRGB; abgeschnittenes Weiß kanalweise auf 1 normiert.
Gemeinsame Entsättigung R/T zu ihren Leuchtdichten bis min(R,T)≥0 und max(R)+max(T)≤1.
Unabhängige Bisektion prüft 80 Fälle, separate Spektralintegration und Weißnormalisierung geprüft.
Rohe RGB-Werte bilinear in log(1+Pigment) interpolieren, erst danach Gamut anpassen.
17/33/65² Tabellen gegen 772/1540/4612 Proben (Zellmitten plus 516 gemeinsame Rand-/Zufallsproben).
Maximale Fehler 0,001039/0,0002705/0,00006896; gröbste unter Prüfgrenze 1/1024: 33², 52 272 Bytes.
Gamut vor Interpolation erreicht auf gemeinsamen Proben nur 0,00674/0,00357/0,00146.
Keine globale Schranke. Drei wechselnde Läufe, gleiche 516 Abfragen: Median 662,68/0,528 ms direkt/LUT.
Tabellenaufbau 1,38 s; lokale CPU-Messung, kein nativer Gerätebudgetnachweis.

`leaf_pigment_fields.py`: zwölf seeded Wellen mit 8–40 mm Wellenlänge, tanh-begrenzte Summe.
Räumliche Chl-Variation ±30 %, Ant ±15 %; Chl am Relief bis 25 % reduziert, eigene Gestaltung.
2000 Punkte unabhängig von Batch/Reihenfolge identisch; andere Seeds unterscheiden sich.
Drei Zustände (Chl/Ant): 45/0,2; 8/0,2; 10/8 µg/cm², keine vollständige Saisonsimulation.
Materialversion 3 speichert symmetrische R/T-Näherung; Mischgewicht w=max(T)/(max(R)+max(T)).
Shaderkarten R/(1−w), T/w und w; alle begrenzt, tatsächliche 8-Bit-PNGs dekodiert geprüft.
Maximale Quantisierungsfehler 0,00155/0,00244/0,00210, separat von Gamutänderung bis 0,01461.
PROSPECT-R/T sind hemisphärisch; Principled-plus-Translucent reproduziert keine exakte Winkelverteilung.
512² Karten inklusive Schreiben/Prüfen 0,746/0,716/0,748 s; Diagnose-NPZ separat 0,85–0,89 s.
Prozesspeak 384,6 MiB inklusive Python/Bibliotheken/Diagnose, kein Runtime-Assetbudget.
Frischer Reload ohne Optikerzeugung; ursprüngliche Aderpläne und 52 gemeinsame LOD-Vertices erhalten.
Reproduktion: `leaf_pigment_table_experiment.py`, `leaf_pigment_experiment.py`,
`leaf_pigment_reload.py`, Replay/Graph-Reload aus 2488 mit `--folder .../pigment-leaves`.

## Bildabnahme und nächste Lücke
Nahbild vor/nach bei gleicher Kamera optisch geprüft: räumliche Farbe statt fast einheitlichem Grün,
aber schwächerer Feinaderkontrast. Durchlicht trennt grüne/gelbe/rote Pigmentproben sichtbar.
Das belegt die optische Grundfunktion, keine pauschale Verbesserung aller Blattmerkmale.
Nächste Bildlücke: Aderkontrast, Relief und Glanz zusammen an botanischen Referenzen kalibrieren;
separate Blattseiten und Winkelmessungen fehlen. Alle neuen Ansichten 2560×1440.
Fünf bestehende Material-v2-Ansichten nach Shaderänderung neu gerendert: pixelgenau unverändert.
42 öffentliche Renderprodukte mit aktuellen Quellen-/Implementierungshashes geprüft.
