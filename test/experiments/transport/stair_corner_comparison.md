# Ganze Endprofile an einer wirklichen Radwegecke (#434)

Die [feste Wiener Eingabe](fixtures/stair-cycle-corner-vienna.json) ergänzt den
ganzen ursprünglichen `1204661482:0`-Radweg, 48,485506 m, zum
[23-Quellen-Korridorstand](stair_corridor_comparison.md). Beide ursprünglichen
Achsen treffen sich am wirklichen MVT-Knoten `[2953,3071]`. Die flach endenden
2-m-Wegbänder decken jeweils rund einen Meter des anderen Endquerschnitts nicht.
Dieser alte Gegenfall bleibt ohne konstruktive Änderung ausdrücklich abgelehnt.

## Konstruktion und Varianten

`stair_corner_geometry.apply_pads` bildet die konvexe Hülle beider ganzer
ursprünglicher Endprofile. Das zusätzliche ebene Podest gehört beiden Quellen;
die vorhandene konforme Anordnung und gemeinsame Höhenplanung bauen es zusammen
mit den ganzen Wegen. Kein Dreieck wird nachträglich über einen Spalt gelegt.
Die alten Achsen, Priors, Attribute und IDs bleiben unverändert; die **abgeleiteten
Flächen** der beiden Quellen erhalten einen ausdrücklich dokumentierten Zusatz.
Der gespeicherte Reader leitet denselben Zusatz aus der gebundenen Eingabe ab.

Eine Deklaration benötigt zwei verschiedene gewöhnliche äußere Fuß-/Radwege,
wirkliche übereinstimmende Endpunkte und einen gewählten Originalknoten.
Innenknoten, bloße Nähe, Brücke/Tunnel, indoor/level, Treppen, ungewählte Knoten,
degenerierte Profile und doppelte Deklarationen werden zurückgewiesen. Höchstens
acht kleine Podeste, bisherige Anordnungs-/Solver-/Speichergrenzen und ganze
Double-/Float32-Endprofilprüfungen bleiben Pflicht. Die alten Defaults bleiben
unverändert. Das ist ein lokales Kontaktrezept, keine allgemeine Freigabe von
Knickradien, Geh-/Fahrhüllen, Selbstschnittfreiheit oder rechtlichen Wegmaßen.

