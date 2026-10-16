# Ganze äußere Radkorridore und begrenztes Terrain (#434)

Die feste [Wiener Eingabe](fixtures/stair-cycle-corridors-vienna.json) ergänzt
die ganzen ursprünglichen Teile `39881120:50` (1955,095 m) und `39881120:39`
(669,978 m) zum [Südstand](stair_cycle_south_comparison.md). Alle bisherigen
21 Achsen/Priors bleiben bytegleich; Originale, Attribute und IDs unverändert.
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

## Geprüfter Stand `674cb400e`

32 fokussierte Kontrollen, 44,375 s, terminal OK; frische Erzeugung und
unabhängiger gespeicherter Reader bestanden. Der frühere Südstand besteht mit
dem neuen Code ebenfalls in einem isolierten Reader, ohne seine Referenzen zu
ändern. Neue Terrain- und Pfeilerreader bestehen zusätzlich mit gesperrten
CDT-, LP-, Kegel-Solver- und Körper-/Pfeiler-Buildern. Kein Neubau wird als
gespeicherte Prüfung ausgegeben; tatsächliche Array-/Quell-/Bildbindung bleibt Pflicht.

23 ganze Quellteile, zehn geschützte ganze Endprofile und zehn wirkliche
Unterführungen; kleinster tatsächlicher Float32-Durchgang 2,519989 m.
Straßenkörper 6704 V/13444 F, weiterhin drei Treppen/24 Stufen und zyklische
geschlossene Vertexlinks. Originalinventar 1657 Linienteile/71 Flächen;
1634 Linien und alle 71 Flächen samt Originalen/IDs bleiben unzugelassen.
86 Linien besitzen mögliche Bandüberlappungen; das ist kein Konflikt- oder
Freiraumnachweis. Der ganze abgelehnte kurze Nachbar hat einen eigenen konkreten
Kataloggrund. Keine unzugelassenen Quellen aus der Bilanz entfernt.

9090 statt 14352 Ausgangszellen, 1754 grobe Blöcke; maximal 23,912 m Zellbreite
fern von Verkehrsflächen. 25245 echte DEM-Kontrollsamples verfeinern zusätzlich
1051 Blöcke. Größter verbliebener Kontrollfehler 0,25 m; die 5,460-m-Variante
ohne diese Kontrolle bleibt verworfen. Nach konformer Triangulation und lokaler
Brückenverfeinerung: 12349 Höhenpunkte/24340 Facetten. Geschlossener Terrainkörper
24698 V/49392 F in beiden Präzisionen. 1398 freie Übergangskanten wurden konform
geteilt; alle 3613 ursprünglichen Bodenkontaktfacetten bleiben XYZ-bytegleich
in Double und Float32. Ganze tatsächliche Kontaktabstände 0/0 m; keine daraus
abgeleitete exakte Null-Überlappungsvolumenbehauptung.

Alle vier ganzen neuen Crops stimmen exakt gegen dieselben ursprünglichen
Roh-Decodes; sämtliche 12349 endgültigen Terrainziele ebenfalls exakt. Tatsächliche
Punkte je Kachel 14/8937/5678, 14/8937/5679, 14/8938/5678, 14/8938/5679:
11538/362/445/4. Keine Extrapolation; auch die vierte Kachel bleibt erforderlich.
Gemessene Erdneigung maximal 0,664667338/0,665433524, konstruktiv
9,998000015/9,997904310 bei unveränderter Grenze 10; 33 konstruktive Facetten/
10,235310 m², tatsächlicher 5-m-Bereich geprüft. Kein Statik-/Aushubsolidnachweis.

Der bestehende Pfeilerplan löst die neuen Verkehrsabstände erneut: 64 feste und
70 verschobene Stationen. Die kurze Brückenquelle ohne Innenstation bleibt
ausgewiesen. Ganze positive Körper/Kappen/Vertexlinks, kleinste gemessene Höhe
rund 0,111908 m und direkte 0,2-m-Fremdverkehrsausschlüsse in beiden Präzisionen.
Größter Double-Kontaktfehler 2,8422e−14 m, tatsächlicher Float32-Kontaktfehler
15,512 µm, größter eigener Umschlag 26,776 µm. 81 Versuche, 11548 akzeptierte
Kontaktpaarprüfungen und 2616 ausgegebene Körperfacetten. Ein geteilter Builder
mit 24340 unteren/1240 oberen Facetten; 9634 Verkehrsdreiecke in zwei Indices,
keine Verkehrsunions, expliziter XY-Payload 462432 B und eigene Masken 77072 B.

