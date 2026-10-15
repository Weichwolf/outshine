# Gehwegkörper hinter tatsächlichen Bordkanten (#372)

## Ergebnis und physische Grenzen

204 geschlossene Körper auf67.674,757312m² ergänzen den Python-Stand aus #364.
38.218,170362m tatsächliche Oberkanten schließen an die vorhandenen Borde an;
ursprüngliche Szene/Verkehrsflächen/Beläge/Normalen bleiben exakte Arraypräfixe.
Neu22.560Vertices/44.384Faces, gesamte Szene433.422Vertices/782.840Faces.
Die Quellnutzung wird dadurch nicht neu freigegeben. Keine gemeinsame/native API
oder Runtimeabhängigkeit geändert; kein SDK/Fremdcode verwendet.

Prozedurales Profil: vertikal12cm über demselben ausgewählten Boden, höchstens2m
XY außerhalb der unveränderten Verkehrsfläche. Gehwegmaske reicht höchstens1,82m
über die tatsächlichen Borde hinaus, ohne bestehende Bordkörper zu überdecken.
Diese Grenzen sind Rezeptparameter, keine Norm-/Vermessungsbehauptung; örtlich
schmalere/reduzierte Stücke sind kein Nachweis nutzbarer Mindestbreite einer Route.
Bestehende Querungs-/Wege-, Endport-, Weltrand- und Bauwerks-/Pfeileraussparungen
gelten weiter. Von früheren Quellen gewählte befestigte urbane Borde binden die
Auswahl; Quellen/Attribute und sämtliche lokale Gründe bleiben erhalten.

252 Kandidaten auf91.832,054972m²,204 angenommen.22 wegen degenerierter oder
invertierter Float32-Faces,26 wegen nichtmannigfaltiger Oberfläche ausgelassen.
Bestehende Straßen und Borde bleiben dabei unverändert. Limits256 optionale Körper,
262.144 zusätzliche Faces,1.024 Footprintkoordinaten,60s Aufbau/Audit,
Prozess3GiB/240CPU-s. Einzelne begrenzte Kernaufrufe können das Wandzeitlimit
überlaufen. Hier wurden weder Stück- noch Zeitlimit erreicht.

Keine vollständige Gehwegabdeckung, abgesenkten Querungen, zusammenhängenden
Fußrouten oder Gebäudezugänge. Gebäudekollisionsfreiheit und Reservationsübernahme
bleiben ohne gebundenen Buildings-Plan unbewiesen. Das0,16m-Landing aus Buildings
ist kein übernommener Bodenkontakt. Keine native/LOD-/Statik-/Geräteabnahme.

## Verfahren, Gegenfall und Alternativen

Zuerst [Chen et al., SIGGRAPH2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf)
S.2/Fig2 optisch geprüft: Quellgraph von Form trennen. Ergänzend
[Galin et al., Eurographics2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf)
S.8/Fig17–18 optisch geprüft: parametrischer Querschnitt und Gelände/Bauwerke.
Eigene Geometrie konsumiert den bestehenden installierbaren Ganzflächenkern.

Eingaben:256 tatsächlich gespeicherte Bord-Unterkappen,13.245 Ringkoordinaten.
Einzelpuffern ihrer ursprünglichen Ringe gegenüber Vereinigung vor Offset ergibt
0,099473925668m² symmetrische Differenz: polygonale Rundungssegmente hängen auch
von der Ringdarstellung ab. Dieser erste Vergleich wurde als ungleich verworfen,
nicht durch Lockerung der1e-8m²-Grenze passend gemacht. Erhaltener lokaler Gegenbeleg:
`/tmp/outshine-transport-sidewalks-372-dilation-counterexample.json`.

Beide endgültigen Varianten vereinigen daher zuerst dieselben Ringe. Verglichen:
ein gemeinsamer Buffer gegenüber einzeln gepufferten kanonischen Komponenten plus
abschließender Vereinigung. Volle Maskendifferenz0m², unbeschnittene
Hausdorffdistanz4,69e-13m. Beide252 Kandidaten/14.604 Maskenkoordinaten; gleiche
Clip-/Kontakt-/Freiraumqualität. Gewählt gemeinsamer Buffer: geringere normale
Teilzeit und Allokationsarbeit. Kein globaler Optimalitätsnachweis.

