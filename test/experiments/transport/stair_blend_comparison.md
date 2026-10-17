# Kontinuierliche Böschungen am Wiener Treppenverbund (#434)

Der neue Versuch plant die umliegenden Bodenhöhen gemeinsam mit den ganzen
Brücken-Unterseiten. Die 18 Quellenkörper und sämtliche Verkehrsprofile bleiben
unverändert. Die harten Inseln der [Terrain-Gegenprobe](stair_terrain_comparison.md)
werden durch zusammenhängende Böschungen und örtliche konstruktive Stützflächen
ersetzt. Ganze Double-Kontakte und ein aus der tatsächlichen Float32-Rundung
hergeleitetes Höhenband werden getrennt geprüft. Übrige Quellen, fünf Außenprofile,
Fahr-/Gehhüllen, Statik und native Übernahme bleiben offen; #434 bleibt aktiv.

## Verfahren und räumlicher Vertrag

Eingang bleibt der [Außenversuch](stair_outer_comparison.md) mit ursprünglichen
OSM-/DEM-Hashes, Frame und Quell-IDs. Der neue Plan entsteht aus diesen versionierten
Eingängen; kein früherer privater Terrain-Ausgabebaum ist notwendig.
Ein vorläufiger Höhenfit ohne Brückenfreigabe bestimmt die zu groben Bodenfacetten.
Nur deren innere Boden-/Bodenkanten und Facetten erhalten freie Mittelpunkte.
Ganze ursprüngliche Boden-Kontaktfacetten, deren Endpunkte und ihre Höhen bleiben
fest. Danach werden beide Zielfunktionen auf derselben eingefrorenen Konnektivität
und mit denselben Qualitätsbedingungen geprüft.

