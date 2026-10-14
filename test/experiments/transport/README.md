# Transport-Experimente

Python-Verfahren für zusammenhängende OSM-Straßen, Wege und Schienen. Alle Aufrufe
laufen vom Repository-Root. Ergebnis, Abhängigkeiten und offene Lücken stehen in
[#18](https://github.com/Weichwolf/outshine/issues/18),
[#19](https://github.com/Weichwolf/outshine/issues/19),
[#20](https://github.com/Weichwolf/outshine/issues/20) und
[#21](https://github.com/Weichwolf/outshine/issues/21).

- `transport_*.py`: Vektorplan, Bauteile, Fahrkurven, lokale Prüfungen und Renderings.
- `road_*.py`, `earthwork_*.py`, `clearance_aprons.py`: frühere Profil-/Kontaktvergleiche.
- `probes/<Thema>/`: fallbezogene Verfahrensvergleiche und Fehlerdiagnosen.
- `transport_network_recipes.json`: gemeinsame Rezept- und Fahrzeugparameter.
- Weitere Dateien erhalten frühere Asset-/LOD-/Speichervergleiche.

Eingaben und Messergebnisse liegen unter `build/experiments/transport/`;
ausgewählte Bilder unter `build/shots/experiments/transport/`, Logs im System-Temp.
Die Probes verwenden ihre jeweiligen Eingabestände; gemessene Hashes bleiben unverändert.
Für Probe-Aufrufe zeigt `PYTHONPATH=test/experiments/transport` auf die Solvermodule.

```sh
python3 test/experiments/transport/transport_roadbed_window_experiment.py --help
python3 test/experiments/transport/transport_coupled_roadbed_experiment.py --help
python3 test/experiments/transport/transport_roadbed_window_render.py --help
```

Transport besitzt Verkehrsnetze; Buildings und Vegetation besitzen ihre eigenen Rezepte.
Gemeinsame räumliche Verträge stehen in [#18](https://github.com/Weichwolf/outshine/issues/18). Gebäudekonflikte hier prüfen nur diese Schnittstelle.
Historische Berichte behalten ursprüngliche Pfade, Hashes und Messcommits nach der Verschiebung.
Vor erneutem Anwenden hashgeprüfter Produkte die Nachweise unter den neuen Modulnamen regenerieren.

## Lokaler Showcase

Wie bei Buildings (8766) und Vegetation (8765) veröffentlicht der Transport-Showcase
einen festen Bildstand. Port 8767 zeigt Grundformen, Anschlüsse und echte OSM-Netze;
fehlende lokale Nachweise bleiben unvollständig. Bilder lassen sich vergrößern und
als PNG herunterladen. „Stand laden“ aktualisiert die Anzeige ohne automatisches Neuladen.

```sh
.venv/bin/python -m pip install numpy scipy 'shapely>=2.1' matplotlib pillow
.venv/bin/python test/experiments/transport/showcase.py --render-fixtures --serve
```

Die Grundfälle benutzen den bestehenden Flächensolver mit gemeinsamen Randschnitten.
GEOS ist als Systembibliothek erforderlich (`libgeos-dev` unter Debian).
Der Solver importiert die installierte Referenzbibliothek: bei vorhandenem `lib/`
einmal `.venv/bin/python -m pip install -e ./lib` ausführen.
Erzeugte Bilder, Produkte und Messreports liegen unter
`build/experiments/transport/showcase/fixtures/`, die veröffentlichte Website unter
`build/experiments/transport/showcase/site/`. Der Server erzeugt keine Geometrie bei Anfragen.

```sh
.venv/bin/python test/experiments/transport/showcase.py --publish
.venv/bin/python test/experiments/transport/showcase.py --serve --host 0.0.0.0 --port 8767
.venv/bin/python -W error test/experiments/transport/showcase_checks.py
```

`--catalog` wählt einen anderen Bildkatalog. Jeder Eintrag nennt Bild, Messreport und
Erzeugungsnachweis unter `build/`. `origin.json` enthält den Erzeugungscommit,
`working_tree_dirty` und unter `records` die SHA-256-Werte von Bild und Report,
indiziert durch den Report-Dateinamen ohne Erweiterung. Nur übereinstimmende lesbare
PNGs werden veröffentlicht. Synthetische Flächenprüfungen belegen keine glatten
Fahrkurven, vollständige Befahrbarkeit, Höhen oder native Echtzeitqualität.
Der Auftrag und die Abnahme stehen in [#49](https://github.com/Weichwolf/outshine/issues/49).

## Geschlossene 3D-Körper und gespeicherte Wiener Quellkachel

`transport.close_surface` aus `lib/` schließt bereits konforme Fahrflächen mit
expliziten Vertexhöhen. Obere Anschlussvertices bleiben gemeinsam, Innenlöcher
offen; Boden und Randwände bilden einen gerichteten Körper. Das ist ein
Geometrieverfahren, kein Höhen-/Verkehrsnetzsolver. Der Vergleich prüft dieselben
Körperbuffer, Kanten und Volumina für NumPy-Sortierung und eine Dictionary-Referenz.
Frische Worker messen sieben Aufbauzeiten und ihren vollständigen Peak-RSS.

```sh
.venv/bin/python -m pip install -r test/experiments/transport/requirements.txt
.venv/bin/python -m pip install -e ./lib
.venv/bin/python test/experiments/transport/capture_transport_tile.py
.venv/bin/python test/experiments/transport/transport_flat_network.py --input build/experiments/transport/inputs --output build/experiments/transport/flat-vienna Vienna
.venv/bin/python test/experiments/transport/transport_flat_surface_network.py --input build/experiments/transport/flat-vienna --output build/experiments/transport/surface-vienna-cuts --junction-mode cuts Vienna
.venv/bin/python test/experiments/transport/surface_solid_benchmark.py
.venv/bin/python test/experiments/transport/surface_solid_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
```

Die Aufnahme liest ausschließlich die bereits gepinnte OpenFreeMap-Kachel aus dem
Client-Quellcache und prüft ihren Hash. Originalfeatures, Attribute, IDs und
zurückgestellte Flächen bleiben im gespeicherten Input erhalten. Die ebene
Nutzungsbilanz nennt jeden Achsteil samt konkretem Grund bei Nichtverwendung.
Eine Quellkachel ersetzt nicht das frühere vollständige Wiener Testfenster.

Blender ist als Systempaket erforderlich (`blender` unter Debian). Vier
Workbench-Inspektionen in 1920×1080 liegen unter `build/experiments/transport/solid/`:
geneigter T-Anschluss, Ring, getrennte Querung und Wiener Quellkachel. Die Kamera
prüft sämtliche Vertices auf vollständige Bildabdeckung; Skriptfehler ergeben
einen fehlgeschlagenen Renderprozess. Der Showcase veröffentlicht nur den letzten
vollständig gerenderten, zu Bild und Report passenden Stand.
Flatheight und Brückenniveaus sind explizite Rezeptannahmen. DEM, Rampen, Stützen,
fahrbare Kurven und native Abnahme bleiben offen. Bibliotheksübernahme: #50.

## Grobes DEM und ganze Fahrflächen (#20)

`capture_height_targets.py` liest behaltene Mapterhorn-Terrarium-Kacheln (512×512)
und interpoliert an Pixelzentren mit Nachbarkacheln. Neue Quellen werden nur mit
`--fetch` erworben; vorhandene Cachedateien werden nicht automatisch erneuert.
URL, Bytes und SHA-256 bleiben im Report. Das DEM ist ein weicher Höhenprior,
keine beobachtete Fahrbahnhöhe. `surface_height_frames.py` ordnet den verwendeten
Originalachsen prozedurale Längs-/Quergefälle zu; Originalattribute bleiben erhalten.
Anschlüsse sowie mehrdeutige/degenerierte Richtungen bekommen konservative isotrope
Grenzen; jede unsichere Richtung wird mit Face und Quell-ID festgehalten.

```sh
.venv/bin/python -m pip install -e './lib[transport]'
.venv/bin/python test/experiments/transport/capture_height_targets.py
.venv/bin/python test/experiments/transport/surface_height_frames.py
.venv/bin/python -W error test/experiments/transport/height_source_checks.py
.venv/bin/python -W error test/experiments/transport/surface_height_checks.py
.venv/bin/python test/experiments/transport/surface_height_benchmark.py
.venv/bin/python test/experiments/transport/surface_height_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
```

Bei fehlendem DEM-Cache zuerst denselben Capture-Aufruf einmal mit `--fetch`:
`curl` ist neben den Python-Paketen erforderlich. Der Vergleich nutzt identische
Vertexpriors, Gradientenrahmen und acht Sekunden Solverzeit. Clarabel löst die
elliptischen Ganzflächenbedingungen; SciPy/HiGHS verwendet ein konservativ
einbeschriebenes 16-Eck. Beide erfüllen denselben geforderten Neigungsstandard,
die lineare Approximation hat aber eine kleinere zulässige Menge. Präparation,
Solverzeit und Peak-RSS frischer Worker werden getrennt ausgewiesen; akzeptierte
Flächen erhalten zusätzlich eine unabhängige Normalenprüfung.

Vier 1920×1080-Ansichten zeigen die gesamte gespeicherte Wiener Kachel sowie das
vollständige Bauteil mit dem schlechtesten DEM-Gradienten, jeweils vorher/nachher
mit derselben Kamera und ohne vertikale Überhöhung. Orange zeigt unzulässige
Ganzflächengradienten im ungeprüften Prior, Grün Anschlüsse und Grau Korridore.
Die Detailauswahl schneidet keine Dreiecke und löst die Höhen nicht erneut.
Dateien liegen unter `build/experiments/transport/heights/`. Ganze Flächen,
gemeinsame Höhen und geschlossene Körper sind geprüft; C1/Fahrkurven, finaler
Terrainanschluss, funktionale Zugänge und erhöhte Verkehrsrezepte bleiben offen.

## Gemeinsame Bodenfläche und vorbereitete Kontakte (#20)

`ground_surface.py` ergänzt außerhalb der tatsächlichen ebenerdigen Fahrflächen
eine begrenzte konforme CDT-Partition. Die Verkehrsfläche erhält keine neuen XY-
Koordinaten und bleibt über Vertex-/Face-Zuordnungen nachvollziehbar. Ein gemeinsamer
Höhenplan begrenzt Straßen nach ihren Rezepten und Bodenfacetten nach dem ausdrücklich
prozeduralen Böschungsrezept 2:3. Unabhängige flache Straßenpläne können einen steilen
Zwischenraum erzeugen; die gemeinsame Lösung plant deren Höhen ausdrücklich neu.
Input-/Output-Coverage, vollständige Domain, Ränder und ganze Gradienten werden geprüft.

```sh
.venv/bin/python -W error test/experiments/transport/ground_surface_checks.py
.venv/bin/python -W error test/experiments/transport/ground_surface_experiment.py --compare-geometry
.venv/bin/python -W error test/experiments/transport/ground_surface_experiment.py
.venv/bin/python -W error test/experiments/transport/surface_contact_benchmark.py
.venv/bin/python -W error test/experiments/transport/ground_surface_showcase.py
.venv/bin/python test/experiments/transport/showcase.py --publish
```

Der Geometrievergleich nutzt optimierte Coverage-Union und allgemeine Union samt
denselben Vor-/Nachprüfungen; identische vorbereitete Meshs werden verlangt. Der
Kontaktvergleich nutzt die installierte `geometry.SurfaceSampler`-Bibliothek:
STRtree-Prädikat gegen Bounding-Boxes plus exakten Distanzfilter, identische Treffer,
Face-IDs/Höhen/Normalen/Fehlgründe und unabhängige Ebenenprüfung. Je Methode frischer
Worker und fünf Wiederholungen; Indexvorbereitung, Query inkl. Ergebniskopie und
Prozess-Peak-RSS getrennt. Keine Solver-/Providerarbeit in der fertigen Kontaktabfrage.

Vier 1920×1080-3D-Inspektionen unter `build/experiments/transport/ground/` zeigen die
gesamte Kachel und einen 120×120-m-Kontaktbereich vor/nachher mit gleicher Kamera.
Vorher ist absichtlich ungültig: unverändertes grobes DEM durchdringt gefittete Straßen.
Nachher ist die gemeinsame obere Fläche genau einmal belegt. Ein 5-m-Schaublock
visualisiert ihren Ausschnitt; er ist kein Aushubvolumen-/Material-/Kollisionsnachweis.
Brücken/Dächer gehören nicht in diese ebenerdige Bodenpartition. Gespeicherte Daten
und Z-Bezug bleiben erhalten; keine vollständige Place-, C1- oder Eingangsabnahme.
Das C0-Mesh approximiert den DEM-Prior; der gemessene Fehler an Face-Zentren wird
ausgewiesen und beweist keine globale DEM-Fehlerschranke. Adaptive Verfeinerung und
glatte Profile bleiben weitere Arbeit; funktionale Zugangskontakte bleiben separat.

## Gegenbeispiel: feste Terrain-Fächer verfeinern (#20)

```sh
.venv/bin/python -W error test/experiments/transport/ground_refinement_checks.py
.venv/bin/python -W error test/experiments/transport/ground_refinement_experiment.py
.venv/bin/python -W error test/experiments/transport/ground_refinement_showcase.py
```

Kantenmittelpunkte und Flächenzentren erhalten bei gleichem Budget alle bisherigen
Vertices, Straßen und äußeren Ränder. Analytische Ganzflächenintervalle begrenzen
die neue Einzelhöhe; eine lokale DEM-Stichprobe verhindert den zuerst gemessenen
Extremfehleranstieg. Beide Methoden reduzieren danach den gemessenen Mittel-/RMS-
Fehler. Der Zentrenpfad kostet weniger und verbessert p95 stärker; der Kantenpfad
hat den kleineren gemessenen Maximalfehler. Die Prüfmenge bleibt unabhängig von
den eingefügten Kandidaten. Dies ist keine globale DEM-Fehlerschranke.

Die tatsächlich geöffneten 1920×1080-Ansichten (grob, 50%-Morph, fein) zeigen jedoch
dreieckige Buckel und scharfe Falten. Dieser Pfad ist als **visuelle Lösung verworfen**;
er wird weder in `lib/` übernommen noch ersetzt er die freigegebene Showcase.
Reproduktion, Meshs und Messungen liegen unter `build/experiments/transport/refinement/`.
Der vertikale grob/fein-Unterschied ist für affine Eltern-/Kindflächen an den
feinen Vertices exakt begrenzt; das beweist weder C1 noch einen Bildgewinn.
Als Nächstes die freie Terrain-Konnektivität zwischen tatsächlichen Straßenrändern
prüfen, anstatt die unpassenden alten Innenkanten weiter zu fixieren.

## Bodenverbindungen austauschen (#20)

```sh
.venv/bin/python -W error test/experiments/transport/ground_reconnect_checks.py
.venv/bin/python -W error test/experiments/transport/ground_reconnect_experiment.py
.venv/bin/python -W error test/experiments/transport/ground_reconnect_showcase.py
```

Der gespeicherte Zentrenversuch liefert dieselben 2500 zusätzlichen Punkte für zwei
Verbindungsvarianten: GEOS-Delaunay mit Winkelgewinn gegen rein probengesteuerten
Kantentausch. Beide halten sämtliche XYZ, Straßenfaces und Außenränder fest.
Streng konvexe Bodenpaare dürfen die Diagonale tauschen, wenn die neuen Ganzflächen
die Neigungsgrenze und denselben lokalen DEM-Qualitätscheck erfüllen. Bis acht
Durchgänge, eine Million Proben, 65536 Faces/Vertices und zwölf Sekunden kooperative
Zeitgrenze; eine begrenzte Phase kann überlaufen. Kein Frame-Verfahren.

Drei frische Worker je Variante prüfen dieselben 40734 unabhängigen Abfragen, volle
Coverage/Rand und Neigung, unveränderte Quellen und Verkehrsgeometrie. Zeit umfasst
den Kantentausch einschließlich Vorbereitung/DEM-Proben, nicht spätere unabhängige
Abnahme oder PNG-Export; Prozess-Peak-RSS wird separat gemessen. Ergebnisse und sechs
1920×1080-Ansichten liegen unter `build/experiments/transport/reconnect/`.
Gleiche Kamera vor/nachher für Kachel und 120×120-m-Bereich, keine Höhenüberhöhung.

Geöffnete Detailbilder zeigen weniger Sternfalten; gemeinsame Kontakte bleiben
erhalten. C0-Facetten bleiben sichtbar. Globale DEM-/C1-/native Abnahme und stabile
LOD-Übergänge fehlen: der alte affine Eltern-/Kindbeweis gilt nach dem Kantentausch
ausdrücklich nicht mehr. Dieser Pfad bleibt Experiment, keine `lib/`-Promotion und
kein Ersatz des freigegebenen #69-Bodenplans.

## Externe CityEngine-Operationsreferenz (#19)

```sh
.venv/bin/python -W error test/experiments/transport/esri_transport_catalog.py
.venv/bin/python -W error test/experiments/transport/esri_transport_ghidra.py
.venv/bin/python -W error test/experiments/transport/verify_esri_transport_ghidra.py
.venv/bin/python -W error test/experiments/transport/verify_esri_transport_oracle.py
.venv/bin/python -W error test/experiments/transport/esri_transport_comparison.py
.venv/bin/python -W error test/experiments/transport/esri_transport_showcase.py
```

Die externe SDK-Core-Binary liegt unter `~/Downloads/esri_ce_sdk-3.4.12206/bin/`.
SDK-Version über die öffentliche `prt::getVersion`-ABI geprüft; SHA256
`850a1b10182e9793f67791438989835de08a9facc2bd0d4f12c26c9a8be5ab45`.
`nm -S --defined-only` und `c++filt` katalogisieren 16675 Textsymbole, davon 507
Geometriekandidaten einschließlich Aliassen/Cold-Clones. Elf Funktionen decken
Offset, Schnitt, Projektion, Triangulation, Extrusion und Version ab.

Ghidra verwendet den vorhandenen Buildings-Skriptansatz, Snap-Projekte ausschließlich
unter `~/snap/ghidra/common/`. Disassemblierung ist auf jeden ELF-Symbolbereich begrenzt.
Ausgehende Sprünge werden als dokumentierte CALL_RETURN-Grenzhypothese modelliert;
ihr Zielcode gehört nicht zum untersuchten Körper. Ursprüngliche Adressen/Mnemonics,
direkte Aufrufe/PLT-Auflösung, indirekte Unsicherheit, Bytezählung und Exporthashes
stehen im Report. Kein verifizierter Decompiler-Typvertrag oder Vollständigkeitsbeweis
aller ausführbaren Pfade. Neue Nonce, exakte Bereiche und sämtliche frischen Exporte
sind Pflicht; Exit 0 mit fehlendem/altem Teilstand wird abgewiesen. Prozess360 s,
Decompiler25 s pro Funktion; globale Autoanalyse aus. Keine externen Codeexporte im Git.

Linux-Orakel laufen in eigenen Prozessen: CPU8 s, Adressraum2 GiB, Parent-Wartegrenze10 s.
Blockierter C-Aufruf und verweigerte3-GiB-Abbildung prüfen diese Grenzen; Prozessstart
ist kein universeller harter Whole-call-Nachweis. `Vector3`-/Skalar-Ausgaben haben
Canaries, private Offset-Vektoren geprüfte lesbare Speicherbereiche. Nur gepinnte
skalare Schnitt-/Ray- und Offset-ABIs werden aufgerufen. Ray-Modus2 liefert den
Multiplikator der eingegebenen Richtung, nicht grundsätzlich Meter.

Der SDK-Triangulationsbackend `libglutess.so` besitzt SHA256
`2b10c97d5a3c92a2885786d4e0d497cb753b9786ef0b450db1ee61381c1fda78`.
Seine dokumentierte GLU-C-API verwendet opaque Handles; sie beweist nicht den privaten
`util::Triangulator`-/Processor-Vertrag. Raw-Rasterdreiecke bleiben gespeichert;
exakte Nullfaces werden vor CAD-Ausgabe entfernt, Randpunkte mit dem gemeinsamen
`conform_triangles` eingepasst. SDK-Extrusion und Mesh-/Hauptachsenobjekte bleiben
Decompiler-Hypothesen. Die 0,35-m-Bildkörper erzeugt unsere eigene Schließbibliothek.

29 Offset-, 13 Triangulationsfälle und 256 echte Wiener Quellsegmentpaare vergleichen
dieselben Eingaben, Geometrie/Topologie/Grenzen und Kosten. Float32-XY/Abstand beim
Offset sind für beide Varianten identisch; Transport nutzt außerdem Rundprofile,
dieser Vergleich untersucht ausdrücklich den Mitre-Inset. Der kleine SDK-Helfer
liefert beim Dumbbell nur ein Fragment, GEOS beide; Löcher haben keinen expliziten
ABI-Parameter und werden als fehlendes Orakel ausgewiesen. `edgesIntersect` hat
beobachtete halb offene Enden und akzeptiert auch schiefe Annäherungen; kein Ersatz
für echte räumliche Kontakte. Triangulationsdomänen/Ränder/Höfe/Quellpunkte stimmen
nach offengelegtem CAD-Adapter im geprüften Umfang überein.

Vier gleiche Offset-Qualitätsfälle haben je Methode drei frische Worker/fünf Samples.
Zeit für vorbereitete Operation und XY-Decode, Startup getrennt; SDK-Speicherprüfungen
zählen. Peak-RSS aus Linux VmHWM gehört zum aktuellen Exec-Adressraum einschließlich
Imports/Quelle/Receipt; `ru_maxrss` wird separat gehalten, weil Fork-Vorgeschichte
kleine Workerpeaks verdecken kann. Keine Geräte-/Runtime- oder Netzgenerator-Abnahme.
Ergebnisse, Assembly, externe Hypothesen und sechs1920×1080-Bilder liegen unter
`build/experiments/transport/esri-reference/`. Keine SDK-/Fremdcode-Abhängigkeit in lib/runtime.

```sh
.venv/bin/python -W error -m unittest discover -s lib/tests
.venv/bin/python -W error test/experiments/transport/surface_solid_checks.py
```