Konstruktion: ursprüngliches affines Höhenfeld und dasselbe Feld+0,12m;
feiner tatsächlicher Boden bleibt unabhängige Referenz. Ganze affine Überlappungen
in beiden Richtungen, Shell/Kappenbesitz, vollständige Footprints, Freiräume,
Breite, Gradient≤0,05, positives Volumen=Fläche×0,12 und Float32-Faces prüfen.
Für jede tatsächliche Anschlusslinie schneiden beide Kappentriangulationen
die Linie in affine Teilintervalle. Deren vollständige Endpunktmenge wird auf
beiden tatsächlichen Kappen ausgewertet; Eckpunkte allein reichen ausdrücklich nicht.
Der Gegenfall mit verborgenem inneren Anschlusspeak wird abgelehnt.

## Tatsächlicher Export und Prüfungen

Sauberer Produzent/Leser/Kosten/Renderer:
`a2f0d335ed48c236b55be20ff6298720f2936731`.
Leser konsumiert tatsächliche gespeicherte Körper/Kappen, baut sie nicht erneut auf.
204 Körper vollständig zugeordnet, gegenseitige positive Flächenüberdeckung0;
5.480 affine Anschluss-Endpunktabfragen. Maximalfehler: Bodenkontakt2,8422e-14m,
Bordoberkante9,0950e-13m, Höhe4,5519e-15m, Volumen1,3572e-11m³.
Kontaktwerte gelten für Double. Float32 prüft separat keine kollabierten/verkehrten
neuen Faces; dieselbe Kontaktgenauigkeit in Float32 wird nicht behauptet.

71 Quell-/Bibliothekskontrollen terminal0,0,315s, darunter vier neue Kontrollen
für volle Straßen-/Querungs-/Portfreiheit, gleichwertige Masken, ganze geneigte
Bordanschlüsse und verborgenen inneren Anschlusspeak. Bestehende Support-, Bord-,
Belag-, obere/untere Gleis-, Quellen- und Zulassungskontrollen bleiben enthalten.
Tatsächlicher Szenenimport prüft Positions-/Index-/Normalen-/Farbattribute exakt.
16 tatsächliche Blenderkontrollen terminal0,5,994s. Ruff/Format/Funktionslängen,
`git diff --check` und `make lint-docs` bestanden. Ein Kontrollname wurde danach
präzisiert; dieselben vier Gehwegkontrollen erneut terminal0,0,043s.
Native/Welt-/Metal-/A18-Gates bleiben offen.

## Kosten

Linux/Python3.13.5,NumPy2.2.4,Shapely2.2.0/GEOS3.14.1,einBLAS-Thread;
parallele Agentenlast möglich. Gesamtlauf45,0495s,Peak410,637MiB.
Beide vollständigen Auswahlphasen4,740/4,804s; Feldindex5,874ms,
Aufbau samt Audit17,8084s. Doppelte Auswahl dient dem Vergleich, nicht dem Runtimeplan.

Isolierte Teilmessung nur Vereinigung/Dilatation derselben256 echten Bordformen
nach Dekodierung/Footprintvorbereitung; ohne Clip/Audit/Körper/Packen/Rendern.
Sieben normale Wiederholungen, vorige Ausgabe jeweils freigegeben. Je Methode
separater Memray1.20.0-Lauf; erwartete Maske in beiden Workern gleich gehalten.

| Teilkosten | Gemeinsamer Buffer | Einzelne Komponenten |
|---|---:|---:|
| Normaler Median | 135,288ms | 181,860ms |
| Prozesspeak normal inkl. Eingaben | 117.288.960B | 116.498.432B |
| Allokationsereignisse | 169.350 | 230.983 |
| Kumulativ angeforderte Bytes | 60.833.702B | 118.740.140B |
| Getrackter Peak Heap | 5.490.184B | 6.316.853B |
| Instrumentierte Zeit | 403,237ms | 566,436ms |

Beide18.201 unbeschnittene Ausgabekoordinaten/293.696WKB-Bytes. Ereignisse enthalten
realloc, kumulative Bytes sind kein Live-Speicher. Vollständige interne Kopierbytes
ungemessen; explizite neue Darstellungskopien XYZ3.195.648B/Ecknormalen1.597.824B.
Keine Übertragung des Teilfaktors auf Gesamtaufbau, Frame- oder Gerätebudgets.