Desbrun et al., SIGGRAPH 1999, [Originalpaper](https://people.eecs.berkeley.edu/~jrs/meshpapers/DesbrunMeyerSchroderBarr.pdf),
[lokales PDF](../../../doc/references/transport/siggraph/1999-implicit-fairing.pdf),
S. 2 §2.1/Gl. 2 und S. 3 §2.7 wurden optisch geprüft: Membranenergie sowie harte/weiche
Kontakte. Galin2010 Fig.20 liefert weiterhin die Gelände-/Profiltrennung.
Hier wird ein eigener skalarer Reparaturfit auf festen XY-Dreiecken verwendet:
`delta = h - DEM`, Energie `deltaᵀ M delta + lambda deltaᵀ K delta` mit
flächengewichteter Masse und `K = Gᵀ area G`.
Das ist weder der vollständige Krümmungsfluss noch eine volumenerhaltende Methode.

Erdböschungen haben weiterhin ganze Gradienten ≤2/3. Nicht begehbare konstruktive
Stützflächen sind nur vollständig innerhalb 5 m der gewählten Bodenwege erlaubt.
Ihre zusätzliche Gradientenzulage wird flächengewichtet bestraft; sie ändern kein
Verkehrsprofil. Begrenzte konstruktive Varianten 2 und 10 werden getrennt geprüft.
Das sind neue Stützflächen, keine steileren als Erde ausgegebenen Böschungen.
Jede tatsächliche Klasse muss sowohl Double als auch Float32 bestehen.

Jeder positive Boden-/Brücken-Dreiecksüberlapp liefert Höhenbedingungen an allen
Schnittpolygonvertices. Der Boden bleibt unter der ganzen wirklichen Unterseite;
ein 10 cm Abstand läuft innerhalb 1 m zum tatsächlichen Bodenkontakt auf 0 aus.
Direkte unabhängige ganze Dreieckstests prüfen danach beide gespeicherten Exporte.
Dieser Bodenabstand ist keine Geh-/Fahrhülle; die bestehenden sechs Verkehrs-
Unterführungen behalten ihre bisherigen Bedingungen und Körper.

## Ganzes Kontaktband im tatsächlichen Export

Der Terrain-Cap ist eine einzige kontinuierliche Höhenfunktion auf einem konvexen
Rechteck. Doppelte XY mit verschiedenen Höhen, innere Sprünge, Löcher, umgedrehte
Facetten und offene/mehrdeutige Körper werden abgelehnt. Jede ursprüngliche
Bodenfacette besitzt vollständige metrische Abdeckung durch echte Kindfacetten;
deren ganzen affinen Elternhöhen werden geprüft.

Für den tatsächlichen Float32-Export werden die größten XY-/Z-Vertexbewegungen
von Terrain und Originalboden gemessen. Die maximale Double-Terrainnorm ist eine
globale Lipschitzkonstante L. Ein baryzentrischer Punkt bewegt sich höchstens wie
sein am stärksten gerundeter Eckpunkt. Deshalb gilt über dem ganzen ursprünglichen
Bodenprodukt das Höhenband
`E = e_Double + delta_z_T + delta_z_G + L * (delta_xy_T + delta_xy_G)`.
`e_Double` enthält die gemessene Elternhöhenabweichung und die bestehende metrische
Schnittauflösung. Die tatsächliche Float32-Fläche muss das komplette Bodenprodukt
abdecken; ganze positive Dreiecksüberlappungen müssen in beiden Richtungen innerhalb E
liegen. Keine bloße Stichprobe oder frei gewählte zusätzliche Kontakt-Toleranz.

Die frühere geschlossene Stützwandfläche aus #475 erfüllt die C0-Voraussetzung
ausdrücklich nicht und wird schon davor abgelehnt. Deren meterhoher Randabstand
wird durch diese Herleitung nicht freigegeben. Ein exaktes Nullvolumen überlappender
Float32-Solids wird weiterhin nicht behauptet; das endliche Kontaktband wird belegt.

## Gemessener lokaler Stand

Geometrieerzeugung auf `69d319ee3`, unabhängiger Leser, elf Kontrollen und getrennte
Kostenmessungen auf `a657aee96` einschließlich des damaligen Masters `b5f6e617d`.
Alle Prozesse endeten mit Status 0; beim Datenfit bedeutet dies ausdrücklich
eine abgeschlossene **Ablehnung**, keine erfolgreiche Geometrieabnahme.
Das ursprüngliche Verkehrsprodukt bleibt bytegleich
`104f44633cfebac880108761cbb571366cf9fe2ea3b60ce07ebe3a566d428ef2`.
Die letzte Membranhöhenfolge und sämtliche sieben normalen Wiederholungen sind
bytegleich zur gespeicherten Referenz. Keine SDK-Aufrufe gehören zu diesem Versuch.

| Gleiche Eingabe und Exportbedingungen | Datenfit λ=0 | Membranfit λ=16 m² |
|---|---:|---:|
| Solverstatus | Solved | Solved |
| Größter konstruktiver Gradient Double / Float32 | 9,997999 / **10,059083** | 8,720056 / 8,773996 |
| Tatsächlicher Export zugelassen | nein, Grenze 10 verletzt | ja, örtliches Terrain |
| Normale Zeit | 2791,45 ms, abgelehnter Versuch | Median 2334,74 ms, 7 Fits |
| Retained P/A-Speicher | 1 058 940 B | 1 300 068 B |
| Prozess-Peak einschließlich Vorbereitung | 131 309 568 B | 131 444 736 B |
| Separat profilierte Allokationsereignisse | 3 157 538 | 3 220 556 |
| Separat kumulierte Allokationsanforderungen | 327 160 117 B | 337 431 487 B |
| Separater getrackter Heap-Peak | 33 960 489 B | 35 161 860 B |

Gemessen auf Linux/x86_64, Intel i5-1135G7, zwei sichtbare Kerne/vier logische CPUs;
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0/GEOS 3.14.1,
Clarabel 0.11.1, ein Solver-/BLAS-Thread. Keine eigenen Render-/Testprozesse während
der nacheinander ausgeführten normalen Fits; gemeinsame Hostlast nicht exklusiv.
Die Phase umfasst Matrix-/Ganzflächenbedingungen und einen begrenzten Solve nach
Import, Dekodierung, festem Netz und zwei Aufwärmkandidaten. Prüfung danach außerhalb
der Zeit. Keine gültige Ausgabemediane oder Beschleunigungsrate für den Datenfit.
Memray 1.20.0 mit Python-/nativen Traces lief separat: 5487,53 ms zugelassener Fit,
6154,48 ms abgelehnter Versuch; Prozess-Peaks 140 210 176 / 140 087 296 B.
Allokationsanforderungen sind keine eindeutigen/live Bytes. Die Höhenfolge umfasst
45 016 B; bekannte Konvertierungs-/Datumskopien 90 032 B, zusätzliche Prüf-XYZ-Puffer
337 620 B außerhalb der Phase. Interne Solverkopien bleiben ungemessen.
Kein Echtzeit-, A18-, RTX-, nativer oder Gesamtgeneratornachweis.

Der Vorplan erhält 71 freie Facettenmittelpunkte und 101 freie Kantenmittelpunkte:
5627 Höhenvertices, 11006 Terrainfacetten; 6377 Solvervariablen, 41954 Zeilen,
78366 A-Nichtnullen. Die begrenzte Stützvariante 2 scheitert; Variante 10 besteht.
43 konstruktive Facetten belegen 26,6953 m² projiziert beziehungsweise 35,3945 m²
Oberfläche. Erdgradienten maximal 0,664668 Double / 0,666086 Float32.
Alle 1990 ursprünglichen Bodenkontaktfacetten sind ganz abgedeckt; gemessener
Elternhöhenfehler höchstens 5,69e−14 m. Das hergeleitete Float32-Kontaktband beträgt
0,00057672435 m; tatsächliche ganze Extremabstände liegen bei −0,00001468888 m
und +0,00001318151 m. Ganze Brückenunterseiten: Mindestabstand 0 m in beiden
Darstellungen, einschließlich echter Widerlagerkontakte. Innere Höhensprünge: 0.
Geschlossener Terrainsolid: 11254 Vertices, 22504 Facetten, keine Nullflächen,
alle Vertexlinks zyklisch; die untere Abschlussplatte ist ausdrücklich künstlich.

Analytischer Auf-/Abtrag auf demselben verfeinerten affinen DEM: 983,9701 m³ Auftrag,
7756,1283 m³ Abtrag, vorzeichenbehaftet −6772,1582 m³; Bilanzrest <1e−12 m³.
1621 Facetten werden an ihrem linearen Nullschnitt getrennt integriert.
Das sind weder vermessene Erdmengen noch ein Integral des ursprünglichen bilinearen
DEM und auch keine zusätzlichen ganzen Cut-/Fill-Solids. Maximale Höhenänderung 4,1880 m.

Die vier frischen 1280×720 PNGs wurden vollständig dekodiert, an Szenen-/Mesh-/
Importhashes gebunden und tatsächlich geöffnet. Gleiche Kameras, Farben und Licht
je Vorher/Nachher: ganze Übersicht sowie ein 110 m Ausschnitt am Treppenverbund.
Die harten Inselränder verschwinden; Brücken bleiben frei, örtliche Stützflächen
werden separat als konstruktiv dargestellt. Die Übersicht ändert nur 2,2062 %,
der Ausschnitt 36,1829 % der Pixel; dies belegt Änderung, keine Qualitätsskala.
EEVEE 5.2.2 LTS auf llvmpipe/CPU, 16 Samples: Übersicht 14,99/15,62 s,
Ausschnitt 29,19/29,08 s. Keine Bilder oder Ersatzgalerie unter `doc/`.

Lokale Originalprodukte: System-Temp `outshine-transport-terrain-blend-69d319ee3`,
Kosten `outshine-transport-terrain-blend-cost-a657aee96`. Plan-SHA256
`44ec5e33d3c6f6488e0efa5334cfae99cddeb0a7ff40738feba9a579cedecd17`,
Membrankörper `70966893d2caa4a61c5b861108e4d3b9805162836087b5728f1f3114069651f7`.
`comparison.json`, `verification.json`, `render-completeness.json` und die getrennten
Kostenbelege erhalten Generation-/Leser-Commit und tatsächliche Artefakthashes.
Der Leser besteht auch mit ausdrücklich gesperrtem Solver, CDT und Körperbauer.
Vorige #475-Gegenbeispiele bleiben abgelehnt und erhalten; deren Gates ändern sich nicht.

## Reproduktion und verbleibende Abnahme

[Aufrufe und Pakete](README.md). Die lokalen Kontrollen enthalten echte Quellen,
Innenflächenkonflikte ohne Eckpunkttreffer, veränderte Materialrollen, den früheren
Sprung-Gegenfall und absichtlich unterschiedliche Float32-Triangulationen.
Der reine Datenfit und der Membranfit unterliegen denselben Exportbedingungen;
ein erfolgreicher Solverstatus darf einen ungültigen Export nicht freigeben.
Beide Kandidaten, auch ein abgelehnter Rohfit, werden mit ihren Ursachen erhalten.

Kein SDK, Manifold oder nativer Produktcode gehört zum neuen Verfahren.
Die vorhandenen Bibliotheksoperatoren bleiben unverändert. Neue räumliche/native
APIs oder eine globale Quellfreigabe werden nicht aus diesem örtlichen Versuch
abgeleitet. Materialparameter anderer Besitzer bleiben unberührt; die technische
Python-Darstellung nutzt vorhandene Erd-/Betonfarben.
