# Gespeicherter 1ULP-Wasser-Gegenfall (#935 / #760)

Der unveränderte [native Eingabesatz](fixtures/native-geos-3141/README.md) aus
#933 scheitert hier mit `InsufficientProgress` nach einer Iteration. Face257 hat
Vertices714/713/712, positive exakte Binary64-Fläche2,359409320975948e-13m² und
33,49m lange Kanten. Die ersten beiden Punkte unterscheiden sich horizontal
um1ULP=1,4210854715202004e-14m. Dadurch erreicht |A|5,827543902314253e13.
Das misst einen geometrischen Konditionierungsfall. Physische Unzulässigkeit
der Originalinstanz ist damit weder bewiesen noch widerlegt; ebenso wenig ein
allgemeiner Solverfehler.

## Geometriefreie Kontrolle

Positive Zeilenskalierung mit `s=1/max(1,max(abs(Arow)))` erhält mathematisch
`A*x<=b`. Clarabel meldet damit `Solved` nach28 Iterationen. Im Originalproblem
bleibt jedoch eine maximale Verletzung2900,9375; die unabhängig aus den tatsächlich
rekonstruierten Float64-Dreiecken geprüfte physische Neigung verletzt ihre Grenze
um3220,7919. Der skalierte Solverresidual ist9,95e-14. Die Dualvariablen werden
für die Originalprüfung korrekt mit s zurücktransformiert. Ein guter skalierter
Residual oder Solverstatus ersetzt weder Originalresiduen noch Geometrieprüfung.
Dieser Pfad wird verworfen; keine Toleranz wurde gelockert.

## Explizite kausale Gegenprobe

`water_sliver_edge.contract_edge` ist ausschließlich eine vom Aufrufer vorgeschlagene
Ein-Kanten-Intervention:714→713, deklarierte maximale Verschiebung1e-12m. Kein
automatisches Welding, Flächenschwellwert oder neues Bibliotheksverhalten. Drei
überlebende Faces ändern eine Ecke; ausschließlich Face257 wird durch identische
Indices hinfällig. Es werden1493 Vertices/1494 Faces/4472 Richtungszeilen neu gelöst,
nicht alte P/A/q/b nachträglich passend gekürzt. Alle überlebenden ursprünglichen
Vertex-/Face-/Richtungs-/Primärzeilenindices werden ausdrücklich mit ausgegeben.

Der bisherige Fit besteht nach26 Iterationen mit unveränderten physikalischen
Whole-face-/Normal-/Kontaktgrenzen1e-9 und finiten Primal-/Dualgrenzen1e-7.
Whole-face-Verletzung0, maximale Stationarität1,92e-12, duale Lücke6,60e-10.
Der bestehende unabhängige Kreuzprodukt-/PROJ-Reader prüft zusätzlich67.230 Punkte
und201.240 Richtungsgrade ohne Verletzung. Diese Stichproben ersetzen die separate
Whole-prism-Hülle nicht. Der Fit verschiebt weiche ENU-Ziele maximal0,77573m;
das sind erzeugte Priorziele, keine beobachteten Wasserstandkontakte.

Die Änderung ist **nicht topologisch neutral**:

| Größe | Gemessene Änderung |
|---|---|
| Größte XY-Verschiebung | 1,4211e-14m; alle übrigen behalten ihre Originalkoordinaten |
| Entferntes Originalface | 257; Fläche2,3594e-13m²; drei Richtungszeilen protokolliert |
| Betroffene Originalfaces | 257,259,653,895 |
| Konservative symmetrische Flächenabweichung | ≤6,73301e-13m² |
| GEOS-Flächendifferenz | 3,45274e-14m² |
| Innenlöcher der gespeicherten Triangulierung | 2→1: zusätzliches3,45274e-14m²-Loch verschwindet |
| Belegte Quellinsel | 352,573870915m²; ihr bestehender Ring bleibt exakt identisch |
| Außenring | Hausdorff-Abweichung0 |
| Erfasste Herkunftsmitgliedschaften | 1619→1616; entferntes Face hatte0/1/3; alle13 Quellen bleiben |

