# Nördlicher Radweg und darstellungssichere Kontaktflächen (#434)

Dieselbe begrenzte Versuchsfolge verarbeitet jetzt eine zweite feste
[Planungseingabe](fixtures/stair-cycle-north-vienna.json). Sie ergänzt den ganzen
ursprünglichen Radweg `39881120:23` zum [19-Quellenstand](stair_cycle_comparison.md).
Quellen-/DEM-Auswahl und geschützte Profile sind Instanzdaten; eine Straße erzeugt
keinen neuen Katalogeintrag. Kein gemeinsames Rezeptformat oder Universalsolver
wird daraus abgeleitet; den Indexvergleich und die gemeinsame Entscheidung besitzt #483.

Die Achsen teilen zwei ursprüngliche MVT-Vertices. Der vorhandene Fachplan nutzt
2158/1789 als echten Endkontakt; die zusätzliche gemeinsame Bandfläche bleibt eine
ausdrückliche Planentscheidung. Der interne gemeinsame Vertex 2677/2591 wird nicht
pauschal zu einem gemessenen Straßenknoten erklärt. Bisherige Achsen, Priors und
vollständige Quellattribute bleiben erhalten; gemeinsame Höhen und Terrain sind neu.
Das nördliche alte Außenprofil ist ganz gebunden; sechs wirkliche Außenprofile
bleiben offen (vier bisherige und zwei neue). Keine globale/native Freigabe.

## Tatsächliche Gegenfälle und Verfahren

Der frühere Zellen-/Kontaktkantenschnitt erzeugt am neuen Eingang zwei reale
Float32-Probleme. Drei schmale Bodenfacetten bestehen Double, überschreiten nach
Rundung aber die Erdgradientengrenze. Darüber hinaus dreht eine lange zusätzliche
Kontakt-Unterteilung ihre projizierte Orientierung um. Solverstatus oder ein
geschlossener Double-Körper genügen deshalb nicht. Beide Gegenfälle bleiben
abgelehnt, keine Erde wird als konstruktive Fläche außerhalb ihres Bereichs umetikettiert.

Die neue Zerlegung vereinigt sämtliche Zellen am Wegegrundriss zu einem Bodenband
und trianguliert dessen Differenz zum ursprünglichen Wegegrundriss. Ferne Zellen
bleiben regelmäßig. Ganze Quellkontaktfacetten und ihre tatsächlichen Kanten bleiben
unverändert; nur die freien Bodenflächen werden konform zerlegt/verfeinert.
Die gespeicherten tatsächlichen Double-/Float32-Höhenfelder und sämtliche alten
Kontakt-/Neigungs-/Unterführungsgrenzen bleiben verpflichtende Abnahme.

Die eigene Rundungsreserve verwendet tatsächliche Float32-XY und einen ausdrücklich
beschränkten Höhenbereich: Extremwerte von DEM/Pins plus/minus 8 m. Für jede Facette
werden die acht extremen Vorzeichenkombinationen des halben größten Float32-ULP
durch ihren affinen Gradientenoperator Gq abgebildet. Ihr größter Normwert ist E.
Für den Double-Operator Gd gilt über die affine Ebene `Gq*h = T*(Gd*h)` mit
`T = Gq*XY_double`; die konstante Höhe fällt heraus. Die Spektralnorm von T liefert
zusammen mit der vorhandenen Double-Reserve einen Ausschlusstest für zusätzliche
Kegelbedingungen. Beide Enden des erlaubten konstruktiven Zuschlags werden geprüft.
Die selektive Variante fügt nur nicht bereits implizierte Bedingungen hinzu;
die vollständige Variante prüft alle. Gleiche feste Vernetzung, Höhenintervalle,
Gradientengrenzen, Ziele und Zeit-/Speicherlimits; tatsächlicher Export bleibt
entscheidend. Keine Konvergenz oder globale Optimalität aus MaxIterations ableiten.