Vier frische vollständig decodierte/geöffnete 1280×720-Bilder vergleichen den
vorigen 21-Quellen-/70-Pfeilerstand mit dem neuen ganzen Verbund: je dieselbe
Übersichts- und ganze Brückenkamera. Positionen, Topologie, Quellnormale, Farben,
Szenen-/Helper-/PNG-Hashes sind gebunden. Sichtbar sind die zwei weitergehenden
Korridore und ihre zusätzlichen Durchgänge; keine Detailaufnahme oder veränderte
Straßenbreite. Sehr schmale Wege bleiben in der Fernübersicht schwach lesbar;
das ist keine vollständige Stadtdarstellung. Blender 5.2.2 LTS EEVEE/llvmpipe,
kein Outshine-/GPU-/Gerätenachweis. Forschung bleibt lokal, keine Doc-Galerie.

## Gleiche Qualität und Kosten

Fester neuer Plan, dieselbe Energie, Klassen-/Kontakt-/Float32-/Zeitgrenzen;
selektive ausreichende Kegel gegen alle tatsächlichen Float32-Kegel. Sieben
gültige normale Fits je Variante plus Warmfit, normale Priorität, nach Import/
Decode; Matrixvorbereitung und Solve gemessen, tatsächliche ganze Feld-/Körper-
Abnahme und IO außerhalb. Exakte Höhenidentität allein erlaubt Prüfungsreuse.
Keine eigenen Builds/Renderer parallel; gemeinsamer Host nicht isoliert.
i5-1135G7, zwei physische/vier logische CPUs, 16 GiB RAM. Keine frühere Kachel
mit anderer Abdeckung als Kostenbaseline und kein allgemeiner Speedup-Faktor.

| Neuer gleicher Terrainplan | Selektive Kegel | Alle Kegel |
|---|---:|---:|
| Phase Median ms |4552,872|6252,724|
| Prozess-Peak B |184143872|235544576|
| Zusätzliche Kegel |50|20727|
| Solvervariablen nach 2788 festen Eliminationen |10669|10669|
| Solverzeilen |101086|163117|
| Solver-A-Nichtnullen |125423|206007|
| Beobachtete Allokationsanfragen |4668952|5303705|
| Angeforderte B |553214799|658584822|
| Getrackter Heap-Peak B |78184655|123554009|

Die separaten vollständigen Memray-1.20.0-Phasen verwenden Python-Allocator-
Ereignisse/native Stacktraces; instrumentierte Zeiten 9327,885/11699,851 ms sind
keine normalen Laufzeiten. Kumulative Anfragen sind kein lebender/unique RAM.
Größter Höhenunterschied der gültigen Varianten 6,186 µm. Beide verwenden
je Fit 296376 B explizite XY-Quantisierungskopien und 322968 B Shift-/Delta-/Linear-
Payload; vollständige interne Kopien bleiben unbeobachtet.

Zusätzlich die ganze Straßen-/Terrainpipeline einschließlich Quelldateien/Decode,
Layout/Profil, Körper, Terrain, ganze Physik, Katalog und Darstellungsexport
unter einem separaten Memray-Tracker: 57038446 native Anfragen,
9504333280 angeforderte B, 342503700 B getrackter Heap-Peak und 374419456 B
Prozess-Peak vor nachträglichem Reader/Profilerdateiscan. Python-Allocatorereignisse
und native Stacktraces hier aus; diese Zählungen sind nicht von der Fit-Tabelle
abzuziehen. 51,162 s instrumentiert, frischer kompletter Reader bestanden.
Alle sieben Körper-/Höhen-/Plan-/Darstellungsprodukte sind nach dtype, Form und
rohen Arraybytes identisch zum kanonischen Export; unbekannte Bett-Höhen-NaNs
werden als gespeicherte Bits verglichen. Pfeiler/Blender sind außerhalb dieser
Messung. Der erhebliche Aufbau-/Prüfaufwand bleibt offen, kein Lade-/Framebeleg.

Geprüfte Produkte/Reader: `/tmp/outshine-transport-stair-corridors-674cb400e/`;
Pfeiler und vier aktuelle Bilder: `/tmp/outshine-transport-stair-corridor-support-674cb400e/`;
Belege/Kosten/Originalinputprüfung: `/tmp/outshine-transport-stair-corridors-evidence-674cb400e/`.
Sämtliche sieben äußeren Profile, weitere Quellen/Ebenen, genaue Fahr-/Gehhüllen,
Eckrezept, Handläufe/Statik und native Übernahme bleiben offen. #434 bleibt aktiv.