## Eingaben und Reproduktion

Unveränderte ursprüngliche Wiener OpenFreeMap-/Mapterhornquellen aus
[#364](curb_comparison.md#eingaben-und-frische-belege),Ost/Nord/Hoch inMetern,
Ursprung48,21°N/16,37°E,Snapshot20260927_080001_pt; kein Quellrefresh.
Zusätzlich tatsächlicher Bordstand
`/tmp/outshine-transport-curbs-364-result-4e33f6f11`.

| Bindung | SHA256 |
|---|---|
| Ursprünglicher Bordbericht | `b7b26c17052273a6ace843880d4654261b46c1dacd237527b37869b10100f810` |
| Tatsächliche Borde | `4db10d8ede0e9ee0dcd7df19212841c94fd570de5ef3fea8b91f80fd32788431` |
| Vorher-Szene | `3cbb4f6ac7e0f555e09204d1e9c76fbff35ba8497519e3c59e012e215c8a21c9` |
| Neuer Vergleichsbericht | `2b575752914fcadde3e1dd0a5b127021edc47b501318c873ad318f0ce3274c23` |
| Tatsächliche Gehwegkörper | `5806b20c23d9d54b088ad57593a02e67ba36207b699e324a0f13120e346bb830` |
| Nachher-Szene | `3e272f12fe5ca9f9e6bd8026761e7320280c678e6838a0f4dde83cb372d15b31` |

Produkte `/tmp/outshine-transport-sidewalks-372-result-a2f0d335e`:
vollständiger Kandidaten-/Quell-/Auslassungsbericht,sidewalks.npz,verification.json,
after-body.npz und gebundene Szenen-/Renderreceipts. Kosten:
`/tmp/outshine-transport-sidewalks-372-cost-a2f0d335e`,vierJSONs/zweiMemraytraces.
Terminale Phasenlogs `/tmp/outshine-transport-sidewalks-372-*-a2f0d335e.log`.
Alle benötigten Originaleingaben und der aktuelle Vorhervergleich bleiben erhalten.
[CLI-Reproduktion](README.md#breite-gehwegkörper-hinter-den-borden-372) benötigt
explizite frische Temp-Ausgaben und einen sauberen eingefrorenen Commit.

## Tatsächlich geöffnete Bilder

Frisches Vorher/Nachher1280×720 und lokale1920×1080-Nachherprüfung vollständig
geöffnet. Dieselbe regionale Kamera wie #364 mit1100m Bildspanne, gleicher
Blickrichtung/Zielpunkt und gleichem Licht; keine Nahaufnahme, Überhöhung oder
verbreiterten Darstellungsprofile. Blender5.2.2EEVEE/Mesa llvmpipeLLVM19.1.7,
16Samples, AgX und vorhandenes Tageslichtprofil unverändert. Beton liest den
gepinnten MaterialJSON-Albedowert; lokaler Inspektionsshader weiterhin
roughness0,86/metallic0, keine vollständige native Materialübernahme behauptet.

Gehwege bilden jetzt lesbarere breite Ränder etwa am Straßenzug unten links und
den Nebenstraßen rechts oberhalb der Brücke. In720p bleibt der Gewinn moderat;
das Gesamtbild ist weiter blass/schematisch. Kein neuer großer Riss oder überdeckte
Querung im geöffneten Paar erkennbar; unvollständige48 Körper bleiben ausdrücklich
eine örtliche geometrische Lücke. Kein fertiges Stadt-/Fußwegenetz behauptet.
Pixels.py:13.191/921.600Pixel verändert(1,4313%),11.079 um mehr als1/255,
max45/255,mittlereRGB-Kanalabweichung0,175779/255. Das misst Unterschied, keine Qualität.

SHA256 before.png `3f7885911ea7f8ff4e230768917c275e6850e1c175e1fc779e64312276f62b8d`,
after.png `696d195f2c48f1377d516308970b8f897ee3d55e04101ff96a0ea30e509c5bb6`.
720p-Render21,551→21,844s/Peak2.363.284→2.390.508KiB;
lokale1080p-Prüfung32,977s/2.508.100KiB. Kompletter Darstellungspayload
63.029.520→66.766.608B. Offline-Inspektionskosten, kein Framebudgetbeleg.
Alle PNGs und Vorhervergleich bleiben lokal; nur master veröffentlicht integrierte Places.