SIGGRAPH-Recherche zuerst: Deering 1995, [Autor-PDF](https://www.michaelfrankdeering.com/Projects/HardWare/CompressedGeometry/SigGeometryCompression.pdf),
[lokales PDF](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
S. 3 §4 optisch geprüft: lokale Koordinaten und bewusst begrenzte Positionspräzision.
Die Autorenfassung enthält keine Ergebnisfotos; daraus wird kein Bildvergleich
übernommen. Die hier verwendete Gradienten-/Würfelabschätzung ist eigene Arbeit,
keine behauptete Formel dieses Papers. Membranenergie und Profil-/Terraintrennung
bleiben wie in den vorigen Vergleichen belegt. Kein Fremdcode oder SDK in der Runtime.

## Reproduktion und offene Abnahme

[Aufrufe](README.md). Der alte 19-Quellen-Eingang bleibt bytegleich und benutzt
weiterhin seine bisherigen Operatorvorgaben. Für neue gewöhnliche Bodenquellen
gelten harte Anzahl-/Profilgrenzen; indoor, Brücken/Tunnel und andere Ebenen werden
aus diesem gewöhnlichen Rezept ausdrücklich abgelehnt. Räumliche/native APIs
bleiben unverändert. Ein erfolgreicher vollständiger Export, unabhängiger Leser,
tatsächliche Bildprüfung und gleiche Kostenkontrollen werden im PR belegt.

Übrige Quellen/Ebenen, Stationsrezept, Fahr-/Gehhüllen, Radien, Statik, allgemeine
Selbstschnittfreiheit und native Übernahme bleiben offen; #434 bleibt aktiv.

## Tatsächlicher Lieferstand

Geprüfter Stand `b26d15fb20e873c336bb1a6b32ac2178f15fb3fd`, einschließlich
Master/Buildings `f2f0ee2b2`. 31 betroffene Kontrollen bestehen auf diesem
eingefrorenen Stand in 33,224 s. Sie enthalten den wirklichen Float32-Kantenfehler,
den Neigungs-Gegenfall, unzulässige indoor-/Ebenenquellen, harte Eingabegrenzen und
die Gleichheit von Zielfunktionsdifferenzen/Kegelresiduen bei Variablenelimination.
Ruff/Format, Funktionen ≤120 Zeilen und Diffprüfung bestanden; native Gates offen.

Die neue ganze Quelle misst 632,753076 m und behält das vorhandene Profil
2 m / Gradientengrenze 0,06. 20 Quellen, 19 gemeinsame flache Kontakte,
drei Treppen/24 Stufen, sechs tatsächliche Float32-Unterführungen ≥2,519989 m;
5226 Körpervertices/10476 Facetten und sämtliche zyklischen Vertexlinks geprüft.
Alle fünf geschützten ganzen Anschlussquerschnitte bestehen in beiden Präzisionen.
Originalinventar vollständig erhalten: 1637 ungeprüfte Linien und 71 Flächen,
darunter 51 potentielle Linienüberlappungen und acht überlappende Polygone.
Sie sind keine räumlich freigegebenen Hindernisse oder Netzteile.

Die 2587 Originalkontaktfacetten sind im gespeicherten Endkörper **als ganze
XYZ-Facetten bytewertgleich**, Double und Float32. Ganze gemessene Oberflächen-
Kontaktextrema deshalb 0/0 m; kein exaktes Null-Überlappungsvolumen des gesamten
Verkehrskörpers behauptet. Die weiterhin unabhängige generische Rundungshülle
beträgt 0,660512 mm. Ganze Brückenunterseiten bestehen einschließlich Widerlagern.
Terrain: 6597 Höhenvertices/12918 Facetten; geschlossener Außensolid
13194 Vertices/26384 Facetten, keine Nullflächen/inneren Sprünge.
Erde maximal Double/Float32 0,664666687/0,665464545; konstruktiv
9,998000051/9,998638969 bei unveränderter Grenze 10. 39 konstruktive Facetten,
15,7341 m² projiziert/20,6569 m² Oberfläche, innerhalb des bisherigen 5-m-Bereichs.
Der künstliche untere Abschluss ist keine vermessene Erdgrenze.

Beide versionierten DEM-Ausschnitte sind exakt aus den ursprünglichen Rohbytes
geprüft; 6742 echte Punkte einschließlich aller Endplanpunkte stimmen exakt mit
dem unabhängigen vollständigen Decode überein (0 m Fehler). Auf-/Abtrag relativ
zu demselben affinen DEM: 1418,5009/9699,4258 m³, Bilanzrest <2e−12 m³.
Kein bilineares Erdvolumen oder zusätzliches Cut-/Fill-Solid behauptet.

Der unabhängige Leser besteht auch mit gesperrtem Solver, CDT und Körperbauer.
Zusätzlich wurden alle sieben früheren 19-Quellen-Dateien frisch erzeugt und
**bytegleich** verglichen: Körper, Höhenplan, Straßendarstellung, Terrainplan,
Höhenfeld, Terrainsolid und kombinierte Darstellung. Der alte Eingang bleibt
SHA256 `b7e6681b8a5dc26a8508b981dca861d97af846c5355a243ec7a84d22d99c245e`.

## Konditionierung, Bild und fairer Kostenvergleich

Die erste Randbandvariante bestand die physischen Grenzen, erzeugte aber sichtbare
Zacken unter den langen Brücken. Auch sämtliche Rundungskegel beseitigten diese
Zacken bei MaxIterations nicht. Beide Bilder wurden geöffnet und als Bildlieferung
verworfen; ein erfolgreicher Prozess genügte ausdrücklich nicht.
Die Korrektur löst nach `delta = h - DEM`, setzt feste Höhen direkt ein und entfernt
deren Variablen/Gleichungen. Das ist eine äquivalente Verschiebung/Substitution
der vorgesehenen quadratischen Energie, keine weichere Physik. Desbrun 1999,
S. 3 §2.7 wurde nochmals optisch geprüft (harte Kontakte als Hintergrund, keine
Übernahme eines fremden Solvers). Die eigene Substitution wird separat algebraisch
und am tatsächlichen Export geprüft. Auf derselben Vernetzung sinken die
konstruktiven Flächen von 302,2750 auf 15,7341 m²; die Zacken verschwinden.

Die vier endgültigen 1280×720 PNGs sind frisch, vollständig dekodiert, an aktuelle
Szenen-/Mesh-/Helperhashes und exakte Importarrays gebunden und tatsächlich geöffnet.
Gleiche Kameras/Farben/Licht vor/nach: ganze Übersicht plus automatisch aus den
Quellflächen abgeleiteter 917,60-m-Ausschnitt. Pixeländerungen 18,2593/27,9097 %,
keine Qualitätsskala. EEVEE 5.2.2 LTS, llvmpipe/CPU, 16 Samples: vor/nach Übersicht
11,49/11,70 s, Ausschnitt 17,68/17,80 s. Keine Galerie/Bilder unter `doc/`.

Kosten auf Linux/x86_64, Intel i5-1135G7, zwei sichtbare Kerne/vier logische CPUs;
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0/GEOS 3.14.1,
Clarabel 0.11.1, ein Solver-/BLAS-Thread. Nacheinander, keine eigenen Render-/
Testprozesse während normaler Messungen, gemeinsamer Host nicht exklusiv.
Gleiche feste Vernetzung/Energie/Höhenintervalle/Exportgrenzen, 8 s und
100 Iterationen unverändert. Matrixaufbau und Solve nach Import/Decode und einem
Aufwärmfit; tatsächliche Feld-/Körperprüfung danach außerhalb der Zeit.

| Gleiche Qualität, je sieben normale gültige Fits | Selektiv | Alle Kegel |
|---|---:|---:|
| Zusätzliche Rundungskegel | 43 | 10331 |
| Tatsächliche Solvervariablen | 5331 | 5331 |
| Tatsächliche Solverzeilen | 57331 | 88195 |
| Tatsächliche A-Nichtnullen | 73330 | 111944 |
| Normaler Median Matrix/Solve | 2483,41 ms | 3060,79 ms |
| Normaler Prozess-Peak | 146 706 432 B | 173 457 408 B |
| Separat profilierte Zeit | 6298,42 ms | 6678,78 ms |
| Separater Prozess-Peak | 155 664 384 B | 181 530 624 B |
| Separate Allokationsereignisse | 3 318 341 | 3 629 786 |
| Separat kumulierte Anforderungen | 375 250 112 B | 427 343 834 B |
| Separater getrackter Heap-Peak | 43 245 336 B | 65 648 883 B |

2003 feste Variablen sind eliminiert; ursprüngliche plus reduzierte Sparsearrays
werden beide gezählt: selektiv 1 572 096 + 1 192 880 B, vollständig
2 104 044 + 1 656 248 B. Bekannte zusätzliche Shift-/Delta-/Linearvektoren
176 016 B, XY-Quantisierung/Kopie 158 328 B, Höhenfolge 52 776 B;
bekannte Wiederherstellungs-/Ausgabepuffer 111 448 B. Prüf-XYZ-Puffer 395 820 B
außerhalb der Zeit. Interne Kopiersummen ungemessen. Memray 1.20.0 lief separat;
Anforderungsbytes sind keine eindeutigen/live Bytes.
Selektiv meldet AlmostSolved, vollständig Solved; beide bestehen die ganzen
Exportbedingungen, Höhenunterschied maximal 0,00000073181 m. Wiederholungen sind
je Strategie bytegleich zur unabhängig geprüften Höhenfolge. Keine globale
Optimalität, Geräte-/Frame- oder Gesamtgeneratorbeschleunigung behauptet.

Ganzer frischer Aufbau bis vor Display 17,17 s, einzelner Forschungslauf.
Originalprodukte im System-Temp `outshine-transport-stair-north-b26d15fb2`,
Kosten `outshine-transport-stair-north-cost-b26d15fb2`; verworfene Bildgegenfälle
`outshine-transport-stair-north-b22f83eed` und `outshine-transport-stair-north-all-b22f83eed`.
Plan-SHA256 `68c97b946b9ad60e7f81813c7352c0efac4848fb32a7b0928854a9d215ff8af3`,
Endkörper `fe54e943075787e47e06119d3d6cb9275ab9132c04319434aaa5636d4cc1aa89`.
Feste Eingänge, geprüfter Stand, unabhängiger Leser, frische Bilder und getrennte
Kostenbelege bleiben durch vollständige Hashes gebunden. #434 bleibt aktiv.