Die Flächenschranke summiert die beiden beim Verschieben jeder Ecke überstrichenen
Kantendreiecke. Ihre Flächen werden aus den exakten rationalen Werten der Binary64-
Koordinaten berechnet und die ausgegebene obere Grenze aufgerundet. Ein bloßes
`Verschiebung*Gegenkantenlänge/2` wäre bei tangentialer Eckbewegung falsch.
GEOS-Flächen-/Overlayzahlen allein sind bei dieser Größe kein verlässliches Orakel.

Die übernommenen CSR-Einträge sind ursprüngliche Herkunft, kein neuer Beweis
positiver Quellenüberlappung. Erneute Quellprojektion auf diesem Host liefert bereits
für die unveränderten Facetten1625 statt der gespeicherten1619 Einträge; nach der
Intervention1622, mit identischen Mitgliedschaften sämtlicher überlebender Faces.
Die ursprünglichen Quellringe enthalten genau eine Insel. Die zusätzlichen winzigen
Überlappungen sind hostabhängig; daraus keine neuen Quellen oder verlorenen Inseln
ableiten und den originalen Mac-CSR nicht durch Linux-Zahlen ersetzen.

## Entscheidung und Grenzen

Der einzelne geometrische Eingriff entfernt die beobachtete Blockade; pauschale
Zeilenskalierung liefert dagegen einen falsch positiven Solverstatus. Für diese
Gegeneingabe ist eine ausdrücklich gemessene numerische Geometriekorrektur plausibel.
Eine produktive Regel muss betroffene Quellringe, Innenlöcher und Anschlüsse am
kanonischen Erzeugungsort gemeinsam beurteilen. Ein kleines Flächenmaß allein
berechtigt weder zur Inselentfernung noch zur automatischen Vertexverschmelzung.
Geschützte Kontaktvertices werden hier vor der Intervention abgewiesen. Der echte
Fall hat keine harten Punktkontakte; beliebige Punktkontakte und Nachbarregionen
sind damit nicht abgenommen. Keine native Portierung dieses Test-Indexpaares.

SIGGRAPH2016 [Mesh Arrangements for Solid Geometry](https://www.cs.columbia.edu/cg/mesh-arrangements/),
lokale Original-PDF S10/§6.1 optisch geprüft: Rundung exakter Geometrie kann ihre
Topologie ändern. Die dortige 3D-Heuristik wird nicht übernommen.
Shapelys [set_precision](https://shapely.readthedocs.io/en/latest/reference/shapely.set_precision.html)
kann schmale Teile löschen; [remove_repeated_points](https://shapely.readthedocs.io/en/latest/reference/shapely.remove_repeated_points.html)
mit positiver Toleranz kann ungültige Geometrie erzeugen. Daher keine ungeprüfte
globale Quantisierung/Snap-Routine als Reparatur dieses Fits.

## Reproduktion und Kosten

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water \
  .venv/bin/python -B -W error test/experiments/water/compare_water_sliver.py \
  --output /tmp/outshine-research-water-sliver-<commit>/checked
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water \
  .venv/bin/python -B -W error -m unittest discover \
  -s test/experiments/water -p test_water_sliver.py
```

Sieben Kontrollen: exakte positive1ULP-Fläche, vollständige Index-/Zeilenherkunft,
expliziter Lochverlust bei identischer Quellinsel, geschützte Kontaktvertices,
Verschiebungs-/Kantenprüfung, erhaltene andere Kleinstfläche und tangentiale
Bewegung als Gegenbeispiel zur falschen Flächenschranke. Originaldateihashes prüfen.
Intervention scannt4485 Faceindices, liefert352.000B bekannte Arrayausgaben und
benötigte hier rund70ms uninstrumentiert; separater Tracemalloc-Replay671.505B Peak.
Die exakten rationalen Diagnoseprüfungen sind dabei enthalten. Solver A13424×1493,
34300NNZ/417576B, P7467NNZ/95580B; Vektoren119336B. Quellenreader misst seine
angeforderten1678/1674 Schnitte separat. Temporäre/native GEOS-/Solverallokationen,
voller Prozesspeak, native CPU/GPU und Geräte-/Place-Bildabnahme sind unbewiesen.
Keine neue Abhängigkeit, Pins oder Bildveröffentlichung; #760 bleibt offen.
