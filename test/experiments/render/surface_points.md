# Konservative Flächenpunkte scheitern schon an der Blattdichte (#952)

Parent #899. Der begrenzte Geometrieversuch wird vor einem Material-Fit verworfen.
Die vorhandene native Zelle besitzt wenige große Flächen statt dichter Oberflächen-
samples. Der feste Subpixel-Support ist am Ende der Punkt-Hierarchie überwiegend
nicht erreichbar. Weglassen erzeugt Lücken, der Blattstopp verbreitert die Silhouette.
Keine Aussage gegen sämtliche Punkt-/Voxel-/Hybridverfahren und kein weiterer Sweep.

## Verfahren und tatsächliche Grenzen

SIGGRAPH zuerst: [Rusinkiewicz/Levoy, QSplat 2000](https://graphics.stanford.edu/papers/qsplat/),
Autoren-PDF S.2/S.5/S.7 optisch geprüft: incident-Dreieckskugeln, Hierarchie und
Silhouetten-/Dichtegrenzen. Die Referenz übernimmt diese geometrische Idee, nicht
QSplats Quantisierung, Viererstruktur, Normalenkegel oder Leistungsversprechen.
SciPy cKDTree erzeugt einen binären Raumindex; kein kopierter Fremdcode.

Für jedes Dreieck wird der Radius seiner kleinsten umschließenden Kugel bestimmt:
bei stumpfem/rechtem Winkel halbe längste Kante, sonst `abc/(4*Fläche)`.
Jeder Eingabevertex erhält das Maximum seiner incidenten Dreiecksradien.
Das deckt das Dreieck durch die **Vereinigung** der Vertexkugeln: liegt jeder
Vertex v_i in einer Kugel(c,R), dann gilt für p=Σw_i v_i
`Σw_i |p-v_i|² = Σw_i |v_i-c|² - |p-c|² <= R²`.
Mindestens eine Vertexkugel enthält p. Nullflächen tragen keinen Oberflächensupport;
ihre Originaladressen bleiben erhalten. Hier gibt es keine solchen Facetten.

Geometrisch gleiche Vertices teilen ein Blatt, behalten aber sämtliche ursprünglichen
Adressen im cKDTree-Order/Teilbaumspan. Keine Normalen-/UV-/Materialgleichheit daraus
ableiten. Eltern schließen beide Kinderkugeln ein. Double-Geometrie mit zusätzlicher
numerischem Zuschlag zum Radius wird geprüft; keine globale Weld-/Oberflächentoleranz.

Für Kamerazentrum p=(x,y,z), Kugelradius r, z>r+near und Brennweite f enthält ein
Kreis um project(p) mit `rho=f*r*sqrt(1+(x²+y²)/z²)/(z-r)` die ganze Projektion.
Die tatsächliche Kugelprojektion hat mindestens Durchmesser `2*f*r/sqrt(z²-r²)`
(kleine Ellipsenachse). Unabhängige Tangentialstrahlen prüfen diese Untergrenze.
Ein **Supportdurchmesser von 1px** steuert den Schnitt, kein bewiesener 1px-Bildfehler.
Zu große Endblätter bleiben ausdrücklich ungelöster Bedarf. Kameranahe unprojizierbare
Blätter und Arbeitsbudgetende werden ebenfalls nicht stillschweigend zugelassen.
Frustum-cull und Teilbaumspannen müssen alle Originaladressen genau einmal abdecken.
Diese algebraischen Argumente sind keine rigorosen Floating-point-/GPU-Rasterbeweise.

## Original und Ergebnis

Unverändertes `test/outshine/data/render/building-full-view/manifest.json`, native
Originalbytes/Quellen/Teile; kein neuer Export oder kamerabasiertes Training.
576 Quellen, 14169 Facetten, 28672 Vertices; 8321 geometrische Blätter/16641 Knoten.
20351 zusätzliche Vertexadressen an gleichen Positionen bleiben erhalten.
Alle 120 Originalposen und 8 vorhandenen Gegenposen durchlaufen die Auswahl;
36 Schnitte enthalten Punkte. Alle 128 Partitionen erhalten sämtliche 28672 Adressen.

Drei ursprüngliche sichtbare Posen behalten 1280×720, originale Brennweite und
gleiche 4×4 Subpixelstrahlen. Ausgangsgeometrie rund 5,6km entfernt, etwa 42×5px groß.
Die gemeinsame ROI enthält auch jeden Kreisüberhang; kein Fehler wird weggecroppt.
Die Kreismasken sind reine Deckungsdiagnosen, keine Tiefe/Normalen/PBR/Weltbilder.
Der Kugelquery cullt nur den Frustum, keine Rückseiten. Die Kreismasken sind damit
eine konservative Diagnose dieses Rezepts, kein vollständiger QSplat-Renderer.

| Pose | Ausgewählte Punkte | Endblätter >1px, konservativ / sicher zu groß | Originalfläche | Nur zulässige Punkte: Verlust/Zusatz | Blattstopp: Verlust/Zusatz |
|---|---:|---:|---:|---:|---:|
| static-020 | 8198 | 5668/5584 | 73,375px | 21,125/20,6875px | 0/120px |
| moving-020 | 8198 | 5669/5586 | 73,1875px | 21/20,5px | 0/119,875px |
| moving-021 | 8229 | 5935/5716 | 75,8125px | 23/22,5625px | 0/124,875px |

„Sicher zu groß“ betrifft die Kugelprojektion dieses Blattrezepts; keine optimale
Untergrenze für beliebige andere Surfels. Die erste Pose kollabiert nur 117 innere
Knoten. Der Blattstopp überdeckt die Quellsilhouette, malt aber 2,64mal deren Fläche.
Fast gleiche Gesamtfläche beim Weglassen beweist ebenfalls keine korrekte Verteilung.

Eine einfache konservative Fallbackpolitik fordert alle Originalfacetten mit einem
unsupported Vertex an: 11851/11853/12289 Facetten aus 499/499/514 Quellen. Das ist
eine Offline-Gegenkontrolle unter dieser Politik, kein metadata-only Runtime-Selektor
oder optimaler Hybrid-Untergrenzennachweis. Darunter liegen 5008/5008/5206
tatsächlich nach vorne gerichtete Facetten, ebenfalls aus 499/499/514 Quellen.
Originale werden nicht gelöscht.

## Kosten und Abnahme

Die tatsächliche Double-/Index-Hierarchie hält 913456B Geometriearrays; der native
Originalkörper meldet 1028316B, die normalisierte UInt32-Diagnose 1886340B ohne die
zusätzliche abgeleitete Normalenkopie. Die Punktdatei enthält **keine** vollständigen
Material-/UV-/Normalverteilungen, Tiefen-/Schatten-/Simulationsprodukte. Keine
Asset-Speicherersparnis aus diesem ungleichen Umfang oder Papier-Bytes ableiten.
Alle 128 Diagnose-Schnitte gemeinsam halten 10699716B; das ist keine Runtime-Residenz.

Original-Float32→Double und Triangel-Indexierung kopieren mindestens 688128+1020168B
Koordinatenpayload. Finale Hierarchie: sechs eigene Arrays/913456B; weitere cKDTree-
und NumPy-Allokationen/Kopien nicht vollständig erfasst. Linux/i5-1135G7 unter
interaktiver Last: Aufbau 0,531s, getracter Wiederholungs-Peak 7074573B/122 verbleibende
Blöcke. Gesamte 128 Queries einschließlich separater Adressprüfung 22,42s. Keine
nativen/GPU-/App-Budgetzeiten oder vollständigen nativen Allocator-Peaks daraus.

13 gezielte Kontrollen prüfen Dreiecksdeckung, Kinderkugeln, gleiche Positionen mit
getrennten Adressen, Perspektive/Tangentialstrahlen, Near/Frustum, Budgetende,
gespeicherten Eigenverbraucher und Kreismasken. Frischer Replay benötigt nur
`hierarchy.npz`/`views.json`, kein Originalmesh. Material-Fit, dichtere Neubemusterung,
größerer Support und neue native Bindings folgen diesem Negativ nicht. Master
integriert; Gesamt-Fernrepräsentation, Geräte-/Welt-/p99-Abnahme bleiben offen.

```sh
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error -m unittest test_surface_point_hierarchy
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/render/compare_surface_points.py --output /tmp/outshine-surface-points-proof
PYTHONPATH=lib/src:test/experiments/render .venv/bin/python -B test/experiments/render/plot_surface_points.py --output /tmp/outshine-surface-points-proof
PYTHONPATH=lib/src:test/experiments/render .venv/bin/python -B test/experiments/render/compare_surface_points.py --hierarchy /tmp/outshine-surface-points-proof/hierarchy.npz --views /tmp/outshine-surface-points-proof/views.json --output /tmp/outshine-surface-points-replay.npz
```

Vorhandene NumPy/SciPy/Trimesh/Rtree/Matplotlib-Umgebung, keine neue Abhängigkeit.
Diagnosen bleiben lokal und werden tatsächlich geöffnet; keine Bildveröffentlichung.
