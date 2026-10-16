# Ganze äußere Radkorridore und begrenztes Terrain (#434)

Die feste [Wiener Eingabe](fixtures/stair-cycle-corridors-vienna.json) ergänzt
die ganzen ursprünglichen Teile `39881120:50` (1955,095 m) und `39881120:39`
(669,978 m) zum [Südstand](stair_cycle_south_comparison.md). Alle bisherigen
21 Achsen, Priors, Originale, Attribute und IDs bleiben bytegleich erhalten.
Gemeinsame Höhen und Terrain werden neu geplant; keine gemessenen Deckhöhen,
vollständige Quell-/Wegenetz-, Statik- oder native Abnahme wird daraus abgeleitet.

## Ganze Kontakte und örtlicher Rückfall

Beide alten Nordweg-Enden binden an wirkliche innere Vertices von `39881120:50`.
Der bisher offene Anfang von `32651280:3` und ein Ende des neuen `39881120:39`
erhalten ebenfalls ganze gemeinsame Kappen. Zehn geschützte ganze Profile,
zehn Unterführungen einschließlich der bisherigen sechs und alle drei
Treppen/24 Stufen bleiben in Double und tatsächlichem Float32 zu prüfen.
Echte Quellvertices und zusätzlich gewählte gemeinsame Bandflächen bleiben
unterscheidbar; kein Kontakt wird aus bloßer Nähe behauptet.

Sieben Profile bleiben offen: `4420992360:6` am Anfang, `32651280:3` am Ende,
`39881120:51` beidseitig, neuer `39881120:50` beidseitig und neuer `39881120:39`
am Anfang. Drei frühere Frontiers sind geschlossen, drei neue liegen weiter
außen. Der ganze 48,486-m-Kandidat `1204661482:0` schließt den bisherigen
Südquerschnitt gerade nicht vollständig. Dieser tatsächliche Gegenfall bleibt
mit Original-ID und konkretem Auslassungsgrund im Katalog; eine eigene
konstruktive Eck-/Knotenform ist noch erforderlich. Stationswege mit indoor/
Ebenenangaben werden weiterhin nicht als gewöhnlicher Erdbodenanschluss gewählt.

## Dieselbe Abdeckung bei unterschiedlicher Arbeitsauswahl

Das ganze neue Rechteck umfasst 2044993,168 m². Uniforme 12-m-Zellen überschreiten
das bestehende Terrain-Facettenbudget. Uniforme 24-m-Zellen bleiben unter dem
Budget, können aber die ganzen Brückenbedingungen nicht erfüllen; auch die
diagnostische Double-Formulierung ohne Float32-Reserven ist unlösbar. Diese
Negativen bleiben keine gültigen Kosten-/Bildreferenzen und erhöhen keine Grenzen.

Die neue optionale Zellwahl behält das ursprüngliche feine Raster innerhalb
48 m um die tatsächlichen Verkehrsprojektionen einschließlich Brücken. Nur
außerhalb werden benachbarte 2×2-Zellen zusammengefasst; gesamte Rechteckabdeckung,
Quellkontakte und Sichtbarkeit bleiben erhalten. Fernzellen sind höchstens 24 m
breit. Ohne DEM-Kontrollen wird keine Zelle zusammengefasst.

Reine Abstandsauswahl verliert in echten DEM-Kontrollen bis 5,460 m. Daher
prüft die Auswahl vier Kantenmitten und das Zentrum jedes vorgesehenen groben
Blocks gegen dessen wirkliche DEM-Ecken, im Zentrum beide möglichen Diagonalen.
Über 0,25 m bleiben die ursprünglichen feinen Zellen erhalten. Das ist eine
Kontrolle der ausgelassenen Rasterpunkte, keine Schranke für die gesamte
kontinuierliche DEM-Oberfläche oder das spätere angepasste Terrain. Die tatsächliche
physische Abnahme des fertigen Feldes/Körpers bleibt zusätzlich verpflichtend.

Grobes und feines Raster werden zusammen mit den unveränderten Quellfacetten
konform trianguliert. Ganze Kappen/Übergänge und geschlossene Vertexlinks werden
in beiden Präzisionen geprüft. 16384 Höhenpunkte, 32768 Terrainfacetten,
acht Solversekunden/100 Iterationen, Erd-/Konstruktionsneigungen und tatsächlicher
5-m-Konstruktionsbereich bleiben unverändert. Ein erfolgreicher Solverstatus
allein reicht nicht. Das bisherige uniforme Verfahren bleibt unveränderter
Default; vorhandene Versuche werden nicht auf das neue Raster umgestellt.

Zuerst lokale SIGGRAPH-Referenz: Lindstrom et al. 1996,
[Continuous Height Field LOD](../../../doc/references/transport/siggraph/1996-continuous-height-field-lod.pdf),
S. 1 Fig. 1 und S. 2 Motivation optisch gelesen. Kritische Formen/Fehlermaße
begründen lokale Auflösung. Unser fester, kamerafreier zweistufiger Kontakt-/
DEM-Versuch ist nicht deren dynamischer Bildschirm-LOD-Algorithmus und übernimmt
keinen Fremdcode. Die bestehende Plan-/Geometriestufung bleibt Chen et al. 2008;
die ursprünglichen Solver-/Kontaktbelege stehen bei den verlinkten Vorläufern.

## Reproduktion

Mit der bestehenden NumPy/SciPy/Shapely/Clarabel-Umgebung; keine neuen Pakete:

```sh
export PYTHONPATH=lib/src:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_terrain_cells_checks stair_corridor_checks \
  stair_cycle_checks stair_cycle_north_checks stair_cycle_south_checks -q
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-cycle-corridors-vienna.json \
  --base-network /tmp/outshine-transport-stair-south-237bd38f5 \
  --base-terrain /tmp/outshine-transport-stair-south-237bd38f5 \
  --output /tmp/outshine-transport-stair-corridors-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py` als unabhängiger gespeicherter
Reader ohne Neubau/Fit. Pfeiler kommen aus `stair_support_experiment.py`/
`stair_support_verify.py` mit derselben neuen Fixture und den neuen geprüften
Produkten. Private Ergebnisse, Logs, tatsächliche 1280×720-Bilder und Kosten
bleiben im System-Temp. Vier ursprüngliche DEM-Rohdateien und die ganze
Original-PBF bleiben unverändert; die größeren vollständigen Crops sind feste
versionierte Eingaben. Keine SDK-/Runtime-, lib-, native oder Reservations-API.