Zuerst lokale SIGGRAPH-Primärquelle geprüft: Chen et al. 2008,
[Interactive Procedural Street Modeling](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
S. 2/Fig. 2 auch optisch. Die Trennung Graph/3D-Geometrie trägt die unveränderten
Quellachsen; konstruktive Kreuzungsdetails schließt das Paper ausdrücklich aus.
Die Profilhülle ist unsere eigene kleine Konstruktion mit vorhandenen GEOS-/CDT-
Bibliotheksoperationen, kein dort belegter Kreuzungsalgorithmus und kein Fremdcode.

Gleiche 24 ganze Quellen, Profile, LP-Grenzen und tatsächliche physische Prüfungen:

| Größe | Profilhülle | Umschreibendes achsenparalleles Rechteck |
|---|---:|---:|
| Ganzes gemeinsam besessenes Podest, m² | 1,525638 | 3,276825 |
| Fläche außerhalb beider ursprünglicher Bänder, m² | 0,381410 | 0,552405 |
| Geschlossener Wegekörper, Dreiecke | 13940 | 13984 |
| LP-Variablen / Zeilen / Nichtnullwerte | 6576 / 142978 / 423288 | 6598 / 143406 / 424572 |
| Explizite sparse Arrays, Byte | 5105764 | 5121260 |
| Explizite XY-Quantisierungskopien, Byte | 77976 | 78240 |
| Eigene gespeicherte Körperarrays, Byte | 921456 | 924412 |
| Normaler Einzelaufbau, s | 7,734455 | 7,294717 |
| Prozess-Peak einschließlich anschließender Prüfungen, Byte | 371150848 | 369410048 |

Beide Varianten bestehen dieselben zwölf ganzen Endprofilkontakte, zehn
Unterführungen, drei Treppen/24 Stufen und geschlossene zyklische Vertexlinks.
Die Hülle wird wegen der kleineren Ergänzung und geringeren geometrischen Arbeit
gewählt. Kein Geschwindigkeits-, allgemeiner Speicher- oder globaler Optimalitäts-
vorteil wird aus den einzelnen verrauschten CPU-Läufen abgeleitet. Das Rechteck
bleibt ausführbare Vergleichsvariante; die Hülle rotiert mit den Profilen.

Getrennte Memray-1.20.0-Phasen mit Python-Allokatoren und nativen Traces:
Hülle/Rechteck 18564265/18628426 Ereignisse,
2466018068/2473275237 kumulativ angeforderte Byte und
342043587/342710149 Byte beobachteter Heap-Peak. Instrumentierte Zeiten
30,998005/26,790252 s bleiben von normalen Zeiten getrennt. Gemessen sind Podest,
Anordnung, LP und geschlossener Körper **nach** Eingabedecode; ganze tatsächliche
Prüfungen liegen außerhalb des Trackers. Keine Terrain-/Pfeiler-/Renderkosten in
diesen Allokationszahlen; kumulative Bytes sind kein Live-Speicher. Nicht explizit
gezählte interne Kopien bleiben unbekannt. i5-1135G7, 2 physische/4 logische Kerne,
16 GiB, Linux/Python 3.13.5; geteilter Host, keine isolierte Geräteabnahme.

## Ganzer Export, Bilder und Grenzen

Quell-/Verfahrensstand `f3b62d1f9`, zusätzliche Rotationskontrolle `a9ce6bbcd`:
40 betroffene Kontrollen und eine neue Rotationskontrolle bestanden, ohne Skip.
Frische vollständige Erzeugung und unabhängige gespeicherte Reader bestanden.
Aktuelle Terrain-/Pfeilerreader bestehen auch bei gesperrten CDT-/LP-/Kegelsolvern,
Körper-/Pfeilerbuildern und Arrayexportern. Der vorherige 23-Quellen-Reader besteht
mit unveränderten Referenzen; dessen ursprünglicher Readerbeleg bleibt erhalten.

Das gleiche ganze Terrainrechteck umfasst 2044993,168 m²; 9093 statt 14352
Ausgangszellen, endgültig 12472 Punkte/24586 Höhenfeldfacetten. 0,25-m-DEM-Kontrolle,
16384 Punkte/32768 Höhenfeldfacetten, acht Solversekunden/100 Iterationen,
Neigung 2/3 für Erde und 10 konstruktiv sowie tatsächlicher 5-m-Bereich bleiben
unverändert. Alle 3786 Bodenfacetten sind XYZ-bytegleich in Double und Float32;
die ganzen gemessenen Bodenkontakte sind 0/0 m. Alle aktuellen Terrainziele und
vier ganze Crops stimmen exakt mit den vier vollständigen Original-Decodes überein.
Kein Null-Überlappungsvolumen oder kontinuierlicher DEM-Fehlerbeweis wird behauptet.

Kleinster tatsächlicher Float32-Durchgang 2,519989 m; Erdneigung maximal
0,664667040/0,665433524, konstruktiv 9,997999617/9,998441054. 33 konstruktive
Facetten/10,235310 m² bleiben im wirklichen 5-m-Bereich. Der geschlossene
Wegekörper hat 6952 Vertices/13940 Dreiecke. Das Terrain hat als geschlossener
Körper 24944 Vertices/49884 Dreiecke; die Facettengrenze betrifft sein Höhenfeld.
Frische Erzeugung vor Displayexport 28,297235 s, bis dahin Prozess-Peak
368304128 Byte; kein Vergleich mit anders belasteten alten Erzeugungsläufen.

64 feste/70 verschobene ganze Pfeiler bleiben gültig: 81 Versuche, 11548
Kontaktpaarprüfungen, ein Builder mit aktuellem Terrain und Quellunterseiten.
Direkte 0,2-m-Fremdverkehrsabstände, ganze positive Höhen und Double-/Float32-
Kontakte bleiben geprüft; kein neuer Statik-/Fundament-/Bodenkennwertnachweis.

Vier frische tatsächlich geöffnete 1280×720-Bilder vergleichen den alten
23-Quellen-/70-Pfeilerstand mit dem neuen 24-Quellen-/70-Pfeilerstand. Je Paar
exakt gleiche Kamera, Licht und Palette, tatsächliche vollständige Meshimporte
mit gebundenen Normalen/Farben/Topologie. Ganze Terrainübersicht und moderater
750-m-Netzausschnitt einschließlich der ganzen Brücke; kein Nahdetail oder
öffentliche Galerie. Blender 5.2.2 LTS/Eevee, llvmpipe/LLVM 19.1.7, Offline-CPU.
Im Netzausschnitt ist der zusätzliche kurze Weg sichtbar; die Gesamtübersicht
und sehr schmale helle Wege bleiben schwach lesbar. Das ist ein kleiner örtlicher
Funktionsgewinn, kein überzeugendes vollständiges Stadtbild. Pixelvergleich:
1993/921600 Pixel geändert, 715 um mehr als 1/255, keine schwarzen Löcher.

Sieben echte Außenprofile bleiben offen: Anfang `4420992360:6`, Ende
`32651280:3`, Anfang `39881120:51`, beide Enden `39881120:50`, Anfang
`39881120:39` und Ende `1204661482:0`. Eine alte Frontier ist geschlossen,
eine neue liegt am anderen Ende des hinzugefügten Weges. Alle ursprünglichen
1657 Linienteile/71 Flächen bleiben erhalten: 24 Kandidaten, 1633 übrige Linien
und sämtliche Flächen mit Gründen. U-Bahn-/Indoor-Rezepte, weitere Netze und
Ebenen, sichere Geh-/Fahrhüllen, Knickradien, Handläufe, Statik und native
Integration bleiben offen. `admitted_source_ids=[]`; #434 bleibt aktiv.

## Reproduktion und Belege

Keine neuen Runtime-/SDK-/lib-/Reservations-/nativen Abhängigkeiten.
NumPy/SciPy/Shapely/Clarabel wie im Korridorvergleich; Memray optional für Kosten.

```sh
export PYTHONPATH=lib/src:test/experiments/transport
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -W error -m unittest stair_corner_checks stair_corridor_checks \
  stair_cycle_checks stair_cycle_north_checks stair_cycle_south_checks \
  stair_terrain_cells_checks -q
python test/experiments/transport/stair_corner_cost.py \
  --method profile_hull --output /tmp/outshine-transport-corner-hull.json
python test/experiments/transport/stair_corner_cost.py \
  --method profile_box --output /tmp/outshine-transport-corner-box.json
python test/experiments/transport/stair_cycle_experiment.py \
  --fixture test/experiments/transport/fixtures/stair-cycle-corner-vienna.json \
  --base-network /tmp/outshine-transport-stair-corridors-674cb400e \
  --base-terrain /tmp/outshine-transport-stair-corridors-674cb400e \
  --output /tmp/outshine-transport-stair-corner-<commit>
```

Danach derselbe Aufruf mit `stair_cycle_verify.py`; Pfeiler über
`stair_support_experiment.py`/`stair_support_verify.py` mit gleicher Fixture und
den neuen geprüften Produkten. `stair_corner_cost.py --profile <frische.bin>`
liefert getrennte Allokationen; ein Worker je Variante. Renderer bleibt
`render_transport_research.py`, Kamera/Licht/Mesh aus dem gemeinsamen Referenzkern.
Die vier versionierten Crops/PBF und ursprünglichen Rohhashes bleiben unverändert.
Lokale aktuelle Körper/Terrainprodukte: `/tmp/outshine-transport-stair-corner-full-edcd738ce/`;
Pfeiler/vier Bilder: `/tmp/outshine-transport-stair-corner-support-f3b62d1f9/`;
Prüf-, Rohkontakt-, Kosten-, Render- und SHA-Belege:
`/tmp/outshine-transport-stair-corner-8f4142125/`. Die tatsächlichen Commitfelder
der Belege sind maßgeblich; Verzeichnisnamen bezeichnen teils den Ausgangsstand.
